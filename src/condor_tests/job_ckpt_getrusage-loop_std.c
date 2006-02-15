/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 


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

#if defined(Darwin)
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
