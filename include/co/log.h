#pragma once

#include "flag.h"

DEC_uint32(min_log_level);

namespace __co {
namespace log {
namespace xx {

struct LogInit {
    LogInit();
    ~LogInit() = default;
};

static LogInit g_log_init;

enum LogLevel {
    debug = 0,
    info = 1,
    warn = 2,
    error = 3,
    fatal = 4
};

struct LogSaver {
    LogSaver(const char* fname, unsigned fnlen, unsigned line, int level);
    ~LogSaver();

    co::string& s;
    size_t n;
};

struct FatalLogSaver {
    FatalLogSaver(const char* fname, unsigned fnlen, unsigned line);
    ~FatalLogSaver();

    co::string& s;
};

struct filine {
    constexpr filine(const char* s, int n) noexcept
        : file(s), line(n) {
    }
    const char* file;
    int line;
};

constexpr int path_len(const char* s, int i=0) {
    return *(s+i) ? path_len(s, i+1) : i;
}

constexpr const char* path_base(const char* s, int i) {
    return (s[i] == '/' || s[i] == '\\') ? (s + i + 1) : (i == 0 ? s : path_base(s, i - 1));
}

constexpr int path_base_len(const char* s) {
    return path_len(path_base(s, path_len(s)));
}

} // xx

#define _filine_ xx::filine(__builtin_FILE(), __builtin_LINE())

template<typename... V>
struct debug {
    debug(V&&... v, xx::filine&& fl=_filine_) {
        if (FLG_min_log_level <= xx::debug) xx::LogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line, xx::debug
        ).s.cat(std::forward<V>(v)...);
    }
};

template<typename... V>
debug(V&&...) -> debug<V...>;

template<typename... V>
struct info {
    info(V&&... v, xx::filine&& fl=_filine_) {
        if (FLG_min_log_level <= xx::info) xx::LogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line, xx::info
        ).s.cat(std::forward<V>(v)...);
    }
};

template<typename... V>
info(V&&...) -> info<V...>;

template<typename... V>
struct warn {
    warn(V&&... v, xx::filine&& fl=_filine_) {
        if (FLG_min_log_level <= xx::warn) xx::LogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line, xx::warn
        ).s.cat(std::forward<V>(v)...);
    }
};

template<typename... V>
warn(V&&...) -> warn<V...>;

template<typename... V>
struct error {
    error(V&&... v, xx::filine&& fl=_filine_) {
        if (FLG_min_log_level <= xx::error) xx::LogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line, xx::error
        ).s.cat(std::forward<V>(v)...);
    }
};

template<typename... V>
error(V&&...) -> error<V...>;

template<typename... V>
struct fatal {
    fatal(V&&... v, xx::filine&& fl=_filine_) {
        xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat("fatal error! ", std::forward<V>(v)...);
    }
};

template<typename... V>
fatal(V&&...) -> fatal<V...>;

template<typename... V>
struct check {
    check(bool cond, V&&... v, xx::filine&& fl=_filine_) {
        if (!cond) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat("check failed! ", std::forward<V>(v)...);
    }
};

template<typename... V>
check(bool, V&&...) -> check<V...>;

template<typename T, typename... V>
struct check_notnull {
    check_notnull(T* p, V&&... v, xx::filine&& fl=_filine_) {
        if (p == nullptr) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat("check_notnull failed! ", std::forward<V>(v)...);
    }
};

template<typename T, typename... V>
check_notnull(T*, V&&...) -> check_notnull<T, V...>;

template<typename A, typename B, typename... V>
struct check_eq {
    check_eq(A&& a, B&& b, V&&... v, xx::filine&& fl=_filine_) {
        if (!(a == b)) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat(
            "check_eq failed: ",
            std::forward<A>(a), " vs ", std::forward<B>(b), "! ",
            std::forward<V>(v)...
        );
    }
};

template<typename A, typename B, typename... V>
check_eq(A&&, B&&, V&&...) -> check_eq<A, B, V...>;

template<typename A, typename B, typename... V>
struct check_ne {
    check_ne(A&& a, B&& b, V&&... v, xx::filine&& fl=_filine_) {
        if (!(a != b)) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat(
            "check_ne failed: ",
            std::forward<A>(a), " vs ", std::forward<B>(b), "! ",
            std::forward<V>(v)...
        );
    }
};

template<typename A, typename B, typename... V>
check_ne(A&&, B&&, V&&...) -> check_ne<A, B, V...>;

template<typename A, typename B, typename... V>
struct check_lt {
    check_lt(A&& a, B&& b, V&&... v, xx::filine&& fl=_filine_) {
        if (!(a < b)) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat(
            "check_lt failed: ",
            std::forward<A>(a), " vs ", std::forward<B>(b), "! ",
            std::forward<V>(v)...
        );
    }
};

template<typename A, typename B, typename... V>
check_lt(A&&, B&&, V&&...) -> check_lt<A, B, V...>;

template<typename A, typename B, typename... V>
struct check_le {
    check_le(A&& a, B&& b, V&&... v, xx::filine&& fl=_filine_) {
        if (!(a <= b)) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat(
            "check_le failed: ",
            std::forward<A>(a), " vs ", std::forward<B>(b), "! ",
            std::forward<V>(v)...
        );
    }
};

template<typename A, typename B, typename... V>
check_le(A&&, B&&, V&&...) -> check_le<A, B, V...>;

template<typename A, typename B, typename... V>
struct check_gt {
    check_gt(A&& a, B&& b, V&&... v, xx::filine&& fl=_filine_) {
        if (!(a > b)) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat(
            "check_gt failed: ",
            std::forward<A>(a), " vs ", std::forward<B>(b), "! ",
            std::forward<V>(v)...
        );
    }
};

template<typename A, typename B, typename... V>
check_gt(A&&, B&&, V&&...) -> check_gt<A, B, V...>;

template<typename A, typename B, typename... V>
struct check_ge {
    check_ge(A&& a, B&& b, V&&... v, xx::filine&& fl=_filine_) {
        if (!(a >= b)) xx::FatalLogSaver(
            xx::path_base(fl.file, xx::path_len(fl.file)),
            xx::path_base_len(fl.file), fl.line
        ).s.cat(
            "check_ge failed: ",
            std::forward<A>(a), " vs ", std::forward<B>(b), "! ",
            std::forward<V>(v)...
        );
    }
};

template<typename A, typename B, typename... V>
check_ge(A&&, B&&, V&&...) -> check_ge<A, B, V...>;

#undef _filine_

// set a write callback to write the logs,
// also logging to local file if @also_log2local is true.
void set_write_cb(void(*cb)(const void*, size_t), bool also_log2local=false);

// flush the log buffer and close the logging system
void close();

} // namespace log
} // namespace __co

using namespace __co;
