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
#include "my_username.h"
#include "condor_uid.h"
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


// Return a pointer to a newly allocated string that contains the
// valid daemon name that corresponds with the given name.  Basically,
// see if there's an '@'.  If so, resolve everything after it as a
// hostname.  If not, resolve what we were passed as a hostname.
// The string is allocated with strnewp() (or it's equivalent), so you
// should deallocate it with delete [].
char*
get_daemon_name( const char* name )
{
	char *tmp, *fullname, *tmpname, *daemon_name = NULL;
	int size;

		// First, check for a '@' in the name.
	tmpname = strdup( name );
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
			fullname = strnewp( my_full_hostname() );
		}
		if( fullname ) {
			size = strlen(tmpname) + strlen(fullname) + 2;
			daemon_name = new char[size];
			sprintf( daemon_name, "%s@%s", tmpname, fullname );
			delete [] fullname;
		} 
	} else {
			// There's no '@', just try to resolve the hostname.
		daemon_name = get_full_hostname( tmpname );
	}
	free( tmpname );
		// If there was an error, this will still be NULL.
	return daemon_name;
}


// Given some name, create a valid name for ourself with our full
// hostname.  If the name contains an '@', strip off everything after
// it and append my_full_hostname().  If there's no '@', try to
// resolve what we have and see if it's my_full_hostname.  If so, use
// it, otherwise, use name@my_full_hostname().  We return the answer
// in a string which should be deallocated w/ delete [].
char*
build_valid_daemon_name( char* name ) 
{
	char *tmp, *tmpname, *daemon_name = NULL;
	int size;

		// This flag determines if we want to just return a copy of
		// my_full_hostname(), or if we want to append
		// "@my_full_hostname" to the name we were given.  The name we
		// were given might include an '@', in which case, we trim off
		// everything after the '@'.
	bool just_host = false;

	if( name && *name ) {
		tmpname = strnewp( name );
		tmp = strchr( tmpname, '@' );
		if( tmp ) {
				// name we were passed has an '@', ignore everything
				// after (and including) the '@'.  
			*tmp = '\0';
		} else {
				// no '@', see if what we have is our hostname
			if( (tmp = get_full_hostname(name)) ) {
				if( !strcmp(tmp, my_full_hostname()) ) {
						// Yup, so just the full hostname.
					just_host = true;
				}					
				delete [] tmp;
			}
		}
	} else {
			// Passed NULL for the name.
		just_host = true;
	}

	if( just_host ) {
		daemon_name = strnewp( my_full_hostname() );
	} else {
		size = strlen(tmpname) + strlen(my_full_hostname()) + 2; 
		daemon_name = new char[size];
		sprintf( daemon_name, "%s@%s", tmpname, my_full_hostname() ); 
		delete [] tmpname;
	}
	return daemon_name;
}


/* 
   Return a string on the heap (must be deallocated with delete()) that 
   contains the default daemon name for the calling process.  If we're
   root (additionally, on UNIX, if we're condor), we default to the
   full hostname.  Otherwise, we default to username@full.hostname.
*/
char*
default_daemon_name( void )
{
	if( is_root() ) {
		return strnewp( my_full_hostname() );
	}
#ifndef WIN32
	if( getuid() == get_real_condor_uid() ) {
		return strnewp( my_full_hostname() );
	}
#endif /* ! LOSE32 */
	char* name = my_username();
	if( ! name ) {
		return NULL;
	}
	char* host = my_full_hostname();
	if( ! host ) {
		free( name );
		return NULL;
	}
	int size = strlen(name) + strlen(host) + 2;
	char* ans = new char[size];
	if( ! ans ) {
		free( name );
		return NULL;
	}
	sprintf( ans, "%s@%s", name, host );
	free(name);
	return ans;
}


char*
get_schedd_addr(const char* name, const char* pool)
{
	static char addr[100];
	Daemon d( DT_SCHEDD, name, pool );
	if( d.locate() ) {
		strncpy( addr, d.addr(), 100 );
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
	if( d.locate() ) {
		strncpy( addr, d.addr(), 100 );
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
	if( d.locate() ) {
		strncpy( addr, d.addr(), 100 );
		return addr;
	} else {
		return NULL;
	}
} 

char*
get_negotiator_addr(const char* name)
{
	static char addr[100];
	Daemon d( DT_NEGOTIATOR, name );
	if( d.locate() ) {
		strncpy( addr, d.addr(), 100 );
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
	if( d.locate() ) {
		strncpy( addr, d.addr(), 100 );
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


} /* extern "C" */
