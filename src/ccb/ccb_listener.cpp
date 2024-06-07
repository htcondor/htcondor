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
#include "condor_config.h"
#include "ccb_listener.h"
#include "subsystem_info.h"

static int CCB_TIMEOUT = 300;

CCBListener::CCBListener(char const *ccb_address):
	m_ccb_address(ccb_address),
	m_sock(NULL),
	m_waiting_for_connect(false),
	m_waiting_for_registration(false),
	m_registered(false),
	m_reconnect_timer(-1),
	m_heartbeat_timer(-1),
	m_heartbeat_interval(0),
	m_last_contact_from_peer(0),
	m_heartbeat_disabled(false),
	m_heartbeat_initialized(false)
{
}

CCBListener::~CCBListener()
{
	if( m_sock ) {
		daemonCore->Cancel_Socket( m_sock );
		delete m_sock;
	}
	if( m_reconnect_timer != -1 ) {
		daemonCore->Cancel_Timer( m_reconnect_timer );
	}
	StopHeartbeat();
}

void
CCBListener::InitAndReconfig()
{
	int new_heartbeat_interval = param_integer("CCB_HEARTBEAT_INTERVAL",1200,0);
	if( new_heartbeat_interval != m_heartbeat_interval ) {
		if( new_heartbeat_interval < 30 && new_heartbeat_interval > 0 ) {
			new_heartbeat_interval = 30;
				// CCB server doesn't expect a high rate of unsolicited
				// input from us
			dprintf(D_ALWAYS,
					"CCBListener: using minimum heartbeat interval of %ds\n",
					new_heartbeat_interval);
		}
		m_heartbeat_interval = new_heartbeat_interval;
		if( m_heartbeat_initialized ) {
			RescheduleHeartbeat();
		}
	}

	// Globals are evil, but at least now it's configurable.
	CCB_TIMEOUT = param_integer("CCB_TIMEOUT", 300);
}

bool
CCBListener::RegisterWithCCBServer(bool blocking)
{
	ClassAd msg;

	if( m_waiting_for_connect || m_reconnect_timer != -1 || m_waiting_for_registration || m_registered) {
			// already registered or being registered
		return m_registered;
	}

	msg.Assign( ATTR_COMMAND, CCB_REGISTER );
	if( !m_ccbid.empty() ) {
		// we are reconnecting; trying to preserve ccbid so that prospective
		// clients with stale information can still contact us
		msg.Assign( ATTR_CCBID, m_ccbid );
		msg.Assign( ATTR_CLAIM_ID, m_reconnect_cookie );
	}

		// for debugging purposes only, identify ourselves to the CCB server
	std::string name;
	formatstr(name, "%s %s",get_mySubSystem()->getName(),daemonCore->publicNetworkIpAddr());
	msg.Assign( ATTR_NAME, name );

	bool success = SendMsgToCCB(msg,blocking);
	if( success ) {
		if( blocking ) {
			success = ReadMsgFromCCB();
		}
		else {
			// now we wait for CCB server to respond with our CCBID
			m_waiting_for_registration = true;
		}
	}
	return success;
}

