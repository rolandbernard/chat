// Copyright (c) 2019 Roland Bernard

/*
 *     +---|    in    |---+
 *     |                  |
 *     v                  v
 *  | in1 |            | in2 |
 *     |                  |
 *     v                  |
 *    xor<--sha256(k1)<---+
 *     |                  |
 *     |                  v
 *     +--->sha256(k2)-->xor
 *     |                  |
 *     v                  |
 *    xor<--sha256(k3)<---+
 *     |                  |
 *     :                  :
 *     :                  :
 *     |                  |
 *     +--->sha256(kn)-->xor
 *     |                  |
 *  | out1 |           | out2 |
 *     |                  |
 *     +-->|    out   |<--+
 *
 * */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "cipher.h"
#include "hash.h"
#include "random.h"

#define CIPHER_ITER 16
#define RAND_PADDING 4

static void cipher_apply(data512_t out, const data512_t in, const data512_t key, bool_t enc) {
    data256_t data[3];
    data256_t subkey[CIPHER_ITER] = { { 0 } };
    // copy data for manipulation
    for(int i = 0; i < sizeof(data256_t); i++) {
        data[0][i] = in[i];
        data[2][i] = in[i+sizeof(data256_t)];
        subkey[0][i] = key[i];
        subkey[1][i] = key[i+sizeof(data256_t)];
    }
#ifndef DUMMY_CIPHER
    // generate all missing subkeys
    for(int i = 2; i < CIPHER_ITER; i++) {
        hash_sha256(subkey[i], (uint8_t*)subkey, sizeof(subkey));
    }
    // apply the encryption
    for(int i = (enc ? 0 : CIPHER_ITER-1); (enc ? i < CIPHER_ITER : i >= 0 ); (enc ? i++ : i--)) {
        data256_t tmp;
        for(int j = 0; j < sizeof(data256_t); j++) {
            data[1][j] = subkey[i][j];
        }
        hash_sha256(tmp, (uint8_t*)data[i%2], 2*sizeof(data256_t));
        for(int j = 0; j < sizeof(data256_t); j++) {
            // Select eighter data[0] or data[2] and xor tmp onto it
            data[((i+1)%2)<<1][j] ^= tmp[j];
        }
    }
#endif
    // write the result to output
    for(int i = 0; i < sizeof(data256_t); i++) {
        out[i] = data[0][i];
        out[i+sizeof(data256_t)] = data[2][i];
    }
}

void cipher_encryptblock(data512_t cipher, const data512_t plain, const data512_t key) {
    cipher_apply(cipher, plain, key, 1);
}

void cipher_decryptblock(data512_t plain, const data512_t cipher, const data512_t key) {
    cipher_apply(plain, cipher, key, 0);
}

len_t cipher_encryptdata(uint8_t* out, const uint8_t* inin, len_t len, const data512_t indicator, const data512_t key) {
    const uint8_t* in;
    uint8_t* newin = NULL;
    if(out+len >= inin && inin+len >= out) {
        // copy input to enable in and out to overlap
        newin = (uint8_t*)malloc(len);
        memcpy(newin, inin, len);
        in = newin;
    } else {
        in = inin;
    }
    data512_t tmp[4] = { { 0 } }; // 0 - current block key, 1 - first half of key, 2 - second half of key, 3 - current block to encrypt
    for(len_t i = 0; i < sizeof(data512_t); i++) {
        tmp[1][i] = indicator[i];
        tmp[2][i] = key[i];
    }
    len_t i; // input index
    len_t o; // output index
    // break up into blocks
    for(i = 0, o = 0; i < len+sizeof(len_t); i+=sizeof(data512_t)-RAND_PADDING, o+=sizeof(data512_t)) {
        hash_sha512(tmp[0], (uint8_t*)tmp, sizeof(tmp)); // compute next block key
        data512_t outblock;
        len_t j;
#ifndef DUMMY_CIPHER
        random_get512(tmp[3]);
#else
        memset(tmp[3], 0, sizeof(tmp[3]));
#endif
        for(j = 0; j < sizeof(data512_t)-RAND_PADDING && i+j < len; j++) {
            tmp[3][j] = in[i+j];
        }
        if(j <= sizeof(data512_t)-sizeof(len_t)-RAND_PADDING) {
            // the very last block contains as very last data (before fixed random padding) the length of the data
            for(len_t k = 0; k < sizeof(len_t); k++)
                tmp[3][sizeof(data512_t)-1-k-RAND_PADDING] = (uint8_t)(len >> (k*8));
        }
        cipher_encryptblock(outblock, tmp[3], tmp[0]); // encrypt block
        for(len_t j = 0; j < sizeof(data512_t); j++) {
            out[o+j] = outblock[j];
        }
    }
    if(newin != NULL) {
        free(newin);
    }
    return o;
}

len_t cipher_decryptdata(uint8_t* out, const uint8_t* in, len_t len, const data512_t indicator, const data512_t key) {
    assert(len % sizeof(data512_t) == 0);
    data512_t tmp[4] = { { 0 } };
    for(len_t i = 0; i < sizeof(data512_t); i++) {
        tmp[1][i] = indicator[i];
        tmp[2][i] = key[i];
    }
    len_t i; // input index
    len_t o; // output index
    // decrypt every block
    for(i = 0, o = 0; i < len; i+=sizeof(data512_t), o+=sizeof(data512_t)-RAND_PADDING) {
        hash_sha512(tmp[0], (uint8_t*)tmp, sizeof(tmp)); // compute next blockkey
        len_t j;
        for(j = 0; j < sizeof(data512_t); j++) {
            tmp[3][j] = in[i+j];
        }
        cipher_decryptblock(tmp[3], tmp[3], tmp[0]); // decrypt block
        for(len_t j = 0; j < sizeof(data512_t)-RAND_PADDING; j++) {
            out[o+j] = tmp[3][j];
        }
    }
    len_t rlen = 0;
    for(len_t j = 0; j < sizeof(len_t); j++) /* get length */ {
        rlen |= (len_t)out[o-1-j] << (j*8);
    }
    return rlen;
}
