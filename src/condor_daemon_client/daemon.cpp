/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


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
#include "condor_daemon_core.h"
#include "dc_collector.h"
#include "time_offset.h"
#include "condor_netdb.h"
#include "daemon_core_sock_adapter.h"
#include "subsystem_info.h"
#include "condor_sinful.h"

#include "counted_ptr.h"

void
Daemon::common_init() {
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
	m_daemon_ad_ptr = NULL;
	char buf[200];
	sprintf(buf,"%s_TIMEOUT_MULTIPLIER",get_mySubSystem()->getName() );
	Sock::set_timeout_multiplier( param_integer(buf,0) );
	m_has_udp_command_port = true;
}


Daemon::Daemon( daemon_t tType, const char* tName, const char* tPool ) 
{
		// We are no longer allowed to create a "default" collector
		// since there can be more than one
		// Use CollectorList::create()
/*	if ((tType == DT_COLLECTOR) && (tName == NULL)) {
		EXCEPT ( "Daemon constructor (type=COLLECTOR, name=NULL) called" );
		}*/

	common_init();
	_type = tType;

	if( tPool ) {
		_pool = strnewp( tPool );
	} else {
		_pool = NULL;
	}

	if( tName && tName[0] ) {
		if( is_valid_sinful(tName) ) {
			New_addr( strnewp(tName) );
		} else {
			_name = strnewp( tName );
		}
	} 
	dprintf( D_HOSTNAME, "New Daemon obj (%s) name: \"%s\", pool: "  
			 "\"%s\", addr: \"%s\"\n", daemonString(_type), 
			 _name ? _name : "NULL", _pool ? _pool : "NULL",
			 _addr ? _addr : "NULL" );
}


Daemon::Daemon( const ClassAd* tAd, daemon_t tType, const char* tPool ) 
{
	if( ! tAd ) {
		EXCEPT( "Daemon constructor called with NULL ClassAd!" );
	}

	common_init();
	_type = tType;

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
	case DT_CLUSTER:
		_subsys = strnewp( "CLUSTERD" );
		break;
	case DT_COLLECTOR:
		_subsys = strnewp( "COLLECTOR" );
		break;
	case DT_NEGOTIATOR:
		_subsys = strnewp( "NEGOTIATOR" );
		break;
	case DT_CREDD:
		_subsys = strnewp( "CREDD" );
		break;
	case DT_QUILL:
		_subsys = strnewp( "QUILL" );
		break;
	case DT_LEASE_MANAGER:
		_subsys = strnewp( "LEASE_MANAGER" );
		break;
	case DT_GENERIC:
		_subsys = strnewp( "GENERIC" );
		break;
	case DT_HAD:
		_subsys = strnewp( "HAD" );
		break;
	default:
		EXCEPT( "Invalid daemon_type %d (%s) in ClassAd version of "
				"Daemon object", (int)_type, daemonString(_type) );
	}

	if( tPool ) {
		_pool = strnewp( tPool );
	} else {
		_pool = NULL;
	}

	getInfoFromAd( tAd );

	dprintf( D_HOSTNAME, "New Daemon obj (%s) name: \"%s\", pool: "
			 "\"%s\", addr: \"%s\"\n", daemonString(_type), 
			 _name ? _name : "NULL", _pool ? _pool : "NULL",
			 _addr ? _addr : "NULL" );

	// let's have our own copy of the daemon's ad in this case.
	m_daemon_ad_ptr = new ClassAd(*tAd);	

}


Daemon::Daemon( const Daemon &copy ): ClassyCountedPtr()
{
		// initialize all data members to NULL, since deepCopy() has
		// code not to leak anything in case it's overwriting a value
	common_init();
	deepCopy( copy );
}

 
Daemon&
Daemon::operator=(const Daemon &copy)
{
		// don't copy ourself!
	if (&copy != this) {
		deepCopy( copy );
	}
	return *this;
}


void
Daemon::deepCopy( const Daemon &copy )
{
		// NOTE: strnewp(NULL) returns NULL, and doesn't seg fault,
		// which is exactly what we want everywhere in this method.

	New_name( strnewp(copy._name) );
	New_hostname( strnewp(copy._hostname) );
	New_full_hostname( strnewp(copy._full_hostname) );
	New_addr( strnewp(copy._addr) );
	New_version( strnewp(copy._version) );
	New_platform( strnewp(copy._platform) );
	New_pool( strnewp(copy._pool) );

	if( copy._error ) {
		newError( copy._error_code, copy._error );
	} else {
		if( _error ) { 
			delete [] _error;
			_error = NULL;
		}
		_error_code = copy._error_code;
	}

	if( _id_str ) {
		delete [] _id_str;
	}
	_id_str = strnewp( copy._id_str );

	if( _subsys ) {
		delete [] _subsys;
	}
	_subsys = strnewp( copy._subsys );

	_port = copy._port;
	_type = copy._type;
	_is_local = copy._is_local;
	_tried_locate = copy._tried_locate;
	_tried_init_hostname = copy._tried_init_hostname;
	_tried_init_version = copy._tried_init_version;
	_is_configured = copy._is_configured;
	if(copy.m_daemon_ad_ptr) {
		m_daemon_ad_ptr = new ClassAd(*copy.m_daemon_ad_ptr);
	}
		/*
		  there's nothing to copy for _sec_man... it'll already be
		  instantiated at this point, and the SecMan object is really
		  static in CEDAR, anyway, so all it's doing is incrementing a
		  reference count
		*/

	setCmdStr( copy._cmd_str );
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
	if( m_daemon_ad_ptr) { delete m_daemon_ad_ptr; }
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
	} else if( _type == DT_GENERIC ) {
		dt_str = _subsys;
	} else {
		dt_str = daemonString(_type);
	}
	MyString buf;
	if( _is_local ) {
		ASSERT( dt_str );
		buf.sprintf( "local %s", dt_str );
	} else if( _name ) {
		ASSERT( dt_str );
		buf.sprintf( "%s %s", dt_str, _name );
	} else if( _addr ) {
		ASSERT( dt_str );
		Sinful sinful(_addr);
		sinful.clearParams(); // too much info is ugly
		buf.sprintf( "%s at %s", dt_str, sinful.getSinful() );
		if( _full_hostname ) {
			buf.sprintf_cat( " (%s)", _full_hostname );
		}
	} else {
		return "unknown daemon";
	}
	_id_str = strnewp( buf.Value() );
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
Daemon::reliSock( int timeout, time_t deadline, CondorError* errstack, bool non_blocking, bool ignore_timeout_multiplier )
{
	if( !checkAddr() ) {
			// this already deals w/ _error for us...
		return NULL;
	}
	ReliSock* sock;
	sock = new ReliSock();

	sock->set_deadline( deadline );

	if( !connectSock(sock,timeout,errstack,non_blocking,ignore_timeout_multiplier) )
	{
		delete sock;
		return NULL;
	}

	return sock;
}

