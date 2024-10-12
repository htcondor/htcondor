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
#include "condor_version.h"
#include "condor_open.h"

#include "daemon.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_query.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "condor_daemon_core.h"
#include "dc_collector.h"
#include "time_offset.h"
#include "condor_netdb.h"
#include "subsystem_info.h"
#include "condor_netaddr.h"
#include "condor_sinful.h"
#include "condor_claimid_parser.h"
#include "authentication.h"

#include "ipv6_hostname.h"

void
Daemon::common_init() {
	_type = DT_NONE;
	_port = -1;
	_is_local = false;
	_tried_locate = false;
	_tried_init_hostname = false;
	_tried_init_version = false;
	_is_configured = true;
	_error_code = CA_SUCCESS;
	m_daemon_ad_ptr = NULL;
	char buf[200];
	snprintf(buf,sizeof(buf),"%s_TIMEOUT_MULTIPLIER",get_mySubSystem()->getName() );
	Sock::set_timeout_multiplier( param_integer(buf, param_integer("TIMEOUT_MULTIPLIER", 0)) );
	dprintf(D_DAEMONCORE, "*** TIMEOUT_MULTIPLIER :: %d\n", Sock::get_timeout_multiplier());
	m_has_udp_command_port = true;
	collector_list_it = collector_list.begin();
}

DaemonAllowLocateFull::DaemonAllowLocateFull( daemon_t tType, const char* tName, const char* tPool ) 
	: Daemon(  tType, tName,  tPool ) 
{

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
		_pool = tPool;
	}

	if( tName && tName[0] ) {
		if( is_valid_sinful(tName) ) {
			Set_addr(tName);
		} else {
			_name = tName;
		}
	} 
	dprintf( D_HOSTNAME, "New Daemon obj (%s) name: \"%s\", pool: "  
			 "\"%s\", addr: \"%s\"\n", daemonString(_type), 
			 _name.c_str(), _pool.c_str(), _addr.c_str());
}

DaemonAllowLocateFull::DaemonAllowLocateFull( const ClassAd* tAd, daemon_t tType, const char* tPool ) 
	: Daemon(  tAd,  tType,  tPool ) 
{

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
		_subsys = "MASTER";
		break;
	case DT_STARTD:
		_subsys = "STARTD";
		break;
	case DT_SCHEDD:
		_subsys = "SCHEDD";
		break;
	case DT_CLUSTER:
		_subsys = "CLUSTERD";
		break;
	case DT_COLLECTOR:
		_subsys = "COLLECTOR";
		break;
	case DT_NEGOTIATOR:
		_subsys = "NEGOTIATOR";
		break;
	case DT_CREDD:
		_subsys = "CREDD";
		break;
	case DT_GENERIC:
		_subsys = "GENERIC";
		break;
	case DT_HAD:
		_subsys = "HAD";
		break;
	default:
		EXCEPT( "Invalid daemon_type %d (%s) in ClassAd version of "
				"Daemon object", (int)_type, daemonString(_type) );
	}

	if( tPool ) {
		_pool = tPool;
	}

	getInfoFromAd( tAd );

	dprintf( D_HOSTNAME, "New Daemon obj (%s) name: \"%s\", pool: "
			 "\"%s\", addr: \"%s\"\n", daemonString(_type), 
			 _name.c_str(), _pool.c_str(), _addr.c_str() );

	// let's have our own copy of the daemon's ad in this case.
	m_daemon_ad_ptr = new ClassAd(*tAd);	

}

DaemonAllowLocateFull::DaemonAllowLocateFull( const DaemonAllowLocateFull &copy )
	: Daemon( copy )
{

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
	_name = copy._name;
	_alias = copy._alias;
	_hostname = copy._hostname;
	_full_hostname = copy._full_hostname;
	Set_addr(copy._addr);
	_version = copy._version;
	_platform = copy._platform;

	_error = copy._error;
	_error_code = copy._error_code;

	_id_str = copy._id_str;

	_subsys = copy._subsys;

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

	m_owner = copy.m_owner;
	m_methods = copy.m_methods;

		/*
		  there's nothing to copy for _sec_man... it'll already be
		  instantiated at this point, and the SecMan object is really
		  static in CEDAR, anyway, so all it's doing is incrementing a
		  reference count
		*/

	_cmd_str = copy._cmd_str;
}


Daemon::~Daemon() 
{
	if( IsDebugLevel( D_HOSTNAME ) ) {
		dprintf( D_HOSTNAME, "Destroying Daemon object:\n" );
		display( D_HOSTNAME );
		dprintf( D_HOSTNAME, " --- End of Daemon object info ---\n" );
	}
	if( m_daemon_ad_ptr) { delete m_daemon_ad_ptr; }
}


//////////////////////////////////////////////////////////////////////
// Data-providing methods
//////////////////////////////////////////////////////////////////////

ClassAd *
Daemon::locationAd() {
	if( m_daemon_ad_ptr ) {
		// dprintf( D_ALWAYS, "locationAd(): found daemon ad, returning it\n" );
		return m_daemon_ad_ptr;
	}

	if( m_location_ad_ptr ) {
		// dprintf( D_ALWAYS, "locationAd(): found location ad, returning it\n" );
		return m_location_ad_ptr;
	}

	ClassAd * locationAd = new ClassAd();
	const char * buffer = NULL;

	buffer = this->addr();
	if(! buffer) { goto failure; }
	if(! locationAd->InsertAttr(ATTR_MY_ADDRESS, buffer)) { goto failure; }

	buffer = this->name();
	if(! buffer) { buffer = "Unknown"; }
	if(! locationAd->InsertAttr(ATTR_NAME, buffer)) { goto failure; }

	buffer = this->fullHostname();
	if(! buffer) { buffer = "Unknown"; }
	if(! locationAd->InsertAttr(ATTR_MACHINE, buffer)) { goto failure; }

	/* This will inevitably be overwritten by CondorVersion(), below,
	   so I don't know what the original was attempting accomplish here. */
	buffer = this->version();
	if(! buffer) { buffer = ""; }
	if(! locationAd->InsertAttr(ATTR_VERSION, buffer)) { goto failure; }

	AdTypes ad_type;
	if(! convert_daemon_type_to_ad_type(this->type(), ad_type)) { goto failure; }
	buffer = AdTypeToString(ad_type);
	if(! buffer) { goto failure; }
	if(! locationAd->InsertAttr(ATTR_MY_TYPE, buffer)) { goto failure; }

	buffer = CondorVersion();
	if(! locationAd->InsertAttr(ATTR_VERSION, buffer)) { goto failure; }

	buffer = CondorPlatform();
	if(! locationAd->InsertAttr(ATTR_PLATFORM, buffer)) { goto failure; }

	// dprintf( D_ALWAYS, "locationAd(): synthesized location ad, returning it.\n" );
	m_location_ad_ptr = locationAd;
	return m_location_ad_ptr;

  failure:;
	// dprintf( D_ALWAYS, "Daemon::locationAd() failed.\n" );
	delete locationAd;
	return NULL;
}



const char*
Daemon::name( void )
{
	if( _name.empty() ) {
		locate();
	}
	return _name.empty() ? nullptr : _name.c_str();
}


const char*
Daemon::hostname( void )
{
	if( _hostname.empty() && ! _tried_init_hostname ) {
		initHostname();
	}
	return _hostname.empty() ? nullptr : _hostname.c_str();
}


const char*
Daemon::version( void )
{
	if( _version.empty() && ! _tried_init_version ) {
		initVersion();
	}
	return _version.empty() ? nullptr : _version.c_str();
}


const char*
Daemon::platform( void )
{
	if( _platform.empty() && ! _tried_init_version ) {
		initVersion();
	}
	return _platform.empty() ? nullptr : _platform.c_str();
}


const char*
Daemon::fullHostname( void )
{
	if( _full_hostname.empty() && ! _tried_init_hostname ) {
		initHostname();
	}
	return _full_hostname.empty() ? nullptr : _full_hostname.c_str();
}


const char*
Daemon::addr( void )
{
	if( _addr.empty() ) {
		locate();
	}
	return _addr.empty() ? nullptr : _addr.c_str();
}


const char*
Daemon::pool( void )
{
	if( _pool.empty() ) {
		locate();
	}
	return _pool.empty() ? nullptr : _pool.c_str();
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
	if( ! _id_str.empty() ) {
		return _id_str.c_str();
	}
	locate();

	const char* dt_str;
	if( _type == DT_ANY ) {
		dt_str = "daemon";
	} else if( _type == DT_GENERIC ) {
		dt_str = _subsys.c_str();
	} else {
		dt_str = daemonString(_type);
	}
	std::string buf;
	if( _is_local ) {
		ASSERT( dt_str );
		formatstr( buf, "local %s", dt_str );
	} else if( ! _name.empty() ) {
		ASSERT( dt_str );
		formatstr( buf, "%s %s", dt_str, _name.c_str() );
	} else if( ! _addr.empty() ) {
		ASSERT( dt_str );
		Sinful sinful(_addr.c_str());
		sinful.clearParams(); // too much info is ugly
		formatstr( buf, "%s at %s", dt_str,
		           sinful.getSinful() ? sinful.getSinful() : _addr.c_str() );
		if( ! _full_hostname.empty() ) {
			formatstr_cat( buf, " (%s)", _full_hostname.c_str() );
		}
	} else {
		return "unknown daemon";
	}
	_id_str = buf;
	return _id_str.c_str();
}

void
Daemon::display( int debugflag ) 
{
	dprintf( debugflag, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)_type, daemonString(_type), 
			 _name.c_str(),
			 _addr.c_str() );
	dprintf( debugflag, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 _full_hostname.c_str(),
			 _hostname.c_str(),
			 _pool.c_str(), _port );
	dprintf( debugflag, "IsLocal: %s, IdStr: %s, Error: %s\n", 
			 _is_local ? "Y" : "N",
			 _id_str.c_str(),
			 _error.c_str() );
}


