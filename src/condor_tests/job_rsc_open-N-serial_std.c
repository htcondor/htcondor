/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
