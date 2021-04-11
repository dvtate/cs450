#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/**
*
*/
FILE* openInputFile(char* file_name) {
	FILE* ret = fopen(file_name, "r");
	if (!ret) {
		puts("my-zip: cannot open file");
		exit(1);
	}
	return ret;
}

int main(int argc, char** argv) {
	// Handle No args
	if (argc < 2) {
		puts("my-zip: file1 [file2 ...]");
		return 1;
	}

	// State variables
	uint32_t dups = 1;
	unsigned short fnum = 2;
	FILE* fp = openInputFile(argv[1]);
	char prev = fgetc(fp);
	
	
	// For each char in each file in order given
	while (1) {
		char c = fgetc(fp);
	
		// switch files on eof
		if (c == EOF && fnum < argc) {
			fclose(fp);
			fp = openInputFile(argv[fnum++]);
			continue;
		} else if (c == EOF) {
			break;
		}
	
		// output RLE
		if (c == prev) {
			dups++;
		} else {
			fwrite(&dups, sizeof(dups), 1, stdout);
			fputc(prev, stdout);
			dups = 1;
			prev = c;
		}
	}
	
	// 
	fwrite(&dups, sizeof(dups), 1, stdout);
	fputc(prev, stdout);

	// Clean up	
	fclose(fp);
	fflush(stdout);

	return 0;
}
