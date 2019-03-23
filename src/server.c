// Copyright (c) 2019 Roland Bernard

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>

#include "server.h"

#define TIMEOUT_SEC 2
#define MAX_HISTORY_SIZE 1048576
#define SERVER_CLOCK 1000
#define START_BUFFER_LEN 1024

error_t server_main(config_t conf) {
	bool_t use_dis = conf.flag & FLAG_CONF_AUTO_DIS;
	bool_t use_udp = use_dis;
	bool_t def_host = conf.flag & FLAG_CONF_DEF_HOST;

	// create discovery socket if needed
	int udp_sock = 0;
	if(use_udp) {
		udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(udp_sock == -1) {
			perror("udp socket couldn't be created");
			return ERROR;
		}
		// set timeout
		struct timeval tv;
		tv.tv_sec = TIMEOUT_SEC;
		tv.tv_usec = 0;
		if(setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
			perror("setsockopt error");
			return ERROR;
		}
		int enable = 1;
		if (setsockopt(udp_sock ,SOL_SOCKET, SO_REUSEADDR, &enable ,sizeof(enable)) == -1) {
			perror("setsockopt error");
			return ERROR;
		}
		// bind socket
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		if(def_host)
			addr.sin_addr.s_addr = INADDR_ANY;
		else
			addr.sin_addr.s_addr = inet_addr(conf.host);
		addr.sin_port = htons(conf.port);
		if(bind(udp_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			perror("couldn't bind udp socket");
			return ERROR;
		}
	}

	// create  socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		perror("socket couldn't be created");
		return ERROR;
	}
	// set timeout
	struct timeval tv;
	tv.tv_sec = TIMEOUT_SEC;
	tv.tv_usec = 0;
	if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
		perror("setsockopt error");
		return ERROR;
	}
	int enable = 1;
	if (setsockopt(sock ,SOL_SOCKET, SO_REUSEADDR, &enable ,sizeof(enable)) == -1) {
		perror("setsockopt error");
		return ERROR;
	}
	// bind socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	if(def_host)
		addr.sin_addr.s_addr = INADDR_ANY;
	else
		addr.sin_addr.s_addr = inet_addr(conf.host);
	addr.sin_port = htons(conf.port);
	if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("couldn't bind socket");
		return ERROR;
	}
	// configure sockert to listen
	if(listen(sock, 64) == -1) {
		perror("couldn't bind socket");
		return ERROR;
	}
	// configure socket to be nonblocking
	int flags = fcntl(sock, F_GETFL);
	if(flags == -1) {
		perror("couldn't get fd flags");
		return ERROR;
	}
	flags |= O_NONBLOCK;
	if(fcntl(sock, F_SETFL, flags) == -1) {
		perror("couldn't set fd flags");
		return ERROR;
	}

	// setup list to store all clients
	struct pollfd* listenfd = (struct pollfd*)malloc(sizeof(struct pollfd)*3);
	listenfd[0].fd = STDIN_FILENO;
	listenfd[0].events = POLLIN;
	listenfd[1].fd = sock;
	listenfd[1].events = POLLIN;
	if(use_dis) {
		listenfd[2].fd = udp_sock;
		listenfd[2].events = POLLIN;
	} else {
		listenfd[2].fd = 0;
		listenfd[2].events = 0;
	}
	id_t* cids = NULL;
	len_t cid = 1;
	len_t num_clients_con = 0;

	bool_t end = 0;
	char* buffer = (char*)malloc(START_BUFFER_LEN);
	len_t buffer_len = START_BUFFER_LEN;
	int len;

	// variables to keep track of some stats
	uint64_t num_messg = 0;
	uint32_t num_messg_hist = 0;
	time_t start_time = time(NULL);
	uint64_t loops = 0;

	char* history = (char*)malloc(MAX_HISTORY_SIZE);
	len_t history_len = 0;

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
		fprintf(stderr, "uptime: %i days %i hours %i min. %i sec. (%lu)\n", day, hou, min, sec, loops);
		fprintf(stderr, "number of messages: %lu (%u)\n", num_messg, num_messg_hist);
		fprintf(stderr, "number of clients: %lu (%lu)\n", num_clients_con, cid);
		fprintf(stderr, "\x1b[3A"); // go up 3 lines

		poll(listenfd, 3+num_clients_con, SERVER_CLOCK);

		// accept discovery messages
		if(use_udp && (listenfd[2].revents & POLLIN)) {
			struct sockaddr_storage addr;
			unsigned int addr_len = sizeof(addr);
			len = recvfrom(udp_sock, buffer, buffer_len, MSG_DONTWAIT, (struct sockaddr*)&addr, &addr_len);
			if(use_dis && len == 2 && buffer[0] == 'H' && buffer[1] == 'I') /* if we get a discovery package we return ok */ {
				buffer[0] = 'O';
				buffer[1] = 'K';
				sendto(udp_sock, buffer, 2, 0, (struct sockaddr*)&addr, addr_len);
			}
		}

		if(listenfd[1].revents & POLLIN) {
			// accept new client if there is one
			int new_client = accept(sock, NULL, NULL);
			if(new_client != -1) {
				id_t id = cid;
				// send id to the client
				for(uint32_t i = 0; i < sizeof(id_t); i++)
					buffer[i] = (id >> (8*i)) & 0xff;
				len = 0;
				while(len < sizeof(id_t)) {
					int tmp_len = send(new_client, buffer, sizeof(id_t)-len, 0);
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
				cids = realloc(cids, sizeof(id_t)*num_clients_con);
				listenfd = realloc(listenfd, sizeof(struct pollfd)*(3+num_clients_con));
				cids[num_clients_con-1] = id;
				cid++;
				listenfd[3+num_clients_con-1].fd = new_client;
				listenfd[3+num_clients_con-1].events = POLLIN;
				listenfd[3+num_clients_con-1].revents = 0;
			}
		}

		// see if anyone wants to send anything
		for(int i = 0; i < num_clients_con; i++) {
			if(listenfd[3+i].revents & POLLIN) {
				len = recv(listenfd[3+i].fd, buffer+sizeof(id_t), sizeof(len_t), MSG_DONTWAIT);
				if(len >= 1) {
					len += recv(listenfd[3+i].fd, buffer+sizeof(id_t)+len, sizeof(len_t)-len, MSG_WAITALL);
					if(len == sizeof(len_t)) {
						len_t len_read = 0;
						for(uint32_t j = 0; j < sizeof(len_t); j++)
							len_read |= (len_t)(uint8_t)buffer[sizeof(id_t)+j] << (8*j);
						if(len_read+len+sizeof(id_t) > buffer_len) {
							buffer = realloc(buffer, 2*buffer_len);
							buffer_len *= 2;
						}
						len += recv(listenfd[3+i].fd, buffer+sizeof(id_t)+len, len_read, MSG_WAITALL);
						if(len == sizeof(len_t)+len_read) {
							// add the id to the message
							for(uint32_t j = 0; j < sizeof(id_t); j++)
								buffer[j] = (cids[i] >> (8*j)) & 0xff;
							len += sizeof(id_t);
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
							if(MAX_HISTORY_SIZE >= len)
								while(history_len+len > MAX_HISTORY_SIZE) {
									unsigned int len_first = 0;
									for(uint32_t j = 0; j < sizeof(len_t); j++)
										len_read |= (len_t)(uint8_t)buffer[sizeof(id_t)+j] << (8*j);
									history_len -= sizeof(id_t)+sizeof(len_t)+len_first;
									memmove(history, history+sizeof(id_t)+sizeof(len_t)+len_first, history_len);
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
							memmove(cids+i, cids+i+1, sizeof(id_t)*(num_clients_con-i));
						}
					} else {
						// disconnect client
						num_clients_con--;
						close(listenfd[3+i].fd);
						memmove(listenfd+3+i, listenfd+3+i+1, sizeof(struct pollfd)*(num_clients_con-i));
						memmove(cids+i, cids+i+1, sizeof(id_t)*(num_clients_con-i));
					}
				} else if(len == 0) {
					// disconnect client
					num_clients_con--;
					close(listenfd[3+i].fd);
					memmove(listenfd+3+i, listenfd+3+i+1, sizeof(struct pollfd)*(num_clients_con-i));
					memmove(cids+i, cids+i+1, sizeof(id_t)*(num_clients_con-i));
				} /* else error (EAGAIN || EWOULDBLOCK) */
			}
		}

		if(listenfd[0].revents & POLLIN) {
			// read stdin
			len = read(STDIN_FILENO, buffer, buffer_len);
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
	if(use_udp)
		close(udp_sock);
	close(sock);
	free(history);
	free(buffer);

	return OK;
}
