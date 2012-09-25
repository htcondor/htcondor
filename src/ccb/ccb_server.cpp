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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "ccb_server.h"
#include "condor_sinful.h"
#include "util_lib_proto.h"
#include "condor_open.h"

static unsigned int
ccbid_hash(const CCBID &ccbid) {
	return ccbid;
}

static bool
CCBIDFromString(CCBID &ccbid,char const *ccbid_str)
{
	if( sscanf(ccbid_str,"%lu",&ccbid)!=1 ) {
		return false;
	}
	return true;
}

static char const *
CCBIDToString(CCBID ccbid,MyString &ccbid_str)
{
	ccbid_str.formatstr("%lu",ccbid);
	return ccbid_str.Value();
}

static bool
CCBIDFromContactString(CCBID &ccbid,char const *ccb_contact)
{
	// format is "IPAddress#CCBID"
	ccb_contact = strchr(ccb_contact,'#');
	if( !ccb_contact ) {
		return false;
	}
	return CCBIDFromString(ccbid,ccb_contact+1);
}

static void
CCBIDToContactString(char const *my_address,CCBID ccbid,MyString &ccb_contact)
{
	ccb_contact.formatstr("%s#%lu",my_address,ccbid);
}

CCBServer::CCBServer():
	m_registered_handlers(false),
	m_targets(ccbid_hash),
	m_reconnect_info(ccbid_hash),
	m_reconnect_fp(NULL),
	m_last_reconnect_info_sweep(0),
	m_reconnect_info_sweep_interval(0),
	m_next_ccbid(1),
	m_next_request_id(1),
	m_read_buffer_size(0),
	m_write_buffer_size(0),
	m_requests(ccbid_hash),
	m_polling_timer(-1)
{
}

CCBServer::~CCBServer()
{
	CloseReconnectFile();
	if( m_registered_handlers ) {
		daemonCore->Cancel_Command(CCB_REGISTER);
		daemonCore->Cancel_Command(CCB_REQUEST);
		m_registered_handlers = false;
	}
	if( m_polling_timer != -1 ) {
		daemonCore->Cancel_Timer( m_polling_timer );
		m_polling_timer = -1;
	}
	CCBTarget *target=NULL;
	m_targets.startIterations();
	while( m_targets.iterate(target) ) {
		RemoveTarget(target);
	}
}

void
CCBServer::RegisterHandlers()
{
	if( m_registered_handlers ) {
		return;
	}
	m_registered_handlers = true;

	int rc = daemonCore->Register_CommandWithPayload(
		CCB_REGISTER,
		"CCB_REGISTER",
		(CommandHandlercpp)&CCBServer::HandleRegistration,
		"CCBServer::HandleRegistration",
		this,
		DAEMON);
	ASSERT( rc >= 0 );

	rc = daemonCore->Register_CommandWithPayload(
		CCB_REQUEST,
		"CCB_REQUEST",
		(CommandHandlercpp)&CCBServer::HandleRequest,
		"CCBServer::HandleRequest",
		this,
		READ);
	ASSERT( rc >= 0 );

}

void
CCBServer::InitAndReconfig()
{
		// construct the CCB address to be advertised by CCB listeners
	Sinful sinful(daemonCore->publicNetworkIpAddr());
		// strip out <>'s, private address, and CCB listener info
	sinful.setPrivateAddr(NULL);
	sinful.setCCBContact(NULL);
	ASSERT( sinful.getSinful() && sinful.getSinful()[0] == '<' );
	m_address.formatstr("%s",sinful.getSinful()+1);
	if( m_address[m_address.Length()-1] == '>' ) {
		m_address.setChar(m_address.Length()-1,'\0');
	}

	m_read_buffer_size = param_integer("CCB_SERVER_READ_BUFFER",2*1024);
	m_write_buffer_size = param_integer("CCB_SERVER_WRITE_BUFFER",2*1024);

	m_last_reconnect_info_sweep = time(NULL);

	m_reconnect_info_sweep_interval = param_integer("CCB_SWEEP_INTERVAL",1200);

	CloseReconnectFile();

	MyString old_reconnect_fname = m_reconnect_fname;
	char *fname = param("CCB_RECONNECT_FILE");
	if( fname ) {
		m_reconnect_fname = fname;
		if( m_reconnect_fname.find(".ccb_reconnect") == -1 ) {
			// required for preen to ignore this file
			m_reconnect_fname += ".ccb_reconnect";
		}
		free( fname );
	}
	else {
		char *spool = param("SPOOL");
		ASSERT( spool );
		Sinful my_addr( daemonCore->publicNetworkIpAddr() );
		m_reconnect_fname.formatstr("%s%c%s-%s.ccb_reconnect",
			spool,
			DIR_DELIM_CHAR,
			my_addr.getHost() ? my_addr.getHost() : "localhost",
			my_addr.getPort() ? my_addr.getPort() : "0");
		free( spool );
	}

	if( old_reconnect_fname != m_reconnect_fname &&
		!old_reconnect_fname.IsEmpty() &&
		!m_reconnect_fname.IsEmpty() )
	{
		// reconnect filename changed
		// not worth freaking out on error here
		remove( m_reconnect_fname.Value() );
		rename( old_reconnect_fname.Value(), m_reconnect_fname.Value() );
	}
	if( old_reconnect_fname.IsEmpty() &&
		!m_reconnect_fname.IsEmpty() &&
		m_reconnect_info.getNumElements() == 0 )
	{
		// we are starting up from scratch, so load saved info
		LoadReconnectInfo();
	}

	Timeslice poll_slice;
	poll_slice.setTimeslice( // do not run more than this fraction of the time
		param_double("CCB_POLLING_TIMESLICE",0.05) );

	poll_slice.setDefaultInterval( // try to run this often
		param_integer("CCB_POLLING_INTERVAL",20,0) );

	poll_slice.setMaxInterval( // run at least this often
		param_integer("CCB_POLLING_MAX_INTERVAL",600) );

	if( m_polling_timer != -1 ) {
		daemonCore->Cancel_Timer(m_polling_timer);
	}

	m_polling_timer = daemonCore->Register_Timer(
		poll_slice,
		(TimerHandlercpp)&CCBServer::PollSockets,
		"CCBServer::PollSockets",
		this);

	RegisterHandlers();
}


