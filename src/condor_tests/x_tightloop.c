
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>
/*
#include <string.h>
#include <sys/file.h>
*/

struct rusage myrusage;

FILE* OUT = NULL;
char *output;
int fastexit = 0;


void
catch_sigalrm( int sig )
{
	int fd;
	size_t len;
	size_t ret;
	int lockstatus;
	char bigbuff[512];

	getrusage(RUSAGE_SELF, &myrusage);
	fd = open((const char*)output, O_WRONLY | O_APPEND, 0777);
	if(fd == -1) {
		fprintf(stderr, "Can't open %s", output );
		exit(1);
	}
	sprintf( bigbuff, "done <%d.%d>\n", myrusage.ru_utime.tv_sec,myrusage.ru_utime.tv_usec);
	len = strlen((const char*)bigbuff);
	ret =  write(fd, (const void *)&bigbuff, len);
	if( ret == -1 ) {
		fprintf(stderr, "hmmm error writing file: %s\n",strerror(errno));
	}
	close(fd);
	if(fastexit == 0){
		sleep(15);
	}
	exit( 0 );
}

main(int argc, char ** argv)
{
	/*
	printf("args in for alarm is %d\n",argc);
	*/

	signal( SIGALRM, catch_sigalrm );

	if(argc < 3) {
		printf("Usage: %s \n",argv[0]);
		exit(1);
	} 

	/*
	printf("sleep = %s log = %s\n",argv[1], argv[2]);
	*/
	
	if(argc == 4){
		printf("setting no wait\n");
		fastexit = argv[3];
	}

	output = argv[2];

	alarm((unsigned int)atoi(argv[1]));

	while(1) { ; }
}
