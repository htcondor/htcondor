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
//#include "condor_distribution.h"
#define _CONDOR_ATTR_MAIN
#include "condor_attributes.h"

#if 0
// Initialize our logic
int
AttrInit( void )
{
    unsigned	i;
    for ( i=0;  i<sizeof(CondorAttrList)/sizeof(CONDOR_ATTR_ELEM);  i++ )
    {
		// Sanity check
		if ( (unsigned) CondorAttrList[i].sanity != i ) {
			fprintf( stderr, "Attribute sanity check failed!!\n" );
			return -1;
		}
        CondorAttrList[i].cached = NULL;
    }
	return 0;
}

// Get an attribute variable's name
const char *
AttrGetName( CONDOR_ATTR which )
{
    CONDOR_ATTR_ELEM	*local = &CondorAttrList[which];

	// Simple case first; out of cache
    if ( local->cached ) {
        return local->cached;
    }

	// Otherwise, fill the cache
	char	*tmps=0;
	switch ( local->flag )
	{
	case  ATTR_FLAG_NONE:
		tmps = const_cast<char *>( local->string );
		break;
    case ATTR_FLAG_DISTRO:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
		tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
		if (tmps)
			sprintf( tmps, local->string, myDistro->Get() );
		break;
    case ATTR_FLAG_DISTRO_UC:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
		tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
		if (tmps)
			sprintf( tmps, local->string, myDistro->GetUc() );
		break;
    case ATTR_FLAG_DISTRO_CAP:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
		tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
		if (tmps)
			sprintf( tmps, local->string, myDistro->GetCap() );
		break;
    }

	// Then, return it
	local->cached = tmps;
	return local->cached;
}

#define ATTR_CONDOR_LOAD_AVG			AttrGetName( ATTRE_CONDOR_LOAD_AVG )
#define ATTR_CONDOR_ADMIN				AttrGetName( ATTRE_CONDOR_ADMIN )
#define ATTR_PLATFORM					AttrGetName( ATTRE_PLATFORM )
#define ATTR_TOTAL_CONDOR_LOAD_AVG			AttrGetName( ATTRE_TOTAL_LOAD )
#define ATTR_VERSION					AttrGetName( ATTRE_VERSION )

#endif
