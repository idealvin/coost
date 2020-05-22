/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#include "co/hash/murmur_hash.h"

//-----------------------------------------------------------------------------
//
// Note - This code makes a few assumptions about how your machine behaves -
//
// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4
//
// And it has a few limitations -
//
// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian machines.

uint32_t murmur_hash32(const void* key, uint32_t len, uint32_t seed) {
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value
    uint32_t h = seed ^ len;

    // Mix 4 bytes at a time into the hash
    const uint8_t* p = (const uint8_t*) key;

    while (len >= 4) {
        uint32_t k = *((uint32_t*)p);
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        p += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch (len) {
      case 3:
        h ^= p[2] << 16;
      case 2:
        h ^= p[1] << 8;
      case 1:
        h ^= p[0];
        h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// -------------------------------------------------------------------
//
// The same caveats as 32-bit MurmurHash2 apply here - beware of alignment
// and endian-ness issues if used across multiple platforms.
//
// 64-bit hash for 64-bit platforms

uint64_t murmur_hash64(const void* key, size_t len, uint64_t seed) {
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* p = (const uint64_t*) key;
    const uint64_t* end = p + (len >> 3); // len / 8

    while (p != end) {
        uint64_t k = *p++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    const uint8_t* p2 = (const uint8_t*) p;

    switch (len & 7) {
      case 7:
        h ^= ((uint64_t) p2[6]) << 48;
      case 6:
        h ^= ((uint64_t) p2[5]) << 40;
      case 5:
        h ^= ((uint64_t) p2[4]) << 32;
      case 4:
        h ^= ((uint64_t) p2[3]) << 24;
      case 3:
        h ^= ((uint64_t) p2[2]) << 16;
      case 2:
        h ^= ((uint64_t) p2[1]) << 8;
      case 1:
        h ^= ((uint64_t) p2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}
