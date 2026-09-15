#pragma once

#include "string.h"
#include <signal.h>

namespace os {

// get value of environment variable
co::string env(const char* name);

// set value of environment variable
bool env(const char* name, const char* value);

// get home dir of current user
co::string homedir();

// current working directory
co::string cwd();

// executable path
co::string exepath();

// executable directory
co::string exedir();

// executable name
co::string exename();

// current process id
int pid();

// number of CPU cores
int cpunum();

// get size of a page in bytes
size_t pagesize();

typedef void (*sig_handler_t)(int);

// set signal handler, return the old handler
sig_handler_t signal(int sig, sig_handler_t handler, int flag=0);

// execute a shell command
bool system(const char* cmd);

inline bool system(const co::string& cmd) {
    return os::system(cmd.c_str());
}

} // namespace os
