// Copyright (c) 2019 Roland Bernard

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "random.h"
#include "hash.h"

static data512_t state = { 0 };

void random_seed(data512_t seed) {
    for(len_t i = 0; i < sizeof(state); i++)
        state[i] ^= seed[i];
    hash_sha512(state, state, sizeof(state));
}

void random_seed_unix_urandom() {
    data512_t tmp;
    int urfd = open("/dev/urandom", O_RDONLY);
    read(urfd, tmp, sizeof(tmp));
    for(len_t i = 0; i < sizeof(tmp); i++)
        tmp[i] ^= state[i];
    hash_sha512(state, tmp, sizeof(tmp));
    close(urfd);
}

void random_get(data256_t ret) {
    hash_sha512(state, state, sizeof(state));
    for(len_t i = 0; i < sizeof(data256_t); i++)
        ret[i] = state[i];
}

void random_get512(data512_t ret) {
    random_get(ret);
    random_get(ret + sizeof(data256_t));
}
