/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_ver_info.h"

#include "daemon.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_parameters.h"
#include "condor_adtypes.h"
#include "condor_query.h"
#include "get_daemon_name.h"
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "internet.h"
#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dc_collector.h"

extern char *mySubSystem;


void Daemon::common_init() {
	_type = DT_NONE;
	_port = -1;
	_is_local = false;
	_tried_locate = false;
	_tried_init_hostname = false;
	_tried_init_version = false;
	_is_configured = true;
	_addr = NULL;
	_name = NULL;
	_pool = NULL;
	_version = NULL;
	_platform = NULL;
	_error = NULL;
	_error_code = CA_SUCCESS;
	_id_str = NULL;
	_subsys = NULL;
	_hostname = NULL;
	_full_hostname = NULL;
	_cmd_str = NULL;
	char buf[200];
	sprintf(buf,"%s_TIMEOUT_MULTIPLIER",mySubSystem);
	Sock::set_timeout_multiplier( param_integer(buf,0) );
}


Daemon::Daemon( daemon_t type, const char* name, const char* pool ) 
{
		// We are no longer allowed to create a "default" collector
		// since there can be more than one
		// Use DCCollector::getCollectors()
	if ((type == DT_COLLECTOR) && (name == NULL)) {
		EXCEPT ( "Daemon constructor (type=COLLECTOR, name=NULL) called" );
	}

	common_init();
	_type = type;

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
	dprintf( D_HOSTNAME, "New Daemon obj (%s) name: \"%s\", pool: "
			 "\"%s\", addr: \"%s\"\n", daemonString(_type), 
			 _name ? _name : "NULL", _pool ? _pool : "NULL",
			 _addr ? _addr : "NULL" );
}


Daemon::Daemon( ClassAd* ad, daemon_t type, const char* pool ) 
{
	if( ! ad ) {
		EXCEPT( "Daemon constructor called with NULL ClassAd!" );
	}

	common_init();
	_type = type;

	switch( _type ) {
	case DT_MASTER:
		_subsys = strnewp( "MASTER" );
		break;
	case DT_STARTD:
		_subsys = strnewp( "STARTD" );
		break;
	case DT_SCHEDD:
		_subsys = strnewp( "SCHEDD" );
		break;
	default:
		EXCEPT( "Invalid daemon_type %d (%s) in ClassAd version of "
				"Daemon object", (int)_type, daemonString(_type) );
	}

	if( pool ) {
		_pool = strnewp( pool );
	} else {
		_pool = NULL;
	}

		// construct the appropriate IP_ADDR attribute
	MyString addr_attr = _subsys;
	addr_attr += "IpAddr";

	char *tmp = NULL;
	ad->LookupString( ATTR_NAME, &tmp );
	if( tmp ) {
		_name = strnewp( tmp );
		free( tmp );
		tmp = NULL;
	} 

	ad->LookupString( addr_attr.GetCStr(), &tmp );
	if( tmp ) {
		_addr = strnewp( tmp );
		free( tmp );
		tmp = NULL;
		_tried_locate = true;		
	} 

	ad->LookupString( ATTR_MACHINE, &tmp );
	if( tmp ) {
		_full_hostname = strnewp( tmp );
		free( tmp );
		tmp = NULL;
		initHostnameFromFull();
		_tried_init_hostname = false;
	} 

	ad->LookupString( ATTR_VERSION, &tmp );
	if( tmp ) {
		_version = strnewp( tmp );
		free( tmp );
		tmp = NULL;
		_tried_init_version = true;
	} 

	ad->LookupString( ATTR_PLATFORM, &tmp );
	if( tmp ) {
		_platform = strnewp( tmp );
		free( tmp );
		tmp = NULL;
	} 

	dprintf( D_HOSTNAME, "New Daemon obj (%s) name: \"%s\", pool: "
			 "\"%s\", addr: \"%s\"\n", daemonString(_type), 
			 _name ? _name : "NULL", _pool ? _pool : "NULL",
			 _addr ? _addr : "NULL" );
}


