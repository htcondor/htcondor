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
#include "condor_common.h"
#include "condor_pidenvid.h"
#include "procapi.h"
#include <stdio.h>

/* This is a ProcApi test program for NT.  It can also be used in Unix... */

/* This quick-n-dirty tester is best used with perfmon, the 
   really cool performance tool provided with windows.  Use 
   it to determine pids in the system ("ID Process") and then
   check some of the results against what it says. */

int main()
{ 
	piPTR pi = NULL;
	pid_t pids[3];
	int status;
	PidEnvID penvid;

	pidenvid_init(&penvid);

// Use this for getProcSetInfo()...
//	printf ( "Feed me three pids\n" );
//	scanf ( "%d %d %d", &pids[0],  &pids[1],  &pids[2] );

// Use this for single pids:
	printf ( "Give me a parent pid:\n" );
	scanf ( "%d", &pids[0] );

	while (1) {
			
		ProcAPI::getProcInfo( pids[0], pi, status );
//		ProcAPI::getFamilyInfo( pids[0], pi, &penvid, status );
//		ProcAPI::getProcSetInfo( pids, 3, pi, status );

		ProcAPI::printProcInfo ( pi );
		printf ( "\n" );

// Use this for testing less-than-one-second sampling intervals:
//		for ( int i=0 ; i<1000000 ; i++ )
//			;

	// Otherwise you can use this:
	sleep (5);
	}

	return 0;
}