bool
CCBListener::SendMsgToCCB(ClassAd &msg,bool blocking)
{
	if( !m_sock ) {
		Daemon ccb(DT_COLLECTOR,m_ccb_address.c_str());

		int cmd = -1;
		msg.LookupInteger( ATTR_COMMAND, cmd );
		if( cmd != CCB_REGISTER ) {
			dprintf(D_ALWAYS,"CCBListener: no connection to CCB server %s"
					" when trying to send command %d\n",
					m_ccb_address.c_str(), cmd );
			return false;
		}

		// Specifying USE_TMP_SEC_SESSION to force a fresh security
		// session.  Otherwise we can end up in a catch-22 where we
		// are trying to reconnect to the CCB server and we try to use
		// a cached security session which is no longer valid, but our
		// CCB server cannot send us the invalidation message because
		// we are trying to reconnect to it.  Expring this session
		// right away is also a good idea, because if we are just
		// starting up, the return address associated with it will not
		// have any CCB information attached, which again means that
		// the CCB server has no way to invalidate it.

		if( blocking ) {
			m_sock = ccb.startCommand( cmd, Stream::reli_sock, CCB_TIMEOUT, NULL, NULL, false, USE_TMP_SEC_SESSION );
			if( m_sock ) {
				Connected();
			}
			else {
				Disconnected();
				return false;
			}
		}
		else if( !m_waiting_for_connect ) {
			if (IsDebugLevel(D_COMMAND)) {
				const char * addr = ccb.addr();
				dprintf (D_COMMAND, "CCBListener::SendMsgToCCB(%s,...) making non-blocking connection to %s\n", getCommandStringSafe(cmd), addr ? addr : "NULL");
			}
			m_sock = ccb.makeConnectedSocket(Stream::reli_sock, CCB_TIMEOUT, 0, NULL, true /*nonblocking*/ );
			if( !m_sock ) {
				Disconnected();
				return false;
			}
			m_waiting_for_connect = true;
			incRefCount(); // do not let ourselves be deleted until called back
			ccb.startCommand_nonblocking( cmd, m_sock, CCB_TIMEOUT, NULL, CCBListener::CCBConnectCallback, this, NULL, false, USE_TMP_SEC_SESSION );
			return false;
		}
	}

	return WriteMsgToCCB(msg);
}

bool
CCBListener::WriteMsgToCCB(ClassAd &msg)
{
	if( !m_sock || m_waiting_for_connect ) {
		return false;
	}

	m_sock->encode();
	if( !putClassAd( m_sock, msg ) || !m_sock->end_of_message() ) {
		Disconnected();
		return false;
	}

	return true;
}

void
CCBListener::CCBConnectCallback(bool success,Sock *sock,CondorError * /*errstack*/, const std::string & /*trust_domain*/, bool /*should_try_token_auth*/, void *misc_data)
{
	CCBListener *self = (CCBListener *)misc_data;

	self->m_waiting_for_connect = false;

	ASSERT( self->m_sock == sock );

	if( success ) {
		ASSERT( self->m_sock->is_connected() );
		self->Connected();
		self->RegisterWithCCBServer();
	}
	else {
		delete self->m_sock;
		self->m_sock = NULL;
		self->Disconnected();
	}

	self->decRefCount(); // remove ref count from when we started the connect
}

void
CCBListener::ReconnectTime(int /* timerID */)
{
	m_reconnect_timer = -1;

	RegisterWithCCBServer();
}

void
CCBListener::Connected()
{
	int rc = daemonCore->Register_Socket(
		m_sock,
		m_sock->peer_description(),
		(SocketHandlercpp)&CCBListener::HandleCCBMsg,
		"CCBListener::HandleCCBMsg",
		this);

	ASSERT( rc >= 0 );

	m_last_contact_from_peer = time(NULL);
	RescheduleHeartbeat();
}

void
CCBListener::Disconnected()
{
	if( m_sock ) {
		daemonCore->Cancel_Socket( m_sock );
		delete m_sock;
		m_sock = NULL;
	}

	if( m_waiting_for_connect ) {
		m_waiting_for_connect = false;
		decRefCount();
	}

	m_waiting_for_registration = false;
	m_registered = false;

	StopHeartbeat();

	if( m_reconnect_timer != -1 ) {
		return; // already in progress
	}

	int reconnect_time = param_integer("CCB_RECONNECT_TIME",60);

	dprintf(D_ALWAYS,
			"CCBListener: connection to CCB server %s failed; "
			"will try to reconnect in %d seconds.\n",
			m_ccb_address.c_str(), reconnect_time);

	m_reconnect_timer = daemonCore->Register_Timer(
		reconnect_time,
		(TimerHandlercpp)&CCBListener::ReconnectTime,
		"CCBListener::ReconnectTime",
		this );

	ASSERT( m_reconnect_timer != -1 );
}

void
CCBListener::StopHeartbeat()
{
	if( m_heartbeat_timer != -1 ) {
		daemonCore->Cancel_Timer( m_heartbeat_timer );
		m_heartbeat_timer = -1;
	}
}

