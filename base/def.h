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
#define __thread __declspec(thread)
#else
#define __forceinline __attribute__((always_inline))
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

// Borrowed from ruki's tbox(https://github.com/tboox/tbox).
// See details in co/context/arch.h.
#if defined(__LP64__) || \
    defined(__64BIT__) || \
    defined(_LP64) || \
    defined(__x86_64) || \
    defined(__x86_64__) || \
    defined(__amd64) || \
    defined(__amd64__) || \
    defined(__arm64) || \
    defined(__arm64__) || \
    defined(__sparc64__) || \
    defined(__PPC64__) || \
    defined(__ppc64__) || \
    defined(__powerpc64__) || \
    defined(_M_X64) || \
    defined(_M_AMD64) || \
    defined(_M_IA64) || \
    (defined(__WORDSIZE) && (__WORDSIZE == 64)) 
  #define ARCH64
#else
  #define ARCH32
#endif