Daemon::~Daemon() 
{
	if( DebugFlags & D_HOSTNAME ) {
		dprintf( D_HOSTNAME, "Destroying Daemon object:\n" );
		display( D_HOSTNAME );
		dprintf( D_HOSTNAME, " --- End of Daemon object info ---\n" );
	}
	if( _name ) delete [] _name;
	if( _pool ) delete [] _pool;
	if( _addr ) delete [] _addr;
	if( _error ) delete [] _error;
	if( _id_str ) delete [] _id_str;
	if( _subsys ) delete [] _subsys;
	if( _hostname ) delete [] _hostname;
	if( _full_hostname ) delete [] _full_hostname;
	if( _version ) delete [] _version;
	if( _platform ) { delete [] _platform; }
	if( _cmd_str ) { delete [] _cmd_str; }
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
	if( ! _hostname && ! _tried_init_hostname ) {
		initHostname();
	}
	return _hostname;
}


char*
Daemon::version( void )
{
	if( ! _version && ! _tried_init_version ) {
		initVersion();
	}
	return _version;
}


char*
Daemon::platform( void )
{
	if( ! _platform && ! _tried_init_version ) {
		initVersion();
	}
	return _platform;
}


char*
Daemon::fullHostname( void )
{
	if( ! _full_hostname && ! _tried_init_hostname ) {
		initHostname();
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
	locate();

	const char* dt_str;
	if( _type == DT_ANY ) {
		dt_str = "daemon";
	} else {
		dt_str = daemonString(_type);
	}
	char buf[128];
	if( _is_local ) {
		sprintf( buf, "local %s", dt_str );
	} else if( _name ) {
		sprintf( buf, "%s %s", dt_str, _name );
	} else if( _addr ) {
		sprintf( buf, "%s at %s", dt_str, _addr );
		if( _full_hostname ) {
			strcat( buf, " (" );
			strcat( buf, _full_hostname );
			strcat( buf, ")" );
		}
	} else {
		return "unknown daemon";
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
Daemon::reliSock( int sec, CondorError* errstack )
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
		if (errstack) {
			errstack->pushf("CEDAR", CEDAR_ERR_CONNECT_FAILED,
				"Failed to connect to %s", _addr);
		}
		delete reli;
		return NULL;
	}
}


SafeSock*
Daemon::safeSock( int sec, CondorError* errstack )
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
		if (errstack) {
			errstack->pushf("CEDAR", CEDAR_ERR_CONNECT_FAILED,
				"Failed to connect to %s", _addr);
		}
		delete safe;
		return NULL;
	}
}


Sock*
Daemon::startCommand( int cmd, Stream::stream_type st, int sec, CondorError* errstack )
{
	Sock* sock;
	switch( st ) {
	case Stream::reli_sock:
		sock = reliSock(sec, errstack);
		break;
	case Stream::safe_sock:
		sock = safeSock(sec, errstack);
		break;
	default:
		EXCEPT( "Unknown stream_type (%d) in Daemon::startCommand",
				(int)st );
	}
	if( ! sock ) {
			// _error will already be set.
		return NULL;
	}


	if (startCommand ( cmd, sock, sec, errstack )) {
		return sock;
	} else {
		delete sock;
		return NULL;
	}

}


bool
Daemon::startCommand( int cmd, Sock* sock, int sec, CondorError *errstack )
{

	// basic sanity check
	if( ! sock ) {
		dprintf ( D_ALWAYS, "startCommand() called with a NULL Sock*, failing.\n" );
		return false;
	} else {
		dprintf ( D_SECURITY, "STARTCOMMAND: starting %i to %s on %s port %i.\n", cmd, sin_to_string(sock->endpoint()), (sock->type() == Stream::safe_sock) ? "UDP" : "TCP", sock->get_port());
	}

	// set up the timeout
	if( sec ) {
		sock->timeout( sec );
	}

	// give them the benefit of the doubt
	bool    other_side_can_negotiate = true;

	// look at the version if it is available.  we must disable
	// negotiation when talking to pre-6.3.3.
	if (_version) {
		dprintf(D_SECURITY, "DAEMON: talking to a %s daemon.\n", _version);
		CondorVersionInfo vi(_version);
		if ( !vi.built_since_version(6,3,3) ) {
			dprintf( D_SECURITY, "DAEMON: "
					 "security negotiation not possible, disabling.\n" );
			other_side_can_negotiate = false;
		}
	}

	// if they passed in NULL (the default for backwards compatibility),
	// we'll collect the errors and dump them in the log if there's a
	// failure.  to collect the errors, we need our own errstack
	CondorError stack_errstack;

	// new select which one we will use.  default to ours but pick
	// the one passed in if it's not NULL.
	CondorError *errstack_select = &stack_errstack;
	if ( errstack ) {
		errstack_select = errstack;
	}

	// call startCommand with the selected error stack
	bool result = _sec_man.startCommand(cmd, sock, other_side_can_negotiate, errstack_select);

	// dump the errors in the log if not being collected
	if (!result && !errstack) {
		dprintf( D_ALWAYS, "ERROR: %s\n", errstack_select->getFullText() );
	}
				
	return result;

}

