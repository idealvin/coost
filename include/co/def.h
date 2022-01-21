#pragma once

#include <stdint.h>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#ifdef _MSC_VER
#ifndef __thread
#define __thread __declspec(thread)
#endif
#else
#ifndef __forceinline 
#define __forceinline __attribute__((always_inline))
#endif
#endif

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

#if (defined(__GNUC__) && __GNUC__ >= 3) || defined(__clang__)
#define  unlikely(x)  __builtin_expect(!!(x), 0)
#else
#define  unlikely(x)  (x)
#endif

#include "config.h"

// __codef: define global variables
// __codec: declare classes, functions, global variables
// Do not use __codef and __codec outside of cocoyaxi.
#if COCOYAXI_SHARED > 0
  #ifdef _WIN32
    #ifdef BUILDING_CO_SHARED
      #define __codef __declspec(dllexport)
      #define __codec __declspec(dllexport)
    #else
      #define __codef
      #define __codec __declspec(dllimport)
    #endif
  #else
    #ifdef BUILDING_CO_SHARED
      #define __codef __attribute__((visibility("default")))
      #define __codec __attribute__((visibility("default")))
    #else
      #define __codef
      #define __codec __attribute__((visibility("default")))
    #endif
  #endif
#else
  #define __codef
  #define __codec
#endif
