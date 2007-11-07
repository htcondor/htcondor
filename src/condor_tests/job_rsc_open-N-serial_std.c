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

/*
This program opens, closes, and deletes a long series of files in sequence.  It is meant to aggravate the file instrumenting code which should stop leaking memory after about 100 files have been opened.  Perhaps it will be useful for tracking down fd leaks as well.
*/

#ifndef IRIX62
#define _POSIX_C_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#if defined(IRIX)
#include <sys/select.h>
#endif
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>


int main( int argc, char *argv[] )
{
	char filename[_POSIX_PATH_MAX];
	int success,max,i;
	FILE *file;

	if( argc!=2 ) {
		fprintf(stderr,"Use: %s <number-of-files>\n",argv[0]);
		fprintf(stdout,"FAILURE\n");
		return -1;
	}

	max = atoi(argv[1]);

	for( i=0; i<max; i++ ) {
		sprintf(filename,"temp.%d",i);
		file = fopen(filename,"w+");
		if(!file) {
			fprintf(stderr,"Couldn't open %s: %s\n",filename,strerror(errno));
			fprintf(stdout,"FAILURE\n");
			return -1;
		}
		success = fprintf(file,"This is file %d\n",i);
		if( success<0 ) {
			fprintf(stderr,"Couldn't print to %s: %s\n",filename,strerror(errno));
			fprintf(stdout,"FAILURE\n");
			return -1;
		}
		success = fclose(file);
		if( success!=0 ) {
			fprintf(stderr,"Couldn't close %s: %s\n",filename,strerror(errno));
			fprintf(stdout,"FAILURE\n");
			return -1;
		}
		success = unlink(filename);
		if( success==-1 ) {
			fprintf(stderr,"Couldn't unlink %s: %s\n",filename,strerror(errno));
			fprintf(stdout,"FAILURE\n");
			return -1;
		}
	}

	fprintf(stdout,"SUCCESS\n");

	return 0;
}
