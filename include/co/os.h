#pragma once

#include "fastring.h"
#include <signal.h>

namespace os {

// get value of an environment variable
fastring env(const char* name);

// set value of an environment variable
bool env(const char* name, const char* value);

/**
 * We try to use `/` as the path separator on all platforms. 
 * On windows, `\` in results of the following APIs will be converted to `/`, if 
 * the result does not start with `\\`. 
 *   - homedir()
 *   - cwd()
 *   - exepath()
 */

// get home dir of current user
fastring homedir();

// get current working directory
fastring cwd();

// get executable path
fastring exepath();

// get executable directory
fastring exedir();

// get executable name
fastring exename();

// get current process id
int pid();

// get number of processors
int cpunum();

// run as a daemon
void daemon();

typedef void (*sig_handler_t)(int);

// set a handler for the specified signal
// return the old handler.
sig_handler_t signal(int sig, sig_handler_t handler, int flag=0);

} // namespace os
