#include <stdio.h>
#include <fcntl.h>

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
