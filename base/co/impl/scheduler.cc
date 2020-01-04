#include "scheduler.h"

DEF_uint32(co_sched_num, os::cpunum(), "number of coroutine schedulers, default: cpu num");
DEF_uint32(co_stack_size, 1024 * 1024, "size of the stack shared by coroutines, default: 1M");

namespace co {

const timer_id_t null_timer_id;
__thread Scheduler* gSched = 0;

Scheduler::Scheduler(uint32 id)
    : _id(id), _stack(0), _running(0), _wait_ms(-1),
      _stop(false), _timeout(false) {
    _main_co = new Coroutine();
    _co_pool.reserve(1024);
    _co_pool.push_back(_main_co);
    _co_ids.reserve(1024);
    _it = _timer.end();
}

Scheduler::~Scheduler() {
    this->stop();
    free(_stack);
    for (size_t i = 0; i < _co_pool.size(); ++i) {
        delete _co_pool[i];
    }
}

void Scheduler::stop() {
    if (atomic_swap(&_stop, true) == false) {
        _epoll.signal();
        _ev.wait();
    }
}

static void main_func(tb_context_from_t from) {
    ((Coroutine*)from.priv)->ctx = from.ctx;
    gSched->running()->cb->run();
    gSched->recycle(gSched->running()); // recycle the current coroutine
    tb_context_jump(from.ctx, 0);       // jump back to the main context
}

inline void Scheduler::save_stack(Coroutine* co) {
    if (co->stack) {
        co->stack->clear();
    } else {
        co->stack = new fastream(_stack + FLG_co_stack_size - (char*)co->ctx);
    }
    co->stack->append(co->ctx, _stack + FLG_co_stack_size - (char*)co->ctx);
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
    if (_stack == 0) _stack = (char*) malloc(FLG_co_stack_size);

    if (co->ctx == 0) {
        co->ctx = tb_context_make(_stack, FLG_co_stack_size, main_func);
        from = tb_context_jump(co->ctx, _main_co);
    } else {
        // restore stack for the coroutine
        assert((_stack + FLG_co_stack_size) == (char*)co->ctx + co->stack->size());
        memcpy(co->ctx, co->stack->data(), co->stack->size());
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
    std::unordered_map<Coroutine*, timer_id_t> timer_task_co;

    while (!_stop) {
        int n = _epoll.wait(_wait_ms);
        if (_stop) break;

        if (unlikely(n == -1)) {
            ELOG << "epoll wait error: " << co::strerror();
            continue;
        }

        for (int i = 0; i < n; ++i) {
            auto& ev = _epoll[i];
            if (_epoll.is_ev_pipe(ev)) {
                _epoll.handle_ev_pipe();
                continue;
            }

          #if defined(_WIN32)
            PerIoInfo* info = (PerIoInfo*) _epoll.ud(ev);
            info->n = ev.dwNumberOfBytesTransferred;
            if (info->co) this->resume(info->co);
          #elif defined(__linux__)
            this->resume(_co_pool[_epoll.ud(ev)]);
          #else
            this->resume((Coroutine*)_epoll.ud(ev));
          #endif
        }

        do {
            {
                ::MutexGuard g(_task_mtx);
                if (!_task_cb.empty()) _task_cb.swap(task_cb);
                if (!_task_co.empty()) _task_co.swap(task_co);
            }

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

        do {
            {
                ::MutexGuard g(_timer_task_mtx);
                if (!_timer_task_co.empty()) _timer_task_co.swap(timer_task_co);
            }
            if (!timer_task_co.empty()) {
                for (auto it = timer_task_co.begin(); it != timer_task_co.end(); ++it) {
                    if (it->first->ev != ev_ready) continue;
                    if (it->second != null_timer_id) _timer.erase(it->second);
                    this->resume(it->first);
                }
                timer_task_co.clear();
            }
        } while (0);

        do {
            this->check_timeout(task_co);

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

void Scheduler::check_timeout(std::vector<Coroutine*>& res) {
    if (_timer.empty()) {
        if (_wait_ms != (uint32)-1) _wait_ms = -1;
        return;
    }

    do {
        int64 now_ms = now::ms();

        auto it = _timer.begin();
        for (; it != _timer.end(); ++it) {
            if (it->first > now_ms) break;
            Coroutine* co = it->second;
            if (co->ev != 0) atomic_swap(&co->ev, 0);
            res.push_back(co);
        }

        if (it != _timer.begin()) {
            if (_it != _timer.end() && _it->first <= now_ms) {
                _it = it;
            }
            _timer.erase(_timer.begin(), it);
        }

        if (!_timer.empty()) {
            _wait_ms = (int) (_timer.begin()->first - now_ms);
        } else {
            if (_wait_ms != (uint32)-1) _wait_ms = -1;
        }
    } while (0);
}

SchedulerMgr::SchedulerMgr() : _index(-1) {
    if (FLG_co_sched_num > (uint32) os::cpunum()) FLG_co_sched_num = os::cpunum();
    if (FLG_co_stack_size == 0) FLG_co_stack_size = 1024 * 1024;

    _n = FLG_co_sched_num;
    (_n && (_n - 1)) ? (_n = -1) : --_n;

  #ifdef _WIN32
    _Wsa_startup();
  #endif

    LOG << "coroutine schedulers start, sched num: " << FLG_co_sched_num
        << ", stack size: " << (FLG_co_stack_size >> 10) << 'k';

    for (uint32 i = 0; i < FLG_co_sched_num; ++i) {
        Scheduler* s = new Scheduler(i);
        s->loop_in_thread();
        _scheds.push_back(s);
    }
}

SchedulerMgr::~SchedulerMgr() {
    for (size_t i = 0; i < _scheds.size(); ++i) {
        delete _scheds[i];
    }

  #ifdef _WIN32
    _Wsa_cleanup();
  #endif
}

inline SchedulerMgr& sched_mgr() {
    static SchedulerMgr kSchedMgr;
    return kSchedMgr;
}

void go(Closure* cb) {
    sched_mgr()->add_task(cb);
}

void sleep(uint32 ms) {
    gSched != 0 ? gSched->sleep(ms) : sleep::ms(ms);
}

void stop() {
    auto& scheds = co::schedulers();
    for (size_t i = 0; i < scheds.size(); ++i) {
        scheds[i]->stop();
    }
}

std::vector<Scheduler*>& schedulers() {
    return sched_mgr().schedulers();
}

int sched_id() {
    return gSched->id();
}

} // co