bool
Daemon::sendCommand( int cmd, Sock* sock, int sec, CondorError* errstack )
{
	
	if( ! startCommand( cmd, sock, sec, errstack )) {
		return false;
	}
	if( ! sock->eom() ) {
		char err_buf[256];
		sprintf( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( CA_COMMUNICATION_ERROR, err_buf );
		return false;
	}
	return true;
}


bool
Daemon::sendCommand( int cmd, Stream::stream_type st, int sec, CondorError* errstack )
{
	Sock* tmp = startCommand( cmd, st, sec, errstack );
	if( ! tmp ) {
		return false;
	}
	if( ! tmp->eom() ) {
		char err_buf[256];
		sprintf( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( CA_COMMUNICATION_ERROR, err_buf );
		delete tmp;
		return false;
	}
	delete tmp;
	return true;
}


bool
Daemon::sendCACmd( ClassAd* req, ClassAd* reply, bool force_auth,
				   int timeout )
{
	ReliSock cmd_sock;
	return sendCACmd( req, reply, &cmd_sock, force_auth, timeout );
}


bool
Daemon::sendCACmd( ClassAd* req, ClassAd* reply, ReliSock* cmd_sock,
				   bool force_auth, int timeout )
{
	if( !req ) {
		newError( CA_INVALID_REQUEST,
				  "sendCACmd() called with no request ClassAd" ); 
		return false;
	}
	if( !reply ) {
		newError( CA_INVALID_REQUEST,
				  "sendCACmd() called with no reply ClassAd" );
		return false;
	}
	if( ! cmd_sock ) {
		newError( CA_INVALID_REQUEST,
				  "sendCACmd() called with no socket to use" );
		return false;
	}
	if( !checkAddr() ) {
			// this already deals w/ _error for us...
		return false;
	}
	
	req->SetMyTypeName( COMMAND_ADTYPE );
	req->SetTargetTypeName( REPLY_ADTYPE );

	if( timeout >= 0 ) {
		cmd_sock->timeout( timeout );
	}

	if( ! cmd_sock->connect(_addr) ) {
		MyString err_msg = "Failed to connect to ";
		err_msg += daemonString(_type);
		err_msg += " ";
		err_msg += _addr;
		newError( CA_CONNECT_FAILED, err_msg.Value() );
		return false;
	}

	int cmd;
	if( force_auth ) {
		cmd = CA_AUTH_CMD;
	} else {
		cmd = CA_CMD;
	}
	CondorError errstack;
	if( ! startCommand(cmd, cmd_sock, 20, &errstack) ) {
		MyString err_msg = "Failed to send command (";
		if( cmd == CA_CMD ) {
			err_msg += "CA_CMD";
		} else {
			err_msg += "CA_AUTH_CMD";
		}
		err_msg += "): ";
		err_msg += errstack.getFullText();
		newError( CA_COMMUNICATION_ERROR, err_msg.Value() );
		return false;
	}
	if( force_auth ) {
		CondorError e;
		if( ! forceAuthentication(cmd_sock, &e) ) {
			newError( CA_NOT_AUTHENTICATED, e.getFullText() );
			return false;
		}
	}

		// due to an EVIL bug in authenticate(), our timeout just got
		// set to 20.  so, if we were given a timeout, we have to set
		// it again... :(
	if( timeout >= 0 ) {
		cmd_sock->timeout( timeout );
	}

	if( ! req->put(*cmd_sock) ) { 
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send request ClassAd" );
		return false;
	}
	if( ! cmd_sock->end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR,
				  "Failed to send end-of-message" );
		return false;
	}

		// Now, try to get the reply
	cmd_sock->decode();
	if( ! reply->initFromStream(*cmd_sock) ) {
		newError( CA_COMMUNICATION_ERROR, "Failed to read reply ClassAd" );
		return false;
	}
	if( !cmd_sock->end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR, "Failed to read end-of-message" );
		return false;
	}

		// Finally, interpret the results
	char* result_str = NULL;
	if( ! reply->LookupString(ATTR_RESULT, &result_str) ) {
		MyString err_msg = "Reply ClassAd does not have ";
		err_msg += ATTR_RESULT;
		err_msg += " attribute";
		newError( CA_INVALID_REPLY, err_msg.Value() );
		return false;
	}
	CAResult result = getCAResultNum( result_str );
	if( result == CA_SUCCESS ) { 
			// we recognized it and it's good, just return.
		free( result_str );
		return true;		
	}

		// Either we don't recognize the result, or it's some known
		// failure.  Either way, look for the error string if there is
		// one, and set it. 
	char* err = NULL;
	if( ! reply->LookupString(ATTR_ERROR_STRING, &err) ) {
		if( ! result ) {
				// we didn't recognize the result, so don't assume
				// it's a failure, just let the caller interpret the
				// reply ClassAd if they know how...
			free( result_str );
			return true;
		}
			// otherwise, it's a known failure, but there's no error
			// string to help us...
		MyString err_msg = "Reply ClassAd returned '";
		err_msg += result_str;
		err_msg += "' but does not have the ";
		err_msg += ATTR_ERROR_STRING;
		err_msg += " attribute";
		newError( result, err_msg.Value() );
		free( result_str );
		return false;
	}
	if( result ) {
			// We recognized the error result code, so use that. 
		newError( result, err );
	} else {
			// The only way this is possible is if the reply is using
			// codes in the CAResult enum that we don't yet recognize.
			// From our perspective, it's an invalid reply, something
			// we're not prepared to handle.  The caller can further
			// interpret the reply classad if they know how...
		newError( CA_INVALID_REPLY, err );
	}			  
	free( err );
	free( result_str );
	return false;
}


