#pragma once

#include "fastring.h"
#include <signal.h>

namespace os {

// get value of an environment variable
__coapi fastring env(const char* name);

// set value of an environment variable
__coapi bool env(const char* name, const char* value);

/**
 * We try to use `/` as the path separator on all platforms. 
 * On windows, `\` in results of the following APIs will be converted to `/`, if 
 * the result does not start with `\\`. 
 *   - homedir()
 *   - cwd()
 *   - exepath()
 */

// get home dir of current user
__coapi fastring homedir();

// get current working directory
__coapi fastring cwd();

// get executable path
__coapi fastring exepath();

// get executable directory
__coapi fastring exedir();

// get executable name
__coapi fastring exename();

// get current process id
__coapi int pid();

// get number of processors
__coapi int cpunum();

// run as a daemon
__coapi void daemon();

typedef void (*sig_handler_t)(int);

// set a handler for the specified signal
// return the old handler.
__coapi sig_handler_t signal(int sig, sig_handler_t handler, int flag = 0);

// execute a shell command
__coapi bool system(const char* cmd);

inline bool system(const fastring& cmd) {
    return os::system(cmd.c_str());
}

} // namespace os
