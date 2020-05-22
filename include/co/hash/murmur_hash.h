/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

uint32_t murmur_hash32(const void* data, size_t len, uint32_t seed);
uint64_t murmur_hash64(const void* data, size_t len, uint64_t seed);
