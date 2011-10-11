#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	int delay = 10;
	int i;
	char line[1024];
	char *file = "./output_file";
	FILE *fout = NULL;

	if (argc == 2) {
		delay = atoi(argv[1]);
	}

	if (argc == 3) {
		delay = atoi(argv[1]);
		file = argv[2];
	}

	if (delay < 10) {
		printf("Delay has a minimum of 10 seconds\n");
		fflush(NULL);
		delay = 10;
	}

	fout = fopen(file, "a");
	if (fout == NULL) {
		printf("Can't open output file! %d(%s)\n", errno, strerror(errno));
	}

	/* If there is any stdin, copy it to output, then start the work when
		stdin closes */
	printf("Dumping stdin:\n");
	fprintf(fout, "Dumping stdin:\n");
	fflush(NULL);
	while(fgets(line, 1024, stdin) != NULL) {
		printf("stdin: %s", line);
		fprintf(fout, "stdin: %s", line);
		fflush(NULL);
	}

	printf("Doing work for %d seconds\n", delay);
	fprintf(fout, "Doing work for %d seconds\n", delay);
	fflush(NULL);

	for (i = 0; i < delay; i++) {
		printf("i = %d\n", i);
		fprintf(fout, "i = %d\n", i);
		fflush(NULL);
		sleep(1);
	}

	printf("Done.\n");
	fprintf(fout, "Done.\n");
	fflush(NULL);

	if (fclose(fout) < 0) {
		printf("Failed to close fout? %d(%s)\n", errno, strerror(errno));
		fflush(NULL);
	}

	return 0;
}
