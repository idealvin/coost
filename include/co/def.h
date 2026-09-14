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

namespace co {

constexpr uint8  max_uint8  = (uint8)  ~((uint8) 0);
constexpr uint16 max_uint16 = (uint16) ~((uint16)0);
constexpr uint32 max_uint32 = (uint32) ~((uint32)0);
constexpr uint64 max_uint64 = (uint64) ~((uint64)0);
constexpr int8  max_int8  = (int8)  (max_uint8  >> 1);
constexpr int16 max_int16 = (int16) (max_uint16 >> 1);
constexpr int32 max_int32 = (int32) (max_uint32 >> 1);
constexpr int64 max_int64 = (int64) (max_uint64 >> 1);
constexpr int8  min_int8  = (int8)  ~max_int8;
constexpr int16 min_int16 = (int16) ~max_int16;
constexpr int32 min_int32 = (int32) ~max_int32;
constexpr int64 min_int64 = (int64) ~max_int64;

#if defined(__s390x__)
constexpr int cache_line_size = 256;
#elif defined(__powerpc64__) || defined(_M_PPC64)
constexpr int cache_line_size = 128;
#elif defined(__aarch64__) || defined(_M_ARM64)
constexpr int cache_line_size = 128;
#else
constexpr int cache_line_size = 64;
#endif

} // co

#if SIZE_MAX == UINT64_MAX
#define __arch64 1
#elif SIZE_MAX == UINT32_MAX
#define __arch32 1
#else
#error "platform not supported"
#endif

#ifndef __cacheline_aligned
#define __cacheline_aligned alignas(co::cache_line_size)
#endif

#ifndef _MSC_VER
#ifndef __forceinline
#define __forceinline __attribute__((always_inline))
#endif
#else
#ifndef __thread
#define __thread __declspec(thread)
#endif
#endif

#ifndef __unlikely
#if (defined(__GNUC__) && __GNUC__ >= 3) || defined(__clang__)
#define __unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define __unlikely(x) (x)
#endif
#endif
