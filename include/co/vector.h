#pragma once

#include "god.h"
#include "mem.h"
#include <string.h>
#include <assert.h>
#include <initializer_list>

namespace co {

template<typename T>
class vector {
  public:
    constexpr vector() noexcept
        : _cap(0), _size(0), _p(0) {
    }

    // create an empty vector with capacity: @cap
    explicit vector(size_t cap)
        : _cap(cap), _size(0), _p((T*) co::alloc(sizeof(T) * cap)) {
    }

    // create an vector of n elements with value @x
    //   - cond: X is not int or T is int.
    //   - e.g. 
    //     co::vector<int> a(4, 3);     -> [3,3,3,3]
    //     co::vector<char> x(2, 'x');  -> ['x','x']
    template<typename X, god::if_t<
        !god::is_same<god::rm_cvref_t<X>, int>() ||
        god::is_same<god::rm_cv_t<T>, int>(), int
    > = 0>
    vector(size_t n, X&& x)
        : _cap(n), _size(n), _p((T*) co::alloc(sizeof(T) * n)) {
        for (size_t i = 0; i < n; ++i) new (_p + i) T(x);
    }

    // create an vector of n elements with default value
    //   - cond: X is int and T is not int.
    //   - e.g. 
    //     co::vector<fastring> a(4, 0);
    template<typename X, god::if_t<
        god::is_same<god::rm_cvref_t<X>, int>() &&
        !god::is_same<god::rm_cv_t<T>, int>(), int
    > = 0>
    vector(size_t n, X&&)
        : _cap(n), _size(n), _p((T*) co::alloc(sizeof(T) * n)) {
        for (size_t i = 0; i < n; ++i) new (_p + i) T();
    }

    vector(const vector& x)
        : _cap(x.size()), _size(_cap) {
        _p = (T*) co::alloc(sizeof(T) * _cap);
        this->_copy_n(_p, x._p, _cap);
    }

    vector(vector&& x) noexcept
        : _cap(x._cap), _size(x._size), _p(x._p) {
        x._p = 0;
        x._cap = x._size = 0;
    }

    // create vector from an initializer list
    vector(std::initializer_list<T> x)
        : _cap(x.size()), _size(0), _p((T*) co::alloc(sizeof(T) * _cap)) {
        for (const auto& e : x) new (_p + _size++) T(e);
    }

    template<typename It, god::if_t<god::is_class<It>(), int> = 0>
    vector(It beg, It end) : vector(8) {
        this->append(beg, end);
    }

    // create vector from an vector
    vector(T* p, size_t n)
        : _cap(n), _size(n), _p((T*) co::alloc(sizeof(T) * n)) {
        this->_copy_n(_p, p, n);
    }

    ~vector() { this->reset(); }

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

    vector& operator=(const vector& x) {
        if (&x != this) {
            this->reset();
            new (this) vector(x);
        }
        return *this;
    }

    vector& operator=(vector&& x) {
        if (&x != this) {
            this->reset();
            new (this) vector(std::move(x));
        }
        return *this;
    }

    vector& operator=(std::initializer_list<T> x) {
        this->reset();
        new (this) vector(x);
        return *this;
    }

    void reserve(size_t n) {
        if (_cap < n) {
            _p = this->_realloc(_p, sizeof(T) * _cap, sizeof(T) * n); assert(_p);
            _cap = n;
        }
    }

    void resize(size_t n) {
        this->reserve(n);
        this->_destruct_range(_p, n, _size);
        this->_construct_range(_p, _size, n);
        _size = n;
    }

    // destroy all elements and free the memory
    void reset() {
        if (_p) {
            this->_destruct_range(_p, 0, _size);
            co::free(_p, sizeof(T) * _cap);
            _p = 0;
            _cap = _size = 0;
        }
    }

    void clear() {
        this->_destruct_range(_p, 0, _size);
        _size = 0;
    }

    void append(const T& x) {
        this->_realloc_if_no_more_memory();
        new (_p + _size++) T(x);
    }

    void append(T&& x) {
        this->_realloc_if_no_more_memory();
        new (_p + _size++) T(std::move(x));
    }

    void append(size_t n, const T& x) {
        const size_t m = n + _size;
        this->reserve(m);
        for (size_t i = _size; i < m; ++i) new (_p + i) T(x);
        _size += n;
    }

    template<typename It, god::if_t<god::is_class<It>(), int> = 0>
    void append(It beg, It end) {
        for (auto it = beg; it != end; ++it) this->append(*it);
    }

