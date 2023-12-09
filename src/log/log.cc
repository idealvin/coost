#include "co/log.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/mem.h"
#include "co/str.h"
#include "co/time.h"
#include "co/co.h"
#include "../co/hook.h"

#ifdef _WIN32
#include "StackWalker.hpp"
#else
#include <unistd.h>
#include <sys/select.h>
#ifdef HAS_BACKTRACE_H
#include <backtrace.h>
#include <cxxabi.h>
#endif
#endif
#include <time.h>

#ifdef _MSC_VER
#pragma warning (disable:4722)
#endif

static fastring* g_log_dir;
static fastring* g_log_file_name;
static void _at_mod_init() {
    DEF_string(log_dir, "logs", ">>#0 log dir, will be created if not exists");
    DEF_string(log_file_name, "", ">>#0 name of log file, use exename if empty");
    g_log_dir = &FLG_log_dir;
    g_log_file_name = &FLG_log_file_name;
}
DEF_int32(min_log_level, 0, ">>#0 write logs at or above this level, 0-4 (debug|info|warning|error|fatal)");
DEF_int32(max_log_size, 4096, ">>#0 max size of a single log");
DEF_int64(max_log_file_size, 256 << 20, ">>#0 max size of log file, default: 256MB");
DEF_uint32(max_log_file_num, 8, ">>#0 max number of log files");
DEF_uint32(max_log_buffer_size, 32 << 20, ">>#0 max size of log buffer, default: 32MB");
DEF_uint32(log_flush_ms, 128, ">>#0 flush the log buffer every n ms");
DEF_bool(cout, false, ">>#0 also logging to terminal");
DEF_bool(log_daily, false, ">>#0 if true, enable daily log rotation");
DEF_bool(log_compress, false, ">>#0 if true, compress rotated log files with xz");

// When this value is true, the above flags should have been initialized, 
// and we are safe to start the logging thread.
static bool g_init_done;
static bool g_dummy = []() {
    atomic_store(&g_init_done, true, mo_release);
    return *co::_make_static<bool>(false);
}();

namespace _xx {
namespace log {
namespace xx {

class LogTime;
class LogFile;
class Logger;
class ExceptHandler;

struct Mod {
    Mod();
    ~Mod() = default;
    fastring* exename;
    fastring* stream;
    LogTime* log_time;
    LogFile* log_file;
    LogFile* log_fatal;
    Logger* logger;
    ExceptHandler* except_handler;
    bool check_failed;
};

static Mod* g_mod;
inline Mod& mod() { return *g_mod; }

inline void log2stderr(const char* s, size_t n) {
  #ifdef _WIN32
    auto r = ::fwrite(s, 1, n, stderr); (void)r;
  #else
    auto r = __sys_api(write)(STDERR_FILENO, s, n); (void)r;
  #endif
}

inline void log2stderr(const char* s) {
    log2stderr(s, strlen(s));
}

inline void signal_safe_sleep(int ms) {
  #ifdef _WIN32
    co::hook_sleep(false);
    ::Sleep(ms);
    co::hook_sleep(true);
  #else
    struct timeval tv = { 0, ms * 1000 };
    __sys_api(select)(0, 0, 0, 0, &tv);
  #endif
}

// time for logs: "0723 17:00:00.123"
class LogTime {
  public:
    enum {
        t_len = 17, // length of time 
        t_min = 8,  // position of minute
        t_sec = t_min + 3,
        t_ms = t_sec + 3,
    };

    LogTime() : _start(0) {
        for (int i = 0; i < 60; ++i) {
            const auto p = (uint8*) &_tb[i];
            p[0] = (uint8)('0' + i / 10);
            p[1] = (uint8)('0' + i % 10);
        }
        memset(_buf, 0, sizeof(_buf));
        this->update();
    }

    void update();
    const char* get() const { return _buf; }
    uint32 day() const { return *(uint32*)_buf; }
    int64 sec() const { return _start; }

