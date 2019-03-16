
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "random.h"
#include "cipher.h"

int main(int argc, char** argv) {
	data512_t key;
	data512_t ind;
	char buffer[1025];
	len_t len;

	len = read(STDIN_FILENO, buffer, 1024);
	if(buffer[len-1] == '\n') len--;
	buffer[len] = 0;

	for(len_t i = 0; i < len; i++)
		fprintf(stdout, "%.2hhx ", buffer[i]);
	fprintf(stdout, "\n");

	random_seed_unix_urandom();
	random_get(key);
	random_get(ind);
	len = cipher_encryptdata(buffer, buffer, len, ind, key);
	buffer[len] = 0;

	for(len_t i = 0; i < len; i++)
		fprintf(stdout, "%.2hhx ", buffer[i]);
	fprintf(stdout, "\n");

	len = cipher_decryptdata(buffer, buffer, len, ind, key);
	buffer[len] = 0;

	for(len_t i = 0; i < len; i++)
		fprintf(stdout, "%.2hhx ", buffer[i]);
	fprintf(stdout, "(%s)\n", buffer);

	return 0;
}