void
CCBListener::RescheduleHeartbeat()
{
	if( !m_heartbeat_initialized ) {
		if( !m_sock ) {
			return;
		}

		m_heartbeat_initialized = true;

		m_heartbeat_disabled = false;
		CondorVersionInfo const *server_version = m_sock->get_peer_version();
		if( m_heartbeat_interval <= 0 ) {
			dprintf(D_ALWAYS,"CCBListener: heartbeat disabled because interval is configured to be 0\n");
		}
		else if( server_version ) {
			if( !server_version->built_since_version(7,5,0) ) {
				m_heartbeat_disabled = true;
				dprintf(D_ALWAYS,"CCBListener: server is too old to support heartbeat, so not sending one.\n");
			}
		}
	}

	if( m_heartbeat_interval <= 0 || m_heartbeat_disabled ) {
		StopHeartbeat();
		m_heartbeat_initialized = true;
	}
	else if( m_sock && m_sock->is_connected() ) {
		int next_time = m_heartbeat_interval - (time(NULL)-m_last_contact_from_peer);
		if( next_time < 0 || next_time > m_heartbeat_interval) {
			next_time = 0;
		}
		if( m_heartbeat_timer == -1 ) {
			m_last_contact_from_peer = time(NULL);
			m_heartbeat_timer = daemonCore->Register_Timer(
				next_time,
				m_heartbeat_interval,
				(TimerHandlercpp)&CCBListener::HeartbeatTime,
				"CCBListener::HeartbeatTime",
				this );
			ASSERT( m_heartbeat_timer != -1 );
		}
		else {
			daemonCore->Reset_Timer(
				m_heartbeat_timer,
				next_time,
				m_heartbeat_interval);
		}
	}
}

void
CCBListener::HeartbeatTime(int /* timerID */)
{
	int age = time(NULL) - m_last_contact_from_peer;
	if( age > 3*m_heartbeat_interval ) {
		dprintf(D_ALWAYS, "CCBListener: no activity from CCB server in %ds; "
				"assuming connection is dead.\n", age);
		Disconnected();
		return;
	}

	dprintf(D_FULLDEBUG, "CCBListener: sent heartbeat to server.\n");

	ClassAd msg;
	msg.Assign(ATTR_COMMAND, ALIVE);
	SendMsgToCCB(msg,false);
}

int
CCBListener::HandleCCBMsg(Stream * /*sock*/)
{
	ReadMsgFromCCB();
	return KEEP_STREAM;
}

bool
CCBListener::ReadMsgFromCCB()
{
	if( !m_sock ) {
		return false;
	}
	m_sock->timeout(CCB_TIMEOUT);
	ClassAd msg;
	if( !getClassAd( m_sock, msg ) || !m_sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCBListener: failed to receive message from CCB server %s\n",
				m_ccb_address.c_str());
		Disconnected();
		return false;
	}

	m_last_contact_from_peer = time(NULL);
	RescheduleHeartbeat();

	int cmd = -1;
	msg.LookupInteger( ATTR_COMMAND, cmd );
	switch( cmd ) {
	case CCB_REGISTER:
		return HandleCCBRegistrationReply( msg );
	case CCB_REQUEST:
		return HandleCCBRequest( msg );
	case ALIVE:
		dprintf(D_FULLDEBUG,"CCBListener: received heartbeat from server.\n");
		return true;
	}

	std::string msg_str;
	sPrintAd(msg_str, msg);
	dprintf( D_ALWAYS,
			 "CCBListener: Unexpected message received from CCB "
			 "server: %s\n",
			 msg_str.c_str() );
	return false;
}

bool
CCBListener::HandleCCBRegistrationReply( ClassAd &msg )
{
	if( !msg.LookupString(ATTR_CCBID,m_ccbid) ) {
		std::string msg_str;
		sPrintAd(msg_str, msg);
		EXCEPT("CCBListener: no ccbid in registration reply: %s",
			   msg_str.c_str() );
	}
	msg.LookupString(ATTR_CLAIM_ID,m_reconnect_cookie);
	dprintf(D_ALWAYS,
			"CCBListener: registered with CCB server %s as ccbid %s\n",
			m_ccb_address.c_str(),
			m_ccbid.c_str() );

	m_waiting_for_registration = false;
	m_registered = true;

	daemonCore->daemonContactInfoChanged();

	return true;
}

