/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "x_waste_second.h"


int
compare( struct timeval* tp, struct timeval* old_tp ) 
{
	if( ! (tp && old_tp) ) {
		fprintf( stderr, "ERROR: missing structure for compare()\n" );
		exit( 1 );
	}

	if( (tp->tv_sec < old_tp->tv_sec) ||
		((tp->tv_sec = old_tp->tv_sec) && 
		 (tp->tv_usec < old_tp->tv_usec)) ) {
		fprintf( stderr, 
				 "New values (%d:%d) are older than old values (%d:%d)!\n",
				 (int)tp->tv_sec, (int)tp->tv_usec,
				 (int)old_tp->tv_sec, (int)old_tp->tv_usec );
		return 0;
	}
	return 1;
}	 


void
save( struct timeval* tp, struct timeval* old_tp ) 
{
	if( ! (tp && old_tp) ) {
		fprintf( stderr, "ERROR: missing structure for compare()\n" );
		exit( 1 );
	}
	old_tp->tv_sec = tp->tv_sec;
	old_tp->tv_usec = tp->tv_usec;
}


int
main( int argc, char* argv[] )
{
	struct timeval tp;
	struct timeval old_tp;
	int rval;
	int i;
	int num_tests = 200;

	memset( &old_tp, 0, sizeof(struct timeval) );

	if( argc > 1 ) {
		num_tests = atoi( argv[1] );
	}

	for( i=0; i<num_tests; i++ ) {
		rval = gettimeofday(&tp, NULL);
		if (rval < 0) {
			perror("gettimeofday");
			return 1;
		}
		if( ! compare(&tp, &old_tp) ) {
			fprintf( stderr, "Test failed on attempt %d\n", i+1 );
			exit( 1 );
		}
		save( &tp, &old_tp );
		x_waste_a_second();
	}

	printf( "SUCCESS\n");
	return 0;
}
