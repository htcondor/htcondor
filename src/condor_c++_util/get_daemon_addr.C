/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Cai, Weiru
** 	         University of Wisconsin, Computer Sciences Dept.
** Modified by Jim Basney: added startd functionality
** 
*/ 

///////////////////////////////////////////////////////////////////////////////
// Get the ip address and port number of a schedd from the collector.
///////////////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_query.h"
#include "condor_config.h"
#include "my_hostname.h"
#include "get_full_hostname.h"

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
	int size;
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
get_daemon_addr( const char* constraint_attr, 
				 const char* name, AdTypes adtype,
				 const char* attribute, const char* subsys )
{
	CondorQuery			query(adtype);
	ClassAd*			scan;
	static char			daemonAddr[100];
	char				constraint[500];
	ClassAdList			ads;
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

	sprintf(constraint, "%s == \"%s\"", constraint_attr, fullname ); 
	query.addConstraint(constraint);
	query.fetchAds(ads);
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
get_schedd_addr(const char* name)
{
	return get_daemon_addr( ATTR_NAME, name, SCHEDD_AD, 
							ATTR_SCHEDD_IP_ADDR, "SCHEDD" );
} 


char*
get_startd_addr(const char* name)
{
	return get_daemon_addr( ATTR_MACHINE, name, STARTD_AD, 
							ATTR_STARTD_IP_ADDR, "STARTD" );
} 


char*
get_master_addr(const char* name)
{
	return get_daemon_addr( ATTR_NAME, name, MASTER_AD, 
							ATTR_MASTER_IP_ADDR, "MASTER" );
} 


} /* extern "C" */
