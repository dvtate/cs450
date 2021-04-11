#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

int main(int argc, char** argv) {
	// Handle No args
	if (argc < 2) {
		puts("my-unzip: file1 [file2 ...]");
		return 1;
	}


	// Bitfield for reading from file
	struct read_head_t {
		// Number of occurances
		uint32_t n ;
		
		// Relevant character
		char c;
	} rh;
	
	for (unsigned fnum = 1; fnum < argc; fnum++) {
		// Open file
		FILE* fp = fopen(argv[fnum], "r");
		if (!fp) {
			puts("my-unzip: cannot open file");
			return 1;
		}

		// Decompress file
		// Note: can't use sizeof read_head_t because compiler may add padding
		while (fread(&rh, 5, 1, fp) == 1)
			for (; rh.n > 0; rh.n--)
				fputc(rh.c, stdout);
	}
	
	return 0;
}
