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
#include <stdio.h>
#include <fcntl.h>

int read();
int write();
int exit();

int main(int argc, char* argv[])
{
	int rfd;
	int wfd;
	char buf[256];
	int idx = 0;
	int readcnt = 0;
	int writecnt = 0;

    char* testfilein = argv[1];
    char* testfileout = argv[2];
	printf("Test file in is %s\n",testfilein);
	printf("Test file out is %s\n",testfileout);
	rfd = open(testfilein,O_RDONLY,0);
	wfd = open(testfileout,O_WRONLY| O_CREAT,0666);
	printf("rfd is %d\n",rfd);
	printf("wfd is %d\n",wfd);

	for( idx = 0; idx != 256; idx++)
	{
		buf[idx] = '\0';
	}

	readcnt = read( rfd, buf, 256 );
	writecnt = write( wfd, buf, readcnt);
	printf("Read %d and wrote %d\n",readcnt,writecnt);
	printf("%s",buf);
	exit(0);
}
