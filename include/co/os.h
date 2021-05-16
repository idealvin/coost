#pragma once

#include "fastring.h"
#include <signal.h>

namespace os {

// os::env("HOME")
fastring env(const char* name);

// get home dir of current user
fastring homedir();

// get current working directory
fastring cwd();

// get executable path
fastring exepath();

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
sig_handler_t set_sig_handler(int sig, sig_handler_t handler, int flag=0);

} // namespace os
