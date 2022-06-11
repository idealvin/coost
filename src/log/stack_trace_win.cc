#ifdef _WIN32

#include "stack_trace.h"
#include "StackWalker.hpp"
#include "co/fs.h"
#include "co/mem.h"

#include <stdio.h>
#include <string.h>

namespace ___ {
namespace log {

class StackTraceImpl : public StackTrace, StackWalker {
  public:
    static const int kOptions =
        StackWalker::SymUseSymSrv |
        StackWalker::RetrieveSymbol |
        StackWalker::RetrieveLine |
        StackWalker::RetrieveModuleInfo;

    StackTraceImpl() : StackTrace(), StackWalker(kOptions), _f(0), _skip(0) {}

    virtual ~StackTraceImpl() = default;

    virtual void dump_stack(void* f, int skip) {
        _f = (fs::file*) f;
        _skip = skip;
        this->ShowCallstack(GetCurrentThread());
    }

  private:
    virtual void OnOutput(LPCSTR s) {
        if (_skip > 0) { --_skip; return; }
        const size_t n = strlen(s);
        if (_f && *_f) _f->write(s, n);
        auto r = ::fwrite(s, 1, n, stderr); (void)r;
    }

    virtual void OnSymInit(LPCSTR, DWORD, LPCSTR) {}
    virtual void OnLoadModule(LPCSTR, LPCSTR, DWORD64, DWORD, DWORD, LPCSTR, LPCSTR, ULONGLONG) {}
    virtual void OnDbgHelpErr(LPCSTR, DWORD, DWORD64) {}

  private:
    fs::file* _f; // file
    int _skip;
};

StackTrace* stack_trace() {
    static auto kst = co::static_new<StackTraceImpl>();
    return kst;
}

} // log
} // ___

#endif
