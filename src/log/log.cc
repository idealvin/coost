#include "co/log.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/str.h"
#include "co/path.h"
#include "co/time.h"
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

#ifndef _WIN32
#include <unistd.h>
#include <sys/select.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable:4722)
#endif

DEF_string(log_dir, "logs", "#0 log dir, will be created if not exists");
DEF_string(log_file_name, "", "#0 name of log file, using exename if empty");
DEF_int32(min_log_level, 0, "#0 write logs at or above this level, 0-4 (debug|info|warning|error|fatal)");
DEF_int32(max_log_size, 4096, "#0 max size of a single log");
DEF_int64(max_log_file_size, 256 << 20, "#0 max size of log file, default: 256MB");
DEF_uint32(max_log_file_num, 8, "#0 max number of log files");
DEF_uint32(max_log_buffer_size, 32 << 20, "#0 max size of log buffer, default: 32MB");
DEF_uint32(log_flush_ms, 128, "#0 flush the log buffer every n ms");
DEF_bool(cout, false, "#0 also logging to terminal");
DEF_bool(syslog, false, "#0 add syslog header to each log if true");
DEF_bool(also_log_to_local, false, "#0 if true, also log to local file when write-cb is set");
DEF_bool(log_daily, false, "#0 if true, enable daily log rotation");
DEF_bool(log_compress, false, "#0 if true, compress rotated log files with xz");

namespace ___ {
namespace log {
namespace xx {

// handler for SIGINT SIGTERM SIGQUIT
void on_signal(int sig);

// handler for SIGSEGV SIGABRT SIGFPE SIGBUS SIGILL
void on_failure(int sig);

#ifdef _WIN32
// handler for win32 exceptions
LONG WINAPI on_exception(PEXCEPTION_POINTERS p);
#endif

class LogTime {
  public:
    // start byte of minute, second, ms in _buf: "0723 17:00:00.123"
    enum {
        b_min = 8,
        b_sec = b_min + 3,
        b_ms  = b_sec + 3,
        b_max = b_ms + 3
    };

    LogTime() : _start(0) {
        memset(_buf, 0, sizeof(_buf));
        this->update();
    }

    ~LogTime() = default;

    void update();
    const char* get() const { return _buf; }
    const int size() const { return b_max; }

  private:
    time_t _start;
    struct tm _tm;
    char _buf[24]; // 0723 17:00:00.123
};

class LevelLogger {
  public:
    LevelLogger();
    ~LevelLogger();

    struct Config {
        Config() : max_log_buffer_size(32 << 20), max_log_size(4096) {}
        fastring log_dir;
        fastring log_path_base;
        uint32 max_log_buffer_size;
        int32 max_log_size;
    };

    void init();

    void stop(bool signal_safe=false);

    void push(char* s, size_t n);

    void push_fatal_log(fastream* log);

    void on_signal(int sig);
    void on_failure(int sig);

  #ifdef _WIN32
    int on_exception(PEXCEPTION_POINTERS p); // for windows only
  #endif

    void set_write_cb(const std::function<void(const void*, size_t)>& cb);

    void set_single_write_cb(const std::function<void(const void*, size_t)>& cb);

    const std::function<void(const void*, size_t)>& single_write_cb() const {
        return _single_write_cb;
    }

  private:
    bool open_log_file(int level=0);
    void write(fastream* fs);
    void thread_fun();
    void compress_rotated_log(const fastring& path);

  private:
    Mutex _log_mutex;
    SyncEvent _log_event;
    std::unique_ptr<Thread> _log_thread;
    std::unique_ptr<fastream> _fs;
    std::unique_ptr<Config> _config;
    fs::file _file;
    fastring _path; // path of log file
    fastring _stmp;
    std::deque<fastring> _old_paths;
    std::function<void(const void*, size_t)> _write_cb;
    std::function<void(const void*, size_t)> _single_write_cb;

