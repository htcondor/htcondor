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
#include "condor_string.h"
#include "condor_config.h"


// Returns the full hostname of the given host in a newly allocated
// string, which should be de-allocated with delete [].  If the
// optional 2nd arg is non-NULL, set it to point to the hostent we get
// back.  WARNING: Using the 2nd arg is _not_ thread-safe.

char*
get_full_hostname( const char* host, struct in_addr* sin_addrp ) 
{
	char* full_host;
	char* tmp;
	struct hostent *host_ptr;
	int h, i;

	if( (host_ptr = gethostbyname( host )) == NULL ) {
			// If the resolver can't find it, just return NULL
		if( sin_addrp ) {
			memset( sin_addrp, 0, sizeof(struct in_addr) );
		}
		return NULL;
	}
	if( sin_addrp ) {
		*sin_addrp = *(struct in_addr*)(host_ptr->h_addr_list[0]);
	}

		// See if it's correct in the hostent we've got.
	if( host_ptr->h_name && 
		(tmp = strchr(host_ptr->h_name, '.')) ) { 
			// There's a '.' in the "name", use that as full.
		full_host = strnewp( host_ptr->h_name );
		return full_host;
	}

		// We still haven't found it yet, try all the aliases
		// until we find one with a '.'
	for( i=0; host_ptr->h_aliases[i]; i++ ) {
		if( (tmp = strchr(host_ptr->h_aliases[i], '.')) ) { 
			full_host = strnewp( host_ptr->h_aliases[i] );
			return full_host;
		}
	}

		// Still haven't found it, try to param for the domain.
	if( (tmp = param("DEFAULT_DOMAIN_NAME")) ) {
		h = strlen( host );
		i = strlen( tmp );
		if( tmp[0] == '.' ) {
			full_host = new char[h+i+1];
			sprintf( full_host, "%s%s", host, tmp );
		} else {
				// There's no . at the front, so we have to add that.
			full_host = new char[h+i+2];
			sprintf( full_host, "%s.%s", host, tmp );
		}
		free( tmp );
		return full_host;
	}

		// Still can't find it, just give up.
	full_host = strnewp( host );
	return full_host;
}
