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