void
Daemon::display( FILE* fp ) 
{
	fprintf( fp, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)_type, daemonString(_type), 
			 _name.c_str(),
			 _addr.c_str() );
	fprintf( fp, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 _full_hostname.c_str(),
			 _hostname.c_str(),
			 _pool.c_str(), _port );
	fprintf( fp, "IsLocal: %s, IdStr: %s, Error: %s\n", 
			 _is_local ? "Y" : "N",
			 _id_str.c_str(),
			 _error.c_str() );
}

bool
Daemon::nextValidCm()
{
	bool rval = false;

	while (rval == false && collector_list_it != collector_list.end()) {
		if (++collector_list_it != collector_list.end()) {
			rval = findCmDaemon(collector_list_it->c_str());
			if( rval == true ) {
				locate();
			}
		}
	}
	return rval;
}


void
Daemon::rewindCmList()
{
	const char *dname = nullptr;

	collector_list_it = collector_list.begin();
	if (!collector_list.empty()) {
		dname = collector_list_it->c_str();
	}
	findCmDaemon( dname );
	locate();
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

	int rc = sock->connect(_addr.c_str(), 0, non_blocking, errstack);
	if(rc || (non_blocking && rc == CEDAR_EWOULDBLOCK)) {
		return true;
	}

	if (errstack) {
		errstack->pushf("CEDAR", CEDAR_ERR_CONNECT_FAILED,
			"Failed to connect to %s", _addr.c_str());
	}
	return false;
}


StartCommandResult
Daemon::startCommand_internal( const SecMan::StartCommandRequest &req, int timeout, SecMan *sec_man )
{
	// This function may be either blocking or non-blocking, depending
	// on the flag that is passed in.  All versions of Daemon::startCommand()
	// ultimately get here.

	// NOTE: if there is a callback function, we _must_ guarantee that it is
	// eventually called in all code paths.

	ASSERT(req.m_sock);

	// If caller wants non-blocking with no callback function,
	// we _must_ be using UDP.
	ASSERT(!req.m_nonblocking || req.m_callback_fn || req.m_sock->type() == Stream::safe_sock);

	// set up the timeout
	if( timeout ) {
		req.m_sock->timeout( timeout );
	}

	auto start_command_result = sec_man->startCommand(req);
	// when sec_man->startCommand returns, sock may have been closed and the sock object deleted.
	// do NOT add code referencing the sock here!!!
	return start_command_result;
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
	default: break;
	}

	EXCEPT( "Unknown stream_type (%d) in Daemon::makeConnectedSocket",
			(int)st );
	return NULL;
}

StartCommandResult
Daemon::startCommand( int cmd, Stream::stream_type st,Sock **sock,int timeout, CondorError *errstack, int subcmd, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking, char const *cmd_description, bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	// This function may be either blocking or non-blocking, depending on
	// the flag that was passed in.

	// If caller wants non-blocking with no callback function and we're
	// creating the Sock, there's no way for the caller to finish the
	// command (since it doesn't have the Sock), which makes no sense.
	// Also, there's no one to delete the Sock.
	ASSERT(!nonblocking || callback_fn);

	if (IsDebugLevel(D_COMMAND)) {
		const char * addr = this->addr();
		dprintf (D_COMMAND, "Daemon::startCommand(%s,...) making connection to %s\n", getCommandStringSafe(cmd), addr ? addr : "NULL");
	}

	*sock = makeConnectedSocket(st,timeout,0,errstack,nonblocking);
	if( ! *sock ) {
		if ( callback_fn ) {
			(*callback_fn)( false, NULL, errstack, "", false, misc_data );
			return StartCommandSucceeded;
		} else {
			return StartCommandFailed;
		}
	}

	// Prepare the request.
	SecMan::StartCommandRequest req;
	req.m_cmd = cmd;
	req.m_sock = *sock;
	req.m_raw_protocol = raw_protocol;
	req.m_resume_response = resume_response;
	req.m_errstack = errstack;
	req.m_subcmd = subcmd;
	req.m_callback_fn = callback_fn;
	req.m_misc_data = misc_data;
	req.m_nonblocking = nonblocking;
	req.m_cmd_description = cmd_description;
	req.m_sec_session_id = sec_session_id ? sec_session_id : m_sec_session_id.c_str();
	req.m_owner = m_owner;
	req.m_methods = m_methods;

	return startCommand_internal( req, timeout, &_sec_man );
}


bool
Daemon::startSubCommand( int cmd, int subcmd, Sock* sock, int timeout, CondorError *errstack, char const *cmd_description,bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	SecMan::StartCommandRequest req;
	req.m_cmd = cmd;
	req.m_sock = sock;
	req.m_raw_protocol = raw_protocol;
	req.m_resume_response = resume_response;
	req.m_errstack = errstack;
	req.m_subcmd = subcmd;
	req.m_callback_fn = nullptr;
	req.m_misc_data = nullptr;
	// This is a blocking version of startCommand().
	req.m_nonblocking = false;
	req.m_cmd_description = cmd_description;
	req.m_sec_session_id = sec_session_id ? sec_session_id : m_sec_session_id.c_str();
	req.m_owner = m_owner;
	req.m_methods = m_methods;

	auto rc = startCommand_internal(req, timeout, &_sec_man);

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
	EXCEPT("startCommand(nonblocking=false) returned an unexpected result: %d",rc);
	return false;
}


Sock*
Daemon::startSubCommand( int cmd, int subcmd, Stream::stream_type st, int timeout, CondorError* errstack, char const *cmd_description, bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	// This is a blocking version of startCommand.
	const bool nonblocking = false;
	Sock *sock = NULL;
	StartCommandResult rc = startCommand(cmd,st,&sock,timeout,errstack,subcmd,NULL,NULL,nonblocking,cmd_description,raw_protocol,sec_session_id,resume_response);
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
	EXCEPT("startCommand(blocking=true) returned an unexpected result: %d",rc);
	return NULL;
}


Sock*
Daemon::startCommand( int cmd, Stream::stream_type st, int timeout, CondorError* errstack, char const *cmd_description, bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	// This is a blocking version of startCommand.
	const bool nonblocking = false;
	Sock *sock = NULL;
	StartCommandResult rc = startCommand(cmd,st,&sock,timeout,errstack,0,NULL,NULL,nonblocking,cmd_description,raw_protocol,sec_session_id,resume_response);
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
	EXCEPT("startCommand(blocking=true) returned an unexpected result: %d",rc);
	return NULL;
}

StartCommandResult
Daemon::startCommand_nonblocking( int cmd, Stream::stream_type st, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, char const *cmd_description, bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	// This is a nonblocking version of startCommand.
	const int nonblocking = true;
	Sock *sock = NULL;
	// We require that callback_fn be non-NULL. The startCommand() we call
	// here does that check.
	return startCommand(cmd,st,&sock,timeout,errstack,0,callback_fn,misc_data,nonblocking,cmd_description,raw_protocol,sec_session_id,resume_response);
}

StartCommandResult
Daemon::startCommand_nonblocking( int cmd, Sock* sock, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, char const *cmd_description, bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	SecMan::StartCommandRequest req;
	req.m_cmd = cmd;
	req.m_sock = sock;
	req.m_raw_protocol = raw_protocol;
	req.m_resume_response = resume_response;
	req.m_errstack = errstack;
	req.m_subcmd = 0; // no sub-command
	req.m_callback_fn = callback_fn;
	req.m_misc_data = misc_data;
	// This is the nonblocking version of startCommand().
	req.m_nonblocking = true;
	req.m_cmd_description = cmd_description;
	req.m_sec_session_id = sec_session_id ? sec_session_id : m_sec_session_id.c_str();
	req.m_owner = m_owner;
	req.m_methods = m_methods;

	return startCommand_internal(req, timeout, &_sec_man);
}

bool
Daemon::startCommand( int cmd, Sock* sock, int timeout, CondorError *errstack, char const *cmd_description,bool raw_protocol, char const *sec_session_id, bool resume_response )
{
	SecMan::StartCommandRequest req;
	req.m_cmd = cmd;
	req.m_sock = sock;
	req.m_raw_protocol = raw_protocol;
	req.m_resume_response = resume_response;
	req.m_errstack = errstack;
	req.m_subcmd = 0; // no sub-command
	req.m_callback_fn = nullptr;
	req.m_misc_data = nullptr;
	// This is the blocking version of startCommand().
	req.m_nonblocking = false;
	req.m_cmd_description = cmd_description;
	req.m_sec_session_id = sec_session_id ? sec_session_id : m_sec_session_id.c_str();
	req.m_owner = m_owner;
	req.m_methods = m_methods;

	StartCommandResult rc = startCommand_internal(req, timeout, &_sec_man);
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
	EXCEPT("startCommand(nonblocking=false) returned an unexpected result: %d",rc);
	return false;
}

