#include "co/log.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/mem.h"
#include "co/str.h"
#include "co/array.h"
#include "co/time.h"
#include "co/thread.h"
#include "../co/hook.h"
#include "stack_trace.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/select.h>
#endif
#include <time.h>

#ifdef _MSC_VER
#pragma warning (disable:4722)
#endif

DEF_string(log_dir, "logs", ">>#0 log dir, will be created if not exists");
DEF_string(log_file_name, "", ">>#0 name of log file, use exename if empty");
DEF_int32(min_log_level, 0, ">>#0 write logs at or above this level, 0-4 (debug|info|warning|error|fatal)");
DEF_int32(max_log_size, 4096, ">>#0 max size of a single log");
DEF_int64(max_log_file_size, 256 << 20, ">>#0 max size of log file, default: 256MB");
DEF_uint32(max_log_file_num, 8, ">>#0 max number of log files");
DEF_uint32(max_log_buffer_size, 32 << 20, ">>#0 max size of log buffer, default: 32MB");
DEF_uint32(log_flush_ms, 128, ">>#0 flush the log buffer every n ms");
DEF_bool(cout, false, ">>#0 also logging to terminal");
DEF_bool(log_daily, false, ">>#0 if true, enable daily log rotation");
DEF_bool(log_compress, false, ">>#0 if true, compress rotated log files with xz");

// Detect if it is safe to start the logging thread.
// When this value is true, the flag log_dir and log_file_name should have been 
// initialized, and we are safe to start the logging thread.
static bool _is_safe_to_start = false;
static fastring _co_log_xx = []() { _is_safe_to_start = true; return ""; }();

#ifdef _WIN32
LONG WINAPI _co_on_exception(PEXCEPTION_POINTERS p); // handler for win32 exceptions

void _co_set_except_handler() {
    SetUnhandledExceptionFilter(_co_on_exception);
}
#else
void _co_set_except_handler() {}
#endif

namespace ___ {
namespace log {
namespace xx {

class LogTime;
class LogFile;
class FailureHandler;
class Logger;

// collection of global variables or objects
struct Global {
    Global();
    ~Global() = delete;

    bool check_failed;
    fastring* s; // cache
    fastring* exename;
    LogTime* log_time;
    LogFile* log_file;
    FailureHandler* failure_handler;
    Logger* logger;
};

inline Global& global() {
    static Global* g = co::static_new<Global>();
    return *g;
}

// do cleanup at exit
struct Cleanup {
    Cleanup() { (void) global(); }
    ~Cleanup();
};

static Cleanup _gc;

// time string for the logs: "0723 17:00:00.123"
class LogTime {
  public:
    enum {
        t_len = 17, // length of time 
        t_min = 8,  // position of minute
        t_sec = t_min + 3,
        t_ms = t_sec + 3,
    };

    LogTime() : _start(0) {
        memset(_buf, 0, sizeof(_buf));
        this->update();
    }

    void update();
    const char* get() const { return _buf; }
    uint32 day() const { return *(uint32*)_buf; }

  private:
    time_t _start;
    struct tm _tm;
    char _buf[24]; // save the time string
};

// the local file that logs will be written to
class LogFile {
  public:
    LogFile()
        : _file(256), _path(256), _path_base(256),
          _day(0), _checked(false) {
        _file.open("", 'a');
    }

    fs::file& open(const char* topic, int level, LogTime* t);
    void write(const char* p, size_t n, LogTime* t);
    void write(const char* topic, const char* p, size_t n, LogTime* t);

  private:
    bool check_config(const char* topic, int level, LogTime* t);
    uint32 day_in_path(const fastring& path);
    void compress_file(const fastring& path);

  private:
    fs::file _file;
    fastring _path;
    fastring _path_base; // prefix of the log path: log_dir/log_file_name 
    co::deque<fastring> _old_paths; // paths of old log files
    uint32 _day;
    bool _checked;
};

void on_signal(int sig);  // handler for SIGINT SIGTERM SIGQUIT
void on_failure(int sig); // handler for SIGSEGV SIGABRT SIGFPE SIGBUS SIGILL

#ifdef _WIN32
LONG WINAPI on_exception(PEXCEPTION_POINTERS p); // handler for win32 exceptions
#endif

class FailureHandler {
  public:
    FailureHandler();
    ~FailureHandler();

