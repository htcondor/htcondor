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
