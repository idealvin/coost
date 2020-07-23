#include "co/log.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/str.h"
#include "co/path.h"
#include "co/__/stack_trace.h"

#include <time.h>
#include <signal.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#ifndef _WIN32
#include <sys/select.h>
#include <sys/time.h>
#endif

DEF_string(log_dir, "logs", "#0 log dir, will be created if not exists");
DEF_string(log_file_name, "", "#0 name of log file, using exename if empty");
DEF_int32(min_log_level, 0, "#0 write logs at or above this level, 0-4 (debug|info|warning|error|fatal)");
DEF_int64(max_log_file_size, 256 << 20, "#0 max size of log file, default: 256MB");
DEF_uint32(max_log_file_num, 8, "#0 max number of log files");
DEF_uint32(max_log_buffer_size, 32 << 20, "#0 max size of log buffer, default: 32MB");
DEF_bool(cout, false, "#0 also logging to terminal");

namespace ___ {
namespace log {
namespace xx {

__thread fastream* xxLog = NULL;

// failure handler for SIGSEGV SIGABRT SIGFPE SIGBUS.
void on_failure();

// Signal handler for SIGINT SIGTERM SIGQUIT.
void on_signal(int sig);

inline void install_signal_handler() {
    signal(SIGINT, xx::on_signal);
    signal(SIGTERM, xx::on_signal);
  #ifndef _WIN32
    signal(SIGQUIT, xx::on_signal);
  #endif
}

class LogTime {
  public:
    LogTime();

    const char* update();

    const char* get() {
        return _buf;
    }

    void reset() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        _start = tv.tv_sec * 1000 + tv.tv_usec;
      #ifdef _WIN32
        _localtime64_s(&_tm, &tv.tv_sec);
      #else
        localtime_r(&tv.tv_sec, &_tm);
      #endif
        strftime(_buf, 16, "%m%d %H:%M:%S", &_tm);
        snprintf(_buf + 13, 8, ".%06d", (int) tv.tv_usec);
    }

  private:
    time_t _start;
    struct tm _tm;
    char _buf[21];
    uint16 _cache[60];
};

struct Config {
    Config() : log_dir("logs"), log_file_name() {
        min_log_level = 0;
        max_log_file_size = 256 << 20;
        max_log_file_num = 8;
        max_log_buffer_size = 32 << 20;
        cout = false;
    }

    fastring log_dir;
    fastring log_file_name;
    int32 min_log_level;
    int64 max_log_file_size;
    uint32 max_log_file_num;
    uint32 max_log_buffer_size;
    bool cout;
};

class LevelLogger {
  public:
    LevelLogger();

    ~LevelLogger() {
        this->stop();
    }

    void push(fastream* log, int level) {
        if (level < _config->min_log_level) return;

        const char* updated = _log_time.update();

        MutexGuard g(_log_mutex);
        if (updated) memcpy(_time_str, updated, 20);
        memcpy((char*)(log->data() + 1), _time_str, 20);

        if (unlikely(_fs->size() >= _config->max_log_buffer_size)) {
            const char* p = strchr(_fs->data() + (_fs->size() >> 1) + 7, '\n');
            size_t len = _fs->data() + _fs->size() - p - 1;
            CLOG << "log buffer is full, drop " << (_fs->size() - len) << " bytes";

            memcpy((char*)(_fs->data()), "......\n", 7);
            memcpy((char*)(_fs->data()) + 7, p + 1, len);
            _fs->resize(len + 7);
        }

        _fs->append(log->data(), log->size());
        if (_fs->size() > (_fs->capacity() >> 1)) _log_event.signal();
    }

    void push_fatal_log(fastream* log);

    void init();

    // Stop the logging thread and call write() to handle buffered logs.
    void stop();

    void safe_stop();

    void on_failure() {
        this->safe_stop();
        if (this->open_log_file(fatal)) {
            _file.write(_log_time.get());
            _file.write("] ");
            _stack_trace->set_file(&_file);
        }
    }

