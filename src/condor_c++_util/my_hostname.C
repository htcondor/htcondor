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
#include "condor_config.h"
#include "internet.h"
#include "get_full_hostname.h"
#include "my_hostname.h"

static char* hostname = NULL;
static char* full_hostname = NULL;
static unsigned int ip_addr;
static struct in_addr sin_addr;
static bool has_sin_addr = false;
static int hostnames_initialized = 0;
static int ipaddr_initialized = 0;
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
	if( ! ipaddr_initialized ) {
		init_ipaddr(0);
	}
	return ip_addr;
}


struct in_addr*
my_sin_addr()
{
	if( ! ipaddr_initialized ) {
		init_ipaddr(0);
	}
	return &sin_addr;
}


char*
my_ip_string()
{
	if( ! ipaddr_initialized ) {
		init_ipaddr(0);
	}
	return inet_ntoa(sin_addr);
}

void
init_full_hostname()
{
	char *tmp;

		// If we don't have our IP addr yet, we'll get it for free if
		// we want it.  
	if( ! ipaddr_initialized ) {
		tmp = get_full_hostname( hostname, &sin_addr );
		has_sin_addr = true;
		init_ipaddr(0);
	} else {
		tmp = get_full_hostname( hostname );
	}

	if( full_hostname ) {
		free( full_hostname );
	}
	if( tmp ) {
			// Found it, use it.
		full_hostname = strdup( tmp );
		delete [] tmp;
	} else {
			// Couldn't find it, just use what we've already got. 
		full_hostname = strdup( hostname );
	}
}

void
init_ipaddr( int config_done )
{
	char *network_interface, *tmp;

    if( ! hostname ) {
		init_hostnames();
	}

	if( config_done ) {
		if( (network_interface = param("NETWORK_INTERFACE")) ) {
			if( is_ipaddr((const char*)network_interface, &sin_addr) ) {
					// We were given a valid IP address, which we now
					// have in ip_addr.  Just make sure it's in host
					// order now:
				has_sin_addr = true;
				ip_addr = ntohl( sin_addr.s_addr );
				ipaddr_initialized = TRUE;
			} else {
				dprintf( D_ALWAYS, 
						 "init_ipaddr: Invalid network interface string: \"%s\"\n", 
						 network_interface );
				dprintf( D_ALWAYS, "init_ipaddr: Using default interface.\n" );
			} 
			free( network_interface );
		}
	}

	if( ! ipaddr_initialized ) {
		if( ! has_sin_addr ) {
				// Get our official host info to initialize sin_addr
			tmp = get_full_hostname( hostname, &sin_addr );
			if( ! tmp ) {
				EXCEPT( "gethostbyname(%s) failed, errno = %d", hostname, errno );
			}
			has_sin_addr = true;
				// We don't need the full hostname, we've already got
				// that... 
			delete [] tmp;
		}
		ip_addr = ntohl( sin_addr.s_addr );
		ipaddr_initialized = TRUE;
	}
}

} /* extern "C" */

#ifdef WIN32
	// see below comment in init_hostname() to learn why we must
	// include condor_io in this module.
#include "condor_io.h"
#endif

void
init_hostnames()
{
    char *tmp, hostbuf[MAXHOSTNAMELEN];
    hostbuf[0]='\0';
#ifdef WIN32
	// There are a  tools in Condor, like
	// condor_history, which do not use any CEDAR sockets but which call
	// some socket helper functions like gethostbyname().  These helper
	// functions will fail unless WINSOCK is initialized.  WINSOCK
	// is initialized via a global constructor in CEDAR, so we must
	// make certain we call at least one CEDAR function so the linker
	// brings in the global constructor to initialize WINSOCK! 
	// In addition, some global constructors end up calling
	// init_hostnames(), and thus will fail if the global constructor
	// in CEDAR is not called first.  Instead of relying upon a
	// specified global constructor ordering (which we cannot), 
	// we explicitly invoke SockInitializer::init() right here -Todd T.
	SockInitializer startmeup;
	startmeup.init();
#endif

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
    if( ! full_hostname ) {
		init_full_hostname();
	}
	hostnames_initialized = TRUE;
}
