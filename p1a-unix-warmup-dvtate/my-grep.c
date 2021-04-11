#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
* Print all lines in file containing term
*/
void grepFile(FILE* fp, const char* term) {
	char* buff = NULL;
	size_t buff_size = 0;
	while (getline(&buff, &buff_size, fp) >= 0)
		if (strstr(buff, term))
			printf("%s", buff);
}


int main(int argc, char** argv) {
	// Invalid args 
	if (argc < 2) {
		puts("my-grep: searchterm [file ...]");
		return 1;
	}
	
	// Grep from stdin
	if (argc == 2) {
		grepFile(stdin, argv[1]);		
		return 0;
	}
	
	// Grep from list of files
	for (unsigned short i = 2; i < argc; i++) {
		// Open file
		FILE* fp = fopen(argv[i], "r");
		if (!fp) {
			puts("my-grep: cannot open file");
			return 1;	
		}
		
		// Grep file
		grepFile(fp, argv[1]);
		fclose(fp);
	}
	return 0;
}