    // append n elements
    void append(const T* p, size_t n) {
        if (p < _p || p >= _p + _size) {
            this->reserve(_size + n);
            this->_copy_n(_p + _size, p, n);
            _size += n;
        } else {
            assert(p + n <= _p + _size);
            const size_t x = p - _p;
            this->reserve(_size + n);
            this->_copy_n(_p + _size, _p + x, n);
            _size += n;
        }
    }

    // append an vector, &x == this is ok.
    void append(const vector& x) {
        if (&x != this) {
            this->append(x.data(), x.size());
        } else {
            this->reserve(_size << 1);
            this->_copy_n(_p + _size, _p, _size);
            _size <<= 1;
        }
    }

    // append an vector, elements in @x will be moved to the end of this vector
    void append(vector&& x) {
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

    // insert a new element (construct with args x...) at the back
    //   - e.g. 
    //     co::vector<fastring> x; x.emplace_back(4, 'x'); // x.back() -> "xxxx"
    template<typename ... X>
    void emplace_back(X&& ... x) {
        this->_realloc_if_no_more_memory();
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
        if (_size > 0) this->_destruct(_p[--_size]);
    }

    // remove the nth element, and move the last element to the nth position
    void remove(size_t n) {
        if (n < _size) {
            if (n != _size - 1) {
                this->_destruct(_p[n]);
                new (_p + n) T(std::move(_p[--_size]));
                this->_destruct(_p[_size]);
            } else {
                this->_destruct(_p[--_size]);
            }
        }
    }

    void swap(vector& x) noexcept {
        std::swap(_cap, x._cap);
        std::swap(_size, x._size);
        std::swap(_p, x._p);
    }

    void swap(vector&& x) noexcept {
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
    void _realloc_if_no_more_memory() {
        if (unlikely(_cap == _size)) {
            const size_t cap = _cap;
            _cap += (_cap >> 1) + 1;
            _p = this->_realloc(_p, sizeof(T) * cap, sizeof(T) * _cap); assert(_p);
        }
    }

    template<typename X, god::if_t<god::is_trivially_copyable<X>(), int> = 0>
    X* _realloc(X* p, size_t o, size_t n) {
        return (X*) co::realloc(p, o, n);
    }

    template<typename X, god::if_t<!god::is_trivially_copyable<X>(), int> = 0>
    X* _realloc(X* p, size_t o, size_t n) {
        X* x = (X*) co::try_realloc(p, o, n);
        if (!x) {
            x = (X*) co::alloc(n);
            this->_move_n(x, p, _size);
            this->_destruct_range(p, 0, _size);
            co::free(p, o);
        }
        return x;
    }

    template<typename X, god::if_t<god::is_trivially_copyable<X>(), int> = 0>
    void _copy_n(X* dst, const X* src, size_t n) {
        memcpy(dst, src, sizeof(X) * n);
    }

    template<typename X, god::if_t<!god::is_trivially_copyable<X>(), int> = 0>
    void _copy_n(X* dst, const X* src, size_t n) {
        for (size_t i = 0; i < n; ++i) new (dst + i) X(src[i]);
    }

    template<typename X, god::if_t<god::is_trivially_copyable<X>(), int> = 0>
    void _move_n(X* dst, X* src, size_t n) {
        memcpy(dst, src, sizeof(X) * n);
    }

    template<typename X, god::if_t<!god::is_trivially_copyable<X>(), int> = 0>
    void _move_n(X* dst, X* src, size_t n) {
        for (size_t i = 0; i < n; ++i) new (dst + i) X(std::move(src[i]));
    }

    template<typename X, god::if_t<!god::is_class<X>(), int> = 0>
    void _construct_range(X* p, size_t beg, size_t end) {
        if (beg < end) memset(p + beg, 0, (end - beg) * sizeof(X));
    }

    template<typename X, god::if_t<god::is_class<X>(), int> = 0>
    void _construct_range(X* p, size_t beg, size_t end) {
        for (; beg < end; ++beg) new (p + beg) X();
    }

    template<typename X, god::if_t<god::is_trivially_destructible<X>(), int> = 0>
    void _destruct(X&) {}

    template<typename X, god::if_t<!god::is_trivially_destructible<X>(), int> = 0>
    void _destruct(X& p) { p.~X(); }

    template<typename X, god::if_t<god::is_trivially_destructible<X>(), int> = 0>
    void _destruct_range(X*, size_t, size_t) {}

    template<typename X, god::if_t<!god::is_trivially_destructible<X>(), int> = 0>
    void _destruct_range(X* p, size_t beg, size_t end) {
        for (; beg < end; ++beg) p[beg].~X();
    }
  
  private:
    size_t _cap;
    size_t _size;
    T* _p;
};

} // co