bool
CCBListener::HandleCCBRequest( ClassAd &msg )
{
	std::string address;
	std::string connect_id;
	std::string request_id;
	std::string name;
	if( !msg.LookupString( ATTR_MY_ADDRESS, address) ||
		!msg.LookupString( ATTR_CLAIM_ID, connect_id) ||
		!msg.LookupString( ATTR_REQUEST_ID, request_id) )
	{
		std::string msg_str;
		sPrintAd(msg_str, msg);
		EXCEPT("CCBListener: invalid CCB request from %s: %s",
			   m_ccb_address.c_str(),
			   msg_str.c_str() );
	}

	msg.LookupString( ATTR_NAME, name );

	if( name.find(address) == std::string::npos ) {
		formatstr_cat(name, " with reverse connect address %s",address.c_str());
	}
	dprintf(D_FULLDEBUG|D_NETWORK,
			"CCBListener: received request to connect to %s, request id %s.\n",
			name.c_str(), request_id.c_str());

	return DoReversedCCBConnect( address.c_str(), connect_id.c_str(), request_id.c_str(), name.c_str() );
}

bool
CCBListener::DoReversedCCBConnect( char const *address, char const *connect_id, char const *request_id, char const *peer_description )
{
	Daemon daemon( DT_ANY, address );
	CondorError errstack;
	Sock *sock = daemon.makeConnectedSocket(
		Stream::reli_sock,CCB_TIMEOUT,0,&errstack,true /*nonblocking*/);

	ClassAd *msg_ad = new ClassAd;
	ASSERT( msg_ad );
	msg_ad->Assign( ATTR_CLAIM_ID, connect_id );
	msg_ad->Assign( ATTR_REQUEST_ID, request_id );
		// the following is put in the message because that is an easy (lazy)
		// way to make it available to ReportReverseConnectResult
	msg_ad->Assign( ATTR_MY_ADDRESS, address);

	if( !sock ) {
			// Failed to create socket or initiate connect
		ReportReverseConnectResult(msg_ad,false,"failed to initiate connection");
		delete msg_ad;
		return false;
	}

	if( peer_description ) {
		char const *peer_ip = sock->peer_ip_str();
		if( peer_ip && !strstr(peer_description,peer_ip)) {
			std::string desc;
			formatstr(desc, "%s at %s",peer_description,sock->get_sinful_peer());
			sock->set_peer_description(desc.c_str());
		}
		else {
			sock->set_peer_description(peer_description);
		}
	}

	incRefCount();      // do not delete self until called back

	int rc = daemonCore->Register_Socket(
		sock,
		sock->peer_description(),
		(SocketHandlercpp)&CCBListener::ReverseConnected,
		"CCBListener::ReverseConnected",
		this);

	if( rc < 0 ) {
		ReportReverseConnectResult(msg_ad,false,"failed to register socket for non-blocking reversed connection");
		delete msg_ad;
		delete sock;
		decRefCount();
		return false;
	}

	rc = daemonCore->Register_DataPtr(msg_ad);
	ASSERT( rc );

	return true;
}

int
CCBListener::ReverseConnected(Stream *stream)
{
	Sock *sock = (Sock *)stream;
	ClassAd *msg_ad = (ClassAd *)daemonCore->GetDataPtr();
	ASSERT( msg_ad );

	if( sock ) {
		daemonCore->Cancel_Socket( sock );
	}

	if( !sock || !sock->is_connected() ) {
		ReportReverseConnectResult(msg_ad,false,"failed to connect");
	}
	else {

			// The reverse-connect protocol is designed to look like a
			// raw cedar command, in case the thing we are connecting
			// to is a cedar command socket.

		sock->encode();
		int cmd = CCB_REVERSE_CONNECT;
		if( !sock->put(cmd) ||
			!putClassAd( sock, *msg_ad ) ||
			!sock->end_of_message() )
		{
			ReportReverseConnectResult(msg_ad,false,"failure writing reverse connect command");
		}
		else {
			((ReliSock*)sock)->isClient(false);
			static_cast<ReliSock*>(sock)->resetHeaderMD();
			daemonCore->HandleReqAsync(sock);
			sock = NULL; // daemonCore took ownership of sock
			ReportReverseConnectResult(msg_ad,true);
		}
	}

	delete msg_ad;
	if( sock ) {
		delete sock;
	}
	decRefCount(); // we incremented ref count when setting up callback

	return KEEP_STREAM;
}

