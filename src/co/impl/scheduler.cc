#include "scheduler.h"
#include "co/os.h"

DEF_uint32(co_sched_num, os::cpunum(), "#1 number of coroutine schedulers, default: cpu num");
DEF_uint32(co_stack_size, 1024 * 1024, "#1 size of the stack shared by coroutines, default: 1M");

namespace co {

const timer_id_t null_timer_id;
__thread Scheduler* gSched = 0;

Scheduler::Scheduler(uint32 id, uint32 stack_size)
    : _id(id), _stack_size(stack_size), _stack(0), _running(0), 
      _wait_ms(-1), _co_pool(), _stop(false), _timeout(false) {
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

static void main_func(tb_context_from_t from) {
    ((Coroutine*)from.priv)->ctx = from.ctx;
    gSched->running()->cb->run();
    gSched->recycle(gSched->running()); // recycle the current coroutine
    tb_context_jump(from.ctx, 0);       // jump back to the main context
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
    SOLOG << "resume co: " << co->id;
    tb_context_from_t from;
    _running = co;
    if (_stack == 0) _stack = (char*) malloc(FLG_co_stack_size);

    if (co->ctx == 0) {
        co->ctx = tb_context_make(_stack, FLG_co_stack_size, main_func);
        from = tb_context_jump(co->ctx, _main_co);
    } else {
        // restore stack for the coroutine
        assert((_stack + FLG_co_stack_size) == (char*)co->ctx + co->stack.size());
        memcpy(co->ctx, co->stack.data(), co->stack.size());
        from = tb_context_jump(co->ctx, _main_co);
    }

    if (from.priv) {
        assert(_running == from.priv);
        _running->ctx = from.ctx;   // update context for the coroutine
        this->save_stack(_running); // save stack of the coroutine        
    }
}

void Scheduler::loop() {
    gSched = this;
    std::vector<Closure*> task_cb;
    std::vector<Coroutine*> task_co;
    std::unordered_map<Coroutine*, timer_id_t> timer_tasks;

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
            Coroutine* co = (Coroutine*) _epoll.ud(ev);
            if (co) this->resume(co);
          #elif defined(__linux__)
            uint64 ud = _epoll.ud(ev);
            if (ud >> 32) this->resume(_co_pool[ud >> 32]);
            if ((uint32)ud) this->resume(_co_pool[(uint32)ud]);
          #else
            this->resume((Coroutine*)_epoll.ud(ev));
          #endif
        }

        SOLOG << "> check newly or ready tasks..";
        do {
            _task_mgr.get_tasks(task_cb, task_co);

            if (!task_cb.empty()) {
                for (size_t i = 0; i < task_cb.size(); ++i) {
                    this->resume(this->new_coroutine(task_cb[i]));
                }
                task_cb.clear();
            }

            if (!task_co.empty()) {
                for (size_t i = 0; i < task_co.size(); ++i) {
                    this->resume(task_co[i]);
                }
                task_co.clear();
            }
        } while (0);

        SOLOG << "> check ready timer tasks..";
        do {
            _timer_mgr.get_tasks(timer_tasks);
            if (!timer_tasks.empty()) {
                for (auto it = timer_tasks.begin(); it != timer_tasks.end(); ++it) {
                    if (it->first->state == S_ready) this->del_timer(it->second);
                    this->resume(it->first);
                }
                timer_tasks.clear();
            }
        } while (0);

        SOLOG << "> check timeout tasks..";
        do {
            _wait_ms = _timer_mgr.check_timeout(task_co);
            SOLOG << ">> timeout n: " << task_co.size();

            if (!task_co.empty()) {
                _timeout = true;
                for (size_t i = 0; i < task_co.size(); ++i) {
                    this->resume(task_co[i]);
                }
                _timeout = false;
                task_co.clear();
            }
        } while (0);
    }

    _ev.signal();
}

uint32 TimerManager::check_timeout(std::vector<Coroutine*>& res) {
    if (_timer.empty()) return -1;

    SOLOG << ">> check timeout, timed_wait num: " << _timer.size();
    int64 now_ms = now::ms();

    auto it = _timer.begin();
    for (; it != _timer.end(); ++it) {
        if (it->first > now_ms) break;
        Coroutine* co = it->second;
        if (co->state == S_init || atomic_swap(&co->state, S_init) == S_wait) {
            SOLOG << ">>> timedout timer: " << COTID(it);
            res.push_back(co);
        }
    }

    if (it != _timer.begin()) {
        if (_it != _timer.end() && _it->first <= now_ms) {
            _it = it;
        }
        _timer.erase(_timer.begin(), it);
    }
    SOLOG << ">> check timeout, remain timed_wait num: " << _timer.size();

    if (_timer.empty()) return -1;
    return (int) (_timer.begin()->first - now_ms);
}

#ifdef _WIN32
extern void wsa_startup();
extern void wsa_cleanup();
#else
inline void wsa_startup() {}
inline void wsa_cleanup() {}
#endif

SchedManager::SchedManager() {
    if (FLG_co_sched_num == 0) FLG_co_sched_num = os::cpunum();
    if (FLG_co_sched_num > (uint32)co::max_sched_num()) FLG_co_sched_num = co::max_sched_num();
    if (FLG_co_stack_size == 0) FLG_co_stack_size = 1024 * 1024;

    _n = -1;
    _r = static_cast<uint32>((1ULL << 32) % FLG_co_sched_num);
    _s = _r == 0 ? (FLG_co_sched_num - 1) : -1;

    wsa_startup();

    LOG << "coroutine schedulers start, sched num: " << FLG_co_sched_num
        << ", stack size: " << (FLG_co_stack_size >> 10) << 'k';

    for (uint32 i = 0; i < FLG_co_sched_num; ++i) {
        Scheduler* s = new Scheduler(i, FLG_co_stack_size);
        s->loop_in_thread();
        _scheds.push_back(s);
    }
}

SchedManager::~SchedManager() {
    for (size_t i = 0; i < _scheds.size(); ++i) {
        delete _scheds[i];
    }
    wsa_cleanup();
}

void SchedManager::stop() {
    for (size_t i = 0; i < _scheds.size(); ++i) {
        _scheds[i]->stop();
    }
}

} // co
