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

// return-n.c
//
// - if no arguments, returns 0
// - otherwise returns first argument (if positive), or raises signal
//   (if negative)
// - if second argument exists, sleeps for that many seconds first
//
// 2000-11-28 <pfc@cs.wisc.edu>

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "x_waste_second.h"

int main( int argc, char* argv[] )
{
	int i, arg;

	if( argc > 2 ) {
		for( i = 0; i < atoi(argv[2]); i++ ) {
			x_waste_a_second();
		}
	}

	printf("Output from return-n\n");

	if( argc > 1 ) {
		arg = atoi( argv[1] );
		if( arg >= 0 )
			return arg;
		else
			raise( 0 - arg );
	}
	return 0;
}
