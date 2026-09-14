#include "co/log.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/time.h"
#include "co/thread.h"
#include "co/stl.h"
#include "stacktrace.h"
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef _MSC_VER
#pragma warning (disable:4722) // call exit() in destructor
#endif
#else
#include <unistd.h>
#include <sys/select.h>
#endif

#define SS(name, c, e) static const char* name[2] = { c, e };

SS(s_log_dir, "@i 日志目录", "@i log directory");
SS(s_max_log_size, "@i 单条日志最大大小", "@i max size of a single log");
SS(s_max_log_file_size, "@i 日志文件最大大小", "@i max size of log file");
SS(s_max_log_file_num, "@i 日志文件最大数量", "@i max number of log files");
SS(s_max_log_buffer_size, "@i 日志缓存最大大小", "@i max size of log buffer");
SS(s_log_flush_ms, "@i 刷新日志缓存时间间隔(毫秒)", "@i flush log buffer every n ms");
SS(s_also_log2console, "@i 日志也输出到终端", "@i also logging to console");
SS(s_log_daily, "@i 日志文件按天轮转", "@i rotate log files by day");

DEF_string(log_dir, "logs", s_log_dir);
DEF_uint32(min_log_level, 0, "@i 0-4 (debug|info|warn|error|fatal)");
DEF_uint32(max_log_size, 4096, s_max_log_size);
DEF_int64(max_log_file_size, 256 << 20, s_max_log_file_size);
DEF_uint32(max_log_file_num, 8, s_max_log_file_num);
DEF_uint32(max_log_buffer_size, 32 << 20, s_max_log_buffer_size);
DEF_uint32(log_flush_ms, 128, s_log_flush_ms);
DEF_bool(also_log2console, false, s_also_log2console);
DEF_bool(log_daily, false, s_log_daily);

static co::string* g_cache;
static void (*g_write_cb)(const void*, size_t) = nullptr;
static bool g_also_log2local = false;
static bool g_has_fatal_log = false;
static bool g_day_changed = false;
static uint32 g_day;         // current day
static uint32 g_last_day;    // last day
static char g_last_time[24]; // time before day changed

namespace _xx {
namespace log {
namespace xx {

struct LogTime;
struct LogFile;
struct FatalFile;
struct Logger;
struct CrashHandler;

struct __cacheline_aligned Mod {
    Mod();
    ~Mod() = default;
    const char* exename;
    co::string* log_dir;
    co::string* log_path_prefix;
    LogTime* log_time;
    LogFile* log_file;
    FatalFile* fatal_file;
    Logger* logger;
    CrashHandler* crash_handler;
};

static Mod* g_mod;
inline Mod& mod() { return *g_mod; }

#ifdef _WIN32
static HANDLE g_h_stderr;

inline void _cerr(const void* s, size_t n) {
    if (g_h_stderr) {
        DWORD bytes_written = 0;
        ::WriteFile(g_h_stderr, s, (DWORD)n, &bytes_written, NULL);
    }
}

inline void _sleep(int ms) { time::sleep(ms); }

#else
inline void _cerr(const void* s, size_t n) {
    auto r = ::write(STDERR_FILENO, s, n); (void)r;
}

inline void _sleep(int ms) {
    struct timeval tv = { 0, ms * 1000 };
    ::select(0, 0, 0, 0, &tv);
}
#endif

inline void _cerr(const char* s) { _cerr(s, strlen(s)); }

// time for logs: "0723 17:00:00.123"
struct __cacheline_aligned LogTime {
    enum {
        t_len = 17, // length of time
        t_pmin = 8, // position of minute
        t_psec = t_pmin + 3,
        t_pms = t_psec + 3,
    };

    LogTime() : _start(0) {
        for (int i = 0; i < 60; ++i) {
            const auto p = (uint8*) &_tb[i];
            p[0] = (uint8)('0' + i / 10);
            p[1] = (uint8)('0' + i % 10);
        }
        ::memset(_buf, 0, sizeof(_buf));
        this->update();
        static_assert((offsetof(LogTime, _buf) & 3) == 0);
    }

    void update();
    const char* get() const { return _buf; }
    uint32 day() const { return *(uint32*)_buf; }

