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