void
CCBServer::PollSockets()
{
		// Find out if any of our registered target daemons have
		// disconnected or sent us a response to a request.
		// We don't just register all of these sockets with DaemonCore
		// out of fear that the overhead of dealing with all of these
		// sockets in every iteration of the select loop may be
		// too much.

	CCBTarget *target=NULL;
	m_targets.startIterations();
	while( m_targets.iterate(target) ) {
		if( target->getSock()->readReady() ) {
			HandleRequestResultsMsg(target);
		}
	}

	// periodically call the following
	SweepReconnectInfo();
}

int
CCBServer::HandleRegistration(int cmd,Stream *stream)
{
	ReliSock *sock = (ReliSock *)stream;
	ASSERT( cmd == CCB_REGISTER );

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	ClassAd msg;
	sock->decode();
	if( !msg.initFromStream( *sock ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to receive registration "
				"from %s.\n", sock->peer_description() );
		return FALSE;
	}

	SetSmallBuffers(sock);

	MyString name;
	if( msg.LookupString(ATTR_NAME,name) ) {
			// target daemon name is purely for debugging purposes
		name.formatstr_cat(" on %s",sock->peer_description());
		sock->set_peer_description(name.Value());
	}

	CCBTarget *target = new CCBTarget(sock);

	MyString reconnect_cookie_str,reconnect_ccbid_str;
	CCBID reconnect_cookie,reconnect_ccbid;
	bool reconnected = false;
	if( msg.LookupString(ATTR_CLAIM_ID,reconnect_cookie_str) &&
		CCBIDFromString(reconnect_cookie,reconnect_cookie_str.Value()) &&
		msg.LookupString( ATTR_CCBID,reconnect_ccbid_str) &&
		CCBIDFromContactString(reconnect_ccbid,reconnect_ccbid_str.Value()) )
	{
		target->setCCBID( reconnect_ccbid );
		reconnected = ReconnectTarget( target, reconnect_cookie );
	}

	if( !reconnected ) {
		AddTarget( target );
	}

	CCBReconnectInfo *reconnect_info = GetReconnectInfo( target->getCCBID() );
	ASSERT( reconnect_info );

	sock->encode();

	ClassAd reply_msg;
	MyString ccb_contact;
	CCBIDToString( reconnect_info->getReconnectCookie(),reconnect_cookie_str );
		// We send our address as part of the CCB contact string, rather
		// than letting the target daemon fill it in.  This is to give us
		// potential flexibility on the CCB server side to do things like
		// assign different targets to different CCB server sub-processes,
		// each with their own command port.
	CCBIDToContactString( m_address.Value(), target->getCCBID(), ccb_contact );

	reply_msg.Assign(ATTR_CCBID,ccb_contact.Value());
	reply_msg.Assign(ATTR_COMMAND,CCB_REGISTER);
	reply_msg.Assign(ATTR_CLAIM_ID,reconnect_cookie_str.Value());

	if( !reply_msg.put( *sock ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to send registration response "
				"to %s.\n", sock->peer_description() );

		RemoveTarget( target );
		return KEEP_STREAM; // we have already closed this socket
	}

	return KEEP_STREAM;
}

