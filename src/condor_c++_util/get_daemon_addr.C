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

 

///////////////////////////////////////////////////////////////////////////////
// Get the ip address and port number of a schedd from the collector.
///////////////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_config.h"
#include "condor_string.h"
#include "daemon.h"
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "daemon_types.h"

extern "C" {

// Return the host portion of a daemon name string.  Either the name
// includes an "@" sign, in which case we return whatever is after it,
// or it doesn't, in which case we just return what we got passed.
const char*
get_host_part( const char* name )
{
	char* tmp;
	if (name == NULL) return NULL;
	tmp = strchr( name, '@' );
	if( tmp ) {
		return ++tmp;
	} else {
		return name;
	}
}


// Return a pointer to a static buffer that contains the valid daemon
// name that corresponds with the given name.  Basically, see if
// there's an '@'.  If so, resolve everything after it as a hostname.
// If not, resolve what we were passed as a hostname.
char*
get_daemon_name( const char* name )
{
	char *tmp, *fullname, *tmpname;
	char daemon_name[256];
	daemon_name[0] = '\0';
	tmpname= strdup( name );
	int had_error = 0;

		// First, check for a '@' in the name.
	tmp = strchr( tmpname, '@' );
	if( tmp ) {
			// There's a '@'.
		*tmp = '\0';
		tmp++;
		if( *tmp ) {
				// There was something after the @, try to resolve it
				// as a full hostname:
			fullname = get_full_hostname( tmp );
		} else {
				// There was nothing after the @, use localhost:
			fullname = my_full_hostname();
		}
		if( fullname ) {
			sprintf( daemon_name, "%s@%s", tmpname, fullname );
		} else {
			had_error = 1;
		}
	} else {
			// There's no '@', just try to resolve the hostname.
		if( (fullname = get_full_hostname(tmpname)) ) {
			sprintf( daemon_name, "%s", fullname );
		} else {
			had_error = 1;
		}			
	}
	free( tmpname );
	if( had_error ) {
		return NULL;
	} else {
		return strnewp( daemon_name );
	}
}


// Given some name, create a valid name for ourself with our full
// hostname.  If the name contains an '@', strip off everything after
// it and append my_full_hostname().  If there's no '@', try to
// resolve what we have and see if it's my_full_hostname.  If so, use
// it, otherwise, use name@my_full_hostname().  We return the answer
// in a newly allocated string which should be deallocated w/ delete. 
char*
build_valid_daemon_name( char* name ) 
{
	char *tmp;
	char daemonName[100];

	if( name && *name ) {
		tmp = strchr( name, '@' );
		if( tmp ) {
				// name we were passed has an '@'
			*tmp = '\0';
			sprintf( daemonName, "%s@%s", name, my_full_hostname() );
		} else {
				// no '@', see if what we have is our hostname
			if( !strcmp(get_full_hostname(name), my_full_hostname()) ) {
				sprintf( daemonName, "%s", my_full_hostname() );
			} else {
				sprintf( daemonName, "%s@%s", name, my_full_hostname() );
			}
		}
	} else {
		sprintf( daemonName, "%s", my_full_hostname() );
	}
	return strnewp( daemonName );
}


// Return a newly allocated string which contains the name of the
// given daemon (specified by subsys string) as configured on the
// local host.  This string should be deallocated with delete []. 
char*
my_daemon_name( const char* subsys )
{
	char line [100], *tmp, *my_name;
	sprintf( line, "%s_NAME", subsys );
	tmp = param( line );
	if( tmp ) {
		my_name = build_valid_daemon_name(tmp);
		free( tmp );
	} else {
		my_name = strnewp( my_full_hostname() );
	}
	return my_name;
}


char*
get_schedd_addr(const char* name, const char* pool)
{
	static char addr[100];
	Daemon d( DT_SCHEDD, name, pool );
	if( d.addr() ) {
		sprintf( addr, "%s", d.addr() );
		return addr;
	} else {
		return NULL;
	}
} 


char*
get_startd_addr(const char* name, const char* pool)
{
	static char addr[100];
	Daemon d( DT_STARTD, name, pool );
	if( d.addr() ) {
		sprintf( addr, "%s", d.addr() );
		return addr;
	} else {
		return NULL;
	}
} 


char*
get_master_addr(const char* name, const char* pool)
{
	static char addr[100];
	Daemon d( DT_MASTER, name, pool );
	if( d.addr() ) {
		sprintf( addr, "%s", d.addr() );
		return addr;
	} else {
		return NULL;
	}
} 

char*
get_negotiator_addr(const char* name)
{
	static char addr[100];
	Daemon d( DT_COLLECTOR, name );
	if( d.addr() ) {
		sprintf( addr, "%s", d.addr() );
		return addr;
	} else {
		return NULL;
	}
}


char*
get_collector_addr(const char* name)
{
	static char addr[100];
	Daemon d( DT_COLLECTOR, name );
	if( d.addr() ) {
		sprintf( addr, "%s", d.addr() );
		return addr;
	} else {
		return NULL;
	}
}


char*
get_daemon_addr( daemon_t dt, const char* name, const char* pool )
{
	switch( dt ) {
	case DT_MASTER:
		return get_master_addr( name, pool );
	case DT_STARTD:
		return get_startd_addr( name, pool );
	case DT_SCHEDD:
		return get_schedd_addr( name, pool );
	case DT_NEGOTIATOR:
		return get_negotiator_addr( name );
	case DT_COLLECTOR:
		return get_collector_addr( name );
	default:
		return NULL;
	}
	return NULL;
}


int
is_valid_sinful( char *sinful )
{
	char* tmp;
	if( !sinful ) return FALSE;
	if( !(sinful[0] == '<') ) return FALSE;
	if( !(tmp = strchr(sinful, ':')) ) return FALSE;
	if( !(tmp = strrchr(sinful, '>')) ) return FALSE;
	return TRUE;
}


} /* extern "C" */
