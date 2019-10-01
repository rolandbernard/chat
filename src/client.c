// Copyright (c) 2019 Roland Bernard

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <time.h>

#include "client.h"
#include "termio.h"
#include "netio.h"
#include "random.h"
#include "hash.h"
#include "image.h"

#define TIMEOUT_SEC 2
#define MAX_SEARCH_TRY 10
#define START_BUFFER_LEN 1024
#define STATUS_BUFFER_LEN 64
#define TMP_BUFFER_LEN 512
#define CLIENT_CLOCK 100

#define MAX_IMG_WIDTH 1024
#define MAX_IMG_HEIGHT 1024

error_t client_main(const config_t conf) {
    bool_t use_dis = conf.flag & FLAG_CONF_AUTO_DIS;
    bool_t use_udp = use_dis;
    bool_t def_host = conf.flag & FLAG_CONF_DEF_HOST;
    bool_t ignore_breaking = conf.flag & FLAG_CONF_IGN_BREAK;
    bool_t use_utf8 = conf.flag & FLAG_CONF_UTF8;
    bool_t use_alternet = conf.flag & FLAG_CONF_USE_ALTERNET;
    bool_t use_group = conf.flag & FLAG_CONF_USE_GROUP;
    bool_t use_enc = conf.flag & FLAG_CONF_USE_ENC;
    bool_t use_typing = conf.flag & FLAG_CONF_USE_TYP;
    bool_t use_enter_exit = conf.flag & FLAG_CONF_USE_LOG;

    int udp_sock = 0;
    // create udp_sock
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
            perror("udp timeout setsockopt error");
            return ERROR;
        }
        // enable broadcast
        int broadcastEnable=1;
        if(setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) {
            perror("udp broadcast setsockopt error");
            return ERROR;
        }
        // bind socket
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = INADDR_ANY;
        saddr.sin_port = htons(0);
        if(bind(udp_sock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
            perror("couldn't bind udp socket");
            return ERROR;
        }
    }

    // create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        perror("socket coudn't be created");
        return ERROR;
    }
    // set timeout
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
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
    struct hostent* hoste = gethostbyname(conf.host);
    if(hoste == NULL || hoste->h_addr_list[0] == NULL) {
        perror("couldn't find the specified ip address");
        return ERROR;
    }
    addr.sin_addr = *(((struct in_addr**)hoste->h_addr_list)[0]);
    addr.sin_port = htons(conf.port);
    server_addr = (struct sockaddr*)&addr;
    server_addr_len = sizeof(addr);

    unsigned char end = 0;
    // use udp_sock
    if(use_udp && use_dis) {
        fprintf(stderr, "Looking for server...\n");
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        saddr.sin_port = htons(conf.port);
        int i = MAX_SEARCH_TRY;
        while(i) {
            char buffer[TMP_BUFFER_LEN] = "HI";
            int len = sendto(udp_sock, buffer, 2, 0, (struct sockaddr*)&saddr, sizeof(saddr));
            if(len == 2) {
                unsigned int raddr_len = sizeof(raddr);
                len = recvfrom(udp_sock, buffer, TMP_BUFFER_LEN, 0, (struct sockaddr*)&raddr, &raddr_len);
                if(len == 2 && buffer[0] == 'O' && buffer[1] == 'K') {
                    fprintf(stderr, "\x1b[A\x1b[M");
                    server_addr = (struct sockaddr*)&raddr;
                    server_addr_len = raddr_len;
                    def_host = 0;
                    break;
                }
            } else {
                perror("failed sending discovery");
                return ERROR;
            }
            i--;
            len = read(STDIN_FILENO, buffer, TMP_BUFFER_LEN);
            for(int j = 0; j < len; j++)
                if(buffer[j] == 3 /* <C-c> */ || buffer[j] == 4 /* <C-d> */) {
                    i = 0;
                    end = 1;
                }
        }
        if(def_host && !end)
            fprintf(stderr, "couldn't find a server, trying defaults\n");
    }

    if(!end) {
        // connect
        if(connect(sock, server_addr, server_addr_len) == -1) {
            perror("couldn't connect to addr");
            return ERROR;
        }
    }

    struct pollfd listenfd[2];
    listenfd[0].fd = STDIN_FILENO;
    listenfd[0].events = POLLIN;
    listenfd[1].fd = sock;
    listenfd[1].events = POLLIN;
    listenfd[1].revents = 0;

    char* buffer = (char*)malloc(START_BUFFER_LEN);
    len_t buffer_len = START_BUFFER_LEN;
    len_t buff_len = 0;
    uint32_t cursor_pos = 0;

    id_t id = 0;
    id_t last_cid = ~0;
    int len;

    if(!end) {
        len = recv(sock, buffer, sizeof(id_t), MSG_WAITALL);
        // get the id
        if(len == sizeof(id_t)) {
            id = 0;
            for(uint32_t i = 0; i < sizeof(id_t); i++)
                id |= (id_t)(uint8_t)buffer[i] << (8*i);
        } else {
            perror("didn't recv client id");
            return ERROR;
        }

        // send entering info
        if(use_enter_exit) {
            msgbuf_t msg;
            msg.cid = id;
            msg.name = conf.name;
            msg.group = conf.group;
            msg.data_len = 0;
            msg.data = NULL;
            msg.flag = (use_enc ? FLAG_MSG_ENC : 0) | FLAG_MSG_ENT;
            if(use_enc) {
                data256_t ind;
                random_get(ind);
                for(uint32_t j = 0; j < INDICATOR_LEN; j++)
                    msg.indicator[j] = 0;
                for(uint32_t j = 0; j < sizeof(data256_t); j++)
                    msg.indicator[j%INDICATOR_LEN] ^= ind[j];
                hash_sha512(msg.key, conf.passwd, strlen(conf.passwd));
            }
            net_sendmsg(sock, &msg);
        }
    }

    char status[STATUS_BUFFER_LEN];
    snprintf(status, STATUS_BUFFER_LEN, "...");
    struct timeval last_status;
    gettimeofday(&last_status, NULL);
    uint64_t max_status_time_usec = 2000000;

    random_seed_unix_urandom();
    term_init(use_alternet);
    while(!end) {
        if(max_status_time_usec != 0) {
            struct timeval now;
            gettimeofday(&now, NULL);
            if((now.tv_sec - last_status.tv_sec)*1000000 + (now.tv_usec - last_status.tv_usec) >= max_status_time_usec) {
                status[0] = 0;
                max_status_time_usec = 0;
            }
        }

        char tmp_in[TMP_BUFFER_LEN];
        if(use_group)
            snprintf(tmp_in, TMP_BUFFER_LEN, "@%s: %s", conf.group, status);
        else
            snprintf(tmp_in, TMP_BUFFER_LEN, "no group: %s", status);
        term_set_title(tmp_in);

        term_reset_promt();

        // get mesages
        if(listenfd[1].revents & POLLIN) {
            msgbuf_t msg;
            if(use_enc)
                hash_sha512(msg.key, conf.passwd, strlen(conf.passwd));
            error_t ret = net_recvmsg(sock, &msg);
            if(ret == OK) {
                if(!use_group || (msg.group != NULL && strcmp(msg.group, conf.group) == 0)) {
                    if(msg.flag & FLAG_MSG_TYP) /* typing info */ {
                        if(msg.cid != id && strcmp(status, "...") != 0) {
                            if(!use_group && msg.group != NULL)
                                snprintf(status, STATUS_BUFFER_LEN, "%s@%s is typing...", msg.name, msg.group);
                            else
                                snprintf(status, STATUS_BUFFER_LEN, "%s is typing...", msg.name);
                            gettimeofday(&last_status, NULL);
                            max_status_time_usec = 500000;
                        }
                    } else if(msg.flag & FLAG_MSG_ENT) /* entering info */ {
                        if(msg.cid != id && strcmp(status, "...") != 0) {
                            if(!use_group && msg.group != NULL)
                                snprintf(status, STATUS_BUFFER_LEN, "%s@%s entered the chat...", msg.name, msg.group);
                            else
                                snprintf(status, STATUS_BUFFER_LEN, "%s entered the chat...", msg.name);
                            gettimeofday(&last_status, NULL);
                            max_status_time_usec = 10000000;
                        }
                    } else if(msg.flag & FLAG_MSG_EXT) /* exiting info */ {
                        if(msg.cid != id && strcmp(status, "...") != 0) {
                            if(!use_group && msg.group != NULL)
                                snprintf(status, STATUS_BUFFER_LEN, "%s@%s left the chat...", msg.name, msg.group);
                            else
                                snprintf(status, STATUS_BUFFER_LEN, "%s left the chat...", msg.name);
                            gettimeofday(&last_status, NULL);
                            max_status_time_usec = 10000000;
                        }
                    }

                    if(msg.data != NULL) /* normal message */ {
                        uint8_t flags =
                            (use_utf8 ? FLAG_TERM_UTF8 : 0) |
                            (ignore_breaking ? FLAG_TERM_IGN_BREAK : 0) |
                            (msg.cid == id ? FLAG_TERM_OWN : 0) |
                            (msg.cid != last_cid ? FLAG_TERM_PRINT_NAME : 0) |
                            (!use_group ? FLAG_TERM_SHOW_GROUP : 0);
                        term_write_msg(&msg, flags);
                        last_cid = msg.cid;
                        free(msg.data);
                    }
                }
                free(msg.name);
                free(msg.group);
            } else if(ret == CONNECTION_CLOSED) {
                // disconnected
                end = 1;
            }
        }
        // print input
        uint8_t flags =
            (use_utf8 ? FLAG_TERM_UTF8 : 0) |
            (ignore_breaking ? FLAG_TERM_IGN_BREAK : 0);
        term_wrire_promt(buffer, buff_len, cursor_pos, flags);

        term_refresh();

        poll(listenfd, 2, CLIENT_CLOCK);

        // read stdin
        if(listenfd[0].revents & POLLIN) {
            int len_in = read(STDIN_FILENO, tmp_in, TMP_BUFFER_LEN-8); // -8 to hava a buffer for unicode characters that were not read fully
            if(len_in >= 1) {
                for(int i = 0; i < len_in; i++) {
                    if(tmp_in[i] == '\b' || tmp_in[i] == '\x7f') /* delete */ {
                        if(cursor_pos > 0) {
                            int num_byte = 1;
                            if(use_utf8)
                                while(cursor_pos-num_byte > 0 && (buffer[cursor_pos-num_byte] & 0xc0) == 0x80) num_byte++;
                            cursor_pos -= num_byte;
                            buff_len -= num_byte;
                            memmove(buffer+cursor_pos, buffer+cursor_pos+num_byte, buff_len);
                        }
                    } else if(tmp_in[i] == '\t') /* tab */ {
                        if(buff_len+1 > buffer_len) {
                            buffer = realloc(buffer, buffer_len*2);
                            buffer_len *= 2;
                        }
                        memmove(buffer+cursor_pos+1, buffer+cursor_pos, buff_len-cursor_pos);
                        buffer[cursor_pos] = ' ';
                        cursor_pos++;
                        buff_len++;
                    } else if(tmp_in[i] == 3 /* <C-c> */ || tmp_in[i] == 4 /* <C-D> */) {
                        end = 1;
                    } else if(tmp_in[i] == 1 /* <C-a> */) {
                        img_data_t img;
                        int n;
                        buffer[buff_len] = 0;
                        img.data = (uint8_t *)stbi_load(buffer, &(img.w), &(img.h), &n, 3);

                        if(img.data != NULL) {
                            int x = img.w;
                            int y = img.h;
                            while(x > MAX_IMG_WIDTH || y > MAX_IMG_HEIGHT) /* maximum size */ {
                                x >>= 1;
                                y >>= 1;
                            }
                            msgbuf_t msg;
                            msg.cid = id;
                            msg.name = conf.name;
                            msg.group = conf.group;
                            msg.data_len = 2*sizeof(int) + x*y*3; // size + with, height
                            msg.data = (char*)malloc(2*sizeof(int) + x*y*3);
                            for(int i = 0; i < sizeof(int); i++)
                                msg.data[i] = x >> 8*i;
                            for(int i = 0; i < sizeof(int); i++)
                                msg.data[i+sizeof(int)] = y >> 8*i;
                            for(int ix = 0; ix < x; ix++)
                                for(int iy = 0; iy < y; iy++) {
                                    msg.data[2*sizeof(int)+3*(iy*x+ix)] = img.data[3*(iy*img.h/y*img.w+ix*img.w/x)];
                                    msg.data[2*sizeof(int)+3*(iy*x+ix)+1] = img.data[3*(iy*img.h/y*img.w+ix*img.w/x)+1];
                                    msg.data[2*sizeof(int)+3*(iy*x+ix)+2] = img.data[3*(iy*img.h/y*img.w+ix*img.w/x)+2];
                                }
                            msg.flag = (use_enc ? FLAG_MSG_ENC : 0) | FLAG_MSG_IMG;
                            if(use_enc) {
                                data256_t ind;
                                random_get(ind);
                                for(uint32_t j = 0; j < INDICATOR_LEN; j++)
                                    msg.indicator[j] = 0;
                                for(uint32_t j = 0; j < sizeof(data256_t); j++)
                                    msg.indicator[j%INDICATOR_LEN] ^= ind[j];
                                hash_sha512(msg.key, conf.passwd, strlen(conf.passwd));
                            }

                            net_sendmsg(sock, &msg);
                            stbi_image_free(img.data);
                            free(msg.data);

                            buff_len = 0;
                            cursor_pos = 0;
                        }
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
                            msgbuf_t msg;
                            msg.cid = id;
                            msg.name = conf.name;
                            msg.group = conf.group;
                            msg.data_len = buff_len;
                            msg.data = buffer;
                            msg.flag = (use_enc ? FLAG_MSG_ENC : 0);
                            if(use_enc) {
                                data256_t ind;
                                random_get(ind);
                                for(uint32_t j = 0; j < INDICATOR_LEN; j++)
                                    msg.indicator[j] = 0;
                                for(uint32_t j = 0; j < sizeof(data256_t); j++)
                                    msg.indicator[j%INDICATOR_LEN] ^= ind[j];
                                hash_sha512(msg.key, conf.passwd, strlen(conf.passwd));
                            }

                            net_sendmsg(sock, &msg);

                            buff_len = 0;
                            cursor_pos = 0;
                        }
                    } else /* character was typed */ {
                        char tmp_out[10];
                        tmp_out[0] = tmp_in[i];
                        int num_byte = 1;
                        int ch = tmp_in[i];
                        if((tmp_in[i] & 0x80) && use_utf8) {
                            while(tmp_in[i] & (0x80 >> num_byte)) num_byte++;
                            ch = (ch & (0xff >> (num_byte+1))) << ((num_byte-1)*6);
                            while(i+num_byte > len_in) {
                                len_in += read(STDIN_FILENO, tmp_in+len_in, i+num_byte-len_in);
                            }
                            for(int j = num_byte-2; j >= 0 && i+1 < len_in && (tmp_in[i+1] & 0xc0) == 0x80; j--) /* read every byte as long as we dont hit the end and it is a actual continuation byte */ {
                                i++;
                                tmp_out[num_byte-1-j] = tmp_in[i];
                                ch |= (tmp_in[i] & 0x3f) << j*6;
                            }
                        }
                        if((isprint(ch) || num_byte >= 2)) /* normal char and utf-8 (I have not found an easy way to check utf8 for printability) */ {
                            if(buff_len+num_byte > buffer_len) {
                                buffer = realloc(buffer, buffer_len*2);
                                buffer_len *= 2;
                            }
                            memmove(buffer+cursor_pos+num_byte, buffer+cursor_pos, buff_len-cursor_pos);
                            memcpy(buffer+cursor_pos, tmp_out, num_byte);
                            cursor_pos += num_byte;
                            buff_len += num_byte;
                            // send typing info
                            if(use_typing) {
                                msgbuf_t msg;
                                msg.cid = id;
                                msg.name = conf.name;
                                msg.group = conf.group;
                                msg.data_len = 0;
                                msg.data = NULL;
                                msg.flag = (use_enc ? FLAG_MSG_ENC : 0) | FLAG_MSG_TYP;
                                if(use_enc) {
                                    data256_t ind;
                                    random_get(ind);
                                    for(uint32_t j = 0; j < INDICATOR_LEN; j++)
                                        msg.indicator[j] = 0;
                                    for(uint32_t j = 0; j < sizeof(data256_t); j++)
                                        msg.indicator[j%INDICATOR_LEN] ^= ind[j];
                                    hash_sha512(msg.key, conf.passwd, strlen(conf.passwd));
                                }
                                net_sendmsg(sock, &msg);
                            }
                        }
                    }
                }
            }
        }
    }
    term_reset_promt();
    term_end(use_alternet);

    // send exit info
    if(use_enter_exit) {
        msgbuf_t msg;
        msg.cid = id;
        msg.name = conf.name;
        msg.group = conf.group;
        msg.data_len = 0;
        msg.data = NULL;
        msg.flag = (use_enc ? FLAG_MSG_ENC : 0) | FLAG_MSG_EXT;
        if(use_enc) {
            data256_t ind;
            random_get(ind);
            for(uint32_t j = 0; j < INDICATOR_LEN; j++)
                msg.indicator[j] = 0;
            for(uint32_t j = 0; j < sizeof(data256_t); j++)
                msg.indicator[j%INDICATOR_LEN] ^= ind[j];
            hash_sha512(msg.key, conf.passwd, strlen(conf.passwd));
        }
        net_sendmsg(sock, &msg);
    }

    if(use_udp)
        close(udp_sock);
    close(sock);

    free(buffer);

    return OK;
}
