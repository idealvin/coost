#include "sched.h"
#include "co/os.h"
#include "co/rand.h"
#include <mutex>

DEF_uint32(co_sched_num, os::cpunum(), ">>#1 number of coroutine schedulers");
DEF_uint32(co_stack_num, 8, ">>#1 number of stacks per scheduler, must be power of 2");
DEF_uint32(co_stack_size, 1024 * 1024, ">>#1 size of the stack shared by coroutines");
DEF_bool(co_sched_log, false, ">>#1 print logs for coroutine schedulers");

#ifdef _MSC_VER
extern LONG WINAPI _co_on_exception(PEXCEPTION_POINTERS p);
#endif

namespace co {

#ifdef _WIN32
extern void init_sock();
extern void cleanup_sock();
#else
inline void init_sock() {}
inline void cleanup_sock() {}
#endif

co::table<SockCtx>* g_ctx_tb;

namespace xx {

__thread Sched* gSched = 0;

Sched::Sched(uint32 id, uint32 sched_num, uint32 stack_num, uint32 stack_size)
    : _cputime(0), _task_mgr(), _timer_mgr(), _wait_ms(-1), _timeout(false),
      _bufs(128), _co_pool(), _running(0), _id(id), _sched_num(sched_num),
      _stack_num(stack_num), _stack_size(stack_size) {
    new(&_x.ev) co::sync_event();
    _x.epoll = co::make<Epoll>(id);
    _x.stopped = false;
    _main_co = _co_pool.pop(); // id 0 is reserved for _main_co
    _main_co->sched = this;
    _stack = (Stack*) co::zalloc(stack_num * sizeof(Stack));
}

Sched::~Sched() {
    this->stop();
    co::del(_x.epoll);
    _x.ev.~sync_event();
    for (size_t i = 0; i < _bufs.size(); ++i) {
        void* p = _bufs[i];
        god::cast<Buffer*>(&p)->reset();
    }
    _bufs.clear();
    co::free(_stack, _stack_num * sizeof(Stack));
}

static int g_cnt = 0;

void Sched::stop() {
    const int n = atomic_inc(&g_cnt, mo_relaxed);
    if (atomic_swap(&_x.stopped, true, mo_acq_rel) == false) {
        _x.epoll->signal();
      #if defined(_WIN32) && defined(BUILDING_CO_SHARED)
        if (n == 1) {
            // the thread may not respond in dll, wait at most 64ms here
            co::Timer t;
            while (!_x.ev.wait(1) && t.ms() < 64);
        }
      #else
        _x.ev.wait();
      #endif
    }
}

void Sched::main_func(tb_context_from_t from) {
    ((Coroutine*)from.priv)->ctx = from.ctx;
  #ifdef _MSC_VER
    __try {
        ((Coroutine*)from.priv)->sched->running()->cb->run();
    } __except(_co_on_exception(GetExceptionInformation())) {
    }
  #else
    ((Coroutine*)from.priv)->sched->running()->cb->run();
  #endif // _WIN32
    tb_context_jump(from.ctx, 0); // jump back to the from context
}

/*
 *  scheduling thread:
 *
 *    resume(co) -> jump(co->ctx, main_co)
 *       ^             |
 *       |             v
 *  jump(main_co)  main_func(from): from.priv == main_co
 *    yield()          |
 *       |             v
 *       <-------- co->cb->run():  run on _stack
 */
void Sched::resume(Coroutine* co) {
    CHECK_EQ(co->sched, this);
    tb_context_from_t from;
    Stack* const s = co->stack;
    _running = co;
    if (s->p == 0) {
        s->p = (char*) co::alloc(_stack_size);
        s->top = s->p + _stack_size;
        s->co = co;
    }

    if (co->ctx == 0) {
        // resume new coroutine
        if (s->co != co) { this->save_stack(s->co); s->co = co; }
        co->ctx = tb_context_make(s->p, _stack_size, main_func);
        SCHEDLOG << "resume new co: " << co << " id: " << co->id;
        from = tb_context_jump(co->ctx, _main_co); // jump to main_func(from):  from.priv == _main_co

    } else {
        // remove timer before resume the coroutine
        if (co->it != _timer_mgr.end()) {
            SCHEDLOG << "del timer: " << co->it;
            _timer_mgr.del_timer(co->it);
            co->it = _timer_mgr.end();
        }

        // resume suspended coroutine
        SCHEDLOG << "resume co: " << co << " id: " <<  co->id << " stack: " << co->buf.size();
        if (s->co != co) {
            this->save_stack(s->co);
            CHECK_EQ(s->top, (char*)co->ctx + co->buf.size());
            memcpy(co->ctx, co->buf.data(), co->buf.size()); // restore stack data
            s->co = co;
        }
        from = tb_context_jump(co->ctx, _main_co); // jump back to where yiled() was called
    }

    if (from.priv) {
        // yield() was called in the coroutine, update context for it
        assert(_running == from.priv);
        _running->ctx = from.ctx;
        SCHEDLOG << "yield co: " << _running << " id: " << _running->id;
    } else {
        // the coroutine has terminated, recycle it
        _running->stack->co = 0;
        this->recycle(_running);
    }
}

void Sched::loop() {
    gSched = this;
    co::vector<Closure*> new_tasks(512);
    co::vector<Coroutine*> ready_tasks(512);
    co::Timer timer;

    while (!_x.stopped) {
        int n = _x.epoll->wait(_wait_ms);
        if (_x.stopped) break;

        if (unlikely(n == -1)) {
            if (co::error() != EINTR) ELOG << "epoll wait error: " << co::strerror();
            continue;
        }

        if (_sched_num > 1) timer.restart();
        SCHEDLOG << "> check I/O tasks ready to resume, num: " << n;

        for (int i = 0; i < n; ++i) {
            auto& ev = (*_x.epoll)[i];
            if (_x.epoll->is_ev_pipe(ev)) {
                _x.epoll->handle_ev_pipe();
                continue;
            }

          #if defined(_WIN32)
            auto info = xx::per_io_info(ev.lpOverlapped);
            auto co = (Coroutine*) info->co;
            if (atomic_bool_cas(&info->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                info->n = ev.dwNumberOfBytesTransferred;
                if (co->sched == this) {
                    this->resume(co);
                } else {
                    co->sched->add_ready_task(co);
                }
            } else {
                co::free(info, info->mlen);
            }
          #elif defined(__linux__)
            int32 rco = 0, wco = 0;
            auto& ctx = co::get_sock_ctx(_x.epoll->user_data(ev));
            if ((ev.events & EPOLLIN)  || !(ev.events & EPOLLOUT)) rco = ctx.get_ev_read(this->id());
            if ((ev.events & EPOLLOUT) || !(ev.events & EPOLLIN))  wco = ctx.get_ev_write(this->id());
            if (rco) this->resume(&_co_pool[rco]);
            if (wco) this->resume(&_co_pool[wco]);
          #else
            this->resume((Coroutine*)_x.epoll->user_data(ev));
          #endif
        }

        SCHEDLOG << "> check tasks ready to resume..";
        do {
            _task_mgr.get_all_tasks(new_tasks, ready_tasks);

            if (!new_tasks.empty()) {
                const size_t c = new_tasks.capacity();
                const size_t s = new_tasks.size();
                SCHEDLOG << ">> resume new tasks, num: " << s;
                for (size_t i = 0; i < s; ++i) {
                    this->resume(this->new_coroutine(new_tasks[i]));
                }
                if (c >= 8192 && s <= (c >> 1)) {
                    co::vector<Closure*>(s).swap(new_tasks);
                }
                new_tasks.clear();
            }

            if (!ready_tasks.empty()) {
                const size_t c = ready_tasks.capacity();
                const size_t s = ready_tasks.size();
                SCHEDLOG << ">> resume ready tasks, num: " << s;
                for (size_t i = 0; i < s; ++i) {
                    this->resume(ready_tasks[i]);
                }
                if (c >= 8192 && s <= (c >> 1)) {
                    co::vector<Coroutine*>(s).swap(ready_tasks);
                }
                ready_tasks.clear();
            }
        } while (0);

        SCHEDLOG << "> check timedout tasks..";
        do {
            _wait_ms = _timer_mgr.check_timeout(ready_tasks);

            if (!ready_tasks.empty()) {
                SCHEDLOG << ">> resume timedout tasks, num: " << ready_tasks.size();
                _timeout = true;
                for (size_t i = 0; i < ready_tasks.size(); ++i) {
                    this->resume(ready_tasks[i]);
                }
                _timeout = false;
                ready_tasks.clear();
            }
        } while (0);

        if (_running) _running = 0;
        if (_sched_num > 1) atomic_add(&_cputime, timer.us(), mo_relaxed);
    }

    _x.ev.signal();
}

uint32 TimerManager::check_timeout(co::vector<Coroutine*>& res) {
    if (_timer.empty()) return (uint32)-1;

    int64 now_ms = now::ms();
    auto it = _timer.begin();
    for (; it != _timer.end(); ++it) {
        if (it->first > now_ms) break;
        Coroutine* co = it->second;
        if (co->it != _timer.end()) co->it = _timer.end();
        if (!co->waitx) {
            res.push_back(co);
        } else {
            auto w = co->waitx;
            // TODO: is mo_relaxed safe here?
            if (atomic_bool_cas(&w->state, st_wait, st_timeout, mo_relaxed, mo_relaxed)) {
                res.push_back(co);
            }
        }
    }

    if (it != _timer.begin()) {
        if (_it != _timer.end() && _it->first <= now_ms) _it = it;
        _timer.erase(_timer.begin(), it);
    }

    return _timer.empty() ? (uint32)-1 : (uint32)(_timer.begin()->first - now_ms);
}

struct SchedInfo {
    SchedInfo() : cputime(co::sched_num(), 0), seed(co::rand()) {}
    co::vector<int64> cputime;
    uint32 seed;
};

static __thread SchedInfo* g_si;

inline SchedInfo& sched_info() {
    return g_si ? *g_si : *(g_si = co::_make_static<SchedInfo>());
}

static uint32 g_nco = 0;
static bool g_main_thread_as_sched;

SchedManager::SchedManager() {
    co::init_sock();

    const uint32 ncpu = os::cpunum();
    auto& n = FLG_co_sched_num;
    auto& m = FLG_co_stack_num;
    auto& s = FLG_co_stack_size;
    if (n == 0 || n > ncpu) n = ncpu;
    if (m == 0 || (m & (m - 1)) != 0) m = 8;
    if (s == 0) s = 1024 * 1024;

    if (n != 1) {
        if ((n & (n - 1)) == 0) {
            _next = [](const co::vector<Sched*>& v) {
                if (g_nco < v.size()) {
                    const uint32 i = atomic_fetch_inc(&g_nco);
                    if (i < v.size()) return v[i];
                }
                auto& si = sched_info();
                const uint32 x = god::cast<uint32>(v.size() - 1);
                const uint32 i = co::rand(si.seed) & x;
                const uint32 k = i != x ? i + 1 : 0;
                const int64 ti = v[i]->cputime();
                const int64 tk = v[k]->cputime();
                return (si.cputime[k] == tk || ti <= (si.cputime[k] = tk)) ? v[i] : v[k];
            };
        } else {
            _next = [](const co::vector<Sched*>& v) {
                if (g_nco < v.size()) {
                    const uint32 i = atomic_fetch_inc(&g_nco);
                    if (i < v.size()) return v[i];
                }
                auto& si = sched_info();
                const uint32 x = god::cast<uint32>(v.size());
                const uint32 i = co::rand(si.seed) % x;
                const uint32 k = i != x - 1 ? i + 1 : 0;
                const int64 ti = v[i]->cputime();
                const int64 tk = v[k]->cputime();
                return (si.cputime[k] == tk || ti <= (si.cputime[k] = tk)) ? v[i] : v[k];
            };
        }
    } else {
        _next = [](const co::vector<Sched*>& v) { return v[0]; };
    }

    for (uint32 i = 0; i < n; ++i) {
        Sched* sched = co::_make_static<Sched>(i, n, m, s);
        if (i != 0 || !g_main_thread_as_sched) sched->start();
        _scheds.push_back(sched);
    }

    is_active() = true;
}

SchedManager::~SchedManager() {
    this->stop();
    co::cleanup_sock();
}

std::once_flag g_sched_man_flag;
static SchedManager* g_sched_man;

inline SchedManager* sched_man() {
    std::call_once(g_sched_man_flag, []() {
        g_sched_man = co::_make_static<SchedManager>();
    });
    return g_sched_man;
}

static int g_sched_nifty;
SchedInitializer::SchedInitializer() {
    if (g_sched_nifty++ == 0) {
        g_ctx_tb = co::_make_static<co::table<SockCtx>>(15, 16);
    }
}

SchedInitializer::~SchedInitializer() {
}

void SchedManager::stop() {
    for (size_t i = 0; i < _scheds.size(); ++i) {
        _scheds[i]->stop();
    }
    atomic_swap(&is_active(), false, mo_acq_rel);
}

} // xx

void go(Closure* cb) {
    xx::sched_man()->next_sched()->add_new_task(cb);
}

void co::Sched::go(Closure* cb) {
    ((xx::Sched*)this)->add_new_task(cb);
}

void co::MainSched::loop() {
    ((xx::Sched*)this)->loop();
}

const co::vector<co::Sched*>& scheds() {
    return (co::vector<co::Sched*>&) xx::sched_man()->scheds();
}

int sched_num() {
    return xx::is_active() ? (int)xx::sched_man()->scheds().size() : os::cpunum();
}

co::Sched* sched() { return (co::Sched*) xx::gSched; }

co::Sched* next_sched() {
    return (co::Sched*) xx::sched_man()->next_sched();
}

co::MainSched* main_sched() {
    xx::g_main_thread_as_sched = true;
    return (co::MainSched*) xx::sched_man()->scheds()[0];
}

void* coroutine() {
    const auto s = xx::gSched;
    return s ? s->running() : 0;
}

int sched_id() {
    const auto s = xx::gSched;
    return s ? s->id() : -1;
}

int coroutine_id() {
    const auto s = xx::gSched;
    return (s && s->running()) ? s->coroutine_id() : -1;
}

void add_timer(uint32 ms) {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    s->add_timer(ms);
}

bool add_io_event(sock_t fd, _ev_t ev) {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    return s->add_io_event(fd, ev);
}

void del_io_event(sock_t fd, _ev_t ev) {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    return s->del_io_event(fd, ev);
}

void del_io_event(sock_t fd) {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    s->del_io_event(fd);
}

void yield() {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    s->yield();
}

void resume(void* p) {
    const auto co = (xx::Coroutine*)p;
    co->sched->add_ready_task(co);
}

void sleep(uint32 ms) {
    const auto s = xx::gSched;
    s ? s->sleep(ms) : sleep::ms(ms);
}

bool timeout() {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    return s && s->timeout();
}

bool on_stack(const void* p) {
    const auto s = xx::gSched;
    CHECK(s) << "MUST be called in coroutine..";
    return s->on_stack(p);
}

void stop_scheds() {
    xx::sched_man()->stop();
}

} // co
