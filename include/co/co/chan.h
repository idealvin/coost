#pragma once

#include "../def.h"
#include "../god.h"
#include "../atomic.h"
#include <functional>

namespace co {
namespace xx {

class __coapi pipe {
  public:
    typedef std::function<void(void*, void*, int)> F;
    pipe(uint32 buf_size, uint32 blk_size, uint32 ms, F&& f);
    ~pipe();

    pipe(pipe&& p) : _p(p._p) {
        p._p = 0;
    }

    // copy constructor, allow co::pipe to be captured by value in lambda.
    pipe(const pipe& p) : _p(p._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const pipe&) = delete;

    // read a block
    void read(void* p, int v) const;

    // write a block
    void write(void* p, int v) const;

    // return true if the read or write operation timed out
    bool timeout() const;
  
  private:
    uint32* _p;
};

} // xx

template <typename T>
class chan {
  public:
    /**
     * @param cap  max capacity of the queue, 1 by default.
     * @param ms   default timeout in milliseconds, -1 by default.
     */
    explicit chan(uint32 cap=1, uint32 ms=(uint32)-1)
        : _p(cap * sizeof(T), sizeof(T), ms,
          [](void* dst, void* src, int v) {
              switch (v) {
                case 0:
                  new (dst) T(*static_cast<const T*>(src));
                  break;
                case 1:
                  new (dst) T(std::move(*static_cast<T*>(src)));
                  break;
                case 2:
                  static_cast<T*>(dst)->~T();
                  new (dst) T(std::move(*static_cast<T*>(src)));
                  static_cast<T*>(src)->~T();
                  break;
              }
          }) {
    }

    ~chan() = default;

    chan(chan&& c) : _p(std::move(c._p)) {}

    chan(const chan& c) : _p(c._p) {}

    void operator=(const chan&) = delete;

    void operator<<(const T& x) const {
        _p.write((void*)&x, 0);
    }

    void operator<<(T&& x) const {
        _p.write((void*)&x, 1);
    }

    void operator>>(T& x) const {
        _p.read((void*)&x, 2);
    }

    // return true if the read or write operation timed out
    bool timeout() const {
        return _p.timeout();
    }

  private:
    xx::pipe _p;
};

template <typename T>
using Chan = chan<T>;

} // co
