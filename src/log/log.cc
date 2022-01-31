#include "co/log.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/str.h"
#include "co/path.h"
#include "co/time.h"
#include "co/alloc.h"
#include "co/thread.h"
#include "../co/hook.h"
#include "stack_trace.h"

#include <time.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#include <sys/select.h>
#endif

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
DEF_bool(syslog, false, ">>#0 add syslog header to each log if true");
DEF_bool(log_daily, false, ">>#0 if true, enable daily log rotation");
DEF_bool(log_compress, false, ">>#0 if true, compress rotated log files with xz");

namespace ___ {
namespace log {
namespace xx {

class LogTime;
class LogFile;
class FailureHandler;
class LevelLogger;

// collection of global variables or objects
struct Global {
    Global();
    ~Global() = delete;

    bool check_failed;
    fastring* s; // cache
    LogTime* log_time;
    LogFile* log_file;
    FailureHandler* failure_handler;
    LevelLogger* level_logger;
};

inline Global& global() {
    static Global* g = co::new_static<Global>();
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
        : _path(256), _path_base(256), _exename(os::exename()), _day(0) {
    }

    fs::file& open(int level=0);
    void write(const char* p, size_t n);

  private:
    bool check_config(bool signal_safe);
    uint32 day_in_path(const fastring& path);
    void compress_file(const fastring& path);

  private:
    fs::file _file;
    fastring _path;
    fastring _path_base; // prefix of the log path: log_dir/log_file_name 
    fastring _exename;   // use exename as log_file_name by default
    std::deque<fastring> _old_paths; // paths of old log files
    uint32 _day;
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
    std::map<int, os::sig_handler_t> _old_handlers;
  #ifdef _WIN32
    void* _ex_handler; // exception handler for windows
  #endif
};

class LevelLogger {
  public:
    LevelLogger() : _log_event(true, false), _stop(0), _write_flags(0) {}
    ~LevelLogger() = delete;

    bool start();
    void stop(bool signal_safe=false);

    void push(char* s, size_t n);
    void push_fatal_log(char* s, size_t n);

    void set_write_cb(const std::function<void(const void*, size_t)>& cb, int flags) {
        _write_cb = cb;
        _write_flags = flags;
    }

  private:
    void write_logs(const char* p, size_t n);
    void thread_fun();

