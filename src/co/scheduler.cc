#include "scheduler.h"
#include "co/os.h"

DEF_uint32(co_sched_num, os::cpunum(), "#1 number of coroutine schedulers, default: os::cpunum()");
DEF_uint32(co_stack_size, 1024 * 1024, "#1 size of the stack shared by coroutines, default: 1M");
DEF_bool(co_debug_log, false, "#1 enable debug log for coroutine library");

namespace co {

__thread SchedulerImpl* gSched = 0;

SchedulerImpl::SchedulerImpl(uint32 id, uint32 sched_num, uint32 stack_size)
    : _wait_ms((uint32)-1), _id(id), _sched_num(sched_num),
      _stack_size(stack_size), _stack(0), _stack_top(0),
      _running(0), _co_pool(), _stop(false), _timeout(false) {
    _epoll = new Epoll(id);
    _main_co = _co_pool.pop(); // coroutine with zero id is reserved for _main_co
}

SchedulerImpl::~SchedulerImpl() {
    this->stop();
    delete _epoll;
    free(_stack);
}

void SchedulerImpl::stop() {
    if (atomic_swap(&_stop, true) == false) {
        _epoll->signal();
        _ev.wait();
    }
}

void SchedulerImpl::main_func(tb_context_from_t from) {
    ((Coroutine*)from.priv)->ctx = from.ctx;
    gSched->running()->cb->run(); // run the coroutine function
    gSched->recycle();            // recycle the current coroutine
    tb_context_jump(from.ctx, 0); // jump back to the from context
}

/*
 *  scheduler thread:
 *
 *    resume(co) -> jump(co->ctx, main_co)
 *       ^             |
 *       |             v
 *  jump(main_co)  main_func(from): from.priv == main_co
 *    yield()          |
 *       |             v
 *       <-------- co->cb->run():  run on _stack
 */
void SchedulerImpl::resume(Coroutine* co) {
    tb_context_from_t from;
    _running = co;
    if (_stack == 0) {
        _stack = (char*) malloc(_stack_size);
        _stack_top = _stack + _stack_size;
    }

    if (co->ctx == 0) {
        // resume new coroutine
        co->ctx = tb_context_make(_stack, _stack_size, main_func);
        CO_DBG_LOG << "resume new co: " << co << " id: " << co->id;
        from = tb_context_jump(co->ctx, _main_co); // jump to main_func(from):  from.priv == _main_co

    } else {
        // remove timer before resume the coroutine
        if (!is_null_timer_id(co->it)) {
            CO_DBG_LOG << "del timer: " << co->it;
            _timer_mgr.del_timer(co->it);
            set_null_timer_id(co->it);
        }

        // resume suspended coroutine
        CO_DBG_LOG << "resume co: " << co << ", id: " <<  co->id << ", stack: " << co->stack.size();
        CHECK(_stack_top == (char*)co->ctx + co->stack.size());
        memcpy(co->ctx, co->stack.data(), co->stack.size()); // restore stack data
        from = tb_context_jump(co->ctx, _main_co); // jump back to where the user called yiled()
    }

    // yiled() was called in coroutine, the scheduler will jump back to resume()
    if (from.priv) {
        assert(_running == from.priv);
        _running->ctx = from.ctx;   // update context for the coroutine
        CO_DBG_LOG << "yield co: " << _running << " id: " << _running->id
                   << ", stack: " << (size_t)(_stack_top - (char*)from.ctx);
        this->save_stack(_running); // save stack data for the coroutine
    }
}

void SchedulerImpl::loop() {
    gSched = this;
    std::vector<Closure*> new_tasks;
    std::vector<Coroutine*> ready_tasks;

    while (!_stop) {
        int n = _epoll->wait(_wait_ms);
        if (_stop) break;

        if (unlikely(n == -1)) {
            ELOG << "epoll wait error: " << co::strerror();
            continue;
        }

        for (int i = 0; i < n; ++i) {
            auto& ev = (*_epoll)[i];
            if (_epoll->is_ev_pipe(ev)) {
                _epoll->handle_ev_pipe();
                continue;
            }

          #if defined(_WIN32)
            auto info = (IoEvent::PerIoInfo*) ev.lpOverlapped;
            auto co = (Coroutine*) info->co;
            if (atomic_compare_swap(&info->ios, 0, 1) == 0) {
                info->n = ev.dwNumberOfBytesTransferred;
                if (co->s == this) {
                    this->resume(co);
                } else {
                    ((SchedulerImpl*)co->s)->add_ready_task(co);
                }
            } else {
                free(info);
            }
          #elif defined(__linux__)
            int32 rco = 0, wco = 0;
            auto& ctx = co::get_sock_ctx(_epoll->user_data(ev));
            if ((ev.events & EPOLLIN)  || !(ev.events & EPOLLOUT)) rco = ctx.get_ev_read(this->id());
            if ((ev.events & EPOLLOUT) || !(ev.events & EPOLLIN))  wco = ctx.get_ev_write(this->id());
            if (rco) this->resume(_co_pool[rco]);
            if (wco) this->resume(_co_pool[wco]);
          #else
            this->resume((Coroutine*)_epoll->user_data(ev));
          #endif
        }

        CO_DBG_LOG << "> check tasks ready to resume..";
        do {
            _task_mgr.get_all_tasks(new_tasks, ready_tasks);

            if (!new_tasks.empty()) {
                CO_DBG_LOG << ">> resume new tasks, num: " << new_tasks.size();
                for (size_t i = 0; i < new_tasks.size(); ++i) {
                    this->resume(this->new_coroutine(new_tasks[i]));
                }
                new_tasks.clear();
            }

            if (!ready_tasks.empty()) {
                CO_DBG_LOG << ">> resume ready tasks, num: " << ready_tasks.size();
                for (size_t i = 0; i < ready_tasks.size(); ++i) {
                    this->resume(ready_tasks[i]);
                }
                ready_tasks.clear();
            }
        } while (0);

        CO_DBG_LOG << "> check timedout tasks..";
        do {
            _wait_ms = _timer_mgr.check_timeout(ready_tasks);

            if (!ready_tasks.empty()) {
                CO_DBG_LOG << ">> resume timedout tasks, num: " << ready_tasks.size();
                _timeout = true;
                for (size_t i = 0; i < ready_tasks.size(); ++i) {
                    this->resume(ready_tasks[i]);
                }
                _timeout = false;
                ready_tasks.clear();
            }
        } while (0);

        if (_running) _running = 0;
    }

    _ev.signal();
}

uint32 TimerManager::check_timeout(std::vector<Coroutine*>& res) {
    if (_timer.empty()) return (uint32)-1;

    int64 now_ms = now::ms();
    auto it = _timer.begin();
    for (; it != _timer.end(); ++it) {
        if (it->first > now_ms) break;
        Coroutine* co = it->second;
        if (!is_null_timer_id(co->it)) set_null_timer_id(co->it);
      #ifdef _WIN32
        auto info = (IoEvent::PerIoInfo*) co->ioinfo;
        if (info) { /* 2 for io_timeout */
            if (atomic_compare_swap(&info->ios, 0, 2) == 0) res.push_back(co);
        } else if (co->state == st_init || atomic_swap(&co->state, st_init) == st_wait) {
            res.push_back(co);
        }
      #else
        if (co->state == st_init || atomic_swap(&co->state, st_init) == st_wait) {
            res.push_back(co);
        }
      #endif
    }

    if (it != _timer.begin()) {
        if (_it != _timer.end() && _it->first <= now_ms) _it = it;
        _timer.erase(_timer.begin(), it);
    }

    if (_timer.empty()) return (uint32)-1;
    return (int) (_timer.begin()->first - now_ms);
}

#ifdef _WIN32
extern void wsa_startup();
extern void wsa_cleanup();

#else
inline void wsa_startup() {
#ifndef _CO_DISABLE_HOOK
    if (CO_RAW_API(close) == 0) ::close(-1);
    if (CO_RAW_API(read) == 0)  { auto r = ::read(-1, 0, 0);  (void)r; }
    if (CO_RAW_API(write) == 0) { auto r = ::write(-1, 0, 0); (void)r; }
    CHECK(CO_RAW_API(close) != 0);
    CHECK(CO_RAW_API(read) != 0);
    CHECK(CO_RAW_API(write) != 0);

  #ifdef __linux__
    if (CO_RAW_API(epoll_wait) == 0) ::epoll_wait(-1, 0, 0, 0);
    CHECK(CO_RAW_API(epoll_wait) != 0);
  #else
    if (CO_RAW_API(kevent) == 0) ::kevent(-1, 0, 0, 0, 0, 0);
    CHECK(CO_RAW_API(kevent) != 0);
  #endif
#endif
}

inline void wsa_cleanup() {}
#endif

inline bool& initialized() {
    static bool kInitialized = false;
    return kInitialized;
}

SchedulerManager::SchedulerManager() {
    wsa_startup();
    if (FLG_co_sched_num == 0 || FLG_co_sched_num > (uint32)os::cpunum()) FLG_co_sched_num = os::cpunum();
    if (FLG_co_stack_size == 0) FLG_co_stack_size = 1024 * 1024;

    _n = (uint32)-1;
    _r = static_cast<uint32>((1ULL << 32) % FLG_co_sched_num);
    _s = _r == 0 ? (FLG_co_sched_num - 1) : -1;

    for (uint32 i = 0; i < FLG_co_sched_num; ++i) {
        SchedulerImpl* s = new SchedulerImpl(i, FLG_co_sched_num, FLG_co_stack_size);
        s->start();
        _scheds.push_back(s);
    }

    initialized() = true;
}

SchedulerManager::~SchedulerManager() {
    for (size_t i = 0; i < _scheds.size(); ++i) delete (SchedulerImpl*)_scheds[i];
    wsa_cleanup();
}

void SchedulerManager::stop_all_schedulers() {
    for (size_t i = 0; i < _scheds.size(); ++i) {
        ((SchedulerImpl*)_scheds[i])->stop();
    }
}

void Scheduler::go(Closure* cb) {
    ((SchedulerImpl*)this)->add_new_task(cb);
}

void go(Closure* cb) {
    ((SchedulerImpl*) scheduler_manager()->next_scheduler())->add_new_task(cb);
}

const std::vector<Scheduler*>& all_schedulers() {
    return scheduler_manager()->all_schedulers();
}

Scheduler* scheduler() { return gSched; }

Scheduler* next_scheduler() {
    return scheduler_manager()->next_scheduler();
}

int scheduler_num() {
    if (initialized()) return (int) scheduler_manager()->all_schedulers().size();
    return os::cpunum();
}

int scheduler_id() {
    return gSched ? ((SchedulerImpl*)gSched)->id() : -1;
}

int coroutine_id() {
    return (gSched && gSched->running()) ? gSched->coroutine_id() : -1;
}

void add_timer(uint32 ms) {
    CHECK(gSched) << "MUST be called in coroutine..";
    gSched->add_timer(ms);
}

bool add_io_event(sock_t fd, io_event_t ev) {
    CHECK(gSched) << "MUST be called in coroutine..";
    return gSched->add_io_event(fd, ev);
}

void del_io_event(sock_t fd, io_event_t ev) {
    CHECK(gSched) << "MUST be called in coroutine..";
    return gSched->del_io_event(fd, ev);
}

void del_io_event(sock_t fd) {
    CHECK(gSched) << "MUST be called in coroutine..";
    gSched->del_io_event(fd);
}

void yield() {
    CHECK(gSched) << "MUST be called in coroutine..";
    gSched->yield();
}

void sleep(uint32 ms) {
    gSched ? gSched->sleep(ms) : sleep::ms(ms);
}

bool timeout() {
    return gSched && gSched->timeout();
}

bool on_stack(const void* p) {
    CHECK(gSched) << "MUST be called in coroutine..";
    return gSched->on_stack(p);
}

void stop() {
    return scheduler_manager()->stop_all_schedulers();
}

} // co
