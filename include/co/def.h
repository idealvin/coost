#pragma once

#include <stdint.h>
#include <stddef.h>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define MAX_UINT8  ((uint8)  ~((uint8) 0))
#define MAX_UINT16 ((uint16) ~((uint16)0))
#define MAX_UINT32 ((uint32) ~((uint32)0))
#define MAX_UINT64 ((uint64) ~((uint64)0))

#define MAX_INT8   ((int8)  (MAX_UINT8  >> 1))
#define MAX_INT16  ((int16) (MAX_UINT16 >> 1))
#define MAX_INT32  ((int32) (MAX_UINT32 >> 1))
#define MAX_INT64  ((int64) (MAX_UINT64 >> 1))

#define MIN_INT8   ((int8)  ~MAX_INT8)
#define MIN_INT16  ((int16) ~MAX_INT16)
#define MIN_INT32  ((int32) ~MAX_INT32)
#define MIN_INT64  ((int64) ~MAX_INT64)

#define DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&) = delete; \
    void operator=(const T&) = delete

#if SIZE_MAX == UINT64_MAX
#define __arch64 1
#else
#define __arch32 1
#endif

#ifdef _MSC_VER
#ifndef __thread
#define __thread __declspec(thread)
#endif
#else
#ifndef __forceinline 
#define __forceinline __attribute__((always_inline))
#endif
#endif

#ifndef unlikely
#if (defined(__GNUC__) && __GNUC__ >= 3) || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) (x)
#endif
#endif

#define _CO_CONCAT(x, y) x##y
#define _CO_STRINGIFY(x) #x
#define CO_CONCAT(x, y) _CO_CONCAT(x, y)
#define CO_STRINGIFY(x) _CO_STRINGIFY(x)
#define CO_ANONYMOUS_VAR(x) CO_CONCAT(x, __LINE__)

// generated from config.h.in
#include "config.h"

// __coapi: used to export symbols in shared library
// Do not use (or reuse outside of coost) this definiton  yourself
#if COOST_SHARED > 0
  #ifdef _WIN32
    #ifdef BUILDING_CO_SHARED
      #define __coapi __declspec(dllexport)
    #else
      #define __coapi __declspec(dllimport)
    #endif
  #else
    #define __coapi __attribute__((visibility("default")))
  #endif
#else
  #define __coapi
#endif
