// Copyright (c) 2019 Roland Bernard

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "netio.h"
#include "cipher.h"
#include "hash.h"

#include <stdio.h>

error_t net_sendmsg(int sock, const msgbuf_t* msg) {
	len_t namelen = strlen(msg->name);
	len_t grouplen;
	if(msg->group == NULL)
		grouplen = 0;
	else
		grouplen = strlen(msg->group);

	len_t totallen = namelen+1;	// <id><len><name>\0
	if(msg->group != NULL)
		totallen += 1+grouplen;		// <id><len><name>[@<group>]\0
	if(msg->flag & FLAG_MSG_ENC)
		totallen += 11 + INDICATOR_LEN;	// <id><len>[~<ind>:KEY]<name>[@<group>]\0
	if(msg->flag & FLAG_MSG_TYP)
		totallen += 4;		// <id><len>[~<ind>:KEY]<name>[@<group>][|TYP]\0
	else if(msg->flag & FLAG_MSG_ENT)
		totallen += 4;		// <id><len>[~<ind>:KEY]<name>[@<group>][|ENT]\0
	else if(msg->flag & FLAG_MSG_EXT)
		totallen += 4;		// <id><len>[~<ind>:KEY]<name>[@<group>][|EXT]\0
	else
		totallen += msg->data_len;	// <id><len>[~<ind>:KEY]<name>[@<group>][|TYP]\0[<data>]
	
	uint8_t* buffer = (uint8_t*)malloc(sizeof(id_t)+sizeof(len_t)+totallen+2*sizeof(data256_t)); // +2*sizeof(data256_t) to be sure everything fits even after encryption
	len_t buflen = 0;
	len_t enc_start;
	if(msg->flag & FLAG_MSG_ENC) /* add indicator and encryption string */ {
		buffer[sizeof(id_t)+sizeof(len_t)+buflen++] = '~';
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, msg->indicator, INDICATOR_LEN);
		buflen += INDICATOR_LEN;
		enc_start = buflen;
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, ":ENCRYPTED", 10);
		buflen += 10;
	}
	memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, msg->name, namelen); /* add user name */
	buflen += namelen;
	if(msg->group != NULL) /* add group name */ {
		buffer[sizeof(id_t)+sizeof(len_t)+buflen++] = '@';
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, msg->group, grouplen);
		buflen += grouplen;
	}
	if(msg->flag & FLAG_MSG_TYP) /* add typping identifier */ {
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, "|TYP", 4);
		buflen += 4;
	} else if(msg->flag & FLAG_MSG_ENT) /* add enter identifier */ {
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, "|ENT", 4);
		buflen += 4;
	} else if(msg->flag & FLAG_MSG_EXT) /* add exit identifier */ {
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, "|EXT", 4);
		buflen += 4;
	}

	buffer[sizeof(id_t)+sizeof(len_t)+buflen++] = 0;
	if(msg->data != NULL) {
		memcpy(buffer+sizeof(id_t)+sizeof(len_t)+buflen, msg->data, msg->data_len); /* Add data */
		buflen += msg->data_len;
	}

	if(msg->flag & FLAG_MSG_ENC) /* encrypt the data after the indicator */ {
		data512_t ind;
		hash_sha512(ind, msg->indicator, INDICATOR_LEN);
		buflen = cipher_encryptdata(buffer+sizeof(id_t)+sizeof(len_t)+enc_start, buffer+sizeof(id_t)+sizeof(len_t)+enc_start, buflen-enc_start, ind, msg->key)+enc_start;
	}
	for(len_t i = 0; i < sizeof(id_t); i++) /* add the id of the client at the start */
		buffer[i] = (msg->cid >> (i*8)) & 0xff;
	for(len_t i = 0; i < sizeof(len_t); i++) /* add the length of the message at the start after the id */
		buffer[sizeof(id_t)+i] = (buflen >> (i*8)) & 0xff;

	/* send the message */
	len_t len_send = 0;
	while(len_send < buflen+sizeof(len_t)) {
		len_t tmp_len = send(sock, buffer+len_send, sizeof(id_t)+sizeof(len_t)+buflen-len_send, 0);
		if(tmp_len == -1) {
			free(buffer);
			return ERROR;
		} else
			len_send += tmp_len;
	}

	free(buffer);
	return OK;
}

