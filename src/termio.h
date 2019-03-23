// Copyright (c) 2019 Roland Bernard
#ifndef __TERMIO_H__
#define __TERMIO_H__

#include "types.h"

void term_init(bool_t use_alternet);

void term_refresh();

void term_reset_promt();

void term_set_title(const char* str);

#define FLAG_TERM_IGN_BREAK 1
#define FLAG_TERM_SHOW_GROUP 2
#define FLAG_TERM_OWN 8
#define FLAG_TERM_UTF8 16
#define FLAG_TERM_PRINT_NAME 32

void term_wrire_promt(const char* buffer, len_t len, uint32_t cursor_pos, uint8_t flag);

void term_write_msg(const msgbuf_t* msg, uint8_t flag);

void term_write_str(const char* str);

void term_end(bool_t use_alternet);

#endif
