#pragma once

#include "god.h"
#include "log.h"

namespace co {
namespace xx {

struct E {
    E() : _s("") {}
    explicit E(const char* s) : _s(s) {}

    E(const E&) = default;
    ~E() = default;

    const char* error() const { return _s; }

  private:
    const char* _s;
};

} // xx

inline xx::E error(const char* s) {
    return xx::E(s);
}

template <typename T, typename X=void>
class maybe;

// T is void
template <typename T>
class maybe<T, god::enable_if_t<god::is_same<T, void>()>> final {
  public:
    using E = xx::E;

    maybe()
        : _e(), _has_error(false) {
    }

    maybe(int) : maybe() {}

    maybe(E&& e)
        : _e(std::move(e)), _has_error(true) {
    }

    maybe(maybe&& m)
        : _e(std::move(m._e)), _has_error(m._has_error) {
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() = default;

    bool has_error() const {
        return _has_error;
    }

    const char* error() const {
        return _e.error();
    }

    void value() const {}

    void must_value() const {
        CHECK(!this->has_error()) << "error: " << _e.error();
    }

  private:
    E _e;
    bool _has_error;
};

// T is scalar type
template <typename T>
class maybe<T, god::enable_if_t<god::is_scalar<T>()>> final {
  public:
    using E = xx::E;

    maybe()
        : _t(T()), _has_error(false) {
    }

    maybe(T t)
        : _t(t), _has_error(false) {
    }

    maybe(E&& e)
        : _e(std::move(e)), _has_error(true) {
    }

    maybe(maybe&& m)
        : _has_error(m._has_error) {
        !_has_error ? (void)(_t = m._t) : (void)(_e = m._e);
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() = default;

    bool has_error() const {
        return _has_error;
    }

    const char* error() const {
        return this->has_error() ? _e.error() : "";
    }

    T value() const {
        return !this->has_error() ? _t : T();
    }

    T must_value() const {
        CHECK(!this->has_error()) << "error: " << _e.error();
        return _t;
    }

  private:
    union {
        E _e;
        T _t;
    };
    bool _has_error;
};

// T is reference
template <typename T>
class maybe<T, god::enable_if_t<god::is_ref<T>()>> final {
  public:
    using E = xx::E;

    maybe(T t)
        : _t(&t), _has_error(false) {
    }

    maybe(E&& e)
        : _e(std::move(e)), _has_error(true) {
    }

    maybe(maybe&& m)
        : _has_error(m._has_error) {
        !_has_error ? (void)(_t = m._t) : (void)(_e = m._e);
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() = default;

    bool has_error() const {
        return _has_error;
    }

    const char* error() const {
        return this->has_error() ? _e.error() : "";
    }

    // for reference, we always check if there is an error.
    T value() const {
        CHECK(!this->has_error()) << "error: " << _e.error();
        return (T)*_t;
    }

    T must_value() const {
        CHECK(!this->has_error()) << "error: " << _e.error();
        return (T)*_t;
    }

  private:
    union {
        E _e;
        god::remove_ref_t<T>* _t;
    };
    bool _has_error;
};

// T is class
template <typename T>
class maybe<T, god::enable_if_t<god::is_class<T>()>> final {
  public:
    using E = xx::E;

    maybe(T&& t)
        : _t(std::move(t)), _has_error(false) {
    }

    maybe(E&& e)
        : _e(std::move(e)), _has_error(true) {
    }

    maybe(maybe&& m)
        : _has_error(m._has_error) {
        !_has_error ? (void)(new (&_t) T(std::move(m._t))) : (void)(_e = m._e);
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() {
        if (!this->has_error()) _t.~T();
    }

    bool has_error() const {
        return _has_error;
    }

    const char* error() const {
        return this->has_error() ? _e.error() : "";
    }

    // for class type, we always check if there is an error.
    T& value() const {
        CHECK(!this->has_error()) << "error: " << _e.error();
        return (T&)_t;
    }

    T& must_value() const {
        CHECK(!this->has_error()) << "error: " << _e.error();
        return (T&)_t;
    }

  private:
    union {
        E _e;
        T _t;
    };
    bool _has_error;
};

} // co
