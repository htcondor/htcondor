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
#include "condor_debug.h"
static char *_FileName_ = __FILE__;

static struct hostent *host_ptr = NULL;
static char* hostname = NULL;
static char* full_hostname = NULL;
static unsigned int ip_addr;
static int hostnames_initialized = 0;
static void init_hostnames();

extern "C" {

// Return our hostname in a static data buffer.
char *
my_hostname()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return hostname;
}


// Return our full hostname (with domain) in a static data buffer.
char*
my_full_hostname()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return full_hostname;
}


// Return the host-ordered, unsigned int version of our hostname.
unsigned int
my_ip_addr()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return ip_addr;
}

} /* extern "C" */

#if !defined(WIN32) && !defined(Solaris)
#include <arpa/nameser.h>
#include <resolv.h>
#endif

void
init_hostnames()
{
	char *tmp, hostbuf[MAXHOSTNAMELEN];
	int i;

	if( hostname ) {
		free( hostname );
	}
	if( full_hostname ) {
		free( full_hostname );
		full_hostname = NULL;
	}

		// Get our local hostname, and strip off the domain if
		// gethostname returns it.
	if( gethostname(hostbuf, sizeof(hostbuf)) == 0 ) {
		if( (tmp = strchr(hostbuf, '.')) ) {
				// There's a '.' in the hostname, assume we've got the
				// full hostname here, save it, and trim the domain
				// off and save that as the hostname.
			full_hostname = strdup( hostbuf );
			*tmp = '\0';
		}
		hostname = strdup( hostbuf );
	} else {
		EXCEPT( "gethostname failed, errno = %d", 
#ifndef WIN32
				errno );
#else
		WSAGetLastError() );
#endif
    }
	
		// Look up our official host information
	if( (host_ptr = gethostbyname(hostbuf)) == NULL ) {
		EXCEPT( "gethostbyname(%s) failed, errno = %d", hostbuf, errno );
	}

		// Grab our ip_addr
	memcpy( &ip_addr, host_ptr->h_addr, (size_t)host_ptr->h_length );
    ip_addr = ntohl( ip_addr );

	    // If we don't have our full_hostname yet, try to find it. 
	if( ! full_hostname ) {
			// See if it's correct in the hostent we've got.
		if( (tmp = strchr(host_ptr->h_name, '.')) ) {
				// There's a '.' in the "name", use that as full.
			full_hostname = strdup( host_ptr->h_name );
		}
	}

	if( ! full_hostname ) {
			// We still haven't found it yet, try all the aliases
			// until we find one with a '.'
		for( i=0; host_ptr->h_aliases[i], !full_hostname; i++ ) {
			if( (tmp = strchr(host_ptr->h_aliases[i], '.')) ) { 
				full_hostname = strdup( host_ptr->h_aliases[i] );
			}
		}
	}


/* On Solaris, the resolver lib is so heavily patched that folks
 * cannot deal with the below code.  And we cannot link in the resolver
 * statically on Solaris, cuz stupid Sun only releases the resolver
 * as a dynamic library.  So we skip the below on Solaris -Todd 5/98
 */
#if !defined( WIN32 ) && !defined(Solaris) 
	if( ! full_hostname ) {
			// We still haven't found it yet, try to use the
			// resolver.  *sigh*
		res_init();
		if( _res.defdname ) {
				// We know our default domain name, append that.
			strcat( hostbuf, "." );
			strcat( hostbuf, _res.defdname );
			full_hostname = strdup( hostbuf );
		}
	}
#endif /* not WIN32 and not Solaris */
	if( ! full_hostname ) {
			// Still can't find it, just give up.
		full_hostname = strdup( hostname );
	}

	hostnames_initialized = TRUE;
}

