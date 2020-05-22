#pragma once

#include "fastring.h"

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

} // namespace os