    void on_signal(int sig);
    void on_failure(int sig);

  #ifdef _WIN32
    int on_exception(PEXCEPTION_POINTERS p); // for windows only
  #endif

  private:
    log::StackTrace* _stack_trace;
    co::map<int, os::sig_handler_t> _old_handlers;
};

class Logger {
  public:
    static const uint32 N = 128 * 1024;
    static const int A = 8; // array size

    Logger() : _log_event(true, false), _log_thread(0), _stop(0) {}
    ~Logger() = delete;

    bool start();
    void stop(bool signal_safe=false);

    void push(char* s, size_t n);
    void push(const char* topic, char* s, size_t n);
    void push_fatal_log(char* s, size_t n);

    void set_write_cb(const std::function<void(const void*, size_t)>& cb, int flags) {
        _llog.write_cb = cb;
        _llog.write_flags = flags;
    }

    void set_write_cb(const std::function<void(const char*, const void*, size_t)>& cb, int flags) {
        _tlog.write_cb = cb;
        _tlog.write_flags = flags;
    }

  private:
    struct LevelLog {
        LevelLog() : bytes(0), counter(0), write_cb(NULL), write_flags(0) {}
        ::Mutex mtx;
        fastream buf;      // logs will be pushed to this buffer
        fastream logs;     // to swap out logs in buf
        char time_str[24]; // "0723 17:00:00.123"
        size_t bytes;
        size_t counter;
        std::function<void(const void*, size_t)> write_cb;
        int write_flags;
    };

    struct PerTopic {
        PerTopic() : buf(N), logs(N), file(), topic(0), bytes(0), counter(0) {}
        fastream buf;
        fastream logs;
        LogFile file;
        const char* topic;
        size_t bytes;   // write bytes
        size_t counter; // write times
    };

    typedef const char* Key;

    struct TLog {
        TLog() : pts(8), write_cb(NULL), write_flags(0) {}
        struct {
            ::Mutex mtx;
            co::hash_map<Key, PerTopic> mp;
            LogTime log_time;
            char time_str[24];
        } v[A];
        co::array<PerTopic*> pts;
        std::function<void(const char*, const void*, size_t)> write_cb;
        int write_flags;
    };

    void write_logs(const char* p, size_t n, LogTime* t);
    void write_tlogs(co::array<PerTopic*>& v, LogTime* t);
    void thread_fun();

