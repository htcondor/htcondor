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
#include "condor_ver_info.h"

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
	_type = type;
	_port = 0;
	_is_local = false;
	_tried_locate = false;
	_auth_cap_known = false;
	_is_auth_cap = false;

	_addr = NULL;
	_name = NULL;
	_version = NULL;
	_error = NULL;
	_id_str = NULL;
	_hostname = NULL;
	_full_hostname = NULL;

	if( pool ) {
		_pool = strnewp( pool );
	} else {
		_pool = NULL;
	}

	if( name && name[0] ) {
		if( is_valid_sinful(name) ) {
			_addr = strnewp( name );
		} else {
			_name = strnewp( name );
		}
	} 
}


Daemon::~Daemon() 
{
	if( _name ) delete [] _name;
	if( _pool ) delete [] _pool;
	if( _addr ) delete [] _addr;
	if( _error ) delete [] _error;
	if( _id_str ) delete [] _id_str;
	if( _hostname ) delete [] _hostname;
	if( _full_hostname ) delete [] _full_hostname;
}


//////////////////////////////////////////////////////////////////////
// Data-providing methods
//////////////////////////////////////////////////////////////////////

char*
Daemon::name( void )
{
	if( ! _name ) {
		locate();
	}
	return _name;
}


char*
Daemon::hostname( void )
{
	if( ! _hostname ) {
		locate();
	}
	return _hostname;
}


char*
Daemon::version( void )
{
	if( ! _version ) {
		locate();
	}
	return _version;
}


char*
Daemon::fullHostname( void )
{
	if( ! _full_hostname ) {
		locate();
	}
	return _full_hostname;
}


char*
Daemon::addr( void )
{
	if( ! _addr ) {
		locate();
	}
	return _addr;
}


char*
Daemon::pool( void )
{
	if( ! _pool ) {
		locate();
	}
	return _pool;
}


int
Daemon::port( void )
{
	if( _port < 0 ) {
		locate();
	}
	return _port;
}


const char*
Daemon::idStr( void )
{
	if( _id_str ) {
		return _id_str;
	}
	if( ! locate() ) {
		return "unknown daemon";
	}
	char buf[128];
	if( _is_local ) {
		sprintf( buf, "local %s", daemonString(_type) );
	} else if( _name ) {
		sprintf( buf, "%s %s", daemonString(_type), _name );
	} else if( _addr ) {
		sprintf( buf, "%s at %s", daemonString(_type), _addr );
		if( _full_hostname ) {
			strcat( buf, " (" );
			strcat( buf, _full_hostname );
			strcat( buf, ")" );
		}
	} else {
		EXCEPT( "Daemon::idStr: locate() successful but _addr not found" );
	}
	_id_str = strnewp( buf );
	return _id_str;
}


void
Daemon::display( int debugflag ) 
{
	dprintf( debugflag, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)_type, daemonString(_type), 
			 _name ? _name : "(null)", 
			 _addr ? _addr : "(null)" );
	dprintf( debugflag, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 _full_hostname ? _full_hostname : "(null)",
			 _hostname ? _hostname : "(null)", 
			 _pool ? _pool : "(null)", _port );
	dprintf( debugflag, "IsLocal: %s, IdStr: %s, Error: %s\n", 
			 _is_local ? "Y" : "N",
			 _id_str ? _id_str : "(null)", 
			 _error ? _error : "(null)" );
}


void
Daemon::display( FILE* fp ) 
{
	fprintf( fp, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)_type, daemonString(_type), 
			 _name ? _name : "(null)", 
			 _addr ? _addr : "(null)" );
	fprintf( fp, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 _full_hostname ? _full_hostname : "(null)",
			 _hostname ? _hostname : "(null)", 
			 _pool ? _pool : "(null)", _port );
	fprintf( fp, "IsLocal: %s, IdStr: %s, Error: %s\n", 
			 _is_local ? "Y" : "N",
			 _id_str ? _id_str : "(null)", 
			 _error ? _error : "(null)" );
}


//////////////////////////////////////////////////////////////////////
// Communication methods
//////////////////////////////////////////////////////////////////////

