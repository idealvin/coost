#pragma once

#include "flag.h"
#include "fastream.h"
#include "atomic.h"
#include <functional>

__coapi DEC_bool(cout);
__coapi DEC_int32(min_log_level);

namespace ___ {
namespace log {

/**
 * stop the logging thread and write all buffered logs to destination.
 *   - This function will be automatically called at exit.
 */
__coapi void exit();

enum {
    log2local = 1,
};

/**
 * set a callback for writing level logs
 *   - By default, logs will be written into a local file. Users can set a callback to 
 *     write logs to different destinations.
 * 
 * @param cb
 *   a callback takes 2 params:  void f(const void* p, size_t n);
 *     - p points to the buffer which may contain multiple logs
 *     - n is size of the data in the buffer
 * 
 * @param flags
 *   formed by ORing any of the following values:
 *     - log::log2local: also log to local file
 */
__coapi void set_write_cb(const std::function<void(const void*, size_t)>& cb, int flags=0);

/**
 * set a callback for writing topic logs (TLOG)
 * 
 * @param cb
 *   a callback takes 3 params:  void f(const char* topic, const void* p, size_t n);
 *     - topic is useful when writing logs to something like kafka
 *     - p points to the buffer which may contain multiple logs
 *     - n is size of the data in the buffer
 * 
 * @param flags
 *   formed by ORing any of the following values:
 *     - log::log2local: also log to local file
 */
__coapi void set_write_cb(const std::function<void(const char*, const void*, size_t)>& cb, int flags=0);

namespace xx {

enum LogLevel {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3,
    fatal = 4
};

class __coapi LevelLogSaver {
  public:
    LevelLogSaver(const char* fname, unsigned fnlen, unsigned line, int level);
    ~LevelLogSaver();

    fastream& stream() const { return _s; }

  private:
    fastream& _s;
    size_t _n;
};

class __coapi FatalLogSaver {
  public:
    FatalLogSaver(const char* fname, unsigned fnlen, unsigned line);
    ~FatalLogSaver();

    fastream& stream() const { return _s; }

  private:
    fastream& _s;
};

class __coapi TLogSaver {
  public:
    TLogSaver(const char* fname, unsigned fnlen, unsigned line, const char* topic);
    ~TLogSaver();

    fastream& stream() const { return _s; }

  private:
    fastream& _s;
    size_t _n;
    const char* _topic;
};

template <int N>
constexpr const char* path_base(const char(&s)[N], int i = N - 1) {
    return (s[i] == '/' || s[i] == '\\') ? (s + i + 1) : (i == 0 ? s : path_base(s, i - 1));
}

template <int N>
constexpr int path_base_len(const char(&s)[N], int i = N - 1) {
    return (s[i] == '/' || s[i] == '\\') ? (N - 2 - i) : (i == 0 ? N-1 : path_base_len(s, i - 1));
}

} // namespace xx
} // namespace log
} // namespace ___

using namespace ___;

#define _CO_FNAME log::xx::path_base(__FILE__)
#define _CO_FNLEN log::xx::path_base_len(__FILE__)
#define _CO_FILELINE _CO_FNAME,_CO_FNLEN,__LINE__

// TLOG are logs grouped by the topic.
// TLOG("xxx") << "hello xxx" << 23;
// It is better to use a literal string for the topic.
#define TLOG(topic) log::xx::TLogSaver(_CO_FILELINE, topic).stream()
#define TLOG_IF(topic, cond) if (cond) TLOG(topic)

// DLOG  ->  debug log
// LOG   ->  info log
// WLOG  ->  warning log
// ELOG  ->  error log
// FLOG  ->  fatal log    A fatal log will terminate the program.
// CHECK ->  fatal log
//
// LOG << "hello world " << 23;
// WLOG_IF(1 + 1 == 2) << "xx";
#define _CO_LOG_STREAM(lv)  log::xx::LevelLogSaver(_CO_FILELINE, lv).stream()
#define _CO_FLOG_STREAM     log::xx::FatalLogSaver(_CO_FILELINE).stream()
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
        _CO_FLOG_STREAM << "check failed: " #a " " #op " " #b ", " << _x_.first << " vs " << _x_.second << "! "

#define CHECK_EQ(a, b) _CO_CHECK_OP(a, b, ==)
#define CHECK_NE(a, b) _CO_CHECK_OP(a, b, !=)
#define CHECK_GE(a, b) _CO_CHECK_OP(a, b, >=)
#define CHECK_LE(a, b) _CO_CHECK_OP(a, b, <=)
#define CHECK_GT(a, b) _CO_CHECK_OP(a, b, >)
#define CHECK_LT(a, b) _CO_CHECK_OP(a, b, <)

// occasional log
#define _CO_LOG_COUNTER CO_CONCAT(_co_log_counter_, __LINE__)

#define _CO_LOG_EVERY_N(n, what) \
    static unsigned int _CO_LOG_COUNTER = 0; \
    if (atomic_fetch_inc(&_CO_LOG_COUNTER, mo_relaxed) % (n) == 0) what

#define _CO_LOG_FIRST_N(n, what) \
    static int _CO_LOG_COUNTER = 0; \
    if (_CO_LOG_COUNTER < (n) && atomic_fetch_inc(&_CO_LOG_COUNTER, mo_relaxed) < (n)) what

#define DLOG_EVERY_N(n) _CO_LOG_EVERY_N(n, DLOG)
#define  LOG_EVERY_N(n) _CO_LOG_EVERY_N(n, LOG)
#define WLOG_EVERY_N(n) _CO_LOG_EVERY_N(n, WLOG)
#define ELOG_EVERY_N(n) _CO_LOG_EVERY_N(n, ELOG)

#define DLOG_FIRST_N(n) _CO_LOG_FIRST_N(n, DLOG)
#define  LOG_FIRST_N(n) _CO_LOG_FIRST_N(n, LOG)
#define WLOG_FIRST_N(n) _CO_LOG_FIRST_N(n, WLOG)
#define ELOG_FIRST_N(n) _CO_LOG_FIRST_N(n, ELOG)
