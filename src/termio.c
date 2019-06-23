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
	buffer_len += sprintf(buffer+buffer_len, "\x1b]2;%s\a", str);
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


/* Parts of this code are from the TerminalImageRenderer project */
#define SUP_SAMP_VER 2
#define SUP_SAMP_HOR 2

#define TIR_COPY_COLOR(X, Y) X[0] = Y[0]; X[1] = Y[1]; X[2] = Y[2];

// Defining posible chars
static const unsigned char tir_map_full_block[] = { 1 };
#if SUP_SAMP_HOR >= 2
static const unsigned char tir_map_half_vert_block[] = {
	1, 0
};
#endif
#if SUP_SAMP_VER >= 2
static const unsigned char tir_map_half_hor_block[] = {
	1,
	0
};
#if SUP_SAMP_HOR >= 2
static const unsigned char tir_map_quart_upleft_block[] = {
	1, 0,
	0, 0
};
static const unsigned char tir_map_quart_upright_block[] = {
	0, 1,
	0, 0
};
static const unsigned char tir_map_quart_downleft_block[] = {
	0, 0,
	1, 0
};
static const unsigned char tir_map_quart_downright_block[] = {
	0, 0,
	0, 1
};
static const unsigned char tir_map_quart_diag_block[] = {
	1, 0,
	0, 1
};
#endif
#endif