bool
Daemon::sendCommand( int cmd, Sock* sock, int sec, CondorError* errstack, char const *cmd_description )
{
	
	if( ! startCommand( cmd, sock, sec, errstack, cmd_description )) {
		return false;
	}
	if( ! sock->end_of_message() ) {
		std::string err_buf;
		formatstr( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( CA_COMMUNICATION_ERROR, err_buf.c_str() );
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
	if( ! tmp->end_of_message() ) {
		std::string err_buf;
		formatstr( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( CA_COMMUNICATION_ERROR, err_buf.c_str() );
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
	
	SetMyTypeName( *req, COMMAND_ADTYPE );
	req->Assign(ATTR_TARGET_TYPE, REPLY_ADTYPE );

	if( timeout >= 0 ) {
		cmd_sock->timeout( timeout );
	}

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "Daemon::sendCACmd(%s,...) making connection to %s\n",
		         getCommandStringSafe(CA_CMD), _addr.c_str());
	}

	if( ! connectSock(cmd_sock) ) {
		std::string err_msg = "Failed to connect to ";
		err_msg += daemonString(_type);
		err_msg += " ";
		err_msg += _addr;
		newError( CA_CONNECT_FAILED, err_msg.c_str() );
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
		std::string err_msg = "Failed to send command (";
		if( cmd == CA_CMD ) {
			err_msg += "CA_CMD";
		} else {
			err_msg += "CA_AUTH_CMD";
		}
		err_msg += "): ";
		err_msg += errstack.getFullText();
		newError( CA_COMMUNICATION_ERROR, err_msg.c_str() );
		return false;
	}
	if( force_auth ) {
		CondorError e;
		if( ! forceAuthentication(cmd_sock, &e) ) {
			newError( CA_NOT_AUTHENTICATED, e.getFullText().c_str() );
			return false;
		}
	}

		// due to an EVIL bug in authenticate(), our timeout just got
		// set to 20.  so, if we were given a timeout, we have to set
		// it again... :(
	if( timeout >= 0 ) {
		cmd_sock->timeout( timeout );
	}

	if( ! putClassAd(cmd_sock, *req) ) {
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
	if( ! getClassAd(cmd_sock, *reply) ) {
		newError( CA_COMMUNICATION_ERROR, "Failed to read reply ClassAd" );
		return false;
	}
	if( !cmd_sock->end_of_message() ) {
		newError( CA_COMMUNICATION_ERROR, "Failed to read end-of-message" );
		return false;
	}

		// Finally, interpret the results
	std::string result_str;
	if( ! reply->LookupString(ATTR_RESULT, result_str) ) {
		std::string err_msg = "Reply ClassAd does not have ";
		err_msg += ATTR_RESULT;
		err_msg += " attribute";
		newError( CA_INVALID_REPLY, err_msg.c_str() );
		return false;
	}
	CAResult result = getCAResultNum(result_str.c_str());
	if( result == CA_SUCCESS ) { 
			// we recognized it and it's good, just return.
		return true;		
	}

		// Either we don't recognize the result, or it's some known
		// failure.  Either way, look for the error string if there is
		// one, and set it. 
	std::string err;
	if( ! reply->LookupString(ATTR_ERROR_STRING, err) ) {
		if( ! result ) {
				// we didn't recognize the result, so don't assume
				// it's a failure, just let the caller interpret the
				// reply ClassAd if they know how...
			return true;
		}
			// otherwise, it's a known failure, but there's no error
			// string to help us...
		std::string err_msg = "Reply ClassAd returned '";
		err_msg += result_str;
		err_msg += "' but does not have the ";
		err_msg += ATTR_ERROR_STRING;
		err_msg += " attribute";
		newError( result, err_msg.c_str() );
		return false;
	}
	if( result ) {
			// We recognized the error result code, so use that. 
		newError( result, err.c_str() );
	} else {
			// The only way this is possible is if the reply is using
			// codes in the CAResult enum that we don't yet recognize.
			// From our perspective, it's an invalid reply, something
			// we're not prepared to handle.  The caller can further
			// interpret the reply classad if they know how...
		newError( CA_INVALID_REPLY, err.c_str() );
	}			  
	return false;
}


//////////////////////////////////////////////////////////////////////
// Locate-related methods
//////////////////////////////////////////////////////////////////////

bool
DaemonAllowLocateFull::locate( Daemon::LocateType method )
{
	return Daemon::locate( method );
}

Daemon::Daemon( const char * subsystem, daemon_t daemon_type ) {
    common_init();
    _type = daemon_type;

    _is_local = true;
    setSubsystem( subsystem );
    readAddressFile( subsystem );
}

bool
Daemon::locate( Daemon::LocateType method )
{
	bool rval=false;

		// Make sure we only call locate() once.
	if( _tried_locate ) {
			// If we've already been here, return whether we found
			// addr or not, the best judge for if locate() worked.
		if( ! _addr.empty() ) {
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
		rval = getDaemonInfo( GENERIC_AD, true, method );
		break;
	case DT_CLUSTER:
		setSubsystem( "CLUSTER" );
		rval = getDaemonInfo( CLUSTER_AD, true, method );
		break;
	case DT_SCHEDD:
		setSubsystem( "SCHEDD" );
		rval = getDaemonInfo( SCHEDD_AD, true, method );
		break;
	case DT_STARTD:
		setSubsystem( "STARTD" );
		rval = getDaemonInfo( STARTD_AD, true, method );
		break;
	case DT_MASTER:
		setSubsystem( "MASTER" );
		rval = getDaemonInfo( MASTER_AD, true, method );
		break;
	case DT_COLLECTOR:
		do {
			rval = getCmInfo( "COLLECTOR" );
		} while (rval == false && nextValidCm() == true);
		break;
	case DT_NEGOTIATOR:
	  	setSubsystem( "NEGOTIATOR" );
		rval = getDaemonInfo ( NEGOTIATOR_AD, true, method );
		break;
	case DT_CREDD:
	  setSubsystem( "CREDD" );
	  rval = getDaemonInfo( CREDD_AD, true, method );
	  break;
	case DT_VIEW_COLLECTOR:
		if( (rval = getCmInfo("CONDOR_VIEW")) ) {
				// If we found it, we're done.
			break;
		} 
			// If there's nothing CONDOR_VIEW-specific, try just using
			// "COLLECTOR".
		do {
			rval = getCmInfo( "COLLECTOR" );
		} while (rval == false && nextValidCm() == true);
		break;
	case DT_TRANSFERD:
		setSubsystem( "TRANSFERD" );
		rval = getDaemonInfo( ANY_AD, true, method );
		break;
	case DT_HAD:
		setSubsystem( "HAD" );
		rval = getDaemonInfo( HAD_AD, true, method );
		break;
	case DT_KBDD:
		setSubsystem( "KBDD" );
		rval = getDaemonInfo( NO_AD, true, method );
		break;
	default:
		EXCEPT( "Unknown daemon type (%d) in Daemon::locate", (int)_type );
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

	if( _port <= 0 && ! _addr.empty() ) {
			// If we have the sinful string and no port, fill it in
		_port = string_to_port( _addr.c_str() );
		dprintf( D_HOSTNAME, "Using port %d based on address \"%s\"\n",
				 _port, _addr.c_str() );
	}

		// Now that we're done with the get*Info() code, if we're a
		// local daemon and we still don't have a name, fill it in.  
	if( _name.empty() && _is_local) {
		char* local_name = localName();
		_name = local_name;
		free(local_name);
	}

	return true;
}


bool
Daemon::setSubsystem( const char* subsys )
{
	_subsys = subsys ? subsys : "";

	return true;
}


bool
Daemon::getDaemonInfo( AdTypes adtype, bool query_collector, LocateType method )
{
	std::string			buf;
	char				*tmp, *my_name;
	char				*host = NULL;
	bool				nameHasPort = false;

	if ( _subsys.empty() ) {
		dprintf( D_ALWAYS, "Unable to get daemon information because no subsystem specified\n");
		return false;
	}

	if( ! _addr.empty() && is_valid_sinful(_addr.c_str()) ) {
		dprintf( D_HOSTNAME, "Already have address, no info to locate\n" );
		_is_local = false;
		return true;
	}

		// If we were not passed a name or an addr, check the
		// config file for a subsystem_HOST, e.g. SCHEDD_HOST=XXXX
	if( _name.empty()  && _pool.empty() ) {
		formatstr( buf, "%s_HOST", _subsys.c_str() );
		if (param(_name, buf.c_str())) {
				// Found an entry.  Use this name.
			dprintf( D_HOSTNAME, 
					 "No name given, but %s defined to \"%s\"\n",
					 buf.c_str(), _name.c_str() );
		}
	}
	if( ! _name.empty() ) {
		// See if daemon name containts a port specification
		_port = getPortFromAddr( _name.c_str() );
		if ( _port >= 0 ) {
			host = getHostFromAddr( _name.c_str() );
			if ( host ) {
				nameHasPort = true;
			} else {
				dprintf(D_ALWAYS, "warning: unable to parse hostname from '%s'"
				        " but will attempt to use this daemon name anyhow\n",
				        _name.c_str());
			}
		}
	}

		// _name was explicitly specified as host:port, so this information can
		// be used directly.  Further name resolution is not necessary.
	if( nameHasPort ) {
		condor_sockaddr hostaddr;

		dprintf( D_HOSTNAME, "Port %d specified in name\n", _port );

		if(host && hostaddr.from_ip_string(host) ) {
			Set_addr(generate_sinful(host, _port));
			dprintf( D_HOSTNAME,
					"Host info \"%s\" is an IP address\n", host );
		} else {
				// We were given a hostname, not an address.
			std::string fqdn;
			dprintf( D_HOSTNAME, "Host info \"%s\" is a hostname, "
					 "finding IP address\n", host );
			if (!get_fqdn_and_ip_from_hostname(host, fqdn, hostaddr)) {
				// With a hostname, this is a fatal Daemon error.
				formatstr( buf, "unknown host %s", host );
				newError( CA_LOCATE_FAILED, buf.c_str() );
				free( host );

					// We assume this is a transient DNS failure.  Therefore,
					// set _tried_locate = false, so that we keep trying in
					// future calls to locate().
				_tried_locate = false;

				return false;
			}
			buf = generate_sinful(hostaddr.to_ip_string().c_str(), _port);
			dprintf( D_HOSTNAME, "Found IP address and port %s\n", buf.c_str() );
			if (fqdn.length() > 0)
				_full_hostname = fqdn;
			_alias = host;
			Set_addr(buf);
		}

		free( host );
		_is_local = false;
		return true;

		// Figure out if we want to find a local daemon or not, and
		// fill in the various hostname fields.
	} else if( ! _name.empty() ) {
			// We were passed a name, so try to look it up in DNS to
			// get the full hostname.

		tmp = get_daemon_name( _name.c_str() );
		if( ! tmp ) {
				// we failed to contruct the daemon name.  the only
				// possible reason for this is being given faulty
				// hostname.  This is a fatal error.
			std::string err_msg = "unknown host ";
			err_msg += get_host_part( _name.c_str() );
			newError( CA_LOCATE_FAILED, err_msg.c_str() );
			return false;
		}
			// if it worked, we've now got the proper values for the
			// name (and the full hostname, since that's just the
			// "host part" of the "name"...
		_alias = get_host_part(_name.c_str());
		_name = tmp;
		dprintf( D_HOSTNAME, "Using \"%s\" for name in Daemon object\n",
				 tmp );
		free(tmp);
			// now, grab the fullhost from the name we just made...
		_full_hostname = get_host_part(_name.c_str());
		dprintf( D_HOSTNAME,
		         "Using \"%s\" for full hostname in Daemon object\n",
		         _full_hostname.c_str() );

			// Now that we got this far and have the correct name, see
			// if that matches the name for the local daemon.  
			// If we were given a pool, never assume we're local --
			// always try to query that pool...
		if( ! _pool.empty() ) {
			dprintf( D_HOSTNAME, "Pool was specified, "
					 "forcing collector query\n" );
		} else {
			my_name = localName();
			dprintf( D_HOSTNAME, "Local daemon name would be \"%s\"\n", 
					 my_name );
			if( !strcmp(_name.c_str(), my_name) ) {
				dprintf( D_HOSTNAME, "Name \"%s\" matches local name and "
						 "no pool given, treating as a local daemon\n",
						 _name.c_str() );
				_is_local = true;
			}
			free(my_name);
		}
	} else if ( _type != DT_NEGOTIATOR ) {
			// We were passed neither a name nor an address, so use
			// the local daemon, unless we're NEGOTIATOR, in which case
			// we'll still query the collector even if we don't have the 
            // name
		_is_local = true;
		tmp = localName();
		_name = tmp;
		free(tmp);
		_full_hostname = get_local_fqdn();
		dprintf( D_HOSTNAME, "Neither name nor addr specified, using local "
				 "values - name: \"%s\", full host: \"%s\"\n", 
				 _name.c_str(), _full_hostname.c_str() );
	}

		// Now that we have the real, full names, actually find the
		// address of the daemon in question.

	if( _is_local ) {
		bool foundLocalAd = readLocalClassAd( _subsys.c_str() );
		// need to read the address file if we failed to
		// find a local ad, or if we desire to use the super port
		// (because the super port info is not included in the local ad)
		if(!foundLocalAd || useSuperPort()) {
			readAddressFile( _subsys.c_str() );
		}
	}

	if ((_addr.empty()) && (!query_collector)) {
	  return false;
	}

	if( _addr.empty() ) {
			// If we still don't have it (or it wasn't local), query
			// the collector for the address.
		CondorQuery			query(adtype);
		ClassAd*			scan;
		ClassAdList			ads;

		if( (_type == DT_STARTD && ! strchr(_name.c_str(), '@')) ||
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
			formatstr( buf, "%s == \"%s\"", ATTR_MACHINE, _full_hostname.c_str() ); 
			query.addANDConstraint( buf.c_str() );
		} else if ( ! _name.empty() ) {
			if ( _type == DT_GENERIC ) {
				query.setGenericQueryType(_subsys.c_str());
			}

			formatstr( buf, "%s == \"%s\"", ATTR_NAME, _name.c_str() );
			query.addANDConstraint( buf.c_str() );
			if (method == LOCATE_FOR_LOOKUP)
			{
				query.setLocationLookup(_name);
			}
		} else {
			if ( _type != DT_NEGOTIATOR ) {
					// If we're not querying for negotiator
					//    (which there's only one of)
					// and we don't have the name
					// then how will we possibly know which 
					// result to pick??
				return false;
			}
		}

		if (method == LOCATE_FOR_ADMIN) {
			query.addExtraAttribute(ATTR_SEND_PRIVATE_ATTRIBUTES, "true");
		}

			// We need to query the collector
		CollectorList * collectors = CollectorList::create(_pool.c_str());
		CondorError errstack;
		if (collectors->query (query, ads) != Q_OK) {
			delete collectors;
			newError( CA_LOCATE_FAILED, errstack.getFullText().c_str() );
			return false;
		};
		delete collectors;

		ads.Open();
		scan = ads.Next();
		if(!scan) {
			dprintf( D_ALWAYS, "Can't find address for %s %s\n",
			         daemonString(_type), _name.c_str() );
			formatstr( buf, "Can't find address for %s %s", 
			           daemonString(_type), _name.c_str() );
			newError( CA_LOCATE_FAILED, buf.c_str() );
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
		initStringFromAd( scan, ATTR_VERSION, _version );
		initStringFromAd( scan, ATTR_PLATFORM, _platform );
	}

		// Now that we have the sinful string, fill in the port. 
	_port = string_to_port( _addr.c_str() );
	dprintf( D_HOSTNAME, "Using port %d based on address \"%s\"\n",
			 _port, _addr.c_str() );
	return true;
}


bool
Daemon::getCmInfo( const char* subsys )
{
	std::string buf;
	char* host = NULL;

	setSubsystem( subsys );

	if( ! _addr.empty() && is_valid_sinful(_addr.c_str()) ) {
			// only consider addresses w/ a non-zero port "valid"...
		_port = string_to_port( _addr.c_str() );
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
	if( ! _name.empty() && _pool.empty() ) {
		_pool = _name;
	} else if ( _name.empty() && ! _pool.empty() ) {
		_name = _pool;
	} else if ( ! _name.empty() && ! _pool.empty() ) {
		if( _name != _pool ) {
				// They're different, this is bad.
			EXCEPT( "Daemon: pool (%s) and name (%s) conflict for %s",
					_pool.c_str(), _name.c_str(), subsys );
		}
	}

		// Figure out what name we're really going to use.
	if( ! _name.empty() ) {
			// If we were given a name, use that.
		host = strdup( _name.c_str() );
		_is_local = false;
	}


	if( ! host  || !host[0] ) {
		free( host );
		host = NULL;

			// this is just a fancy wrapper for param()...
		char *hostnames = getCmHostFromConfig( subsys );
		if(!hostnames) {
			formatstr( buf, "%s address or hostname not specified in config file",
					 subsys ); 
			newError( CA_LOCATE_FAILED, buf.c_str() );
			_is_configured = false;
			return false;
		}

		collector_list = split(hostnames);
		collector_list_it = collector_list.begin();
		if (!collector_list.empty()) {
			host = strdup(collector_list_it->c_str());
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
			_name = get_local_fqdn();
			_full_hostname = get_local_fqdn();
			free( host );
			return true;
		}
	}

	if( ! host || !host[0]) {
		formatstr( buf, "%s address or hostname not specified in config file",
				 subsys ); 
		newError( CA_LOCATE_FAILED, buf.c_str() );
		_is_configured = false;
		if( host ) free( host );

		return false;
	} 

	bool ret = findCmDaemon( host );
	free( host );
	return ret;
}


bool
Daemon::findCmDaemon( const char* cm_name )
{
	char* host = NULL;
	std::string buf;
	condor_sockaddr saddr;

	dprintf( D_HOSTNAME, "Using name \"%s\" to find daemon\n", cm_name );

	Sinful sinful( cm_name );

	if( !sinful.valid() || !sinful.getHost() ) {
		dprintf( D_ALWAYS, "Invalid address: %s\n", cm_name );
		formatstr( buf, "%s address or hostname not specified in config file",
				 _subsys.c_str() );
		newError( CA_LOCATE_FAILED, buf.c_str() );
		_is_configured = false;
		return false;
	}

		// See if it's already got a port specified in it, or if we
		// should use the default port for this kind of daemon.
	_port = sinful.getPortNum();
	if( _port < 0 ) {
		_port = getDefaultPort();
		sinful.setPort( _port );
		dprintf( D_HOSTNAME, "Port not specified, using default (%d)\n",
				 _port ); 
	} else {
		dprintf( D_HOSTNAME, "Port %d specified in name\n", _port );
	}
	if( _port == 0 && readAddressFile(_subsys.c_str()) ) {
		dprintf( D_HOSTNAME, "Port 0 specified in name, "
				 "IP/port found in address file\n" );
		_name = get_local_fqdn();
		_full_hostname = get_local_fqdn();
		return true;
	}

		// If we're here, we've got a real port and there's no address
		// file, so we should store the string we used (as is) in
		// _name, so that we can get to it later if we need it.
	if( _name.empty() ) {
		_name = cm_name;
	}

		// Now that we've got the port, grab the hostname for the rest
		// of the logic.  first, stash the copy of the hostname with
		// our handy helper method, then free() the full version
		// (which we've already got stashed in _name if we need it),
		// and finally reset host to point to the host for the rest of
		// this function.
	if( sinful.getHost() ) {
		host = strdup( sinful.getHost() );
	}


	if ( !host ) {
		formatstr( buf, "%s address or hostname not specified in config file",
				 _subsys.c_str() );
		newError( CA_LOCATE_FAILED, buf.c_str() );
		_is_configured = false;
		return false;
	}


	if( saddr.from_ip_string(host) ) {
		Set_addr( sinful.getSinful() ? sinful.getSinful() : "" );
		dprintf( D_HOSTNAME, "Host info \"%s\" is an IP address\n", host );
	} else {
			// We were given a hostname, not an address.
		dprintf( D_HOSTNAME, "Host info \"%s\" is a hostname, "
				 "finding IP address\n", host );

		std::string fqdn;
		int ret = get_fqdn_and_ip_from_hostname(host, fqdn, saddr);
		if (!ret) {
				// With a hostname, this is a fatal Daemon error.
			formatstr( buf, "unknown host %s", host );
			newError( CA_LOCATE_FAILED, buf.c_str() );
			free( host );

				// We assume this is a transient DNS failure.  Therefore,
				// set _tried_locate = false, so that we keep trying in
				// future calls to locate().
			_tried_locate = false;

			return false;
		}
		sinful.setHost(saddr.to_ip_string().c_str());
		// Older versions resolved a CNAME to the final A record FQDN and
		// set that as the alias for the collector sinful here.
		// This runs counter to how SSL host certificate validation is
		// expected to work, and our setting of the alias for this Daemon
		// object.
		if(param_boolean("USE_COLLECTOR_HOST_CNAME", true)) {
			sinful.setAlias(host);
		} else {
			sinful.setAlias(fqdn.c_str());
		}
		dprintf( D_HOSTNAME, "Found CM IP address and port %s\n",
				 sinful.getSinful() ? sinful.getSinful() : "NULL" );
		_full_hostname = fqdn;
		if( host ) {
			_alias = host;
		}
		Set_addr( sinful.getSinful() );
	}

		// If the pool was set, we want to use _name for that, too. 
	if( ! _pool.empty() ) {
		_pool = _name;
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
	if( ! _hostname.empty() && ! _full_hostname.empty() ) {
		return true;
	}

		// if we haven't tried to locate yet, we should do that now,
		// since that's usually the best way to get the hostnames, and
		// we get everything else we need, while we're at it...
	if( ! _tried_locate ) {
		locate();
	}

		// check again if we already have the info
	if( ! _full_hostname.empty() ) {
		if( _hostname.empty() ) {
			return initHostnameFromFull();
		}
		return true;
	}

	if( _addr.empty() ) {
			// this is bad...
		return false;
	}

			// We have no name, but we have an address.  Try to do an
			// inverse lookup and fill in the hostname info from the
			// IP address we already have.

	dprintf( D_HOSTNAME, "Address \"%s\" specified but no name, "
			 "looking up host info\n", _addr.c_str() );

	condor_sockaddr saddr;
	saddr.from_sinful(_addr);
	std::string fqdn = get_full_hostname(saddr);
	if (fqdn.empty()) {
		_hostname.clear();
		_full_hostname.clear();
		dprintf(D_HOSTNAME, "get_full_hostname() failed for address %s\n",
				saddr.to_ip_string().c_str());
		std::string err_msg = "can't find host info for ";
		err_msg += _addr;
		newError( CA_LOCATE_FAILED, err_msg.c_str() );
		return false;
	}

	_full_hostname = fqdn;
	initHostnameFromFull();
	return true;
}


bool
Daemon::initHostnameFromFull( void )
{
		// many of the code paths that find the hostname info just
		// fill in _full_hostname, but not _hostname.  In all cases,
		// if we have _full_hostname we just want to trim off the
		// domain the same way for _hostname.
	if( ! _full_hostname.empty() ) {
		_hostname = _full_hostname;
		size_t dot = _hostname.find('.');
		if (dot != std::string::npos) {
			_hostname.erase(dot);
		}
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
	if( ! _version.empty() && ! _platform.empty() ) {
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
	if( _version.empty() && _is_local ) {
		dprintf( D_HOSTNAME, "No version string in local address file, "
				 "trying to find it in the daemon's binary\n" );
		char* exe_file = param( _subsys.c_str() );
		if( exe_file ) {
			char ver[128];
			CondorVersionInfo vi;
			vi.get_version_from_file(exe_file, ver, 128);
			_version = ver;
			dprintf( D_HOSTNAME, "Found version string \"%s\" "
					 "in local binary (%s)\n", ver, exe_file );
			free( exe_file );
			return true;
		} else {
			dprintf( D_HOSTNAME, "%s not defined in config file, "
					 "can't locate daemon binary for version info\n", 
					 _subsys.c_str() );
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
	{
		int port = param_integer("COLLECTOR_PORT", COLLECTOR_PORT);
		return port;
		break;
	}
	case DT_VIEW_COLLECTOR:
		return param_integer("COLLECTOR_PORT", COLLECTOR_PORT);
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
	_error = str ? str : "";
	_error_code = err_code;
}


char*
Daemon::localName( void )
{
	char buf[100], *tmp, *my_name;
	snprintf( buf, sizeof(buf), "%s_NAME", daemonString(_type) );
	tmp = param( buf );
	if( tmp ) {
		my_name = build_valid_daemon_name( tmp );
		free( tmp );
	} else {
		my_name = strdup( get_local_fqdn().c_str() );
	}
	return my_name;
}

bool
Daemon::useSuperPort()
{
	// If this is a client tool, and the invoking user is root
	// or config knob USE_SUPER_PORT=True, try to use the
	// SUPER_ADDRESS_FILE

	return  get_mySubSystem()->isClient() &&
		    (is_root() || param_boolean("USE_SUPER_PORT",false));
}

bool
Daemon::readAddressFile( const char* subsys )
{
	char* addr_file = NULL;
	FILE* addr_fp;
	std::string param_name;
	std::string buf;
	bool rval = false;
	bool use_superuser = false;

	if ( useSuperPort() )
	{
		formatstr( param_name, "%s_SUPER_ADDRESS_FILE", subsys );
		use_superuser = true;
		addr_file = param( param_name.c_str() );
	}
	if ( ! addr_file ) {
		formatstr( param_name, "%s_ADDRESS_FILE", subsys );
		use_superuser = false;
		addr_file = param( param_name.c_str() );
		if( ! addr_file ) {
			return false;
		}
	}

	dprintf( D_HOSTNAME, "Finding %s address for local daemon, "
			 "%s is \"%s\"\n", use_superuser ? "superuser" : "local",
			 param_name.c_str(), addr_file );

	if( ! (addr_fp = safe_fopen_wrapper_follow(addr_file, "r")) ) {
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
	if( ! readLine(buf, addr_fp) ) {
		dprintf( D_HOSTNAME, "address file contained no data\n" );
		fclose( addr_fp );
		return false;
	}
	chomp(buf);
	if( is_valid_sinful(buf.c_str()) ) {
		dprintf( D_HOSTNAME, "Found valid address \"%s\" in "
				 "%s address file\n", buf.c_str(), use_superuser ? "superuser" : "local" );
		Set_addr(buf);
		rval = true;
	}

		// Let's see if this is new enough to also have a
		// version string and platform string...
	if( readLine(buf, addr_fp) ) {
			// chop off the newline
		chomp(buf);
		_version = buf;
		dprintf( D_HOSTNAME,
				 "Found version string \"%s\" in address file\n",
				 buf.c_str() );
		if( readLine(buf, addr_fp) ) {
			chomp(buf);
			_platform = buf;
			dprintf( D_HOSTNAME,
					 "Found platform string \"%s\" in address file\n",
					 buf.c_str() );
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
	std::string param_name;

	formatstr( param_name, "%s_DAEMON_AD_FILE", subsys );
	addr_file = param( param_name.c_str() );
	if( ! addr_file ) {
		return false;
	}

	dprintf( D_HOSTNAME, "Finding classad for local daemon, "
			 "%s is \"%s\"\n", param_name.c_str(), addr_file );

	if( ! (addr_fp = safe_fopen_wrapper_follow(addr_file, "r")) ) {
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
	adFromFile = new ClassAd;
	InsertFromFile(addr_fp, *adFromFile, "...", adIsEOF, errorReadingAd, adEmpty);
	ASSERT(adFromFile);
	if(!m_daemon_ad_ptr) {
		m_daemon_ad_ptr = new ClassAd(*adFromFile);
	}
	std::unique_ptr<ClassAd> smart_ad_ptr(adFromFile);
	
	fclose(addr_fp);

	if(errorReadingAd) {
		return false;	// did that just leak adFromFile?
	}

	return getInfoFromAd( smart_ad_ptr.get() );
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
	std::string buf = "";
	std::string buf2 = "";
	std::string addr_attr_name = "";
		// TODO Which attributes should trigger a failure if we don't find
		// them in the ad? Just _addr?
	bool ret_val = true;
	bool found_addr = false;

		// We look for _name first because we use it, if available, for
		// error messages if we fail  to find the other attributes.
	initStringFromAd( ad, ATTR_NAME, _name );

		// construct the IP_ADDR attribute
	formatstr( buf, "%sIpAddr", _subsys.c_str() );
	if ( ad->LookupString( buf, buf2 ) ) {
		Set_addr(buf2);
		found_addr = true;
		addr_attr_name = buf;
	}
	else if ( ad->LookupString( ATTR_MY_ADDRESS, buf2 ) ) {
		Set_addr(buf2);
		found_addr = true;
		addr_attr_name = ATTR_MY_ADDRESS;
	}

	if ( found_addr ) {
		dprintf( D_HOSTNAME, "Found %s in ClassAd, using \"%s\"\n",
				 addr_attr_name.c_str(), _addr.c_str());
		_tried_locate = true;
	} else {
		dprintf( D_ALWAYS, "Can't find address in classad for %s %s\n",
		         daemonString(_type), _name.c_str() );
		formatstr( buf, "Can't find address in classad for %s %s",
		           daemonString(_type), _name.c_str() );
		newError( CA_LOCATE_FAILED, buf.c_str() );

		ret_val = false;
	}

	if( initStringFromAd( ad, ATTR_VERSION, _version ) ) {
		_tried_init_version = true;
	} else {
		ret_val = false;
	}

	initStringFromAd( ad, ATTR_PLATFORM, _platform );

	std::string capability;
	if (ad->EvaluateAttrString(ATTR_REMOTE_ADMIN_CAPABILITY, capability)) {
		ClaimIdParser cidp(capability.c_str());
		dprintf(D_FULLDEBUG, "Creating a new administrative session for capability %s\n", cidp.publicClaimId());
		_sec_man.CreateNonNegotiatedSecuritySession(
			CLIENT_PERM,
			cidp.secSessionId(),
			cidp.secSessionKey(),
			cidp.secSessionInfo(),
			COLLECTOR_SIDE_MATCHSESSION_FQU,
			AUTH_METHOD_MATCH,
			addr(),
			1800,
			nullptr,
			true
		);
	}

	if( initStringFromAd( ad, ATTR_MACHINE, _full_hostname ) ) {
		initHostnameFromFull();
		_tried_init_hostname = false;
	} else {
		ret_val = false;
	}

	return ret_val;
}


bool
Daemon::initStringFromAd(const ClassAd* ad, const char* attrname, std::string& value)
{
	if( ! ad->LookupString(attrname, value) ) {
		std::string buf;
		dprintf( D_ALWAYS, "Can't find %s in classad for %s %s\n",
				 attrname, daemonString(_type),
				 _name.c_str() );
		formatstr( buf, "Can't find %s in classad for %s %s",
					 attrname, daemonString(_type),
				   _name.c_str() );
		newError( CA_LOCATE_FAILED, buf.c_str() );
		return false;
	}
	dprintf(D_HOSTNAME, "Found %s in ClassAd, using \"%s\"\n",
	        attrname, value.c_str());
	return true;
}

void
Daemon::Set_addr( const std::string& str )
{
	_addr = str;

	if( ! _addr.empty() ) {
		Sinful sinful(_addr.c_str());

		// Extract the alias first. This ensures that if we
		// rewrite the sinful, we'll re-insert the alias
		// afterwards. This is important for SSL authentication,
		// where the alias is used to verify the daemon's host
		// certificate.
		const char* sinful_alias = sinful.getAlias();
		if (sinful_alias) {
			_alias = sinful_alias;
		}

		char const *priv_net = sinful.getPrivateNetworkName();
		if( priv_net ) {
			bool using_private = false;
			char *our_network_name = param( "PRIVATE_NETWORK_NAME" );
			if( our_network_name ) {
				if( strcmp( our_network_name, priv_net ) == 0 ) {
					char const *priv_addr = sinful.getPrivateAddr();
					dprintf( D_HOSTNAME, "Private network name matched.\n");
					using_private = true;
					if( priv_addr ) {
						// replace address with private address
						std::string buf;
						if( *priv_addr != '<' ) {
							formatstr(buf,"<%s>",priv_addr);
							priv_addr = buf.c_str();
						}
						_addr = priv_addr;
						sinful = Sinful(_addr.c_str());
					}
					else {
						// no private address was specified, so use public
						// address with CCB disabled
						sinful.setCCBContact(NULL);
						_addr = sinful.getSinful();
					}
				}
				free( our_network_name );
			}
			if( !using_private ) {
				dprintf( D_HOSTNAME, "Private network name not matched.\n");
			}
		}

		if( sinful.getCCBContact() ) {
			// CCB cannot handle UDP, so pretend this daemon has no
			// UDP port.
			m_has_udp_command_port = false;
		}
		if( sinful.getSharedPortID() ) {
			// SharedPort does not handle UDP
			m_has_udp_command_port = false;
		}
		if( sinful.noUDP() ) {
			// This address explicitly specifies that UDP is not supported
			m_has_udp_command_port = false;
		}
		if( !sinful.getAlias() && ! _alias.empty() ) {
			sinful.setAlias(_alias.c_str());
			_addr = sinful.getSinful();
		}
	}

	if( ! _addr.empty() ) {
		dprintf( D_HOSTNAME, "Daemon client (%s) address determined: "
				 "name: \"%s\", pool: \"%s\", alias: \"%s\", addr: \"%s\"\n",
				 daemonString(_type),
				 _name.c_str(),
				 _pool.c_str(),
				 _alias.c_str(),
				 _addr.c_str() );
	}
	return;
}

bool
Daemon::checkAddr( void )
{
	bool just_tried_locate = false;
	if( _addr.empty() ) {
		locate();
		just_tried_locate = true;
	}
	if( _addr.empty() ) {
			// _error will already be set appropriately
		return false;
	}
	if( _port == 0 && Sinful(_addr.c_str()).getSharedPortID()) {
			// This is an address with a shared port id but no
			// SharedPortServer address, so it is only good for
			// local connections on the same machine.
		return true;
	}
	else if( _port == 0 ) {
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
		_addr.clear();
		if( _is_local ) {
			_name.clear();
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
	_cmd_str = cmd ? cmd : "";
}


char*
getCmHostFromConfig( const char * subsys )
{ 
	std::string buf;
	char* host = NULL;

		// Try the config file for a subsys-specific hostname 
	formatstr( buf, "%s_HOST", subsys );
	host = param( buf.c_str() );
	if( host ) {
		if( host[0] ) {
			dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.c_str(), 
					 host ); 
			if(host[0] == ':') {
				dprintf( D_ALWAYS, "Warning: Configuration file sets '%s=%s'.  This does not look like a valid host name with optional port.\n", buf.c_str(), host);
			}
			return host;
		} else {
			free( host );
		}
	}

		// Try the config file for a subsys-specific IP addr 
	formatstr( buf, "%s_IP_ADDR", subsys );
	host = param( buf.c_str() );
	if( host ) {
		if( host[0] ) {
			dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.c_str(), host );
			return host;
		} else {
			free( host );
		}
	}

		// settings should take precedence over this). 
	host = param( "CM_IP_ADDR" );
	if( host ) {
		if(  host[0] ) {
			dprintf( D_HOSTNAME, "%s is set to \"%s\"\n", buf.c_str(), 
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

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "Daemon::getTimeOffset(%s,...) making connection to %s\n",
			getCommandStringSafe(DC_TIME_OFFSET), _addr.c_str());
	}

		//
		// First establish a socket to the other daemon
		//
	ReliSock reli_sock;
	reli_sock.timeout( 30 ); // I'm following what everbody else does
	if( ! connectSock(&reli_sock) ) {
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffset() failed to connect "
			"to remote daemon at '%s'\n",
			this->_addr.c_str() );
		return ( false );
	}
		//
		// Next send our command to prepare for the call out to the
		// remote daemon
		//
	if( ! this->startCommand( DC_TIME_OFFSET, (Sock*)&reli_sock ) ) { 
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffset() failed to send "
			"command to remote daemon at '%s'\n",
			this->_addr.c_str() );
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

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "Daemon::getTimeOffsetRange(%s,...) making connection to %s\n",
			getCommandStringSafe(DC_TIME_OFFSET), _addr.c_str());
	}
		//
		// First establish a socket to the other daemon
		//
	ReliSock reli_sock;
	reli_sock.timeout( 30 ); // I'm following what everbody else does
	if( ! connectSock(&reli_sock) ) {
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffsetRange() failed to connect "
			"to remote daemon at '%s'\n",
			this->_addr.c_str() );
		return ( false );
	}
		//
		// Next send our command to prepare for the call out to the
		// remote daemon
		//
	if( ! this->startCommand( DC_TIME_OFFSET, (Sock*)&reli_sock ) ) { 
		dprintf( D_FULLDEBUG, "Daemon::getTimeOffsetRange() failed to send "
			"command to remote daemon at '%s'\n",
			this->_addr.c_str() );
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

bool
Daemon::getInstanceID( std::string & instanceID ) {
	// Enter the cargo cult.
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::getInstanceID() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		dprintf( D_FULLDEBUG, "Daemon::getInstanceID() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	if(! startCommand( DC_QUERY_INSTANCE, (Sock *) & rSock, 5 )) {
		dprintf( D_FULLDEBUG, "Daemon::getInstanceID() failed to send "
			"command to remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	if(! rSock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Daemon::getInstanceID() failed to send "
			"end of message to remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	unsigned char instance_id[17];
	const int instance_length = 16;
	rSock.decode();
	if(! rSock.get_bytes( instance_id, instance_length )) {
		dprintf( D_FULLDEBUG, "Daemon::getInstanceID() failed to read "
			"instance ID from remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	if(! rSock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Daemon::getInstanceID() failed to read "
			"end of message from remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	instanceID.assign( (const char *)instance_id, instance_length );
	return true;
}


bool
Daemon::getSessionToken( const std::vector<std::string> &authz_bounding_limit, int lifetime,
	std::string &token, const std::string &key, CondorError *err)
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::getSessionToken() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;

	const std::string authz_limit_str = join(authz_bounding_limit, ",");
	if (!authz_limit_str.empty() && !ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, authz_limit_str)) {
		if (err) err->pushf("DAEMON", 1, "Failed to create token request ClassAd");
		dprintf(D_FULLDEBUG, "Failed to create token request ClassAd\n");
		return false;
	}
	if ((lifetime > 0) && !ad.InsertAttr(ATTR_SEC_TOKEN_LIFETIME, lifetime)) {
		if (err) err->pushf("DAEMON", 1, "Failed to create token request ClassAd");
		dprintf(D_FULLDEBUG, "Failed to create token request ClassAd\n");
		return false;
	}

	if( (!key.empty()) && !ad.InsertAttr(ATTR_SEC_REQUESTED_KEY, key)) {
		if (err) err->pushf("DAEMON", 1, "Failed to create token request ClassAd");
		dprintf(D_FULLDEBUG, "Failed to create token request ClassAd\n");
		return false;
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		if (err) err->pushf("DAEMON", 1, "Failed to connect to remote daemon at '%s'",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::getSessionToken() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	if (!startCommand( DC_GET_SESSION_TOKEN, &rSock, 20, err)) {
		dprintf(D_FULLDEBUG, "Daemon::getSessionToken() failed to start command for "
			"token request with remote daemon at '%s'.\n", _addr.c_str());
		return false;
	}

	if (!putClassAd(&rSock, ad)) {
		if (err) err->pushf("DAEMON", 1, "Failed to send ClassAd to remote daemon at"
			" '%s'", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::getSessionToken() Failed to send ClassAd to remote"
			" daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	if(! rSock.end_of_message()) {
		dprintf(D_FULLDEBUG, "Daemon::getSessionToken() failed to send "
			"end of message to remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	rSock.decode();

	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad)) {
		if (err) err->pushf("DAEMON", 1, "Failed to recieve response from remote daemon at"
			" at '%s'\n", _addr.c_str() );
		dprintf(D_FULLDEBUG, "Daemon::getSessionToken() failed to recieve response from "
			"remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if(!rSock.end_of_message()) {
		dprintf( D_FULLDEBUG, "Daemon::getSessionToken() failed to read "
			"end of message from remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	std::string err_msg;
	if (result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg)) {
		int error_code = 0;
		result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code);
		if (!error_code) error_code = -1;

		if (err) err->push("DAEMON", error_code, err_msg.c_str());
		return false;
	}

	if (!result_ad.EvaluateAttrString(ATTR_SEC_TOKEN, token)) {
		dprintf(D_FULLDEBUG, "BUG!  Daemon::getSessionToken() received a malformed ad, "
			"containing no resulting token and no error message, from remote daemon "
			"at '%s'\n", _addr.c_str());
		if (err) err->pushf("DAEMON", 1, "BUG!  Daemon::getSessionToken() received a "
			"malformed ad containing no resulting token and no error message, from "
			"remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	return true;
}

bool
Daemon::exchangeSciToken(const std::string &scitoken, std::string &token, CondorError &err) noexcept
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::exchangeSciToken() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;
	if (!ad.InsertAttr(ATTR_SEC_TOKEN, scitoken)) {
		err.pushf("DAEMON", 1, "Failed to create SciToken exchange request ClassAd");
		dprintf(D_FULLDEBUG, "Failed to create SciToken exchange request ClassAd\n");
		return false;
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		err.pushf("DAEMON", 1, "Failed to connect to remote daemon at '%s'",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::exchangeSciToken() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand( DC_EXCHANGE_SCITOKEN, &rSock, 20, &err)) {
		err.pushf("DAEMON", 1, "Failed to start command for SciToken exchange "
			"with remote daemon at '%s'.\n", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::exchangeSciToken() failed to start command for "
			"SciToken exchange with remote daemon at '%s'.\n", _addr.c_str());
		return false;
	}

	if (!putClassAd(&rSock, ad)) {
		err.pushf("DAEMON", 1, "Failed to send ClassAd to remote daemon at"
			" '%s'", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::exchangeSciToken() Failed to send ClassAd to remote"
			" daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if(! rSock.end_of_message()) {
		err.pushf("DAEMON", 1, "Failed to send end of message to remote daemon at"
			" '%s'", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::exchangeSciToken() failed to send "
			"end of message to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	rSock.decode();

	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad)) {
		err.pushf("DAEMON", 1, "Failed to recieve response from remote daemon at"
			" at '%s'\n", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::exchangeSciToken() failed to recieve response from "
			"remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if(!rSock.end_of_message()) {
		err.pushf("DAEMON", 1, "Failed to read end of message to remote daemon at"
			" '%s'", _addr.c_str());
		dprintf( D_FULLDEBUG, "Daemon::exchangeSciToken() failed to read "
			"end of message from remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	std::string err_msg;
	if (result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg)) {
		int error_code = 0;
		result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code);
		if (!error_code) error_code = -1;

		err.push("DAEMON", error_code, err_msg.c_str());
		return false;
	}

	if (!result_ad.EvaluateAttrString(ATTR_SEC_TOKEN, token)) {
		dprintf(D_FULLDEBUG, "BUG!  Daemon::exchangeToken() received a malformed ad, "
			"containing no resulting token and no error message, from remote daemon "
			"at '%s'\n", _addr.c_str());
		err.pushf("DAEMON", 1, "BUG!  Daemon::exchangeSciToken() received a "
			"malformed ad containing no resulting token and no error message, from "
			"remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	return true;
}


bool
Daemon::startTokenRequest( const std::string &identity,
	const std::vector<std::string> &authz_bounding_set, int lifetime,
	const std::string &client_id, std::string &token, std::string &request_id,
	CondorError *err ) noexcept
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::startTokenRequest() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;
	const std::string authz_limit_str = join(authz_bounding_set, ",");

	if (!authz_limit_str.empty() &&
		!ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, authz_limit_str))
	{
		if (err) { err->pushf("DAEMON", 1, "Failed to create token request ClassAd"); }
		dprintf(D_FULLDEBUG, "Failed to create token request ClassAd\n");
		return false;
	}
	// Since token lifetime is capped by the server and clients gain no
	// benefit from shorter lifetimes, don't provide a default lifetime
	// for token requests; just accept the server default.  (Presently,
	// only in the condor_token_request implemention is lifetime not -1.)
	if ((lifetime > 0) && !ad.InsertAttr(ATTR_SEC_TOKEN_LIFETIME, lifetime)) {
		if (err) { err->pushf("DAEMON", 1, "Failed to create token request ClassAd"); }
		dprintf(D_FULLDEBUG, "Failed to create token request ClassAd\n");
		return false;
	}
	if (identity.empty()) {
		std::string domain;
		if (!param(domain, "UID_DOMAIN")) {
			if (err) { err->pushf("DAEMON", 1, "No UID_DOMAIN set!"); }
			dprintf(D_FULLDEBUG, "No UID_DOMAIN set!\n");
			return false;
		}
		if (!ad.InsertAttr(ATTR_USER, "condor@" + domain)) {
			if (err) { err->pushf("DAEMON", 1, "Failed to set the default username"); }
			dprintf(D_FULLDEBUG, "Failed to set the default username\n");
			return false;
		}
	} else {
		auto at_sign = identity.find('@');
		if (at_sign == std::string::npos) {
			std::string domain;
			if (!param(domain, "UID_DOMAIN")) {
				if (err) { err->pushf("DAEMON", 1, "No UID_DOMAIN set!"); }
				dprintf(D_FULLDEBUG, "No UID_DOMAIN set!\n");
				return false;
			}
			if (!ad.InsertAttr(ATTR_USER, identity + "@" + domain)) {
				if (err) { err->pushf("DAEMON", 1, "Unable to set requested id."); }
				dprintf(D_FULLDEBUG, "Unable to set requested id.\n");
				return false;
			}
		} else if (!ad.InsertAttr(ATTR_USER, identity)) {
			if (err) { err->pushf("DAEMON", 1, "Unable to set requested identity."); }
			dprintf(D_FULLDEBUG, "Unable to set requested identity.\n");
			return false;
		}
	}
	if (client_id.empty() || !ad.InsertAttr(ATTR_SEC_CLIENT_ID, client_id)) {
		if (err) { err->pushf("DAEMON", 1, "Unable to set client ID."); }
		dprintf(D_FULLDEBUG, "Unable to set client ID.\n");
		return false;
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		if (err) { err->pushf("DAEMON", 1, "Failed to connect "
			"to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::startTokenRequest() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand( DC_START_TOKEN_REQUEST, &rSock, 20, err)) {
		if (err) { err->pushf("DAEMON", 1, "failed to start "
			"command for token request with remote daemon at '%s'.",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::startTokenRequest() failed to start "
			"command for token request with remote daemon at '%s'.\n",
			_addr.c_str());
		return false;
	}
		// Try forcing encryption.  If it's not available, then this request will be
		// queued ONLY IF auto-approval is allowed.
	rSock.set_crypto_mode(true);

	if (!putClassAd(&rSock, ad) || !rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to send "
			"ClassAd to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::startTokenRequest() failed to send "
			"ClassAd to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	rSock.decode();

	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad)) {
		if (err) { err->pushf("DAEMON", 1, "Failed to recieve "
			"response from remote daemon at at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::startTokenRequest() failed to recieve "
			"response from remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if(!rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to read "
			"end-of-message from remote daemon at '%s'",
			_addr.c_str()); }
		dprintf( D_FULLDEBUG, "Daemon::startTokenRequest() failed to read "
			"end of message from remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	std::string err_msg;
	if (result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg)) {
		int error_code = 0;
		result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code);
		if (!error_code) error_code = -1;

		if (err) { err->push("DAEMON", error_code, err_msg.c_str()); }
		return false;
	}

	if (!result_ad.EvaluateAttrString(ATTR_SEC_TOKEN, token) || token.empty()) {
		if (result_ad.EvaluateAttrString(ATTR_SEC_REQUEST_ID, request_id)
			&& !request_id.empty())
		{
			return true;
		}
		if (err) { err->pushf("DAEMON", 1, "BUG!  Daemon::startTokenRequest() "
			"received a malformed ad, containing no resulting token and no "
			"error message, from remote daemon at '%s'",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "BUG!  Daemon::startTokenRequest() "
			"received a malformed ad, containing no resulting token and no "
			"error message, from remote daemon at '%s'\n",
			_addr.c_str());
		return false;
	}

	return true;
}


bool
Daemon::finishTokenRequest(const std::string &client_id, const std::string &request_id,
	std::string &token, CondorError *err ) noexcept
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::finishTokenRequest() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;
	if (client_id.empty() || !ad.InsertAttr(ATTR_SEC_CLIENT_ID, client_id)) {
		if (err) { err->pushf("DAEMON", 1, "Unable to set client ID."); }
		dprintf(D_FULLDEBUG, "Unable to set client ID.\n");
		return false;
	}
	if (request_id.empty() || !ad.InsertAttr(ATTR_SEC_REQUEST_ID, request_id)) {
		if (err) { err->pushf("DAEMON", 1, "Unable to set request ID."); }
		dprintf(D_FULLDEBUG, "Unable to set request ID.\n");
		return false;
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		if (err) { err->pushf("DAEMON", 1, "Failed to connect "
			"to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::finishTokenRequest() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand( DC_FINISH_TOKEN_REQUEST, &rSock, 20, err)) {
		if (err) { err->pushf("DAEMON", 1, "failed to start "
			"command for token request with remote daemon at '%s'.",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::finishTokenRequest() failed to start "
			"command for token request with remote daemon at '%s'.\n",
			_addr.c_str());
		return false;
	}

	if (!putClassAd(&rSock, ad) || !rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to send "
			"ClassAd to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::finishTokenRequest() Failed to send "
			"ClassAd to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	rSock.decode();

	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad)) {
		if (err) { err->pushf("DAEMON", 1, "Failed to recieve "
			"response from remote daemon at '%s'",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::finishTokenRequest() failed to recieve "
			"response from remote daemon at '%s'\n",
			_addr.c_str());
		return false;
	}

	if (!rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to read "
			"end-of-message from remote daemon at '%s'\n",
			_addr.c_str()); }
		dprintf( D_FULLDEBUG, "Daemon::finishTokenRequest() failed to read "
			"end of message from remote daemon at '%s'\n",
			_addr.c_str() );
		return false;
	}

	std::string err_msg;
	if (result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg)) {
		int error_code = 0;
		result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code);
		if (!error_code) error_code = -1;

		if (err) { err->push("DAEMON", error_code, err_msg.c_str()); }
		return false;
	}

	// We are successful regardless of whether the token has any content --
	// an empty token string (without an error) means that the request is
	// still pending on the server.
	if (!result_ad.EvaluateAttrString(ATTR_SEC_TOKEN, token)) {
		if (err) { err->pushf("DAEMON", 1, "BUG!  Daemon::finishTokenRequest() "
			"received a malformed ad containing no resulting token "
			"and no error message, from remote daemon at '%s'",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "BUG!  Daemon::finishTokenRequest() "
			"received a malformed ad, containing no resulting token "
			"and no error message, from remote daemon at '%s'\n",
			_addr.c_str());
		return false;
	}
	return true;
}


bool
Daemon::listTokenRequest(const std::string &request_id, std::vector<classad::ClassAd> &results,
	CondorError *err ) noexcept
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::listTokenRequest() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;
	if (!request_id.empty()) {
		if (!ad.InsertAttr(ATTR_SEC_REQUEST_ID, request_id)) {
			if (err) { err->pushf("DAEMON", 1, "Unable to set request ID."); }
			dprintf(D_FULLDEBUG, "Unable to set request ID.\n");
			return false;
		}
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		if (err) { err->pushf("DAEMON", 1, "Failed to connect "
			"to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::listTokenRequest() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand( DC_LIST_TOKEN_REQUEST, &rSock, 20, err)) {
		if (err) { err->pushf("DAEMON", 1, "Failed to start "
			"command for listing token requests with remote daemon at '%s'.",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::listTokenRequest() failed to start "
			"command for listing token requests with remote daemon at '%s'.\n",
			_addr.c_str());
		return false;
	}

	if (!putClassAd(&rSock, ad) || !rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to send "
			"ClassAd to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::listTokenRequest() Failed to send "
			"ClassAd to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	rSock.decode();

	while (true) {
		classad::ClassAd ad;
		if (!getClassAd(&rSock, ad) || !rSock.end_of_message()) {
			if (err) { err->pushf("DAEMON", 2, "Failed to receive "
				"response ClassAd from remote daemon at '%s'",
				_addr.c_str()); }
			dprintf(D_FULLDEBUG, "Daemon::listTokenRequest() Failed to receive "
				"response ClassAd from remote daemon at '%s'\n",
				_addr.c_str());
			return false;
		}

		// The use of ATTR_OWNER here as an end-sentinel is arbitrary; I used this attribute and
		// special value just to be similar to the condor_q protocol.
		long long intVal;
		if (ad.EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0)) {
			std::string errorMsg;
			if (ad.EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal &&
				ad.EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
			{
				if (err) { err->pushf("DAEMON", intVal, "%s", errorMsg.c_str()); }
				dprintf(D_FULLDEBUG, "Daemon::listTokenRequest() Failed due "
					"to remote error: '%s' (error code %lld)\n",
					errorMsg.c_str(), intVal);
				return false;
			}
			break;
		}

		results.emplace_back();
		results.back().CopyFrom(ad);
		ad.Clear();
	}
	return true;
}


bool
Daemon::approveTokenRequest( const std::string &client_id, const std::string &request_id,
	CondorError *err ) noexcept
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::approveTokenRequest() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;
	if (request_id.empty()) {
		if (err) { err->pushf("DAEMON", 1, "No request ID provided."); }
		dprintf(D_FULLDEBUG,
			"Daemon::approveTokenRequest(): No request ID provided.\n");
		return false;
	} else if (!ad.InsertAttr(ATTR_SEC_REQUEST_ID, request_id)) {
		if (err) { err->pushf("DAEMON", 1, "Unable to set request ID."); }
		dprintf(D_FULLDEBUG,
			"Daemon::approveTokenRequest(): Unable to set request ID.\n");
		return false;
	}
	if (client_id.empty()) {
		if (err) { err->pushf("DAEMON", 1, "No client ID provided."); }
		dprintf(D_FULLDEBUG,
			"Daemon::approveTokenRequest(): No client ID provided.\n");
		return false;
	} else if (!ad.InsertAttr(ATTR_SEC_CLIENT_ID, client_id)) {
		if (err) { err->pushf("DAEMON", 1, "Unable to set client ID."); }
		dprintf(D_FULLDEBUG,
			"Daemon::approveTokenRequest(): Unable to set client ID.\n");
		return false;
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		if (err) { err->pushf("DAEMON", 1, "Failed to connect "
			"to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::approveTokenRequest() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand( DC_APPROVE_TOKEN_REQUEST, &rSock, 20, err)) {
		if (err) { err->pushf("DAEMON", 1,
			"command for approving token requests with remote daemon at '%s'.",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::approveTokenRequest() failed to start command for "
			"approving token requests with remote daemon at '%s'.\n", _addr.c_str());
		return false;
	}

	if (!putClassAd(&rSock, ad) || !rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to send "
			"ClassAd to remote daemon at '%s'", _addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::approveTokenRequest() Failed to send "
			"ClassAd to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	rSock.decode();

	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad)) {
		if (err) { err->pushf("DAEMON", 1, "Failed to recieve "
			"response from remote daemon at '%s'\n",
			_addr.c_str()); }
		dprintf(D_FULLDEBUG, "Daemon::approveTokenRequest() failed to recieve "
			"response from remote daemon at '%s'\n",
			_addr.c_str());
		return false;
	}

	if (!rSock.end_of_message()) {
		if (err) { err->pushf("DAEMON", 1, "Failed to read "
			"end-of-message from remote daemon at '%s'",
			_addr.c_str()); }
		dprintf( D_FULLDEBUG, "Daemon::approveTokenRequest() failed to read "
			"end of message from remote daemon at '%s'\n",
			_addr.c_str() );
		return false;
	}

	int error_code = 0;
	if (!result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code)) {
		if (err) { err->pushf("DAEMON", 1, "Remote daemon "
			"at '%s' did not return a result.",
			_addr.c_str()); }
		dprintf( D_FULLDEBUG, "Daemon::approveTokenRequest() - Remote daemon "
			"at '%s' did not return a result.\n",
			_addr.c_str() );
		return false;
	}
	if (error_code) {
		std::string err_msg;
		result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg);
		if (err_msg.empty()) {err_msg = "Unknown error.";}

		if (err) { err->push("DAEMON", error_code, err_msg.c_str()); }
		return false;
	}
	return true;
}


bool
Daemon::autoApproveTokens( const std::string &netblock, time_t lifetime,
	CondorError *err ) noexcept
{
	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "Daemon::autoApproveTokenRequest() making connection to "
			"'%s'\n", _addr.c_str() );
	}

	classad::ClassAd ad;
	if (netblock.empty()) {
		if (err) err->pushf("DAEMON", 1, "No netblock provided.");
		dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest(): No netblock provided.");
		return false;
	} else {
		condor_netaddr na;
		if(! na.from_net_string(netblock.c_str())) {
			err->pushf( "DAEMON", 2, "Auto-approval rule netblock invalid." );
			dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest(): auto-approval rule netblock is invalid.\n");
			return false;
		}

		if (!ad.InsertAttr(ATTR_SUBNET, netblock)) {
			if (err) err->pushf("DAEMON", 1, "Unable to set netblock.");
			dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest(): Unable to set netblock.\n");
			return false;
		}
	}
	if( lifetime > 0 ) {
		if(! ad.InsertAttr(ATTR_SEC_LIFETIME, lifetime)) {
			if (err) err->pushf("DAEMON", 1, "Unable to set lifetime.");
			dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest(): Unable to set lifetime.\n");
			return false;
		}
	} else {
		if (err) err->pushf("DAEMON", 2, "Auto-approval rule lifetimes must be greater than zero." );
		dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest(): auto-approval rule lifetimes must be greater than zero.\n" );
		return false;
	}

	ReliSock rSock;
	rSock.timeout( 5 );
	if(! connectSock( & rSock )) {
		if (err) err->pushf("DAEMON", 1, "Failed to connect to remote daemon at '%s'",
			_addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest() failed to connect "
			"to remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!startCommand( DC_AUTO_APPROVE_TOKEN_REQUEST, &rSock, 20, err)) {
		dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest() failed to start command for "
			"auto-approving token requests with remote daemon at '%s'.\n",
			_addr.c_str());
		return false;
	}

	if (!putClassAd(&rSock, ad) || !rSock.end_of_message()) {
		if (err) err->pushf("DAEMON", 1, "Failed to send ClassAd to remote daemon at"
			" '%s'", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::approveTokenRequest() Failed to send ClassAd to "
			"remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	rSock.decode();

	classad::ClassAd result_ad;
	if (!getClassAd(&rSock, result_ad)) {
		if (err) err->pushf("DAEMON", 1, "Failed to recieve response from remote daemon at"
			" at '%s'\n", _addr.c_str());
		dprintf(D_FULLDEBUG, "Daemon::autoApproveTokenRequest() failed to recieve response "
			"from remote daemon at '%s'\n", _addr.c_str());
		return false;
	}

	if (!rSock.end_of_message()) {
		if (err) err->pushf("DAEMON", 1, "Failed to read end-of-message from remote daemon"
			" at '%s'\n", _addr.c_str());
		dprintf( D_FULLDEBUG, "Daemon::autoApproveTokenRequest() failed to read "
			"end of message from remote daemon at '%s'\n", _addr.c_str() );
		return false;
	}

	int error_code = 0;
	if (!result_ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code)) {
		if (err) err->pushf("DAEMON", 1, "Remote daemon at '%s' did not return a result.",
			_addr.c_str());
		dprintf( D_FULLDEBUG, "Daemon::autoApproveTokenRequest() - Remote daemon at '%s' did "
			"not return a result", _addr.c_str() );
		return false;
	}

	if (error_code) {
		std::string err_msg;
		result_ad.EvaluateAttrString(ATTR_ERROR_STRING, err_msg);
		if (err_msg.empty()) {err_msg = "Unknown error.";}

		if (err) err->push("DAEMON", error_code, err_msg.c_str());
		return false;
	}
	return true;
}
