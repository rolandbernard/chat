// Copyright (c) 2019 Roland Bernard
#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

#define MAX_NAME_LEN 64
#define MAX_GROUP_LEN 64
#define MAX_TYPE_LEN 64
#define MAX_BUFFER_LEN 64000

typedef uint32_t id_t;
typedef uint32_t len_t;
typedef uint32_t hash32_t;
typedef uint8_t hash256_t[32];
typedef uint8_t hash512_t[64];

typedef struct {
	id_t cid;
	len_t len;
	char name[MAX_NAME_LEN];	// username
	char group[MAX_GROUP_LEN];	// groupname
	char type[MAX_TYPE_LEN];	// e.g. ~typing, ~key:<key>
	char data[MAX_BUFFER_LEN];
} msgbuf_t;

#endif
