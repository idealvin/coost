#pragma once

#ifdef _MSC_VER

typedef __int8  int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

#ifndef __thread
#define __thread __declspec( thread )
#endif

#else
#include <stdint.h>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#ifndef __forceinline
#define __forceinline __attribute__((always_inline))
#endif

#endif

#define load8(p)     (*(const uint8*) (p))
#define load16(p)    (*(const uint16*)(p))
#define load32(p)    (*(const uint32*)(p))
#define load64(p)    (*(const uint64*)(p))

#define save8(p, v)  (*(uint8*) (p) = (v))
#define save16(p, v) (*(uint16*)(p) = (v))
#define save32(p, v) (*(uint32*)(p) = (v))
#define save64(p, v) (*(uint64*)(p) = (v))

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

#define DISALLOW_COPY_AND_ASSIGN(Type) \
    Type(const Type&) = delete; \
    void operator=(const Type&) = delete

template<typename To, typename From>
inline To force_cast(From f) {
    return (To) f;
}

#if (defined(__GNUC__) && __GNUC__ >= 3) || defined(__clang__)
#define  unlikely(x)  __builtin_expect(!!(x), 0)
#else
#define  unlikely(x)  (x)
#endif