SafeSock*
Daemon::safeSock( int timeout, time_t deadline, CondorError* errstack, bool non_blocking )
{
	if( !checkAddr() ) {
			// this already deals w/ _error for us...
		return NULL;
	}
	SafeSock* sock;
	sock = new SafeSock();

	sock->set_deadline( deadline );

	if( !connectSock(sock,timeout,errstack,non_blocking) )
	{
		delete sock;
		return NULL;
	}

	return sock;
}


bool
Daemon::connectSock(Sock *sock, int sec, CondorError* errstack, bool non_blocking, bool ignore_timeout_multiplier )
{

	sock->set_peer_description(idStr());
	if( sec ) {
		sock->timeout( sec );
		if( ignore_timeout_multiplier ) {
			sock->ignoreTimeoutMultiplier();
		}
	}

	int rc = sock->connect(_addr, 0, non_blocking);
	if(rc || (non_blocking && rc == CEDAR_EWOULDBLOCK)) {
		return true;
	}

	if (errstack) {
		errstack->pushf("CEDAR", CEDAR_ERR_CONNECT_FAILED,
			"Failed to connect to %s", _addr);
	}
	return false;
}


StartCommandResult
Daemon::startCommand( int cmd, Sock* sock, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking, char const *cmd_description, char *version, SecMan *sec_man, bool raw_protocol, char const *sec_session_id )
{
	// This function may be either blocking or non-blocking, depending
	// on the flag that is passed in.  All versions of Daemon::startCommand()
	// ultimately get here.

	// NOTE: if there is a callback function, we _must_ guarantee that it is
	// eventually called in all code paths.

	StartCommandResult start_command_result = StartCommandFailed;
	bool other_side_can_negotiate = true; //default assumption

	ASSERT(sock);

	// If caller wants non-blocking with no callback function,
	// we _must_ be using UDP.
	ASSERT(!nonblocking || callback_fn || sock->type() == Stream::safe_sock);

	// set up the timeout
	if( timeout ) {
		sock->timeout( timeout );
	}

	// look at the version if it is available.  we must disable
	// negotiation when talking to pre-6.3.3.
	if (version) {
		dprintf(D_SECURITY, "DAEMON: talking to a %s daemon.\n", version);
		CondorVersionInfo vi(version);
		if ( !vi.built_since_version(6,3,3) ) {
			dprintf( D_SECURITY, "DAEMON: "
					 "security negotiation not possible, disabling.\n" );
			other_side_can_negotiate = false;
		}
	}

	start_command_result = sec_man->startCommand(cmd, sock, other_side_can_negotiate, raw_protocol, errstack, 0, callback_fn, misc_data, nonblocking, cmd_description, sec_session_id);

	if(callback_fn) {
		// SecMan::startCommand() called the callback function, so we just return here
		return start_command_result;
	}
	else {
		// There is no callback function.
		return start_command_result;
	}
}

Sock *
Daemon::makeConnectedSocket( Stream::stream_type st,
							 int timeout, time_t deadline,
							 CondorError* errstack, bool non_blocking )
{
	switch( st ) {
	case Stream::reli_sock:
		return reliSock(timeout, deadline, errstack, non_blocking);
	case Stream::safe_sock:
		return safeSock(timeout, deadline, errstack, non_blocking);
	}

	EXCEPT( "Unknown stream_type (%d) in Daemon::makeConnectedSocket",
			(int)st );
	return NULL;
}

StartCommandResult
Daemon::startCommand( int cmd, Stream::stream_type st,Sock **sock,int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking, char const *cmd_description, bool raw_protocol, char const *sec_session_id )
{
	// This function may be either blocking or non-blocking, depending on
	// the flag that was passed in.

	// If caller wants non-blocking with no callback function and we're
	// creating the Sock, there's no way for the caller to finish the
	// command (since it doesn't have the Sock), which makes no sense.
	// Also, there's no one to delete the Sock.
	ASSERT(!nonblocking || callback_fn);

	*sock = makeConnectedSocket(st,timeout,0,errstack,nonblocking);
	if( ! *sock ) {
		if ( callback_fn ) {
			(*callback_fn)( false, NULL, errstack, misc_data );
			return StartCommandSucceeded;
		} else {
			return StartCommandFailed;
		}
	}

	return startCommand (
						 cmd,
						 *sock,
						 timeout,
						 errstack,
						 callback_fn,
						 misc_data,
						 nonblocking,
						 cmd_description,
						 _version,
						 &_sec_man,
						 raw_protocol,
						 sec_session_id);
}

