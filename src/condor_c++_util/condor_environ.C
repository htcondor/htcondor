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

#include "condor_common.h"
#include "condor_distribution.h"
#define _CONDOR_ENVIRON_MAIN
#include "condor_environ.h"

// Initialize our logic
int
EnvInit( void )
{
    unsigned	i;
    for ( i=0;  i<sizeof(CondorEnvironList)/sizeof(CONDOR_ENVIRON_ELEM);  i++ )
    {
		// Sanity check
		if ( (unsigned) CondorEnvironList[i].sanity != i ) {
			fprintf( stderr, "Environ sanity check failed!!\n" );
			return -1;
		}
        CondorEnvironList[i].cached = NULL;
    }
	return 0;
}

// Get an environment variable's name
const char *
EnvGetName( CONDOR_ENVIRON which )
{
    CONDOR_ENVIRON_ELEM   *local = &CondorEnvironList[which];

	// Simple case first; out of cache
    if ( local->cached ) {
        return local->cached;
    }

	// Otherwise, fill the cache
	char	*tmps;
	switch ( local->flag )
	{
	case  ENV_FLAG_NONE:
        tmps = (char *) local->string;
		break;
    case ENV_FLAG_DISTRO:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->Get() );
		break;
    case ENV_FLAG_DISTRO_UC:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->GetUc() );
		break;
    }

	// Then, return it
	return ( local->cached = (const char * ) tmps );
}