ReliSock*
Daemon::reliSock( int sec )
{
	if( ! _addr ) {
		if( ! locate() ) {
			return NULL;
		}
	}
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
	if( ! _addr ) {
		if( ! locate() ) {
			return NULL;
		}
	}
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


Sock*
Daemon::startCommand( int cmd, Stream::stream_type st, int sec )
{
	Sock* sock;
	switch( st ) {
	case Stream::reli_sock:
		sock = reliSock();
		break;
	case Stream::safe_sock:
		sock = safeSock();
		break;
	default:
		EXCEPT( "Unknown stream_type (%d) in Daemon::startCommand",
				(int)st );
	}
	if( ! sock ) {
			// _error will already be set.
		return NULL;
	}

	return startCommand ( cmd, sock, sec );

}


Sock*
Daemon::startCommand( int cmd, Sock* sock, int sec )
{
	// the classad for sending
	ClassAd auth_info;

	// temp vars for putting the classad together
	char *buf;
	char *paramer;


	// set up the socket

	if( ! sock ) {
		newError( "startCommand() called with a NULL Sock*" );
		return NULL;
	}
	if( sec ) {
		sock->timeout( sec );
	}


	// read the config

	// default value for config file
	bool always_authenticate = false; 

	paramer = param("ALWAYS_AUTHENTICATE");
	if (paramer) {
		dprintf (D_SECURITY, "STARTCOMMAND: param(ALWAYS_AUTHENTICATE)"
					" == %s\n", paramer);
		if ((stricmp(paramer, "YES") == 0) ||
		    (stricmp(paramer, "TRUE") == 0)) {
			dprintf ( D_SECURITY, "STARTCOMMAND: "
					      "forcing authentication.\n");
			always_authenticate = true;
		}
		free(paramer);
	} else {
		dprintf (D_SECURITY, "STARTCOMMAND: param(ALWAYS_AUTHENTICATE)"
					"failed, assuming FALSE.\n" );
	}

	// find out if client supports authentication negotiation



	// look at the version if we haven't and it is available
	if (!_auth_cap_known && _version) {
		dprintf(D_SECURITY, "STARTCOMMAND: talking to a %s daemon.\n", _version);

		CondorVersionInfo vi(_version);
		int cmpres = vi.compare_versions("$CondorVersion: 6.3.0 Jul 17 2001 $");

		_auth_cap_known = true;
		_is_auth_cap = (cmpres <= 0);
	}


	// possible courses of action:
	const int AUTH_FAIL = 0;
	const int AUTH_OLD  = 1;
	const int AUTH_NO   = 2;
	const int AUTH_ASK  = 3;
	const int AUTH_YES  = 4;

	int authentication_action;


	// now the 'logic' :)
	if (always_authenticate) {
		authentication_action = AUTH_YES;
		if (_auth_cap_known && !_is_auth_cap) {
			authentication_action = AUTH_FAIL;
		}
	} else {
		authentication_action = AUTH_ASK;
		if (_auth_cap_known && !_is_auth_cap) {
			authentication_action = AUTH_OLD;
		}
	}



	// now take action

	sock->encode();

	// AUTH_FAIL - we demand auth, the daemon does not support it
	if (authentication_action == AUTH_FAIL) {
		// cannot possibly authenticate to pre-6.3.0,
		// so exit with error.
		dprintf(D_SECURITY, "STARTCOMMAND: cannot authenticate"
					" with pre 6.3.0\n");

		delete sock;
		return NULL;
	}

	// AUTH_OLD - we support auth, daemon does not.  command is sent
	// like it was in pre 6.3
	if (authentication_action == AUTH_OLD) {
		dprintf(D_SECURITY, "STARTCOMMAND: skipping negotiation\n");

		// just code the command and be done
		sock->code(cmd);
		free(paramer);
		return sock;
	}


	dprintf ( D_SECURITY, "negotiating auth for command %i.\n", cmd);

        // package the ClassAd together

	// allocate a buffer big enough to work with all fields
	int buflen = 128;

	paramer = param("AUTHENTICATION_METHODS");
	if (paramer != NULL) {
		dprintf ( D_SECURITY, "STARTCOMMAND: param(AUTHENTICATION_METHODS) == %s\n", paramer );
		buflen += strlen(paramer);
	} else {
		dprintf ( D_SECURITY, "STARTCOMMAND: param(AUTHENTICATION_METHODS) failed!\n" );
	}

	buf = new char[buflen];
	if (buf == NULL) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: new failed!\n" );
		delete sock;
		return NULL;
	}

	// auth_types
	if (paramer) {
		sprintf(buf, "%s=\"%s\"", ATTR_AUTH_TYPES, paramer);
		free(paramer);
	} else {
		// ZKM
		sprintf(buf, "%s=\"\"", ATTR_AUTH_TYPES);
	}

	auth_info.Insert(buf);
	dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);


	// command
	sprintf(buf, "%s=%i", ATTR_COMMAND, cmd);
	auth_info.Insert(buf);
	dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);

	// authentication action
	if (authentication_action == AUTH_YES) {
		sprintf(buf, "%s=\"YES\"", ATTR_AUTHENTICATE);
	} else {
		sprintf(buf, "%s=\"ASK\"", ATTR_AUTHENTICATE);
	}

	auth_info.Insert(buf);
	dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);

	// free the buffer
	delete [] buf;


	dprintf ( D_SECURITY, "STARTCOMMAND: sending DC_AUTHENTICATE command\n");
	int authcmd = DC_AUTHENTICATE;
	if (! sock->code(authcmd)) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: failed to send DC_AUTHENTICATE\n");
		delete sock;
		return NULL;
	}


	// send the classad
	dprintf ( D_SECURITY, "STARTCOMMAND: sending following classad:\n");
	auth_info.dPrint ( D_SECURITY );

	if (! auth_info.code(*sock)) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: failed to send auth_info\n");
		delete sock;
		return NULL;
	}

	if (! sock->end_of_message()) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: failed to end classad message\n");
		delete sock;
		return NULL;
	}

	bool retval;

	if (authentication_action == AUTH_ASK) {
		ClassAd auth_response;
		sock->decode();
		auth_response.code(*sock);
		sock->end_of_message();

		dprintf ( D_SECURITY, "STARTCOMMAND: server responded with:\n");
		auth_response.dPrint( D_SECURITY );

		buf = new char[128];
		if (buf == NULL) {
			dprintf ( D_ALWAYS, "STARTCOMMAND: new failed!\n");
			delete sock;
			return NULL;
		}

		if (auth_response.LookupString("CONDOR_VERSION", buf)) {
			dprintf ( D_ALWAYS, "STARTCOMMAND: no CONDOR_VERSION "
						"in response ClassAd" );
			New_version(buf);
		}


		if (!auth_response.LookupString(ATTR_AUTHENTICATE, buf)) {
			dprintf ( D_ALWAYS, "STARTCOMMAND: no AUTHENTICATE "
						"in response ClassAd" );
			delete sock;
			return NULL;
		}
		if(strcasecmp(buf, "YES") == 0) {
			authentication_action = AUTH_YES;
		} else {
			authentication_action = AUTH_NO;
		}

	}

	if (authentication_action == AUTH_YES) {
		dprintf ( D_SECURITY, "STARTCOMMAND: authenticate(0xFFFF) "
					"RIGHT NOW.\n");
		retval = sock->authenticate(0xFFFF);
	} else {
		dprintf ( D_SECURITY, "STARTCOMMAND: not authenticating.\n");
		retval = true;
	}

	dprintf (D_SECURITY, "STARTCOMMAND: retval is %i.\n", retval);
	if (retval) {
		dprintf ( D_SECURITY, "STARTCOMMAND: setting sock->encode()\n");
		sock->encode();
		return sock;
	} else {
		dprintf (D_SECURITY, "STARTCOMMAND: authenticate failed.\n");
		delete sock;
		return NULL;
	}
}