    LogTime _log_time;
    char _t[24];
    uint32 _day;
    int _stop;
    bool _check_failed;
    std::unique_ptr<co::StackTrace> _stack_trace;
    std::map<int, os::sig_handler_t> _old_handlers;
    void* _ex_handler; // exception handler for windows
};

LevelLogger::LevelLogger()
    : _log_event(true, false), _fs(new fastream(1 << 20)), _path(256),
      _stop(0), _check_failed(false), _ex_handler(0) {
    _config.reset(new Config);
    _log_time.update();
    memcpy(_t, _log_time.get(), 24);
    _day = *(uint32*)_t;

    _stack_trace.reset(co::new_stack_trace());
    _old_handlers[SIGINT] = os::signal(SIGINT, xx::on_signal);
    _old_handlers[SIGTERM] = os::signal(SIGTERM, xx::on_signal);

  #ifdef _WIN32
    _old_handlers[SIGABRT] = os::signal(SIGABRT, xx::on_failure);
    // Signal handler for SIGSEGV and SIGFPE installed in main thread does 
    // not work for other threads. Use AddVectoredExceptionHandler instead.
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

LevelLogger::~LevelLogger() {
    this->stop();
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

void LevelLogger::compress_rotated_log(const fastring& path) {
    fastring cmd(path.size() + 8);
    cmd.append("xz ").append(path);
    os::system(cmd);
}

void LevelLogger::init() {
    fastring s = FLG_log_file_name;
    s.remove_tail(".log");
    if (s.empty()) s = os::exename();
    s.remove_tail(".exe");

    // log dir, create if not exists.
    _config->log_dir = path::clean(str::replace(FLG_log_dir, "\\", "/"));
    if (!fs::exists(_config->log_dir)) fs::mkdir(_config->log_dir, true);

    // prefix of log file path
    _config->log_path_base = path::join(_config->log_dir, s);

    // read paths of old log files
    s.clear();
    s.append(_config->log_path_base).append("_old.log");
    fs::file f(s.c_str(), 'r');
    if (f) {
        auto v = str::split(f.read(f.size()), '\n');
        _old_paths.insert(_old_paths.end(), v.begin(), v.end());
    }

    _path.reserve(_config->log_path_base.size() + 32);
    _stmp.reserve((_config->log_path_base.size() + 32) * FLG_max_log_file_num);

    auto& nbuf = _config->max_log_buffer_size;
    nbuf = FLG_max_log_buffer_size;
    if (nbuf < (1 << 20)) nbuf = (1 << 20);

    auto& nlog = _config->max_log_size;
    nlog = FLG_max_log_size;
    if (nlog < 128) nlog = 128;
    if ((uint32)nlog >= (nbuf >> 1)) nlog = (int32)((nbuf >> 1) - 1);
  
    // start the logging thread
    _log_thread.reset(new Thread(&LevelLogger::thread_fun, this));
}

void LevelLogger::set_write_cb(const std::function<void(const void*, size_t)>& cb) {
    _write_cb = cb;
}

void LevelLogger::set_single_write_cb(const std::function<void(const void*, size_t)>& cb) {
    _single_write_cb = cb;
    _write_cb = [this](const void* p, size_t n) {
        const char* b = (const char*)p;
        const char* e = b + n;
        const char* s;
        while (b < e) {
            s = (const char*) memchr(b, '\n', e - b);
            if (s) {
                this->single_write_cb()(b, s - b + 1);
                b = s + 1;
            } else {
                this->single_write_cb()(b, e - b);
                break;
            }
        }
    };
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

            MutexGuard g(_log_mutex);
            if (!_fs->empty()) {
                this->write(_fs.get());
                _fs->clear();
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
            while (_stop != 2) signal_safe_sleep(1);

            if (!_fs->empty()) {
                this->write(_fs.get());
                _fs->clear();
            }

            atomic_swap(&_stop, 3);
        } else {
            while (_stop != 3) signal_safe_sleep(1);
        }
    }
}

void LevelLogger::thread_fun() {
    std::unique_ptr<fastream> fs(new fastream(1024*1024));

    while (!_stop) {
        bool signaled = _log_event.wait(FLG_log_flush_ms);
        if (_stop) break;

        _log_time.update();
        {
            MutexGuard g(_log_mutex);
            memcpy(_t, _log_time.get(), _log_time.size());
            if (!_fs->empty()) _fs.swap(fs);
            if (signaled) _log_event.reset();
        }

        if (!fs->empty()) {
            this->write(fs.get());
            fs->clear();
        }
    }

    atomic_swap(&_stop, 2);
}

void LevelLogger::write(fastream* fs) {
    if (!_write_cb || FLG_also_log_to_local) {
        fs::file& f = _file;
        if (f || this->open_log_file()) {
            f.write(fs->data(), fs->size());
        }

        if (f) {
            if (!f.exists() || f.size() >= FLG_max_log_file_size) {
                f.close();
            } else if (FLG_log_daily) {
                uint32 day = *(uint32*)_t;
                if (_day != day) {
                    _day = day;
                    f.close();
                }
            }
        }
    }

    if (_write_cb) _write_cb(fs->data(), fs->size());
    if (FLG_cout) fwrite(fs->data(), 1, fs->size(), stderr);
}

void LevelLogger::push(char* s, size_t n) {
    if (unlikely(n > _config->max_log_size)) {
        n = _config->max_log_size;
        char* const p = s + n - 4;
        p[0] = '.';
        p[1] = '.';
        p[2] = '.';
        p[3] = '\n';
    }
    {
        MutexGuard g(_log_mutex);
        memcpy(*s != '<' ? s + 1 : s + 5, _t, _log_time.size()); // log time

        if (unlikely(_fs->size() + n >= _config->max_log_buffer_size)) {
            const char* p = strchr(_fs->data() + (_fs->size() >> 1) + 7, '\n');
            const size_t len = _fs->data() + _fs->size() - p - 1;
            memcpy((char*)(_fs->data()), "......\n", 7);
            memcpy((char*)(_fs->data()) + 7, p + 1, len);
            _fs->resize(len + 7);
        }

        _fs->append(s, n);
        if (_fs->size() > (_fs->capacity() >> 1)) _log_event.signal();
    }
}

void LevelLogger::push_fatal_log(fastream* log) {
    this->stop();

    char* const s = (char*) log->data();
    memcpy(*s != '<' ? s + 1 : s + 5, _t, _log_time.size());
    this->write(log);
    if (!FLG_cout) fwrite(log->data(), 1, log->size(), stderr);

    if (this->open_log_file(fatal)) {
        _file.write(log->data(), log->size());
    }

    atomic_swap(&_check_failed, true);
    abort();
}

inline void write_to_stderr(const char* s, size_t n) {
  #ifdef _WIN32
    auto r = ::fwrite(s, 1, n, stderr); (void)r;
  #else
    auto r = CO_RAW_API(write)(STDERR_FILENO, s, n); (void)r;
  #endif
}

bool LevelLogger::open_log_file(int level) {
    fastring path_base = _config->log_path_base;
    if (!fs::exists(_config->log_dir)) fs::mkdir(_config->log_dir, true);

    _path.clear();
    _stmp.clear();

    if (level < xx::fatal) {
        _path.append(path_base).append(".log");
        if (fs::exists(_path) && !_old_paths.empty()) {
            auto& path = _old_paths.back();
            fs::rename(_path, path); // rename xx.log to xx_0808_15_30_08.123.log
            if (FLG_compress) this->compress_rotated_log(path);
        }
      
        if (_file.open(_path.c_str(), 'a')) {
            char t[24] = { 0 }; // 0723 17:00:00.123
            memcpy(t, _log_time.get(), _log_time.size());
            for (int i = 0; i < _log_time.size(); ++i) {
                if (t[i] == ' ' || t[i] == ':') t[i] = '_';
            }

            _stmp.append(path_base).append('_').append(t).append(".log");
            _old_paths.push_back(_stmp);

            while (!_old_paths.empty() && _old_paths.size() > FLG_max_log_file_num) {
                if (FLG_compress) _old_paths.front().append(".xz");
                fs::remove(_old_paths.front());
                _old_paths.pop_front();
            }

            _stmp.resize(path_base.size());
            _stmp.append("_old.log");
            fs::file f(_stmp.c_str(), 'w');
            if (f) {
                _stmp.clear();
                for (auto& s : _old_paths) _stmp << s << '\n';
                f.write(_stmp);
            }
        }

    } else {
        _path.append(path_base).append(".fatal");
        _file.open(_path.c_str(), 'a');
    }

    if (!_file) {
        _stmp.clear();
        _stmp << "cann't open the file: " << _path << '\n';
        write_to_stderr(_stmp.data(), _stmp.size());
        return false;
    }

    return true;
}

void LevelLogger::on_signal(int sig) {
    this->stop(true);
    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}

void LevelLogger::on_failure(int sig) {
    this->stop(true);
    this->open_log_file(fatal);
    _stmp.clear();
    if (!_check_failed) {
        if (FLG_syslog) _stmp << "<10>";
        _stmp << 'F' << _t << "] ";
    }

    switch (sig) {
      case SIGABRT:
        if (!_check_failed) _stmp << "SIGABRT: aborted\n";
        break;
    #ifndef _WIN32
      case SIGSEGV:
        _stmp << "SIGSEGV: segmentation fault\n";
        break;
      case SIGFPE:
        _stmp << "SIGFPE: floating point exception\n";
        break;
      case SIGBUS:
        _stmp << "SIGBUS: bus error\n";
        break;
      case SIGILL:
        _stmp << "SIGILL: illegal instruction\n";
        break;
    #endif
      default:
        _stmp << "caught unexpected signal\n";
        break;
    }

    if (!_stmp.empty()) {
        if (_file) _file.write(_stmp.data(), _stmp.size());
        write_to_stderr(_stmp.data(), _stmp.size());
    }

    if (_stack_trace) _stack_trace->dump_stack(
        &_file, _check_failed ? 7 : (sig == SIGABRT ? 4 : 3)
    );
    if (_file) { _file.write('\n'); _file.close(); }

    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}

#ifdef _WIN32
int LevelLogger::on_exception(PEXCEPTION_POINTERS p) {
    _stmp.clear();
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

    this->stop();
    this->open_log_file(fatal);

    _stmp.clear();
    if (FLG_syslog) _stmp << "<10>";
    _stmp << 'F' << _t << "] " << err << '\n';
    if (_file) _file.write(_stmp.data(), _stmp.size());
    write_to_stderr(_stmp.data(), _stmp.size());

    if (_stack_trace) _stack_trace->dump_stack(&_file, 6);
    if (_file) { _file.write('\n'); _file.close(); }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

inline LevelLogger* level_logger() {
    static LevelLogger logger;
    return &logger;
}

void on_signal(int sig) {
    level_logger()->on_signal(sig);
}

void on_failure(int sig) {
    level_logger()->on_failure(sig);
}

#ifdef _WIN32
LONG WINAPI on_exception(PEXCEPTION_POINTERS p) {
    return level_logger()->on_exception(p);
}
#endif

inline fastream& log_stream() {
    static __thread fastream* kLogStream = 0;
    if (kLogStream) return *kLogStream;
    return *(kLogStream = new fastream(256));
}

LevelLogSaver::LevelLogSaver(const char* file, int len, unsigned int line, int level)
    : _s(log_stream()) {
    _n = _s.size();
    if (!FLG_syslog) {
        _s.resize(_n + 18); // make room for level and time: I0523 17:00:00.123
        _s[_n] = "DIWE"[level];
    } else {
        _s.resize(_n + 22); // make room for level and time: I0523 17:00:00.123
        static const char* kHeader[] = { "<15>D", "<14>I", "<12>W", "<11>E" };
        memcpy((char*)_s.data() + _n, kHeader[level], 5);
    }
    (_s << ' ' << co::thread_id() << ' ').append(file, len) << ':' << line << ']' << ' ';
}

LevelLogSaver::~LevelLogSaver() {
    _s << '\n';
    level_logger()->push((char*)_s.data() + _n, _s.size() - _n);
    _s.resize(_n);
}

FatalLogSaver::FatalLogSaver(const char* file, int len, unsigned int line)
    : _s(log_stream()) {
    if (!FLG_syslog) {
        _s.resize(18);
        _s.front() = 'F';
    } else {
        _s.resize(22);
        memcpy((char*)_s.data(), "<10>F", 5);
    }
    (_s << ' ' << co::thread_id() << ' ').append(file, len) << ':' << line << ']' << ' ';
}

FatalLogSaver::~FatalLogSaver() {
    _s << '\n';
    level_logger()->push_fatal_log(&_s);
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
            char* p = (char*) &tb[i];
            p[0] = (char)('0' + i / 10);
            p[1] = (char)('0' + i % 10);
        }
        return tb;
    }();

    _tm.tm_sec += dt;
    if (_tm.tm_min < 59 || _tm.tm_sec < 60) {
        _start = now_sec;
        if (_tm.tm_sec < 60) {
            char* p = (char*)(tb60 + _tm.tm_sec);
            _buf[b_sec] = p[0];
            _buf[b_sec + 1] = p[1];
        } else {
            _tm.tm_min++;
            _tm.tm_sec -= 60;
            *(uint16*)(_buf + b_min) = tb60[_tm.tm_min];
            char* p = (char*)(tb60 + _tm.tm_sec);
            _buf[b_sec] = p[0];
            _buf[b_sec + 1] = p[1];
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
        char* p = _buf + b_ms;
        uint32 x = ms / 100;
        p[0] = (char)('0' + x);
        ms -= x * 100;
        x = ms / 10;
        p[1] = (char)('0' + x);
        p[2] = (char)('0' + (ms - x * 10));
    }
}

} // namespace xx

static std::once_flag init_flag;

void init() {
    std::call_once(init_flag, [](){
      #ifndef _WIN32
        if (!CO_RAW_API(write)) { auto r = ::write(-1, 0, 0); (void)r; }
        if (!CO_RAW_API(select)) ::select(-1, 0, 0, 0, 0);
      #endif
        xx::level_logger()->init();
    });
}

void exit() {
    xx::level_logger()->stop();
}

void close() {
    log::exit();
}

void set_write_cb(const std::function<void(const void*, size_t)>& cb) {
    xx::level_logger()->set_write_cb(cb);
}

void set_single_write_cb(const std::function<void(const void*, size_t)>& cb) {
    xx::level_logger()->set_single_write_cb(cb);
}

} // namespace log
} // namespace ___
