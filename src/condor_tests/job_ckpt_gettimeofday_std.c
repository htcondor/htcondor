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
