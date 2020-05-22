#pragma once

class StackTrace {
  public:
    StackTrace() = default;
    virtual ~StackTrace() = default;

    // @f is a pointer to fs::File
    virtual void set_file(void* f) = 0;

    // @cb will be called before the failure handler starts to work.
    virtual void set_callback(void (*cb)()) = 0;
};

StackTrace* new_stack_trace();
