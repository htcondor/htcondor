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
#include "condor_distribution.h"
#define _CONDOR_ENVIRON_MAIN
#include "condor_environ.h"
#include "condor_debug.h"

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
    if ( local->cached != NULL) {
        return local->cached;
    }

	// Otherwise, fill the cache
	char	*tmps = NULL;
	switch ( local->flag )
	{
	case  ENV_FLAG_NONE:
        tmps = strdup((char *) local->string);
		break;
    case ENV_FLAG_DISTRO:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() + 1);
        sprintf( tmps, local->string, myDistro->Get() );
		break;
    case ENV_FLAG_DISTRO_UC:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() + 1);
        sprintf( tmps, local->string, myDistro->GetUc() );
		break;
	default:
		dprintf(D_ALWAYS, "EnvGetName(): SHOULD NEVER HAPPEN!\n");
		break;
    }

	return ( local->cached = (const char * ) tmps );
}
