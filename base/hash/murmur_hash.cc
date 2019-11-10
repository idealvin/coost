/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#include "murmur_hash.h"

// Beware of alignment and endian-ness issues if used across multiple platforms.
uint64 murmur_hash64(const void* key, size_t len, uint64 seed) {
    const uint64 m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    uint64 h = seed ^ (len * m);

    const uint64* p = (const uint64*) key;
    const uint64* end = p + (len >> 3); // len / 8

    while (p != end) {
        uint64 k = *p++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    const uint8* p2 = (const uint8*) p;

    switch (len & 7) {
      case 7:
        h ^= ((uint64) p2[6]) << 48;
      case 6:
        h ^= ((uint64) p2[5]) << 40;
      case 5:
        h ^= ((uint64) p2[4]) << 32;
      case 4:
        h ^= ((uint64) p2[3]) << 24;
      case 3:
        h ^= ((uint64) p2[2]) << 16;
      case 2:
        h ^= ((uint64) p2[1]) << 8;
      case 1:
        h ^= ((uint64) p2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}
