#ifdef _WIN32
#include "iocp.h"
#include "co/error.h"
#include "co/log.h"
#include "../sched.h"

namespace co {

using Ev = OVERLAPPED_ENTRY;

Iocp::Iocp()
    : _signaled(0), _n(0) {
    _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
    runtime_assert(_iocp != NULL);
    _events = (Ev*) co::_static_alloc(N * sizeof(Ev), co::cache_line_size);
    ::memset(_events, 0, N * sizeof(Ev));
}

Iocp::~Iocp() {
    if (_iocp) { CloseHandle(_iocp); _iocp = NULL; }
}

// if @fd is already binded to another IOCP, CreateIoCompletionPort returns NULL,
// and the error number is set to ERROR_INVALID_PARAMETER
bool Iocp::add_event(sock_t fd) {
    if (CreateIoCompletionPort((HANDLE)fd, _iocp, fd, 0) != NULL) return true;
    const uint32 e = ::GetLastError();
    if (e == ERROR_INVALID_PARAMETER) return true;
    log::error(
        "IOCP bind socket error: ", co::strerror(e),
        ", handle: ", (void*)_iocp, ", fd: ", fd
    );
    return false;
}

int Iocp::wait(int ms) {
    ULONG n = 0;
    if (GetQueuedCompletionStatusEx(_iocp, _events, N, &n, ms, false)) {
        _n = (int)n;
    } else {
        const uint32 e = ::GetLastError();
        if (e == WAIT_TIMEOUT) {
            _n = 0;
        } else {
            _n = -1;
            log::error("IOCP wait error: ", co::strerror(e), ", handle: ", (void*)_iocp);
        }
    }
    return _n;
}

void Iocp::signal() {
    if (atomic_bool_cas(&_signaled, 0, 1, mo_relaxed, mo_relaxed)) {
        if (!PostQueuedCompletionStatus(_iocp, 0, 0, 0)) {
            const uint32 e = ::GetLastError();
            log::error("IOCP post error: ", co::strerror(e), ", handle: ", (void*)_iocp);
            atomic_store(&_signaled, 0, mo_relaxed);
        }
    }
}

void Iocp::handle_events() {
    const auto s = g_sched;
    const int n = _n;
    _n = 0;
    for (int i = 0; i < n; ++i) {
        auto& ev = _events[i];
        per_io_t* p = (per_io_t*) ev.lpOverlapped;
        if (p) {
            Coroutine* co = (Coroutine*) p->co;
            p->n = ev.dwNumberOfBytesTransferred;
            if (atomic_bool_cas(&p->state, 0, 1/*ready*/, mo_relaxed, mo_relaxed)) {
                if (co->sched == s) {
                    s->resume(co);
                } else {
                    co->sched->add_ready_task(co);
                }
            } else {
                co::free(p, p->mlen);
            }
        } else {
            atomic_store(&_signaled, 0, mo_relaxed);
        }
    }
}

per_io_t* per_io_t::create(void* co, int extra, int buf_size) {
    const uint32 m = sizeof(per_io_t) + extra;
    const uint32 n = m + buf_size;
    per_io_t* p = (per_io_t*) co::alloc(n);
    runtime_assert(p);
    ::memset(p, 0, m);
    p->co = co;
    p->mlen = n;
    ((Coroutine*)co)->wtx = (Waitx*)(((char*)&p->co) - offsetof(Waitx, co));
    return p;
}

} // co

#endif
