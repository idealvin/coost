#pragma once

#include "flag.h"
#include "fastream.h"
#include "atomic.h"
#include "thread.h"

#ifdef _MSC_VER
#pragma warning (disable:4722)
#endif

DEC_bool(cout);
DEC_int32(min_log_level);

namespace ___ {
namespace log {

/**
 * initialize the log library and start the logging thread 
 *   - log::init() should be called once at the beginning of main(). 
 *   - It is safe to call log::init() for multiple times. 
 */
void init();

/**
 * close the log library 
 *   - write all buffered logs to destination.
 *   - stop the logging thread.
 */
void close();

namespace xx {

void push_fatal_log(fastream* fs);
void push_level_log(char* s, size_t n);

extern __thread fastream* xxLog;

enum LogLevel {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3,
    fatal = 4
};

// xxx0523 17:00:00.123xxxx
struct log_time_t {
    enum {
        size = 13,
        total_size = 17
    };
    char buf[24];
    char* data;

    log_time_t() {
        memset(buf, 0, 24);
        data = buf + 3;
    }

    void update(const char* s) {
        memcpy(data, s, size);
    }

    void update_ms(uint32 ms) {
        *((uint32*)(buf + 16)) = ms;
    }
};

class LevelLogSaver {
  public:
    LevelLogSaver(const char* file, unsigned line, int level) {
        if (unlikely(xxLog == 0)) xxLog = new fastream(128);
        _n = xxLog->size();
        xxLog->resize(log_time_t::total_size + 1 + _n); // make room for time
        (*xxLog)[_n] = "DIWEF"[level];
        (*xxLog) << ' ' << current_thread_id() << ' ' << file << ':' << line << ']' << ' ';
    }

    ~LevelLogSaver() {
        (*xxLog) << '\n';
        push_level_log((char*)xxLog->data() + _n, xxLog->size() - _n);
        xxLog->resize(_n);
    }

    fastream& fs() {
        return *xxLog;
    }

  private:
    size_t _n;
};

class FatalLogSaver {
  public:
    FatalLogSaver(const char* file, unsigned int line) {
        if (unlikely(xxLog == 0)) xxLog = new fastream(128);
        xxLog->resize(log_time_t::total_size + 1);
        xxLog->front() = 'F';
        (*xxLog) << ' ' << current_thread_id() << ' ' << file << ':' << line << ']' << ' ';
    }

    ~FatalLogSaver() {
        (*xxLog) << '\n';
        push_fatal_log(xxLog);
    }

    fastream& fs() {
        return *xxLog;
    }
};

class CLogSaver {
  public:
    CLogSaver() : _fs(128) {}

    CLogSaver(const char* file, unsigned int line) : _fs(128) {
        _fs << file << ':' << line << ']' << ' ';
    }

    ~CLogSaver() {
        _fs << '\n';
        ::fwrite(_fs.data(), 1, _fs.size(), stderr);
    }

    fastream& fs() {
        return _fs;
    }

  private:
    fastream _fs;
};

} // namespace xx
} // namespace log
} // namespace ___

using namespace ___;

#define COUT   log::xx::CLogSaver().fs()
#define CLOG   log::xx::CLogSaver(__FILE__, __LINE__).fs()

// DLOG  ->  debug log
// LOG   ->  info log
// WLOG  ->  warning log
// ELOG  ->  error log
// FLOG  ->  fatal log    A fatal log will terminate the program.
// CHECK ->  fatal log
//
// LOG << "hello world " << 23;
// WLOG_IF(1 + 1 == 2) << "xx";
#define _DLOG_STREAM  log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::debug).fs()
#define _LOG_STREAM   log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::info).fs()
#define _WLOG_STREAM  log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::warning).fs()
#define _ELOG_STREAM  log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::error).fs()
#define _FLOG_STREAM  log::xx::FatalLogSaver(__FILE__, __LINE__).fs()
#define DLOG  if (FLG_min_log_level <= log::xx::debug)   _DLOG_STREAM
#define LOG   if (FLG_min_log_level <= log::xx::info)     _LOG_STREAM
#define WLOG  if (FLG_min_log_level <= log::xx::warning) _WLOG_STREAM
#define ELOG  if (FLG_min_log_level <= log::xx::error)   _ELOG_STREAM
#define FLOG  _FLOG_STREAM << "fatal error! "

// conditional log
#define DLOG_IF(cond) if (cond) DLOG
#define  LOG_IF(cond) if (cond) LOG
#define WLOG_IF(cond) if (cond) WLOG
#define ELOG_IF(cond) if (cond) ELOG
#define FLOG_IF(cond) if (cond) FLOG

#define CHECK(cond) \
    if (!(cond)) _FLOG_STREAM << "check failed: " #cond "! "

#define CHECK_NOTNULL(p) \
    if ((p) == 0) _FLOG_STREAM << "check failed: " #p " mustn't be NULL! "

#define _CHECK_OP(a, b, op) \
    for (auto _x_ = std::make_pair(a, b); !(_x_.first op _x_.second);) \
        _FLOG_STREAM << "check failed: " #a " " #op " " #b ", " \
                     << _x_.first << " vs " << _x_.second << "! "

#define CHECK_EQ(a, b) _CHECK_OP(a, b, ==)
#define CHECK_NE(a, b) _CHECK_OP(a, b, !=)
#define CHECK_GE(a, b) _CHECK_OP(a, b, >=)
#define CHECK_LE(a, b) _CHECK_OP(a, b, <=)
#define CHECK_GT(a, b) _CHECK_OP(a, b, >)
#define CHECK_LT(a, b) _CHECK_OP(a, b, <)

// occasional log
#define CO_LOG_COUNTER_NAME(x, n) CO_LOG_COUNTER_NAME_CONCAT(x, n)
#define CO_LOG_COUNTER_NAME_CONCAT(x, n) x##n
#define CO_LOG_COUNTER CO_LOG_COUNTER_NAME(CO_log_counter_, __LINE__)

#define _LOG_EVERY_N(n, what) \
    static unsigned int CO_LOG_COUNTER = 0; \
    if (atomic_fetch_inc(&CO_LOG_COUNTER) % (n) == 0) what

#define _LOG_FIRST_N(n, what) \
    static int CO_LOG_COUNTER = 0; \
    if (CO_LOG_COUNTER < (n) && atomic_fetch_inc(&CO_LOG_COUNTER) < (n)) what

#define DLOG_EVERY_N(n) _LOG_EVERY_N(n, DLOG)
#define  LOG_EVERY_N(n) _LOG_EVERY_N(n, LOG)
#define WLOG_EVERY_N(n) _LOG_EVERY_N(n, WLOG)
#define ELOG_EVERY_N(n) _LOG_EVERY_N(n, ELOG)

#define DLOG_FIRST_N(n) _LOG_FIRST_N(n, DLOG)
#define  LOG_FIRST_N(n) _LOG_FIRST_N(n, LOG)
#define WLOG_FIRST_N(n) _LOG_FIRST_N(n, WLOG)
#define ELOG_FIRST_N(n) _LOG_FIRST_N(n, ELOG)
