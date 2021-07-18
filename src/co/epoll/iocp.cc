#ifdef _WIN32

#include "iocp.h"
#include "../hook.h"
#include "../scheduler.h"

namespace co {

Iocp::Iocp(int thread_num) : _stop(0) {
    _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, thread_num);
    CHECK(_iocp != 0) << "create iocp failed..";
    _ev = (OVERLAPPED_ENTRY*) calloc(1024, sizeof(OVERLAPPED_ENTRY));
    Thread(&Iocp::loop, this).detach();
}

Iocp::~Iocp() {
    atomic_compare_swap(&_stop, 0, 1);
    PostQueuedCompletionStatus(_iocp, 0, 0, 0);
    while (_stop != 2) sleep::ms(1);
    CloseHandle(_iocp);
    free(_ev);
}

void Iocp::loop() {
    ULONG n = 0;
    BOOL r = FALSE;
    while (!_stop) {
        r = CO_RAW_API(GetQueuedCompletionStatusEx)(_iocp, _ev, 1024, &n, -1, false);
        if (_stop) break;
        if (r == FALSE) {
            ELOG << "IOCP wait error: " << co::strerror();
            continue;
        }

        for (ULONG i = 0; i < n; ++i) {
            auto& ev = _ev[i];
            if (ev.lpOverlapped == NULL) continue;

            auto info = (IoEvent::PerIoInfo*) ev.lpOverlapped;
            auto co = (Coroutine*) info->co;
            if (atomic_compare_swap(&info->ios, 0, 1) == 0) {
                info->n = ev.dwNumberOfBytesTransferred;
                ((SchedulerImpl*)co->s)->add_ready_task(co);
            } else {
                free(info);
            }
        }
    }
    atomic_swap(&_stop, 2);
}

Iocp* gIocp = NULL;
uint32 gEpollNum = 0;

Epoll::Epoll(uint32 thread_num) : _signaled(false) {
    if (gIocp == NULL) gIocp = new Iocp(thread_num);
    if (gEpollNum == 0) gEpollNum = thread_num;
    _iocp = gIocp;
}

Epoll::~Epoll() {
    if (atomic_dec(&gEpollNum) == 0) delete _iocp;
}

} // co

#endif
