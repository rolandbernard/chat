// Copyright (c) 2019 Roland Bernard
#ifndef __RANDOM_H__
#define __RANDOM_H__

#include "types.h"

void random_seed(data256_t seed);

void random_seed_unix_urandom();

void random_get(data256_t ret);

void random_get512(data512_t ret);

#endif