void
CCBServer::SetSmallBuffers(Sock *sock)
{
		// Adjust socket buffers so we can have loads of these sockets
		// without chewing up too much memory.  We expect to just send
		// and receive small classads.
	sock->set_os_buffers(m_read_buffer_size);
	sock->set_os_buffers(m_write_buffer_size,true);
}

int
CCBServer::HandleRequest(int cmd,Stream *stream)
{
	ReliSock *sock = (ReliSock *)stream;
	ASSERT( cmd == CCB_REQUEST );

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	ClassAd msg;
	sock->decode();
	if( !msg.initFromStream( *sock ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to receive request "
				"from %s.\n", sock->peer_description() );
		return FALSE;
	}

	MyString name;
	if( msg.LookupString(ATTR_NAME,name) ) {
			// client name is purely for debugging purposes
		name.formatstr_cat(" on %s",sock->peer_description());
		sock->set_peer_description(name.Value());
	}
	MyString target_ccbid_str;
	MyString return_addr;
	MyString connect_id; // id target daemon should present to requester
	CCBID target_ccbid;

		// NOTE: using ATTR_CLAIM_ID for connect id so that it is
		// automatically treated as a secret over the network.
		// It must be presented by the target daemon when connecting
		// to the requesting client, so the client can confirm that
		// the connection is in response to its request.

	if( !msg.LookupString(ATTR_CCBID,target_ccbid_str) ||
		!msg.LookupString(ATTR_MY_ADDRESS,return_addr) ||
		!msg.LookupString(ATTR_CLAIM_ID,connect_id) )
	{
		MyString ad_str;
		msg.sPrint(ad_str);
		dprintf(D_ALWAYS,
				"CCB: invalid request from %s: %s\n",
				sock->peer_description(), ad_str.Value() );
		return FALSE;
	}
	if( !CCBIDFromString(target_ccbid,target_ccbid_str.Value()) ) {
		dprintf(D_ALWAYS,
				"CCB: request from %s contains invalid CCBID %s\n",
				sock->peer_description(), target_ccbid_str.Value() );
		return FALSE;
	}

	CCBTarget *target = GetTarget( target_ccbid );
	if( !target ) {
		dprintf(D_ALWAYS,
			"CCB: rejecting request from %s for ccbid %s because no daemon is "
			"currently registered with that id "
			"(perhaps it recently disconnected).\n",
			sock->peer_description(), target_ccbid_str.Value());

		MyString error_msg;
		error_msg.formatstr(
			"CCB server rejecting request for ccbid %s because no daemon is "
			"currently registered with that id "
			"(perhaps it recently disconnected).", target_ccbid_str.Value());
		RequestReply( sock, false, error_msg.Value(), 0, target_ccbid );
		return FALSE;
	}

	SetSmallBuffers(sock);

	CCBServerRequest *request =
		new CCBServerRequest(
			sock,
			target_ccbid,
			return_addr.Value(),
			connect_id.Value() );
	AddRequest( request, target );

	dprintf(D_FULLDEBUG,
			"CCB: received request id %lu from %s for target ccbid %s "
			"(registered as %s)\n",
			request->getRequestID(),
			request->getSock()->peer_description(),
			target_ccbid_str.Value(),
			target->getSock()->peer_description());

	ForwardRequestToTarget( request, target );

	return KEEP_STREAM;
}

int
CCBServer::HandleRequestResultsMsg( Stream * /*stream*/ )
{
	CCBTarget *target = (CCBTarget *)daemonCore->GetDataPtr();
	HandleRequestResultsMsg( target );
	return KEEP_STREAM;
}

