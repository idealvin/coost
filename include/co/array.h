#pragma once

#include "god.h"
#include "mem.h"
#include <string.h>
#include <assert.h>
#include <initializer_list>

namespace co {

template <typename T, typename Alloc=co::default_allocator>
class array {
  public:
    constexpr array() noexcept
        : _cap(0), _size(0), _p(0) {
    }

    // create an empty array with capacity: @cap
    explicit array(size_t cap)
        : _cap(cap), _size(0), _p((T*) Alloc::alloc(sizeof(T) * cap)) {
    }

    // create an array of n elements with the same value: @x
    // condition: X is not int or T is int.
    template <
        typename X,
        god::enable_if_t<
            !god::is_same<god::remove_cvref_t<X>, int>() ||
            god::is_same<god::remove_cv_t<T>, int>(), int
        > = 0
    >
    array(size_t n, X&& x)
        : _cap(n), _size(n), _p((T*) Alloc::alloc(sizeof(T) * n)) {
        for (size_t i = 0; i < n; ++i) new (_p + i) T(x);
    }

    // create an array of n elements with default value
    // condition: X is int and T is not int.
    template <
        typename X,
        god::enable_if_t<
            god::is_same<god::remove_cvref_t<X>, int>() && 
            !god::is_same<god::remove_cv_t<T>, int>(), int
        > = 0
    >
    array(size_t n, X&&)
        : _cap(n), _size(n), _p((T*) Alloc::alloc(sizeof(T) * n)) {
        for (size_t i = 0; i < n; ++i) new (_p + i) T();
    }

    array(const array& x)
        : _cap(x.size()), _size(_cap) {
        _p = (T*) Alloc::alloc(sizeof(T) * _cap);
        this->_copy_n(_p, x._p, _cap);
    }

    array(array&& x) noexcept
        : _cap(x._cap), _size(x._size), _p(x._p) {
        x._p = 0;
        x._cap = x._size = 0;
    }

    // create array from an initializer list
    array(std::initializer_list<T> x)
        : _cap(x.size()), _size(0), _p((T*) Alloc::alloc(sizeof(T) * _cap)) {
        for (const auto& e : x) new (_p + _size++) T(e);
    }

    template <typename It, god::enable_if_t<god::is_class<It>(), int> = 0>
    array(It beg, It end) : array(8) {
        this->append(beg, end);
    }

    // create array from an array
    array(T* p, size_t n) : array(n) {
        this->_copy_n(_p, p, n);
        _size += n;
    }

    ~array() {
        this->reset();
    }

    size_t capacity() const noexcept { return _cap; }
    size_t size() const noexcept { return _size; }
    T* data() const noexcept { return _p; }
    bool empty() const noexcept { return this->size() == 0; }

    T& back() { return _p[_size - 1]; }
    const T& back() const { return _p[_size - 1]; }

    T& front() { return _p[0]; }
    const T& front() const { return _p[0]; }

    T& operator[](size_t n) { return _p[n]; }
    const T& operator[](size_t n) const { return _p[n]; }

    array& operator=(const array& x) {
        if (&x != this) {
            this->reset();
            new (this) array(x);
        }
        return *this;
    }

    array& operator=(array&& x) {
        if (&x != this) {
            this->reset();
            new (this) array(std::move(x));
        }
        return *this;
    }

    array& operator=(std::initializer_list<T> x) {
        this->reset();
        new (this) array(x);
        return *this;
    }

    void reserve(size_t n) {
        if (_cap < n) {
            _p = (T*) Alloc::realloc(_p, sizeof(T) * _cap, sizeof(T) * n); assert(_p);
            _cap = n;
        }
    }

    /**
     * size -> n
     *   - Reduced elements will be destroyed if n is less than the current size.
     *   - NOTE: No element will be created if n is greater than the current size.
     */
    void resize(size_t n) {
        this->reserve(n);
        this->_destruct_range(_p, n, _size);
        _size = n;
    }

    // destroy all elements and free the memory
    void reset() {
        if (_p) {
            this->_destruct_range(_p, 0, _size);
            Alloc::free(_p, sizeof(T) * _cap); _p = 0;
            _cap = _size = 0;
        }
    }

    void clear() {
        this->_destruct_range(_p, 0, _size);
        _size = 0;
    }

    void append(const T& x) {
        if (unlikely(_cap == _size)) {
            const size_t cap = _cap;
            _cap += (_cap >> 1) + 1;
            _p = (T*) Alloc::realloc(_p, sizeof(T) * cap, sizeof(T) * _cap); assert(_p);
        }
        new (_p + _size++) T(x);
    }

    void append(T&& x) {
        if (unlikely(_cap == _size)) {
            const size_t cap = _cap;
            _cap += (_cap >> 1) + 1;
            _p = (T*) Alloc::realloc(_p, sizeof(T) * cap, sizeof(T) * _cap); assert(_p);
        }
        new (_p + _size++) T(std::move(x));
    }

    void append(size_t n, const T& x) {
        const size_t m = n + _size;
        this->reserve(m);
        for (size_t i = _size; i < m; ++i) new (_p + i) T(x);
        _size += n;
    }

