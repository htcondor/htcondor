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

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main( int argc, char *argv[] )
{
	int fd, pos;
	char *buffer = "hello world!\n";
	char input[80];

	if( argc!=2 ) {
		printf("Use: %s <filename>\n",argv[0]);
		return -1;
	}

	unlink(argv[1]);
	fd = open(argv[1],O_WRONLY|O_CREAT,0700);
	if(fd<0) {
		perror(argv[1]);
		return -1;
	}
	write(fd,buffer,strlen(buffer));
	close(fd);

	fd = open(argv[1],O_RDWR|O_TRUNC,0700);
	if(fd<0) {
		perror(argv[1]);
		return -1;
	}

	pos = read( fd, input, 80 );
	if(pos<0) {
		perror("read");
		return -1;
	}

	if(pos!=0) {
		printf("FAILURE (%d) (%s)\n",pos,input);
	} else {
		printf("SUCCESS\n");
	}
	close(fd);

	return 0;	
}