void
CCBServer::HandleRequestResultsMsg( CCBTarget *target )
{
		// Reply from target daemon about whether it succeeded in
		// connecting to the requested client.

	Sock *sock = target->getSock();

	ClassAd msg;
	sock->decode();
	if( !msg.initFromStream( *sock ) || !sock->end_of_message() ) {
			// disconnect
		dprintf(D_FULLDEBUG,
				"CCB: received disconnect from target daemon %s "
				"with ccbid %lu.\n",
				sock->peer_description(), target->getCCBID() );
		RemoveTarget( target );
		return;
	}

	int command = 0;
	if( msg.LookupInteger( ATTR_COMMAND, command ) && command == ALIVE ) {
		SendHeartbeatResponse( target );
		return;
	}

	target->decPendingRequestResults();

	bool success = false;
	MyString error_msg;
	MyString reqid_str;
	CCBID reqid;
	MyString connect_id;
	msg.LookupBool( ATTR_RESULT, success );
	msg.LookupString( ATTR_ERROR_STRING, error_msg );
	msg.LookupString( ATTR_REQUEST_ID, reqid_str );
	msg.LookupString( ATTR_CLAIM_ID, connect_id );

	if( !CCBIDFromString( reqid, reqid_str.Value() ) ) {
		MyString msg_str;
		msg.sPrint(msg_str);
		dprintf(D_ALWAYS,
				"CCB: received reply from target daemon %s with ccbid %lu "
				"without a valid request id: %s\n",
				sock->peer_description(),
				target->getCCBID(),
				msg_str.Value());
		RemoveTarget( target );
		return;
	}

	CCBServerRequest *request = GetRequest( reqid );
	if( request && request->getSock()->readReady() ) {
		// Request socket must have just closed.  To avoid noise in
		// logs when we fail to write to it, delete the request now.
		RemoveRequest( request );
		request = NULL;
	}

	char const *request_desc = "(client which has gone away)";
	if( request ) {
		request_desc = request->getSock()->peer_description();
	}

	if( success ) {
		dprintf(D_FULLDEBUG,"CCB: received 'success' from target daemon %s "
				"with ccbid %lu for "
				"request %s from %s.\n",
				sock->peer_description(),
				target->getCCBID(),
				reqid_str.Value(),
				request_desc);
	}
	else {
		dprintf(D_FULLDEBUG,"CCB: received error from target daemon %s "
				"with ccbid %lu for "
				"request %s from %s: %s\n",
				sock->peer_description(),
				target->getCCBID(),
				reqid_str.Value(),
				request_desc,
				error_msg.Value());
	}

	if( !request ) {
		if( success ) {
				// expected: the client has gone away; it got what it wanted
			return;
		}
		dprintf( D_FULLDEBUG,
				 "CCB: client for request %s to target daemon %s with ccbid "
				 "%lu disappeared before receiving error details.\n",
				 reqid_str.Value(),
				 sock->peer_description(),
				 target->getCCBID());
		return;
	}
	if( connect_id != request->getConnectID() ) {
		MyString msg_str;
		msg.sPrint(msg_str);
		dprintf( D_FULLDEBUG,
				 "CCB: received wrong connect id (%s) from target daemon %s "
				 "with ccbid %lu for "
				 "request %s\n",
				 connect_id.Value(),
				 sock->peer_description(),
				 target->getCCBID(),
				 reqid_str.Value());
		RemoveTarget( target );
		return;
	}

	RequestFinished( request, success, error_msg.Value() );
}

void
CCBServer::SendHeartbeatResponse( CCBTarget *target )
{
	Sock *sock = target->getSock();

	ClassAd msg;
	msg.Assign( ATTR_COMMAND, ALIVE );
	sock->encode();
	if( !msg.put( *sock ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to send heartbeat to target "
				"daemon %s with ccbid %lu\n",
				target->getSock()->peer_description(),
				target->getCCBID());

		RemoveTarget( target );
		return;
	}
	dprintf(D_FULLDEBUG,"CCB: sent heartbeat to target %s\n",
			sock->peer_description());
}

void
CCBServer::ForwardRequestToTarget( CCBServerRequest *request, CCBTarget *target )
{
	Sock *sock = target->getSock();

	ClassAd msg;
	msg.Assign( ATTR_COMMAND, CCB_REQUEST );
	msg.Assign( ATTR_MY_ADDRESS, request->getReturnAddr() );
	msg.Assign( ATTR_CLAIM_ID, request->getConnectID() );
	// for easier debugging
	msg.Assign( ATTR_NAME, request->getSock()->peer_description() );

	MyString reqid_str;
	CCBIDToString( request->getRequestID(), reqid_str);
	msg.Assign( ATTR_REQUEST_ID, reqid_str );

	sock->encode();
	if( !msg.put( *sock ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to forward request id %lu from %s to target "
				"daemon %s with ccbid %lu\n",
				request->getRequestID(),
				request->getSock()->peer_description(),
				target->getSock()->peer_description(),
				target->getCCBID());

		RequestFinished( request, false, "failed to forward request to target" );
		return;
	}

		// Now wait for target to respond (HandleRequestResultsMsg).
		// We will get the response next time we poll the socket.
		// To get a faster response, we _could_ register the socket
		// now, if it has not already been registered.
}