Sock*
Daemon::startCommand( int cmd, Stream::stream_type st, int timeout, CondorError* errstack, char const *cmd_description, bool raw_protocol, char const *sec_session_id )
{
	// This is a blocking version of startCommand.
	const bool nonblocking = false;
	Sock *sock = NULL;
	StartCommandResult rc = startCommand(cmd,st,&sock,timeout,errstack,NULL,NULL,nonblocking,cmd_description,raw_protocol,sec_session_id);
	switch(rc) {
	case StartCommandSucceeded:
		return sock;
	case StartCommandFailed:
		if(sock) {
			delete sock;
		}
		return NULL;
	case StartCommandInProgress:
	case StartCommandWouldBlock: //impossible!
	case StartCommandContinue: //impossible!
		break;
	}
	EXCEPT("startCommand(blocking=true) returned an unexpected result: %d\n",rc);
	return NULL;
}

StartCommandResult
Daemon::startCommand_nonblocking( int cmd, Stream::stream_type st, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, char const *cmd_description, bool raw_protocol, char const *sec_session_id )
{
	// This is a nonblocking version of startCommand.
	const int nonblocking = true;
	Sock *sock = NULL;
	// We require that callback_fn be non-NULL. The startCommand() we call
	// here does that check.
	return startCommand(cmd,st,&sock,timeout,errstack,callback_fn,misc_data,nonblocking,cmd_description,raw_protocol,sec_session_id);
}

StartCommandResult
Daemon::startCommand_nonblocking( int cmd, Sock* sock, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, char const *cmd_description, bool raw_protocol, char const *sec_session_id )
{
	// This is the nonblocking version of startCommand().
	const bool nonblocking = true;
	return startCommand(cmd,sock,timeout,errstack,callback_fn,misc_data,nonblocking,cmd_description,_version,&_sec_man,raw_protocol,sec_session_id);
}

bool
Daemon::startCommand( int cmd, Sock* sock, int timeout, CondorError *errstack, char const *cmd_description,bool raw_protocol, char const *sec_session_id )
{
	// This is a blocking version of startCommand().
	const bool nonblocking = false;
	StartCommandResult rc = startCommand(cmd,sock,timeout,errstack,NULL,NULL,nonblocking,cmd_description,_version,&_sec_man,raw_protocol,sec_session_id);
	switch(rc) {
	case StartCommandSucceeded:
		return true;
	case StartCommandFailed:
		return false;
	case StartCommandInProgress:
	case StartCommandWouldBlock: //impossible!
	case StartCommandContinue: //impossible!
		break;
	}
	EXCEPT("startCommand(nonblocking=false) returned an unexpected result: %d\n",rc);
	return false;
}