  private:
    time_t _start;
    struct tm _tm;
    int16 _tb[64];
    char _buf[24]; // save the time string
};

void LogTime::update() {
    const int64 now_ms = epoch::ms();
    const time_t now_sec = now_ms / 1000;
    const int dt = (int) (now_sec - _start);
    if (dt == 0) goto set_ms;
    if (unlikely(dt < 0 || dt >= 60 || _start == 0)) goto reset;

    _tm.tm_sec += dt;
    if (_tm.tm_min < 59 || _tm.tm_sec < 60) {
        _start = now_sec;
        if (_tm.tm_sec >= 60) {
            _tm.tm_min++;
            _tm.tm_sec -= 60;
            *(uint16*)(_buf + t_min) = _tb[_tm.tm_min];
        }
        const auto p = (char*)(_tb + _tm.tm_sec);
        _buf[t_sec] = p[0];
        _buf[t_sec + 1] = p[1];
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
        const auto p = _buf + t_ms;
        uint32 ms = (uint32)(now_ms - _start * 1000);
        uint32 x = ms / 100;
        p[0] = (char)('0' + x);
        ms -= x * 100;
        x = ms / 10;
        p[1] = (char)('0' + x);
        p[2] = (char)('0' + (ms - x * 10));
    }
}

// the local file that logs will be written to
class LogFile {
  public:
    LogFile()
        : _file(256), _path(256), _path_base(256), _day(0), _checked(false) {
    }

    fs::file& open(const char* topic, int level);
    void write(const char* p, size_t n);
    void write(const char* topic, const char* p, size_t n);
    void close() { _file.close(); }

  private:
    bool check_config(const char* topic, int level);

  private:
    fs::file _file;
    fastring _path;
    fastring _path_base; // prefix of the log path: log_dir/log_file_name 
    co::deque<fastring> _old_paths; // paths of old log files
    uint32 _day;
    bool _checked;
};

bool LogFile::check_config(const char* topic, int level) {
    auto& m = mod();
    auto& s = *m.stream;

    // log dir, backslash to slash
    auto& d = *g_log_dir; // FLG_log_dir;
    for (size_t i = 0; i < d.size(); ++i) if (d[i] == '\\') d[i] = '/';

    // log file name, use process name by default
    auto& f = *g_log_file_name; // FLG_log_file_name;
    s.clear();
    if (f.empty()) {
        s.append(*m.exename);
    } else {
        s.append(f);
        s.remove_suffix(".log");
    }
    s.remove_suffix(".exe");

    // prefix of log file path
    _path_base << d;
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

    _day = m.log_time->day();
    return true;
}

// get day from xx_0808_15_30_08.123.log
inline uint32 get_day_from_path(const fastring& path) {
    uint32 x = 0;
    const int n = LogTime::t_len + 4;
    if (path.size() > n) god::copy<4>(&x, path.data() + path.size() - n);
    return x;
}

// compress log file with the xz command
inline void compress_file(const fastring& path) {
    fastring cmd(path.size() + 8);
    cmd.append("xz ").append(path);
    os::system(cmd);
}

fs::file& LogFile::open(const char* topic, int level) {
    if (!_checked) {
        this->check_config(topic, level);
        _checked = true;
    }

    auto& m = mod();
    auto& s = *m.stream; s.clear();
    auto& d = *g_log_dir; // FLG_log_dir;

    // create log dir if not exists
    if (!topic) {
        if (!fs::exists(d)) fs::mkdir((char*)d.c_str(), true);
    } else {
        s << d;
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
                    FLG_log_daily && get_day_from_path(path) != _day)) {
                    fs::mv(_path, path); // rename xx.log to xx_0808_15_30_08.123.log
                    if (FLG_log_compress) compress_file(path);
                    new_file = true;
                }
            } else {
                new_file = true;
            }
        }
      