//////////////////////////////////////////////////////////////////////
// Locate-related methods
//////////////////////////////////////////////////////////////////////

bool
Daemon::locate( void )
{
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
	case DT_ANY:
		// don't do anything
		rval = true;
		break;
	case DT_CLUSTER:
		rval = getDaemonInfo( "CLUSTER", CLUSTER_AD );
		break;
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
		rval = getCmInfo( "COLLECTOR" );
		break;
	case DT_NEGOTIATOR:
		rval = getCmInfo( "NEGOTIATOR" );
		break;
	case DT_CREDD:
	  rval = getDaemonInfo( "CREDD", ANY_AD, false );
	  break;
	case DT_VIEW_COLLECTOR:
		if( (rval = getCmInfo("CONDOR_VIEW")) ) {
				// If we found it, we're done.
			break;
		} 
			// If there's nothing CONDOR_VIEW-specific, try just using
			// "COLLECTOR".
		rval = getCmInfo( "COLLECTOR" ); 
		break;
	default:
		EXCEPT( "Unknown daemon type (%d) in Daemon::init", (int)_type );
	}

	if( ! rval) {
			// _error will already be set appropriately.
		return false;
	}

		// Now, deal with everything that's common between both.

		// The helpers all try to set _full_hostname, but not
		// _hostname.  If we've got the full host, we always want to
		// trim off the domain for _hostname.
	initHostnameFromFull();

	if( _port < 0 && _addr ) {
			// If we have the sinful string and no port, fill it in
		_port = string_to_port( _addr );
		dprintf( D_HOSTNAME, "Using port %d based on address \"%s\"\n",
				 _port, _addr );
	}

		// Now that we're done with the get*Info() code, if we're a
		// local daemon and we still don't have a name, fill it in.  
	if( ! _name && _is_local) {
		_name = localName();
	}

	return true;
}


