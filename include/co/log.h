#pragma once

#include "flag.h"
#include "fastream.h"
#include "atomic.h"

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
void exit();

// deprecated since v2.0.2, use log::exit() instead.
void close();

namespace xx {

enum LogLevel {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3,
    fatal = 4
};

class LevelLogSaver {
  public:
    LevelLogSaver(const char* file, int len, unsigned int line, int level);
    ~LevelLogSaver();

    fastream& stream() const { return _s; }

  private:
    fastream& _s;
    size_t _n;
};

class FatalLogSaver {
  public:
    FatalLogSaver(const char* file, int len, unsigned int line);
    ~FatalLogSaver();

    fastream& stream() const { return _s; }

  private:
    fastream& _s;
};

} // namespace xx
} // namespace log
} // namespace ___

using namespace ___;

// DLOG  ->  debug log
// LOG   ->  info log
// WLOG  ->  warning log
// ELOG  ->  error log
// FLOG  ->  fatal log    A fatal log will terminate the program.
// CHECK ->  fatal log
//
// LOG << "hello world " << 23;
// WLOG_IF(1 + 1 == 2) << "xx";
#define _CO_LOG_STREAM(lv)  log::xx::LevelLogSaver(__FILE__, sizeof(__FILE__) - 1, __LINE__, lv).stream()
#define _CO_FLOG_STREAM     log::xx::FatalLogSaver(__FILE__, sizeof(__FILE__) - 1, __LINE__).stream()
#define DLOG  if (FLG_min_log_level <= log::xx::debug)   _CO_LOG_STREAM(log::xx::debug)
#define LOG   if (FLG_min_log_level <= log::xx::info)    _CO_LOG_STREAM(log::xx::info)
#define WLOG  if (FLG_min_log_level <= log::xx::warning) _CO_LOG_STREAM(log::xx::warning)
#define ELOG  if (FLG_min_log_level <= log::xx::error)   _CO_LOG_STREAM(log::xx::error)
#define FLOG  _CO_FLOG_STREAM << "fatal error! "

// conditional log
#define DLOG_IF(cond) if (cond) DLOG
#define  LOG_IF(cond) if (cond) LOG
#define WLOG_IF(cond) if (cond) WLOG
#define ELOG_IF(cond) if (cond) ELOG
#define FLOG_IF(cond) if (cond) FLOG

#define CHECK(cond) \
    if (!(cond)) _CO_FLOG_STREAM << "check failed: " #cond "! "

#define CHECK_NOTNULL(p) \
    if ((p) == 0) _CO_FLOG_STREAM << "check failed: " #p " mustn't be NULL! "

#define _CO_CHECK_OP(a, b, op) \
    for (auto _x_ = std::make_pair(a, b); !(_x_.first op _x_.second);) \
        _CO_FLOG_STREAM << "check failed: " #a " " #op " " #b ", " \
                        << _x_.first << " vs " << _x_.second << "! "

#define CHECK_EQ(a, b) _CO_CHECK_OP(a, b, ==)
#define CHECK_NE(a, b) _CO_CHECK_OP(a, b, !=)
#define CHECK_GE(a, b) _CO_CHECK_OP(a, b, >=)
#define CHECK_LE(a, b) _CO_CHECK_OP(a, b, <=)
#define CHECK_GT(a, b) _CO_CHECK_OP(a, b, >)
#define CHECK_LT(a, b) _CO_CHECK_OP(a, b, <)

// occasional log
#define _co_log_counter_name_cat(x, n) x##n
#define _co_log_counter_make_name(x, n) _co_log_counter_name_cat(x, n)
#define _co_log_counter_name _co_log_counter_make_name(_co_log_counter_, __LINE__)

#define _CO_LOG_EVERY_N(n, what) \
    static unsigned int _co_log_counter_name = 0; \
    if (atomic_fetch_inc(&_co_log_counter_name) % (n) == 0) what

#define _CO_LOG_FIRST_N(n, what) \
    static int _co_log_counter_name = 0; \
    if (_co_log_counter_name < (n) && atomic_fetch_inc(&_co_log_counter_name) < (n)) what

#define DLOG_EVERY_N(n) _CO_LOG_EVERY_N(n, DLOG)
#define  LOG_EVERY_N(n) _CO_LOG_EVERY_N(n, LOG)
#define WLOG_EVERY_N(n) _CO_LOG_EVERY_N(n, WLOG)
#define ELOG_EVERY_N(n) _CO_LOG_EVERY_N(n, ELOG)

#define DLOG_FIRST_N(n) _CO_LOG_FIRST_N(n, DLOG)
#define  LOG_FIRST_N(n) _CO_LOG_FIRST_N(n, LOG)
#define WLOG_FIRST_N(n) _CO_LOG_FIRST_N(n, WLOG)
#define ELOG_FIRST_N(n) _CO_LOG_FIRST_N(n, ELOG)
