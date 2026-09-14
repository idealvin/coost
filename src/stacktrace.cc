#include "stacktrace.h"
#include "co/mem.h"

#ifdef _WIN32
#include "StackWalker.h"
#else
#include "co/string.h"
#include "co/os.h"
#include <unistd.h>
#ifdef WITH_BACKTRACE
#include <backtrace.h>
#include <cxxabi.h>
#endif
#endif

namespace co {

#ifdef _WIN32
struct StackTraceImpl : public StackWalker {
    typedef void (*write_cb_t)(const char*, size_t);
    static const int kOptions =
        StackWalker::SymUseSymSrv |
        StackWalker::RetrieveSymbol |
        StackWalker::RetrieveLine |
        StackWalker::RetrieveModuleInfo;

    StackTraceImpl() : StackWalker(kOptions), _f(0) {}

    virtual ~StackTraceImpl() = default;

    void dump_stack(write_cb_t f, size_t p) {
        _f = f;
        this->ShowCallstack(GetCurrentThread(), (CONTEXT*)p);
    }

    virtual void OnOutput(LPCSTR s) {
        const size_t n = strlen(s);
        if (_f) _f(s, n);
    }

    virtual void OnSymInit(LPCSTR, DWORD, LPCSTR) {}
    virtual void OnLoadModule(LPCSTR, LPCSTR, DWORD64, DWORD, DWORD, LPCSTR, LPCSTR, ULONGLONG) {}
    virtual void OnDbgHelpErr(LPCSTR, DWORD, DWORD64) {}

    write_cb_t _f;
};

#else
#ifdef WITH_BACKTRACE
struct StackTraceImpl {
    typedef void (*write_cb_t)(const char*, size_t);
    static const size_t N = 4096; // demangle buffer size
    StackTraceImpl();
    ~StackTraceImpl();

    void dump_stack(write_cb_t f, size_t skip);
    char* demangle(const char* name);
    int backtrace(const char* file, int line, const char* func, int& count);

    write_cb_t _f;
    const char* _exe; // exe path
    char* _buf;       // for demangle
    size_t _size;     // buf size
    co::string _s;    // for stack trace
};

StackTraceImpl::StackTraceImpl()
    : _f(0), _size(N), _s(N) {
    auto path = os::exepath();
    void* p = co::_static_alloc(path.size() + 1);
    ::memcpy(p, path.c_str(), path.size() + 1);
    _exe = (const char*)p;

    _buf = (char*)::malloc(N);
    runtime_assert(_buf);
    ::memset(_buf, 0, N); (void)_buf[0];
    ::memset(_s.data(), 0, _s.capacity()); (void)_s[0];
}

StackTraceImpl::~StackTraceImpl() {
    if (_buf) { ::free(_buf); _buf = nullptr; }
}

// https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html
char* StackTraceImpl::demangle(const char* name) {
    int status = 0;
    size_t n = _size;
    char* p = abi::__cxa_demangle(name, _buf, &n, &status);
    if (_size < n) { /* buffer reallocated, not likely to happen */
        _buf = p;
        _size = n;
    }
    return p;
}

struct user_data_t {
    StackTraceImpl* st;
    int count;
};

void error_cb(void* data, const char* msg, int errnum) {
    auto r = ::write(STDERR_FILENO, msg, strlen(msg));
    r = ::write(STDERR_FILENO, "\n", 1);
    (void)r;
}

int backtrace_cb(void* data, uintptr_t /*pc*/, const char* file, int line, const char* func) {
    user_data_t* ud = (user_data_t*)data;
    return ud->st->backtrace(file, line, func, ud->count);
}

void StackTraceImpl::dump_stack(write_cb_t f, size_t skip) {
    _f = f;
    user_data_t ud = { this, 0 };
    backtrace_state* state = backtrace_create_state(_exe, 1, error_cb, NULL);
    backtrace_full(state, (int)skip, backtrace_cb, error_cb, (void*)&ud);
}

int StackTraceImpl::backtrace(const char* file, int line, const char* func, int& count) {
    if (!file && !func) return 0;
    if (func) {
        char* p = this->demangle(func);
        if (p) func = p;
    }

    const int n = count++;
    _s.clear();
    _s.cat(
        '#', n, "  in ", (func ? func : "???"), " at ", 
       (file ? file : "???"), ':', line, '\n'
    );

    if (_f) _f(_s.data(), _s.size());
    return 0;
}

#else
struct StackTraceImpl {
    void dump_stack(void(*write_cb)(const char*, size_t), size_t) {}
};
#endif

#endif

StackTrace::StackTrace() {
    _p = (void*) co::_make_static<StackTraceImpl>();
}

StackTrace::~StackTrace() {}

void StackTrace::dump_stack(void(*write_cb)(const char*, size_t), size_t p) {
    ((StackTraceImpl*)_p)->dump_stack(write_cb, p);
}

} // co
