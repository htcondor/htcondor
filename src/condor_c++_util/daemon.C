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

#include "daemon.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_query.h"
#include "get_daemon_addr.h"
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "internet.h"

Daemon::Daemon( daemon_t type, const char* name, const char* pool ) 
{
	char buf[256], *tmp;
	_type = type;
	_addr = NULL;
	_port = 0;
	_is_local = false;
	memset( (void*)&_sin_addr, '\0', sizeof(struct in_addr) );

	if( name && name[0] ) {
			// Make sure we fully resolve the hostname
		_name = get_daemon_name( name );
	} else {
		_name = NULL;
	}

	if( pool ) {
		_pool = strnewp( pool );
	} else {
		_pool = NULL;
	}

		// Get all the other info.
	init();

		// init() will always fill in _full_hostname, but not
		// _hostname.  In all cases, we just want to trim off the
		// domain the same way.
	strcpy( buf, _full_hostname );
	tmp = strchr( buf, '.' );
	if( tmp ) {
		*tmp = '\0';
	}
	_hostname = strnewp( buf );

		// Now that we're done with the init() code, if we still don't
		// have a name, fill that in.
	if( ! _name ) {
		_name = my_daemon_name( daemonString(type) );
	}
}


Daemon::~Daemon() 
{
	if( _name ) delete [] _name;
	if( _pool ) delete [] _pool;
	if( _addr ) delete [] _addr;
	if( _hostname ) delete [] _hostname;
	if( _full_hostname ) delete [] _full_hostname;
}


ReliSock*
Daemon::reliSock( int sec )
{
	ReliSock* reli;
	reli = new ReliSock();
	if( sec ) {
		reli->timeout( sec );
	}
	if( reli->connect(_addr, 0) ) {
		return reli;
	} else {
		delete reli;
		return NULL;
	}
}


SafeSock*
Daemon::safeSock( int sec )
{
	SafeSock* safe;
	safe = new SafeSock();
	if( sec ) {
		safe->timeout( sec );
	}
	if( safe->connect(_addr, 0) ) {
		return safe;
	} else {
		delete safe;
		return NULL;
	}
}


void
Daemon::init() 
{
	switch( _type ) {
	case DT_SCHEDD:
		get_daemon_info( "SCHEDD", ATTR_SCHEDD_IP_ADDR, SCHEDD_AD );
		break;
	case DT_STARTD:
		get_daemon_info( "STARTD", ATTR_STARTD_IP_ADDR, STARTD_AD );
		break;
	case DT_MASTER:
		get_daemon_info( "MASTER", ATTR_MASTER_IP_ADDR, MASTER_AD );
		break;
	case DT_COLLECTOR:
		if( ! get_cm_info("COLLECTOR", COLLECTOR_PORT) ) {
			EXCEPT( "Daemon: no host or address for COLLECTOR in config file" );
		}
		break;
	case DT_NEGOTIATOR:
		if( ! get_cm_info("NEGOTIATOR", NEGOTIATOR_PORT) ) {
			EXCEPT( "Daemon: no host or address for NEGOTIATOR in config file" );
		}
		break;
	case DT_VIEW_COLLECTOR:
		if( get_cm_info("CONDOR_VIEW", COLLECTOR_PORT) ) {
				// If we found it, we're done.
			break;
		} 
			// If there's nothing CONDOR_VIEW-specific, try just using
			// "COLLECTOR".
		if( ! get_cm_info("COLLECTOR", COLLECTOR_PORT) ) {
			EXCEPT( "Daemon: no host or address for CONDOR_VIEW or COLLECTOR in config file" );
		} 
		break;
	default:
		EXCEPT( "Unknown daemon type (%d) in Daemon::init", _type );
	}

}


