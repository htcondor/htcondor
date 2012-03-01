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

#ifdef WIN32
#include "condor_header_features.h"
#include "condor_sys_nt.h"
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

struct rusage myrusage;

FILE* outFile = NULL;
char *output;
int slowexit = 0;

#ifdef WIN32
DWORD WINAPI get_timing(__in LPVOID lpParameter)
{
	int fd;
	size_t len;
	int ret;
	float rtime = 0.0;
	int lockstatus;
	char bigbuff[512];

	FILETIME ctime;		//create time
	FILETIME extime;	//exit time
	FILETIME ktime;		//kernel mode time
	FILETIME umtime;		//user mode time

	unsigned int sleeptime = *(unsigned int*)lpParameter;

	Sleep(sleeptime*1000);

	BOOL gottime = GetProcessTimes(
		GetCurrentProcess(),
		&ctime,
		&extime,
		&ktime,
		&umtime);

	fd = open((const char*)output, O_WRONLY | O_APPEND, 0777);
	if(fd == -1) {
		fprintf(stderr, "Can't open %s", output );
		exit(1);
	}

	rtime = ((ULARGE_INTEGER*)&umtime)->QuadPart * .001;
	ret = sprintf_s(bigbuff, 512, "done <%f>\n", rtime);
	if(ret < 0)
	{
		fprintf(stderr, "sprintf_s failed: %d\n", GetLastError() );
		exit(1);
	}

	len = ret;

	ret = write(fd, (const void *)&bigbuff, len);
	if( ret == -1 ) {
		fprintf(stderr, "hmmm error writing file: %s\n",strerror(errno));
	}
	close(fd);
	/*printf("slowexit set to %d\n",slowexit);*/
	if(slowexit != 0){
		sleep(slowexit);
	}

	exit(0);

	return 0;
}
#else
void
catch_sigalrm( int  /*sig*/ )
{
	int fd;
	size_t len;
	int ret;
	float rtime = 0.0;
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
#endif
int main(int argc, char ** argv)
{
	/*
	printf("args in for alarm is %d\n",argc);
	*/
#ifndef WIN32
	signal( SIGALRM, catch_sigalrm );
#endif
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
#ifdef WIN32
	unsigned int sleeptime = atoi(argv[1]);
	HANDLE tHandle = CreateThread(NULL, 0, get_timing, &sleeptime, 0, NULL);
#else
	alarm((unsigned int)atoi(argv[1]));
#endif
	while(1) { ; }

	return 0;
}