    template <typename It, god::enable_if_t<god::is_class<It>(), int> = 0>
    void append(It beg, It end) {
        for (auto it = beg; it != end; ++it) this->append(*it);
    }

    // append n elements from an array
    void append(const T* p, size_t n) {
        this->reserve(_size + n);
        this->_copy_n(_p + _size, p, n);
        _size += n;
    }

    // append an array, &x == this is ok.
    void append(const array& x) {
        if (&x != this) {
            this->append(x.data(), x.size());
        } else {
            this->reserve(_size << 1);
            this->_copy_n(_p + _size, _p, _size);
            _size <<= 1;
        }
    }

    // append an array, elements in @x will be moved to the end of this array
    void append(array&& x) {
        if (&x != this) {
            this->reserve(_size + x.size());
            this->_move_n(_p + _size, x._p, x._size);
            _size += x.size();
        } else {
            this->reserve(_size << 1);
            this->_copy_n(_p + _size, _p, _size);
            _size <<= 1;
        }
    }

    // like append(), but it is safe when p points to part of the array itself.
    void safe_append(const T* p, size_t n) {
        if (p < _p || p >= _p + _size) {
            this->append(p, n);
        } else {
            assert(p + n <= _p + _size);
            const size_t x = p - _p;
            this->reserve(_size + n);
            this->_copy_n(_p + _size, _p + x, n);
            _size += n;
        }
    }

    // insert a new element (construct with args x...) at the back
    template <typename ... X>
    void emplace(X&& ... x) {
        this->reserve(_size + 1);
        new (_p + _size++) T(std::forward<X>(x)...);
    }

    void push_back(const T& x) { this->append(x); }
    void push_back(T&& x) { this->append(std::move(x)); }

    // pop and return the last element
    T pop_back() {
        T x(std::move(_p[--_size]));
        this->_destruct(_p[_size]);
        return x;
    }

    // remove the last element
    void remove_back() {
        this->_destruct(_p[--_size]);
    }

    // remove the nth element, and move the last element to the nth position
    void remove(size_t n) {
        if (n < _size) {
            if (n != _size - 1) {
                this->_destruct(_p[n]);
                new (_p + n) T(std::move(_p[--_size]));
                this->_destruct(_p[_size]);
            } else {
                this->remove_back();
            }
        }
    }

    void swap(array& x) noexcept {
        std::swap(_cap, x._cap);
        std::swap(_size, x._size);
        std::swap(_p, x._p);
    }

    void swap(array&& x) noexcept {
        x.swap(*this);
    }

    class iterator {
      public:
        explicit iterator(T* p) noexcept : _p(p) {}
        T& operator*() const { return *_p; }
        bool operator==(const iterator& it) const noexcept { return _p == it._p; }
        bool operator!=(const iterator& it) const noexcept { return _p != it._p; }
        iterator& operator++() noexcept { ++_p; return *this; }
        iterator operator++(int) noexcept { return iterator(_p++); }
        iterator& operator--() noexcept { --_p; return *this; }
        iterator operator--(int) noexcept { return iterator(_p--); }

      private:
        T* _p;
    };

    iterator begin() const noexcept { return iterator(_p); }
    iterator end() const noexcept { return iterator(_p + _size); }

  private:
    template<typename X, god::enable_if_t<god::is_trivially_copyable<X>(), int> = 0>
    void _copy_n(X* dst, const X* src, size_t n) {
        memcpy(dst, src, sizeof(X) * n);
    }

    template<typename X, god::enable_if_t<!god::is_trivially_copyable<X>(), int> = 0>
    void _copy_n(X* dst, const X* src, size_t n) {
        for (size_t i = 0; i < n; ++i) new (dst + i) X(src[i]);
    }

    template<typename X, god::enable_if_t<god::is_trivially_copyable<X>(), int> = 0>
    void _move_n(X* dst, X* src, size_t n) {
        memcpy(dst, src, sizeof(X) * n);
    }

    template<typename X, god::enable_if_t<!god::is_trivially_copyable<X>(), int> = 0>
    void _move_n(X* dst, X* src, size_t n) {
        for (size_t i = 0; i < n; ++i) new (dst + i) X(std::move(src[i]));
    }

    template<typename X, god::enable_if_t<god::is_trivially_destructible<X>(), int> = 0>
    void _destruct(X&) {}

    template<typename X, god::enable_if_t<!god::is_trivially_destructible<X>(), int> = 0>
    void _destruct(X& p) { p.~X(); }

    template<typename X, god::enable_if_t<god::is_trivially_destructible<X>(), int> = 0>
    void _destruct_range(X*, size_t, size_t) {}

    template<typename X, god::enable_if_t<!god::is_trivially_destructible<X>(), int> = 0>
    void _destruct_range(X* p, size_t beg, size_t end) {
        for (; beg < end; ++beg) _p[beg].~X();
    }
  
  private:
    size_t _cap;
    size_t _size;
    T* _p;
};

} // co
