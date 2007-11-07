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
        tmps = strdup(local->string);
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
