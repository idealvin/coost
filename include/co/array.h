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
    array() noexcept
        : _cap(0), _size(0), _p(0) {
    }

    /**
     * constructor with a capacity
     *   - NOTE: size of the array will be 0, which is different from std::vector
     * 
     * @param cap  capacity of the array.
     */
    explicit array(size_t cap)
        : _cap(cap), _size(0), _p((T*) Alloc::alloc(sizeof(T) * cap)) {
    }

    array(size_t n, const T& x)
        : _cap(n), _size(n), _p((T*) Alloc::alloc(sizeof(T) * n)) {
        for (size_t i = 0; i < n; ++i) new (_p + i) T(x);
    }

    array(const array& x) {
        this->_make_array(x, B<god::is_trivially_copyable<T>()>());
    }

    array(array&& x) noexcept
        : _cap(x._cap), _size(x._size), _p(x._p) {
        x._p = 0;
        x._cap = x._size = 0;
    }

    // co::array<int> v = { 1, 2, 3 };
    array(std::initializer_list<T> x)
        : _cap(x.size()), _size(0), _p((T*) Alloc::alloc(sizeof(T) * _cap)) {
        for (const auto& e : x) new (_p + _size++) T(e);
    }

    template <typename It, god::enable_if_t<god::is_class<It>(), int> = 0>
    array(It beg, It end) : array(8) {
        this->push_back(beg, end);
    }

    // create array from an array
    array(T* p, size_t n) : array(n) {
        this->_push_back(p, n, B<god::is_trivially_copyable<T>()>());
    }

    ~array() {
        this->reset();
    }

    size_t capacity() const { return _cap; }
    size_t size() const { return _size; }
    T* data() const { return _p; }
    bool empty() const { return this->size() == 0; }

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
        this->_resize(n, B<god::is_trivially_destructible<T>()>());
    }

    // destroy all elements and free the memory
    void reset() {
        this->_reset(B<god::is_trivially_destructible<T>()>());
    }

    void clear() {
        this->_resize(0, B<god::is_trivially_destructible<T>()>());
    }

    void push_back(const T& x) {
        if (unlikely(_cap == _size)) {
            const size_t cap = _cap;
            _cap += (_cap >> 1) + 1;
            _p = (T*) Alloc::realloc(_p, sizeof(T) * cap, sizeof(T) * _cap); assert(_p);
        }
        new (_p + _size++) T(x);
    }

    void push_back(T&& x) {
        if (unlikely(_cap == _size)) {
            const size_t cap = _cap;
            _cap += (_cap >> 1) + 1;
            _p = (T*) Alloc::realloc(_p, sizeof(T) * cap, sizeof(T) * _cap); assert(_p);
        }
        new (_p + _size++) T(std::move(x));
    }

    void push_back(size_t n, const T& x) {
        const size_t m = n + _size;
        this->reserve(m);
        for (size_t i = _size; i < m; ++i) new (_p + i) T(x);
        _size += n;
    }

    template <typename It, god::enable_if_t<god::is_class<It>(), int> = 0>
    void push_back(It beg, It end) {
        for (auto it = beg; it != end; ++it) {
            this->push_back(*it);
        }
    }

    // append n elements from an array
    void push_back(T* p, size_t n) {
        this->reserve(_size + n);
        this->_push_back(p, n, B<god::is_trivially_copyable<T>()>());
    }

    T pop_back() {
        return std::move(_p[--_size]);
    }

    // remove the last element
    void remove_back() {
        this->_remove_back(B<god::is_trivially_destructible<T>()>());
    }

    /**
     * remove the nth element, and move the last element to the nth position
     *  - NOTE: n MUST < size
     */
    void remove(size_t n) {
        assert(n < _size);
        if (n != _size - 1) {
            this->_remove(n, B<god::is_trivially_destructible<T>()>());
        } else {
            this->remove_back();
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
        explicit iterator(T* p) : _p(p) {}

        T& operator*() const {
            return *_p;
        }

        bool operator==(iterator it) const {
            return _p == it._p;
        }

        bool operator!=(iterator it) const {
            return _p != it._p;
        }

        iterator& operator++() {
            ++_p;
            return *this;
        }

        iterator operator++(int) {
            return iterator(_p++);
        }

        iterator& operator--() {
            --_p;
            return *this;
        }

        iterator operator--(int) {
            return iterator(_p--);
        }

      private:
        T* _p;
    };

    iterator begin() const {
        return iterator(_p);
    }

    iterator end() const {
        return iterator(_p + _size);
    }

  private:
    template<bool> struct B {};

    void _make_array(const array& x, B<true>) {
        _cap = _size = x.size();
        const size_t n = sizeof(T) * _cap;
        _p = (T*) Alloc::alloc(n);
        memcpy(_p, x._p, n);
    }

    void _make_array(const array& x, B<false>) {
        _cap = _size = x.size();
        _p = (T*) Alloc::alloc(sizeof(T) * _cap);
        for (size_t i = 0; i < _cap; ++i) new (_p + i) T(x[i]);
    }

    void _push_back(T* p, size_t n, B<true>) {
        memcpy(_p + _size, p, n * sizeof(T));
        _size += n;
    }

    void _push_back(T* p, size_t n, B<false>) {
        T* const x = _p + _size;
        for (size_t i = 0; i < n; ++i) new (x + i) T(p[i]);
        _size += n;
    }

    void _resize(size_t n, B<true>) {
        _size = n;
    }

    void _resize(size_t n, B<false>) {
        for (size_t i = n; i < _size; ++i) _p[i].~T();
        _size = n;
    }

    void _reset(B<true>) {
        if (_p) {
            Alloc::free(_p, sizeof(T) * _cap); _p = 0;
            _cap = _size = 0;
        }
    }

    void _reset(B<false>) {
        if (_p) {
            for (size_t i = 0; i < _size; ++i) _p[i].~T();
            Alloc::free(_p, sizeof(T) * _cap); _p = 0;
            _cap = _size = 0;
        }
    }

    void _remove_back(B<true>) {
        --_size;
    }

    void _remove_back(B<false>) {
        _p[--_size].~T();
    }

    void _remove(size_t n, B<true>) {
        new (_p + n) T(std::move(_p[--_size]));
    }

    void _remove(size_t n, B<false>) {
        _p[n].~T();
        new (_p + n) T(std::move(_p[--_size]));
    }

  private:
    size_t _cap;
    size_t _size;
    T* _p;
};

} // co
