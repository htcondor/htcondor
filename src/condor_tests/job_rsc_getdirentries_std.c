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


/* getdirentries is a horrible call.

Let's inspect the prototype of this call:

ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep);

What this call does is fill in the buf array with dirent structures until
it reaches nbytes. Then it returns the total number of bytes written into
the buf array. When -1 is returned, an error happened, when 0 is returned,
there are no more directory entries. basep is an off_t that represents how
far into the file representing the directory we have read.

Each time getdirentries is called, the new entries are written at the
beginning of the buf array. 

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#if defined(HPUX)
#include <ndir.h>
#else
#include <dirent.h>
#endif

#define MATCH (0)

// This appears to be the structure of a dent on Linux. Note that the
// wtf variable seems to not be documented.
// This works on Linux 2.2.19...
// Erik 9/2001
typedef struct kernel_dirent
{
	long int d_ino;
	long d_off;
	unsigned short d_reclen;
	char wtf;
	char d_name[255];
} DENT;

/* create the testing directory, the files in it, and return an open fd
	to the test directory */
int create_test_files(void)
{
	int ret;
	int fd;

	ret = mkdir("testdir", 0777);	
	if(ret == -1 && errno != EEXIST) {
		perror("mkdir");
		return -1;
	}

    // now let's create some files...
	fd = open("testdir/first", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
	if(fd < 0) { 
		perror("Open First"); 
		return -1; 
	}
	close(fd);

	fd = open("testdir/second", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
	if(fd < 0) { 
		perror("Open Second"); 
		return -1; 
	}
	close(fd);

	fd = open("testdir/third", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
	if(fd < 0) { 
		perror("Open Third"); 
		return -1; 
	}
	close(fd);

	fd = open("testdir", O_RDONLY);	
	if(fd < 0) { perror("Open Directory"); return -1; }

	return fd;
}

int cleanup_test_files(void)
{
	int ret;

	ret = unlink("testdir/first");
	if(ret == -1) {
		perror("unlink testdir/first");
		return -1;
	}
	ret = unlink("testdir/second");
	if(ret == -1) {
		perror("unlink testdir/second");
		return -1;
	}
	ret = unlink("testdir/third");
	if(ret == -1) {
		perror("unlink testdir/third");
		return -1;
	}
	ret = rmdir("testdir");
	if(ret == -1) {
		perror("unlink testdir");
		return -1;
	}

	return 0;
}

int main(void) 
{
	off_t basep = 0;
	int fd;
	char buf[8192];
	DENT *window;
	unsigned long voidstar;
	int numfiles, expectednumfiles, totalfiles;
	ssize_t max_bytes, bytes_processed;

	/* Statistics about what we find and how to keep track of it */
	numfiles = 0;
	expectednumfiles = 5; /* '.', '..', 'first', 'second', 'third' */
	totalfiles = 0;

	if ((fd = create_test_files()) <= 0) {
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}

	max_bytes = getdirentries(fd, buf, 8192, &basep);
	if(max_bytes < 0) {
		perror("getdirentries: "); return -1;
	}
	bytes_processed = 0;

	// Walk the list we got back from dirents	
	voidstar = (unsigned long int)buf;
	window = (DENT *)voidstar;

	// While there are sets of dir entries to process...
	while(max_bytes > 0) {

		/* process the array of DENTs */
		while(bytes_processed != max_bytes) {

			/* 
			printf("DIR ENTRY: '%s' [max_bytes = %d]\n", 
				window->d_name==NULL?"(null)":window->d_name, 
				max_bytes);	
			*/

			if (window->d_reclen == 0) {
				printf("Aborting. How can window->d_reclen be zero?\n");
				fflush(NULL);
				exit(EXIT_FAILURE);
			}
	
			/* check to make sure I found what I wanted */
			if (strcmp(window->d_name, ".") == MATCH ||
				strcmp(window->d_name, "..") == MATCH ||
				strcmp(window->d_name, "first") == MATCH ||
				strcmp(window->d_name, "second") == MATCH ||
				strcmp(window->d_name, "third") == MATCH)
			{
				numfiles++;
			}
	
			totalfiles++;

	
			/* go to the next DENT structure. They are stretchy, which is
				why we do it like this. */
			bytes_processed += window->d_reclen;

			voidstar = voidstar + window->d_reclen;
			window = (DENT *)voidstar;
		}
	
		/* get the next round of entries to process */
		max_bytes = getdirentries(fd, buf, 8192, &basep);
		if(max_bytes < 0) {
			perror("getdirentries: "); return -1;
		}
		bytes_processed = 0;

		// Walk the list we got back from dirents	
		voidstar = (unsigned long int)buf;
		window = (DENT *)voidstar;
	}
	close(fd);

	if (cleanup_test_files() < 0) {
		printf("FAILURE\n");
		exit(EXIT_FAILURE);
	}
	
	/* this is not a transitive boolean, because it implies I found
		_certain_ expected files. */
	if (numfiles == expectednumfiles && expectednumfiles == totalfiles) {
		fprintf(stdout, "completed successfully\n");
	} else {
		fprintf(stdout, "expectednumfiles = %d\n", expectednumfiles);
		fprintf(stdout, "totalfiles = %d\n", totalfiles);
		fprintf(stdout, "numfiles = %d\n", numfiles);
		fprintf(stdout, "failed\n");
	}
	return 0;
}
