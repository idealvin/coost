#pragma once

// basic types and macros
#include "def.h"

// runtime_assert
#include "assert.h"

// align_up & align_down
#include "align.h"

// co::error, co::strerror
#include "error.h"

// intrusive linked list
#include "clist.h"

// memory allocator
#include "mem.h"

// Go-style defer for C++
#include "defer.h"

// non-template closure without return value
#include "closure.h"

// co::string and string utilities
#include "string.h"

// STL containers using the optimized allocator
#include "stl.h"

// Go-style path manipulation
#include "path.h"

// JSON library with a fluent interface
#include "json.h"

// base64 / md5 / sha256
#include "base64.h"
#include "md5.h"
#include "sha256.h"

// random numbers and strings
#include "rand.h"

// time utilities
#include "time.h"

// OS utilities
#include "os.h"

// filesystem operations
#include "fs.h"

// thread-safe console output
#include "print.h"

// command-line and config parser
#include "flag.h"

// logging
#include "log.h"

// scheduled task
#include "tasked.h"

// atomic operations & memory order
#include "atomic.h"

// co::thread_id & co::sync_event
#include "thread.h"

// coroutines and socket APIs
#include "co.h"

// co::tcp_client & co::tcp_server
#include "tcp.h"

// RPC
#include "rpc.h"

// benchmarking
#include "benchmark.h"

// unit testing
#include "unitest.h"
