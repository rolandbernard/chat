// Copyright (c) 2019 Roland Bernard

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>

#define DEF_PORT 24242
#define DEF_HOST "127.0.0.1"
#define DEF_GROUP "default"
#define DEF_NAME getlogin()

#define MAX_BUFFER_SIZE 32768
#define MAX_NAME_SIZE 64
// id (4 Byte) + size (4 Byte)+ name + delimeter (1 Byte = '@') + group + delimeter (1 Byte = '\0')
#define MAX_HEADER_SIZE 10+2*MAX_NAME_SIZE
#define MAX_HISTORY_SIZE 64*(MAX_BUFFER_SIZE+MAX_HEADER_SIZE)

#define MAX_MESSAGE_WIDTH 8/10
#define MAX_WRAP_WORD 4/10
#define MAX_RAND_IND 3
#define CLIENT_CLOCK 1000
#define SERVER_CLOCK 1000
#define MAX_SEARCH_TRY 5

#define min(X, Y) ((X) < (Y) ? (X) : (Y))

// used to restore the terminal
struct termios oldterm;

void resetTerm() {
	tcsetattr(STDIN_FILENO, 0, &oldterm);
}

// gets the size of the terminal (only width is actualy used)
void get_termsize(unsigned int* width, unsigned int* height) {
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	*width = w.ws_col;
	*height = w.ws_row;
}

// find the first occurance of the given character in the string
// return -1 if the char isn't contained
int strfndchr(const char* str, char c) {
	int i = 0;
	while(str[i]) {
		if(str[i] == c)
			return i;
		i++;
	}
	return -1;
}

