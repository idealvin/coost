#pragma once

#include "fastring.h"
#include <signal.h>

namespace os {

// get value of an environment variable
__codec fastring env(const char* name);

// set value of an environment variable
__codec bool env(const char* name, const char* value);

/**
 * We try to use `/` as the path separator on all platforms. 
 * On windows, `\` in results of the following APIs will be converted to `/`, if 
 * the result does not start with `\\`. 
 *   - homedir()
 *   - cwd()
 *   - exepath()
 */

// get home dir of current user
__codec fastring homedir();

// get current working directory
__codec fastring cwd();

// get executable path
__codec fastring exepath();

// get executable directory
__codec fastring exedir();

// get executable name
__codec fastring exename();

// get current process id
__codec int pid();

// get number of processors
__codec int cpunum();

// run as a daemon
__codec void daemon();

typedef void (*sig_handler_t)(int);

// set a handler for the specified signal
// return the old handler.
__codec sig_handler_t signal(int sig, sig_handler_t handler, int flag = 0);

// execute a shell command
__codec bool system(const char* cmd);

inline bool system(const fastring& cmd) {
    return os::system(cmd.c_str());
}

} // namespace os
