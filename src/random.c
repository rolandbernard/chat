// Copyright (c) 2019 Roland Bernard

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "random.h"
#include "hash.h"

static data256_t state = { 0 };

void random_seed(data256_t seed) {
	for(len_t i = 0; i < sizeof(state); i++)
		state[i] ^= seed[i];
	hash_sha256(state, (char*)state, sizeof(state));
}

void random_seed_unix_urandom() {
	data256_t tmp;
	int urfd = open("/dev/urandom", O_RDONLY);
	read(urfd, tmp, sizeof(tmp));
	for(len_t i = 0; i < sizeof(tmp); i++)
		tmp[i] ^= state[i];
	hash_sha256(state, (char*)tmp, sizeof(tmp));
	close(urfd);
}

void random_get(data256_t ret) {
	hash_sha256(state, (char*)state, sizeof(state));
	for(len_t i = 0; i < sizeof(state); i++)
		ret[i] = state[i];
}
