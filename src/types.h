// Copyright (c) 2019 Roland Bernard
#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

#define ERROR -1
#define OK 0
#define NO_DATA 4
#define CONNECTION_CLOSED 2
#define ENC_DATA 3

typedef int32_t error_t;
typedef uint16_t bool_t;
typedef uint32_t id_t;
typedef uint64_t len_t;
typedef uint32_t hash32_t;
typedef uint8_t hash256_t[32];
typedef uint8_t hash512_t[64];
typedef uint8_t data256_t[32];
typedef uint8_t data512_t[64];

#define FLAG_MSG_ENC 1
#define FLAG_MSG_TYP 2
#define FLAG_MSG_ENT 4
#define FLAG_MSG_EXT 8
#define FLAG_MSG_IMG 16

typedef struct {
    id_t cid;
    uint8_t flag;
    data512_t ind;
    data512_t key;
    char* name;        // username
    char* group;    // groupname
    len_t data_len;
    char* data;
} msgbuf_t;

// settings flags
#define FLAG_CONF_AUTO_DIS 1
#define FLAG_CONF_IS_SERVER 2
#define FLAG_CONF_UTF8 4
#define FLAG_CONF_IGN_BREAK 8
#define FLAG_CONF_USE_ALTERNET 16
#define FLAG_CONF_DEF_HOST 32
#define FLAG_CONF_USE_GROUP 64
#define FLAG_CONF_USE_ENC 128
#define FLAG_CONF_USE_TYP 256
#define FLAG_CONF_USE_LOG 512

int strfndchr(const char* str, char c);

typedef struct {
    uint32_t flag;
    char* name;
    char* group;
    char* host;
    char* passwd;
    uint16_t port;
} config_t;

typedef struct {
    int w;
    int h;
    uint8_t* data;
} img_data_t;

#endif
