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
#include "condor_debug.h"
#include "condor_string.h"
#include "condor_config.h"


char* get_full_hostname_from_hostent( struct hostent* host_ptr,
									  const char* host );

// Returns the full hostname of the given host in a newly allocated
// string, which should be de-allocated with delete [].  If the
// optional 2nd arg is non-NULL, set it to point to the hostent we get
// back.  WARNING: Using the 2nd arg is _not_ thread-safe.

char*
get_full_hostname( const char* host, struct in_addr* sin_addrp ) 
{
	struct hostent *host_ptr;
	char* tmp;
	bool have_full = false;

	if( (tmp = strchr(host, '.')) ) {
		have_full = true;
	}


	if( ! sin_addrp && have_full ) {
			// Caller doesn't want the IP addr, and the name already
			// has a dot.  That's as good as we're going to do, so we
			// should just exit w/o calling gethostbyname().
		dprintf( D_HOSTNAME, "Given name is fully qualified, done\n" );
		return strnewp( host );
	}

	if( have_full ) {
		ASSERT( sin_addrp );
		dprintf( D_HOSTNAME, "Trying to find IP addr for \"%s\"\n",
				 host );
	} else {
		if( sin_addrp ) {
			dprintf( D_HOSTNAME, "Trying to find full hostname and "
					 "IP addr for \"%s\"\n", host );
		} else {
			dprintf( D_HOSTNAME, "Trying to find full hostname for "
					 "\"%s\"\n", host );
		}
	}

	dprintf( D_HOSTNAME, "Calling gethostbyname(%s)\n", host );
	if( (host_ptr = gethostbyname( host )) == NULL ) {
			// If the resolver can't find it, just return NULL
		if( sin_addrp ) {
			memset( sin_addrp, 0, sizeof(struct in_addr) );
		}
		dprintf( D_HOSTNAME, "gethostbyname() failed: %s (errno: %d)\n", 
				 strerror(errno), errno );
		return NULL;
	}
	if( sin_addrp ) {
		*sin_addrp = *(struct in_addr*)(host_ptr->h_addr_list[0]);
		dprintf( D_HOSTNAME, "Found IP addr in hostent: %s\n", 
				 inet_ntoa(*sin_addrp) );
	}

	if( have_full ) {
			// we're done.
		return strnewp( host );
	}

		// now that we have a hostent, call our helper to find the
		// right fully qualified name out of it (this is shared with
		// a method in the Daemon object, too...)
	return get_full_hostname_from_hostent( host_ptr, host );
}


// Returns the full hostname out of the given hostent in a newly
// allocated string, which should be de-allocated with delete [] the
// optional 2nd arg is non-NULL, set it to point to the hostent we get
// back.  WARNING: This method is _not_ thread-safe.
char*
get_full_hostname_from_hostent( struct hostent* host_ptr,
								const char* host )
{
	const char* tmp_host;
	char* full_host;
	char* tmp;
	int h, i;

	if( ! host_ptr ) {
		dprintf( D_ALWAYS, "get_full_hostname_from_hostent() called with "
				 "no hostent!\n" );
		return NULL;
	}

	dprintf( D_HOSTNAME, "Trying to find full hostname from hostent\n" );

		// See if it's correct in the hostent we've got.
	if( host_ptr->h_name && 
		(tmp = strchr(host_ptr->h_name, '.')) ) { 
			// There's a '.' in the "name", use that as full.
		dprintf( D_HOSTNAME, "Main name in hostent \"%s\" is fully "
				 "qualified\n", host_ptr->h_name );
		return strnewp( host_ptr->h_name );
	}

	dprintf( D_HOSTNAME, "Main name in hostent \"%s\" contains no '.', "
			 "checking aliases\n", 
			 host_ptr->h_name ? host_ptr->h_name : "NULL" );

		// We still haven't found it yet, try all the aliases
		// until we find one with a '.'
	for( i=0; host_ptr->h_aliases[i]; i++ ) {
		dprintf( D_HOSTNAME, "Checking alias \"%s\"\n",
				 host_ptr->h_aliases[i] );
		if( (tmp = strchr(host_ptr->h_aliases[i], '.')) ) { 
			dprintf( D_HOSTNAME, "Alias \"%s\" is fully qualified\n" );
			return strnewp( host_ptr->h_aliases[i] );
		}
	}

	dprintf( D_HOSTNAME, "No host alias is fully qualified, looking for "
			 "DEFAULT_DOMAIN_NAME\n" );

	if( host ) {
		tmp_host = host;
	} else {
		tmp_host = host_ptr->h_name;
	}

		// Still haven't found it, try to param for the domain.
	if( (tmp = param("DEFAULT_DOMAIN_NAME")) ) {
		dprintf( D_HOSTNAME, "DEFAULT_DOMAIN_NAME is defined: \"%s\"\n", tmp );
		h = strlen( tmp_host );
		i = strlen( tmp );
		if( tmp[0] == '.' ) {
			full_host = new char[h+i+1];
			sprintf( full_host, "%s%s", tmp_host, tmp );
		} else {
			full_host = new char[h+i+2];
			sprintf( full_host, "%s.%s", tmp_host, tmp );
		}
		free( tmp );
		dprintf( D_HOSTNAME, "Full hostname for \"%s\" is \"%s\"\n", 
				 tmp_host, full_host );
		return full_host;
	}

	dprintf( D_HOSTNAME, "DEFAULT_DOMAIN_NAME not defined\n" );

		// Still can't find it, just give up.
	full_host = strnewp( tmp_host );
	dprintf( D_HOSTNAME, "Failed to find full hostname for \"%s\", "
			 "returning \"%s\"\n", tmp_host, full_host );
	return full_host;
}

  