bool
Daemon::sendCommand( int cmd, Sock* sock, int sec, CondorError* errstack, char const *cmd_description )
{
	
	if( ! startCommand( cmd, sock, sec, errstack, cmd_description )) {
		return false;
	}
	if( ! sock->eom() ) {
		MyString err_buf;
		err_buf.sprintf( "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( CA_COMMUNICATION_ERROR, err_buf.Value() );
		return false;
	}
	return true;
}


bool
Daemon::sendCommand( int cmd, Stream::stream_type st, int sec, CondorError* errstack, char const *cmd_description )
{
	Sock* tmp = startCommand( cmd, st, sec, errstack, cmd_description );
	if( ! tmp ) {
		return false;
	}
	if( ! tmp->eom() ) {
		MyString err_buf;
		err_buf.sprintf( "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( CA_COMMUNICATION_ERROR, err_buf.Value() );
		delete tmp;
		return false;
	}
	delete tmp;
	return true;
}


bool
Daemon::sendCACmd( ClassAd* req, ClassAd* reply, bool force_auth,
				   int timeout, char const *sec_session_id )
{
	ReliSock cmd_sock;
	return sendCACmd( req, reply, &cmd_sock, force_auth, timeout, sec_session_id );
}


bool
Daemon::sendCACmd( ClassAd* req, ClassAd* reply, ReliSock* cmd_sock,
				   bool force_auth, int timeout, char const *sec_session_id )
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

	if( ! connectSock(cmd_sock) ) {
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
	if( ! startCommand(cmd, cmd_sock, 20, &errstack, NULL, false, sec_session_id) ) {
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
	char* tmp = NULL;

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
	case DT_GENERIC:
		rval = getDaemonInfo( GENERIC_AD );
		break;
	case DT_CLUSTER:
		setSubsystem( "CLUSTER" );
		rval = getDaemonInfo( CLUSTER_AD );
		break;
	case DT_SCHEDD:
		setSubsystem( "SCHEDD" );
		rval = getDaemonInfo( SCHEDD_AD );
		break;
	case DT_STARTD:
		setSubsystem( "STARTD" );
		rval = getDaemonInfo( STARTD_AD );
		break;
	case DT_MASTER:
		setSubsystem( "MASTER" );
		rval = getDaemonInfo( MASTER_AD );
		break;
	case DT_COLLECTOR:
		rval = getCmInfo( "COLLECTOR" );
		break;
	case DT_NEGOTIATOR:
		if( !_pool && (tmp = getCmHostFromConfig( "NEGOTIATOR" )) ) {
				// if NEGOTIATOR_HOST (or equiv) is in the config
				// file, we have to use the old getCmInfo() code to
				// honor what it says... 
			rval = getCmInfo( "NEGOTIATOR" );
			free( tmp );
			tmp = NULL;
		} else {
				// cool, no NEGOTIATOR_HOST, we can treat it just like
				// any other daemon 
	  		setSubsystem( "NEGOTIATOR" );
			rval = getDaemonInfo ( NEGOTIATOR_AD );
		}
		break;
	case DT_CREDD:
	  setSubsystem( "CREDD" );
	  rval = getDaemonInfo( CREDD_AD );
	  break;
	case DT_STORK:
	  setSubsystem( "STORK" );
	  rval = getDaemonInfo( ANY_AD, false );
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
	case DT_QUILL:
		setSubsystem( "QUILL" );
		rval = getDaemonInfo( SCHEDD_AD );
		break;
	case DT_TRANSFERD:
		setSubsystem( "TRANSFERD" );
		rval = getDaemonInfo( ANY_AD );
		break;
	case DT_LEASE_MANAGER:
		setSubsystem( "LEASEMANAGER" );
		rval = getDaemonInfo( LEASE_MANAGER_AD, true );
		break;
	case DT_HAD:
		setSubsystem( "HAD" );
		rval = getDaemonInfo( HAD_AD );
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

	if( _port <= 0 && _addr ) {
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
Daemon::setSubsystem( const char* subsys )
{
	if( _subsys ) {
		delete [] _subsys;
	}
	_subsys = strnewp( subsys );

	return true;
}


bool
Daemon::getDaemonInfo( AdTypes adtype, bool query_collector )
{
	MyString			buf;
	char				*tmp, *my_name;
	char				*host = NULL;
	bool				nameHasPort = false;

	if ( ! _subsys ) {
		dprintf( D_ALWAYS, "Unable to get daemon information because no subsystem specified\n");
		return false;
	}

	if( _addr && is_valid_sinful(_addr) ) {
		dprintf( D_HOSTNAME, "Already have address, no info to locate\n" );
		_is_local = false;
		return true;
	}

		// If we were not passed a name or an addr, check the
		// config file for a subsystem_HOST, e.g. SCHEDD_HOST=XXXX
	if( ! _name  && !_pool ) {
		buf.sprintf( "%s_HOST", _subsys );
		char *specified_host = param( buf.Value() );
		if ( specified_host ) {
				// Found an entry.  Use this name.
			_name = strnewp( specified_host );
			dprintf( D_HOSTNAME, 
					 "No name given, but %s defined to \"%s\"\n",
					 buf.Value(), specified_host );
			free(specified_host);
		}
	}
	if( _name ) {
		// See if daemon name containts a port specification
		_port = getPortFromAddr( _name );
		if ( _port >= 0 ) {
			host = getHostFromAddr( _name );
			if ( host ) {
				nameHasPort = true;
			} else {
				dprintf( D_ALWAYS, "warning: unable to parse hostname from '%s'"
						" but will attempt to use this daemon name anyhow\n",
						_name);
			}
		}
	}

		// _name was explicitly specified as host:port, so this information can
		// be used directly.  Further name resolution is not necessary.
	if( nameHasPort ) {
		struct in_addr sin_addr;
		
		dprintf( D_HOSTNAME, "Port %d specified in name\n", _port );

		if( is_ipaddr(host, &sin_addr) ) {
			buf.sprintf( "<%s:%d>", host, _port );
			New_addr( strnewp(buf.Value()) );
			dprintf( D_HOSTNAME,
					"Host info \"%s\" is an IP address\n", host );
		} else {
				// We were given a hostname, not an address.
			dprintf( D_HOSTNAME, "Host info \"%s\" is a hostname, "
					 "finding IP address\n", host );
			tmp = get_full_hostname( host, &sin_addr );
			if( ! tmp ) {
					// With a hostname, this is a fatal Daemon error.
				buf.sprintf( "unknown host %s", host );
				newError( CA_LOCATE_FAILED, buf.Value() );
				if (host) free( host );

					// We assume this is a transient DNS failure.  Therefore,
					// set _tried_locate = false, so that we keep trying in
					// future calls to locate().
				_tried_locate = false;

				return false;
			}
			buf.sprintf( "<%s:%d>", inet_ntoa(sin_addr), _port );
			dprintf( D_HOSTNAME, "Found IP address and port %s\n", buf.Value() );
			New_addr( strnewp(buf.Value()) );
			New_full_hostname( tmp );
		}

		if (host) free( host );
		_is_local = false;
		return true;

		// Figure out if we want to find a local daemon or not, and
		// fill in the various hostname fields.
	} else if( _name ) {
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
		tmp = NULL;

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
	} else if ( ( _type != DT_NEGOTIATOR ) && ( _type != DT_LEASE_MANAGER ) ) {
			// We were passed neither a name nor an address, so use
			// the local daemon, unless we're NEGOTIATOR, in which case
			// we'll still query the collector even if we don't have the 
            // name
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
		bool foundLocalAd = readLocalClassAd( _subsys );
		if(!foundLocalAd) {
			readAddressFile( _subsys );
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

		if( (_type == DT_STARTD && ! strchr(_name, '@')) ||
			_type == DT_HAD ) { 
				/*
				  So long as an SMP startd has only 1 command socket
				  per startd, we want to take advantage of that and
				  query based on Machine, not Name.  This way, if
				  people supply just the hostname of an SMP, we can
				  still find the daemon.  For example, "condor_vacate
				  host" will vacate all slots on that host, but only if
				  condor_vacate can find the address in the first
				  place.  -Derek Wright 8/19/99 

				  HOWEVER, we only want to query based on ATTR_MACHINE
				  if the name we were given doesn't include an '@'
				  sign already.  if it does, the caller/user must know
				  what they're looking for, and doing the query based
				  just on ATTR_MACHINE will be wrong.  this is
				  especially true in the case of glide-in startds
				  where multiple startds are running on the same
				  machine all reporting to the same collector.
				  -Derek Wright 2005-03-09
				*/
			buf.sprintf( "%s == \"%s\"", ATTR_MACHINE, _full_hostname ); 
			query.addANDConstraint( buf.Value() );
		} else if ( _type == DT_GENERIC ) {
			query.setGenericQueryType(_subsys);
			buf.sprintf("TARGET.%s == \"%s\"", ATTR_TARGET_TYPE, _subsys);
			query.addANDConstraint( buf.Value() );
		} else if ( _name ) {
			buf.sprintf( "%s == \"%s\"", ATTR_NAME, _name ); 
			query.addANDConstraint( buf.Value() );
		} else {
			if ( ( _type != DT_NEGOTIATOR ) && ( _type != DT_LEASE_MANAGER) ) {
					// If we're not querying for negotiator
					//    (which there's only one of)
					// and we don't have the name
					// then how will we possibly know which 
					// result to pick??
				return false;
			}
		}

			// We need to query the collector
		CollectorList * collectors = CollectorList::create(_pool);
		CondorError errstack;
		if (collectors->query (query, ads) != Q_OK) {
			delete collectors;
			newError( CA_LOCATE_FAILED, errstack.getFullText() );
			return false;
		};
		delete collectors;

		ads.Open();
		scan = ads.Next();
		if(!scan) {
			dprintf( D_ALWAYS, "Can't find address for %s %s\n",
					 daemonString(_type), _name ? _name : "" );
			buf.sprintf( "Can't find address for %s %s", 
						 daemonString(_type), _name ? _name : "" );
			newError( CA_LOCATE_FAILED, buf.Value() );
			return false; 
		}

		if ( ! getInfoFromAd( scan ) ) {
			return false;
		}
		if( !m_daemon_ad_ptr) {
			// I don't think we can ever get into a case where we already
			// have located the daemon and have a copy of its ad, but just
			// in case, don't stash another copy of it if we can't find it.
			// I hope this is a deep copy wiht no chaining bullshit
			m_daemon_ad_ptr = new ClassAd(*scan);	
		}
			// The version and platfrom aren't critical, so don't
			// return failure if we can't find them...
		initStringFromAd( scan, ATTR_VERSION, &_version );
		initStringFromAd( scan, ATTR_PLATFORM, &_platform );
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
	MyString buf;
	char* host = NULL;
	char* tmp;
	struct in_addr sin_addr;

	setSubsystem( subsys );

	if( _addr && is_valid_sinful(_addr) ) {
			// only consider addresses w/ a non-zero port "valid"...
		_port = string_to_port( _addr );
		if( _port > 0 ) {
			dprintf( D_HOSTNAME, "Already have address, no info to locate\n" );
			_is_local = false;
			return true;
		}
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
		free( host );
		host = NULL;

			// this is just a fancy wrapper for param()...
		char *hostnames = getCmHostFromConfig( subsys );
		char *itr, *full_name, *host_name, *local_name;
		StringList host_list;

		full_name = my_full_hostname();
		local_name = localName();
		host_list.initializeFromString(hostnames);
		host_list.rewind();
		itr = NULL;
		while ((itr = host_list.next()) != NULL) {
			host_name = getHostFromAddr( itr );
			if ((strlen(full_name) == strlen(host_name) ||
				(strlen(local_name) == strlen(host_name))) &&
				((strcmp(full_name, host_name) == 0) ||
				(strcmp(local_name, host_name) == 0))) {
				host = strnewp(itr);
			}
		}
		free( hostnames );
	}

	if( ! host || !host[0]) {
			// Final step before giving up: check for an address file.
		if( readAddressFile(subsys) ) {
				// if we got the address in the file, we still won't
				// have a good full hostname, so use the local value.
				// everything else (port, hostname, etc), will be
				// initialized and set correctly by our caller based
				// on the fullname and the address.
			New_name( strnewp(my_full_hostname()) );
			New_full_hostname( strnewp(my_full_hostname()) );
			free( host );
			return true;
		}
	}

	if( ! host || !host[0]) {
		buf.sprintf("%s address or hostname not specified in config file",
				 subsys ); 
		newError( CA_LOCATE_FAILED, buf.Value() );
		_is_configured = false;
		if( host ) free( host );

		return false;
	} 

	dprintf( D_HOSTNAME, "Using name \"%s\" to find daemon\n", host ); 

		// See if it's already got a port specified in it, or if we
		// should use the default port for this kind of daemon.
	_port = getPortFromAddr( host );
	if( _port < 0 ) {
		_port = getDefaultPort();
		dprintf( D_HOSTNAME, "Port not specified, using default (%d)\n",
				 _port ); 
	} else {
		dprintf( D_HOSTNAME, "Port %d specified in name\n", _port );
	}
	if( _port == 0 && readAddressFile(subsys) ) {
		dprintf( D_HOSTNAME, "Port 0 specified in name, "
				 "IP/port found in address file\n" );
		New_name( strnewp(my_full_hostname()) );
		New_full_hostname( strnewp(my_full_hostname()) );
		if( host ) {
			free( host );
		}
		return true;
	}

		// If we're here, we've got a real port and there's no address
		// file, so we should store the string we used (as is) in
		// _name, so that we can get to it later if we need it.
	if( ! _name ) {
		New_name( strnewp(host) );
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
		buf.sprintf( "%s address or hostname not specified in config file",
				 subsys ); 
		newError( CA_LOCATE_FAILED, buf.Value() );
		_is_configured = false;
		return false;
	}


	if( is_ipaddr(host, &sin_addr) ) {
		buf.sprintf( "<%s:%d>", host, _port );
		New_addr( strnewp(buf.Value() ) );
		dprintf( D_HOSTNAME, "Host info \"%s\" is an IP address\n", host );
	} else {
			// We were given a hostname, not an address.
		dprintf( D_HOSTNAME, "Host info \"%s\" is a hostname, "
				 "finding IP address\n", host );
		tmp = get_full_hostname( host, &sin_addr );
		if( ! tmp ) {
				// With a hostname, this is a fatal Daemon error.
			buf.sprintf( "unknown host %s", host );
			newError( CA_LOCATE_FAILED, buf.Value() );
			free( host );

				// We assume this is a transient DNS failure.  Therefore,
				// set _tried_locate = false, so that we keep trying in
				// future calls to locate().
			_tried_locate = false;

			return false;
		}
		buf.sprintf( "<%s:%d>", inet_ntoa(sin_addr), _port );
		dprintf( D_HOSTNAME, "Found IP address and port %s\n", buf.Value() );
		New_addr( strnewp(buf.Value() ) );
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
	hostp = condor_gethostbyaddr( (char*)&sockaddr.sin_addr, 
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


bool
Daemon::readAddressFile( const char* subsys )
{
	char* addr_file;
	FILE* addr_fp;
	MyString param_name;
	MyString buf;
	bool rval = false;

	param_name.sprintf( "%s_ADDRESS_FILE", subsys );
	addr_file = param( param_name.Value() );
	if( ! addr_file ) {
		return false;
	}

	dprintf( D_HOSTNAME, "Finding address for local daemon, "
			 "%s is \"%s\"\n", param_name.Value(), addr_file );

	if( ! (addr_fp = safe_fopen_wrapper(addr_file, "r")) ) {
		dprintf( D_HOSTNAME,
				 "Failed to open address file %s: %s (errno %d)\n",
				 addr_file, strerror(errno), errno );
		free( addr_file );
		return false;
	}
		// now that we've got a FILE*, we should free this so we don't
		// leak it.
	free( addr_file );
	addr_file = NULL;

		// Read out the sinful string.
	if( ! buf.readLine(addr_fp) ) {
		dprintf( D_HOSTNAME, "address file contained no data\n" );
		fclose( addr_fp );
		return false;
	}
	buf.chomp();
	if( is_valid_sinful(buf.Value()) ) {
		dprintf( D_HOSTNAME, "Found valid address \"%s\" in "
				 "local address file\n", buf.Value() );
		New_addr( strnewp(buf.Value()) );
		rval = true;
	}

		// Let's see if this is new enough to also have a
		// version string and platform string...
	if( buf.readLine(addr_fp) ) {
			// chop off the newline
		buf.chomp();
		New_version( strnewp(buf.Value()) );
		dprintf( D_HOSTNAME,
				 "Found version string \"%s\" in local address file\n",
				 buf.Value() );
		if( buf.readLine(addr_fp) ) {
			buf.chomp();
			New_platform( strnewp(buf.Value()) );
			dprintf( D_HOSTNAME,
					 "Found platform string \"%s\" in local address file\n",
					 buf.Value() );
		}
	}
	fclose( addr_fp );
	return rval;
}

bool
Daemon::readLocalClassAd( const char* subsys )
{
	char* addr_file;
	FILE* addr_fp;
	ClassAd *adFromFile;
	MyString param_name;
	MyString buf;

	param_name.sprintf( "%s_DAEMON_AD_FILE", subsys );
	addr_file = param( param_name.Value() );
	if( ! addr_file ) {
		return false;
	}

	dprintf( D_HOSTNAME, "Finding classad for local daemon, "
			 "%s is \"%s\"\n", param_name.Value(), addr_file );

	if( ! (addr_fp = safe_fopen_wrapper(addr_file, "r")) ) {
		dprintf( D_HOSTNAME,
				 "Failed to open classad file %s: %s (errno %d)\n",
				 addr_file, strerror(errno), errno );
		free( addr_file );
		return false;
	}
		// now that we've got a FILE*, we should free this so we don't
		// leak it.
	free( addr_file );
	addr_file = NULL;

	int adIsEOF, errorReadingAd, adEmpty = 0;
	adFromFile = new ClassAd(addr_fp, "...", adIsEOF, errorReadingAd, adEmpty);
	ASSERT(adFromFile);
	if(!m_daemon_ad_ptr) {
		m_daemon_ad_ptr = new ClassAd(*adFromFile);
	}
	counted_ptr<ClassAd> smart_ad_ptr(adFromFile);
	
	fclose(addr_fp);

	if(errorReadingAd) {
		return false;	// did that just leak adFromFile?
	}

	return getInfoFromAd( smart_ad_ptr );
}

bool
Daemon::hasUDPCommandPort()
{
	if( !_tried_locate ) {
		locate();
	}
	return m_has_udp_command_port;
}

bool 
Daemon::getInfoFromAd( const ClassAd* ad )
{
	MyString buf = "";
	MyString buf2 = "";
	MyString addr_attr_name = "";
	char *our_network_name = NULL;
		// TODO Which attributes should trigger a failure if we don't find
		// them in the ad? Just _addr?
	bool ret_val = true;
	bool found_addr = false;
	char const *notes = "";

		// We look for _name first because we use it, if available, for
		// error messages if we fail  to find the other attributes.
	initStringFromAd( ad, ATTR_NAME, &_name );

		// Search for the daemon's address. This can be in one of
		// several attributes in the classad. Search through the possible
		// attributes until we find a match.
		// In the future, _addr may become a list of addresses. In that
		// case, we'd want to include all possible matches in the list.
	if ( ad->LookupString( ATTR_PRIVATE_NETWORK_IP_ADDR, buf ) &&
		 ad->LookupString( ATTR_PRIVATE_NETWORK_NAME, buf2 ) &&
		 ( our_network_name = param( "PRIVATE_NETWORK_NAME" ) ) ) {

		StringList addr_list( buf.Value(), ", " );
		StringList network_list( buf2.Value(), "," );

		addr_list.rewind();
		network_list.rewind();
		if ( !addr_list.isEmpty() && !network_list.isEmpty() &&
			 strcmp( network_list.next(), our_network_name ) == 0 ) {

			New_addr( strnewp( addr_list.next() ) );
			found_addr = true;
			addr_attr_name = ATTR_PRIVATE_NETWORK_IP_ADDR;
		}
	}
	free( our_network_name );

	if ( !found_addr && ad->LookupString( ATTR_PUBLIC_NETWORK_IP_ADDR, buf ) ) {
		StringList addr_list( buf.Value(), ", " );

		addr_list.rewind();
		if ( !addr_list.isEmpty() ) {
			New_addr( strnewp( addr_list.next() ) );
			found_addr = true;
			addr_attr_name = ATTR_PUBLIC_NETWORK_IP_ADDR;
		}
	}

	if ( !found_addr ) {
		// construct the IP_ADDR attribute
		buf.sprintf( "%sIpAddr", _subsys );
		if ( ad->LookupString( buf.Value(), buf2 ) ) {
			New_addr( strnewp( buf2.Value() ) );
			found_addr = true;
			addr_attr_name = buf;
		}
	}

	if( _addr ) {
		// See if there is a matching private network address packed
		// into the sinful string (starting in 7.3.0 this replaces the
		// old method of putting it in a separate attribute).

		Sinful sinful(_addr);
		char const *priv_net = sinful.getPrivateNetworkName();
		if( priv_net ) {
			bool using_private = false;
			our_network_name = param( "PRIVATE_NETWORK_NAME" );
			if( our_network_name ) {
				if( strcmp( our_network_name, priv_net ) == 0 ) {
					char const *priv_addr = sinful.getPrivateAddr();
					notes = " (private network name matched)";
					using_private = true;
					if( priv_addr ) {
						// replace address with private address
						if( *priv_addr != '<' ) {
							MyString buf;
							buf.sprintf("<%s>",priv_addr);
							New_addr( strnewp( buf.Value() ) );
						}
						else {
							New_addr( strnewp( priv_addr ) );
						}
					}
					else {
						// no private address was specified, so use public
						// address with CCB disabled
						sinful.setCCBContact(NULL);
						New_addr( strnewp( sinful.getSinful() ) );
					}
				}
				free( our_network_name );
			}
			if( !using_private ) {
				// Remove junk from address that we don't care about so
				// it is not so noisy in logs and such.
				sinful.setPrivateAddr(NULL);
				sinful.setPrivateNetworkName(NULL);
				New_addr( strnewp( sinful.getSinful() ) );
				notes = " (private network name not matched)";
			}
		}
	}

	if ( found_addr ) {
		dprintf( D_HOSTNAME, "Found %s in ClassAd, using \"%s\"%s\n",
				 addr_attr_name.Value(), _addr, notes );
		_tried_locate = true;
	} else {
		dprintf( D_ALWAYS, "Can't find address in classad for %s %s\n",
				 daemonString(_type), _name ? _name : "" );
		buf.sprintf( "Can't find address in classad for %s %s",
					 daemonString(_type), _name ? _name : "" );
		newError( CA_LOCATE_FAILED, buf.Value() );

		ret_val = false;
	}

	if( initStringFromAd( ad, ATTR_VERSION, &_version ) ) {
		_tried_init_version = true;
	} else {
		ret_val = false;
	}

	initStringFromAd( ad, ATTR_PLATFORM, &_platform );

	if( initStringFromAd( ad, ATTR_MACHINE, &_full_hostname ) ) {
		initHostnameFromFull();
		_tried_init_hostname = false;
	} else {
		ret_val = false;
	}

	return ret_val;
}


bool
Daemon::getInfoFromAd( counted_ptr<class ClassAd>& ad )
{
	return getInfoFromAd( ad.get() );
}


bool
Daemon::initStringFromAd( const ClassAd* ad, const char* attrname, char** value )
{
	if( ! value ) {
		EXCEPT( "Daemon::initStringFromAd() called with NULL value!" );
	}
	char* tmp = NULL;
	MyString buf;
	if( ! ad->LookupString(attrname, &tmp) ) {
		dprintf( D_ALWAYS, "Can't find %s in classad for %s %s\n",
				 attrname, daemonString(_type),
				 _name ? _name : "" );
		buf.sprintf( "Can't find %s in classad for %s %s",
					 attrname, daemonString(_type),
					 _name ? _name : "" );
		newError( CA_LOCATE_FAILED, buf.Value() );
		return false;
	}
	if( *value ) {
		delete [] *value;
	}
	*value = strnewp(tmp);
	dprintf( D_HOSTNAME, "Found %s in ClassAd, using \"%s\"\n",
			 attrname, tmp );
	free( tmp );
	tmp = NULL;
	return true;
}

bool
Daemon::initStringFromAd( counted_ptr<class ClassAd>& ad, const char* attrname, char** value )
{
	return initStringFromAd( ad.get(), attrname, value);
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

	if( _addr ) {
		Sinful addr(_addr);
		if( addr.getCCBContact() ) {
			// CCB cannot handle UDP, so pretend this daemon has no
			// UDP port.
			m_has_udp_command_port = false;
		}
	}

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
	bool just_tried_locate = false;
	if( ! _addr ) {
		locate();
		just_tried_locate = true;
	}
	if( ! _addr ) {
			// _error will already be set appropriately
		return false;
	}
	if( _port == 0 ) {
			// if we didn't *just* try locating, we should try again,
			// in case the address file for the thing we're trying to
			// talk to has now been written.
		if( just_tried_locate ) {
			newError( CA_LOCATE_FAILED,
					  "port is still 0 after locate(), address invalid" );
			return false;
		}
			// clear out some things that would confuse locate()
		_tried_locate = false;
		delete [] _addr;
		_addr = NULL;
		if( _is_local ) {
			delete [] _name;
			_name = NULL;
		}
		locate();
		if( _port == 0 ) {
			newError( CA_LOCATE_FAILED,
					  "port is still 0 after locate(), address invalid" );
			return false;
		}
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
	if( rsock->triedAuthentication() ) {
		return true;
	}

	return SecMan::authenticate_sock(rsock, CLIENT_PERM, errstack );
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


char*
getCmHostFromConfig( const char * subsys )
{ 
	MyString buf;
	char* host = NULL;

		// Try the config file for a subsys-specific hostname 
	buf.sprintf( "%s_HOST", subsys );
	host = param( buf.Value() );
	if( host ) {
		if( host[0] ) {
			dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.Value(), 
					 host ); 
			if(host[0] == ':') {
				dprintf( D_ALWAYS, "Warning: Configuration file sets '%s=%s'.  This does not look like a valid host name with optional port.\n", buf.Value(), host);
			}
			return host;
		} else {
			free( host );
		}
	}

		// Try the config file for a subsys-specific IP addr 
	buf.sprintf ("%s_IP_ADDR", subsys );
	host = param( buf.Value() );
	if( host ) {
		if( host[0] ) {
			dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.Value(), host );
			return host;
		} else {
			free( host );
		}
	}

		// settings should take precedence over this). 
	host = param( "CM_IP_ADDR" );
	if( host ) {
		if(  host[0] ) {
			dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.Value(), 
					 host ); 
			return host;
		} else {
			free( host );
		}
	}
	return NULL;
}

/**
 * Contact another daemon and initiate the time offset range 
 * determination logic. We create a socket connection, pass the
 * DC_TIME_OFFSET command then pass the Stream to the cedar stub
 * code for time offset. If this method returns false, then
 * that means we were not able to coordinate our communications
 * with the remote daemon
 * 
 * @param offset - the reference placeholder for the range
 * @return true if it was able to contact the other Daemon
 **/
bool
Daemon::getTimeOffset( long &offset )
{
		//
		// Initialize the offset to the default value
		//
	offset = TIME_OFFSET_DEFAULT;

		//
		// First establish a socket to the other daemon
		//
	ReliSock reli_sock;
	reli_sock.timeout( 30 ); // I'm following what everbody else does
	if( ! connectSock(&reli_sock) ) {
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffset() failed to connect "
		     				  "to remote daemon at '%s'\n",
		     				  this->_addr );
		return ( false );
	}
		//
		// Next send our command to prepare for the call out to the
		// remote daemon
		//
	if( ! this->startCommand( DC_TIME_OFFSET, (Sock*)&reli_sock ) ) { 
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffset() failed to send "
		     				  "command to remote daemon at '%s'\n", 
		     				  this->_addr );
		return ( false );
	}
		//
		// Now that we have established a connection, we'll pass
		// the ReliSock over to the time offset handling code
		//
	return ( time_offset_cedar_stub( (Stream*)&reli_sock, offset ) );
}

/**
 * Contact another daemon and initiate the time offset range 
 * determination logic. We create a socket connection, pass the
 * DC_TIME_OFFSET command then pass the Stream to the cedar stub
 * code for time offset. The min/max range value placeholders
 * are passed in by reference. If this method returns false, then
 * that means for some reason we could not get the range and the
 * range values will default to a known value.
 * 
 * @param min_range - the minimum range value for the time offset
 * @param max_range - the maximum range value for the time offset
 * @return true if it was able to contact the other Daemon
 **/
bool
Daemon::getTimeOffsetRange( long &min_range, long &max_range )
{
		//
		// Initialize the ranges to the default value
		//
	min_range = max_range = TIME_OFFSET_DEFAULT;

		//
		// First establish a socket to the other daemon
		//
	ReliSock reli_sock;
	reli_sock.timeout( 30 ); // I'm following what everbody else does
	if( ! connectSock(&reli_sock) ) {
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffsetRange() failed to connect "
		     				  "to remote daemon at '%s'\n",
		     				  this->_addr );
		return ( false );
	}
		//
		// Next send our command to prepare for the call out to the
		// remote daemon
		//
	if( ! this->startCommand( DC_TIME_OFFSET, (Sock*)&reli_sock ) ) { 
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffsetRange() failed to send "
		     				  "command to remote daemon at '%s'\n", 
		     				  this->_addr );
		return ( false );
	}
		//
		// Now that we have established a connection, we'll pass
		// the ReliSock over to the time offset handling code
		//
	return ( time_offset_range_cedar_stub( (Stream*)&reli_sock,
										   min_range, max_range ) );
}

void Daemon::sendMsg( classy_counted_ptr<DCMsg> msg )
{
		// DCMessenger is garbage collected via ClassyCountedPtr.
		// Ditto for the daemon and message objects.
	DCMessenger *messenger = new DCMessenger(this);

	messenger->startCommand( msg );
}

void Daemon::sendBlockingMsg( classy_counted_ptr<DCMsg> msg )
{
		// DCMessenger is garbage collected via ClassyCountedPtr.
		// Ditto for the daemon and message objects.
	DCMessenger *messenger = new DCMessenger(this);

	messenger->sendBlockingMsg( msg );
}
