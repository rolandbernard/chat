// Copyright (c) 2019 Roland Bernard

#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "termio.h"

#define MAX_BUFFER_SIZE 1048576
#define MAX_INPUT_WIDTH 1
#define MAX_WRAP_WORD 1/3
#define MAX_MESSAGE_WIDTH 3/4
#define MAX_RAND_IND 3

static char* buffer;
static len_t buffer_len = 0;

static uint32_t cursor_row = 0;

static uint32_t width;
static uint32_t height;

void get_termsize(uint32_t* width, uint32_t* height) {
	struct winsize w;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
		*width = ~0; // don't bother with the width
		*height = ~0;
	} else {
		*width = w.ws_col;
		*height = w.ws_row;
	}
}

void term_init(bool_t use_alternet) {
	buffer = (char*)malloc(MAX_BUFFER_SIZE);
	get_termsize(&width, &height);
	if(use_alternet) {
		memcpy(buffer+buffer_len, "\0337\x1b[?47h\x1b[2J\x1b[999B", 18); // save cursor position, change to alternet buffer, clear the screen, go to the last line
		buffer_len += 18;
	}
	memcpy(buffer+buffer_len, "\x1b[?25l\x1b[6 q\n", 12); // hide cursor and change cursor shape
	buffer_len += 12;
	term_refresh();
}

void term_refresh() {
	write(STDOUT_FILENO, buffer, buffer_len);
	buffer_len = 0;
}

void term_reset_promt() {
	memcpy(buffer+buffer_len, "\x1b[?25l", 6); // hide cursor
	buffer_len += 6;
	buffer_len += sprintf(buffer+buffer_len, "\x1b[%iA\r\x1b[J", cursor_row+1); // clear previous output
}

void term_set_title(const char* str) {
	buffer_len += sprintf(buffer+buffer_len, "\e]2;%s\a", str);
}

void term_wrire_promt(const char* data, len_t len, uint32_t cursor_pos, uint8_t flag) {
	get_termsize(&width, &height);
	bool_t use_utf8 = flag & FLAG_TERM_UTF8;
	bool_t ignore_breaking = flag & FLAG_TERM_IGN_BREAK;

	memcpy(buffer+buffer_len, "\n> ", 3);
	buffer_len += 3;

	int print_len = 0;
	int in_row = 2;
	int num_rows = 1;
	int cursor_len = 0;
	int cursor_in_row = 2;
	int cursor_in_row_tmp = 2;
	cursor_row = 0;
	int last_word = 0;
	int last_word_byte = 0;
	int start_space = 0;
	while(print_len < len) {
		if(isblank(data[print_len])) {
			last_word = 0;
			last_word_byte = 0;
		}
		if(ignore_breaking || in_row != 0 || !isblank(data[print_len]) || start_space) /* spaces after newline get ignored */ {
			start_space = 0;
			if(!isblank(data[print_len])) {
				last_word++;
				last_word_byte++;
			}
			buffer[buffer_len++] = data[print_len++];
			if(use_utf8)
				while(print_len < len && (data[print_len] & 0xc0) == 0x80) {
					buffer[buffer_len++] = data[print_len++];
					last_word_byte++;
					cursor_len++;
				}
			in_row++;
			if(in_row >= width*MAX_INPUT_WIDTH) {
				if(ignore_breaking || last_word == 0 || last_word > width*MAX_INPUT_WIDTH*MAX_WRAP_WORD || print_len == len || isblank(data[print_len])) /* don't worry about breaking words */ {
					buffer[buffer_len++] = '\n';
					in_row = 0;
				} else /* avoid breaking the word */ {
					memmove(buffer+buffer_len-last_word_byte+1, buffer+buffer_len-last_word_byte, last_word_byte);
					buffer[buffer_len-last_word_byte] = '\n';
					buffer_len++;
					in_row = last_word;
				}
				num_rows++;
			}
			cursor_len++;
			if(cursor_len <= cursor_pos) /* adjust the cursor */ {
				cursor_in_row++;
				cursor_in_row_tmp++;
				if(cursor_in_row >= width*MAX_INPUT_WIDTH) {
					if(ignore_breaking || last_word == 0 || last_word > width*MAX_INPUT_WIDTH*MAX_WRAP_WORD || print_len == len || isblank(data[print_len])) {
						cursor_in_row = 0;
						cursor_in_row_tmp = 0;
					} else {
						cursor_in_row = last_word;
						cursor_in_row_tmp = 0;
					}
					cursor_row++;
				}
			} else /* maby adjust the cursor if a word shouldn't break */ {
				cursor_in_row_tmp++;
				if(cursor_in_row_tmp >= width*MAX_INPUT_WIDTH) {
					if(ignore_breaking || last_word == 0 || last_word > width*MAX_INPUT_WIDTH*MAX_WRAP_WORD || print_len == len || isblank(data[print_len]))
						cursor_in_row_tmp = 0;
					else {
						if(cursor_len-last_word_byte <= cursor_pos) {
							cursor_in_row = last_word-(cursor_in_row_tmp-cursor_in_row);
							cursor_row++;
						}
						cursor_in_row_tmp = last_word;
					}
				}
			}
		} else {
			start_space++;
			print_len++;
			cursor_len++;
		}
	}

	// position cursor
	if(num_rows-cursor_row > 1)
		buffer_len += sprintf(buffer+buffer_len, "\x1b[%iA", num_rows-cursor_row-1);
	buffer[buffer_len++] = '\r';
	if(cursor_in_row > 0)
		buffer_len += sprintf(buffer+buffer_len, "\x1b[%iC", cursor_in_row);

	// show cursor
	memcpy(buffer+buffer_len, "\x1b[?25h", 6);
	buffer_len += 6;
}

