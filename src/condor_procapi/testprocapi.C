#include "condor_common.h"
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

// Use this for getProcSetInfo()...
//	printf ( "Feed me three pids\n" );
//	scanf ( "%d %d %d", &pids[0],  &pids[1],  &pids[2] );

// Use this for single pids:
	printf ( "Give me a parent pid:\n" );
	scanf ( "%d", &pids[0] );

	while (1) {
			
//		ProcAPI::getProcInfo( pids[0], pi );
		ProcAPI::getFamilyInfo( pids[0], pi );
//		ProcAPI::getProcSetInfo( pids, 3, pi );

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