void
CCBListener::ReportReverseConnectResult(ClassAd *connect_msg,bool success,char const *error_msg)
{
	ClassAd msg = *connect_msg;

	std::string request_id;
	std::string address;
	connect_msg->LookupString(ATTR_REQUEST_ID,request_id);
	connect_msg->LookupString(ATTR_MY_ADDRESS,address);
	if( !success ) {
		dprintf(D_ALWAYS,
				"CCBListener: failed to create reversed connection for "
				"request id %s to %s: %s\n",
				request_id.c_str(),
				address.c_str(),
				error_msg ? error_msg : "");
	}
	else {
		dprintf(D_FULLDEBUG|D_NETWORK,
				"CCBListener: created reversed connection for "
				"request id %s to %s: %s\n",
				request_id.c_str(),
				address.c_str(),
				error_msg ? error_msg : "");
	}

	msg.Assign(ATTR_RESULT,success);
	if( error_msg ) {
		msg.Assign(ATTR_ERROR_STRING,error_msg);
	}
	WriteMsgToCCB( msg );
}

bool
CCBListener::operator ==(CCBListener const &other)
{
	char const *other_addr = other.getAddress();
	if( m_ccb_address.c_str() == other_addr ) {
		return true;
	}
	return other_addr && !strcmp(m_ccb_address.c_str(),other_addr);
}


CCBListener *
CCBListeners::GetCCBListener(char const *address)
{
	classy_counted_ptr<CCBListener> ccb_listener;

	if( !address ) {
		return NULL;
	}

	for(auto & m_ccb_listener : m_ccb_listeners) {
		ccb_listener = m_ccb_listener;
		if( !strcmp(address,ccb_listener->getAddress()) ) {
			return ccb_listener.get();
		}
	}
	return NULL;
}

void
CCBListeners::GetCCBContactString(std::string &result)
{
	classy_counted_ptr<CCBListener> ccb_listener;

	for(auto & m_ccb_listener : m_ccb_listeners) {
		ccb_listener = m_ccb_listener;
		char const *ccbid = ccb_listener->getCCBID();
		if( ccbid && *ccbid ) {
			if( !result.empty() ) {
				result += " ";
			}
			result += ccbid;
		}
	}
}

int
CCBListeners::RegisterWithCCBServer(bool blocking)
{
	int result = 0;

	classy_counted_ptr<CCBListener> ccb_listener;
	for(auto & m_ccb_listener : m_ccb_listeners) {
		ccb_listener = m_ccb_listener;
		if( !ccb_listener->RegisterWithCCBServer(blocking) && blocking ) {
		} else {
			++result;
		}
	}
	return result;
}

void
CCBListeners::Configure(char const *addresses)
{
	CCBListenerList new_ccbs;

	for (const auto& address: StringTokenIterator(addresses)) {
		CCBListener *listener;

			// preserve existing CCBListener if there is one connected
			// to this address
		listener = GetCCBListener(address.c_str());

		if( !listener ) {

			Daemon daemon(DT_COLLECTOR, address.c_str());
			char const *ccb_addr_str = daemon.addr();
			char const *my_addr_str = daemonCore->publicNetworkIpAddr();
			Sinful ccb_addr( ccb_addr_str );
			Sinful my_addr( my_addr_str );

			if( my_addr.addressPointsToMe( ccb_addr ) ) {
				dprintf(D_ALWAYS,"CCBListener: skipping CCB Server %s because it points to myself.\n",address.c_str());
				continue;
			}
			dprintf(D_FULLDEBUG,"CCBListener: good: CCB address %s does not point to my address %s\n",
					ccb_addr_str?ccb_addr_str:"null",
					my_addr_str?my_addr_str:"null");

			listener = new CCBListener(address.c_str());
		}

		new_ccbs.push_back( listener );
	}

	m_ccb_listeners.clear();

	classy_counted_ptr<CCBListener> ccb_listener;
	for(auto & new_ccb : new_ccbs) {
		ccb_listener = new_ccb;
		if( GetCCBListener( ccb_listener->getAddress() ) ) {
				// ignore duplicate entries with same address
			continue;
		}
		m_ccb_listeners.push_back( ccb_listener );

		ccb_listener->InitAndReconfig();
	}
}
