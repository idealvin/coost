#pragma once

#ifdef _WIN32
#include "win/atomic.h"
#else
#include "unix/atomic.h"
#endif
