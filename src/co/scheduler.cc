#include "co/co/scheduler.h"
#include "co/co/io_event.h"
#include "co/os.h"

DEF_uint32(co_sched_num, os::cpunum(), "#1 number of coroutine schedulers, default: os::cpunum()");
DEF_uint32(co_stack_size, 1024 * 1024, "#1 size of the stack shared by coroutines, default: 1M");

namespace co {
namespace xx {

timer_id_t null_timer_id;
__thread Scheduler* gSched = 0;

Scheduler::Scheduler(uint32 id, uint32 stack_size)
    : _id(id), _stack_size(stack_size), _stack(0), _stack_top(0), _running(0), 
      _wait_ms(-1), _co_pool(), _stop(false), _timeout(false) {
    // we can not use a coroutine with id of 0 on linux.
    _main_co = _co_pool.pop();
}

Scheduler::~Scheduler() {
    this->stop();
    free(_stack);
}

void Scheduler::stop() {
    if (atomic_swap(&_stop, true) == false) {
        _epoll.signal();
        _ev.wait();
        _epoll.close();
    }
}

void Scheduler::main_func(tb_context_from_t from) {
    ((Coroutine*)from.priv)->ctx = from.ctx;
    gSched->running()->cb->run();
    gSched->recycle(gSched->running()); // recycle the current coroutine
    tb_context_jump(from.ctx, 0);       // jump back to the from context
}

/*
 *  scheduler thread:
 *    resume(co) -> jump(co->ctx, main_co)
 *       ^             |
 *       |             v
 *  jump(main_co)  main_func(from): from.priv == main_co
 *    yield()          |
 *       |             v
 *       <-------- co->cb->run():  run on _stack
 */
void Scheduler::resume(Coroutine* co) {
    tb_context_from_t from;
    _running = co;
    if (_stack == 0) {
        _stack = (char*) malloc(_stack_size);
        _stack_top = _stack + _stack_size;
    }

    if (co->ctx == 0) {
        //assert(co->it == null_timer_id);
        co->ctx = tb_context_make(_stack, _stack_size, main_func);
        SOLOG << "resume new co: " << co->id << ", ctx: " << co->ctx;
        from = tb_context_jump(co->ctx, _main_co);
    } else {
        //if (co->it != null_timer_id) {
        // We can't compare co->it with null_timer_id directly, as it will cause 
        // a "debug assertion failed" error in dll debug mode on windows.
        if (*(void**)&co->it != *(void**)&null_timer_id) {
            SOLOG << "del timer: " << co->it;
            _timer_mgr.del_timer(co->it);
            co->it = null_timer_id;
        }
        SOLOG << "resume co: " <<  co->id << ", ctx: " << co->ctx << ", sd: " << co->stack.size();
        CHECK(_stack_top == (char*)co->ctx + co->stack.size());
        memcpy(co->ctx, co->stack.data(), co->stack.size()); // restore stack data
        from = tb_context_jump(co->ctx, _main_co);
    }

    if (from.priv) {
        assert(_running == from.priv);
        _running->ctx = from.ctx;   // update context for the coroutine
        SOLOG << "yield co: " << _running->id << ", ctx: " << from.ctx 
              << ", sd: " << (size_t)(_stack_top - (char*)from.ctx);
        this->save_stack(_running); // save stack data for the coroutine
    }
}

void Scheduler::loop() {
    gSched = this;
    std::vector<Closure*> new_tasks;
    std::vector<Coroutine*> ready_tasks;

    while (!_stop) {
        int n = _epoll.wait(_wait_ms);
        if (_stop) break;

        if (unlikely(n == -1)) {
            ELOG << "epoll wait error: " << co::strerror();
            continue;
        }

        SOLOG << "> epoll wait ret: " << n;
        for (int i = 0; i < n; ++i) {
            auto& ev = _epoll[i];
            if (_epoll.is_ev_pipe(ev)) {
                _epoll.handle_ev_pipe();
                continue;
            }

          #if defined(_WIN32)
            IoEvent::PerIoInfo* info = (IoEvent::PerIoInfo*) _epoll.user_data(ev);
            if (info->co) {
                info->n = ev.dwNumberOfBytesTransferred;
                this->resume((Coroutine*)info->co);
            } else {
                free(info);
            }
          #elif defined(__linux__)
            uint64 ud = _epoll.user_data(ev);
            if (ud >> 32) this->resume(_co_pool[ud >> 32]);
            if ((uint32)ud) this->resume(_co_pool[(uint32)ud]);
          #else
            this->resume((Coroutine*)_epoll.user_data(ev));
          #endif
        }

        SOLOG << "> check tasks ready to resume..";
        do {
            _task_mgr.get_all_tasks(new_tasks, ready_tasks);

            if (!new_tasks.empty()) {
                SOLOG << ">> resume new tasks, num: " << new_tasks.size();
                for (size_t i = 0; i < new_tasks.size(); ++i) {
                    this->resume(this->new_coroutine(new_tasks[i]));
                }
                new_tasks.clear();
            }

            if (!ready_tasks.empty()) {
                SOLOG << ">> resume ready tasks, num: " << ready_tasks.size();
                for (size_t i = 0; i < ready_tasks.size(); ++i) {
                    this->resume(ready_tasks[i]);
                }
                ready_tasks.clear();
            }
        } while (0);

        SOLOG << "> check timedout tasks..";
        do {
            _wait_ms = _timer_mgr.check_timeout(ready_tasks);

            if (!ready_tasks.empty()) {
                SOLOG << ">> resume timedout tasks, num: " << ready_tasks.size();
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

    this->cleanup();
    _ev.signal();
}

uint32 TimerManager::check_timeout(std::vector<Coroutine*>& res) {
    if (_timer.empty()) return -1;

    int64 now_ms = now::ms();
    auto it = _timer.begin();
    for (; it != _timer.end(); ++it) {
        if (it->first > now_ms) break;
        Coroutine* co = it->second;
        if (co->state == S_init || atomic_swap(&co->state, S_init) == S_wait) {
            co->it = null_timer_id;
            res.push_back(co);
        }
    }

    if (it != _timer.begin()) {
        if (_it != _timer.end() && _it->first <= now_ms) _it = it;
        _timer.erase(_timer.begin(), it);
    }

    if (_timer.empty()) return -1;
    return (int) (_timer.begin()->first - now_ms);
}

#ifdef _WIN32
extern void wsa_startup();
extern void wsa_cleanup();
inline void init_hooks() {}
#else
inline void wsa_startup() {}
inline void wsa_cleanup() {}

void init_hooks() {
    if (raw_close == 0) {
  #ifdef __linux__
        ::epoll_wait(-1, 0, 0, 0);
        CHECK(raw_epoll_wait != 0);
  #else
        ::kevent(-1, 0, 0, 0, 0, 0);
        CHECK(raw_kevent != 0);
  #endif
        ::close(-1);
        ::read(-1, 0, 0);
        ::write(-1, 0, 0);
    }
}
#endif

SchedulerManager::SchedulerManager() {
    wsa_startup();
    init_hooks();
    CHECK_EQ(sizeof(null_timer_id), sizeof(void*));
    if (FLG_co_sched_num == 0 || FLG_co_sched_num > (uint32)os::cpunum()) FLG_co_sched_num = os::cpunum();
    if (FLG_co_stack_size == 0) FLG_co_stack_size = 1024 * 1024;

    _n = -1;
    _r = static_cast<uint32>((1ULL << 32) % FLG_co_sched_num);
    _s = _r == 0 ? (FLG_co_sched_num - 1) : -1;

    for (uint32 i = 0; i < FLG_co_sched_num; ++i) {
        Scheduler* s = new Scheduler(i, FLG_co_stack_size);
        s->start();
        _scheds.push_back(s);
    }
}

SchedulerManager::~SchedulerManager() {
    for (size_t i = 0; i < _scheds.size(); ++i) delete _scheds[i];
    wsa_cleanup();
}

void SchedulerManager::stop_all_schedulers() {
    for (size_t i = 0; i < _scheds.size(); ++i) _scheds[i]->stop();
}

} // xx
} // co