void term_write_msg(const msgbuf_t* msg, uint8_t flag) {
	get_termsize(&width, &height);
	bool_t use_utf8 = flag & FLAG_TERM_UTF8;
	bool_t ignore_breaking = flag & FLAG_TERM_IGN_BREAK;
	bool_t own = flag & FLAG_TERM_OWN;
	bool_t show_group = flag & FLAG_TERM_SHOW_GROUP;
	bool_t print_name = flag & FLAG_TERM_PRINT_NAME;

	// print the sender if needed
	if(print_name) {
		if(own)
			buffer[buffer_len++] = '\n';
		else {
			if(show_group && msg->group != NULL) {
				buffer_len += sprintf(buffer+buffer_len, "\n%s@%s:\n", msg->name, msg->group);
			} else
				buffer_len += sprintf(buffer+buffer_len, "\n%s:\n", msg->name);
		}
	}

	// determene the needed width to adjust the size of the message
	int effective_width = 0;
	int len_written = 0;
	int in_row = 0;
	int num_rows_msg = 1;
	int last_word = 0;
	int start_space = 0;
	while(len_written < msg->data_len) {
		if(isblank(msg->data[len_written]))
			last_word = 0;
		if(ignore_breaking || num_rows_msg == 1 || in_row != 0 || !isblank(msg->data[len_written]) || start_space != 0) /* spaces after newline get ignored */ {
			start_space = 0;
			if(!isblank(msg->data[len_written]))
				last_word++;
			len_written++;
			if(use_utf8)
				while(len_written < msg->data_len && (msg->data[len_written] & 0xc0) == 0x80)
					len_written++;
			in_row++;
			if(in_row >= width*MAX_MESSAGE_WIDTH) /* we need a new line */ {
				if(ignore_breaking || last_word == 0 || last_word > width*MAX_MESSAGE_WIDTH*MAX_WRAP_WORD || len_written == msg->data_len || isblank(msg->data[len_written])) /* don't worry about breaking words */ {
					effective_width = in_row;
					in_row = 0;
				} else /*  avoid breaking the word */ {
					if(effective_width < in_row-last_word)
						effective_width = in_row-last_word-1;
					in_row = last_word;
				}
				if(len_written != msg->data_len)
					num_rows_msg++;
			}
		} else {
			start_space++;
			len_written++;
		}
	}
	if(effective_width < in_row)
		effective_width = in_row;

	// actualy print the message
	int rand_ind = rand()%MAX_RAND_IND;
	int rows_written = 0;
	len_written = 0;
	in_row = 0;
	last_word = 0;
	int last_word_byte = 0;
	start_space = 0;

	for(int i = 0; i < rand_ind; i++)
		buffer[buffer_len++] = ' ';
	if(own) {
		if(num_rows_msg == 1)
			if(use_utf8)
				buffer_len += sprintf(buffer+buffer_len, "\x1b[34m◢\x1b[m\x1b[37;44m");
			else
				buffer_len += sprintf(buffer+buffer_len, "\x1b[34m_\x1b[m\x1b[37;44m");
		else
			buffer_len += sprintf(buffer+buffer_len, " \x1b[37;44m");
	} else
		if(use_utf8)
			buffer_len += sprintf(buffer+buffer_len, " \x1b[32m◥\x1b[m\x1b[30;42m");
		else
			buffer_len += sprintf(buffer+buffer_len, " \x1b[32m*\x1b[m\x1b[30;42m");

	while(len_written < msg->data_len) {
		if(isblank(msg->data[len_written])) {
			last_word = 0;
			last_word_byte = 0;
		}
		if(ignore_breaking || rows_written == 0 || in_row != 0 || !isblank(msg->data[len_written]) || start_space) /* spaces after newline get ignored */ {
			start_space = 0;
			if(!isblank(msg->data[len_written])) {
				last_word++;
				last_word_byte++;
			}
			buffer[buffer_len++] = msg->data[len_written++];
			if(use_utf8)
				while(len_written < msg->data_len && (msg->data[len_written] & 0xc0) == 0x80) {
					buffer[buffer_len++] = msg->data[len_written++];
					last_word_byte++;
				}
			in_row++;
			if(in_row >= effective_width && len_written != msg->data_len) /* we need a new line */ {
				rows_written++;
				char beg[64];
				int len_beg = 0;
				len_beg += sprintf(beg+len_beg, "\x1b[m\n");
				for(int i = 0; i < rand_ind; i++)
					beg[len_beg++] = ' ';
				if(own) {
					if(rows_written == num_rows_msg-1)
						if(use_utf8)
							len_beg += sprintf(beg+len_beg, "\x1b[34m◢\x1b[m\x1b[37;44m");
						else
							len_beg += sprintf(beg+len_beg, "\x1b[34m_\x1b[m\x1b[37;44m");
					else
						len_beg += sprintf(beg+len_beg, " \x1b[37;44m");
				} else
					len_beg += sprintf(beg+len_beg, "  \x1b[30;42m");

				if(ignore_breaking || last_word == 0 || last_word > width*MAX_MESSAGE_WIDTH*MAX_WRAP_WORD || isblank(msg->data[len_written])) /* don't wory about breaking words */ {
					memcpy(buffer+buffer_len, beg, len_beg);
					buffer_len += len_beg;
					in_row = 0;
				} else /* avoid breaking the word */ {
					memmove(buffer+buffer_len-last_word_byte+last_word+len_beg, buffer+buffer_len-last_word_byte, last_word_byte);
					for(int i = 0; i < last_word; i++)
						buffer[buffer_len-last_word_byte+i] = ' ';
					memcpy(buffer+buffer_len-last_word_byte+last_word, beg, len_beg);
					buffer_len += last_word+len_beg;
					in_row = last_word;
				}
			}
		} else {
			start_space++;
			len_written++;
		}
	}
	for(int i = in_row; i < effective_width; i++)
		buffer[buffer_len++] = ' ';
	buffer_len += sprintf(buffer+buffer_len, "\x1b[m\n");
}

void term_write_str(const char* str) {
	len_t len = strlen(str);
	memcpy(buffer+buffer_len, str, len);
	buffer_len += len;
}

void term_end(bool_t use_alternet) {
	if(use_alternet) {
		memcpy(buffer+buffer_len, "\x1b[2J\x1b[?47l\0338", 12);  // exit alternet buffer, restore cursor
		buffer_len += 12;
	} else
		buffer[buffer_len++] = '\n';
	memcpy(buffer+buffer_len, "\x1b[?25h", 6);
	buffer_len += 6;
	term_refresh();
	free(buffer);
}
