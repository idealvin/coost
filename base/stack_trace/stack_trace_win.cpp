#ifdef _WIN32
#pragma warning (disable:4996)

#include "stack_trace.h"
#include "../fs.h"
#include "StackWalker.hpp"

#include <stdio.h>
#include <string.h>
#include <signal.h>

namespace {

struct Param {
    Param() : f(0), cb(0), p(0) {
        n = 128;
        s = (char*) malloc(n);
    }

    ~Param() {
        memset(s, 0, n);
        free(s);
    }

    char* s;
    unsigned int n; // 16k
    fs::file* f;
    void (*cb)();
    void* p;
    void* h;
};

class StackTraceImpl : public StackTrace, public StackWalker {
  public:
    StackTraceImpl();
    virtual ~StackTraceImpl();

    virtual void set_file(void* f) {
        kParam->f = (fs::file*) f;
    }

    virtual void set_callback(void (*cb)()) {
        kParam->cb = cb;
    }

    virtual void OnOutput(LPCSTR s) {
        size_t len = strlen(s);
        fwrite(s, 1, len, stderr);

        fs::file* f = (fs::file*) kParam->f;
        if (f) f->write(s, len);
    }

    virtual void OnSymInit(LPCSTR, DWORD, LPCSTR) {}

    virtual void OnLoadModule(LPCSTR, LPCSTR, DWORD64, DWORD, DWORD, LPCSTR, LPCSTR, ULONGLONG) {}

    virtual void OnDbgHelpErr(LPCSTR, DWORD, DWORD64) {}

  private:
    static void on_signal(int sig);
    static LONG WINAPI on_exception(PEXCEPTION_POINTERS p);

    static Param* kParam;
};

Param* StackTraceImpl::kParam = 0;

const int kOptions =
    StackWalker::SymUseSymSrv |
    StackWalker::RetrieveSymbol |
    StackWalker::RetrieveLine |
    StackWalker::RetrieveModuleInfo;
 
StackTraceImpl::StackTraceImpl() : StackWalker(kOptions) {
    kParam = new Param;
    kParam->p = this;
    signal(SIGABRT, &StackTraceImpl::on_signal);

    // Signal handler for SIGSEGV and SIGFPE installed in main thread
    // does not work for other threads. SetUnhandledExceptionFilter()
    // may not work on some cases. Use AddVectoredExceptionHandler instead.
    kParam->h = AddVectoredExceptionHandler(1, &StackTraceImpl::on_exception);
}

StackTraceImpl::~StackTraceImpl() {
    signal(SIGABRT, SIG_DFL);
    RemoveVectoredContinueHandler(kParam->h);
    delete kParam;
}

inline void write_msg(const char* msg, size_t len = 0, fs::file* f = 0) {
    if (len == 0) len = strlen(msg);
    fwrite(msg, 1, len, stderr);
    if (f) f->write(msg, len);
}

void StackTraceImpl::on_signal(int sig) {
    if (kParam->cb) kParam->cb();

    char* s = kParam->s;
    s[0] = '\0';

    switch (sig) {
      case SIGABRT:
        if (kParam->cb) strcat(s, "SIGABRT: aborted\n");
        break;
      default:
        strcat(s, "caught unexpected signal\n");
        break;
    }

    fs::file* f = kParam->f;
    if (s[0]) write_msg(s, strlen(s), f);

    StackTraceImpl* fh = (StackTraceImpl*) kParam->p;
    fh->ShowCallstack(GetCurrentThread());

    if (f) {
        f->write('\n');
        f->close();
    }
}

LONG WINAPI StackTraceImpl::on_exception(PEXCEPTION_POINTERS p) {
    char* s = kParam->s;
    s[0] = '\0';

    switch(p->ExceptionRecord->ExceptionCode) {
      case EXCEPTION_ACCESS_VIOLATION:
        strcat(s, "Error: EXCEPTION_ACCESS_VIOLATION\n");
        break;
      case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        strcat(s, "Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n");
        break;
      case EXCEPTION_BREAKPOINT:
        strcat(s, "Error: EXCEPTION_BREAKPOINT\n");
        break;
      case EXCEPTION_DATATYPE_MISALIGNMENT:
        strcat(s, "Error: EXCEPTION_DATATYPE_MISALIGNMENT\n");
        break;
      case EXCEPTION_FLT_DENORMAL_OPERAND:
        strcat(s, "Error: EXCEPTION_FLT_DENORMAL_OPERAND\n");
        break;
      case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        strcat(s, "Error: EXCEPTION_FLT_DIVIDE_BY_ZERO\n");
        break;
      case EXCEPTION_FLT_INEXACT_RESULT:
        strcat(s, "Error: EXCEPTION_FLT_INEXACT_RESULT\n");
        break;
      case EXCEPTION_FLT_INVALID_OPERATION:
        strcat(s, "Error: EXCEPTION_FLT_INVALID_OPERATION\n");
        break;
      case EXCEPTION_FLT_OVERFLOW:
        strcat(s, "Error: EXCEPTION_FLT_OVERFLOW\n");
        break;
      case EXCEPTION_FLT_STACK_CHECK:
        strcat(s, "Error: EXCEPTION_FLT_STACK_CHECK\n");
        break;
      case EXCEPTION_FLT_UNDERFLOW:
        strcat(s, "Error: EXCEPTION_FLT_UNDERFLOW\n");
        break;
      case EXCEPTION_ILLEGAL_INSTRUCTION:
        strcat(s, "Error: EXCEPTION_ILLEGAL_INSTRUCTION\n");
        break;
      case EXCEPTION_IN_PAGE_ERROR:
        strcat(s, "Error: EXCEPTION_IN_PAGE_ERROR\n");
        break;
      case EXCEPTION_INT_DIVIDE_BY_ZERO:
        strcat(s, "Error: EXCEPTION_INT_DIVIDE_BY_ZERO\n");
        break;
      case EXCEPTION_INT_OVERFLOW:
        strcat(s, "Error: EXCEPTION_INT_OVERFLOW\n");
        break;
      case EXCEPTION_INVALID_DISPOSITION:
        strcat(s, "Error: EXCEPTION_INVALID_DISPOSITION\n");
        break;
      case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        strcat(s, "Error: EXCEPTION_NONCONTINUABLE_EXCEPTION\n");
        break;
      case EXCEPTION_PRIV_INSTRUCTION:
        strcat(s, "Error: EXCEPTION_PRIV_INSTRUCTION\n");
        break;
      case EXCEPTION_SINGLE_STEP:
        strcat(s, "Error: EXCEPTION_SINGLE_STEP\n");
        break;
      case EXCEPTION_STACK_OVERFLOW:
        strcat(s, "Error: EXCEPTION_STACK_OVERFLOW\n");
        break;
      default:
        strcat(s, "Error: Unrecognized Exception\n");
        // just ignore unrecognized exception here
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (kParam->cb) kParam->cb();
    fs::file* f = kParam->f;
    if (s[0]) write_msg(s, strlen(s), f);

    StackTraceImpl* fh = (StackTraceImpl*) kParam->p;
    fh->ShowCallstack(GetCurrentThread());

    if (f) {
        f->write('\n');
        f->close();
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

StackTrace* new_stack_trace() {
    return new StackTraceImpl;
}

#endif