void
CCBServer::RequestReply( Sock *sock, bool success, char const *error_msg, CCBID request_cid, CCBID target_cid )
{
	if( success && sock->readReady() ) {
			// the client must have disconnected (which is expected if
			// the client has already received the reversed connection)
		return;
	}

	ClassAd msg;
	msg.Assign( ATTR_RESULT, success );
	msg.Assign( ATTR_ERROR_STRING, error_msg );

	sock->encode();
	if( !msg.put( *sock ) || !sock->end_of_message() ) {
			// Would like to be completely quiet if success and the
			// client has disconnected, since this is normal; however,
			// the above write operations will generate noise when
			// they fail, so at least in FULLDEBUG, we explain what's
			// going on.  Note that most of the time, we should not get
			// here for successful requests, because we either observe
			// the client disconnect earlier, or the above check on
			// the socket catches it.  Why bother sending a reply on
			// success at all?  Because if the client has not yet
			// seen the reverse connect and we just disconnect without
			// telling it the request was successful, then it will
			// think something has gone wrong.
		dprintf(success ? D_FULLDEBUG : D_ALWAYS,
				"CCB: failed to send result (%s) for request id %lu "
				"from %s requesting a reversed connection to target daemon "
				"with ccbid %lu: %s %s\n",
				success ? "request succeeded" : "request failed",
				request_cid,
				sock->peer_description(),
				target_cid,
				error_msg,
				success ? "(since the request was successful, it is expected "
				          "that the client may disconnect before receiving "
				          "results)" : "" );
	}
}

void
CCBServer::RequestFinished( CCBServerRequest *request, bool success, char const *error_msg )
{
	RequestReply(
		request->getSock(),
		success,
		error_msg,
		request->getRequestID(),
		request->getTargetCCBID() );

	RemoveRequest( request );
}

CCBServerRequest *
CCBServer::GetRequest( CCBID request_id )
{
	CCBServerRequest *result = NULL;
	if( m_requests.lookup(request_id,result) == -1 ) {
		return NULL;
	}
	return result;
}

CCBTarget *
CCBServer::GetTarget( CCBID ccbid )
{
	CCBTarget *result = NULL;
	if( m_targets.lookup(ccbid,result) == -1 ) {
		return NULL;
	}
	return result;
}

bool
CCBServer::ReconnectTarget( CCBTarget *target, CCBID reconnect_cookie )
{
	CCBReconnectInfo *reconnect_info = GetReconnectInfo(target->getCCBID());
	if( !reconnect_info ) {
		dprintf(D_ALWAYS,
				"CCB: reconnect request from target daemon %s with ccbid %lu"
				", but this ccbid has no reconnect info!\n",
				target->getSock()->peer_description(),
				target->getCCBID());
		return false;
	}

	char const *previous_ip = reconnect_info->getPeerIP();
	char const *new_ip = target->getSock()->peer_ip_str();
	if( strcmp(previous_ip,new_ip) )
	{
		dprintf(D_ALWAYS,
				"CCB: reconnect request from target daemon %s with ccbid %lu "
				"has wrong IP!  (expected IP=%s)\n",
				target->getSock()->peer_description(),
				target->getCCBID(),
				previous_ip);
		return false;
	}

	if( reconnect_cookie != reconnect_info->getReconnectCookie() )
	{
		dprintf(D_ALWAYS,
				"CCB: reconnect request from target daemon %s with ccbid %lu "
				"has wrong cookie!  (cookie=%lu)\n",
				target->getSock()->peer_description(),
				target->getCCBID(),
				reconnect_cookie);
		return false;
	}

	reconnect_info->alive();

	CCBTarget *existing = NULL;
	if( m_targets.lookup(target->getCCBID(),existing) == 0 ) {
		// perhaps we haven't noticed yet that this existing target socket
		// has become disconnected; get rid of it
		dprintf(D_ALWAYS,
				"CCB: disconnecting existing connection from target daemon "
				"%s with ccbid %lu because this daemon is reconnecting.\n",
				existing->getSock()->peer_description(),
				target->getCCBID());
		RemoveTarget( existing );
	}

	ASSERT( m_targets.insert(target->getCCBID(),target) == 0 );

	dprintf(D_FULLDEBUG,"CCB: reconnected target daemon %s with ccbid %lu\n",
			target->getSock()->peer_description(),
			target->getCCBID());

	return true;
}

void
CCBServer::AddTarget( CCBTarget *target )
{
		// in case of wrap-around in ccbid, handle conflicts
	while(true) {
		target->setCCBID(m_next_ccbid++);

		CCBReconnectInfo *reconnect_info=GetReconnectInfo(target->getCCBID());
		if( reconnect_info ) {
			// do not reuse this CCBID, because we are waiting for a reconnect
			continue;
		}

		if( m_targets.insert(target->getCCBID(),target) == 0 ) {
			break; // success
		}

		CCBTarget *existing = NULL;
		if( m_targets.lookup(target->getCCBID(),existing) != 0 ) {
				// That's odd: there is no conflicting ccbid, so why did
				// the insert fail?!
			EXCEPT( "CCB: failed to insert registered target ccbid %lu "
					"for %s\n",
					target->getCCBID(),
					target->getSock()->peer_description());
		}
		// else this ccbid is already taken, so try again
	}

	// generate reconnect info for this new target daemon so that it
	// can reclaim its CCBID
	CCBID reconnect_cookie = get_random_uint();
	CCBReconnectInfo *reconnect_info = new CCBReconnectInfo(
		target->getCCBID(),
		reconnect_cookie,
		target->getSock()->peer_ip_str());
	AddReconnectInfo( reconnect_info );
	SaveReconnectInfo( reconnect_info );

	dprintf(D_FULLDEBUG,"CCB: registered target daemon %s with ccbid %lu\n",
			target->getSock()->peer_description(),
			target->getCCBID());
}

