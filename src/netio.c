
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "netio.h"
#include "cipher.h"
#include "hash.h"

error_t net_sendmsg(int sock, msgbuf_t* msg) {
	len_t namelen = strlen(msg->name);
	len_t grouplen = strlen(msg->group);

	len_t totallen = namelen+1;	// <len><name>\0
	if(msg->group[0] != 0)
		totallen += 1+grouplen;		// <len><name>[@<group>]\0
	if(msg->flag & FLAG_ENC)
		totallen += 5 + INDICATOR_LEN;	// <len>[~KEY:<ind>]<name>[@<group>]\0
	if(msg->flag & FLAG_TYP)
		totallen += 4;		// <len>[~<ind>:KEY]<name>[@<group>][|TYP]\0
	else
		totallen += msg->data_len;	// <len>[~<ind>:KEY]<name>[@<group>][|TYP]\0[<data>]
	
	uint8_t* buffer = (uint8_t*)malloc(sizeof(len_t)+totallen+2*sizeof(data256_t)); // +2*sizeof(data256_t) to be sure everything fits even after encryption
	len_t buflen = sizeof(len_t);
	len_t enc_start;
	if(msg->flag & FLAG_ENC) {
		buffer[buflen++] = '~';
		memcpy(buffer+buflen, msg->indicator, INDICATOR_LEN);
		buflen += INDICATOR_LEN;
		enc_start = buflen;
		memcpy(buffer+buflen, ":KEY", 4);
		buflen += 4;
	}
	memcpy(buffer+buflen, msg->name, namelen);
	buflen += namelen;
	if(msg->group[0] != 0) {
		buffer[buflen++] = '@';
		memcpy(buffer+buflen, msg->group, grouplen);
		buflen += grouplen;
	}
	if(msg->flag & FLAG_TYP) {
		memcpy(buffer+buflen, "|TYP", 4);
		buflen += 4;
	}
	buffer[buflen++] = 0;
	memcpy(buffer+buflen, msg->data, msg->data_len);
	buflen += msg->data_len;
	if(msg->flag & FLAG_ENC) {
		data512_t ind;
		hash_sha512(ind, msg->indicator, INDICATOR_LEN);
		buflen = cipher_encryptdata(buffer+enc_start, buffer+enc_start, buflen-enc_start, ind, msg->key)+enc_start;
	}
	for(len_t i = 0; i < sizeof(len_t); i++)
		buffer[i] = (uint8_t)(buflen >> (i*8));

	len_t len_send = 0;
	while(len_send < buflen) {
		len_t tmp_len = send(sock, buffer+len_send, buflen-len_send, 0);
		if(tmp_len == -1) {
			free(buffer);
			return ERROR;
		} else
			len_send += tmp_len;
	}

	free(buffer);
	return OK;
}

// find the first occurance of the given character in the string
// return -1 if the char isn't contained
static int64_t strfndchr(const char* str, char c) {
	int i = 0;
	while(str[i]) {
		if(str[i] == c)
			return i;
		i++;
	}
	return -1;
}

// no field in msg will be freed by this function!
error_t net_recvmsg(int sock, msgbuf_t* msg) {
	uint8_t bufferhead[sizeof(id_t)+sizeof(len_t)];
	len_t len = recv(sock, bufferhead, sizeof(id_t)+sizeof(len_t), MSG_DONTWAIT);
	if(len >= 1) {
		len_t tmp_len = recv(sock, bufferhead, sizeof(id_t)+sizeof(len_t)-len, MSG_WAITALL);
		len += tmp_len;
		if(len == sizeof(id_t)+sizeof(len_t)) {
			id_t cid = 0;
			for(len_t i = 0; i < sizeof(id_t); i++)
				cid |= (len_t)bufferhead[i] << (i*8);
			len_t buflen = 0;
			for(len_t i = 0; i < sizeof(len_t); i++)
				buflen |= (len_t)bufferhead[sizeof(id_t)+i] << (i*8);
			buflen -= sizeof(len_t);
			uint8_t* buffer = (uint8_t*)malloc(buflen);
			tmp_len = recv(sock, buffer, buflen, MSG_WAITALL);
			if(tmp_len == buflen-sizeof(len_t)) {
				msg->flag = 0;
				char* msgre = (char*)buffer;
				if(msgre[0] == '~') /* the message is encrypted */ {
					data512_t ind;
					msgre++;
					hash_sha512(ind, msgre, INDICATOR_LEN);
					msgre += INDICATOR_LEN;
					buflen = cipher_decryptdata((uint8_t*)msgre, (uint8_t*)msgre, buflen-1-INDICATOR_LEN, ind, msg->key);
					if(strncmp(msgre, ":KEY", 4) != 0) /* couldn't decrypt the data */ {
						free(buffer);
						return NO_DATA;
					}
					msgre += 4;
					msg->flag |= FLAG_ENC;
				}
				len_t headlen = strlen(msgre);
				len_t atpos = strfndchr(msgre, '@');
				len_t pipepos = strfndchr(msgre, '|');
				len_t datalen = buflen-headlen-1;

				len_t namelen;
				if(atpos != -1)
					namelen = atpos;
				else if(pipepos != -1)
					namelen = pipepos;
				else
					namelen = headlen;

				len_t grouplen;
				if(atpos == -1)
					grouplen = 0;
				else if(pipepos != -1)
					grouplen = pipepos-atpos;
				else
					grouplen = headlen-atpos;

				msg->name = (char*)malloc(namelen+1);
				memcpy(msg->name, msgre, namelen);
				msg->name[namelen] = 0;
				if(grouplen != 0) {
					msg->group = (char*)malloc(grouplen+1);
					memcpy(msg->group, msgre+atpos, grouplen);
					msg->group[grouplen] = '0';
				} else
					msg->group = NULL;

				if(pipepos != -1 && strcmp(msgre+namelen+grouplen+1, "TYP") == 0) {
					msg->flag |= FLAG_TYP;
					msg->data_len = 0;
					msg->data = NULL;
				} else {
					msg->data_len = datalen;
					msg->data = (char*)malloc(datalen);
					memcpy(msg->data, msgre+headlen+1, datalen);
				}
			} else {
				free(buffer);
				if(tmp_len == 0) {
					return CONNECTION_CLOSED;
				} else {
					return ERROR;
				}
			}
		} else if(tmp_len == 0)
			return CONNECTION_CLOSED;
		else {
			return ERROR;
		}
	} else if(len == 0) {
		return CONNECTION_CLOSED;
	} else
		return NO_DATA;
	return OK;
}
