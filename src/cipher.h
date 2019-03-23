// Copyright (c9 2019 Roland Bernard
#ifndef __CIPHER_H__
#define __CIPHER_H__

#include "types.h"

void cipher_encryptblock(data256_t cipher, const data256_t plain, const data512_t key);

void cipher_decryptblock(data256_t cipher, const data256_t plain, const data512_t key);

len_t cipher_encryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key);

len_t cipher_decryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key);

#endif