bool
Daemon::getDaemonInfo( const char* subsys, AdTypes adtype, bool query_collector)
{
	char				buf[512], tmpname[512];
	char				*addr_file, *tmp, *my_name;
	FILE				*addr_fp;

	if( _subsys ) {
		delete [] _subsys;
	}
	_subsys = strnewp( subsys );

	if( _addr && is_valid_sinful(_addr) ) {
		dprintf( D_HOSTNAME, "Already have address, no info to locate\n" );
		_is_local = false;
		return true;
	}

		// If we were not passed a name or an addr, check the
		// config file for a subsystem_HOST, e.g. SCHEDD_HOST=XXXX
	if( ! _name ) {
		sprintf(buf,"%s_HOST",subsys);
		char *specified_host = param(buf);
		if ( specified_host ) {
				// Found an entry.  Use this name.
			_name = strnewp( specified_host );
			dprintf( D_HOSTNAME, 
					 "No name given, but %s defined to \"%s\"\n", buf,
					 specified_host );
			free(specified_host);
		}
	}


		// Figure out if we want to find a local daemon or not, and
		// fill in the various hostname fields.
	if( _name ) {
			// We were passed a name, so try to look it up in DNS to
			// get the full hostname.

		tmp = get_daemon_name( _name );
		if( ! tmp ) {
				// we failed to contruct the daemon name.  the only
				// possible reason for this is being given faulty
				// hostname.  This is a fatal error.
			MyString err_msg = "unknown host ";
			err_msg += get_host_part( _name );
			newError( CA_LOCATE_FAILED, err_msg.Value() );
			return false;
		}
			// if it worked, we've not got the proper values for the
			// name (and the full hostname, since that's just the
			// "host part" of the "name"...
		New_name( tmp );
		dprintf( D_HOSTNAME, "Using \"%s\" for name in Daemon object\n",
				 tmp );
			// now, grab the fullhost from the name we just made...
		tmp = strnewp( get_host_part(_name) ); 
		dprintf( D_HOSTNAME,
				 "Using \"%s\" for full hostname in Daemon object\n", tmp );
		New_full_hostname( tmp );

			// Now that we got this far and have the correct name, see
			// if that matches the name for the local daemon.  
			// If we were given a pool, never assume we're local --
			// always try to query that pool...
		if( _pool ) {
			dprintf( D_HOSTNAME, "Pool was specified, "
					 "forcing collector query\n" );
		} else {
			my_name = localName();
			dprintf( D_HOSTNAME, "Local daemon name would be \"%s\"\n", 
					 my_name );
			if( !strcmp(_name, my_name) ) {
				dprintf( D_HOSTNAME, "Name \"%s\" matches local name and "
						 "no pool given, treating as a local daemon\n",
						 _name );
				_is_local = true;
			}
			delete [] my_name;
		}
	} else {
			// We were passed neither a name nor an address, so use
			// the local daemon.
		_is_local = true;
		New_name( localName() );
		New_full_hostname( strnewp(my_full_hostname()) );
		dprintf( D_HOSTNAME, "Neither name nor addr specified, using local "
				 "values - name: \"%s\", full host: \"%s\"\n", 
				 _name, _full_hostname );
	}

		// Now that we have the real, full names, actually find the
		// address of the daemon in question.

	if( _is_local ) {
		sprintf( buf, "%s_ADDRESS_FILE", subsys );
		addr_file = param( buf );
		if( addr_file ) {
			dprintf( D_HOSTNAME, "Finding address for local daemon, "
					 "%s is \"%s\"\n", buf, addr_file );
			if( (addr_fp = fopen(addr_file, "r")) ) {
					// Read out the sinful string.
				fgets( buf, 100, addr_fp );
					// chop off the newline
				chomp( buf );
				if( is_valid_sinful(buf) ) {
					dprintf( D_HOSTNAME, "Found valid address \"%s\" in "
							 "local address file\n", buf );
					New_addr( strnewp(buf) );
				}
					// Let's see if this is new enough to also have a
					// version string and platform string...
				if( fgets(buf, 200, addr_fp) ) {
						// chop off the newline
					chomp( buf );
					New_version( strnewp(buf) );
					dprintf( D_HOSTNAME, "Found version string \"%s\" in "
							 "local address file\n", buf );
					if( fgets(buf, 200, addr_fp) ) {
							// chop off the newline
						chomp( buf );
						New_platform( strnewp(buf) );
						dprintf( D_HOSTNAME, "Found platform string \"%s\" "
								 "in local address file\n", buf );
					}
				}
				fclose( addr_fp );
			}
			free( addr_file );
		} 
	}

	if ((! _addr) && (!query_collector)) {
	  return false;
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

			// We need to query the collector(s0

		DaemonList * collectors = DCCollector::getCollectors();
		CondorError errstack;
		if (query.fetchAds(ads, collectors, &errstack) != Q_OK) {
			delete collectors;
			newError( CA_LOCATE_FAILED, errstack.getFullText() );
			return false;
		};
		delete collectors;

		ads.Open();
		scan = ads.Next();
		if(!scan) {
			dprintf(D_ALWAYS, "Can't find address for %s %s\n", 
					 daemonString(_type), _name );
			sprintf( buf, "Can't find address for %s %s", 
					 daemonString(_type), _name );
			newError( CA_LOCATE_FAILED, buf );
			return false; 
		}

		// construct the IP_ADDR attribute
		sprintf( tmpname, "%sIpAddr", subsys );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s\n",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( CA_LOCATE_FAILED, buf );
			return false;
		}
		New_addr( strnewp(buf) );
		dprintf( D_HOSTNAME, "Found %s in ClassAd from collector, "
				 "using address \"%s\"\n", tmpname, buf );

		sprintf( tmpname, ATTR_VERSION );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s\n",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( CA_LOCATE_FAILED, buf );
			return false;
		}
		New_version( strnewp(buf) );
		dprintf( D_HOSTNAME, "Found %s in ClassAd from collector, "
				 "using version \"%s\"\n", tmpname, buf );

		sprintf( tmpname, ATTR_PLATFORM );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s\n",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( CA_LOCATE_FAILED, buf );
			return false;
		}
		New_platform( strnewp(buf) );
		dprintf( D_HOSTNAME, "Found %s in ClassAd from collector, "
				 "using platform \"%s\"\n", tmpname, buf );
	}

		// Now that we have the sinful string, fill in the port. 
	_port = string_to_port( _addr );
	dprintf( D_HOSTNAME, "Using port %d based on address \"%s\"\n",
			 _port, _addr );
	return true;
}


