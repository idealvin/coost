#pragma once

#include "god.h"
#include "log.h"
#include "fastring.h"

namespace co {
namespace xx {

struct E {
    E() = default;
    E(const E&) = default;
    ~E() = default;

    E(E&& e) : _s(std::move(e._s)) {}
    explicit E(const char* e) : _s(e) {}

    fastring& error() const { return (fastring&)_s; }

  private:
    fastring _s;
};

} // xx

inline xx::E error(const char* e) {
    return xx::E(e);
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

    maybe(int) : maybe() {
    }

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

    fastring& error() const {
        return _e.error();
    }

    void value() const {}

    void must_value() const {
        CHECK(!this->has_error());
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
        : _e(), _t(T()), _has_error(false) {
    }

    maybe(T t)
        : _e(), _t(t), _has_error(false) {
    }

    maybe(E&& e)
        : _e(std::move(e)), _t(T()), _has_error(true) {
    }

    maybe(maybe&& m)
        : _e(std::move(m._e)), _t(m._t), _has_error(m._has_error) {
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() = default;

    bool has_error() const {
        return _has_error;
    }

    fastring& error() const {
        return _e.error();
    }

    T value() const {
        return _t;
    }

    T must_value() const {
        CHECK(!this->has_error());
        return this->value();
    }

  private:
    E _e;
    T _t;
    bool _has_error;
};

// T is reference
template <typename T>
class maybe<T, god::enable_if_t<god::is_ref<T>()>> final {
  public:
    using E = xx::E;

    maybe(T t)
        : _e(), _t(&t), _has_error(false) {
    }

    maybe(E&& e)
        : _e(std::move(e)), _t(0), _has_error(true) {
    }

    maybe(maybe&& m)
        : _e(std::move(m._e)), _t(m._t), _has_error(m._has_error) {
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() = default;

    bool has_error() const {
        return _has_error;
    }

    fastring& error() const {
        return _e.error();
    }

    T value() const {
        return (T)*_t;
    }

    T must_value() const {
        CHECK(!this->has_error());
        return this->value();
    }

  private:
    E _e;
    god::remove_ref_t<T>* _t;
    bool _has_error;
};

// T is class
template <typename T>
class maybe<T, god::enable_if_t<god::is_class<T>()>> final {
  public:
    using E = xx::E;

    maybe(T&& t)
        : _e(), _t(std::move(t)), _has_error(false) {
    }

    maybe(E&& e)
        : _e(std::move(e)), _has_error(true) {
    }

    maybe(maybe&& m)
        : _e(std::move(m._e)), _has_error(m._has_error) {
        if (!_has_error) new (&_t) T(std::move(m._t));
    }

    maybe(const maybe&) = delete;
    void operator=(const maybe&) = delete;

    ~maybe() {
        if (!this->has_error()) _t.~T();
    }

    bool has_error() const {
        return _has_error;
    }

    fastring& error() const {
        return _e.error();
    }

    T& value() const {
        return (T&)_t;
    }

    T& must_value() const {
        CHECK(!this->has_error());
        return this->value();
    }

  private:
    E _e;
    union {
        T _t;
        char _dummy[sizeof(T)];
    };
    bool _has_error;
};

} // co
