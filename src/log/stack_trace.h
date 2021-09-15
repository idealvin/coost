#pragma once

namespace co {

class StackTrace {
  public:
    StackTrace() = default;
    virtual ~StackTrace() = default;

    /**
     * @param f     a pointer to fs::file
     * @param skip  number of frames to skip
     */
    virtual void dump_stack(void* f, int skip) = 0;
};

// return NULL if stack trace is not supported on that platform.
StackTrace* new_stack_trace();

} // co
