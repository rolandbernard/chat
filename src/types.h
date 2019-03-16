// Copyright (c) 2019 Roland Bernard
#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

#define ERROR -1
#define OK 0
#define NO_DATA 1
#define CONNECTION_CLOSED 2

typedef int32_t error_t;
typedef uint8_t bool_t;
typedef uint32_t id_t;
typedef uint64_t len_t;
typedef uint32_t hash32_t;
typedef uint8_t hash256_t[32];
typedef uint8_t hash512_t[64];
typedef uint8_t data256_t[32];
typedef uint8_t data512_t[64];

#define INDICATOR_LEN 64

#define FLAG_ENC 1
#define FLAG_TYP 2

typedef struct {
	id_t cid;
	len_t total_len;
	uint32_t flag;
	char indicator[INDICATOR_LEN];
	data512_t key;
	char* name;		// username
	char* group;	// groupname
	len_t data_len;
	char* data;
} msgbuf_t;

#endif
