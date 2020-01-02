#pragma once

#include "epoll.h"
#include "../co.h"
#include "../context.h"

#include "../../log.h"
#include "../../flag.h"
#include "../../atomic.h"
#include "../../fastream.h"
#include "../../thread.h"
#include "../../time.h"
#include "../../os.h"

#include <assert.h>
#include <string.h>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

DEC_uint32(co_sched_num);
DEC_uint32(co_stack_size);

namespace co {

class Scheduler;
extern __thread Scheduler* gSched;

std::vector<Scheduler*>& schedulers();

enum _Event_status {
    ev_wait = 1,
    ev_ready = 2,
};

struct Coroutine {
    Coroutine()
        : id(0), ev(0), ctx(0), stack(0), cb(0) {
    }

    Coroutine(int i, Closure* c)
        : id(i), ev(0), ctx(0), stack(0), cb(c) {
    }

    ~Coroutine() {
        if (stack) delete stack;
    }

    int id; // coroutine id
    int ev; // event status

    tb_context_t ctx;  // context, a pointer points to the stack bottom
    fastream* stack;   // save stack data for this coroutine

    union {
        Closure* cb;   // coroutine function
        Scheduler* s;  // scheduler this coroutines runs in
    };
};

#ifdef _WIN32
void _Wsa_startup();
void _Wsa_cleanup();

struct PerIoInfo {
    PerIoInfo(const void* data, int size, Coroutine* c)
        : n(0), flags(0), co(c), s(0) {
        memset(&ol, 0, sizeof(ol));
        buf.buf = (char*) data;
        buf.len = size;
    }

    PerIoInfo(int size, Coroutine* c)
        : n(0), flags(0), co(c), s((char*)malloc(size)) {
        memset(&ol, 0, sizeof(ol));
        buf.buf = s;
        buf.len = size;
    }

    ~PerIoInfo() {
        if (s) free(s);
    }

    void move(DWORD n) {
        buf.buf += n;
        buf.len -= (ULONG) n;
    }

    void resetol() {
        memset(&ol, 0, sizeof(ol));
    }

    WSAOVERLAPPED ol;
    DWORD n;            // bytes transfered
    DWORD flags;        // flags for WSARecv
    Coroutine* co;      // user data, pointer to a coroutine
    char* s;            // dynamic allocated buffer
    WSABUF buf;
};
#endif

typedef std::multimap<int64, Coroutine*>::iterator timer_id_t;
extern timer_id_t null_timer_id;

class Scheduler {
  public:
    Scheduler(uint32 id);
    ~Scheduler();

    uint32 id() const { return _id; }

    Coroutine* running() const { return _running; }

    void resume(Coroutine* co);

    void yield() {
        tb_context_jump(_main_co->ctx, _running);
    }

    void sleep(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        _timer.insert(std::make_pair(now::ms() + ms, _running));
        this->yield();
    }

    timer_id_t add_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        return _it = _timer.insert(_it, std::make_pair(now::ms() + ms, _running));
    }

    timer_id_t add_ev_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        return _timer.insert(std::make_pair(now::ms() + ms, _running));
    }

    void del_timer(const timer_id_t& id) {
        if (_it == id) ++_it;
        _timer.erase(id);
    }

    void loop();

    void loop_in_thread() {
        Thread(&Scheduler::loop, this).detach();
    }

    void stop();

    void add_task(void (*f)());
    void add_task(Closure* cb);
    void add_task(Coroutine* co);
    void add_task(Coroutine* co, timer_id_t id);

  #if defined(_WIN32)
    bool add_ev_read(sock_t fd) {
        return _epoll.add_ev_read(fd);
    }

    bool add_ev_write(sock_t fd) {
        return _epoll.add_ev_write(fd);
    }

  #elif defined(__linux__)
    bool add_ev_read(sock_t fd) {
        return _epoll.add_ev_read(fd, _running->id);
    }

    bool add_ev_write(sock_t fd) {
        return _epoll.add_ev_write(fd, _running->id);
    }

  #else
    bool add_ev_read(sock_t fd) {
        return _epoll.add_ev_read(fd, _running);
    }

    bool add_ev_write(sock_t fd) {
        return _epoll.add_ev_write(fd, _running);
    }
  #endif

    void del_ev_read(sock_t fd) {
        _epoll.del_ev_read(fd);
    }

    void del_ev_write(sock_t fd) {
        _epoll.del_ev_write(fd);
    }

    void del_event(sock_t fd) {
        _epoll.del_event(fd);
    }

    void recycle(Coroutine* co) {
        _co_ids.push_back(co->id);
    }

    void check_timeout(std::vector<Coroutine*>& res);

    bool timeout() const { return _timeout; }

    bool on_stack(void* p) const {
        return (_stack <= (char*)p) && ((char*)p < _stack + FLG_co_stack_size);
    }

  private:
    void save_stack(Coroutine* co);
    Coroutine* new_coroutine(Closure* cb);

  private:
    uint32 _id;          // scheduler id   
    char* _stack;        // stack shared by coroutines in this scheduler
    Coroutine* _main_co; // save the main context
    Coroutine* _running; // the current running coroutine
    Epoll _epoll;

    std::vector<Coroutine*> _co_pool; // coroutine pool
    std::vector<int> _co_ids;         // id of available coroutines in _co_pool

    ::Mutex _task_mtx;
    std::vector<Closure*> _task_cb;   // newly added tasks
    std::vector<Coroutine*> _task_co; // tasks to resume

    std::multimap<int64, Coroutine*> _timer;        // timed-wait tasks: <time_ms, co>
    std::multimap<int64, Coroutine*>::iterator _it; // add_timer() may be faster with it
    uint32 _wait_ms; // time epoll to wait

    ::Mutex _timer_task_mtx;
    std::unordered_map<Coroutine*, timer_id_t> _timer_task_co; // timer tasks to resume

    SyncEvent _ev;
    bool _stop;
    bool _timeout;
};

