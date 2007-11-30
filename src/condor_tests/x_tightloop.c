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
int slowexit = 0;


void
catch_sigalrm( int sig )
{
	int fd;
	size_t len;
	size_t ret;
	float rtime = 0.0;
	int lockstatus;
	char bigbuff[512];

	getrusage(RUSAGE_SELF, &myrusage);
	/*printf("Awake now\n");*/
	fd = open((const char*)output, O_WRONLY | O_APPEND, 0777);
	if(fd == -1) {
		fprintf(stderr, "Can't open %s", output );
		exit(1);
	}
	/*printf("Awake now have log output\n");*/
	rtime = myrusage.ru_utime.tv_sec + (myrusage.ru_utime.tv_usec * .000001);
	sprintf( bigbuff, "done <%f>\n", rtime);
	/*printf("have time calculated.......<%s>\n",bigbuff);*/
	len = strlen((const char*)bigbuff);
	ret =  write(fd, (const void *)&bigbuff, len);
	if( ret == -1 ) {
		fprintf(stderr, "hmmm error writing file: %s\n",strerror(errno));
	}
	close(fd);
	/*printf("slowexit set to %d\n",slowexit);*/
	if(slowexit != 0){
		sleep(slowexit);
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
		slowexit = atoi(argv[3]);
		/*
		printf("setting wait to %d\n",slowexit);
		*/
	}

	output = argv[2];

	alarm((unsigned int)atoi(argv[1]));

	while(1) { ; }
}
