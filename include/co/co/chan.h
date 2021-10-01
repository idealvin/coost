#pragma once

#include "../def.h"

namespace co {
namespace xx {

class __codec Pipe {
  public:
    Pipe(uint32 buf_size, uint32 blk_size, uint32 ms);
    ~Pipe();

    Pipe(Pipe&& p) : _p(p._p) {
        p._p = 0;
    }

    // copy constructor, allow co::Pipe to be captured by value in lambda.
    Pipe(const Pipe& p) : _p(p._p) {
        atomic_inc(_p);
    }

    void operator=(const Pipe&) = delete;

    // read a block
    void read(void* p) const;

    // write a block
    void write(const void* p) const;
  
  private:
    uint32* _p;
};

} // xx

template <typename T>
class Chan {
  public:
    /**
     * @param cap  max capacity of the queue, 1 by default.
     * @param ms   default timeout in milliseconds, -1 by default.
     */
    explicit Chan(uint32 cap=1, uint32 ms=(uint32)-1)
        : _p(cap * sizeof(T), sizeof(T), ms) {
    }

    ~Chan() = default;

    Chan(Chan&& c) : _p(std::move(c._p)) {}

    Chan(const Chan& c) : _p(c._p) {}

    void operator=(const Chan&) = delete;

    void operator<<(const T& x) const {
        _p.write(&x);
    }

    void operator>>(T& x) const {
        _p.read(&x);
    }

  private:
    xx::Pipe _p;
};

} // co