// no field in msg will be freed by this function!
error_t net_recvmsg(int sock, msgbuf_t* msg) {
	uint8_t bufferhead[sizeof(id_t)+sizeof(len_t)];
	len_t len = recv(sock, bufferhead, sizeof(id_t)+sizeof(len_t), MSG_DONTWAIT); /* recv the id and length of the message */
	if(len >= 1) {
		len_t tmp_len = recv(sock, bufferhead, sizeof(id_t)+sizeof(len_t)-len, MSG_WAITALL); /* recv the rest of the id and length */
		len += tmp_len;
		if(len == sizeof(id_t)+sizeof(len_t)) {
			msg->cid = 0;
			for(len_t i = 0; i < sizeof(id_t); i++)
				msg->cid |= (id_t)bufferhead[i] << (i*8);
			len_t buflen = 0;
			for(len_t i = 0; i < sizeof(len_t); i++)
				buflen |= (len_t)bufferhead[sizeof(id_t)+i] << (i*8);
			uint8_t* buffer = (uint8_t*)malloc(buflen);
			tmp_len = recv(sock, buffer, buflen, MSG_WAITALL); /* recv the actual message */
			if(tmp_len == buflen) {
				msg->flag = 0;
				char* msgre = (char*)buffer;
				if(*msgre == '~') /* the message is encrypted */ {
					msgre++;
					data512_t ind;
					hash_sha512(ind, msgre, INDICATOR_LEN);
					msgre += INDICATOR_LEN;
					buflen = cipher_decryptdata((uint8_t*)msgre, (uint8_t*)msgre, buflen-1-INDICATOR_LEN, ind, msg->key);
					if(strncmp(msgre, ":ENCRYPTED", 10) != 0) /* couldn't decrypt the data */ {
						free(buffer);
						return ENC_DATA;
					}
					msgre += 10;
					buflen -= 10;
					msg->flag |= FLAG_MSG_ENC;
				}
				/* extract the header information */
				len_t headlen = strlen(msgre);
				if(headlen >= buflen) {
					free(buffer);
					return ERROR;
				}
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
					grouplen = pipepos-atpos-1;
				else
					grouplen = headlen-atpos-1;

				/* fill the message buffer */
				msg->name = (char*)malloc(namelen+1);
				memcpy(msg->name, msgre, namelen);
				msg->name[namelen] = 0;
				if(grouplen != 0) {
					msg->group = (char*)malloc(grouplen+1);
					memcpy(msg->group, msgre+atpos+1, grouplen);
					msg->group[grouplen] = 0;
				} else
					msg->group = NULL;

				if(pipepos != -1 && strcmp(msgre+pipepos+1, "TYP") == 0) /* the message only contains typing information */ {
					msg->flag |= FLAG_MSG_TYP;
					msg->data_len = 0;
					msg->data = NULL;
				} else if(pipepos != -1 && strcmp(msgre+pipepos+1, "ENT") == 0) /* the message only contains enter information */ {
					msg->flag |= FLAG_MSG_ENT;
					msg->data_len = 0;
					msg->data = NULL;
				} else if(pipepos != -1 && strcmp(msgre+pipepos+1, "EXT") == 0) /* the message only contains exit information */ {
					msg->flag |= FLAG_MSG_EXT;
					msg->data_len = 0;
					msg->data = NULL;
				} else if(datalen == 0) {
					msg->data_len = 0;
					msg->data = NULL;
				} else {
					msg->data_len = datalen;
					msg->data = (char*)malloc(datalen);
					memcpy(msg->data, msgre+headlen+1, datalen);
				}
				free(buffer);
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
		else /* if(tmp_len == -1) */
			return ERROR;
	} else if(len == 0)
		return CONNECTION_CLOSED;
	else /* if(len == -1) */
		return NO_DATA;
	return OK;
}