void
CCBServer::RemoveTarget( CCBTarget *target )
{
		// hang up on all requests for this target
	HashTable<CCBID,CCBServerRequest *> *trequests;
	while( (trequests = target->getRequests()) ) {
		CCBServerRequest *request = NULL;
		trequests->startIterations();
		if( trequests->iterate(request) ) {
			RemoveRequest( request );
			// note that trequests may point to a deleted hash table
			// at this point, so do not reference it anymore
		}
		else {
			break;
		}
	}

	if( m_targets.remove(target->getCCBID()) != 0 ) {
		EXCEPT("CCB: failed to remove target ccbid=%lu, %s",
			   target->getCCBID(), target->getSock()->peer_description());
	}

	dprintf(D_FULLDEBUG,"CCB: unregistered target daemon %s with ccbid %lu\n",
			target->getSock()->peer_description(),
			target->getCCBID());

	delete target;
}

void
CCBServer::AddRequest( CCBServerRequest *request, CCBTarget *target )
{
		// in case of wrap-around in ccbid, handle conflicts
	while(true) {
		request->setRequestID(m_next_request_id++);

		if( m_requests.insert(request->getRequestID(),request) == 0 ) {
			break; // success
		}

		CCBServerRequest *existing = NULL;
		if( m_requests.lookup(request->getRequestID(),existing) != 0 ) {
				// That's odd: there is no conflicting id, so why did
				// the insert fail?!
			EXCEPT( "CCB: failed to insert request id %lu "
					"for %s\n",
					request->getRequestID(),
					request->getSock()->peer_description());
		}
		// else this ccbid is already taken, so try again
	}

		// add this request to the list of requests waiting for the target
	target->AddRequest( request, this );

	int rc = daemonCore->Register_Socket (
		request->getSock(),
		request->getSock()->peer_description(),
		(SocketHandlercpp)&CCBServer::HandleRequestDisconnect,
		"CCBServer::HandleRequestDisconnect",
		this );

	ASSERT( rc >= 0 );
	rc = daemonCore->Register_DataPtr(request);
	ASSERT( rc );
}

int
CCBServer::HandleRequestDisconnect( Stream * /*stream*/ )
{
	CCBServerRequest *request = (CCBServerRequest *)daemonCore->GetDataPtr();
	RemoveRequest( request );
	return KEEP_STREAM;
}


void
CCBServer::RemoveRequest( CCBServerRequest *request )
{
	daemonCore->Cancel_Socket( request->getSock() );

	if( m_requests.remove(request->getRequestID()) != 0 ) {
		EXCEPT("CCB: failed to remove request id=%lu from %s for ccbid %lu",
			   request->getRequestID(),
			   request->getSock()->peer_description(),
			   request->getTargetCCBID());
	}

		// remove this request from the list of requests waiting for the target
	CCBTarget *target = GetTarget( request->getTargetCCBID() );
	if( target ) {
		target->RemoveRequest( request );
	}

	dprintf(D_FULLDEBUG,
			"CCB: removed request id=%lu from %s for ccbid %lu\n",
			request->getRequestID(),
			request->getSock()->peer_description(),
			request->getTargetCCBID());

	delete request;
}

CCBTarget::CCBTarget(Sock *sock):
	m_sock(sock),
	m_ccbid(-1),
	m_pending_request_results(0),
	m_socket_is_registered(false),
	m_requests(NULL)
{
}

CCBTarget::~CCBTarget()
{
	if( m_socket_is_registered ) {
		daemonCore->Cancel_Socket( m_sock );
	}
	if( m_sock ) {
		delete m_sock;
	}
	if( m_requests ) {
		delete m_requests;
	}
}

void
CCBTarget::incPendingRequestResults(CCBServer *ccb_server)
{
	m_pending_request_results++;

	if( !m_socket_is_registered ) {
		// It is not essential that we register the target socket,
		// because we also poll all target sockets periodically.
		// However, while there are outstanding requests (and hence
		// expectation of a reply from the target), we register
		// the target socket just to reduce chances of a busy
		// target having its incoming buffers fill up, etc.

		int rc = daemonCore->Register_Socket (
			m_sock,
			m_sock->peer_description(),
			(SocketHandlercpp)(int (CCBServer::*)(Stream*))&CCBServer::HandleRequestResultsMsg,
			"CCBServer::HandleRequestResultsMsg",
			ccb_server );

		ASSERT( rc >= 0 );
		rc = daemonCore->Register_DataPtr(this);
		ASSERT( rc );

		m_socket_is_registered = true;
	}
}

