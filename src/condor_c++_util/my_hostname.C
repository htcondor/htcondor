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
static char hostname[MAXHOSTNAMELEN];
static char full_hostname[MAXHOSTNAMELEN];
static unsigned int ip_addr;
static int hostnames_initialized = 0;
static void init_hostnames();

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


void
init_hostnames()
{
	char* tmp;

		// Get our local hostname, and strip off the domain if
		// gethostname returns it.
	if( gethostname(hostname, sizeof(hostname)) == 0 ) {
		tmp = strchr( hostname, '.' );
		if( tmp ) {
			*tmp = '\0';
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
	if( (host_ptr = gethostbyname(hostname)) == NULL ) {
		EXCEPT( "gethostbyname(%s) failed, errno = %d", hostname, errno );
	}

		// Grab our ip_addr and fully qualified hostname
	memcpy( &ip_addr, host_ptr->h_addr, (size_t)host_ptr->h_length );
	ip_addr = ntohl( ip_addr );

	strcpy( full_hostname, host_ptr->h_name );
	
	hostnames_initialized = TRUE;
}

