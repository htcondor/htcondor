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
** This program just computes for an approximate amount of time, then
** exits with status 0.  The approximate time in seconds is specified
** on the command line.  It just counts in a loop, so the actual time
** will vary greatly depending on what kind of CPU it gets run on.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


#if defined(LINUX) && defined(GLIBC)
#define getrusage __hide_getrusage
#endif

#if defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/time.h>
#endif

#include <sys/resource.h>

#if defined(LINUX) && defined(GLIBC)
#undef getrusage
#ifdef __cplusplus
extern "C" int getrusage(int, struct rusage * );
#else
extern int getrusage(int, struct rusage * );
#endif
#endif

#ifdef Solaris251
#ifdef __cplusplus
extern "C" int getrusage(int who, struct rusage *rusage);
#else
int getrusage(int who, struct rusage *rusage);
#endif
#endif

#include "x_fake_ckpt.h"
#include "x_waste_second.h"

extern int CkptInProgress;
extern int errno;

#define MEG (1024 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

void do_loop( int count, int ckptfreq );
void usage();

int main( int argc, char *argv[])
{
	int		count;
	int		ckptfreq = 0;

	switch( argc ) {

	  case 1:
		if( scanf("\n%d", &count) != 1 ) {
			fprintf( stderr, "Input syntax error\n" );
			exit( 1 );
		}
		break;

	  case 2:
		if( (count=atoi(argv[1])) <= 0 ) {
			usage();
		}
		break;
		
	  case 3:
		if( (count=atoi(argv[1])) <= 0 ) {
			usage();
		}
		if( (ckptfreq=atoi(argv[2])) <= 0 ) {
			usage();
		}
		break;

	  default:
		usage();

	}

#if 0
	sbrk( 5 * MEG );
#endif

	do_loop( count, ckptfreq );
	printf( "Normal End of Job\n" );

	exit( 0 );
}



void do_loop( int count, int ckptfreq )
{
	int		i;
	struct rusage   buf;

	for( i=0; i<count; i++ ) {
		x_waste_a_second();
		printf( "%d\t", i );

		if( getrusage(RUSAGE_SELF,&buf) < 0 ) {
			printf( "getrusage(%d) failed: %d(%s)\n", (int)RUSAGE_SELF, 
					errno, strerror(errno));
			exit(1);
		} else {
			printf( "\tUserTime = %d.%06d",
				(int)buf.ru_utime.tv_sec, (int)buf.ru_utime.tv_usec );
			printf( "\tSystemTime = %d.%06d",
				(int)buf.ru_stime.tv_sec, (int)buf.ru_stime.tv_usec );
		}
		
		fflush( stdout );
		if( write(1,"\n",1) != 1 ) {
			exit( errno );
		}
		if ( ckptfreq > 0 )
			if ( i % ckptfreq == 0 ) {
/*				printf("Checkpointing...\n"); fflush(stdout);*/
/*				ckpt_and_exit();*/
			}
	}
}


void usage()
{
	printf( "Usage: loop count [counts-between-checkpoints]\n" );
	exit( 1 );
}


#ifdef __cplusplus
}
#endif