int main(int argc, char** argv) {
	unsigned int port = DEF_PORT;
	const char* host = DEF_HOST;
	unsigned char def_host = 1;
	unsigned char is_server = 0;
	unsigned char use_alternet = 0;
	unsigned char use_group = 1;
	unsigned char use_utf8 = 1;
	unsigned char use_auto_dis = 0;
	unsigned char ignore_breaking = 0;
	const char* group = DEF_GROUP;
	const char* name = DEF_NAME;

	// evaluate parameters
	for(int i = 1; i < argc; i++) {
		if(strcmp("-h", argv[i]) == 0 || strcasecmp("--ip", argv[i]) == 0) /* server ip */ {
			if(i+1 < argc) {
				host = argv[i+1];
				def_host = 0;
				i++;
			} else
				fprintf(stderr, "no host specified, option is ignored\n");
		} else if(strcmp("-p", argv[i]) == 0 || strcasecmp("--port", argv[i]) == 0) /* server port */ {
			if(i+1 < argc) {
				unsigned int nport = atoi(argv[i+1]);
				if(nport == 0 || nport > 0xFFFF)
					perror("illegal port number, option is ignored\n");
				else
					port = nport;
				i++;
			} else
				fprintf(stderr, "no port specified, option is ignored\n");
		} else if(strcmp("-a", argv[i]) == 0 || strcasecmp("--alternet", argv[i]) == 0) /* use alternet screen buffer */ {
			use_alternet = 1;
		} else if(strcmp("-s", argv[i]) == 0 || strcasecmp("--server", argv[i]) == 0) /* is this a server */ {
			is_server = 1;
		} else if(strcmp("-G", argv[i]) == 0 || strcasecmp("--no-group", argv[i]) == 0) /* don't use group */ {
			use_group = 0;
		} else if(strcmp("-H", argv[i]) == 0 || strcasecmp("--auto-discovery", argv[i]) == 0) /* use automatic host discovery */ {
			use_auto_dis = 1;
		}  else if(strcmp("-B", argv[i]) == 0 || strcasecmp("--ignore-break", argv[i]) == 0) /* use automatic host discovery */ {
			ignore_breaking = 1;
		} else if(strcmp("-U", argv[i]) == 0 || strcasecmp("--no-utf-8", argv[i]) == 0) /* don't use utf-8 */ {
			use_utf8 = 0;
		} else if(strcmp("-n", argv[i]) == 0 || strcasecmp("--name", argv[i]) == 0) /* name of the user */ {
			if(i+1 < argc) {
				if(strlen(argv[i+1]) > MAX_NAME_SIZE)
					fprintf(stderr, "name is to long, option is ignored\n");
				else if(strfndchr(argv[i+1], '@') != -1)
					fprintf(stderr, "name can't contain '@', option is ignored\n");
				else
					name = argv[i+1];
				i++;
			} else
				fprintf(stderr, "no name specified, option is ignored\n");
		} else if(strcmp("-g", argv[i]) == 0 || strcasecmp("--group", argv[i]) == 0) /* name of the group */ {
			if(i+1 < argc) {
				if(strlen(argv[i+1]) > MAX_NAME_SIZE)
					fprintf(stderr, "group name is to long, option is ignored\n");
				else if(strfndchr(argv[i+1], '@') != -1)
					fprintf(stderr, "group name can't contain '@', option is ignored\n");
				else
					group = argv[i+1];
				i++;
			} else
				fprintf(stderr, "no group specified, option is ignored\n");
		}  else if(strcmp("--help", argv[i]) == 0) /* output help */ {
			fprintf(stderr,
				"Usage: chat [options]\n"
				"\n"
				"Options:\n"
				"  -h, --ip IP            set the servers ip (def: '127.0.0.1')\n"
				"  -p, --port PORT        select the servers port (def: '24242')\n"
				"  -s, --server           make this a server\n"
				"\n"
				"Options for clients:\n"
				"  -n, --name NAME        set the name (def: '')\n"
				"  -G, --no-group         do not use the group feature\n"
				"  -H, --auto-discovery   use automatic discovery\n"
				"  -B, --ignore-break     do not worry about breaking words\n"
				"  -U, --no-utf-8         avoid any utf-8 I/O processing\n"
				"  -g, --group GROUP      set the group (def: 'default')\n"
				"  -a, --alternet        *use the alternet frame buffer\n"
				"  --help                 show this help page\n"
				"\n"
				"* This may cause problems if the terminal\n"
				"  does not support certain features\n"
			);
			exit(EXIT_SUCCESS);
		} else {
			fprintf(stderr, "unknown option '%s', option is ignored\n", argv[i]);
		}
	}

	// configure input to be less processed
	struct termios newterm;
	tcgetattr(STDIN_FILENO, &oldterm);
	atexit(resetTerm);
	newterm = oldterm;
	newterm.c_lflag &= ~(ICANON | ECHO | ISIG);
	//newterm.c_oflag &= ~(OPOST);
	newterm.c_cc[VMIN] = 0;
	newterm.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, 0, &newterm);

	signal(SIGPIPE, SIG_IGN); // an error on send will cause a SIGPIPE
	if(is_server) {
		// create discovery socket if needed
		int discovery_sock;
		if(use_auto_dis) {
			discovery_sock = socket(AF_INET, SOCK_DGRAM, 0);
			if(discovery_sock == -1) {
				perror("discovery socket couldn't be created");
				exit(EXIT_FAILURE);
			}
			int enable = 1;
			if (setsockopt(discovery_sock ,SOL_SOCKET, SO_REUSEADDR, &enable ,sizeof(enable)) == -1) {
				perror("setsockopt error");
				exit(EXIT_FAILURE);
			}
			// bind socket
			struct sockaddr_in addr;
			addr.sin_family = AF_INET;
			if(def_host)
				addr.sin_addr.s_addr = INADDR_ANY;
			else
				addr.sin_addr.s_addr = inet_addr(host);
			addr.sin_port = htons(port);
			if(bind(discovery_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
				perror("couldn't bind discovery socket");
				exit(EXIT_FAILURE);
			}
		}

		// create  socket
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if(sock == -1) {
			perror("socket couldn't be created");
			exit(EXIT_FAILURE);
		}
		// set timeout
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
			perror("setsockopt error");
			exit(EXIT_FAILURE);
		}
		int enable = 1;
		if (setsockopt(sock ,SOL_SOCKET, SO_REUSEADDR, &enable ,sizeof(enable)) == -1) {
			perror("setsockopt error");
			exit(EXIT_FAILURE);
		}
		// bind socket
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		if(def_host)
			addr.sin_addr.s_addr = INADDR_ANY;
		else
			addr.sin_addr.s_addr = inet_addr(host);
		addr.sin_port = htons(port);
		if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			perror("couldn't bind socket");
			exit(EXIT_FAILURE);
		}
		// configure sockert to listen
		if(listen(sock, 64) == -1) {
			perror("couldn't bind socket");
			exit(EXIT_FAILURE);
		}
		// configure socket to be nonblocking
		int flags = fcntl(sock, F_GETFL);
		if(flags == -1) {
			perror("couldn't get fd flags");
			exit(EXIT_FAILURE);
		}
		flags |= O_NONBLOCK;
		if(fcntl(sock, F_SETFL, flags) == -1) {
			perror("couldn't set fd flags");
			exit(EXIT_FAILURE);
		}

		// setup list to store all clients
		struct pollfd* listenfd = (struct pollfd*)malloc(sizeof(struct pollfd)*3);
		listenfd[0].fd = STDIN_FILENO;
		listenfd[0].events = POLLIN;
		listenfd[1].fd = sock;
		listenfd[1].events = POLLIN;
		if(use_auto_dis) {
			listenfd[2].fd = discovery_sock;
			listenfd[2].events = POLLIN;
		} else {
			listenfd[2].fd = 0;
			listenfd[2].events = 0;
		}
		unsigned int* cids = NULL;
		unsigned int cid = 0;
		int num_clients_con = 0;

		int end = 0;
		char buffer[MAX_BUFFER_SIZE+MAX_HEADER_SIZE];
		int len;

		// variables to keep track of some stats
		int num_messg = 0;
		int num_messg_hist = 0;
		int start_time = time(NULL);
		int loops = 0;

		char history[MAX_HISTORY_SIZE];
		unsigned int history_len = 0;

		fprintf(stderr, "\x1b[?25l"); // hide cursor
		while(!end) {
			loops++;
			int sec = time(NULL)-start_time;
			int min = sec/60;
			int hou = min/60;
			int day = hou/24;
			sec %= 60;
			min %= 60;
			hou %= 24;
			fprintf(stderr, "\x1b[3M"); // clear previous output
			fprintf(stderr, "uptime: %i days %i hours %i min. %i sec. (%i)\n", day, hou, min, sec, loops);
			fprintf(stderr, "number of messages: %i (%i)\n", num_messg, num_messg_hist);
			fprintf(stderr, "number of clients: %i (%i)\n", num_clients_con, cid);
			fprintf(stderr, "\x1b[3A"); // go up 3 lines

			poll(listenfd, 3+num_clients_con, SERVER_CLOCK);

			// accept discovery messages
			if(use_auto_dis && (listenfd[2].revents & POLLIN)) {
				struct sockaddr_storage addr;
				unsigned int addr_len = sizeof(addr);
				len = recvfrom(discovery_sock, buffer, 2, MSG_DONTWAIT, (struct sockaddr*)&addr, &addr_len);
				if(len == 2 && buffer[0] == '4' && buffer[1] == '_') /* if we get a package we return the recved date */ {
					buffer[1] = '2';
					sendto(discovery_sock, buffer, 2, 0, (struct sockaddr*)&addr, addr_len);
				}
			}

			if(listenfd[1].revents & POLLIN) {
				// accept new client if there is one
				int new_client = accept(sock, NULL, NULL);
				if(new_client != -1) {
					// send id to the client
					buffer[0] = cid & 0xff;
					buffer[1] = (cid >> 8) & 0xff;
					buffer[2] = (cid >> 16) & 0xff;
					buffer[3] = (cid >> 24) & 0xff;
					len = 0;
					while(len < 4) {
						int tmp_len = send(new_client, buffer, 4-len, 0);
						if(tmp_len == -1)
							break;
						else
							len += tmp_len;
					}
					// send history to the client
					len = 0;
					while(len < history_len) {
						int tmp_len = send(new_client, history, history_len, 0);
						if(tmp_len == -1)
							break;
						else
							len += tmp_len;
					}
					num_clients_con++;
					cids = realloc(cids, sizeof(unsigned int)*num_clients_con);
					listenfd = realloc(listenfd, sizeof(struct pollfd)*(3+num_clients_con));
					cids[num_clients_con-1] = cid;
					cid++;
					listenfd[3+num_clients_con-1].fd = new_client;
					listenfd[3+num_clients_con-1].events = POLLIN;
				}
			}

			// see if anyone wants to send anything
			for(int i = 0; i < num_clients_con; i++) {
				if(listenfd[3+i].revents & POLLIN) {
					len = recv(listenfd[3+i].fd, buffer+4, 4, MSG_DONTWAIT);
					if(len >= 1) {
						len += recv(listenfd[3+i].fd, buffer+4+len, 4-len, MSG_WAITALL);
						if(len == 4) {
							unsigned int len_read = (unsigned int)(unsigned char)buffer[4] | ((unsigned int)(unsigned char)buffer[5] << 8) |
								((unsigned int)(unsigned char)buffer[6] << 16) | ((unsigned int)(unsigned char)buffer[7] << 24);
							len += recv(listenfd[3+i].fd, buffer+4+len, len_read, MSG_WAITALL);
							if(len == 4+len_read) {
								// add the id to the message
								buffer[0] = cids[i] & 0xff;
								buffer[1] = (cids[i] >> 8) & 0xff;
								buffer[2] = (cids[i] >> 16) & 0xff;
								buffer[3] = (cids[i] >> 24) & 0xff;
								len += 4;
								// forward data to anyone
								for(int j = 0; j < num_clients_con; j++) {
									int len_send = 0;
									while(len_send != len) {
										int tmp_len = send(listenfd[3+j].fd, buffer+len_send, len, 0);
										if(tmp_len == -1) /* error */
											break; // if the connection is closed it is removed at the next recv
										else
											len_send += tmp_len;
									}
								}
								// remove messages from history if needed
								if(history_len >= len)
									while(history_len+len > MAX_HISTORY_SIZE) {
										unsigned int len_first = (unsigned int)(unsigned char)history[4] | ((unsigned int)(unsigned char)history[5] << 8) |
											((unsigned int)(unsigned char)history[6] << 16) | ((unsigned int)(unsigned char)history[7] << 24);
										history_len -= 8+len_first;
										memmove(history, history+8+len_first, history_len);
										num_messg_hist--;
									}
								num_messg++;
								num_messg_hist++;
								// add data to history
								memcpy(history+history_len, buffer, len);
								history_len+=len;
							} else {
								// disconnect client
								num_clients_con--;
								close(listenfd[3+i].fd);
								memmove(listenfd+3+i, listenfd+3+i+1, sizeof(struct pollfd)*(num_clients_con-i));
								memmove(cids+i, cids+3+i+1, sizeof(unsigned int)*(num_clients_con-i));
							}
						} else {
							// disconnect client
							num_clients_con--;
							close(listenfd[3+i].fd);
							memmove(listenfd+3+i, listenfd+3+i+1, sizeof(struct pollfd)*(num_clients_con-i));
							memmove(cids+i, cids+3+i+1, sizeof(unsigned int)*(num_clients_con-i));
						}
					} else if(len == 0) {
						// disconnect client
						num_clients_con--;
						close(listenfd[3+i].fd);
						memmove(listenfd+3+i, listenfd+3+i+1, sizeof(struct pollfd)*(num_clients_con-i));
						memmove(cids+i, cids+3+i+1, sizeof(unsigned int)*(num_clients_con-i));
					} /* else error (EAGAIN || EWOULDBLOCK) */
				}
			}

			if(listenfd[0].revents & POLLIN) {
				// read stdin
				len = read(STDIN_FILENO, buffer, MAX_BUFFER_SIZE);
				if(len >= 1)
					for(int i = 0; i < len; i++)
						if(buffer[i] == 'q' || buffer[i] == 'Q' || buffer[i] == 3 /* <C-c> */) /* exit */ {
							end = 1;
							break;
						}
			}
		}
		fprintf(stderr, "\x1b[?25h\x1b[3M"); // show cursor and delete stat output
		for(int i = 0; i < num_clients_con; i++)
			close(listenfd[3+i].fd);
		free(listenfd);
		free(cids);
		if(use_auto_dis)
			close(discovery_sock);
		close(sock);
	} else {
		// create socket
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if(sock == -1) {
			perror("socket coudn't be created");
			exit(EXIT_FAILURE);
		}
		// set timeout
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

		// setup the address of the server
		struct sockaddr* server_addr;
		socklen_t server_addr_len;
		// filled by discovery
		struct sockaddr_storage raddr;
		// given by '-h' or the default
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(host);
		addr.sin_port = htons(port);
		server_addr = (struct sockaddr*)&addr;
		server_addr_len = sizeof(addr);

		// use discovery_sock
		if(use_auto_dis) {
			int discovery_sock = socket(AF_INET, SOCK_DGRAM, 0);
			if(discovery_sock == -1) {
				perror("discovery socket couldn't be created");
				exit(EXIT_FAILURE);
			}
			// set timeout
			struct timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			if(setsockopt(discovery_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
				perror("discovery timeout setsockopt error");
				exit(EXIT_FAILURE);
			}
			// enable broadcast
			int broadcastEnable=1;
			if(setsockopt(discovery_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) {
				perror("discovery broadcast setsockopt error");
				exit(EXIT_FAILURE);
			}
			// bind socket
			struct sockaddr_in saddr;
			saddr.sin_family = AF_INET;
			saddr.sin_addr.s_addr = INADDR_ANY;
			saddr.sin_port = htons(0);
			if(bind(discovery_sock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
				perror("couldn't bind discovery socket");
				exit(EXIT_FAILURE);
			}
			write(STDOUT_FILENO, "Looking for server...\n", 22);
			saddr.sin_family = AF_INET;
			saddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
			saddr.sin_port = htons(port);
			int i = MAX_SEARCH_TRY;
			while(i) {
				char buffer[2] = {'4', '_'};
				int len = sendto(discovery_sock, buffer, 2, 0, (struct sockaddr*)&saddr, sizeof(saddr));
				if(len == 2) {
					unsigned int raddr_len = sizeof(raddr);
					len = recvfrom(discovery_sock, buffer, 2, 0, (struct sockaddr*)&raddr, &raddr_len);
					if(len == 2 && buffer[0] == '4' && buffer[1] == '2') {
						fprintf(stderr, "\x1b[A\x1b[M");
						server_addr = (struct sockaddr*)&raddr;
						server_addr_len = raddr_len;
						def_host = 0;
						break;
					}
				} else {
					perror("failed sending discovery");
					exit(EXIT_FAILURE);
				}
				i--;
			}
			close(discovery_sock);
			if(def_host)
				fprintf(stderr, "couldn't find a server, trying defaults\n");
		}

		// connect
		if(connect(sock, server_addr, server_addr_len) == -1) {
			perror("couldn't connect to addr");
			exit(EXIT_FAILURE);
		}

		struct pollfd listenfd[2];
		listenfd[0].fd = STDIN_FILENO;
		listenfd[0].events = POLLIN;
		listenfd[1].fd = sock;
		listenfd[1].events = POLLIN;
		listenfd[1].revents = 0;

		unsigned char end = 0;
		unsigned int width = 0;
		unsigned int height = 0;

		char tmp_out[3*MAX_BUFFER_SIZE];
		char tmp_in[MAX_BUFFER_SIZE+MAX_HEADER_SIZE];
		char buffer[MAX_BUFFER_SIZE];
		unsigned int buff_len = 0;
		unsigned int cursor_pos = 0;

		int cursor_row = 0;
		int num_rows = 0;

		// get the id
		unsigned int id;
		int len = recv(sock, tmp_in, 4, MSG_WAITALL);
		if(len == 4) {
			id = (unsigned int)(unsigned char)tmp_in[0] | ((unsigned int)(unsigned char)tmp_in[1] << 8)
				| ((unsigned int)(unsigned char)tmp_in[2] << 16) | ((unsigned int)(unsigned char)tmp_in[3] << 24);
		} else {
			perror("didn't recv client id");
			exit(EXIT_FAILURE);
		}
		unsigned int last_cid = ~0;

		get_termsize(&width, &height);
		if(use_alternet)
			write(STDOUT_FILENO, "\0337\x1b[?47h\x1b[2J\x1b[999B", 18); // save cursor position, change to alternet buffer, clear the screen, go to the last line
		write(STDOUT_FILENO, "\x1b[6 q\n", 6); // change cursor shape
		while(!end) {

			len = sprintf(tmp_out, "\x1b[?25l"); // hide cursor
			len += sprintf(tmp_out+len, "\x1b[%iA\x1b[%iM", cursor_row+1, num_rows+1); // clear previous output

			// get mesages
			if(listenfd[1].revents & POLLIN) {
				int len_recv = recv(sock, tmp_in, 8, MSG_DONTWAIT);
				if(len_recv >= 1) {
					len_recv += recv(sock, tmp_in+len_recv, 8-len_recv, MSG_WAITALL);
					if(len_recv == 8) {
						// id of the messege sender
						unsigned int recvid = (unsigned int)(unsigned char)tmp_in[0] | ((unsigned int)(unsigned char)tmp_in[1] << 8) | ((unsigned int)tmp_in[2] << 16) | ((unsigned int)tmp_in[3] << 24);
						// length of the message
						unsigned int recvlen = (unsigned int)(unsigned char)tmp_in[4] | ((unsigned int)(unsigned char)tmp_in[5] << 8) | ((unsigned int)tmp_in[6] << 16) | ((unsigned int)tmp_in[7] << 24);
						len_recv = recv(sock, tmp_in+8, recvlen, MSG_WAITALL);
						if(len_recv == recvlen) {
							len_recv = strlen(tmp_in+8); // length of the name+group
							int group_pos = strfndchr(tmp_in+8, '@');
							if(!use_group || (group_pos != -1 && strcmp(tmp_in+8+group_pos+1, group) == 0)) /* is the message in the right group */ {
								if(use_group)
									tmp_in[8+group_pos] = 0;
								int msg_length = recvlen-len_recv-1;

								// print the sender if needed
								if(last_cid != recvid) {
									if(recvid == id || len_recv == 0)
										len += sprintf(tmp_out+len, "\n");
									else
										len += sprintf(tmp_out+len, "\n%s:\n", tmp_in+8);
								}

								// determene the needed width to adjust the size of the message
								int effective_width = 0;
								int len_written = 0;
								int in_row = 0;
								int num_rows_msg = 1;
								int last_word = 0;
								while(len_written < msg_length) {
									if(isblank(tmp_in[9+len_recv+len_written]))
										last_word = 0;
									if(num_rows_msg == 1 || in_row != 0 || !isblank(tmp_in[9+len_recv+len_written])) /* spaces after newline get ignored */ {
										if(!isblank(tmp_in[9+len_recv+len_written]))
											last_word++;
										len_written++;
										if(use_utf8)
											while(len_written < msg_length && (tmp_in[9+len_recv+len_written] & 0xc0) == 0x80)
												len_written++;
										in_row++;
										if(in_row >= width*MAX_MESSAGE_WIDTH) /* we need a new line */ {
											if(ignore_breaking || last_word == 0 || last_word > width*MAX_WRAP_WORD || len_written == msg_length || isblank(tmp_in[9+len_recv+len_written])) /* don't worry about breaking words */ {
												effective_width = in_row;
												in_row = 0;
											} else /*  avoid breaking the word */ {
												if(effective_width < in_row-last_word)
													effective_width = in_row-last_word;
												in_row = last_word;
											}
											num_rows_msg++;
										}
									} else
										len_written++;
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

								for(int i = 0; i < rand_ind; i++)
									tmp_out[len++] = ' ';
								if(recvid == id) {
									if(num_rows_msg == 1)
										if(use_utf8)
											len += sprintf(tmp_out+len, "\x1b[34m◢\x1b[m\x1b[37;44m");
										else
											len += sprintf(tmp_out+len, "\x1b[34m_\x1b[m\x1b[37;44m");
									else
										len += sprintf(tmp_out+len, " \x1b[37;44m");
								} else
									if(use_utf8)
										len += sprintf(tmp_out+len, " \x1b[32m◥\x1b[m\x1b[30;42m");
									else
										len += sprintf(tmp_out+len, " \x1b[32m*\x1b[m\x1b[30;42m");

								while(len_written < msg_length) {
									if(isblank(tmp_in[9+len_recv+len_written])) {
										last_word = 0;
										last_word_byte = 0;
									}
									if(rows_written == 0 || in_row != 0 || !isblank(tmp_in[9+len_recv+len_written])) /* spaces after newline get ignored */ {
										if(!isblank(tmp_in[9+len_recv+len_written])) {
											last_word++;
											last_word_byte++;
										}
										tmp_out[len++] = tmp_in[9+len_recv+len_written++];
										if(use_utf8)
											while(len_written < msg_length && (tmp_in[9+len_recv+len_written] & 0xc0) == 0x80) {
												tmp_out[len++] = tmp_in[9+len_recv+len_written++];
												last_word_byte++;
											}
										in_row++;
										if(in_row >= effective_width && len_written != msg_length) /* we need a new line */ {
											rows_written++;
											char beg[64];
											int len_beg = 0;
											len_beg += sprintf(beg+len_beg, "\x1b[m\n");
											for(int i = 0; i < rand_ind; i++)
												beg[len_beg++] = ' ';
											if(recvid == id) {
												if(rows_written == num_rows_msg-1)
													if(use_utf8)
														len_beg += sprintf(beg+len_beg, "\x1b[34m◢\x1b[m\x1b[37;44m");
													else
														len_beg += sprintf(beg+len_beg, "\x1b[34m_\x1b[m\x1b[37;44m");
												else
													len_beg += sprintf(beg+len_beg, " \x1b[37;44m");
											} else
												len_beg += sprintf(beg+len_beg, "  \x1b[30;42m");

											if(ignore_breaking || last_word == 0 || last_word > width*MAX_WRAP_WORD || isblank(tmp_in[9+len_recv+len_written])) /* don't wory about breaking words */ {
												memcpy(tmp_out+len, beg, len_beg);
												len += len_beg;
												in_row = 0;
											} else /* avoid breaking the word */ {
												memmove(tmp_out+len-last_word_byte+last_word+len_beg, tmp_out+len-last_word_byte, last_word_byte);
												for(int i = 0; i < last_word; i++)
													tmp_out[len-last_word_byte+i] = ' ';
												memcpy(tmp_out+len-last_word_byte+last_word, beg, len_beg);
												len += last_word+len_beg;
												in_row = last_word;
											}
										}
									} else
										len_written++;
								}
								for(int i = in_row; i < effective_width; i++)
									tmp_out[len++] = ' ';
								len += sprintf(tmp_out+len, "\x1b[m\n");

								last_cid = recvid;
							}
						} // we disconnect at the next recv
					} // we disconnect at the next recv
				} else if(len_recv == 0) {
					// disconnected
					close(sock);
					end = 1;
				}
			}

			// print input
			len += sprintf(tmp_out+len, "\n> ");
			int print_len = 0;
			int in_row = 2;
			num_rows = 1;
			int cursor_len = 0;
			int cursor_in_row = 2;
			cursor_row = 0;
			int last_word = 0;
			int last_word_byte = 0;
			while(print_len < buff_len) {
				if(isblank(buffer[print_len])) {
					last_word = 0;
					last_word_byte = 0;
				}
				if(in_row != 0 || !isblank(buffer[print_len])) /* spaces after newline get ignored */ {
					if(!isblank(buffer[print_len])) {
						last_word++;
						last_word_byte++;
					}
					tmp_out[len++] = buffer[print_len++];
					if(use_utf8)
						while(print_len < buff_len && (buffer[print_len] & 0xc0) == 0x80) {
							tmp_out[len++] = buffer[print_len++];
							last_word_byte++;
							if(cursor_len < cursor_pos)
								cursor_len++;
						}
					in_row++;
					if(in_row >= width) {
						if(ignore_breaking || last_word == 0 || last_word > width/2 || print_len == buff_len || isblank(buffer[print_len])) /* don't worry about breaking words */ {
							tmp_out[len++] = '\n';
							in_row = 0;
						} else /* avoid breaking the word */ {
							memmove(tmp_out+len-last_word_byte+1, tmp_out+len-last_word_byte, last_word_byte);
							tmp_out[len-last_word_byte] = '\n';
							len++;
							in_row = last_word;
						}
						num_rows++;
					}
					if(cursor_len < cursor_pos) /* ajust the cursor */ {
						cursor_len++;
						cursor_in_row++;
						if(cursor_in_row >= width) {
							if(ignore_breaking || last_word == 0 || last_word > width/2 || print_len == buff_len || isblank(buffer[print_len]))
								cursor_in_row = 0;
							else
								cursor_in_row = last_word;
							cursor_row++;
						}
					}
				} else
					print_len++;
			}

			// position cursor
			if(num_rows-cursor_row > 1)
				len += sprintf(tmp_out+len, "\x1b[%iA", num_rows-cursor_row-1);
			tmp_out[len++] = '\r';
			if(cursor_in_row > 0)
				len += sprintf(tmp_out+len, "\x1b[%iC", cursor_in_row);

			// show cursor
			len += sprintf(tmp_out+len, "\x1b[?25h");
			// if there was a change write to screen
			write(STDOUT_FILENO, tmp_out, len);

			poll(listenfd, 2, CLIENT_CLOCK);

			// read stdin
			if(listenfd[0].revents & POLLIN) {
				int len_in = read(STDIN_FILENO, tmp_in, MAX_BUFFER_SIZE);
				if(len_in >= 1) {
					for(int i = 0; i < len_in; i++) {
						if(buff_len < MAX_BUFFER_SIZE) {
							if(tmp_in[i] == '\b' || tmp_in[i] == '\x7f') /* delete */ {
								if(cursor_pos > 0) {
									int num_byte = 1;
									if(use_utf8)
										while(cursor_pos-num_byte > 0 && (buffer[cursor_pos-num_byte] & 0xc0) == 0x80) num_byte++;
									cursor_pos -= num_byte;
									buff_len -= num_byte;
									memmove(buffer+cursor_pos, buffer+cursor_pos+num_byte, buff_len);
								}
							} else if(tmp_in[i] == '\t' && buff_len < MAX_BUFFER_SIZE) /* tab */ {
								memmove(buffer+cursor_pos+1, buffer+cursor_pos, buff_len-cursor_pos);
								buffer[cursor_pos] = ' ';
								cursor_pos++;
								buff_len++;
							} else if(tmp_in[i] == 3 /* <C-c> */) {
								end = 1;
							} else if(tmp_in[i] == '\x1b') /* escape sequence */ {
								i++;
								if(tmp_in[i] == '[') {
									i++;
									if(tmp_in[i] == 'C' && cursor_pos < buff_len) /* right */ {
										cursor_pos++;
										if(use_utf8)
											while(cursor_pos < buff_len && (buffer[cursor_pos] & 0xc0) == 0x80) cursor_pos++;
									} else if(tmp_in[i] == 'D' && cursor_pos > 0) /* left */ {
										cursor_pos--;
										if(use_utf8)
											while(cursor_pos > 0 && (buffer[cursor_pos] & 0xc0) == 0x80) cursor_pos--;
									}
								}
							} else if(tmp_in[i] == '\n') /* enter => send */ {
								if(buff_len > 0) {
									// send message
									len = strlen(name);
									memcpy(tmp_out+4, name, len);
									if(use_group) {
										tmp_out[len+4] = '@';
										len++;
										int tmp_len = strlen(group);
										memcpy(tmp_out+4+len, group, tmp_len);
										len += tmp_len;
									}
									tmp_out[len+4] = 0;
									len++;
									memcpy(tmp_out+4+len, buffer, buff_len);
									len += buff_len;

									tmp_out[0] = len & 0xff;
									tmp_out[1] = (len >> 8) & 0xff;
									tmp_out[2] = (len >> 16) & 0xff;
									tmp_out[3] = (len >> 24) & 0xff;

									int len_send = 0;
									while(len_send < len) {
										int tmp_len = send(sock, tmp_out, len+4-len_send, 0);
										if(tmp_len == -1)
											break;
										else
											len_send += tmp_len;
									}

									buff_len = 0;
									cursor_pos = 0;
								}
							} else {
								tmp_out[0] = tmp_in[i];
								int num_byte = 1;
								int ch = tmp_in[i];
								if((tmp_in[i] & 0x80) && use_utf8) {
									while(tmp_in[i] & (0x80 >> num_byte)) num_byte++;
									ch = (ch & (0xff >> (num_byte+1))) << ((num_byte-1)*6);
									for(int j = num_byte-2; j >= 0; j--) {
										i++;
										tmp_out[num_byte-1-j] = tmp_in[i];
										ch |= (tmp_in[i] & 0x3f) << j*6;
									}
								}
								if((isprint(ch) || num_byte >= 2) && buff_len+num_byte <= MAX_BUFFER_SIZE) /* normal char and utf-8 (I have not found a way to check them for printability) */ {
									memmove(buffer+cursor_pos+num_byte, buffer+cursor_pos, buff_len-cursor_pos);
									memcpy(buffer+cursor_pos, tmp_out, num_byte);
									cursor_pos += num_byte;
									buff_len += num_byte;
								}
							}
						}
					}
				}
			}

			get_termsize(&width, &height);


		}
		if(use_alternet)
			write(STDOUT_FILENO, "\x1b[?47l\0338", 8);  // exit alternet buffer, restore cursor
		else
			fprintf(stdout, "\x1b[%iA\x1b[%iM\n", cursor_row+1, num_rows+1); // clear previous output
	}

	return EXIT_SUCCESS;
}
