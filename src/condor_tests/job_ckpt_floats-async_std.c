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

 

/*
** This program tests to make sure the floating point register is
** being saved and restored correctly after checkpoint.  When running
** this test, make sure to do condor_vacates to force checkpoints.
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

int main( int argc, char* argv[] ) {
	int i, num_iter = 1;
	float x = 45;
	float y = 23;
	float z = 256;
	int count=0;
	int success=1;

	if( argc >= 2 ) {
		num_iter = atoi( argv[1] );
	}
	if( num_iter < 1 ) {
		num_iter = 1;
	}

	for( i=0; i<num_iter; i++ ) {
		while( count<INT_MAX ) {
			count++;
			if( (x+y)>=z ) {
				printf( "error: floating point comparison failed on "
						"iteration 0x%x\n", count );
				success=0;
			}
		}
		count = 0;
	}
	if (success) {
		printf("Program completed successfully.\n");
		printf("Be sure that it checkpointed at least once.\n");
		return 0;
	}
	return 1;
}