bool
Daemon::getCmInfo( const char* subsys )
{
	char buf[128];
	char* host = NULL;
	char* tmp;
	struct in_addr sin_addr;

	if( _subsys ) {
		delete [] _subsys;
	}
	_subsys = strnewp( subsys );

	if( _addr && is_valid_sinful(_addr) ) {
		dprintf( D_HOSTNAME, "Already have address, no info to locate\n" );
		_is_local = false;
		return true;
	}

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
		host = strdup( _name );
		_is_local = false;
	}


	if( ! host  || !host[0] ) {
		getCmHostFromConfig ( subsys, host );
	}

	if( ! host || !host[0]) {
		sprintf( buf, "%s address or hostname not specified in config file",
				 subsys ); 
		newError( CA_LOCATE_FAILED, buf );
		_is_configured = false;
		if( host ) free( host );
		return false;
	} 

		// Now that we finally know what we're going to use, we should
		// store that (as is) in _name, so that we can get to it later
		// if we need it.
	if( ! _name ) {
		New_name( strnewp(host) );
	}
	dprintf( D_HOSTNAME, "Using name \"%s\" to find daemon\n", host ); 

		// See if it's already got a port specified in it, or if we
		// should use the default port for this kind of daemon.
	_port = getPortFromAddr( host );
	if( ! _port ) {
		_port = getDefaultPort();
		dprintf( D_HOSTNAME, "Port not specified, using default (%d)\n",
				 _port ); 
	} else {
		dprintf( D_HOSTNAME, "Port %d specified in name\n", _port );
	}

		// Now that we've got the port, grab the hostname for the rest
		// of the logic.  first, stash the copy of the hostname with
		// our handy helper method, then free() the full version
		// (which we've already got stashed in _name if we need it),
		// and finally reset host to point to the host for the rest of
		// this function.
	tmp = getHostFromAddr( host );
	free( host );
	host = tmp;
	tmp = NULL;



	if ( !host ) {
		sprintf( buf, "%s address or hostname not specified in config file",
				 subsys ); 
		newError( CA_LOCATE_FAILED, buf );
		_is_configured = false;
		return false;
	}


	if( is_ipaddr(host, &sin_addr) ) {
		sprintf( buf, "<%s:%d>", host, _port );
		New_addr( strnewp(buf) );
		dprintf( D_HOSTNAME, "Host info \"%s\" is an IP address\n", host );
	} else {
			// We were given a hostname, not an address.
		dprintf( D_HOSTNAME, "Host info \"%s\" is a hostname, "
				 "finding IP address\n", host );
		tmp = get_full_hostname( host, &sin_addr );
		if( ! tmp ) {
				// With a hostname, this is a fatal Daemon error.
			sprintf( buf, "unknown host %s", host );
			newError( CA_LOCATE_FAILED, buf );
			free( host );
			return false;
		}
		sprintf( buf, "<%s:%d>", inet_ntoa(sin_addr), _port );
		dprintf( D_HOSTNAME, "Found IP address and port %s\n", buf );
		New_addr( strnewp(buf) );
		New_full_hostname( tmp );
	}

		// If the pool was set, we want to use _name for that, too. 
	if( _pool ) {
		New_pool( strnewp(_name) );
	}

	free( host );
	return true;
}