  private:
    bool open_log_file(int level=0);
    void write(fastream* fs);
    void rotate();
    void thread_fun();

  private:
    Mutex _log_mutex;
    SyncEvent _log_event;
    std::unique_ptr<Thread> _log_thread;
    std::unique_ptr<fastream> _fs;
    fs::file _file;
    std::unique_ptr<Config> _config;

    LogTime _log_time;
    char _time_str[21];
    int _stop;
    std::unique_ptr<StackTrace> _stack_trace;
};

LevelLogger::LevelLogger()
    : _log_event(true, false), _fs(new fastream(256 * 1024)), _stop(0) {
    _config.reset(new Config);
    _stack_trace.reset(new_stack_trace());
    _stack_trace->set_callback(&xx::on_failure);
    install_signal_handler();
    memcpy(_time_str, _log_time.get(), 20);
}

void LevelLogger::init() {
    _config->log_dir = path::clean(str::replace(FLG_log_dir, "\\", "/"));
    _config->log_file_name = FLG_log_file_name;
    _config->min_log_level = FLG_min_log_level;
    _config->max_log_file_size = FLG_max_log_file_size;
    _config->max_log_file_num = FLG_max_log_file_num;
    _config->max_log_buffer_size = FLG_max_log_buffer_size;
    _config->cout = FLG_cout;

    if (_config->max_log_file_num <= 0) _config->max_log_file_num = 8;
    if (_config->max_log_file_size <= 0) _config->max_log_file_size = 256 << 20;
    if (_config->max_log_buffer_size < (1 << 20)) _config->max_log_buffer_size = (1 << 20);

    _log_thread.reset(new Thread(&LevelLogger::thread_fun, this));
}

void LevelLogger::stop() {
    if (atomic_compare_swap(&_stop, 0, 1) != 0) return;
    _log_event.signal();
    if (_log_thread != NULL) _log_thread->join();

    MutexGuard g(_log_mutex);
    if (!_fs->empty()) {
        this->write(_fs.get());
        _fs->clear();
    }
}

// Try to call only async-signal-safe api in this function according to:
//   http://man7.org/linux/man-pages/man7/signal-safety.7.html
void LevelLogger::safe_stop() {
    if (atomic_compare_swap(&_stop, 0, 1) != 0) return;

    while (_stop != 2) {
      #ifdef _WIN32
        ::Sleep(8);
      #else
        struct timeval tv = { 0, 8000 };
        select(0, 0, 0, 0, &tv);
      #endif
    }

    if (!_fs->empty()) {
        this->write(_fs.get());
        _fs->clear();
    }
}

void LevelLogger::thread_fun() {
    std::unique_ptr<fastream> fs(new fastream(256 * 1024));

    while (!_stop) {
        bool signaled = _log_event.wait(128);
        if (_stop) break;

        {
            MutexGuard g(_log_mutex);
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

inline void LevelLogger::rotate() {
    if (!_file) return;
    if (!_file.exists() || _file.size() >= _config->max_log_file_size) {
        _file.close();
    }
}

void LevelLogger::write(fastream* fs) {
    fs::file& f = _file;
    if (f || this->open_log_file()) {
        f.write(fs->data(), fs->size());
    }
    this->rotate();
    if (_config->cout) fwrite(fs->data(), 1, fs->size(), stderr);
}

void LevelLogger::push_fatal_log(fastream* log) {
    log::close();

    fastring s(log->size() + 20);
    s.append(_log_time.get()).append(log->data(), log->size());
    fwrite(s.data(), 1, s.size(), stderr);

    if (this->open_log_file(fatal)) {
        _file.write(s.data(), s.size());
        _stack_trace->set_file(&_file);
    }

    _stack_trace->set_callback(0);
    abort();
}

inline fastring remove_dotexe(const fastring& s) {
    return !s.ends_with(".exe") ? s : s.substr(0, s.size() - 4);
}

bool LevelLogger::open_log_file(int level) {
    static fastring exename = remove_dotexe(os::exename());

    fastring name;
    if (level < fatal) {
        name = _config->log_file_name.empty() ? (exename + ".log") : _config->log_file_name;
    } else {
        name = exename + ".fatal";
    }

    fastring path = path::join(_config->log_dir, name);

    // Rename files: xx -> xx.1, xx.1 -> xx.2, xx.2 -> xx.3 ...
    if (fs::fsize(path) >= _config->max_log_file_size) {
        std::vector<fastring> paths;
        paths.push_back(path);

        for (unsigned int i = 1; i < _config->max_log_file_num; ++i) {
            fastring p = (fastring(path.size() + 4) << path << '.' << i);
            paths.push_back(p);
            if (!fs::exists(p)) break;
        }

        if (paths.size() == _config->max_log_file_num) {
            fs::remove(*paths.rbegin());
        }

        for (size_t i = paths.size() - 1; i > 0; --i) {
            fs::rename(paths[i - 1], paths[i]);
        }
    }

    if (!fs::exists(_config->log_dir)) fs::mkdir(_config->log_dir, true);
    if (!_file.open(path.c_str(), 'a')) {
        CLOG << "can't open log file: " << path;
        return false;
    }

    return true;
}

inline LevelLogger* level_logger() {
    static LevelLogger logger;
    return &logger;
}

void on_failure() {
    level_logger()->on_failure();
}

void on_signal(int sig) {
    level_logger()->safe_stop();
    signal(sig, SIG_DFL);
    raise(sig);
}

void push_fatal_log(fastream* fs) {
    level_logger()->push_fatal_log(fs);
}

void push_level_log(fastream* fs, int level) {
    level_logger()->push(fs, level);
}

LogTime::LogTime() {
    memset(_buf, 0, 21);
    for (int i = 0; i < 60; ++i) {
        char* p = (char*) &_cache[i];
        p[0] = i / 10 + '0';
        p[1] = i % 10 + '0';
    }
    this->reset();
}

const char* LogTime::update() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long ms = tv.tv_usec / 1000;
    long us = tv.tv_usec % 1000;
    time_t now_sec = tv.tv_sec * 1000000 + tv.tv_usec;
    if (now_sec == _start) return 0;

    int dt = (int) (now_sec - _start) / 1000000;
    if (unlikely(dt < 0 || dt > 60)) goto reset;

    _tm.tm_sec += dt;
    if (unlikely(_tm.tm_min == 59 && _tm.tm_sec > 59)) goto reset;

    do {
        _start = now_sec;

        if (_tm.tm_sec < 60) {
            char* p = (char*) &_cache[_tm.tm_sec];
            _buf[11] = p[0];
            _buf[12] = p[1];
        } else {
            _tm.tm_min++;
            _tm.tm_sec -= 60;
            *(uint16*)(_buf + 8) = _cache[_tm.tm_min];
            char* p = (char*) &_cache[_tm.tm_sec];
            _buf[11] = p[0];
            _buf[12] = p[1];
        }

        // Set millisecond & microsecond
        _buf[14] = ((char*)&_cache[ms / 100])[1];
        _buf[15] = ((char*)&_cache[ms % 100 / 10])[1];
        _buf[16] = ((char*)&_cache[ms % 10])[1];
        _buf[17] = ((char*)&_cache[us / 100])[1];
        _buf[18] = ((char*)&_cache[us % 100 / 10])[1];
        _buf[19] = ((char*)&_cache[us % 10])[1];

        return _buf;
    } while (0);

  reset:
    this->reset();
    return _buf;
}

} // namespace xx

void init() {
    xx::level_logger()->init();
}

void close() {
    xx::level_logger()->stop();
}

} // namespace log
} // namespace ___