bool
Daemon::sendCommand( int cmd, Sock* sock, int sec )
{
	Sock* tmp = startCommand( cmd, sock, sec );
	if( ! tmp ) {
		return false;
	}
#if 0  // don't need this block, the cmd int was sent in startCommand
	if( ! tmp->eom() ) {
		char err_buf[256];
		sprintf( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( err_buf );
		return false;
	}
#endif
	return true;
}


bool
Daemon::sendCommand( int cmd, Stream::stream_type st, int sec )
{
	Sock* tmp = startCommand( cmd, st, sec );
	if( ! tmp ) {
		return false;
	}
#if 0
	if( ! tmp->eom() ) {
		char err_buf[256];
		sprintf( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( err_buf );
		delete tmp;
		return false;
	}
#endif
	delete tmp;
	return true;
}




//////////////////////////////////////////////////////////////////////
// Locate-related methods
//////////////////////////////////////////////////////////////////////

bool
Daemon::locate( void )
{
	char buf[256], *tmp;
	bool rval;

		// Make sure we only call locate() once.
	if( _tried_locate ) {
			// If we've already been here, return whether we found
			// addr or not, the best judge for if locate() worked.
		if( _addr ) {
			return true;
		} else {
			return false;
		}
	}
	_tried_locate = true;

		// First call a subsystem-specific helper to get everything we
		// have to.  What we do is mostly different between regular
		// daemons and CM daemons.  These must set: _addr, _port, and
		// _is_local.  If possible, they will also set _full_hostname
		// and _name. 
	switch( _type ) {
	case DT_SCHEDD:
		rval = getDaemonInfo( "SCHEDD", SCHEDD_AD );
		break;
	case DT_STARTD:
		rval = getDaemonInfo( "STARTD", STARTD_AD );
		break;
	case DT_MASTER:
		rval = getDaemonInfo( "MASTER", MASTER_AD );
		break;
	case DT_COLLECTOR:
		rval = getCmInfo( "COLLECTOR", COLLECTOR_PORT );
		break;
	case DT_NEGOTIATOR:
		rval = getCmInfo( "NEGOTIATOR", NEGOTIATOR_PORT );
		break;
	case DT_VIEW_COLLECTOR:
		if( (rval = getCmInfo("CONDOR_VIEW", COLLECTOR_PORT)) ) {
				// If we found it, we're done.
			break;
		} 
			// If there's nothing CONDOR_VIEW-specific, try just using
			// "COLLECTOR".
		rval = getCmInfo( "COLLECTOR", COLLECTOR_PORT ); 
		break;
	default:
		EXCEPT( "Unknown daemon type (%d) in Daemon::init", (int)_type );
	}

	if( ! rval) {
			// _error will already be set appropriately.
		return false;
	}

		// Now, deal with everything that's common between both.

		// get*Info() will usually fill in _full_hostname, but not
		// _hostname.  In all cases, if we have _full_hostname we just
		// want to trim off the domain the same way for _hostname. 
	if( _full_hostname ) {
		strcpy( buf, _full_hostname );
		tmp = strchr( buf, '.' );
		if( tmp ) {
			*tmp = '\0';
		}
		New_hostname( strnewp(buf) );
	}

		// Now that we're done with the get*Info() code, if we're a
		// local daemon and we still don't have a name, fill it in.  
	if( ! _name && _is_local) {
		_name = localName();
	}

	return true;
}


bool
Daemon::getDaemonInfo( const char* subsys, AdTypes adtype )
{
	char				buf[512], tmpname[512];
	char				*addr_file, *tmp, *my_name;
	FILE				*addr_fp;
	struct				sockaddr_in sockaddr;
	struct				hostent* hostp;

		// Figure out if we want to find a local daemon or not, and
		// fill in the various hostname fields.
	if( _name ) {
			// We were passed a name, so try to look it up in DNS to
			// get the full hostname.
		
			// First, make sure we're only trying to resolve the
			// hostname part of the name...
		strncpy( tmpname, _name, 512 );
		tmp = strchr( tmpname, '@' );
		if( tmp ) {
				// There's a '@'.
			*tmp = '\0';
				// Now, tmpname holds whatever was before the @ 
			tmp++;
			if( *tmp ) {
					// There was something after the @, try to resolve it
					// as a full hostname:
				if( ! (New_full_hostname(get_full_hostname(tmp))) ) { 
						// Given a hostname, this is a fatal error.
					sprintf( buf, "unknown host %s", tmp );  
					newError( buf );
					return false;
				} 
			} else {
					// There was nothing after the @, use localhost:
				New_full_hostname( strnewp(my_full_hostname()) );
			}
			sprintf( buf, "%s@%s", tmpname, _full_hostname );
			New_name( strnewp(buf) );
		} else {
				// There's no '@', just try to resolve the hostname.
			if( (New_full_hostname(get_full_hostname(tmpname))) ) {
				New_name( strnewp(_full_hostname) );
			} else {
					// Given a hostname, this is a fatal error.
				sprintf( buf, "unknown host %s", tmpname );  
				newError( buf );
				return false;
			}           
		}
			// Now that we got this far and have the correct name, see
			// if that matches the name for the local daemon.  
			// If we were given a pool, never assume we're local --
			// always try to query that pool...
		my_name = localName();
		if( !_pool && !strcmp(_name, my_name) ) {
			_is_local = true;
		}
		delete [] my_name;
	} else if( _addr ) {
			// We got no name, but we have an address.  Try to
			// do an inverse lookup and fill in some hostname info
			// from the IP address we already have.
		string_to_sin( _addr, &sockaddr );
		hostp = gethostbyaddr( (char*)&sockaddr.sin_addr, 
							   sizeof(struct in_addr), AF_INET ); 
		if( ! hostp ) {
			New_full_hostname( NULL );

			sprintf( buf, "cant find host info for %s", _addr );
			newError( buf );
		} else {
			New_full_hostname( strnewp(hostp->h_name) );
		}
	} else {
			// We were passed neither a name nor an address, so use
			// the local daemon.
		_is_local = true;
		New_name( localName() );
		New_full_hostname( strnewp(my_full_hostname()) );
	}

		// Now that we have the real, full names, actually find the
		// address of the daemon in question.

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
			New_addr( strnewp(buf) );
		}

		sprintf( buf, "%s", subsys );
		char* exe_file = param( buf );
		if( exe_file ) {
			char ver[128];
			CondorVersionInfo vi;
			vi.get_version_from_file(exe_file, ver, 128);
			New_version( strnewp(ver) );
		}
		free( exe_file );


	}

	if( ! _addr ) {
			// If we still don't have it (or it wasn't local), query
			// the collector for the address.
		CondorQuery			query(adtype);
		ClassAd*			scan;
		ClassAdList			ads;

		if( _type == DT_STARTD ) {
				/*
				  So long as an SMP startd has only 1 command socket
				  per startd, we want to take advantage of that and
				  query based on Machine, not Name.  This way, if
				  people supply just the hostname of an SMP, we can
				  still find the daemon.  For example, "condor_vacate
				  host" will vacate all VMs on that host, but only if
				  condor_vacate can find the address in the first
				  place.  -Derek Wright 8/19/99 
				*/
			sprintf(buf, "%s == \"%s\"", ATTR_MACHINE, _full_hostname ); 
		} else {
			sprintf(buf, "%s == \"%s\"", ATTR_NAME, _name ); 
		}
		query.addANDConstraint(buf);
		query.fetchAds(ads, _pool);
		ads.Open();
		scan = ads.Next();
		if(!scan) {
			dprintf(D_ALWAYS, "Can't find address for %s %s", 
					 daemonString(_type), _name );
			sprintf( buf, "Can't find address for %s %s", 
					 daemonString(_type), _name );
			newError( buf );
			return false; 
		}

		// construct the IP_ADDR attribute
		sprintf( tmpname, "%sIpAddr", subsys );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( buf );
			return false;
		}
		New_addr( strnewp(buf) );

		sprintf( tmpname, ATTR_VERSION );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( buf );
			return false;
		}
		dprintf (D_ALWAYS, "VERSION: %s\n", buf);
		New_version( strnewp(buf) );

	}

		// Now that we have the sinful string, fill in the port. 
	_port = string_to_port( _addr );
	return true;
}