void
CCBTarget::decPendingRequestResults()
{
	m_pending_request_results--;

	if( m_pending_request_results <= 0 ) {
		if( m_socket_is_registered ) {
			// not expecting any more results, so go back into
			// slow polling mode for this target
			m_socket_is_registered = false;
			daemonCore->Cancel_Socket( m_sock );
		}
	}
}

void
CCBTarget::AddRequest(CCBServerRequest *request, CCBServer *ccb_server)
{
	incPendingRequestResults(ccb_server);

	if( !m_requests ) {
		m_requests = new HashTable<CCBID,CCBServerRequest *>(ccbid_hash);
		ASSERT( m_requests );
	}
	int rc = m_requests->insert(request->getRequestID(),request);
	ASSERT( rc == 0 );
}

void
CCBTarget::RemoveRequest(CCBServerRequest *request)
{
	if( m_requests ) {
		m_requests->remove( request->getRequestID() );

		if( m_requests->getNumElements() == 0 ) {
			delete m_requests;
			m_requests = NULL;
		}
	}
}

CCBServerRequest::CCBServerRequest(Sock *sock,CCBID target_ccbid,char const *return_addr,char const *connect_id):
	m_sock(sock),
	m_target_ccbid(target_ccbid),
	m_request_id(-1),
	m_return_addr(return_addr),
	m_connect_id(connect_id)
{
}

CCBServerRequest::~CCBServerRequest()
{
	if( m_sock ) {
		delete m_sock;
	}
}

CCBReconnectInfo::CCBReconnectInfo(CCBID ccbid,CCBID reconnect_cookie,char const *peer_ip):
	m_ccbid(ccbid),
	m_reconnect_cookie(reconnect_cookie)
{
	m_last_alive = time(NULL);
	strncpy(m_peer_ip,peer_ip,IP_STRING_BUF_SIZE);
	m_peer_ip[IP_STRING_BUF_SIZE-1] = '\0';
}

CCBReconnectInfo *
CCBServer::GetReconnectInfo(CCBID ccbid)
{
	CCBReconnectInfo *result = NULL;
	if( m_reconnect_info.lookup(ccbid,result) == -1 ) {
		return NULL;
	}
	return result;
}

void
CCBServer::AddReconnectInfo( CCBReconnectInfo *reconnect_info )
{
	if( m_reconnect_info.insert(reconnect_info->getCCBID(),reconnect_info) == 0 ) {
		return;
	}

	ASSERT( m_reconnect_info.remove(reconnect_info->getCCBID()) == 0 );
	ASSERT( m_reconnect_info.insert(reconnect_info->getCCBID(),reconnect_info) == 0);
}

void
CCBServer::RemoveReconnectInfo( CCBReconnectInfo *reconnect_info )
{
	ASSERT( m_reconnect_info.remove(reconnect_info->getCCBID()) == 0 );
	delete reconnect_info;
}

void
CCBServer::CloseReconnectFile()
{
	if( m_reconnect_fp ) {
		fclose(m_reconnect_fp);
		m_reconnect_fp = NULL;
	}
}

bool
CCBServer::OpenReconnectFileIfExists()
{
	return OpenReconnectFile(true);
}

bool
CCBServer::OpenReconnectFile(bool only_if_exists)
{
	if( m_reconnect_fp ) {
		return true;
	}
	if( m_reconnect_fname.IsEmpty() ) {
		return false;
	}
	if( !only_if_exists ) {
		m_reconnect_fp = safe_fcreate_fail_if_exists(m_reconnect_fname.Value(),"w+",0600);
	}
	if( !m_reconnect_fp ) {
		m_reconnect_fp = safe_fopen_no_create(m_reconnect_fname.Value(),"r+");
	}
	if( !m_reconnect_fp ) {
		if( only_if_exists && errno == ENOENT ) {
			return false;
		}
		EXCEPT("CCB: Failed to open %s: %s\n",
			   m_reconnect_fname.Value(),strerror(errno));
	}
	return true;
}

