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
#include "condor_query.h"
#include "condor_config.h"
#include "my_hostname.h"
#include "get_full_hostname.h"
#include "daemon_types.h"

extern "C" {

// Return the host portion of a daemon name string.  Either the name
// includes an "@" sign, in which case we return whatever is after it,
// or it doesn't, in which case we just return what we got passed.
const char*
get_host_part( const char* name )
{
	char* tmp;
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
	static char daemon_name[256];
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
		return daemon_name;
	}
}


// Given some name, create a valid name for ourself with our full
// hostname.  If the name contains an '@', strip off everything after
// it and append my_full_hostname().  If there's no '@', try to
// resolve what we have and see if it's my_full_hostname.  If so, use
// it, otherwise, use name@my_full_hostname().
char*
build_valid_daemon_name( char* name ) 
{
	char *tmp;
	static char daemonName[100];

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
	return daemonName;
}


// Return a pointer to a static buffer which contains the name of the
// given daemon (specified by subsys string) as configured on the
// local host.
char*
my_daemon_name( const char* subsys )
{
	static char my_name[100];
	char line [100], *tmp;
	sprintf( line, "%s_NAME", subsys );
	tmp = param( line );
	if( tmp ) {
		sprintf( my_name, "%s", build_valid_daemon_name(tmp) );
		free( tmp );
	} else {
		sprintf( my_name, "%s", my_full_hostname() );
	}
	return my_name;
}


char*
real_get_daemon_addr( const char* constraint_attr, 
				 const char* name, AdTypes adtype,
				 const char* attribute, const char* subsys, 
				 const char* pool )
{

	static char			daemonAddr[100];
	char				constraint[500];
	char				*fullname = NULL, *addr_file, *tmp, *my_name;
	FILE				*addr_fp;
	int					is_local = 0;

	daemonAddr[0] = '\0';
	my_name = my_daemon_name( subsys );

		// Figure out if we want to find a local daemon or not.
	if( name && *name ) {
		fullname = (char*)name;
		if( !strcmp( fullname, my_name ) ) {
			is_local = 1;
		}
	} else {
		fullname = my_name;
		is_local = 1;
	}

	if( is_local ) {
		sprintf( constraint, "%s_ADDRESS_FILE", subsys );
		addr_file = param( constraint );
		if( addr_file ) {
			if( (addr_fp = fopen(addr_file, "r")) ) {
					// Read out the sinful string.
				fgets( daemonAddr, 100, addr_fp );
					// chop off the newline
				tmp = strchr( daemonAddr, '\n' );
				if( tmp ) {
					*tmp = '\0';
				}
				fclose( addr_fp );
			}
			free( addr_file );
		} 
		if( daemonAddr[0] == '<' ) {
				// We found something reasonable.
			return daemonAddr;
		}
	}

	CondorQuery			query(adtype);
	ClassAd*			scan;
	ClassAdList			ads;

	sprintf(constraint, "%s == \"%s\"", constraint_attr, fullname ); 
	query.addConstraint(constraint);
	query.fetchAds(ads, pool);
	ads.Open();
	scan = ads.Next();
	if(!scan)
	{
		return NULL; 
	}
	if(scan->EvalString(attribute, NULL, daemonAddr) == FALSE)
	{
		return NULL; 
	}
	return daemonAddr;
}


char*
get_schedd_addr(const char* name, const char* pool)
{
	return real_get_daemon_addr( ATTR_NAME, name, SCHEDD_AD, 
								 ATTR_SCHEDD_IP_ADDR, "SCHEDD", pool );
} 


char*
get_startd_addr(const char* name, const char* pool)
{
	return real_get_daemon_addr( ATTR_MACHINE, get_host_part(name), STARTD_AD, 
								 ATTR_STARTD_IP_ADDR, "STARTD", pool );
} 


char*
get_master_addr(const char* name, const char* pool)
{
	return real_get_daemon_addr( ATTR_NAME, name, MASTER_AD, 
								 ATTR_MASTER_IP_ADDR, "MASTER", pool );
} 


char*
get_cm_addr( const char* name, char* config_name, int port )
{
	static char addr[30];
	struct hostent* hostp;
	char* tmp = NULL;

	if( name && *name ) {
		tmp = strdup( name );
	} else {
		tmp = param( config_name );
	}
	if( ! tmp ) {
		return NULL;
	} 
	hostp = gethostbyname( tmp );
	free( tmp );
	if( ! hostp ) {
		return NULL;
	}
	sprintf( addr, "<%s:%d>",
			 inet_ntoa( *(struct in_addr*)(hostp->h_addr_list[0]) ),
			 port );
	return addr;
}


char*
get_negotiator_addr(const char* name)
{
	return get_cm_addr( name, "NEGOTIATOR_HOST", NEGOTIATOR_PORT );
}


char*
get_collector_addr(const char* name)
{
	return get_cm_addr( name, "COLLECTOR_HOST", COLLECTOR_PORT );
}


char*
get_daemon_addr( daemonType dt, const char* name, const char* pool )
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
