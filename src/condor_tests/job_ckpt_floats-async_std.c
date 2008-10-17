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
** This program tests to make sure the floating point register is
** being saved and restored correctly after checkpoint.  When running
** this test, make sure to do condor_vacates to force checkpoints.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>
#include <time.h>

int main( int argc, char* argv[] ) {
	int i, j, s, num_k_iter, num_secs = 1;
	time_t old_time, new_time;
	float x = 45;
	float y = 23;
	float z = 256;
	int success=1;
	struct utsname uname_s;

	if( argc >= 2 ) {
		num_secs = atoi( argv[1] );
	}
	if( num_secs < 1 ) {
		num_secs = 1;
	}

	old_time = time( NULL );
	s = 0;
	while ( s < num_secs ) {
		for( j=0; j<10000; j++ ) {
			if( (x+y)>=z ) {
				printf( "error: floating point comparison failed on "
						"sec: %d, i: %d, j: %d\n", s, i, j );
				success=0;
			}
		}
		new_time = time( NULL );
		if ( new_time > old_time ) {
			old_time = new_time;
			s++;
		}
	}
	if (success) {
		printf("Program completed successfully.\n");
		printf("Be sure that it checkpointed at least once.\n");
		return 0;
	}
	return 1;
}

