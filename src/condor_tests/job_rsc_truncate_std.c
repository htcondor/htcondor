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