bool
Daemon::getCmHostFromConfig (const char * subsys, char *& host) { 
	MyString buf;

		// Try the config file for a subsys-specific hostname 
	buf.sprintf( "%s_HOST", subsys );
	host = param( buf.Value() );
	if( host && host[0] ) {
		dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.Value(), 
				 host ); 
		return true;
	}

		// Try the config file for a subsys-specific IP addr 
	buf.sprintf ("%s_IP_ADDR", subsys );
	host = param( buf.Value() );
	if( host && host[0] ) {
		dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.Value(), host );
		return true;
	}

		// settings should take precedence over this). 
	host = param( "CM_IP_ADDR" );
	if( host && host[0] ) {
		dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.Value(), 
				 host ); 
		return true;
	}

	return false;
}

bool
Daemon::initHostname( void )
{
		// make sure we only try this once
	if( _tried_init_hostname ) {
		return true;
	}
	_tried_init_hostname = true;

		// if we already have the info, we're done
	if( _hostname && _full_hostname ) {
		return true;
	}

		// if we haven't tried to locate yet, we should do that now,
		// since that's usually the best way to get the hostnames, and
		// we get everything else we need, while we're at it...
	if( ! _tried_locate ) {
		locate();
	}

	if( ! _addr ) {
			// this is bad...
		return false;
	}

			// We have no name, but we have an address.  Try to do an
			// inverse lookup and fill in the hostname info from the
			// IP address we already have.

	dprintf( D_HOSTNAME, "Address \"%s\" specified but no name, "
			 "looking up host info\n", _addr );

	struct sockaddr_in sockaddr;
	struct hostent* hostp;
	string_to_sin( _addr, &sockaddr );
	hostp = gethostbyaddr( (char*)&sockaddr.sin_addr, 
						   sizeof(struct in_addr), AF_INET ); 
	if( ! hostp ) {
		New_hostname( NULL );
		New_full_hostname( NULL );
		dprintf( D_HOSTNAME, "gethostbyaddr() failed: %s (errno: %d)\n",
				 strerror(errno), errno );
		MyString err_msg = "can't find host info for ";
		err_msg += _addr;
		newError( CA_LOCATE_FAILED, err_msg.Value() );
		return false;
	}

		// This will print all the D_HOSTNAME messages we need, and it
		// returns a newly allocated string, so we won't need to
		// strnewp() it again
	char* tmp = get_full_hostname_from_hostent( hostp, NULL );
	New_full_hostname( tmp );
	initHostnameFromFull();
	return true;
}