inline Coroutine* Scheduler::new_coroutine(Closure* cb) {
    if (!_co_ids.empty()) {
        Coroutine* co = _co_pool[_co_ids.back()];
        co->cb = cb;
        co->ctx = 0;
        co->ev = 0;
        _co_ids.pop_back();
        return co;
    } else {
        Coroutine* co = new Coroutine((int)_co_pool.size(), cb);
        _co_pool.push_back(co);
        return co;
    }
}

inline void Scheduler::add_task(Closure* cb) {
    {
        ::MutexGuard g(_task_mtx);
       _task_cb.push_back(cb);
    }
    _epoll.signal();
}

inline void Scheduler::add_task(Coroutine* co) {
    {
        ::MutexGuard g(_task_mtx);
        _task_co.push_back(co);
    }
    _epoll.signal();
}

inline void Scheduler::add_task(Coroutine* co, timer_id_t id) {
    {
        ::MutexGuard g(_timer_task_mtx);
        _timer_task_co.insert(std::make_pair(co, id));
    }
    _epoll.signal();
}

class SchedulerMgr {
  public:
    SchedulerMgr();
    ~SchedulerMgr();

    Scheduler* operator->() {
        if (_n != (uint32)-1) return _scheds[atomic_inc(&_index) & _n];
        return _scheds[atomic_inc(&_index) % _scheds.size()];
    }

    std::vector<Scheduler*>& schedulers() {
        return _scheds;
    }

  private:
    std::vector<Scheduler*> _scheds;
    uint32 _index;
    uint32 _n;
};

#ifdef _WIN32
class EvRead {
  public:
    EvRead(sock_t fd) : _fd(fd), _id(null_timer_id) {
        gSched->add_ev_read(fd);
    }

    ~EvRead() {
        if (_id != null_timer_id) gSched->del_timer(_id);
    }

    void wait() {
        gSched->yield();
    }

    bool wait(int ms, int err=-1) {
        if (_id == null_timer_id) {
            _id = (err == -1 ? gSched->add_timer(ms) : gSched->add_ev_timer(ms));
        }

        gSched->yield();
        if (!gSched->timeout()) return true;

        gSched->del_ev_read(_fd);
        ELOG_IF(!CancelIo((HANDLE)_fd)) << "cancel io for fd " << _fd << " failed..";

        _id = null_timer_id;
        WSASetLastError(ETIMEDOUT);
        return false;
    }

  private:
    sock_t _fd;
    timer_id_t _id;
};

class EvWrite {
  public:
    EvWrite(sock_t fd) : _fd(fd), _id(null_timer_id) {
        gSched->add_ev_write(fd);
    }

    ~EvWrite() {
        gSched->del_ev_write(_fd);
        if (_id != null_timer_id) gSched->del_timer(_id);
    }

    void wait() {
        gSched->yield();
    }

    bool wait(int ms, int err=-2) {
        if (_id == null_timer_id) {
            _id = (err == -2 ? gSched->add_timer(ms) : gSched->add_ev_timer(ms));
        }

        gSched->yield();
        if (!gSched->timeout()) return true;

        ELOG_IF(!CancelIo((HANDLE)_fd)) << "cancel io for fd " << _fd << " failed..";

        _id = null_timer_id;
        WSASetLastError(ETIMEDOUT);
        return false;
    }

  private:
    sock_t _fd;
    timer_id_t _id;
};

#else
class EvRead {
  public:
    EvRead(sock_t fd) : _fd(fd), _id(null_timer_id) {}

    ~EvRead() {
        if (_id != null_timer_id) gSched->del_timer(_id);
    }

    void wait() {
        gSched->add_ev_read(_fd);
        gSched->yield();
    }

    bool wait(int ms, int err=-1) {
        if (_id == null_timer_id) {
            gSched->add_ev_read(_fd);
            _id = (err == -1 ? gSched->add_timer(ms) : gSched->add_ev_timer(ms));
        }

        gSched->yield();
        if (!gSched->timeout()) return true;

        gSched->del_ev_read(_fd);
        _id = null_timer_id;
        errno = ETIMEDOUT;
        return false;
    }

  private:
    sock_t _fd;
    timer_id_t _id; // must initialize it manually on some platforms.
};

class EvWrite {
  public:
    EvWrite(sock_t fd) : _has_ev(false), _fd(fd), _id(null_timer_id) {}

    ~EvWrite() {
        if (_has_ev) gSched->del_ev_write(_fd);
        if (_id != null_timer_id) gSched->del_timer(_id);
    }

    void wait() {
        if (!_has_ev) {
            _has_ev = true;
            gSched->add_ev_write(_fd);
        }
        gSched->yield();
    }

    bool wait(int ms, int err=-2) {
        if (!_has_ev) {
            _has_ev = true;
            gSched->add_ev_write(_fd);
            _id = (err == -2 ? gSched->add_timer(ms) : gSched->add_ev_timer(ms));
        }

        gSched->yield();
        if (!gSched->timeout()) return true;

        _id = null_timer_id;
        errno = ETIMEDOUT;
        return false;
    }

  private:
    bool _has_ev;
    sock_t _fd;
    timer_id_t _id;
};

#endif

} // co