  private:
    LevelLog _llog;
    TLog _tlog;
    SyncEvent _log_event;
    Thread* _log_thread;
    int _stop; // 0: init, 1: stopping, 2: logging thread stopped, 3: final
};

Global::Global()
    : check_failed(false), logger(NULL) {
  #ifndef _WIN32
    if (!__sys_api(write)) { auto r = ::write(-1, 0, 0); (void)r; }
    if (!__sys_api(select)) ::select(-1, 0, 0, 0, 0);
  #endif
    s = co::static_new<fastring>(4096);
    exename = co::static_new<fastring>(os::exename());
    log_time = co::static_new<LogTime>();
    log_file = co::static_new<LogFile>();
    failure_handler = co::static_new<FailureHandler>();
    logger = co::static_new<Logger>();
}

Cleanup::~Cleanup() {
    global().logger->stop();
}

bool Logger::start() {
    // check config, ensure max_log_buffer_size >= 1M, max_log_size >= 256
    static bool x = []() {
        auto& bs = FLG_max_log_buffer_size;
        auto& ls = FLG_max_log_size;
        if (bs < (1 << 20)) bs = 1 << 20;
        if (ls < 256) ls = 256;
        if ((uint32)ls > (bs >> 2)) ls = (int32)(bs >> 2);
        return true;
    }();
    (void)x;

    if (atomic_bool_cas(&_log_thread, (void*)0, (void*)8)) {
        global().log_time->update();
        memcpy(_llog.time_str, global().log_time->get(), 24);
        _llog.buf.reserve(N);
        _llog.logs.reserve(N);
        for (int i = 0; i < A; ++i) {
            memcpy(_tlog.v[i].time_str, global().log_time->get(), 24);
        }
        atomic_store(&_log_thread, co::static_new<Thread>(&Logger::thread_fun, this));
    } else {
        while (atomic_load(&_log_thread, mo_relaxed) == (Thread*)8) {
            sleep::ms(1);
        }
    }
    return true;
}

void signal_safe_sleep(int ms) {
  #ifdef _WIN32
    co::disable_hook_sleep();
    ::Sleep(ms);
    co::enable_hook_sleep();
  #else
    struct timeval tv = { 0, ms * 1000 };
    __sys_api(select)(0, 0, 0, 0, &tv);
  #endif
}

// if signal_safe is true, try to call only async-signal-safe api in this function 
// according to:  http://man7.org/linux/man-pages/man7/signal-safety.7.html
void Logger::stop(bool signal_safe) {
    auto x = atomic_compare_swap(&_stop, 0, 1);
    if (x == 0) {
        if (!signal_safe) {
            if (_log_thread) {
                _log_event.signal();
                _log_thread->join();
            }
        } else {
            // wait until the logging thread stopped
            if (_log_thread) {
                while (_stop != 2) signal_safe_sleep(1);
            }
        }

        // it may be not safe if logs are still being pushed to the buffer 
        !signal_safe ? _llog.mtx.lock() : signal_safe_sleep(1);
        if (!_llog.buf.empty()) {
            _llog.buf.swap(_llog.logs);
            this->write_logs(_llog.logs.data(), _llog.logs.size(), global().log_time);
            _llog.logs.clear();
        }
        if (!signal_safe) _llog.mtx.unlock();

        for (int i = 0; i < A; ++i) {
            auto& v = _tlog.v[i];
            if (!signal_safe) v.mtx.lock();
            for (auto it = v.mp.begin(); it != v.mp.end(); ++it) {
                auto pt = &it->second;
                if (!pt->buf.empty()) {
                    pt->buf.swap(pt->logs);
                    if (!pt->topic) pt->topic = it->first;
                    _tlog.pts.push_back(pt);
                }
            }
            
            if (!_tlog.pts.empty()) {
                this->write_tlogs(_tlog.pts, &v.log_time);
                _tlog.pts.clear();
            }
            if (!signal_safe) v.mtx.unlock();
        }

        atomic_swap(&_stop, 3);
    } else {
        while (_stop != 3) signal_safe_sleep(1);
    }
}

void Logger::thread_fun() {
    while (!_is_safe_to_start) _log_event.wait(8);
    while (!_stop) {
        bool signaled = _log_event.wait(FLG_log_flush_ms);
        if (_stop) break;

        global().log_time->update();
        {
            ::MutexGuard g(_llog.mtx);
            memcpy(_llog.time_str, global().log_time->get(), LogTime::t_len);
            if (!_llog.buf.empty()) _llog.buf.swap(_llog.logs);
        }

        if (!_llog.logs.empty()) {
            this->write_logs(_llog.logs.data(), _llog.logs.size(), global().log_time);
            _llog.logs.clear();
        }

        for (int i = 0; i < A; ++i) {
            auto& v = _tlog.v[i];
            v.log_time.update();
            {
                ::MutexGuard g(v.mtx);
                memcpy(v.time_str, v.log_time.get(), LogTime::t_len);
                for (auto it = v.mp.begin(); it != v.mp.end(); ++it) {
                    PerTopic* pt = &it->second;
                    if (!pt->buf.empty()) {
                        pt->buf.swap(pt->logs);
                        if (!pt->topic) pt->topic = it->first;
                        _tlog.pts.push_back(pt);
                    }
                }
            }

            if (!_tlog.pts.empty()) {
                this->write_tlogs(_tlog.pts, &v.log_time);
                _tlog.pts.clear();
            }
        }

        if (signaled) _log_event.reset();
    }

    atomic_swap(&_stop, 2);
}

void Logger::write_logs(const char* p, size_t n, LogTime* t) {
    // log to local file
    if (!_llog.write_cb || (_llog.write_flags & log::log2local)) {
        global().log_file->write(p, n, t);
    }

    // write logs through the write callback
    if (_llog.write_cb) _llog.write_cb(p, n);

    // log to stderr
    if (FLG_cout) fwrite(p, 1, n, stderr);

    _llog.bytes += n;
    if (++_llog.counter == 32) {
        const size_t avg = _llog.bytes >> 5;
        const size_t cap = _llog.logs.capacity();
        if ((cap > N * 8) && (cap > avg * 4)) {
            _llog.logs.reset();
            _llog.logs.reserve(avg < (N >> 1) ? N : (avg << 1));
        }
        _llog.bytes = 0;
        _llog.counter = 0;
    }
}

void Logger::write_tlogs(co::array<PerTopic*>& v, LogTime* t) {
    for (size_t i = 0; i < v.size(); ++i) {
        auto pt = v[i];
        if (!_tlog.write_cb || (_tlog.write_flags & log::log2local)) {
            pt->file.write(pt->topic, pt->logs.data(), pt->logs.size(), t);
        }

        if (_tlog.write_cb) {
            _tlog.write_cb(pt->topic, pt->logs.data(), pt->logs.size());
        }

        if (FLG_cout) fwrite(pt->logs.data(), 1, pt->logs.size(), stderr);

        pt->bytes += pt->logs.size();
        if (++pt->counter == 32) {
            const size_t avg = pt->bytes >> 5;
            const size_t cap = pt->logs.capacity();
            if ((cap > N * 8) && (cap > avg * 4)) {
                pt->logs.reset();
                pt->logs.reserve(avg < (N >> 1) ? N : (avg << 1));
            }
            pt->bytes = 0;
            pt->counter = 0;
        }
        pt->logs.clear();
    }
}

void Logger::push(char* s, size_t n) {
    static bool ks = this->start(); (void)ks;
    if (!_stop) {
        if (unlikely(n > (uint32)FLG_max_log_size)) {
            n = FLG_max_log_size;
            char* const p = s + n - 4;
            p[0] = '.';
            p[1] = '.';
            p[2] = '.';
            p[3] = '\n';
        }
        {
            ::MutexGuard g(_llog.mtx);
            memcpy(s + 1, _llog.time_str, LogTime::t_len); // log time

            auto& buf = _llog.buf;
            if (unlikely(buf.size() + n >= FLG_max_log_buffer_size)) {
                const char* p = strchr(buf.data() + (buf.size() >> 1) + 7, '\n');
                const size_t len = buf.data() + buf.size() - p - 1;
                memcpy((char*)(buf.data()), "......\n", 7);
                memcpy((char*)(buf.data()) + 7, p + 1, len);
                buf.resize(len + 7);
            }

            buf.append(s, n);
            if (buf.size() > (buf.capacity() >> 1)) _log_event.signal();
        }
    }
}

void Logger::push(const char* topic, char* s, size_t n) {
    static bool ks = this->start(); (void)ks;
    if (!_stop) {
        if (unlikely(n > (uint32)FLG_max_log_size)) {
            n = FLG_max_log_size;
            char* const p = s + n - 4;
            p[0] = '.';
            p[1] = '.';
            p[2] = '.';
            p[3] = '\n';
        }

        auto& v = _tlog.v[murmur_hash(topic, strlen(topic)) & (A - 1)];
        {
            ::MutexGuard g(v.mtx);
            memcpy(s, v.time_str, LogTime::t_len);

            auto& x = v.mp[topic];
            auto& buf = x.buf;
            if (unlikely(buf.size() + n >= FLG_max_log_buffer_size)) {
                const char* p = strchr(buf.data() + (buf.size() >> 1) + 7, '\n');
                const size_t len = buf.data() + buf.size() - p - 1;
                memcpy((char*)(buf.data()), "......\n", 7);
                memcpy((char*)(buf.data()) + 7, p + 1, len);
                buf.resize(len + 7);
            }

            buf.append(s, n);
            if (buf.size() > (buf.capacity() >> 1)) _log_event.signal();
        }
    }
}

void Logger::push_fatal_log(char* s, size_t n) {
    this->stop();
    LogTime* const t = global().log_time;
    memcpy(s + 1, t->get(), LogTime::t_len);

    this->write_logs(s, n, t);
    if (!FLG_cout) fwrite(s, 1, n, stderr);
    if (global().log_file->open(NULL, fatal, t)) global().log_file->write(s, n, t);

    atomic_swap(&global().check_failed, true);
    abort();
}

inline void log2stderr(const char* s, size_t n) {
  #ifdef _WIN32
    auto r = ::fwrite(s, 1, n, stderr); (void)r;
  #else
    auto r = __sys_api(write)(STDERR_FILENO, s, n); (void)r;
  #endif
}

// get day from xx_0808_15_30_08.123.log
inline uint32 LogFile::day_in_path(const fastring& path) {
    uint32 x = 0;
    const int n = LogTime::t_len + 4;
    if (path.size() > n) god::byte_cp<4>(&x, path.data() + path.size() - n);
    return x;
}

// compress log file with the xz command
inline void LogFile::compress_file(const fastring& path) {
    fastring cmd(path.size() + 8);
    cmd.append("xz ").append(path);
    os::system(cmd);
}

bool LogFile::check_config(const char* topic, int level, LogTime* t) {
    auto& s = *global().s;

    // log dir, backslash to slash
    for (size_t i = 0; i < FLG_log_dir.size(); ++i) {
        if (FLG_log_dir[i] == '\\') FLG_log_dir[i] = '/';
    }

    // log file name, use process name by default
    s.clear();
    if (FLG_log_file_name.empty()) {
        s.append(*global().exename);
    } else {
        s.append(FLG_log_file_name);
        s.remove_tail(".log");
    }
    s.remove_tail(".exe");

    // prefix of log file path
    _path_base << FLG_log_dir;
    if (!_path_base.empty() && _path_base.back() != '/') _path_base << '/';
    if (topic) {
        _path_base << topic << '/' << s << '_' << topic;
    } else {
        _path_base << s;
    }

    // read paths of old log files
    if (level != xx::fatal) {
        s.clear();
        s.append(_path_base).append("_old.log");
        fs::file f(s.c_str(), 'r');
        if (f) {
            auto v = str::split(f.read(f.size()), '\n');
            for (auto& x : v) _old_paths.push_back(std::move(x));
        }
    }

    _day = t->day();
    return true;
}

fs::file& LogFile::open(const char* topic, int level, LogTime* t) {
    if (!_checked) {
        this->check_config(topic, level, t);
        _checked = true;
    }

    auto& g = global();
    auto& s = *g.s; s.clear();

    if (!topic) {
        if (!fs::exists(FLG_log_dir)) fs::mkdir((char*)FLG_log_dir.c_str(), true);
    } else {
        s << FLG_log_dir;
        if (!s.empty() && s.back() != '/') s << '/';
        s << topic;
        if (!fs::exists(s)) fs::mkdir((char*)s.c_str(), true);
    }

    _path.clear();
    if (level != xx::fatal) {
        _path.append(_path_base).append(".log");

        bool new_file = !fs::exists(_path);
        if (!new_file) {
            if (!_old_paths.empty()) {
                auto& path = _old_paths.back();
                if (fs::fsize(_path) >= FLG_max_log_file_size || (
                    FLG_log_daily && this->day_in_path(path) != _day)) {
                    fs::rename(_path, path); // rename xx.log to xx_0808_15_30_08.123.log
                    if (FLG_log_compress) this->compress_file(path);
                    new_file = true;
                }
            } else {
                new_file = true;
            }
        }
      
        if (_file.open(_path.c_str(), 'a') && new_file) {
            char x[24] = { 0 }; // 0723 17:00:00.123
            memcpy(x, t->get(), LogTime::t_len);
            for (int i = 0; i < LogTime::t_len; ++i) {
                if (x[i] == ' ' || x[i] == ':') x[i] = '_';
            }

            s.clear();
            s << _path_base << '_' << x << ".log";
            _old_paths.push_back(s);

            while (!_old_paths.empty() && _old_paths.size() > FLG_max_log_file_num) {
                if (FLG_log_compress) _old_paths.front().append(".xz");
                fs::remove(_old_paths.front());
                _old_paths.pop_front();
            }

            s.resize(_path_base.size());
            s.append("_old.log");
            fs::file f(s.c_str(), 'w');
            if (f) {
                s.clear();
                for (auto& x : _old_paths) s << x << '\n';
                f.write(s);
            }
        }

    } else {
        _path.append(_path_base).append(".fatal");
        _file.open(_path.c_str(), 'a');
    }

    if (!_file) {
        s.clear();
        s << "cann't open the file: " << _path << '\n';
        log2stderr(s.data(), s.size());
    }
    return _file;
}

void LogFile::write(const char* p, size_t n, LogTime* t) {
    if (FLG_log_daily) {
        const uint32 day = t->day();
        if (_day != day) { _day = day; _file.close(); }
    }

    if (_file || this->open(NULL, 0, t)) {
        _file.write(p, n);
        const uint64 size = _file.size(); // -1 if not exists
        if (size >= (uint64)FLG_max_log_file_size) _file.close();
    }
}

void LogFile::write(const char* topic, const char* p, size_t n, LogTime* t) {
    if (FLG_log_daily) {
        const uint32 day = t->day();
        if (_day != day) { _day = day; _file.close(); }
    }

    if (_file || this->open(topic, 0, t)) {
        _file.write(p, n);
        const uint64 size = _file.size(); // -1 if not exists
        if (size >= (uint64)FLG_max_log_file_size) _file.close();
    }
}

FailureHandler::FailureHandler()
    : _stack_trace(log::stack_trace()) {
    _old_handlers[SIGINT] = os::signal(SIGINT, xx::on_signal);
    _old_handlers[SIGTERM] = os::signal(SIGTERM, xx::on_signal);

  #ifdef _WIN32
    _old_handlers[SIGABRT] = os::signal(SIGABRT, xx::on_failure);
    // Signal handler for SIGSEGV and SIGFPE installed in main thread does 
    // not work for other threads. Use SetUnhandledExceptionFilter instead.
    _co_set_except_handler();
  #else
    const int x = SA_RESTART | SA_ONSTACK;
    _old_handlers[SIGQUIT] = os::signal(SIGQUIT, xx::on_signal);
    _old_handlers[SIGABRT] = os::signal(SIGABRT, xx::on_failure, x);
    _old_handlers[SIGSEGV] = os::signal(SIGSEGV, xx::on_failure, x);
    _old_handlers[SIGFPE] = os::signal(SIGFPE, xx::on_failure, x);
    _old_handlers[SIGBUS] = os::signal(SIGBUS, xx::on_failure, x);
    _old_handlers[SIGILL] = os::signal(SIGILL, xx::on_failure, x);
    os::signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE
  #endif
}

FailureHandler::~FailureHandler() {
    os::signal(SIGINT, SIG_DFL);
    os::signal(SIGTERM, SIG_DFL);
    os::signal(SIGABRT, SIG_DFL);
  #ifndef _WIN32
    os::signal(SIGSEGV, SIG_DFL);
    os::signal(SIGFPE, SIG_DFL);
    os::signal(SIGBUS, SIG_DFL);
    os::signal(SIGILL, SIG_DFL);
  #endif
}

void FailureHandler::on_signal(int sig) {
    if (global().logger) global().logger->stop(true);
    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}

void FailureHandler::on_failure(int sig) {
    auto& g = global();
    if (g.logger) g.logger->stop(true);

    auto& f = g.log_file->open(NULL, fatal, g.log_time);
    auto& s = *g.s; s.clear();
    if (!g.check_failed) {
        s << 'F' << g.log_time->get() << "] ";
    }

    switch (sig) {
      case SIGABRT:
        if (!g.check_failed) s << "SIGABRT: aborted\n";
        break;
    #ifndef _WIN32
      case SIGSEGV:
        s << "SIGSEGV: segmentation fault\n";
        break;
      case SIGFPE:
        s << "SIGFPE: floating point exception\n";
        break;
      case SIGBUS:
        s << "SIGBUS: bus error\n";
        break;
      case SIGILL:
        s << "SIGILL: illegal instruction\n";
        break;
    #endif
      default:
        s << "caught unexpected signal\n";
        break;
    }

    if (!s.empty()) {
        if (f) f.write(s);
        log2stderr(s.data(), s.size());
    }

    if (_stack_trace) _stack_trace->dump_stack(
        &f, g.check_failed ? 7 : (sig == SIGABRT ? 4 : 3)
    );
    if (f) { f.write('\n'); f.close(); }

    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}

#ifdef _WIN32
int FailureHandler::on_exception(PEXCEPTION_POINTERS p) {
    auto& g = global();
    const char* err = NULL;

    switch (p->ExceptionRecord->ExceptionCode) {
      case EXCEPTION_ACCESS_VIOLATION:
        err = "Error: EXCEPTION_ACCESS_VIOLATION";
        break;
      case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        err = "Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        break;
      case EXCEPTION_DATATYPE_MISALIGNMENT:
        err = "Error: EXCEPTION_DATATYPE_MISALIGNMENT";
        break;
      case EXCEPTION_FLT_DENORMAL_OPERAND:
        err = "Error: EXCEPTION_FLT_DENORMAL_OPERAND";
        break;
      case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        err = "Error: EXCEPTION_FLT_DIVIDE_BY_ZERO";
        break;
      case EXCEPTION_FLT_INVALID_OPERATION:
        err = "Error: EXCEPTION_FLT_INVALID_OPERATION";
        break;
      case EXCEPTION_FLT_OVERFLOW:
        err = "Error: EXCEPTION_FLT_OVERFLOW";
        break;
      case EXCEPTION_FLT_STACK_CHECK:
        err = "Error: EXCEPTION_FLT_STACK_CHECK";
        break;
      case EXCEPTION_FLT_UNDERFLOW:
        err = "Error: EXCEPTION_FLT_UNDERFLOW";
        break;
      case EXCEPTION_ILLEGAL_INSTRUCTION:
        err = "Error: EXCEPTION_ILLEGAL_INSTRUCTION";
        break;
      case EXCEPTION_IN_PAGE_ERROR:
        err = "Error: EXCEPTION_IN_PAGE_ERROR";
        break;
      case EXCEPTION_INT_DIVIDE_BY_ZERO:
        err = "Error: EXCEPTION_INT_DIVIDE_BY_ZERO";
        break;
      case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        err = "Error: EXCEPTION_NONCONTINUABLE_EXCEPTION";
        break;
      case EXCEPTION_PRIV_INSTRUCTION:
        err = "Error: EXCEPTION_PRIV_INSTRUCTION";
        break;
      case EXCEPTION_STACK_OVERFLOW:
        err = "Error: EXCEPTION_STACK_OVERFLOW";
        break;
      case 0xE06D7363: // STATUS_CPP_EH_EXCEPTION, std::runtime_error()
        err = "Error: STATUS_CPP_EH_EXCEPTION";
        break;
      case 0xE0434f4D: // STATUS_CLR_EXCEPTION, VC++ Runtime error
        err = "Error: STATUS_CLR_EXCEPTION";
        break;
      case 0xCFFFFFFF: // STATUS_APPLICATION_HANG
        err = "Error: STATUS_APPLICATION_HANG -- Application hang";
        break;
      case STATUS_INVALID_HANDLE:
        err = "Error: STATUS_INVALID_HANDLE";
        break;
      case STATUS_STACK_BUFFER_OVERRUN:
        err = "Error: STATUS_STACK_BUFFER_OVERRUN";
        break;
      default:
        err = "Unexpected error: ";
        break;
    }

    if (g.logger) g.logger->stop();
    auto& f = g.log_file->open(NULL, fatal, g.log_time);
    auto& s = *g.s; s.clear();
    s << 'F' << g.log_time->get() << "] " << err;
    if (err[0] == 'U') s << (void*)(size_t)p->ExceptionRecord->ExceptionCode;
    s << '\n';
    if (f) f.write(s);
    log2stderr(s.data(), s.size());

    if (_stack_trace) _stack_trace->dump_stack(&f, 6);
    if (f) { f.write('\n'); f.close(); }

    ::exit(0);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void on_signal(int sig) {
    global().failure_handler->on_signal(sig);
}

void on_failure(int sig) {
    global().failure_handler->on_failure(sig);
}

void LogTime::update() {
    const int64 now_ms = epoch::ms();
    const time_t now_sec = now_ms / 1000;
    const int dt = (int) (now_sec - _start);

    if (unlikely(_start == 0)) goto reset;
    if (unlikely(dt < 0 || dt > 60)) goto reset;
    if (dt == 0) goto set_ms;

    static uint16* tb60 = []() {
        static uint16 tb[60];
        for (int i = 0; i < 60; ++i) {
            uint8* p = (uint8*) &tb[i];
            p[0] = (uint8)('0' + i / 10);
            p[1] = (uint8)('0' + i % 10);
        }
        return tb;
    }();

    _tm.tm_sec += dt;
    if (_tm.tm_min < 59 || _tm.tm_sec < 60) {
        _start = now_sec;
        if (_tm.tm_sec < 60) {
            int8* p = (int8*)(tb60 + _tm.tm_sec);
            _buf[t_sec] = (char)p[0];
            _buf[t_sec + 1] = (char)p[1];
        } else {
            _tm.tm_min++;
            _tm.tm_sec -= 60;
            *(uint16*)(_buf + t_min) = tb60[_tm.tm_min];
            char* p = (char*)(tb60 + _tm.tm_sec);
            _buf[t_sec] = p[0];
            _buf[t_sec + 1] = p[1];
        }
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
        uint32 ms = (uint32)(now_ms - _start * 1000);
        char* p = _buf + t_ms;
        uint32 x = ms / 100;
        p[0] = (char)('0' + x);
        ms -= x * 100;
        x = ms / 10;
        p[1] = (char)('0' + x);
        p[2] = (char)('0' + (ms - x * 10));
    }
}

inline fastream& log_stream() {
    static __thread fastream* kls = 0;
    return kls ? *kls : *(kls = new fastream(256));
}

LevelLogSaver::LevelLogSaver(const char* prefix, int n, int level)
    : _s(log_stream()) {
    _n = _s.size();
    _s.resize(_n + (LogTime::t_len + 1)); // make room for: "I0523 17:00:00.123"
    _s[_n] = "DIWE"[level];
    (_s << ' ' << co::thread_id() << ' ').append(prefix, n);
}

LevelLogSaver::~LevelLogSaver() {
    _s << '\n';
    global().logger->push((char*)_s.data() + _n, _s.size() - _n);
    _s.resize(_n);
}

FatalLogSaver::FatalLogSaver(const char* prefix, int n)
    : _s(log_stream()) {
    _s.resize(LogTime::t_len + 1);
    _s.front() = 'F';
    (_s << ' ' << co::thread_id() << ' ').append(prefix, n);
}

FatalLogSaver::~FatalLogSaver() {
    _s << '\n';
    global().logger->push_fatal_log((char*)_s.data(), _s.size());
}

TLogSaver::TLogSaver(const char* prefix, int n, const char* topic)
    : _s(log_stream()), _topic(topic) {
    _n = _s.size();
    _s.resize(_n + (LogTime::t_len)); // make room for: "0523 17:00:00.123"
    (_s << ' ' << co::thread_id() << ' ').append(prefix, n);
}

TLogSaver::~TLogSaver() {
    _s << '\n';
    global().logger->push(_topic, (char*)_s.data() + _n, _s.size() - _n);
    _s.resize(_n);
}

} // namespace xx

void exit() {
    xx::global().logger->stop();
}

void set_write_cb(const std::function<void(const void*, size_t)>& cb, int flags) {
    xx::global().logger->set_write_cb(cb, flags);
}

void set_write_cb(const std::function<void(const char*, const void*, size_t)>& cb, int flags) {
    xx::global().logger->set_write_cb(cb, flags);
}

} // namespace log
} // namespace ___

#ifdef _WIN32
LONG WINAPI _co_on_exception(PEXCEPTION_POINTERS p) {
    return ___::log::xx::global().failure_handler->on_exception(p);
}
#endif
