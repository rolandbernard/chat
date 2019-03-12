
#include <stdio.h>
#include <string.h>

#include "hash.h"

int main(int argc, char** argv) {

	hash512_t hash;
	hash_sha512(hash, argv[1], strlen(argv[1]));
	for(len_t i = 0; i < 64; i++)
		fprintf(stdout, "%.2hhx", hash[i]);
	fprintf(stdout, "\n");

	return 0;
}