  private:
    ::Mutex _log_mutex;
    SyncEvent _log_event;
    std::unique_ptr<Thread> _log_thread;
    fastream _log_buf;  // logs will be pushed to this buffer
    fastream _logs;     // to swap out logs in _log_buf
    char _log_time[24]; // "0723 17:00:00.123"
    int _stop;          // 0: init, 1: stopping, 2: logging thread stopped, 3: final
    int _write_flags;   // flags for the write callback
    std::function<void(const void*, size_t)> _write_cb; // user callback for writing logs
};

Global::Global()
    : check_failed(false), level_logger(NULL) {
  #ifndef _WIN32
    if (!CO_RAW_API(write)) { auto r = ::write(-1, 0, 0); (void)r; }
    if (!CO_RAW_API(select)) ::select(-1, 0, 0, 0, 0);
  #endif
    s = co::new_static<fastring>(4096);
    log_time = co::new_static<LogTime>();
    log_file = co::new_static<LogFile>();
    failure_handler = co::new_static<FailureHandler>();
    level_logger = co::new_static<LevelLogger>();
}

Cleanup::~Cleanup() {
    global().level_logger->stop();
}

bool LevelLogger::start() {
    // check config, ensure max_log_buffer_size >= 1M, max_log_size >= 256 
    auto& bs = FLG_max_log_buffer_size;
    auto& ls = FLG_max_log_size;
    if (bs < (1 << 20)) bs = (1 << 20);
    if (ls < 256) ls = 256;
    if ((uint32)ls > (bs >> 2)) ls = (int32)(bs >> 2);

    global().log_time->update();
    memcpy(_log_time, global().log_time->get(), 24);

    _log_buf.reserve(1 << 20);
    _logs.reserve(1 << 20);
    _log_thread.reset(new Thread(&LevelLogger::thread_fun, this));
    return true;
}

void signal_safe_sleep(int ms) {
  #ifdef _WIN32
    co::hook::disable_hook_sleep();
    ::Sleep(ms);
    co::hook::enable_hook_sleep();
  #else
    struct timeval tv = { 0, ms * 1000 };
    CO_RAW_API(select)(0, 0, 0, 0, &tv);
  #endif
}

void LevelLogger::stop(bool signal_safe) {
    auto x = atomic_compare_swap(&_stop, 0, 1);

    if (!signal_safe) {
        if (x == 0) {
            _log_event.signal();
            if (_log_thread != NULL) _log_thread->join();

            ::MutexGuard g(_log_mutex);
            if (!_log_buf.empty()) {
                this->write_logs(_log_buf.data(), _log_buf.size());
                _log_buf.clear();
            }

            atomic_swap(&_stop, 3);
        } else {
            while (_stop != 3) sleep::ms(1);
        }

    } else {
        // Try to call only async-signal-safe api in this function according to:
        //   http://man7.org/linux/man-pages/man7/signal-safety.7.html
        if (x == 0) {
            // wait until the logging thread stopped
            if (_log_thread != NULL) {
                while (_stop != 2) signal_safe_sleep(1);
            }

            // it may be not safe if logs are still being pushed to _log_buf, 
            // sleep 1 ms here
            signal_safe_sleep(1);
            if (!_log_buf.empty()) {
                this->write_logs(_log_buf.data(), _log_buf.size());
                _log_buf.clear();
            }

            atomic_swap(&_stop, 3);
        } else {
            while (_stop != 3) signal_safe_sleep(1);
        }
    }
}

void LevelLogger::thread_fun() {
    while (!_stop) {
        bool signaled = _log_event.wait(FLG_log_flush_ms);
        if (_stop) break;

        global().log_time->update();
        {
            ::MutexGuard g(_log_mutex);
            memcpy(_log_time, global().log_time->get(), LogTime::t_len);
            if (!_log_buf.empty()) _log_buf.swap(_logs);
            if (signaled) _log_event.reset();
        }

        if (!_logs.empty()) {
            this->write_logs(_logs.data(), _logs.size());
            _logs.clear();
        }
    }

    atomic_swap(&_stop, 2);
}

void LevelLogger::write_logs(const char* p, size_t n) {
    // log to local file
    if (!_write_cb || (_write_flags & log::log2local)) {
        global().log_file->write(p, n);
    }

    // write logs through the write callback
    if (_write_cb) {
        if (!(_write_flags & log::splitlogs)) { /* write all logs once */
            _write_cb(p, n);
        } else { /* split logs and write one by one */
            const char* b = p;
            const char* e = b + n;
            const char* s;
            while (b < e) {
                s = (const char*) memchr(b, '\n', e - b);
                if (s) {
                    _write_cb(b, s - b + 1);
                    b = s + 1;
                } else {
                    _write_cb(b, e - b);
                    break;
                }
            }
        }
    }

    // log to stderr
    if (FLG_cout) fwrite(p, 1, n, stderr);
}

void LevelLogger::push(char* s, size_t n) {
    static bool ks = this->start();
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
            ::MutexGuard g(_log_mutex);
            memcpy(*s != '<' ? s + 1 : s + 5, _log_time, LogTime::t_len); // log time

            if (unlikely(_log_buf.size() + n >= FLG_max_log_buffer_size)) {
                const char* p = strchr(_log_buf.data() + (_log_buf.size() >> 1) + 7, '\n');
                const size_t len = _log_buf.data() + _log_buf.size() - p - 1;
                memcpy((char*)(_log_buf.data()), "......\n", 7);
                memcpy((char*)(_log_buf.data()) + 7, p + 1, len);
                _log_buf.resize(len + 7);
            }

            _log_buf.append(s, n);
            if (_log_buf.size() > (_log_buf.capacity() >> 1)) _log_event.signal();
        }
    }
}

void LevelLogger::push_fatal_log(char* s, size_t n) {
    this->stop();
    memcpy(*s != '<' ? s + 1 : s + 5, global().log_time->get(), LogTime::t_len);

    this->write_logs(s, n);
    if (!FLG_cout) fwrite(s, 1, n, stderr);
    if (global().log_file->open(fatal)) global().log_file->write(s, n);

    atomic_swap(&global().check_failed, true);
    abort();
}

inline void log2stderr(const char* s, size_t n) {
  #ifdef _WIN32
    auto r = ::fwrite(s, 1, n, stderr); (void)r;
  #else
    auto r = CO_RAW_API(write)(STDERR_FILENO, s, n); (void)r;
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

bool LogFile::check_config (bool signal_safe) {
    auto& s = *global().s;

    // log dir, backslash to slash
    for (size_t i = 0; i < FLG_log_dir.size(); ++i) {
        if (FLG_log_dir[i] == '\\') FLG_log_dir[i] = '/';
    }

    // log file name, use process name by default
    s.clear();
    if (FLG_log_file_name.empty()) {
        s.append(_exename);
    } else {
        s.append(FLG_log_file_name);
        s.remove_tail(".log");
    }
    s.remove_tail(".exe");

    // prefix of log file path
    _path_base << FLG_log_dir << '/' << s;

    // read paths of old log files
    if (!signal_safe) {
        s.clear();
        s.append(_path_base).append("_old.log");
        fs::file f(s.c_str(), 'r');
        if (f) {
            auto v = str::split(f.read(f.size()), '\n');
            _old_paths.insert(_old_paths.end(), v.begin(), v.end());
        }
    }

    _day = global().log_time->day();
    return true;
}

fs::file& LogFile::open(int level) {
    static bool _chk = this->check_config(level == fatal);
    auto& g = global();
    auto& s = *g.s;

    if (!fs::exists(FLG_log_dir)) fs::mkdir(FLG_log_dir, true);
    _path.clear();

    if (level < xx::fatal) {
        _path.append(_path_base).append(".log");

        bool new_file = !fs::exists(_path);
        if (!new_file) {
            if (!_old_paths.empty()) {
                auto& path = _old_paths.back();
                if (fs::fsize(_path) >= FLG_max_log_file_size || this->day_in_path(path) != _day) {
                    fs::rename(_path, path); // rename xx.log to xx_0808_15_30_08.123.log
                    if (FLG_log_compress) this->compress_file(path);
                    new_file = true;
                }
            } else {
                new_file = true;
            }
        }
      
        if (_file.open(_path.c_str(), 'a') && new_file) {
            char t[24] = { 0 }; // 0723 17:00:00.123
            memcpy(t, g.log_time->get(), LogTime::t_len);
            for (int i = 0; i < LogTime::t_len; ++i) {
                if (t[i] == ' ' || t[i] == ':') t[i] = '_';
            }

            s.clear();
            s.append(_path_base).append('_').append(t).append(".log");
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

void LogFile::write(const char* p, size_t n) {
    if (FLG_log_daily) {
        const uint32 day = global().log_time->day();
        if (_day != day) { _day = day; _file.close(); }
    }

    if (_file || this->open()) {
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
    // not work for other threads. Use AddVectoredExceptixx::onHandler instead.
    _ex_handler = AddVectoredExceptionHandler(1, xx::on_exception);
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
  #ifdef _WIN32
    if (_ex_handler) {
        RemoveVectoredExceptionHandler(_ex_handler);
        _ex_handler = NULL;
    }
  #else
    os::signal(SIGSEGV, SIG_DFL);
    os::signal(SIGFPE, SIG_DFL);
    os::signal(SIGBUS, SIG_DFL);
    os::signal(SIGILL, SIG_DFL);
  #endif
}

void FailureHandler::on_signal(int sig) {
    global().level_logger->stop(true);
    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}

void FailureHandler::on_failure(int sig) {
    auto& g = global();
    g.level_logger->stop(true);

    auto& f = g.log_file->open(fatal);
    auto& s = *g.s; s.clear();
    if (!g.check_failed) {
        if (FLG_syslog) s << "<10>";
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
      default:
        // ignore unrecognized exception here
        return EXCEPTION_CONTINUE_SEARCH;
    }

    g.level_logger->stop();
    auto& f = g.log_file->open(fatal);
    auto& s = *g.s; s.clear();
    if (FLG_syslog) s << "<10>";
    s << 'F' << g.log_time->get() << "] " << err << '\n';
    if (f) f.write(s);
    log2stderr(s.data(), s.size());

    if (_stack_trace) _stack_trace->dump_stack(&f, 6);
    if (f) { f.write('\n'); f.close(); }

    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI on_exception(PEXCEPTION_POINTERS p) {
    global().failure_handler->on_exception(p);
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

LevelLogSaver::LevelLogSaver(const char* file, int len, unsigned int line, int level)
    : _s(log_stream()) {
    _n = _s.size();
    if (!FLG_syslog) {
        _s.resize(_n + (LogTime::t_len + 1)); // make room for: "I0523 17:00:00.123"
        _s[_n] = "DIWE"[level];
    } else {
        _s.resize(_n + (LogTime::t_len + 5)); // make room for: "<1x>I0523 17:00:00.123"
        static const char* kHeader[] = { "<15>D", "<14>I", "<12>W", "<11>E" };
        memcpy((char*)_s.data() + _n, kHeader[level], 5);
    }
    (_s << ' ' << co::thread_id() << ' ').append(file, len) << ':' << line << ']' << ' ';
}

LevelLogSaver::~LevelLogSaver() {
    _s << '\n';
    global().level_logger->push((char*)_s.data() + _n, _s.size() - _n);
    _s.resize(_n);
}

FatalLogSaver::FatalLogSaver(const char* file, int len, unsigned int line)
    : _s(log_stream()) {
    if (!FLG_syslog) {
        _s.resize(LogTime::t_len + 1);
        _s.front() = 'F';
    } else {
        _s.resize(LogTime::t_len + 5);
        memcpy((char*)_s.data(), "<10>F", 5);
    }
    (_s << ' ' << co::thread_id() << ' ').append(file, len) << ':' << line << ']' << ' ';
}

FatalLogSaver::~FatalLogSaver() {
    _s << '\n';
    global().level_logger->push_fatal_log((char*)_s.data(), _s.size());
}

} // namespace xx

void exit() {
    xx::global().level_logger->stop();
}

void set_write_cb(const std::function<void(const void*, size_t)>& cb, int flags) {
    xx::global().level_logger->set_write_cb(cb, flags);
}

} // namespace log
} // namespace ___
