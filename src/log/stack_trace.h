#pragma once

namespace ___ {
namespace log {

class StackTrace {
  public:
    /**
     * @param f     a pointer to fs::file
     * @param skip  number of frames to skip
     */
    #ifndef _WIN32
    virtual void dump_stack(void* f, int skip) = 0;
    #else
    virtual void dump_stack(void* f, int skip,void * connetx) = 0;
    #endif // win32
    
    
  protected:
    StackTrace() = default;
    virtual ~StackTrace() = default;
};

// return NULL if stack trace is not supported on that platform.
StackTrace* stack_trace();

} // log
} // ___