bool
Daemon::initHostnameFromFull( void )
{
	char* copy;
	char* tmp;

		// many of the code paths that find the hostname info just
		// fill in _full_hostname, but not _hostname.  In all cases,
		// if we have _full_hostname we just want to trim off the
		// domain the same way for _hostname.
	if( _full_hostname ) {
		copy = strnewp( _full_hostname );
		tmp = strchr( copy, '.' );
		if( tmp ) {
			*tmp = '\0';
		}
		New_hostname( strnewp(copy) );
		delete [] copy;
		return true; 
	}
	return false;
}


bool
Daemon::initVersion( void )
{
		// make sure we only try this once
	if( _tried_init_version ) {
		return true;
	}
	_tried_init_version = true;

		// if we already have the info, we're done
	if( _version && _platform ) {
		return true;
	}

		// if we haven't done the full locate, we should do that now,
		// since that's the most likely way to find the version and
		// platform strings, and we get lots of other good info while
		// we're at it...
	if( ! _tried_locate ) {
		locate();
	}

		// If we didn't find the version string via locate(), and
		// we're a local daemon, try to ident the daemon's binary
		// directly. 
	if( ! _version && _is_local ) {
		dprintf( D_HOSTNAME, "No version string in local address file, "
				 "trying to find it in the daemon's binary\n" );
		char* exe_file = param( _subsys );
		if( exe_file ) {
			char ver[128];
			CondorVersionInfo vi;
			vi.get_version_from_file(exe_file, ver, 128);
			New_version( strnewp(ver) );
			dprintf( D_HOSTNAME, "Found version string \"%s\" "
					 "in local binary (%s)\n", ver, exe_file );
			free( exe_file );
			return true;
		} else {
			dprintf( D_HOSTNAME, "%s not defined in config file, "
					 "can't locate daemon binary for version info\n", 
					 _subsys );
			return false;
		}
	}

		// if we're not local, and locate() didn't find the version
		// string, we're screwed.
	dprintf( D_HOSTNAME, "Daemon isn't local and couldn't find "
			 "version string with locate(), giving up\n" );
	return false;
}


int
Daemon::getDefaultPort( void )
{
	switch( _type ) {
	case DT_COLLECTOR:
		return COLLECTOR_PORT;
		break;
	case DT_NEGOTIATOR:
		return NEGOTIATOR_PORT;
		break;
	case DT_VIEW_COLLECTOR:
		return CONDOR_VIEW_PORT;
		break;
	default:
		return 0;
		break;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////
// Other helper methods
//////////////////////////////////////////////////////////////////////

void
Daemon::newError( CAResult err_code, const char* str )
{
	if( _error ) {
		delete [] _error;
	}
	_error = strnewp( str );
	_error_code = err_code;
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
Daemon::New_platform ( char* plat )
{
	if( _platform ) {
		delete [] _platform;
	} 
	_platform = plat;
	return plat;
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


bool
Daemon::checkAddr( void )
{
	if( ! _addr ) {
		locate();
	}
	if( ! _addr ) {
			// _error will already be set appropriately
		return false;
	}
	return true;
}


bool
Daemon::forceAuthentication( ReliSock* rsock, CondorError* errstack )
{
	if( ! rsock ) {
		return false;
	}

		// If we're already authenticated, return success...
	if( rsock->isAuthenticated() ) {
		return true;
	}

	char *p = SecMan::getSecSetting( "SEC_%s_AUTHENTICATION_METHODS",
									 "CLIENT" ); 
	MyString methods;
	if( p ) {
		methods = p;
		free(p);
	} else {
		methods = SecMan::getDefaultAuthenticationMethods();
	}
	return rsock->authenticate( methods.Value(), errstack );
}


void
Daemon::setCmdStr( const char* cmd )
{
	if( _cmd_str ) { 
		delete [] _cmd_str;
		_cmd_str = NULL;
	}
	if( cmd ) {
		_cmd_str = strnewp( cmd );
	}
}
