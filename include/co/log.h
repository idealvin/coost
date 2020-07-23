#pragma once

#include "flag.h"
#include "fastream.h"
#include "atomic.h"
#include "thread.h"

#ifdef _MSC_VER
#pragma warning (disable:4722)
#endif

DEC_int32(min_log_level);

namespace ___ {
namespace log {

// log::init() must be called once at the beginning of main().
void init();

// Write all buffered logs to destination and stop the logging thread.
void close();

namespace xx {

void push_fatal_log(fastream* fs);
void push_level_log(fastream* fs, int level);

extern __thread fastream* xxLog;

enum LogLevel {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3,
    fatal = 4
};

class LevelLogSaver {
  public:
    LevelLogSaver(const char* file, unsigned line, int level) : _level(level) {
        if (xxLog == 0) xxLog = new fastream(128);
        xxLog->clear();

        (*xxLog) << "DIWEF"[level];
        xxLog->resize(21); // make room for time: 1108 18:16:08.123456
        (*xxLog) << ' ' << current_thread_id() << ' ' << file << ':' << line << ']' << ' ';
    }

    ~LevelLogSaver() {
        (*xxLog) << '\n';
        push_level_log(xxLog, _level);
    }

    fastream& fs() {
        return *xxLog;
    }

  private:
    int _level;
};

class FatalLogSaver {
  public:
    FatalLogSaver(const char* file, unsigned int line) {
        if (xxLog == 0) xxLog = new fastream(128);
        xxLog->clear();
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
    CLogSaver() = default;

    CLogSaver(const char* file, unsigned int line) {
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

// DLOG  ->  DEBUG
// LOG   ->  INFO
// WLOG  ->  WARNING
// ELOG  ->  ERROR
// FLOG  ->  FATAL    A FATAL log will terminate the program.
// CHECK ->  FATAL
//
// LOG << "hello world " << 23;
// WLOG_IF(1 + 1 == 2) << "xx";
#define DLOG  if (FLG_min_log_level <= log::xx::debug) \
              log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::debug).fs()
#define LOG   log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::info).fs()
#define WLOG  log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::warning).fs()
#define ELOG  log::xx::LevelLogSaver(__FILE__, __LINE__, log::xx::error).fs()
#define _FLOG log::xx::FatalLogSaver(__FILE__, __LINE__).fs()
#define FLOG  _FLOG << "fatal error! "

#define DLOG_IF(cond) if (cond) DLOG
#define  LOG_IF(cond) if (cond) LOG
#define WLOG_IF(cond) if (cond) WLOG
#define ELOG_IF(cond) if (cond) ELOG
#define FLOG_IF(cond) if (cond) FLOG

#define CHECK(cond) \
    if (!(cond)) _FLOG << "check failed: " #cond "! "

#define CHECK_NOTNULL(p) \
    if ((p) == 0) _FLOG << "check failed: " #p " mustn't be NULL! "

#define _CHECK_OP(a, b, op) \
    for (auto _x_ = std::make_pair(a, b); !(_x_.first op _x_.second);) \
        _FLOG << "check failed: " #a " " #op " " #b ", " \
              << _x_.first << " vs " << _x_.second << "! "

#define CHECK_EQ(a, b) _CHECK_OP(a, b, ==)
#define CHECK_NE(a, b) _CHECK_OP(a, b, !=)
#define CHECK_GE(a, b) _CHECK_OP(a, b, >=)
#define CHECK_LE(a, b) _CHECK_OP(a, b, <=)
#define CHECK_GT(a, b) _CHECK_OP(a, b, >)
#define CHECK_LT(a, b) _CHECK_OP(a, b, <)

// Occasional Log.
#define XX_LOG_COUNTER_NAME(x, n) XX_LOG_COUNTER_NAME_CONCAT(x, n)
#define XX_LOG_COUNTER_NAME_CONCAT(x, n) x##n
#define XX_LOG_COUNTER XX_LOG_COUNTER_NAME(XX_log_counter_, __LINE__)

#define _LOG_EVERY_N(n, what) \
    static unsigned int XX_LOG_COUNTER = 0; \
    if (atomic_fetch_inc(&XX_LOG_COUNTER) % (n) == 0) what

#define _LOG_FIRST_N(n, what) \
    static int XX_LOG_COUNTER = 0; \
    if (XX_LOG_COUNTER < (n) && atomic_fetch_inc(&XX_LOG_COUNTER) < (n)) what

#define DLOG_EVERY_N(n) _LOG_EVERY_N(n, DLOG)
#define  LOG_EVERY_N(n) _LOG_EVERY_N(n, LOG)
#define WLOG_EVERY_N(n) _LOG_EVERY_N(n, WLOG)
#define ELOG_EVERY_N(n) _LOG_EVERY_N(n, ELOG)

#define DLOG_FIRST_N(n) _LOG_FIRST_N(n, DLOG)
#define  LOG_FIRST_N(n) _LOG_FIRST_N(n, LOG)
#define WLOG_FIRST_N(n) _LOG_FIRST_N(n, WLOG)
#define ELOG_FIRST_N(n) _LOG_FIRST_N(n, ELOG)