    time_t _start;
    struct tm _tm;
    int16 _tb[64];
    char _buf[24]; // save the time string
};

void LogTime::update() {
    const int64 now_ms = co::now.ms();
    const time_t now_sec = now_ms / 1000;
    const int dt = (int) (now_sec - _start);
    if (dt == 0) goto set_ms;
    if (dt < 0 || dt >= 60 || _start == 0) goto reset;

    _tm.tm_sec += dt;
    if (_tm.tm_min < 59 || _tm.tm_sec < 60) {
        _start = now_sec;
        if (_tm.tm_sec >= 60) {
            _tm.tm_min++;
            _tm.tm_sec -= 60;
            *(uint16*)(_buf + t_pmin) = _tb[_tm.tm_min];
        }
        const auto p = (char*)(_tb + _tm.tm_sec);
        _buf[t_psec] = p[0];
        _buf[t_psec + 1] = p[1];
        goto set_ms;
    }

reset:
    {
        _start = now_sec;
      #ifdef _WIN32
        _localtime64_s(&_tm, &_start);
      #else
        localtime_r(&_start, &_tm);
      #endif
        strftime(_buf, 16, "%m%d %H:%M:%S.", &_tm);
    }

set_ms:
    {
        const auto p = _buf + t_pms;
        uint32 ms = (uint32)(now_ms - _start * 1000);
        uint32 x = ms / 100;
        p[0] = (char)('0' + x);
        ms -= x * 100;
        x = ms / 10;
        p[1] = (char)('0' + x);
        p[2] = (char)('0' + (ms - x * 10));
    }
}

struct LogFile {
    LogFile() : _file(256), _path(256) {}

    fs::file& open();
    explicit operator bool() const { return (bool)_file; }
    void close() { _file.close(); }
    void rotate();
    void write(const void* p, size_t n);

    fs::file _file;
    co::string _path;
    co::deque<co::string> _old_paths; // paths of old log files
};

struct FatalFile {
    FatalFile() : _f(256) {}

    fs::file& open();
    explicit operator bool() const { return (bool)_f; }
    void close() { _f.close(); }

    void write(const void* p, size_t n) {
        if (_f || this->open()) _f.write(p, n);
    }

    fs::file _f;
};

fs::file& FatalFile::open() {
    auto& m = mod();
    auto& d = *m.log_dir;
    if (!fs::exists(d)) fs::mkdir(d.data(), true);

    auto& s = *g_cache; s.clear();
    s.append(*m.log_path_prefix).append(".fatal");

    if (!_f.open(s.c_str(), 'a')) {
        _cerr("can not open fatal log file: ");
        s.cat('\n');
        _cerr(s.data(), s.size());
    }

    return _f;
}

inline void LogFile::rotate() {
    _file.close();
    if (!_old_paths.empty()) {
        auto& path = _old_paths.back();
        fs::mv(_path, path); // rename xx.log to xx_0808_15_30_08.123.log
    }
}

fs::file& LogFile::open() {
    auto& s = *g_cache; s.clear();
    auto& m = mod();
    auto& d = *m.log_dir;
    auto& path_prefix = *m.log_path_prefix;
    if (!fs::exists(d)) fs::mkdir(d.data(), true);

    _path.clear();
    _path.append(path_prefix).append(".log");

    bool new_file = !fs::exists(_path) || _old_paths.empty();
    if (!new_file && FLG_log_daily) {
        auto& path = _old_paths.back();
        const uint32 day = !g_day_changed ? g_day : g_last_day;
        const uint32 day_in_path = [](const co::string& path) {
            uint32 x = 0;
            const int n = LogTime::t_len + 4;
            if (path.size() > n) ::memcpy(&x, path.data() + path.size() - n, 4);
            return x;
        }(path); // get day from xx_0808_15_30_08.123.log
        if (day_in_path != day) {
            fs::mv(_path, path);
            new_file = true;
        }
    }
    
    if (_file.open(_path.c_str(), 'a') && new_file) {
        char x[24] = { 0 }; // 0723 17:00:00.123
        const char* const t = !g_day_changed ? m.log_time->get() : g_last_time ;
        ::memcpy(x, t, LogTime::t_len);
        for (int i = 0; i < LogTime::t_len; ++i) {
            if (x[i] == ' ' || x[i] == ':') x[i] = '_';
        }

        s.clear();
        s.cat(path_prefix, '_', x, ".log");
        _old_paths.push_back(s);

        while (_old_paths.size() > FLG_max_log_file_num) {
            fs::rm(_old_paths.front());
            _old_paths.pop_front();
        }

        s.resize(path_prefix.size());
        s.append(".log.list");
        fs::file f(s.c_str(), 'w');
        if (f) {
            s.clear();
            for (auto& x : _old_paths) s.cat(x, '\n');
            f.write(s);
        }
    }

    if (!_file) {
        s.clear();
        s.cat("can not open log file: ", _path, '\n');
        _cerr(s.data(), s.size());
    }
    return _file;
}

inline void LogFile::write(const void* p, size_t n) {
    if (_file || this->open()) {
        _file.write(p, n);
        const int64 x = _file.size();
        if (x < 0) _file.close(); // file may be deleted
        if (x >= FLG_max_log_file_size || g_day_changed) this->rotate();
    }
}

struct Logger {
    static const uint32 N = 128 * 1024;

