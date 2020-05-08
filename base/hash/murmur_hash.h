/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#pragma once

#include "../def.h"
#include <stddef.h>

uint32 murmur_hash32(const void* data, size_t len, uint32 seed);
uint64 murmur_hash64(const void* data, size_t len, uint64 seed);
