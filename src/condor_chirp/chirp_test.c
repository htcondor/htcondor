
#include "chirp_client.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 65536

char buffer[BUFFER_SIZE];

int main( int argc, char *argv[] )
{
	struct chirp_client *client;
	int result;
	int rfd, wfd;
	int actual;

	client = chirp_client_connect_default();
	if(!client) {
		fprintf(stderr,"couldn't connect to server: %s\n",strerror(errno));
		return 1;
	}

	rfd = chirp_client_open(client,"/etc/hosts","r",0);
	if(rfd<0) {
		fprintf(stderr,"couldn't open /etc/hosts: %s\n",strerror(errno));
		return 1;
	}

	wfd = chirp_client_open(client,"/tmp/outfile","wtc",0777);
	if(wfd<0) {
		fprintf(stderr,"couldn't open /tmp/outfile: %s\n",strerror(errno));
		return 1;
	}

	while(1) {
		result = chirp_client_read(client,rfd,buffer,BUFFER_SIZE);
		if(result==0) break;

		if(result<0) {
			fprintf(stderr,"couldn't read data: %s\n",strerror(errno));
			return 1;
		}

		actual = chirp_client_write(client,wfd,buffer,result);
		if(actual!=result) {
			fprintf(stderr,"couldn't write data: %s\n",strerror(errno));
			return 1;
		}
	}

	chirp_client_close(client,rfd);
	chirp_client_close(client,wfd);

	result = chirp_client_rename(client,"/tmp/outfile","/tmp/outfile.renamed");
	if(result<0) {
		fprintf(stderr,"couldn't rename /tmp/outfile: %s\n",strerror(errno));
		return 1;
	}

	result = chirp_client_unlink(client,"/tmp/outfile.renamed");
	if(result<0) {
		fprintf(stderr,"couldn't rename /tmp/outfile: %s\n",strerror(errno));
		return 1;
	}

	chirp_client_disconnect(client);
	return 0;
}