    Logger(LogTime* t, LogFile* f)
        : _log_event(true, false), _time(*t), _file(*f), _state(-1) {
        ::memcpy(_x.time_str, _time.get(), 24);
    }

    ~Logger() { this->stop(); }

    void start();
    void stop(bool signal_safe=false);
    void push_normal_log(char* s, size_t n);
    void push_fatal_log(char* s, size_t n);
    void write_logs(const char* p, size_t n);
    void thread_fun();

    struct __cacheline_aligned X {
        std::mutex mtx;
        co::string buf;
        char time_str[24];
    };

    X _x;
    co::string _buf; // to swap out logs
    co::sync_event _log_event;
    LogTime& _time;
    LogFile& _file;
    int _state; // -1: init, 0: running, 1: stopping, 2: stopped, 3: final
};

void Logger::start() {
    _time.update();
    g_day = _time.day();
    ::memcpy(_x.time_str, _time.get(), 24);
    _x.buf.reserve(N);
    _buf.reserve(N);
    co::atomic_store(&_state, 0);
    std::thread(&Logger::thread_fun, this).detach();
}

// if @signal_safe is true, try to call only async-signal-safe APIs according to:
// http://man7.org/linux/man-pages/man7/signal-safety.7.html
void Logger::stop(bool signal_safe) {
    if (FLG_log_flush_ms > 1) co::atomic_store(&FLG_log_flush_ms, 1);

    int s = co::atomic_cas(&_state, 0, 1);
    if (s < 0) return; // thread not started
    if (s == 0) {
        if (!signal_safe) _log_event.notify_one();
        while (co::atomic_load(&_state) != 2) _sleep(1);
        co::atomic_store(&_state, 3);
    } else {
        while (co::atomic_load(&_state) != 3) _sleep(1);
    }
}

void Logger::push_normal_log(char* s, size_t n) {
    if (n <= FLG_max_log_size) goto _1;
    {
        n = FLG_max_log_size;
        char* const p = s + n - 4;
        p[0] = '.';
        p[1] = '.';
        p[2] = '.';
        p[3] = '\n';
    }

_1:
    std::lock_guard<std::mutex> g(_x.mtx);
    auto& buf = _x.buf;

    if (_state <= 0) {
        ::memcpy(s + 1, _x.time_str, LogTime::t_len); // log time

        if (buf.size() + n < FLG_max_log_buffer_size) goto _2;
        {
            const char* p = strchr(buf.data() + (buf.size() >> 1) + 7, '\n');
            const size_t len = buf.data() + buf.size() - p - 1;
            ::memcpy(buf.data(), "......\n", 7);
            ::memcpy(buf.data() + 7, p + 1, len);
            buf.resize(len + 7);
        }

    _2:
        buf.append(s, n);
        if (buf.size() > (buf.capacity() >> 1)) _log_event.notify_one();
    }
}

void Logger::push_fatal_log(char* s, size_t n) {
    this->stop();
    ::memcpy(s + 1, _time.get(), LogTime::t_len);

    this->write_logs(s, n);
    if (!FLG_also_log2console) _cerr(s, n);
    mod().fatal_file->write(s, n);
    co::atomic_store(&g_has_fatal_log, true);

#ifdef _WIN32
    RaiseException(0xE880E237, 0, 0, NULL);
#else
    ::abort();
#endif
}

void Logger::write_logs(const char* p, size_t n) {
    if (!g_write_cb || g_also_log2local) _file.write(p, n);
    if (g_write_cb) g_write_cb(p, n);
    if (FLG_also_log2console) _cerr(p, n);
}

void Logger::thread_fun() {
    bool signaled = false;
    bool stopped = false;

    while (true) {
        signaled = _log_event.wait(FLG_log_flush_ms);
        if (_state > 0) stopped = true;

        _time.update();
        if (FLG_log_daily && _time.day() != g_day) {
            g_day_changed = true;
            g_day = _time.day();
            g_last_day = *(uint32*)_x.time_str;
            ::memcpy(g_last_time, _x.time_str, 24);
        }

        {
            std::lock_guard<std::mutex> g(_x.mtx);
            ::memcpy(_x.time_str, _time.get(), LogTime::t_len);
            if (!_x.buf.empty()) _x.buf.swap(_buf);
        }

        if (!_buf.empty()) {
            this->write_logs(_buf.data(), _buf.size());
            _buf.clear();
        }

        if (FLG_log_daily && g_day_changed) g_day_changed = false;
        if (signaled) _log_event.reset();
        if (stopped) break;
    }

    co::atomic_store(&_state, 2);
}

struct CrashHandler {
    CrashHandler() { this->install_handlers(); }
    ~CrashHandler() { this->uninstall_handlers(); }

