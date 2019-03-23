// Copyright (c) 2019 Roland Bernard

#include <assert.h>

#include "cipher.h"
#include "hash.h"

#define ROTR(X, N) ((X >> (N)) | (X << (8-(N))))
#define ROTL(X, N) ((X << (N)) | (X >> (8-(N))))

void cipher_encryptblock(data256_t cipher, const data256_t plain, const data512_t key) {
	for(len_t i = 0; i < sizeof(data256_t); i++)
		cipher[i] = plain[i];
	for(len_t i = 0; i < sizeof(data256_t); i++) {
		cipher[i] = ROTL(cipher[i], key[i] >> 5);
		uint8_t tmp = cipher[i];
		cipher[i] = cipher[key[i] & 0x1F];
		cipher[key[i] & 0x1F] = tmp;
	}
	for(len_t i = 0; i < sizeof(data256_t); i++)
		cipher[i] ^= key[sizeof(data256_t)+i];
}

void cipher_decryptblock(data256_t cipher, const data256_t plain, const data512_t key) {
	for(len_t i = 0; i < sizeof(data256_t); i++)
		cipher[i] = plain[i];
	for(len_t i = 0; i < sizeof(data256_t); i++)
		cipher[i] ^= key[sizeof(data256_t)+i];
	for(len_t i = 0; i < sizeof(data256_t); i++) {
		len_t ind = sizeof(data256_t)-1-i;
		uint8_t tmp = cipher[ind];
		cipher[ind] = cipher[key[ind] & 0x1F];
		cipher[key[ind] & 0x1F] = tmp;
		cipher[ind] = ROTR(cipher[ind], key[ind] >> 5);
	}
}

len_t cipher_encryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key) {
	data512_t tmp[2];
	for(len_t i = 0; i < sizeof(data512_t); i++) {
		tmp[0][i] = indicator[i];
		tmp[1][i] = key[i];
	}
	hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	len_t i;
	for(i = 0; i < len+sizeof(len_t); i+=sizeof(data256_t)) {
		data256_t block;
		len_t j;
		for(j = 0; j < sizeof(data256_t) && i+j < len; j++)
			block[j] = in[i+j];
		if(j <= sizeof(data256_t)-sizeof(len_t)) {
			for(len_t k = 0; k < sizeof(len_t); k++)
				block[sizeof(data256_t)-1-k] = (uint8_t)(len >> (k*8));
		}
		cipher_encryptblock(block, block, tmp[0]);
		for(len_t j = 0; j < sizeof(data256_t); j++)
			out[i+j] = block[j];
		hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	}
	return i;
}

len_t cipher_decryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key) {
	/* assert(len%32 == 0); // to be a valid cipherstream */
	data512_t tmp[2];
	for(len_t i = 0; i < sizeof(data512_t); i++) {
		tmp[0][i] = indicator[i];
		tmp[1][i] = key[i];
	}
	hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	len_t i;
	for(i = 0; i < len; i+=sizeof(data256_t)) {
		data256_t block;
		len_t j;
		for(j = 0; j < sizeof(data256_t) && i+j < len; j++)
			block[j] = in[i+j];
		cipher_decryptblock(block, block, tmp[0]);
		for(len_t j = 0; j < sizeof(data256_t); j++)
			out[i+j] = block[j];
		hash_sha512(tmp[0], (char*)tmp, sizeof(tmp));
	}
	len_t rlen = 0;
	for(len_t j = 0; j < sizeof(len_t); j++)
		rlen |= (len_t)out[i-1-j] << (j*8);
	return rlen;
}