static const unsigned char* tir_maps[] = {
	tir_map_full_block,
#if SUP_SAMP_HOR >= 2
	tir_map_half_vert_block,
#endif
#if SUP_SAMP_VER >= 2
	tir_map_half_hor_block,
#if SUP_SAMP_HOR >= 2
	tir_map_quart_upleft_block,
	tir_map_quart_upright_block,
	tir_map_quart_downleft_block,
	tir_map_quart_downright_block,
	tir_map_quart_diag_block,
#endif
#endif
};
static const int tir_map_sizes[][2] = {
	{1, 1},
#if SUP_SAMP_HOR >= 2
	{2, 1},
#endif
#if SUP_SAMP_VER >= 2
	{1, 2},
#if SUP_SAMP_HOR >= 2
	{2, 2},
	{2, 2},
	{2, 2},
	{2, 2},
	{2, 2},
#endif
#endif
};
static const char* tir_map_chars[] = {
	"█",
#if SUP_SAMP_HOR >= 2
	"▌",
#endif
#if SUP_SAMP_VER >= 2
	"▀",
#if SUP_SAMP_HOR >= 2
	"▘",
	"▝",
	"▖",
	"▗",
	"▚",
#endif
#endif
};

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

	int rand_ind = rand()%MAX_RAND_IND;

	if(!(msg->flag & FLAG_MSG_IMG)) {
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
	} else if(msg->flag & FLAG_MSG_IMG) {
		int x = 0;
		int y = 0;
		for(int i = 0; i < sizeof(int); i++)
			x |= (int)(uint8_t)msg->data[i] << 8*i;
		for(int i = 0; i < sizeof(int); i++)
			y |= (int)(uint8_t)msg->data[sizeof(int)+i] << 8*i;
		
		int csy = y*width*MAX_MESSAGE_WIDTH/x/2;
		int csx = width*MAX_MESSAGE_WIDTH;

		for(int i = 0; i < rand_ind; i++)
			buffer[buffer_len++] = ' ';
		
		/* buffer_len += sprintf(buffer+buffer_len, "\x1b[34m◢\x1b[m\x1b[37;44m"); */
		if(own) {
			if(csy == 1) {
				buffer_len += sprintf(buffer+buffer_len, "\x1b[34m◢\x1b[m\x1b[37;44m");
			} else
				buffer[buffer_len++] = ' ';
		} else
			buffer_len += sprintf(buffer+buffer_len, "\x1b[32m◥\x1b[m");

		for(uint32_t cy = 0; cy < csy; cy++) {
			for(uint32_t cx = 0; cx < csx; cx++) {
				int min_loss = 3*255*SUP_SAMP_HOR*SUP_SAMP_VER+1; // Maximum posible loss
				const char* min = " ";
				uint8_t min_fc[3] = {0};
				uint8_t min_bc[3] = {0};

				uint8_t tmp_fc[3];
				uint8_t tmp_bc[3];

				for(int i = 0; i < sizeof(tir_maps)/sizeof(tir_maps[0]); i++) {
					int fcolor[3] = {0, 0, 0};
					int bcolor[3] = {0, 0, 0};
					int num_forg = 0;
					int num_backg = 0;
					// Full char
					for(int ix = 0; ix < SUP_SAMP_HOR; ix++)
						for(int iy = 0; iy < SUP_SAMP_VER; iy++) {
							int px = (cx*SUP_SAMP_HOR+ix)*x/(width*MAX_MESSAGE_WIDTH*SUP_SAMP_HOR);
							int py = (cy*SUP_SAMP_VER+iy)*y/(y*width*MAX_MESSAGE_WIDTH/x/2*SUP_SAMP_VER);
							if(tir_maps[i][iy*tir_map_sizes[i][1]/SUP_SAMP_VER*tir_map_sizes[i][0]+ix*tir_map_sizes[i][0]/SUP_SAMP_HOR]) {
								fcolor[0] += (int)msg->data[2*sizeof(int)+3*(py*x+px)];
								fcolor[1] += (int)msg->data[2*sizeof(int)+3*(py*x+px)+1];
								fcolor[2] += (int)msg->data[2*sizeof(int)+3*(py*x+px)+2];
								num_forg++;
							} else {
								bcolor[0] += (int)msg->data[2*sizeof(int)+3*(py*x+px)];
								bcolor[1] += (int)msg->data[2*sizeof(int)+3*(py*x+px)+1];
								bcolor[2] += (int)msg->data[2*sizeof(int)+3*(py*x+px)+2];
								num_backg++;
							}
						}
					if(num_forg != 0) {
						fcolor[0] /= num_forg;
						fcolor[1] /= num_forg;
						fcolor[2] /= num_forg;
					}
					if(num_backg != 0) {
						bcolor[0] /= num_backg;
						bcolor[1] /= num_backg;
						bcolor[2] /= num_backg;
					}

					int loss = 0;
					for(int ix = 0; ix < SUP_SAMP_HOR; ix++)
						for(int iy = 0; iy < SUP_SAMP_VER; iy++) {
							int px = (cx*SUP_SAMP_HOR+ix)*x/(width*MAX_MESSAGE_WIDTH*SUP_SAMP_HOR);
							int py = (cy*SUP_SAMP_VER+iy)*y/(y*width*MAX_MESSAGE_WIDTH/x/2*SUP_SAMP_HOR);
							if(tir_maps[i][iy*tir_map_sizes[i][1]/SUP_SAMP_VER*tir_map_sizes[i][0]+ix*tir_map_sizes[i][0]/SUP_SAMP_HOR]) {
								loss += abs((int)msg->data[2*sizeof(int)+3*(py*x+px)]-fcolor[0]);
								loss += abs((int)msg->data[2*sizeof(int)+3*(py*x+px)+1]-fcolor[1]);
								loss += abs((int)msg->data[2*sizeof(int)+3*(py*x+px)+2]-fcolor[2]);
								num_forg++;
							} else {
								loss += abs((int)msg->data[2*sizeof(int)+3*(py*x+px)]-bcolor[0]);
								loss += abs((int)msg->data[2*sizeof(int)+3*(py*x+px)+1]-bcolor[1]);
								loss += abs((int)msg->data[2*sizeof(int)+3*(py*x+px)+2]-bcolor[2]);
								num_backg++;
							}
						}
					tmp_fc[0] = (unsigned char)fcolor[0];
					tmp_fc[1] = (unsigned char)fcolor[1];
					tmp_fc[2] = (unsigned char)fcolor[2];

					tmp_bc[0] = (unsigned char)bcolor[0];
					tmp_bc[1] = (unsigned char)bcolor[1];
					tmp_bc[2] = (unsigned char)bcolor[2];

					if(loss < min_loss) {
						min = tir_map_chars[i];
						TIR_COPY_COLOR(min_bc, tmp_bc)
							TIR_COPY_COLOR(min_fc, tmp_fc)
							min_loss = loss;
					}
				}
				buffer_len += sprintf(buffer+buffer_len, "\x1b[38;2;%hhu;%hhu;%hhum\x1b[48;2;%hhu;%hhu;%hhum",
						min_fc[0], min_fc[1], min_fc[2], min_bc[0], min_bc[1], min_bc[2]);
				memcpy(buffer+buffer_len, min, strlen(min));
				buffer_len += strlen(min);
			}
			buffer_len += sprintf(buffer+buffer_len, "\x1b[m\n");
			if(cy != csy-1) {
				for(int i = 0; i < rand_ind; i++)
					buffer[buffer_len++] = ' ';
				if(own && cy == csy-2) {
					buffer_len += sprintf(buffer+buffer_len, "\x1b[34m◢\x1b[m\x1b[37;44m");
				} else {
					buffer[buffer_len++] = ' ';
				}
			}
		}
	}
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