bool
Daemon::getCmInfo( const char* subsys, int port )
{
	char buf[128];
	char* host = NULL;
	char* local_host = NULL;
	char* remote_host = NULL;
	char* tmp;
	struct in_addr sin_addr;
	struct hostent* hostp;

		// We know this without any work.
	_port = port;

		// For CM daemons, normally, we're going to be local (we're
		// just not sure which config parameter is going to find it
		// for us).  So, by default, we want _is_local set to true,
		// and only if either _name or _pool are set do we change
		// _is_local to false.  
	_is_local = true;

		// For CM daemons, the "pool" and "name" should be the same
		// thing.  See if either is set, and if so, use it for both.  
	if( _name && ! _pool ) {
		New_pool( strnewp(_name) );
	} else if ( ! _name && _pool ) {
		New_name( strnewp(_pool) );
	} else if ( _name && _pool ) {
		if( strcmp(_name, _pool) ) {
				// They're different, this is bad.
			EXCEPT( "Daemon: pool (%s) and name (%s) conflict for %s",
					_pool, _name, subsys );
		}
	}

		// Figure out what name we're really going to use.
	if( _name && *_name ) {
			// If we were given a name, use that.
		remote_host = strdup( _name );
		host = remote_host;
		_is_local = false;
	}

		// Try the config file for a subsys-specific IP addr 
	sprintf( buf, "%s_IP_ADDR", subsys );
	local_host = param( buf );

	if( ! local_host ) {
			// Try the config file for a subsys-specific hostname 
		sprintf( buf, "%s_HOST", subsys );
		local_host = param( buf );
	}

	if( ! local_host ) {
			// Try the generic CM_IP_ADDR setting (subsys-specific
			// settings should take precedence over this). 
		local_host = param( "CM_IP_ADDR" );
	}

	if( local_host && ! host ) {
		host = local_host;
	}
	if( local_host && remote_host && !strcmp(local_host, remote_host) ) { 
			// We've got the same thing, we're really local, even
			// though we were given a "remote" host.
		_is_local = true;
		host = local_host;
	}
	if( ! host ) {
		sprintf( buf, "%s address or hostname not specified in config file",
				 subsys ); 
		newError( buf );
		if( local_host ) free( local_host );
		if( remote_host ) free( remote_host );
		return false;
	} 

	if( is_ipaddr(host, &sin_addr) ) {
		sprintf( buf, "<%s:%d>", host, _port );
		if( local_host ) free( local_host );
		if( remote_host ) free( remote_host );
		New_addr( strnewp(buf) );

			// See if we can get the canonical name
		hostp = gethostbyaddr( (char*)&sin_addr, 
							   sizeof(struct in_addr), AF_INET ); 
		if( ! hostp ) {
			New_full_hostname( NULL );
			sprintf( buf, "can't find host info for %s", _addr );
			newError( buf );
		} else {
			New_full_hostname( strnewp(hostp->h_name) );
		}
	} else {
			// We were given a hostname, not an address.
		tmp = get_full_hostname( host, &sin_addr );
		if( ! tmp ) {
				// With a hostname, this is a fatal Daemon error.
			sprintf( buf, "unknown host %s", host );
			newError( buf );
			if( local_host ) free( local_host );
			if( remote_host ) free( remote_host );
			return false;
		}
		if( local_host ) free( local_host );
		if( remote_host ) free( remote_host );
		sprintf( buf, "<%s:%d>", inet_ntoa(sin_addr), _port );
		New_addr( strnewp(buf) );
		New_full_hostname( tmp );
	}

		// For CM daemons, we always want the name to be whatever the
		// full_hostname is.
	New_name( strnewp(_full_hostname) );

		// If the pool was set, we want to use the full-hostname for
		// that, too.
	if( _pool ) {
		New_pool( strnewp(_full_hostname) );
	}

	return true;
}


