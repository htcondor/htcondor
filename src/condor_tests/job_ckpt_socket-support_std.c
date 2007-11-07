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
Get and display the first few lines of a url.
Do it several times.
*/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

extern int errno;

#define URL_SIZE 256
#define BUFFER_SIZE 80
#define HTTP_PORT 80

int open_tcp( char *server, int port );
FILE * fopen_http( char *server, char *path );

void loop_delay( long howlong )
{
	long i;

	for(i=0;i<(howlong*30000000);i++) {}
}


int main( int argc, char *argv[] )
{
	FILE *stream;
	char *url;
	int loops,i,j;

	char method[URL_SIZE];
	char server[URL_SIZE];
	char path[URL_SIZE];
	char buffer[BUFFER_SIZE];

	if(argc!=3) {
		printf("Use: %s <url> <loops>\n",argv[0]);
		return EXIT_FAILURE;
	}

	url = argv[1]; 
	loops = atoi(argv[2]);

	printf("url:   %s\n",url);
	printf("loops: %d\n",loops);

	sscanf(url,"%[^:]://%[^/]%s",method,server,path);

	printf("method: %s\n",method);
	printf("server: %s\n",server);
	printf("path:   %s\n",path);

	for(i=0; i<loops; i++) {

		printf("Making connection...\n");

		stream = fopen_http(server,path);
		if(!stream) {
			printf("FAILURE: fopen_http(): %s\n",strerror(errno));
			return -1;
		}

		printf("Getting data...\n");

		for(j=0; j<5; j++) {
			fgets(buffer,BUFFER_SIZE,stream);
			if(ferror(stream)) {
				printf("FAILURE: stream has an error: %s\n",strerror(errno));
				return -1;
			}
			printf(">>> %s",buffer);
			loop_delay(1);
		}

		printf("Closing connection...\n");
		fclose(stream);

		loop_delay(1);
	}

	printf("SUCCESS\n");

	return EXIT_SUCCESS;
}

FILE *fopen_http( char *server, char *path )
{
	int fd;
	FILE *stream;
	char buffer[BUFFER_SIZE];

	/* Make the connection */
	fd = open_tcp(server,HTTP_PORT);

	/* Put a buffered stream on the fd */
	stream = fdopen(fd,"r+");
	if(!stream) {
		printf("FAILURE: fdopen(): %s\n", strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	}

	/* Send the request */
	fprintf(stream,"GET %s HTTP/1.0\n\n",path);
	fflush(stream);

	/* Eat up any header data until a blank line */
	while(fgets(buffer,BUFFER_SIZE,stream)) {
		if(strlen(buffer)==2) {
			return stream;
		}
	}

	/* If we got here, the end of the header was not found */
	fclose(stream);
	return NULL;
}

int open_tcp( char *server, int port )
{
	struct sockaddr_in addr;
	struct hostent *host;
	int fd;

	host = gethostbyname( server );
	if(!host) {
		printf("FAILURE: gethostbyname failed: h_errno = %d\n", h_errno);
		exit(EXIT_FAILURE);
	}

	addr.sin_port = htons(port);
	addr.sin_family = host->h_addrtype;
	addr.sin_addr = *((struct in_addr *) host->h_addr_list[0]);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd<0)
	{
		printf("FAILURE: socket(): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(connect(fd, (struct sockaddr *) &addr, sizeof(addr))<0) {
		printf("FAILURE: connect(): %s\n", strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	}

	return fd;
}
