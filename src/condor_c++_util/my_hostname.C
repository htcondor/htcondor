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
#include "get_full_hostname.h"

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


void
init_hostnames()
{
	char *tmp, hostbuf[MAXHOSTNAMELEN];
	int i;
	hostbuf[0]='\0';

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
		if( hostbuf[0] ) {
			if( (tmp = strchr(hostbuf, '.')) ) {
					// There's a '.' in the hostname, assume we've got
					// the full hostname here, save it, and trim the
					// domain off and save that as the hostname.
				full_hostname = strdup( hostbuf );
				*tmp = '\0';
			} 
			hostname = strdup( hostbuf );
		} else {
			EXCEPT( "gethostname succeeded, but hostbuf is empty" );
		}
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
		tmp = get_full_hostname( hostbuf );
		if( tmp ) {
				// Found it, use it.
			full_hostname = strdup( tmp );
		} else {
				// Couldn't find it, just use what we've got in hostbuf. 
			full_hostname = strdup( hostbuf );
		}
	}
	hostnames_initialized = TRUE;
}

