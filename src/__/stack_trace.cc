#ifndef _WIN32
#include "co/__/stack_trace.h"

#if defined(__linux__) && !defined(__ANDROID__) && defined(HAS_EXECINFO_H)
#include "co/fs.h"
#include "co/os.h"
#include "co/fastream.h"
#include "co/co/hook.h"

#include <unistd.h>
#include <execinfo.h>
#include <sys/wait.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

namespace {

struct Param {
    Param() : f(0), cb(0), s(8 * 1024) {
        memset((char*)s.data(), 0, s.capacity());
        exe = os::exepath();
    }

    ~Param() {
        memset((char*)s.data(), 0, s.capacity());
    }

    fs::file* f;
    void (*cb)();
    fastream s;
    fastring exe;
};

class StackTraceImpl : public StackTrace {
  public:
    StackTraceImpl();
    virtual ~StackTraceImpl();

    virtual void set_file(void* f) {
        kParam->f = (fs::file*) f;
    }

    virtual void set_callback(void (*cb)()) {
        kParam->cb = cb;
    }

  private:
    static Param* kParam;

    static void on_signal(int sig);
};

Param* StackTraceImpl::kParam = 0;

StackTraceImpl::StackTraceImpl() {
    kParam = new Param;
    const int flag = SA_RESTART | SA_ONSTACK;
    os::signal(SIGSEGV, &StackTraceImpl::on_signal, flag);
    os::signal(SIGABRT, &StackTraceImpl::on_signal, flag);
    os::signal(SIGFPE, &StackTraceImpl::on_signal, flag);
    os::signal(SIGBUS, &StackTraceImpl::on_signal, flag);
    os::signal(SIGILL, &StackTraceImpl::on_signal, flag);
}

StackTraceImpl::~StackTraceImpl() {
    os::signal(SIGSEGV, SIG_DFL);
    os::signal(SIGABRT, SIG_DFL);
    os::signal(SIGFPE, SIG_DFL);
    os::signal(SIGBUS, SIG_DFL);
    os::signal(SIGILL, SIG_DFL);
    delete kParam;
}

inline void write_to_stderr(const char* s, size_t n) {
    auto r = raw_api(write)(STDERR_FILENO, s, n);
    (void)r;
}

#define write_msg(msg, len, f) \
    do { \
        write_to_stderr(msg, len); \
        if (f) f->write(msg, len); \
    } while (0)

#define safe_abort(n) \
    do { \
        kill(getppid(), SIGCONT); \
        os::signal(SIGABRT, SIG_DFL); \
        abort(); \
    } while (n)

#define abort_if(cond, msg) \
    if (cond) { \
        write_to_stderr(msg, strlen(msg)); \
        safe_abort(0); \
    }

static void addr2line(const char* exe, const char* addr, char* buf, size_t len) {
    int pipefd[2];
    abort_if(pipe(pipefd) != 0, "create pipe failed");

    pid_t pid = fork();
    if (pid == 0) {
        raw_api(close)(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);

        int r = execlp("addr2line", "addr2line", addr, "-f", "-C", "-e", exe, (void*)0);
        abort_if(r == -1, "execlp addr2line failed");
        //int r = execlp("atos", "atos", "-o", exe, addr, (void*)0);
        //abort_if(r == -1, "execlp atos failed");
    }

    raw_api(close)(pipefd[1]);
    abort_if(waitpid(pid, NULL, 0) != pid, "waitpid failed");

    ssize_t r = raw_api(read)(pipefd[0], buf, len - 1);
    raw_api(close)(pipefd[0]);
    buf[r > 0 ? r : 0] = '\0';
}

void StackTraceImpl::on_signal(int sig) {
    if (kParam->cb) kParam->cb();

    // fork and stop the parent process, stack trace will be done in the child process
    pid_t pid = fork();
    if (pid != 0) {
        int status;
        kill(getpid(), SIGSTOP);
        waitpid(pid, &status, WNOHANG);
        os::signal(SIGABRT, SIG_DFL);
        abort();
    }

    fs::file* file = kParam->f;
    fastream& fs = kParam->s;
    bool check_failed = (sig == SIGABRT && !kParam->cb);

    do {
        switch (sig) {
          case SIGSEGV:
            fs.append("SIGSEGV: segmentation fault\n");
            break;
          case SIGABRT:
            if (!check_failed) fs.append("SIGABRT: aborted\n");
            break;
          case SIGFPE:
            fs.append("SIGFPE: floating point exception\n");
            break;
          case SIGBUS:
            fs.append("SIGBUS: bus error\n");
            break;
          case SIGILL:
            fs.append("SIGILL: illegal instruction\n");
            break;
          default:
            fs.append("caught unexpected signal\n");
            break;
        }
    } while (0);

    // backtrace and turn addrs to function names, line numbers
    fastring& exe = kParam->exe;

    void** addrs = (void**) (fs.data() + fs.capacity() - 4096); // last 4k
    int nframes = backtrace(addrs, 128);
    int nskip = nframes > 2 ? 2 : 0;
    if (check_failed && nframes > 7) nskip = 7;

    int maxaddrlen = 0;
    char* buf = ((char*)addrs) + nframes * sizeof(void*);
    size_t buflen = 4096 - nframes * sizeof(void*);

    for (int i = nskip; i < nframes; ++i) {
        fs << '#' << (i - nskip) << "  ";

        size_t prelen = fs.size();
        char* line = buf;

        Dl_info di;
        if (dladdr(addrs[i], &di) == 0 || di.dli_fname[0] != '/' || exe == di.dli_fname) {
            fs << addrs[i];
            if (maxaddrlen == 0) maxaddrlen = (int) (fs.size() - prelen);
            int n = maxaddrlen - (int) (fs.size() - prelen);
            if (n > 0) fs.append(n, ' ');
            addr2line(exe.c_str(), fs.c_str() + prelen, buf, buflen);
        } else {
            void* p = (void*) ((char*)addrs[i] - (char*)di.dli_fbase);
            fs << p;
            if (maxaddrlen == 0) maxaddrlen = (int) (fs.size() - prelen);
            int n = maxaddrlen - (int) (fs.size() - prelen);
            if (n > 0) fs.append(n, ' ');
            addr2line(di.dli_fname, fs.c_str() + prelen, buf, buflen);
        }

        fs << " in ";

      #ifdef __linux__
        char* fend = strchr(line, '\n');
        if (fend) {
            *fend = '\0';
            fs << line << " at "; // function name
            line = fend + 1;
            *line != '?' ? (fs << line) : (fs << exe << '\n');
        } else {
            fs << "???\n"; // addr2line returns nothing ?
        }

      #else
        fs << (*line ? line : "???\n");
      #endif

        if (check_failed && i == nskip) write_to_stderr("\n", 1);
        write_msg(fs.data(), fs.size(), file);
        fs.clear();
    }

    if (nframes == 0) {
        fs << "frames not found by backtrace..\n";
        write_msg(fs.data(), fs.size(), file);
    }

    if (file) {
        file->write('\n');
        file->close();
    }

    kill(getppid(), SIGCONT);
    _Exit(EXIT_SUCCESS);
}

#undef write_msg
#undef safe_abort
#undef abort_if
}

#else

namespace {
class StackTraceImpl : public StackTrace {
  public:
    StackTraceImpl() = default;
    virtual ~StackTraceImpl() = default;

    virtual void set_file(void* f) {}

    virtual void set_callback(void (*cb)()) {}
};
}
#endif

StackTrace* new_stack_trace() {
    return new StackTraceImpl;
}
#endif
