// Copyright (c) 2019 Roland Bernard
#ifndef __HASH_H__
#define __HASH_H__

#include "types.h"

hash32_t hash_crc32(const char* data, len_t size, const hash32_t* table);

hash32_t hash_fnv_1a32(const char* data, len_t size);

void hash_sha256(hash256_t ret, const char* data, len_t size);

void hash_sha512(hash512_t ret, const char* data, len_t size);

#endif
