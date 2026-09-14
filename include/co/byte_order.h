#pragma once

#include "def.h"

#if defined(__BYTE_ORDER__)
namespace co {

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
inline uint16 hton16(uint16 v) { return __builtin_bswap16(v); }
inline uint32 hton32(uint32 v) { return __builtin_bswap32(v); }
inline uint64 hton64(uint64 v) { return __builtin_bswap64(v); }
inline uint16 ntoh16(uint16 v) { return __builtin_bswap16(v); }
inline uint32 ntoh32(uint32 v) { return __builtin_bswap32(v); }
inline uint64 ntoh64(uint64 v) { return __builtin_bswap64(v); }

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline uint16 hton16(uint16 v) { return v; }
inline uint32 hton32(uint32 v) { return v; }
inline uint64 hton64(uint64 v) { return v; }
inline uint16 ntoh16(uint16 v) { return v; }
inline uint32 ntoh32(uint32 v) { return v; }
inline uint64 ntoh64(uint64 v) { return v; }

#else
#error "Unsupported endian"
#endif

} // co

#elif defined(_MSC_VER)
#include <stdlib.h>

namespace co {

// Microsoft says: all native scalar types are little-endian for the platforms
// that MS VC++ targets (x86, x64, ARM, ARM64).
// See https://learn.microsoft.com/en-us/cpp/standard-library/bit-enum for details.
inline uint16 hton16(uint16 v) { return _byteswap_ushort(v); }
inline uint32 hton32(uint32 v) { return _byteswap_ulong(v); }
inline uint64 hton64(uint64 v) { return _byteswap_uint64(v); }
inline uint16 ntoh16(uint16 v) { return _byteswap_ushort(v); }
inline uint32 ntoh32(uint32 v) { return _byteswap_ulong(v); }
inline uint64 ntoh64(uint64 v) { return _byteswap_uint64(v); }

} // co

#endif
