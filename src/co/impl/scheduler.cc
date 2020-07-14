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
    SOLOG << ">>> resume co: " << co->id;
    tb_context_from_t from;
    _running = co;
    if (_stack == 0) _stack = (char*) malloc(_stack_size);

    if (co->ctx == 0) {
        co->ctx = tb_context_make(_stack, _stack_size, main_func);
        from = tb_context_jump(co->ctx, _main_co);
    } else {
        // restore stack for the coroutine
        assert((_stack + _stack_size) == (char*)co->ctx + co->stack.size());
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
    std::vector<Closure*> new_tasks;
    std::vector<Coroutine*> ready_tasks;
    std::unordered_map<Coroutine*, timer_id_t> ready_timer_tasks;

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

        SOLOG << "> check tasks ready to resume..";
        do {
            _task_mgr.get_all_tasks(new_tasks, ready_tasks, ready_timer_tasks);

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

            if (!ready_timer_tasks.empty()) {
                SOLOG << ">> resume ready timer tasks, num: " << ready_timer_tasks.size();
                for (auto it = ready_timer_tasks.begin(); it != ready_timer_tasks.end(); ++it) {
                    if (it->first->state == S_ready) this->del_timer(it->second);
                    this->resume(it->first);
                }
                ready_timer_tasks.clear();
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

        if (_running) _running = NULL;
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
#else
static inline void wsa_startup() {}
static inline void wsa_cleanup() {}
#endif

SchedManager::SchedManager() {
    wsa_startup();
    if (FLG_co_sched_num == 0) FLG_co_sched_num = os::cpunum();
    if (FLG_co_sched_num > (uint32)co::max_sched_num()) FLG_co_sched_num = co::max_sched_num();
    if (FLG_co_stack_size == 0) FLG_co_stack_size = 1024 * 1024;

    _n = -1;
    _r = static_cast<uint32>((1ULL << 32) % FLG_co_sched_num);
    _s = _r == 0 ? (FLG_co_sched_num - 1) : -1;

    LOG << "coroutine schedulers start, sched num: " << FLG_co_sched_num
        << ", stack size: " << (FLG_co_stack_size >> 10) << 'k';

    for (uint32 i = 0; i < FLG_co_sched_num; ++i) {
        Scheduler* s = new Scheduler(i, FLG_co_stack_size);
        s->loop_in_thread();
        _scheds.push_back(s);
    }
}

SchedManager::~SchedManager() {
    for (size_t i = 0; i < _scheds.size(); ++i) delete _scheds[i];
    wsa_cleanup();
}

void SchedManager::stop() {
    for (size_t i = 0; i < _scheds.size(); ++i) _scheds[i]->stop();
}

} // co
