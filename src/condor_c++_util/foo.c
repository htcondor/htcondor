/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#define _POSIX_SOURCE

#include "condor_common.h"
#include "user_log.h"

main( int argc, char *argv[] )
{
	int		i;
	LP		*lp;

	int		cluster, proc, subproc;

	if( argc != 4 ) {
		fprintf( stderr, "Usage: %s cluster proc subproc\n", argv[0] );
		exit( 1 );
	}

	cluster = atoi( argv[1] );
	proc = atoi( argv[2] );
	subproc = atoi( argv[3] );

	lp =  InitUserLog( "condor", "/tmp/condor", cluster, proc, subproc );
	PutUserLog( lp, "At line %d in %s\n", __LINE__, __FILE__ );
	PutUserLog(lp, "At line %d in %s\n", __LINE__, __FILE__ );

	BeginUserLogBlock( lp );
	for( i=0; i<10; i++ ) {
		PutUserLog( lp, "This is line %d of output\n", i );
	}
	EndUserLogBlock( lp );

	exit( 0 );
}

extern "C" int SetSyscalls( int mode ) { return mode; }

