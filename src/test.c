
#include <stdio.h>
#include <string.h>

#include "random.h"

#define NUM_ROLL 10000000
#define NUM_POS 6

int main(int argc, char** argv) {

	len_t acc[NUM_POS] = { 0 };
	random_seed_unix_urandom();
	for(len_t i = 0; i < NUM_ROLL; i++) {
		data256_t randdata;
		random_get(randdata);
		uint32_t ind = 0;
		for(len_t j = 0; j < sizeof(randdata); j++)
			ind ^= randdata[j] << ((j*8)%(sizeof(ind)*8));
		acc[ind%NUM_POS]++;
	}
	for(len_t i = 0; i < NUM_POS; i++)
		printf("%i: %i\n", i+1, acc[i]);

	return 0;
}