//////////////////////////////////////////////////////////////////////
// Other helper methods
//////////////////////////////////////////////////////////////////////

void
Daemon::newError( char* str )
{
	if( _error ) {
		delete [] _error;
	}
	_error = strnewp( str );
}


char*
Daemon::localName( void )
{
	char buf[100], *tmp, *my_name;
	sprintf( buf, "%s_NAME", daemonString(_type) );
	tmp = param( buf );
	if( tmp ) {
		my_name = build_valid_daemon_name( tmp );
		free( tmp );
	} else {
		my_name = strnewp( my_full_hostname() );
	}
	return my_name;
}


char*
Daemon::New_full_hostname( char* str )
{
	if( _full_hostname ) {
		delete [] _full_hostname;
	} 
	_full_hostname = str;
	return str;
}


char*
Daemon::New_hostname( char* str )
{
	if( _hostname ) {
		delete [] _hostname;
	} 
	_hostname = str;
	return str;
}


char*
Daemon::New_addr( char* str )
{
	if( _addr ) {
		delete [] _addr;
	} 
	_addr = str;
	return str;
}

char*
Daemon::New_version ( char* ver )
{
	if( _version ) {
		delete [] _version;
	} 
	_version = ver;
	return ver;
}


char*
Daemon::New_name( char* str )
{
	if( _name ) {
		delete [] _name;
	} 
	_name = str;
	return str;
}


char*
Daemon::New_pool( char* str )
{
	if( _pool ) {
		delete [] _pool;
	} 
	_pool = str;
	return str;
}
