#pragma once

#ifdef _WIN32
#include "win/thread.h"
#else
#include "unix/thread.h"
#endif
