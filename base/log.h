#pragma once

#include "flag.h"
#include "fastream.h"
#include "atomic.h"
#include "thread.h"

#ifdef _WIN32
#pragma warning (disable:4722)
#endif

DEC_int32(min_log_level);

namespace ___ {
namespace log {

void init();

// Write all buffered logs to destination and stop the logging thread.
void close();

namespace xx {

void push_fatal_log(fastream* fs);
void push_level_log(fastream* fs, int level);

extern __thread fastream* _Log;

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
        if (_Log == 0) _Log = new fastream(128);
        _Log->clear();

        (*_Log) << "DIWEF"[level];
        _Log->resize(14); // make room for time: 1108 18:16:08
        (*_Log) << ' ' << gettid() << ' ' << file << ':' << line << ']' << ' ';
    }

    ~LevelLogSaver() {
        (*_Log) << '\n';
        push_level_log(_Log, _level);
    }

    fastream& fs() {
        return *_Log;
    }

  private:
    int _level;
};

class FatalLogSaver {
  public:
    FatalLogSaver(const char* file, unsigned int line) {
        if (_Log == 0) _Log = new fastream(128);
        _Log->clear();
        (*_Log) << ' ' << gettid() << ' ' << file << ':' << line << ']' << ' ';
    }

    ~FatalLogSaver() {
        (*_Log) << '\n';
        push_fatal_log(_Log);
    }

    fastream& fs() {
        return *_Log;
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

#define CHECK_NOT0(p) \
    if ((p) == 0) _FLOG << "check failed: " #p " mustn't be 0! "

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
#define _LOG_COUNTER_NAME(x, n) _LOG_COUNTER_NAME_CONCAT(x, n)
#define _LOG_COUNTER_NAME_CONCAT(x, n) x##n
#define _LOG_COUNTER _LOG_COUNTER_NAME(_Log_counter_, __LINE__)

#define _LOG_EVERY_N(n, what) \
    static unsigned int _Log_counter_ = 0; \
    if (atomic_fetch_inc(&_Log_counter_) % (n) == 0) what

#define _LOG_FIRST_N(n, what) \
    static int _Log_counter_ = 0; \
    if (_Log_counter_ < (n) && atomic_fetch_inc(&_Log_counter_) < (n)) what

#define DLOG_EVERY_N(n) _LOG_EVERY_N(n, DLOG)
#define  LOG_EVERY_N(n) _LOG_EVERY_N(n, LOG)
#define WLOG_EVERY_N(n) _LOG_EVERY_N(n, WLOG)
#define ELOG_EVERY_N(n) _LOG_EVERY_N(n, ELOG)

#define DLOG_FIRST_N(n) _LOG_FIRST_N(n, DLOG)
#define  LOG_FIRST_N(n) _LOG_FIRST_N(n, LOG)
#define WLOG_FIRST_N(n) _LOG_FIRST_N(n, WLOG)
#define ELOG_FIRST_N(n) _LOG_FIRST_N(n, ELOG)
