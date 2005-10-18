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
#include <strings.h>
#include <limits.h>
#include <sys/utsname.h>
#include "x_my_bogokips.h"

int main( int argc, char* argv[] ) {
	int i, j, s, num_k_iter, num_secs = 1;
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

		/*
		  unfortunately, our "BOGOKIPS" is timed for no-op iterations,
		  and we're doing this extra floating point addition and
		  comparison, so our timing is therefore off.  a
		  "fudge-factor" of 2/5 seems to give roughly accurate timings
		  on linux, while on solaris, the extra floating point ops
		  don't seem to matter and we can use the BOGOKIPS directly.
		*/
	uname( &uname_s );
	if( ! strcmp(uname_s.sysname, "SunOS") ) { 
		num_k_iter = BOGOKIPS;
	} else {
		num_k_iter = (int) ((BOGOKIPS/5)*2);
	}

	for( s=0; s<num_secs; s++ ) {
		for( i=0; i<num_k_iter; i++ ) {
			for( j=0; j<1000; j++ ) {
				if( (x+y)>=z ) {
					printf( "error: floating point comparison failed on "
							"sec: %d, i: %d, j: %d\n", s, i, j );
					success=0;
				}
			}
		}
	}
	if (success) {
		printf("Program completed successfully.\n");
		printf("Be sure that it checkpointed at least once.\n");
		return 0;
	}
	return 1;
}

