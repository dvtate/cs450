#include <stdlib.h>
#include <stdio.h>

/**
* Print entire contents of a file to stdout
*/
void printFile(FILE* fp) {
	// TODO use static buffer size for better performance
	//   I wrote grep first so was easier to copy-pasta
	char* buff = NULL;
	size_t buff_size = 0;
	while (getline(&buff, &buff_size, fp) >= 0)
		printf("%s", buff);
}


int main (int argc, char** argv) {
	// Spec said to not implement reading from stdin
	if (argc < 2)
		return 0;
	
	// Iterate through files list
	for (unsigned short i = 1; i < argc; i++) {
		// Open file
		FILE* fp = fopen(argv[i], "r");
		if (!fp) {
			puts("my-cat: cannot open file");
			return 1;
		}
		
		// Cat file
		printFile(fp);
		fclose(fp);
	}
}