        if (_file.open(_path.c_str(), 'a') && new_file) {
            char x[24] = { 0 }; // 0723 17:00:00.123
            memcpy(x, m.log_time->get(), LogTime::t_len);
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

void LogFile::write(const char* p, size_t n) {
    auto& m = mod();
    if (FLG_log_daily) {
        const uint32 day = m.log_time->day();
        if (_day != day) { _day = day; _file.close(); }
    }

    if (_file || this->open(NULL, 0)) {
        _file.write(p, n);
        const uint64 size = _file.size(); // -1 if not exists
        if (size >= (uint64)FLG_max_log_file_size) _file.close();
    }
}

void LogFile::write(const char* topic, const char* p, size_t n) {
    auto& m = mod();
    if (FLG_log_daily) {
        const uint32 day = m.log_time->day();
        if (_day != day) { _day = day; _file.close(); }
    }
    if (_file || this->open(topic, 0)) {
        _file.write(p, n);
        const uint64 size = _file.size(); // -1 if not exists
        if (size >= (uint64)FLG_max_log_file_size) _file.close();
    }
}

class Logger {
  public:
    static const uint32 N = 128 * 1024;
    static const int A = 8; // array size

    Logger(LogTime* t, LogFile* f);
    ~Logger() { this->stop(); }

    bool start();
    void stop(bool signal_safe=false);

    void push_level_log(char* s, size_t n);
    void push_topic_log(const char* topic, char* s, size_t n);
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
    struct alignas(64) LevelLog {
        LevelLog() : x(), buf(), sec(0), bytes(0), write_cb(), write_flags(0) {}
        struct alignas(64) X {
            std::mutex m;
            fastream buf;      // logs will be pushed to this buffer
            char time_str[24]; // "0723 17:00:00.123"
        };
        X x;
        fastream buf; // to swap out logs
        int64 sec;
        size_t bytes;
        std::function<void(const void*, size_t)> write_cb;
        int write_flags;
    };

    struct PerTopic {
        PerTopic() : file(), sec(0), bytes(0) {}
        LogFile file;
        int64 sec;
        size_t bytes; // write bytes
    };

    typedef const char* Key;

    struct alignas(64) TopicLog {
        TopicLog() : write_flags(0) {}
        struct alignas(64) X {
            std::mutex m;
            co::hash_map<Key, fastream> buf;
            char time_str[24];
        };
        X x[A];
        co::hash_map<Key, fastream> buf[A];
        co::hash_map<Key, PerTopic> pts;
        std::function<void(const char*, const void*, size_t)> write_cb;
        int write_flags;
    };

    void write_level_logs(const char* p, size_t n);
    void write_topic_logs(LogFile& f, const char* topic, const char* p, size_t n);
    void thread_fun();

  private:
    LevelLog _llog;
    TopicLog _tlog;
    co::sync_event _log_event;
    LogTime& _time;
    LogFile& _file;
    int _stop; // -2: init, -1: starting, 0: running, 1: stopping, 2: stopped, 3: final
};

Logger::Logger(LogTime* t, LogFile* f)
    : _llog(), _tlog(), _log_event(true, false), _time(*t), _file(*f), _stop(-2) {
    memcpy(_llog.x.time_str, _time.get(), 24);
    _llog.sec = _time.sec();
    for (int i = 0; i < A; ++i) {
        memcpy(_tlog.x[i].time_str, _time.get(), 24);
    }
}

bool Logger::start() {
    if (atomic_bool_cas(&_stop, -2, -1)) {
        do {
            // ensure max_log_buffer_size >= 1M, max_log_size >= 256
            auto& bs = FLG_max_log_buffer_size;
            auto& ls = *(uint32*)&FLG_max_log_size;
            if (bs < (1 << 20)) bs = 1 << 20;
            if (ls < 256) ls = 256;
            if (ls > (bs >> 2)) ls = bs >> 2;
        } while (0);

        do {
            _time.update();
            memcpy(_llog.x.time_str, _time.get(), 24);
            _llog.sec = _time.sec();
            _llog.x.buf.reserve(N);
            _llog.buf.reserve(N);
            for (int i = 0; i < A; ++i) {
                memcpy(_tlog.x[i].time_str, _time.get(), 24);
            }
        } while (0);

        atomic_store(&_stop, 0);
        std::thread(&Logger::thread_fun, this).detach();
    } else {
        while (atomic_load(&_stop, mo_relaxed) < 0) {
            sleep::ms(1);
        }
    }
    return true;
}

// if signal_safe is true, try to call only async-signal-safe api in this function 
// according to:  http://man7.org/linux/man-pages/man7/signal-safety.7.html
void Logger::stop(bool signal_safe) {
    int s = atomic_cas(&_stop, 0, 1);
    if (s < 0) return; // thread not started
    if (s == 0) {
        if (!signal_safe) _log_event.signal();
      #if defined(_WIN32) && defined(BUILDING_CO_SHARED)
        // the thread may not respond in dll, wait at most 64ms here
        co::Timer t;
        while (_stop != 2 && t.ms() < 64) signal_safe_sleep(1);
      #else
        while (_stop != 2) signal_safe_sleep(1);
      #endif

        do {
            // it may be not safe if logs are still being pushed to the buffer 
            auto& x = _llog.x;
            !signal_safe ? x.m.lock() : signal_safe_sleep(1);
            if (!x.buf.empty()) {
                x.buf.swap(_llog.buf);
                this->write_level_logs(_llog.buf.data(), _llog.buf.size());
                _llog.buf.clear();
            }
            if (!signal_safe) x.m.unlock();
        } while (0);

        for (int i = 0; i < A; ++i) {
            auto& x = _tlog.x[i];
            auto& buf = _tlog.buf[i];
            if (!signal_safe) x.m.lock();
            if (!x.buf.empty()) x.buf.swap(buf);
            if (!signal_safe) x.m.unlock();

            for (auto it = buf.begin(); it != buf.end(); ++it) {
                auto& pt = _tlog.pts[it->first];
                auto& d = it->second;
                if (!d.empty()) {
                    this->write_topic_logs(pt.file, it->first, d.data(), d.size());
                }
                d.clear();
            }
        }

        atomic_swap(&_stop, 3);
    } else {
        while (_stop != 3) signal_safe_sleep(1);
    }
}

std::once_flag g_flag;
static bool g_thread_started;

void Logger::push_level_log(char* s, size_t n) {
    if (unlikely(!g_thread_started)) {
        std::call_once(g_flag, [this]() {
            this->start();
            atomic_store(&g_thread_started, true);
        });
    }
    if (unlikely(n > FLG_max_log_size)) {
        n = FLG_max_log_size;
        char* const p = s + n - 4;
        p[0] = '.';
        p[1] = '.';
        p[2] = '.';
        p[3] = '\n';
    }
    {
        std::lock_guard<std::mutex> g(_llog.x.m);
        if (!_stop) {
            memcpy(s + 1, _llog.x.time_str, LogTime::t_len); // log time

            auto& buf = _llog.x.buf;
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

void Logger::push_topic_log(const char* topic, char* s, size_t n) {
    if (unlikely(!g_thread_started)) {
        std::call_once(g_flag, [this]() {
            this->start();
            atomic_store(&g_thread_started, true);
        });
    }
    if (unlikely(n > FLG_max_log_size)) {
        n = FLG_max_log_size;
        char* const p = s + n - 4;
        p[0] = '.';
        p[1] = '.';
        p[2] = '.';
        p[3] = '\n';
    }

    auto& x = _tlog.x[murmur_hash(topic, strlen(topic)) & (A - 1)];
    {
        std::lock_guard<std::mutex> g(x.m);
        if (!_stop) {
            memcpy(s, x.time_str, LogTime::t_len);

            auto& buf = x.buf[topic];
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
    memcpy(s + 1, _time.get(), LogTime::t_len);

    this->write_level_logs(s, n);
    if (!FLG_cout) fwrite(s, 1, n, stderr);
    if (_file.open(NULL, fatal)) _file.write(s, n);

    atomic_store(&mod().check_failed, true);
    abort();
}

void Logger::write_level_logs(const char* p, size_t n) {
    if (!_llog.write_cb || (_llog.write_flags & log::log2local)) {
        _file.write(p, n);
    }
    if (_llog.write_cb) _llog.write_cb(p, n);
    if (FLG_cout) fwrite(p, 1, n, stderr);
}

void Logger::write_topic_logs(LogFile& f, const char* topic, const char* p, size_t n) {
    if (!_tlog.write_cb || (_tlog.write_flags & log::log2local)) {
        f.write(topic, p, n);
    }
    if (_tlog.write_cb) _tlog.write_cb(topic, p, n);
    if (FLG_cout) fwrite(p, 1, n, stderr);
}

void Logger::thread_fun() {
    bool signaled;
    int64 sec;
    while (atomic_load(&g_init_done, mo_acquire) != true) _log_event.wait(8);
    while (!_stop) {
        signaled = _log_event.wait(FLG_log_flush_ms);
        if (_stop) break;

        // level logs
        do {
            _time.update();
            {
                auto& x = _llog.x;
                std::lock_guard<std::mutex> g(x.m);
                memcpy(x.time_str, _time.get(), LogTime::t_len);
                if (!x.buf.empty()) x.buf.swap(_llog.buf);
            }

            if (!_llog.buf.empty()) {
                this->write_level_logs(_llog.buf.data(), _llog.buf.size());
            }
            if (_llog.bytes < _llog.buf.size()) {
                _llog.bytes = _llog.buf.size();
            }

            _llog.buf.clear();
            _llog.buf.reserve(N);
            sec = _time.sec();
            if (_llog.sec + 60 <= sec) {
                _llog.sec = sec;
                const auto cap = _llog.buf.capacity();
                if (cap > N && _llog.bytes < (cap >> 1)) {
                    _llog.buf.reset();
                    _llog.buf.reserve(god::align_up<N>(_llog.bytes));
                }
                _llog.bytes = 0;
            }
        } while (0);

        // topic logs
        for (int i = 0; i < A; ++i) {
            _time.update();
            auto& x = _tlog.x[i];
            auto& buf = _tlog.buf[i];
            {
                std::lock_guard<std::mutex> g(x.m);
                memcpy(x.time_str, _time.get(), LogTime::t_len);
                if (!x.buf.empty()) x.buf.swap(buf);
            }

            for (auto it = buf.begin(); it != buf.end(); ++it) {
                auto& pt = _tlog.pts[it->first];
                auto& s = it->second;
                if (!s.empty()) {
                    this->write_topic_logs(pt.file, it->first, s.data(), s.size());
                }
                if (pt.bytes < s.size()) pt.bytes = s.size();

                s.clear();
                s.reserve(N >> 1);
                sec = _time.sec();
                if (pt.sec == 0) pt.sec = sec;
                if (pt.sec + 60 <= sec) {
                    pt.sec = sec;
                    const auto cap = s.capacity();
                    if (cap > N && pt.bytes < (cap >> 1)) {
                        s.reset();
                        s.reserve(god::align_up<N>(pt.bytes));
                    }
                    pt.bytes = 0;
                }
            }
        }

        if (signaled) _log_event.reset();
    }

    atomic_swap(&_stop, 2);
}

#ifdef _WIN32
class StackTrace : public StackWalker {
  public:
    typedef void (*write_cb_t)(const char*, size_t);
    static const int kOptions =
        StackWalker::SymUseSymSrv |
        StackWalker::RetrieveSymbol |
        StackWalker::RetrieveLine |
        StackWalker::RetrieveModuleInfo;

    StackTrace() : StackWalker(kOptions), _f(0), _skip(0) {}

    virtual ~StackTrace() = default;

    void dump_stack(void* f, int skip) {
        _f = (write_cb_t) f;
        _skip = skip;
        this->ShowCallstack(GetCurrentThread());
    }

  private:
    virtual void OnOutput(LPCSTR s) {
        if (_skip > 0) { --_skip; return; }
        const size_t n = strlen(s);
        if (_f) _f(s, n);
        auto r = ::fwrite(s, 1, n, stderr); (void)r;
    }

    virtual void OnSymInit(LPCSTR, DWORD, LPCSTR) {}
    virtual void OnLoadModule(LPCSTR, LPCSTR, DWORD64, DWORD, DWORD, LPCSTR, LPCSTR, ULONGLONG) {}
    virtual void OnDbgHelpErr(LPCSTR, DWORD, DWORD64) {}

  private:
    write_cb_t _f;
    int _skip;
};

#else 

class StackTrace{
  public:
    typedef void (*write_cb_t)(const char*, size_t);
    StackTrace()
        : _f(0), _buf((char*)::malloc(4096)), _size(4096), _s(4096), _exe(os::exepath()) {
        memset(_buf, 0, 4096);
        memset((char*)_s.data(), 0, _s.capacity());
        (void) _exe.c_str();
    }

    ~StackTrace() {
        if (_buf) { ::free(_buf); _buf = NULL; }
    }

    void dump_stack(void* f, int skip);
    char* demangle(const char* name);
    int backtrace(const char* file, int line, const char* func, int& count);

  private:
    write_cb_t _f;
    char* _buf;    // for demangle
    size_t _size;  // buf size
    fastream _s;   // for stack trace
    fastring _exe; // exe path
};

#ifdef HAS_BACKTRACE_H
// https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html
char* StackTrace::demangle(const char* name) {
    int status = 0;
    size_t n = _size;
    char* p = abi::__cxa_demangle(name, _buf, &n, &status);
    if (_size < n) { /* buffer reallocated */
        _buf = p;
        _size = n;
    }
    return p;
}

struct user_data_t {
    StackTrace* st;
    int count;
};

void error_cb(void* data, const char* msg, int errnum) {
    log2stderr(msg);
    log2stderr("\n", 1);
}

int backtrace_cb(void* data, uintptr_t /*pc*/, const char* file, int line, const char* func) {
    user_data_t* ud = (user_data_t*) data;
    return ud->st->backtrace(file, line, func, ud->count);
}

void StackTrace::dump_stack(void* f, int skip) {
    _f = (write_cb_t) f;
    struct user_data_t ud = { this, 0 };
    struct backtrace_state* state = backtrace_create_state(_exe.c_str(), 1, error_cb, NULL);
    backtrace_full(state, skip, backtrace_cb, error_cb, (void*)&ud);
}

int StackTrace::backtrace(const char* file, int line, const char* func, int& count) {
    if (!file && !func) return 0;
    if (func) {
        char* p = this->demangle(func);
        if (p) func = p;
    }

    _s.clear();
    _s << '#' << (count++) << "  in " << (func ? func : "???") << " at " 
       << (file ? file : "???") << ':' << line << '\n';

    if (_f) _f(_s.data(), _s.size());
    log2stderr(_s.data(), _s.size());
    return 0;
}

#else
char* StackTrace::demangle(const char*) { return 0; }
void StackTrace::dump_stack(void*, int) {}
#endif // HAS_BACKTRACE_H

#endif // _WIN32

class ExceptHandler {
  public:
    ExceptHandler();
    ~ExceptHandler();

    void handle_signal(int sig);
    int handle_exception(void* e); // for windows only

    static void write_fatal_message(const char* p, size_t n) {
        mod().log_file->write(p, n);
        mod().log_fatal->write(p, n);
    }

  private:
    StackTrace* _stack_trace;
    co::map<int, os::sig_handler_t> _old_handlers;
};

void on_signal(int sig) {
    mod().except_handler->handle_signal(sig);
}

#ifdef _WIN32
LONG WINAPI on_except(PEXCEPTION_POINTERS p) {
    return mod().except_handler->handle_exception((void*)p);
}
#endif

ExceptHandler::ExceptHandler() {
    _stack_trace = co::_make_static<StackTrace>();
    _old_handlers[SIGINT] = os::signal(SIGINT, on_signal);
    _old_handlers[SIGTERM] = os::signal(SIGTERM, on_signal);
  #ifdef _WIN32
    _old_handlers[SIGABRT] = os::signal(SIGABRT, on_signal);
    // Signal handler for SIGSEGV and SIGFPE installed in main thread does 
    // not work for other threads. Use SetUnhandledExceptionFilter instead.
    SetUnhandledExceptionFilter(on_except);
  #else
    const int x = SA_RESTART; // | SA_ONSTACK;
    _old_handlers[SIGQUIT] = os::signal(SIGQUIT, on_signal);
    _old_handlers[SIGABRT] = os::signal(SIGABRT, on_signal, x);
    _old_handlers[SIGSEGV] = os::signal(SIGSEGV, on_signal, x);
    _old_handlers[SIGFPE] = os::signal(SIGFPE, on_signal, x);
    _old_handlers[SIGBUS] = os::signal(SIGBUS, on_signal, x);
    _old_handlers[SIGILL] = os::signal(SIGILL, on_signal, x);
    os::signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE
  #endif
}

ExceptHandler::~ExceptHandler() {
    os::signal(SIGINT, SIG_DFL);
    os::signal(SIGTERM, SIG_DFL);
    os::signal(SIGABRT, SIG_DFL);
  #ifndef _WIN32
    os::signal(SIGQUIT, SIG_DFL);
    os::signal(SIGSEGV, SIG_DFL);
    os::signal(SIGFPE, SIG_DFL);
    os::signal(SIGBUS, SIG_DFL);
    os::signal(SIGILL, SIG_DFL);
  #endif
}

#if defined(_WIN32) && !defined(SIGQUIT)
#define SIGQUIT SIGTERM
#endif

void ExceptHandler::handle_signal(int sig) {
    auto& m = mod();
    if (sig == SIGINT || sig == SIGTERM || sig == SIGQUIT) {
        m.logger->stop(true);
        os::signal(sig, _old_handlers[sig]);
        raise(sig);
        return;
    }

    m.logger->stop(true);
    m.log_file->open(NULL, 0);
    m.log_fatal->open(NULL, fatal);
    auto f = &ExceptHandler::write_fatal_message;
    auto& s = *m.stream; s.clear();
    if (!m.check_failed) {
        s << 'F' << m.log_time->get() << "] ";
    }

    switch (sig) {
      case SIGABRT:
        if (!m.check_failed) s << "SIGABRT: aborted\n";
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
        f(s.data(), s.size());
        log2stderr(s.data(), s.size());
    }

    if (_stack_trace) _stack_trace->dump_stack(
        (void*)f, m.check_failed ? 7 : (sig == SIGABRT ? 4 : 3)
    );

    os::signal(sig, _old_handlers[sig]);
    raise(sig);
}

#ifdef _WIN32
int ExceptHandler::handle_exception(void* e) {
    auto& m = mod();
    const char* err = NULL;
    auto p = (PEXCEPTION_POINTERS)e;

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
        err = "Error: STATUS_APPLICATION_HANG";
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

    m.logger->stop();
    m.log_file->open(NULL, 0);
    m.log_fatal->open(NULL, fatal);
    auto f = &ExceptHandler::write_fatal_message;
    auto& s = *m.stream; s.clear();
    s << 'F' << m.log_time->get() << "] " << err;
    if (err[0] == 'U') s << (void*)(size_t)p->ExceptionRecord->ExceptionCode;
    s << '\n';
    f(s.data(), s.size());
    log2stderr(s.data(), s.size());
    if (_stack_trace) _stack_trace->dump_stack((void*)f, 6);

    ::exit(0);
    return EXCEPTION_EXECUTE_HANDLER;
}

#else
int ExceptHandler::handle_exception(void*) { return 0; }
#endif // _WIN32

Mod::Mod() {
    _at_mod_init();
    exename = co::_make_static<fastring>(os::exename());
    stream = co::_make_static<fastring>(4096);
    log_time = co::_make_static<LogTime>();
    log_file = co::_make_static<LogFile>();
    log_fatal = co::_make_static<LogFile>();
    logger = co::_make_static<Logger>(log_time, log_file);
    except_handler = co::_make_static<ExceptHandler>();
    check_failed = false;
}

static int g_nifty_counter;
Initializer::Initializer() {
    if (g_nifty_counter++ == 0) {
        g_mod = co::_make_static<Mod>();
    }
}

static __thread fastream* g_s;

inline fastream& log_stream() {
    return g_s ? *g_s : *(g_s = co::_make_static<fastream>(256));
}

LevelLogSaver::LevelLogSaver(const char* fname, unsigned fnlen, unsigned line, int level)
    : _s(log_stream()) {
    _n = _s.size();
    _s.resize(_n + (LogTime::t_len + 1)); // make room for: "I0523 17:00:00.123"
    _s[_n] = "DIWE"[level];
    (_s << ' ' << co::thread_id() << ' ').append(fname, fnlen) << ':' << line << "] ";
}

LevelLogSaver::~LevelLogSaver() {
    _s << '\n';
    mod().logger->push_level_log((char*)_s.data() + _n, _s.size() - _n);
    _s.resize(_n);
}

FatalLogSaver::FatalLogSaver(const char* fname, unsigned fnlen, unsigned line)
    : _s(log_stream()) {
    _s.resize(LogTime::t_len + 1);
    _s.front() = 'F';
    (_s << ' ' << co::thread_id() << ' ').append(fname, fnlen) << ':' << line << "] ";
}

FatalLogSaver::~FatalLogSaver() {
    _s << '\n';
    mod().logger->push_fatal_log((char*)_s.data(), _s.size());
}

TLogSaver::TLogSaver(const char* fname, unsigned fnlen, unsigned line, const char* topic)
    : _s(log_stream()), _topic(topic) {
    _n = _s.size();
    _s.resize(_n + (LogTime::t_len)); // make room for: "0523 17:00:00.123"
    (_s << ' ' << co::thread_id() << ' ').append(fname, fnlen) << ':' << line << "] ";
}

TLogSaver::~TLogSaver() {
    _s << '\n';
    mod().logger->push_topic_log(_topic, (char*)_s.data() + _n, _s.size() - _n);
    _s.resize(_n);
}

} // xx

void exit() {
    xx::mod().logger->stop();
}

void set_write_cb(const std::function<void(const void*, size_t)>& cb, int flags) {
    xx::mod().logger->set_write_cb(cb, flags);
}

void set_write_cb(const std::function<void(const char*, const void*, size_t)>& cb, int flags) {
    xx::mod().logger->set_write_cb(cb, flags);
}

} // log
} // _xx

#ifdef _WIN32
LONG WINAPI _co_on_exception(PEXCEPTION_POINTERS p) {
    return _xx::log::xx::on_except(p);
}
#endif
