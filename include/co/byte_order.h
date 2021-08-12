#pragma once

#ifdef _WIN32
#include <WinSock2.h>

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#else /* mingw */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  #define htonll(x) __builtin_bswap64(x)
  #define ntohll(x) __builtin_bswap64(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  #define htonll(x) x
  #define ntohll(x) x
#else
  #error "Unsupported endian"
#endif
#endif

inline unsigned short hton16(unsigned short v) {
    return htons(v);
}

inline unsigned int hton32(unsigned int v) {
    return htonl(v);
}

inline unsigned short ntoh16(unsigned short v) {
    return ntohs(v);
}

inline unsigned int ntoh32(unsigned int v) {
    return ntohl(v);
}

inline unsigned long long hton64(unsigned long long v) {
    return htonll(v);
}

inline unsigned long long ntoh64(unsigned long long v) {
    return ntohll(v);
}

#else /* linux, mac */
#include <stdint.h>

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>

#define htobe16 OSSwapHostToBigInt16
#define htobe32 OSSwapHostToBigInt32
#define htobe64 OSSwapHostToBigInt64
#define be16toh OSSwapBigToHostInt16
#define be32toh OSSwapBigToHostInt32
#define be64toh OSSwapBigToHostInt64

#elif defined(__linux__)
#include <endian.h>
#else
#include <sys/endian.h>
#endif

inline uint16_t hton16(uint16_t v) {
    return htobe16(v);
}

inline uint32_t hton32(uint32_t v) {
    return htobe32(v);
}

inline uint64_t hton64(uint64_t v) {
    return htobe64(v);
}

inline uint16_t ntoh16(uint16_t v) {
    return be16toh(v);
}

inline uint32_t ntoh32(uint32_t v) {
    return be32toh(v);
}

inline uint64_t ntoh64(uint64_t v) {
    return be64toh(v);
}

#endif
