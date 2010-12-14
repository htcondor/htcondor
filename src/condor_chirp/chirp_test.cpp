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


#include "chirp_client.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 65536

char buffer[BUFFER_SIZE];
char buffer2[BUFFER_SIZE];

#if defined(WIN32)
#define TEST_WRITE_FILE ".\\chirp test.tmp"
#define TEST_READ_FILE  ".\\testread"
#define TEST_DIR		".\\testdir"
#else
#define TEST_WRITE_FILE "/tmp/chirp test.tmp"
#define TEST_READ_FILE  "/etc/hosts"
#define TEST_DIR		"/tmp/chirpdir"
#endif

int main( int argc, char *argv[] )
{
	struct chirp_client *client;
	int result,result2;
	int rfd, wfd;
	int actual;
	int total_size;

	client = chirp_client_connect_default();
	if(!client) {
		fprintf(stderr,"couldn't connect to server: %s\n",strerror(errno));
		return 1;
	}

	rfd = chirp_client_open(client,TEST_READ_FILE,"r",0);
	if(rfd<0) {
		fprintf(stderr,"couldn't open /etc/hosts: %s\n",strerror(errno));
		return 1;
	}

	wfd = chirp_client_open(client,TEST_WRITE_FILE,"wtc",0777);
	if(wfd<0) {
		fprintf(stderr,"couldn't open %s: %s\n",TEST_WRITE_FILE,strerror(errno));
		return 1;
	}

	total_size = 0;

	while(1) {
		int pos = chirp_client_lseek(client,rfd,0,1);

		result = chirp_client_read(client,rfd,buffer,BUFFER_SIZE);

		if(result==0) break;

		if(result<0) {
			fprintf(stderr,"couldn't read data: %s\n",strerror(errno));
			return 1;
		}

		total_size += result;

		/*back up and read again, just to verify seek*/
		chirp_client_lseek(client,rfd,pos,0);
		result2 = chirp_client_read(client,rfd,buffer2,BUFFER_SIZE);
		if(result2 == 0 || result2 < 0) {
			fprintf(stderr,"couldn't re-read data: %s\n",strerror(errno));
			return 1;
		}

		if(result2 > result) result2 = result;
		if(memcmp(buffer,buffer2,result2)) {
			fprintf(stderr,"data does not match!\n");
			return 1;
		}

		/*make sure we are at end of original read*/
		result = chirp_client_lseek(client,rfd,pos+result,0);

		actual = chirp_client_write(client,wfd,buffer,result);
		if(actual!=result) {
			fprintf(stderr,"couldn't write data: %s\n",strerror(errno));
			return 1;
		}

		result = chirp_client_fsync(client,wfd);
		if(result != 0) {
			fprintf(stderr,"couldn't fsync: %s\n",strerror(errno));
			return 1;
		}
	}

	/*do some more seek tests*/
	result = chirp_client_lseek(client,wfd,0,1);
	if(result != total_size) {
		fprintf(stderr,"lseek(0,1) = %d/%d\n",result,total_size);
		return 1;
	}

	result = chirp_client_lseek(client,wfd,0,0);  /*rewind*/
	if(result != 0) {
		fprintf(stderr,"lseek(0,0) = %d\n",result);
		return 1;
	}

	result = chirp_client_lseek(client,wfd,total_size,0);
	if(result != total_size) {
		fprintf(stderr,"lseek(%d,0) = %d/%d\n",total_size,result,total_size);
		return 1;
	}

	result = chirp_client_lseek(client,wfd,-total_size,2);  /*rewind*/
	if(result != 0) {
		fprintf(stderr,"lseek(-%d,2) = %d/0\n",total_size,result);
		return 1;
	}


	chirp_client_close(client,rfd);
	chirp_client_close(client,wfd);

	result = chirp_client_rename(client,TEST_WRITE_FILE,TEST_WRITE_FILE ".renamed");
	if(result<0) {
		fprintf(stderr,"couldn't rename %s: %s\n",TEST_WRITE_FILE,strerror(errno));
		return 1;
	}

	result = chirp_client_unlink(client,TEST_WRITE_FILE ".renamed");
	if(result<0) {
		fprintf(stderr,"couldn't rename %s: %s\n",TEST_WRITE_FILE,strerror(errno));
		return 1;
	}

	result = chirp_client_mkdir(client,TEST_DIR,766);
	if(result<0) {
		fprintf(stderr,"couldn't mkdir %s :%s\n", TEST_DIR,strerror(errno));
	}

	result = chirp_client_rmdir(client,TEST_DIR);
	if(result<0) {
		fprintf(stderr,"couldn't rmdir %s :%s\n", TEST_DIR, strerror(errno));
	}

	chirp_client_disconnect(client);
	return 0;
}
