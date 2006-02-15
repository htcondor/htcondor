/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#define _CONDOR_ATTR_MAIN
#include "condor_attributes.h"

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
	char	*tmps;
	switch ( local->flag )
	{
	case  ATTR_FLAG_NONE:
        tmps = (char *) local->string;
		break;
    case ATTR_FLAG_DISTRO:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->Get() );
		break;
    case ATTR_FLAG_DISTRO_UC:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->GetUc() );
		break;
    case ATTR_FLAG_DISTRO_CAP:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->GetCap() );
		break;
    }

	// Then, return it
	return ( local->cached = (const char * ) tmps );
}
