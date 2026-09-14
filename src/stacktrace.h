#pragma once

#include <stddef.h>

namespace co {

struct StackTrace {
    StackTrace();
    ~StackTrace();

    // on windows, @p is a pointer to CONTEXT
    // for other platforms, @p is number of frames to skip
    void dump_stack(void(*f)(const char*, size_t), size_t p);

    void* _p;
};

} // co