    void install_handlers();
    void uninstall_handlers();
    void handle_crash(size_t e);

    co::StackTrace _stack_trace;
    co::map<int, os::sig_handler_t> _old_handlers;
};

static void output_crash_info(const char* p, size_t n) {
    _cerr(p, n);
    auto& m = mod();
    if (*m.log_file) m.log_file->write(p, n);
    if (*m.fatal_file) m.fatal_file->write(p, n);
}

#ifdef _WIN32
static BOOL WINAPI console_ctrl_handler(DWORD c) {
    if (
        c == CTRL_C_EVENT || c == CTRL_BREAK_EVENT ||
        c == CTRL_CLOSE_EVENT || c == CTRL_LOGOFF_EVENT ||
        c == CTRL_SHUTDOWN_EVENT
    ) {
        mod().logger->stop(true);
        return TRUE;
    }
    return FALSE;
}

static LONG WINAPI on_exception(PEXCEPTION_POINTERS p) {
    mod().crash_handler->handle_crash((size_t)p);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void on_sigabrt(int sig) {
    RaiseException(0xE880E235, 0, 0, NULL);
}

void CrashHandler::install_handlers() {
    _old_handlers[SIGABRT] = os::signal(SIGABRT, on_sigabrt);
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
        _cerr("SetConsoleCtrlHandler failed\n");
    }
    SetUnhandledExceptionFilter(on_exception);
}

void CrashHandler::uninstall_handlers() {
    SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
    os::signal(SIGABRT, SIG_DFL);
}

#define CASE_EXCEPT(e) case e: err = #e; break

void CrashHandler::handle_crash(size_t e) {
    auto& m = mod();
    const char* err = "";
    auto p = (PEXCEPTION_POINTERS)e;
    const uint32 code = p->ExceptionRecord->ExceptionCode;

    switch (code) {
        case 0xE880E233:
            err = "runtime_assert failed";
            break;
        case 0xE880E237: // check failed
            break;
        case 0xE880E235:
            err = "SIGABRT: aborted";
            break;
        CASE_EXCEPT(EXCEPTION_ACCESS_VIOLATION);
        CASE_EXCEPT(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
        CASE_EXCEPT(EXCEPTION_DATATYPE_MISALIGNMENT);
        CASE_EXCEPT(EXCEPTION_FLT_DIVIDE_BY_ZERO);
        CASE_EXCEPT(EXCEPTION_ILLEGAL_INSTRUCTION);
        CASE_EXCEPT(EXCEPTION_INT_DIVIDE_BY_ZERO);
        CASE_EXCEPT(EXCEPTION_NONCONTINUABLE_EXCEPTION);
        CASE_EXCEPT(EXCEPTION_STACK_OVERFLOW);
        CASE_EXCEPT(STATUS_INVALID_HANDLE);
        CASE_EXCEPT(STATUS_STACK_BUFFER_OVERRUN);
        case 0xE06D7363: // std::runtime_error()
            err = "STATUS_CPP_EH_EXCEPTION";
            break;
        case 0xE0434f4D: // VC++ Runtime error
            err = "STATUS_CLR_EXCEPTION";
            break;
        case 0xCFFFFFFF:
            err = "STATUS_APPLICATION_HANG";
            break;
        default:
            err = "Unexpected exception: ";
            break;
    }

    m.logger->stop(true);
    if (!*m.log_file) m.log_file->open();
    if (!*m.fatal_file) m.fatal_file->open();

    auto& s = *g_cache; s.clear();
    if (!g_has_fatal_log) s.cat('F', m.log_time->get(), ']', ' ', err);
    if (*err == 'U') s << (void*)(size_t)code;
    if (!s.empty()) s << '\n';

    output_crash_info(s.data(), s.size());
    _stack_trace.dump_stack(output_crash_info, (size_t)p->ContextRecord);
    _Exit(-1);
}

#else
static void on_signal(int sig) {
    mod().crash_handler->handle_crash(sig);
}

void CrashHandler::install_handlers() {
    _old_handlers[SIGINT] = os::signal(SIGINT, on_signal);
    _old_handlers[SIGTERM] = os::signal(SIGTERM, on_signal);
    _old_handlers[SIGABRT] = os::signal(SIGABRT, on_signal);
    _old_handlers[SIGQUIT] = os::signal(SIGQUIT, on_signal);
    _old_handlers[SIGSEGV] = os::signal(SIGSEGV, on_signal);
    _old_handlers[SIGFPE] = os::signal(SIGFPE, on_signal);
    _old_handlers[SIGBUS] = os::signal(SIGBUS, on_signal);
    _old_handlers[SIGILL] = os::signal(SIGILL, on_signal);
    os::signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE
}

void CrashHandler::uninstall_handlers() {
    os::signal(SIGINT, SIG_DFL);
    os::signal(SIGTERM, SIG_DFL);
    os::signal(SIGABRT, SIG_DFL);
    os::signal(SIGQUIT, SIG_DFL);
    os::signal(SIGSEGV, SIG_DFL);
    os::signal(SIGFPE, SIG_DFL);
    os::signal(SIGBUS, SIG_DFL);
    os::signal(SIGILL, SIG_DFL);
}

void CrashHandler::handle_crash(size_t e) {
    auto& m = mod();
    const int sig = (int)e;

    if (sig == SIGINT || sig == SIGTERM || sig == SIGQUIT) {
        m.logger->stop(true);
        os::signal(sig, _old_handlers[sig]);
        raise(sig);
        return;
    }

    m.logger->stop(true);
    if (!*m.log_file) m.log_file->open();
    if (!*m.fatal_file) m.fatal_file->open();

    auto& s = *g_cache; s.clear();
    if (!g_has_fatal_log) {
        s.cat('F', m.log_time->get(), ' ', co::thread_id(), ']', ' ');
    }

    switch (sig) {
        case SIGABRT:
            if (!g_has_fatal_log) s.append("SIGABRT: aborted\n");
            break;
        case SIGSEGV:
            s.append("SIGSEGV: segmentation fault\n");
            break;
        case SIGFPE:
            s.append("SIGFPE: floating point exception\n");
            break;
        case SIGBUS:
            s.append("SIGBUS: bus error\n");
            break;
        case SIGILL:
            s.append("SIGILL: illegal instruction\n");
            break;
    }

    if (!s.empty()) output_crash_info(s.data(), s.size());
    const int skip = g_has_fatal_log ? 9 : (sig == SIGABRT ? 4 : 3);
    _stack_trace.dump_stack(output_crash_info, skip);

    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}
#endif

Mod::Mod() {
    exename = []() {
        auto s = os::exename();
        s.remove_suffix(".exe");
        void* p = co::_static_alloc(s.size() + 1);
        ::memcpy(p, s.c_str(), s.size() + 1);
        return (const char*)p;
    }();

    log_dir = co::_make_static<co::string>(64);
    log_dir->cat("logs");

    log_path_prefix = co::_make_static<co::string>(64);
    log_path_prefix->cat("logs", '/', exename);

    log_time = co::_make_static<LogTime>();
    log_file = co::_make_static<LogFile>();
    fatal_file = co::_make_static<FatalFile>();
    logger = co::_make_static<Logger>(log_time, log_file);
    crash_handler = co::_make_static<CrashHandler>();
}

static int g_nifty_counter;

// Use a named function instead of a lambda.
// MSVC 19.44 hits an internal compiler error (C1001 in constexpr.cpp:10363)
// when a capture-less lambda is converted to a function pointer here.
static void after_parse_cb() {
    {
        auto& b = FLG_max_log_buffer_size; // >= 1M
        auto& l = FLG_max_log_size;        // >= 256
        auto& f = FLG_max_log_file_size;   // > 0
        if (b < (1 << 20)) b = 1 << 20;
        if (l < 256) l = 256;
        if (l > (b >> 2)) l = b >> 2;
        if (f <= 0) f = 256 << 20;
    }

    // log_dir & log_path_prefix
    if (FLG_log_dir != "logs") {
        if (FLG_log_dir.contains('\\')) FLG_log_dir.replace("\\", "/");
        g_mod->log_dir->assign(FLG_log_dir);

        auto& x = *g_mod->log_path_prefix;
        x.clear();
        x.cat(FLG_log_dir);
        if (!x.empty() && x.back() != '/') x.cat('/');
        x.cat(g_mod->exename);
    }

    // old log paths
    {
        auto& s = *g_cache;
        s.clear();
        s.append(*g_mod->log_path_prefix).append(".log.list");
        fs::file f(s.c_str(), 'r');
        if (f) {
            auto v = co::split(f.read((size_t)f.size()), '\n');
            for (auto& x : v) {
                g_mod->log_file->_old_paths.emplace_back(std::move(x));
            }
        }
    }

    // start the logging thread
    g_mod->logger->start();
}

LogInit::LogInit() {
    const int n = ++g_nifty_counter;
    if (n == 1) {
    #ifdef _WIN32
        g_h_stderr = []() {
            auto h = GetStdHandle(STD_ERROR_HANDLE);
            return h && h != INVALID_HANDLE_VALUE ? h : NULL;
        }();
    #endif
        g_cache = co::_make_static<co::string>(4096);
        g_mod = co::_make_static<Mod>();
    }

    if (n == 2) {
        flag::run_before_parse([]() {
            flag::unhide("log_dir");
            flag::unhide("min_log_level");
            flag::unhide("max_log_size");
            flag::unhide("max_log_file_size");
            flag::unhide("max_log_file_num");
            flag::unhide("max_log_buffer_size");
            flag::unhide("log_flush_ms");
            flag::unhide("also_log2console");
            flag::unhide("log_daily");
        });

        flag::run_after_parse(after_parse_cb);
    }
}

static __thread co::string* g_s;

inline co::string& log_stream() {
    return g_s ? *g_s : *(g_s = co::_make_rootic<co::string>(256));
}

LogSaver::LogSaver(const char* fname, unsigned fnlen, unsigned line, int level)
    : s(log_stream()) {
    n = s.size();
    s.resize(n + (LogTime::t_len + 1)); // make room for: "I0523 17:00:00.123"
    s[n] = "DIWE"[level];
    s.cat(' ', co::thread_id(), ' ').append(fname, fnlen).cat(':', line, ']', ' ');
}

LogSaver::~LogSaver() {
    s << '\n';
    mod().logger->push_normal_log(s.data() + n, s.size() - n);
    s.resize(n);
}

FatalLogSaver::FatalLogSaver(const char* fname, unsigned fnlen, unsigned line)
    : s(log_stream()) {
    s.resize(LogTime::t_len + 1);
    s.front() = 'F';
    s.cat(' ', co::thread_id(), ' ').append(fname, fnlen).cat(':', line, ']', ' ');
}

FatalLogSaver::~FatalLogSaver() {
    s << '\n';
    mod().logger->push_fatal_log(s.data(), s.size());
}

} // xx

void set_write_cb(void(*cb)(const void*, size_t), bool also_log2local) {
    g_write_cb = cb;
    g_also_log2local = also_log2local;
}

void close() {
    xx::mod().logger->stop();
}

} // log
} // _xx

#ifdef _WIN32
LONG WINAPI _co_on_exception(PEXCEPTION_POINTERS p) {
    return _xx::log::xx::on_exception(p);
}
#endif
