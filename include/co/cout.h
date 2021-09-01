#pragma once

#include "fastream.h"
#include <mutex>

namespace co {
namespace xx {

class Cout {
  public:
    Cout() {
        this->mutex().lock();
    }

    Cout(const char* file, unsigned int line) {
        this->mutex().lock();
        this->stream() << file << ':' << line << ']' << ' ';
    }

    ~Cout() {
        auto& s = this->stream().append('\n');
        ::fwrite(s.data(), 1, s.size(), stderr);
        s.clear();
        this->mutex().unlock();
    }

    std::mutex& mutex() {
        static std::mutex kMtx;
        return kMtx;
    }

    fastream& stream() {
        static fastream kStream(128);
        return kStream;
    }
};

} // xx
} // co

#define COUT   co::xx::Cout().stream()
#define CLOG   co::xx::Cout(__FILE__, __LINE__).stream()