void
Daemon::get_daemon_info( const char* subsys, 
						 const char* attribute, AdTypes adtype )
{
	char				buf[500];
	char				*addr_file, *tmp, *my_name;
	FILE				*addr_fp;
	struct sockaddr_in	sin;

		// See what the local daemon would be named.
	my_name = my_daemon_name( subsys );

		// Figure out if we want to find a local daemon or not, and
		// fill in the various hostname fields.
	if( _name && *_name ) {
		if( !strcmp( _name, my_name ) ) {
			_is_local = true;
		}
		_full_hostname = strnewp( get_host_part(_name) );
	} else {
		_is_local = true;
			// We might have a copy of an empty string here, so
			// prevent a potential memory leak.
		if( _name ) delete [] _name;
		_name = strnewp( my_name );
		_full_hostname = strnewp( my_full_hostname() );
		_sin_addr = *(my_sin_addr());
	}

		// We're done with this now.
	delete [] my_name;

	if( _is_local ) {
		sprintf( buf, "%s_ADDRESS_FILE", subsys );
		addr_file = param( buf );
		if( addr_file ) {
			if( (addr_fp = fopen(addr_file, "r")) ) {
					// Read out the sinful string.
				fgets( buf, 100, addr_fp );
				fclose( addr_fp );
					// chop off the newline
				tmp = strchr( buf, '\n' );
				if( tmp ) {
					*tmp = '\0';
				}
			}
			free( addr_file );
		} 
		if( is_valid_sinful(buf) ) {
			_addr = strnewp( buf );
		}
	}

	if( ! _addr ) {
		CondorQuery			query(adtype);
		ClassAd*			scan;
		ClassAdList			ads;

		sprintf(buf, "%s == \"%s\"", ATTR_NAME, _name ); 
		query.addConstraint(buf);
		query.fetchAds(ads, _pool);
		ads.Open();
		scan = ads.Next();
		if(!scan) {
				// Badness XXX
			return; 
		}
		if(scan->EvalString(attribute, NULL, buf) == FALSE) {
				// Badness XXX
			return;
		}
		_addr = strnewp( buf );
	}

		// Now that we have the sinful string, fill in some other
		// goodies.  
	_port = string_to_port( _addr );
	if( ! _sin_addr.s_addr ) {
		string_to_sin( _addr, &sin );
		_sin_addr = sin.sin_addr;
	}
}


bool
Daemon::get_cm_info( const char* subsys, int port )
{
	char buf[64];
	struct hostent* hostp;
	char* host = NULL;

		// We know this without any work.
	_port = port;

		// Figure out what name we're really going to use.
	if( _name && *_name ) {
			// If we were given a name, use that.
		host = strdup( _name );
	}
	
	if( ! host ) {
			// Try the config file for a subsys-specific IP addr 
		sprintf( buf, "%s_IP_ADDR", subsys );
		host = param( buf );
	}

	if( ! host ) {
			// Try the config file for a CM-specific IP addr 
		host = param( "CM_IP_ADDR" );
	}

	if( ! host ) {
			// Try the config file for a subsys-specific hostname 
		sprintf( buf, "%s_HOST", subsys );
		host = param( buf );
	}

	if( ! host ) {
			// Try the config file for a CM-specific hostname 
		host = param( "CM_HOST" );
	}

	if( ! host ) {
		return false;
	} 

	if( is_ipaddr(host, &_sin_addr) ) {
		sprintf( buf, "<%s:%d>", host, _port );
		free( host );
		_addr = strnewp( buf );
			// Make sure we've got the canonical name
		hostp = gethostbyaddr( (char*)&_sin_addr, 
							   sizeof(struct in_addr), AF_INET ); 
		if( ! hostp ) {
				// BADNESS - what to do?  
				// Whatever we do, this should not be a fatal error. -Todd
			_full_hostname = strnewp(" ");
		} else {
			_full_hostname = strnewp( hostp->h_name );
		}
	} else {
			// We were given a hostname, not an address.
		hostp = gethostbyname( host );
		free( host );
		if( ! hostp ) {
				// BADNESS - what to do?
			return false;
		}
		_sin_addr = *(struct in_addr*)(hostp->h_addr_list[0]);
		sprintf( buf, "<%s:%d>", inet_ntoa(_sin_addr), _port );
		_addr = strnewp( buf );
		_full_hostname = strnewp( hostp->h_name );
	}
	if( ! _name ) {
		_name = strnewp( _full_hostname );
	}
	if( !strcmp(_full_hostname, my_full_hostname()) ) {
		_is_local = true;
	}
	return true;
}


