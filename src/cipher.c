// Copyright (c) 2019 Roland Bernard

#include <assert.h>

#include "cipher.h"
#include "hash.h"

#define CIPHER_ITER 16 // must be divisable by 2

static void cipher_apply(data512_t out, const data512_t in, const data512_t key, bool_t enc) {
	data256_t tmp;
	data256_t data[3];
	data256_t subkey[CIPHER_ITER];
	for(int i = 0; i < sizeof(data256_t); i++)
		data[0][i] = in[i];
	for(int i = 0; i < sizeof(data256_t); i++)
		data[2][i] = in[i+sizeof(data256_t)];
	// generate all subkeys
	for(int i = 0, s = sizeof(data256_t), o = 0; i < CIPHER_ITER; i++, o++) {
		if(o + s > sizeof(data256_t)) {
			o = 0;
			s--;
			if(s == 0)
				s = sizeof(data256_t);
		}
		hash_sha256(subkey[i], (char*)key+o, s);
	}
	for(int i = (enc ? 0 : CIPHER_ITER-1); (enc ? i < CIPHER_ITER : i >= 0 ); (enc ? i++ : i--)) {
		for(int j = 0; j < sizeof(data256_t); j++)
			data[1][j] = subkey[i][j];
		hash_sha256(tmp, (char*)data[i%2], 2*sizeof(data256_t));
		for(int j = 0; j < sizeof(data256_t); j++)
			data[((i+1)%2)<<1][j] ^= tmp[j];
	}

	for(int i = 0; i < sizeof(data256_t); i++)
		out[i] = data[0][i];
	for(int i = 0; i < sizeof(data256_t); i++)
		out[i+sizeof(data256_t)] = data[2][i];
}

void cipher_encryptblock(data512_t cipher, const data512_t plain, const data512_t key) {
	cipher_apply(cipher, plain, key, 1);
}

void cipher_decryptblock(data512_t plain, const data512_t cipher, const data512_t key) {
	cipher_apply(plain, cipher, key, 0);
}

len_t cipher_encryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key) {
	data512_t tmp[2];
	for(len_t i = 0; i < sizeof(data512_t); i++) {
		tmp[0][i] = indicator[i];
		tmp[1][i] = key[i];
	}
	hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	len_t i;
	for(i = 0; i < len+sizeof(len_t); i+=sizeof(data512_t)) {
		data512_t block;
		len_t j;
		for(j = 0; j < sizeof(data512_t) && i+j < len; j++)
			block[j] = in[i+j];
		if(j <= sizeof(data512_t)-sizeof(len_t)) {
			for(len_t k = 0; k < sizeof(len_t); k++)
				block[sizeof(data512_t)-1-k] = (uint8_t)(len >> (k*8));
		}
		cipher_encryptblock(block, block, tmp[0]);
		for(len_t j = 0; j < sizeof(data512_t); j++)
			out[i+j] = block[j];
		hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	}
	return i;
}

len_t cipher_decryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key) {
	/* assert(len%sizeof(data512_t) == 0); // to be a valid cipherstream */
	data512_t tmp[2];
	for(len_t i = 0; i < sizeof(data512_t); i++) {
		tmp[0][i] = indicator[i];
		tmp[1][i] = key[i];
	}
	hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	len_t i;
	for(i = 0; i < len; i+=sizeof(data512_t)) {
		data512_t block;
		len_t j;
		for(j = 0; j < sizeof(data512_t) && i+j < len; j++)
			block[j] = in[i+j];
		cipher_decryptblock(block, block, tmp[0]);
		for(len_t j = 0; j < sizeof(data512_t); j++)
			out[i+j] = block[j];
		hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	}
	len_t rlen = 0;
	for(len_t j = 0; j < sizeof(len_t); j++)
		rlen |= (len_t)out[i-1-j] << (j*8);
	return rlen;
}
