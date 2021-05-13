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
        _start = time(0);
      #ifdef _WIN32
        _localtime64_s(&_tm, &_start);
      #else
        localtime_r(&_start, &_tm);
      #endif
        strftime(_buf, 16, "%m%d %H:%M:%S", &_tm);
    }

  private:
    time_t _start;
    struct tm _tm;
    char _buf[16];
    uint16 _cache[60];
};

class LevelLogger {
  public:
    LevelLogger();

    ~LevelLogger() {
        this->stop();
    }

    void push(fastream* log) {
        MutexGuard g(_log_mutex);
        memcpy((char*)(log->data() + 1), _time_str, 13);

        if (unlikely(_fs->size() >= FLG_max_log_buffer_size)) {
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
    void init_config();
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
    fastring _log_dir;
    fastring _log_file_name;

    LogTime _log_time;
    char _time_str[16];
    int _stop;
    std::unique_ptr<StackTrace> _stack_trace;
};

LevelLogger::LevelLogger()
    : _log_event(true, false), _fs(new fastream(256 * 1024)), _stop(0) {
    _stack_trace.reset(new_stack_trace());
    _stack_trace->set_callback(&xx::on_failure);
    install_signal_handler();
    memcpy(_time_str, _log_time.get(), 16);
}

inline void LevelLogger::init_config() {
    static bool initialized = false;
    if (!initialized && atomic_compare_swap(&initialized, false, true) == false) {
        FLG_log_dir = path::clean(str::replace(FLG_log_dir, "\\", "/"));
        FLG_log_file_name.remove_tail(".log");
        if (FLG_log_file_name.empty()) {
            FLG_log_file_name = os::exename();
            FLG_log_file_name.remove_tail(".exe");
        }
        if (FLG_max_log_file_num <= 0) FLG_max_log_file_num = 8;
        if (FLG_max_log_file_size <= 0) FLG_max_log_file_size = 256 << 20;
        if (FLG_max_log_buffer_size < (1 << 20)) FLG_max_log_buffer_size = (1 << 20);
    }
}

void LevelLogger::init() {
    this->init_config();
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

        const char* updated = _log_time.update();
        {
            MutexGuard g(_log_mutex);
            if (updated) memcpy(_time_str, updated, 13);
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
    if (!_file.exists() || _file.size() >= FLG_max_log_file_size) {
        _file.close();
    }
}

void LevelLogger::write(fastream* fs) {
    fs::file& f = _file;
    if (f || this->open_log_file()) {
        f.write(fs->data(), fs->size());
    }
    this->rotate();
    if (FLG_cout) fwrite(fs->data(), 1, fs->size(), stderr);
}

void LevelLogger::push_fatal_log(fastream* log) {
    log::close();

    memcpy((char*)log->data() + 1, _log_time.get(), 13);
    this->write(log);
    if (!FLG_cout) fwrite(log->data(), 1, log->size(), stderr);

    if (this->open_log_file(fatal)) {
        _file.write(log->data(), log->size());
        _stack_trace->set_file(&_file);
    }

    _stack_trace->set_callback(0);
    abort();
}

bool LevelLogger::open_log_file(int level) {
    this->init_config();
    static fastring path_base = path::join(FLG_log_dir, FLG_log_file_name);

    fastring path(path_base.size() + 8);
    if (level < xx::fatal) {
        path.append(path_base).append(".log");

        // rename log files:  xx.log -> xx_n.log
        if (fs::fsize(path) >= FLG_max_log_file_size) {
            std::vector<fastring> paths;
            paths.push_back(path);

            for (uint32 i = 1; i < FLG_max_log_file_num; ++i) {
                fastring p(path.size() + 4);
                p << path_base << '_' << i << ".log";
                paths.push_back(std::move(p));
                if (!fs::exists(paths.back())) break;
            }

            if (paths.size() == FLG_max_log_file_num) {
                fs::remove(paths.back());
            }

            for (size_t i = paths.size() - 1; i > 0; --i) {
                fs::rename(paths[i - 1], paths[i]);
            }
        }
    } else {
        path.append(path_base).append(".fatal");
    }

    if (!fs::exists(FLG_log_dir)) fs::mkdir(FLG_log_dir, true);
    if (!_file.open(path.c_str(), 'a')) {
        printf("failed to open the log file: %s\n", path.c_str());
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

void push_level_log(fastream* fs) {
    level_logger()->push(fs);
}

LogTime::LogTime() {
    memset(_buf, 0, 16);
    for (int i = 0; i < 60; ++i) {
        char* p = (char*) &_cache[i];
        p[0] = i / 10 + '0';
        p[1] = i % 10 + '0';
    }
    this->reset();
}

const char* LogTime::update() {
    time_t now_sec = time(0);
    if (now_sec == _start) return 0;

    int dt = (int) (now_sec - _start);
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

        return _buf;
    } while (0);

  reset:
    this->reset();
    return _buf;
}

} // namespace xx

void init() {
    static bool initialized = false;
    if (atomic_compare_swap(&initialized, false, true) == false) {
        xx::level_logger()->init();
    }
}

void close() {
    xx::level_logger()->stop();
}

} // namespace log
} // namespace ___
