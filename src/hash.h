// Copyright (c) 2019 Roland Bernard
#ifndef __HASH_H__
#define __HASH_H__

#include "types.h"

hash_t hash_crc(const char* data, len_t size, const hash_t* table);

hash_t hash_fnv(const char* data, len_t size);

void hash_sha256(hash256_t ret, const char* data, len_t size);

void hash_sha512(hash512_t ret, const char* data, len_t size);

#endif
