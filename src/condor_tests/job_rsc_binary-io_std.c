/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* This test is to see if we can read/write (specifically) binary data to
a file in stduniv. There had been a bug where someone had used a strdup()
on an opaque buffer in the file table layer which would do all sorts of
bad things if zeros were or were not present in the opaque buffer. So
this test will test that style of failure in our codebase. */

enum
{
	/* I want 256 copies of a specially computed 256 byte block */
	BSIZE = 256 * 256
};

int main(void)
{
	FILE *f;
	int i, j;
	unsigned char write_array[BSIZE];
	unsigned char read_array[BSIZE];
	char file[1024];

	sprintf(file, "binary-io.%lu.out", (unsigned long)getpid());

	printf("My specific test file is: %s\n", file);

	f = fopen(file, "w");
	if (f == NULL) {
		printf("Couldn't open test writing file '%s': %d(%s)\n",
			file, errno, strerror(errno));
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}

	/* Having zero be the first binary character written is highly advantageous
		in case the syscall library happens to treat the buffer as ascii
		instead of an array of opaque data.
	*/

	for (j = 0; j < BSIZE; j++) {
		write_array[j] = j % 256;
	}

	if (fwrite(write_array, BSIZE, 1, f) != 1) {
		printf("Failed to write binary block: %d(%s)\n", 
			errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (fclose(f) == -1) {
		printf("Failed to close write file pointer: %d(%s)\n", 
			errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	f = fopen(file, "r");
	if (f == NULL) {
		printf("Couldn't open test reading file '%s': %d(%s)\n",
			file, errno, strerror(errno));
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}

	/* Now read it in and check each block size that it is the correct
		information */

	if (fread(read_array, BSIZE, 1, f) != 1) {
		printf("Failed to read binary block on iteration %d: %d(%s)\n",
			i, errno, strerror(errno));
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}

	/* check to make sure it is what I thought it was going to be */
	for (i = 0; i < BSIZE; i++) {
		if (read_array[i] != write_array[i]) {
			printf("ERROR: Read binary file differs from written binary "
				"file at %d. 'od -Ax -t x1 <file> might help you figure it "
				"out.\n", i);
			printf("FAILURE\n");
			exit(EXIT_FAILURE);
		}
	}

	if (fclose(f) == -1) {
		printf("Failed to close read file pointer: %d(%s)\n", 
			errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	printf("Test passed, removing file: %s\n", file);
	if (unlink(file) < 0) {
		printf("Failed to unlink test file '%s': %d(%s)\n", 
			file, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	printf("SUCCESS\n");
	
	return 0;
}