void
CCBServer::LoadReconnectInfo()
{
	if( !OpenReconnectFileIfExists() ) {
		return;
	}

	rewind(m_reconnect_fp);
	char buf[128];
	unsigned long line = 0;
	while( fgets(buf,sizeof(buf),m_reconnect_fp) ) {
		line++;
		buf[sizeof(buf)-1] = '\0';

		CCBID ccbid,cookie;
		char ip[128],ccbid_str[128],cookie_str[128];
		ip[127] = ccbid_str[127] = cookie_str[127] = '\0';
		if( sscanf(buf,"%127s %127s %127s",ip,ccbid_str,cookie_str)!=3 ||
			!CCBIDFromString( ccbid, ccbid_str) ||
			!CCBIDFromString( cookie, cookie_str) )
		{
			dprintf(D_ALWAYS,"CCB: ERROR: line %lu is invalid in %s.", line,
					m_reconnect_fname.Value());
			continue;
		}

		if( ccbid > m_next_ccbid ) {
			m_next_ccbid = ccbid+1;
		}

		CCBReconnectInfo *reconnect_info = new CCBReconnectInfo(ccbid,cookie,ip);
		AddReconnectInfo( reconnect_info );
	}

	// In case any reconnect records were not committed to disk in time
	// before we restarted, jump ahead a bit to avoid handing out CCBIDs
	// that may have been recently assigned.
	m_next_ccbid += 100;

	dprintf(D_ALWAYS,"CCB: loaded %d reconnect records from %s.\n",
			m_reconnect_info.getNumElements(), m_reconnect_fname.Value());
}

bool
CCBServer::SaveReconnectInfo(CCBReconnectInfo *reconnect_info)
{
	if( !OpenReconnectFile() ) {
		return false;
	}

	int rc = fseek(m_reconnect_fp,0,SEEK_END);
	if( rc == -1 ) {
		dprintf(D_ALWAYS,"CCB: failed to seek to end of %s: %s\n",
				m_reconnect_fname.Value(), strerror(errno));
		return false;
	}

	MyString ccbid_str,cookie_str;
	rc = fprintf(m_reconnect_fp,"%s %s %s\n",
		reconnect_info->getPeerIP(),
		CCBIDToString(reconnect_info->getCCBID(),ccbid_str),
		CCBIDToString(reconnect_info->getReconnectCookie(),cookie_str));
	if( rc == -1 ) {
		dprintf(D_ALWAYS,"CCB: failed to write reconnect info in %s: %s\n",
				m_reconnect_fname.Value(), strerror(errno));
		return false;
	}
	return true;
}

void
CCBServer::SaveAllReconnectInfo()
{
	if( m_reconnect_fname.IsEmpty() ) {
		return;
	}
	CloseReconnectFile();

	if( m_reconnect_info.getNumElements()==0 ) {
		remove( m_reconnect_fname.Value() );
		return;
	}

	MyString orig_reconnect_fname = m_reconnect_fname;
	m_reconnect_fname.formatstr_cat(".new");

	if( !OpenReconnectFile() ) {
		m_reconnect_fname = orig_reconnect_fname;
		return;
	}

	CCBReconnectInfo *reconnect_info=NULL;
	m_reconnect_info.startIterations();
	while( m_reconnect_info.iterate(reconnect_info) ) {
		if( !SaveReconnectInfo(reconnect_info) ) {
			CloseReconnectFile();
			m_reconnect_fname = orig_reconnect_fname;
			dprintf(D_ALWAYS,"CCB: aborting rewriting of %s\n",
					m_reconnect_fname.Value());
			return;
		}
	}

	CloseReconnectFile();
	int rc;
	rc = rotate_file( m_reconnect_fname.Value(),orig_reconnect_fname.Value() );
	if( rc < 0 ) {
		dprintf(D_ALWAYS,"CCB: failed to rotate rewritten %s\n",
				m_reconnect_fname.Value());
	}
	m_reconnect_fname = orig_reconnect_fname;
}

void
CCBServer::SweepReconnectInfo()
{
	time_t now = time(NULL);

	if( m_reconnect_fp ) {
		// flush writes to the reconnect file periodically
		fflush( m_reconnect_fp );
	}

	if( m_last_reconnect_info_sweep + m_reconnect_info_sweep_interval > now )
	{
		return;
	}
	m_last_reconnect_info_sweep = now;

	// Now it is time to delete expired reconnect records

	CCBReconnectInfo *reconnect_info=NULL;
	CCBTarget *target=NULL;

	m_targets.startIterations();
	while( m_targets.iterate(target) ) {
		reconnect_info = GetReconnectInfo(target->getCCBID());
		ASSERT( reconnect_info );
		reconnect_info->alive();
	}

	unsigned long removed = 0;
	m_reconnect_info.startIterations();
	while( m_reconnect_info.iterate(reconnect_info) ) {
		time_t last = reconnect_info->getLastAlive();
		if( now - last > 2*m_reconnect_info_sweep_interval ) {
			RemoveReconnectInfo( reconnect_info );
			removed++;
		}
	}

	if( removed ) {
		dprintf(D_ALWAYS,
				"CCB: pruning %lu expired reconnect records.\n",removed);

		// rewrite the file to save space, since some records were deleted
		SaveAllReconnectInfo();
	}
}
