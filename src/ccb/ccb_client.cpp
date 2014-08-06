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
#include "condor_crypt.h"
#include "subsystem_info.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "selector.h"
#include "CondorError.h"
#include "ccb_client.h"
#include "condor_sinful.h"
#include "shared_port_endpoint.h"

static bool registered_reverse_connect_command = false;

// hash of CCBClients waiting for a reverse connect command
// indexed by connection id
static HashTable< MyString,classy_counted_ptr<CCBClient> > waiting_for_reverse_connect(MyStringHash);



CCBClient::CCBClient( char const *ccb_contact, ReliSock *target_sock ):
	m_ccb_contact(ccb_contact),
	m_ccb_contacts(ccb_contact," "),
	m_target_sock(target_sock),
	m_target_peer_description(m_target_sock->peer_description()),
	m_ccb_sock(NULL),
	m_ccb_cb(NULL),
	m_deadline_timer(-1)
{
	// balance load across the CCB servers by randomizing order
	m_ccb_contacts.shuffle();

	// Generate some random bits for the connection id.  In a
	// reverse-connect operation, the target daemon must present this
	// connect id to us as a sign that it truly is the daemon
	// registered at the CCB server with the CCBID we desire.  We
	// don't really have to depend on the strength of this, because
	// after the reverse-connect happens, the normal condor security
	// protocol happens, so the two sides should be happy with each
	// other.  However, it is common for the client side to trust the
	// server side, so this extra check is a reasonable precaution.

	const size_t keylen = 20;
	unsigned char *keybuf = Condor_Crypt_Base::randomKey(keylen);
	size_t i;
	for(i=0;i<keylen;i++) {
		m_connect_id.formatstr_cat("%02x",keybuf[i]);
	}
	free( keybuf );
}

CCBClient::~CCBClient()
{
	if( m_ccb_sock ) {
		delete m_ccb_sock;
	}
	if( m_deadline_timer != -1 ) {
		daemonCore->Cancel_Timer(m_deadline_timer);
		m_deadline_timer = -1;
	}
}


bool
CCBClient::ReverseConnect( CondorError *error, bool non_blocking )
{
	if( non_blocking ) {
		// Non-blocking mode requires DaemonCore
		if ( !daemonCore ) {
			dprintf( D_ALWAYS, "Can't do non-blocking CCB reverse connection without DaemonCore!\n" );
			return false;
		}
		m_target_sock->enter_reverse_connecting_state();
		// NOTE: now we _must_ call exit_reverse_connecting_state()
		// before we are done with m_target_sock.  This state
		// protects against the socket being deleted before we
		// are done referencing the socket.  It also makes the
		// socket appear as though it is waiting for a non-blocking
		// connect operation to finish, so our caller can call
		// DaemonCore::Register_Socket() and wait for a callback.

		m_ccb_contacts.rewind();
		return try_next_ccb();
	}

	return ReverseConnect_blocking( error );
}

MyString
CCBClient::myName()
{
	// This is purely for debugging purposes.
	// It is who we say we are when talking to the CCB server.
	MyString name;
	name = get_mySubSystem()->getName();
	if( daemonCore ) {
		name += " ";
		name += daemonCore->publicNetworkIpAddr();
	}
	return name;
}

bool
CCBClient::ReverseConnect_blocking( CondorError *error )
{
	counted_ptr<ReliSock> listen_sock;
	counted_ptr<SharedPortEndpoint> shared_listener;
	char const *listener_addr = NULL;

	if( SharedPortEndpoint::UseSharedPort() ) {
		shared_listener = counted_ptr<SharedPortEndpoint>(new SharedPortEndpoint());
		shared_listener->InitAndReconfig();
		MyString errmsg;
		if( !shared_listener->CreateListener() ) {
			errmsg.formatstr("Failed to create shared port endpoint for reversed connection from %s.",
						   m_target_peer_description.Value());
		}
		else if( !(listener_addr = shared_listener->GetMyRemoteAddress()) ) {
			errmsg.formatstr("Failed to get remote address for shared port endpoint for reversed connection from %s.",
						   m_target_peer_description.Value());
		}
		if( !listener_addr ) {
			if( error ) {
				error->push("CCBClient", CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
			}
			dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());
			return false;
		}
	}
	else {
		listen_sock = counted_ptr<ReliSock>(new ReliSock());
		listen_sock->bind(false,0);
		if( !listen_sock->listen() ) {
			MyString errmsg;
			errmsg.formatstr("Failed to listen for reversed connection from %s.",
						   m_target_peer_description.Value());
			if( error ) {
				error->push("CCBClient", CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
			}
			dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());

			return false;
		}
		listener_addr = listen_sock->get_sinful_public();
	}
	ASSERT( listener_addr );

	m_ccb_contacts.rewind();
	char const *ccb_contact;
	while( (ccb_contact = m_ccb_contacts.next()) ) {
		bool success = false;
		MyString ccb_address, ccbid;
		if( !SplitCCBContact( ccb_contact, ccb_address, ccbid, error ) ) {
			continue;
		}

		ClassAd msg;
		msg.Assign(ATTR_CCBID,ccbid);
		msg.Assign(ATTR_CLAIM_ID,m_connect_id);
		// purely for debugging purposes: identify ourselves
		msg.Assign(ATTR_NAME, myName());
		msg.Assign(ATTR_MY_ADDRESS, listener_addr);

		dprintf(D_NETWORK|D_FULLDEBUG,
				"CCBClient: requesting reverse connection to %s "
				"via CCB server %s#%s; I am listening at %s.\n",
				m_target_peer_description.Value(),
				ccb_address.Value(),
				ccbid.Value(),
				listener_addr);

		Daemon ccb(DT_COLLECTOR,ccb_address.Value(),NULL);
		if( m_ccb_sock ) {
			delete m_ccb_sock;
		}
		m_ccb_sock = ccb.startCommand(
			CCB_REQUEST,
			Stream::reli_sock,
			DEFAULT_CEDAR_TIMEOUT,
			error);

		if( !m_ccb_sock ) {
			continue;
		}

		m_ccb_sock->encode();
		if( !putClassAd( m_ccb_sock, msg ) || !m_ccb_sock->end_of_message() ) {
			if( error ) {
				error->pushf(
					"CCBClient",
					CEDAR_ERR_CONNECT_FAILED,
					"Failed to write request to CCB server %s.",
					ccb_address.Value());
			}
		}

		// now wait for either a connection on m_target_sock or
		// a reply on m_ccb_sock

		Selector selector;
		int listen_fd = -1;
		if( shared_listener.get() ) {
			shared_listener->AddListenerToSelector(selector);
		}
		else {
			listen_fd = listen_sock->get_file_desc();
			selector.add_fd( listen_fd, Selector::IO_READ );
		}
		int ccb_fd = m_ccb_sock->get_file_desc();
		selector.add_fd( ccb_fd, Selector::IO_READ );

		time_t start_time = time(NULL);
		int timeout = m_target_sock->get_timeout_raw();
		time_t deadline = m_target_sock->get_deadline();
		if( deadline && deadline-start_time < timeout ) {
			timeout = deadline-start_time;
			if( timeout <= 0 ) {
				timeout = 1;
			}
		}
		while( ccb_fd != -1 || listen_fd != -1 || shared_listener.get() ) {
			bool timed_out = false;

			if( timeout ) {
				int elapsed = time(NULL) - start_time;
				if( elapsed >= timeout ) {
					timed_out = true;
				}

				selector.set_timeout( timeout - elapsed );
			}

			if( !timed_out ) {
				selector.execute();
				timed_out = selector.timed_out();
			}

			if( timed_out ) {
				MyString errmsg;
				errmsg.formatstr(
					"Timed out waiting for response after requesting reversed "
					"connection from %s ccbid %s via CCB server %s.",
					m_target_peer_description.Value(),
					ccbid.Value(),
					ccb_address.Value());

				if( error ) {
					error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
				}
				else {
					dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());
				}

				break; // try next ccb server
			}

			if( (listen_fd != -1 &&
				 selector.fd_ready(listen_fd,Selector::IO_READ)) ||
				(shared_listener.get() &&
				 shared_listener->CheckListenerReady(selector)) )
			{
				if( AcceptReversedConnection( listen_sock, shared_listener ) ) {
					if( listen_fd != -1 ) {
						selector.delete_fd( listen_fd, Selector::IO_READ );
						listen_fd = -1;
						listen_sock->close();
					}
					if( shared_listener.get() ) {
						shared_listener->RemoveListenerFromSelector(selector);
							// destruct the shared port endpoint
						shared_listener = counted_ptr<SharedPortEndpoint>(NULL);
					}

					success = true;

						// do not wait for CCB server to send us 'success'
						// message if it has not already been sent
					break;
				}
				// else continue to accept connections
			}

			if( ccb_fd != -1 &&
				selector.fd_ready(ccb_fd,Selector::IO_READ))
			{
				selector.delete_fd( ccb_fd, Selector::IO_READ );
				ccb_fd = -1;

				if( !HandleReversedConnectionRequestReply(error)) {
					break; // failure; try next ccb server
				}
			}
		}

		if( success ) {
			return true;
		}
	}
	return false;
}

bool CCBClient::SplitCCBContact( char const *ccb_contact, MyString &ccb_address, MyString &ccbid, CondorError *error )
{
	// expected format: "<address>#ccbid"
	char const *ptr = strchr(ccb_contact,'#');
	if( !ptr ) {
		MyString errmsg;
		errmsg.formatstr("Bad CCB contact '%s' when connecting to %s.",
					   ccb_contact, m_target_peer_description.Value());

		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
		}
		else {
			dprintf(D_ALWAYS,"%s\n",errmsg.Value());
		}
		return false;
	}
	ccb_address = ccb_contact;
	ccb_address.setChar(ptr-ccb_contact,'\0');
	ccbid = ptr+1;
	return true;
}

bool
CCBClient::AcceptReversedConnection(counted_ptr<ReliSock> listen_sock,counted_ptr<SharedPortEndpoint> shared_listener)
{
	// This happens when we are in ReverseConnect_blocking().
	// and our listen socket becomes readable, indicating that
	// (maybe) the daemon we want to talk to has connected to it.

	m_target_sock->close();
	if( shared_listener.get() ) {
		shared_listener->DoListenerAccept( m_target_sock );
		if( !m_target_sock->is_connected() ) {
			dprintf(D_ALWAYS,
					"CCBClient: failed to accept() reversed connection "
					"via shared port (intended target is %s)\n",
					m_target_peer_description.Value());
			return false;
		}
	}
	else if( !listen_sock->accept( m_target_sock ) ) {
		dprintf(D_ALWAYS,
				"CCBClient: failed to accept() reversed connection "
				"(intended target is %s)\n",
				m_target_peer_description.Value());
		return false;
	}

	int cmd;
	ClassAd msg;

	m_target_sock->decode();
	if( !m_target_sock->get(cmd) ||
		!getClassAd( m_target_sock, msg ) ||
		!m_target_sock->end_of_message() )
	{
		dprintf(D_ALWAYS,
				"CCBClient: failed to read hello message from reversed "
				"connection %s (intended target is %s)\n",
				m_target_sock->default_peer_description(),
				m_target_peer_description.Value());
		m_target_sock->close();
		return false;
	}

	MyString connect_id;
	msg.LookupString(ATTR_CLAIM_ID,connect_id);

	if( cmd != CCB_REVERSE_CONNECT || connect_id != m_connect_id ) {
		dprintf(D_ALWAYS,
				"CCBClient: invalid hello message from reversed "
				"connection %s (intended target is %s)\n",
				m_target_sock->default_peer_description(),
				m_target_peer_description.Value());
		m_target_sock->close();
		return false;
	}

	dprintf(D_FULLDEBUG|D_NETWORK,
			"CCBClient: received reversed connection %s "
			"(intended target is %s)\n",
			m_target_sock->default_peer_description(),
			m_target_peer_description.Value());

	m_target_sock->isClient(true);
	return true;
}

bool
CCBClient::HandleReversedConnectionRequestReply(CondorError *error)
{
	// This happens when we are in ReverseConnect_blocking()
	// and the socket upon which we sent our request to the
	// CCB server becomes readable, indicating that the server
	// is sending us back details such as errors that prevented
	// the connection from being formed.

	ClassAd msg;
	bool result = false;
	MyString errmsg;

	m_ccb_sock->decode();
	if( !getClassAd(m_ccb_sock, msg) || !m_ccb_sock->end_of_message() ) {
		errmsg.formatstr("Failed to read response from CCB server "
					   "%s when requesting reversed connection to %s",
					   m_ccb_sock->peer_description(),
					   m_target_peer_description.Value());
		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
		}
		else {
			dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());
		}
		return false;
	}

	msg.LookupBool(ATTR_RESULT,result);
	if( !result ) {
		MyString remote_errmsg;
		msg.LookupString(ATTR_ERROR_STRING,remote_errmsg);

		errmsg.formatstr(
			"received failure message from CCB server %s in response to "
			"request for reversed connection to %s: %s",
			m_ccb_sock->peer_description(),
			m_target_peer_description.Value(),
			remote_errmsg.Value());

		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
		}
		else {
			dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());
		}
	}
	else {
		dprintf(D_FULLDEBUG|D_NETWORK,
				"CCBClient: received 'success' in reply from CCB server %s "
				"in response to request for reversed connection to %s\n",
				m_ccb_sock->peer_description(),
				m_target_peer_description.Value());
	}

	return result;
}

// the CCB request protocol is to send and receive a ClassAd
class CCBRequestMsg: public ClassAdMsg {
public:
	CCBRequestMsg(ClassAd &msg):
		ClassAdMsg(CCB_REQUEST,msg)
	{
	}

	virtual MessageClosureEnum messageSent(
				DCMessenger *messenger, Sock *sock )
	{
		// we have sent the request, now wait for the response
		messenger->startReceiveMsg(this,sock);
		return MESSAGE_CONTINUING;
	}
};

bool
CCBClient::try_next_ccb()
{
	// This function is called in ReverseConnect_nonblocking()
	// initially and whenever we should try the next CCB server in the
	// list (e.g. because all previous ones have failed).
	// Returns true if non-blocking operation successfully initiated.

	RegisterReverseConnectCallback();

	char const *ccb_contact = m_ccb_contacts.next();
	if( !ccb_contact ) {
		dprintf(D_ALWAYS,
				"CCBClient: no more CCB servers to try for requesting "
				"reversed connection to %s; giving up.\n",
				m_target_peer_description.Value());
		ReverseConnectCallback(NULL);
		return false;
	}

	MyString ccbid;
	if( !SplitCCBContact( ccb_contact, m_cur_ccb_address, ccbid, NULL ) ) {
		return try_next_ccb();
	}

	char const *return_address = daemonCore->publicNetworkIpAddr();

	// For now, we require that this daemon has a command port.
	// If needed, we could add support for opening a listen socket here.
	ASSERT( return_address && *return_address );

	Sinful sinful_return(return_address);
	if( sinful_return.getCCBContact() ) {
		// uh oh!  Our return address is via CCB.
		dprintf(D_ALWAYS,
				"CCBClient: WARNING: trying to connect to %s via CCB, but "
				"this appears to be a connection from one private network "
				"to another, which is not supported by CCB.  Either that, "
				"or you have not configured the private network name "
				"to be the same in these two networks when it really should "
				"be.  Assuming the latter.\n",
				m_target_peer_description.Value());

		// strip off CCB contact info in the return address
		sinful_return.setCCBContact(NULL);
		return_address = sinful_return.getSinful();
	}

	dprintf(D_NETWORK|D_FULLDEBUG,
			"CCBClient: requesting reverse connection to %s "
			"via CCB server %s#%s; "
			"I am listening on my command socket %s.\n",
			m_target_peer_description.Value(),
			m_cur_ccb_address.Value(),
			ccbid.Value(),
			return_address);

	classy_counted_ptr<Daemon> ccb_server = new Daemon(DT_COLLECTOR,m_cur_ccb_address.Value(),NULL);

	ClassAd ad;
	ad.Assign(ATTR_CCBID,ccbid);
	ad.Assign(ATTR_CLAIM_ID,m_connect_id);
	// purely for debugging purposes: identify ourselves
	ad.Assign(ATTR_NAME, myName());
	ad.Assign(ATTR_MY_ADDRESS, return_address);

	classy_counted_ptr<CCBRequestMsg> msg = new CCBRequestMsg(ad);

	incRefCount(); // do not delete self before CCBResultsCallback
	m_ccb_cb = new DCMsgCallback(
	    (DCMsgCallback::CppFunction)&CCBClient::CCBResultsCallback,
	    this );
	msg->setCallback( m_ccb_cb );

	msg->setDeadlineTime( m_target_sock->get_deadline() );

	if( ccb_server->addr() && !strcmp(ccb_server->addr(),return_address) ) {
			// Special case: the CCB server is in the same process as
			// the CCB client.  Example where this happens: collector
			// sending DC_INVALIDATE messages to daemons that use this
			// collector as a CCB server.  The trick here is that
			// we do not want to block in authentication when talking
			// to ourself, so we bypass all that.  The following
			// assumes that the CCB messages exchanged between the client
			// and server are small enough to fit in the socket buffer
			// without blocking either side.
		dprintf(D_NETWORK|D_FULLDEBUG,"CCBClient: sending request to self.\n");
		ReliSock *client_sock = new ReliSock;
		ReliSock *server_sock = new ReliSock;

		if( !client_sock->connect_socketpair(*server_sock) ) {
			dprintf(D_ALWAYS,"CCBClient: connect_socket_pair() failed.\n");
			CCBResultsCallback(m_ccb_cb);
			return false;
		}

		classy_counted_ptr<DCMessenger> messenger=new DCMessenger(ccb_server);
			// messenger takes care of deleting client_sock when done
		messenger->writeMsg(msg.get(),client_sock);

			// bypass startCommand() and call the command handler directly
			// this call will take care of deleting server_sock when done
		daemonCore->CallCommandHandler(CCB_REQUEST,server_sock);
	}
	else {
		ccb_server->sendMsg(msg.get());
	}

	// now wait for CCBResultsCallback and/or ReverseConnectCallback
	return true;
}

void
CCBClient::CancelReverseConnect()
{
	if( daemonCore && m_target_sock ) {
		ReverseConnectCallback( NULL );
	}
}

void
CCBClient::CCBResultsCallback(DCMsgCallback *cb)
{
	// We get here when the CCB server sends us back a response
	// indicating success or failure in the (nonblocking) reverse
	// connection attempt.  We only really care about this in the case
	// of failure, so we can report what went wrong and give up
	// waiting for the connection to show up.

	ASSERT( cb );
	m_ccb_cb = NULL;
	if( cb->getMessage()->deliveryStatus() != DCMsg::DELIVERY_SUCCEEDED ) {
		// We failed to communicate with the CCB server, so we should
		// not expect to receive ReverseConnectCallback().
		UnregisterReverseConnectCallback();
		try_next_ccb();
	}
	else {
		ClassAd msg = ((ClassAdMsg *)cb->getMessage())->getMsgClassAd();
		bool result = false;
		MyString remote_errmsg;
		msg.LookupBool(ATTR_RESULT,result);
		msg.LookupString(ATTR_ERROR_STRING,remote_errmsg);

		if( !result ) {
			dprintf(D_ALWAYS,"CCBClient:"
				"received failure message from CCB server %s in response to "
				"(non-blocking) request for reversed connection to %s: %s\n",
				m_cur_ccb_address.Value(),
				m_target_peer_description.Value(),
				remote_errmsg.Value());
			UnregisterReverseConnectCallback();
			try_next_ccb();
		}
		else {
			dprintf(D_FULLDEBUG|D_NETWORK,
				"CCBClient: received 'success' in reply from CCB server %s "
				"in response to (non-blocking) request for reversed connection"
				" to %s\n",
				m_cur_ccb_address.Value(),
				m_target_peer_description.Value());

			// Nothing special to do, because all we care about in
			// 'success' case is ReverseConnectCallback(), which has
			// probably already been called by now.
		}
	}

	decRefCount(); // we called incRefCount() when registering this callback
}

void
CCBClient::RegisterReverseConnectCallback()
{
	// This function is called during ReverseConnect_nonblocking().

	// Why do we use DaemonCore's command socket to listen for the
	// reverse connection rather than creating one of our own (as
	// we do in the ReverseConnect_blocking case)?  Two reasons:
	// 1. DaemonCore already provides a non-blocking listener
	// 2. Once the sockect is connected, it will not have its
	// own port; it will be using the same port as the command
	// socket, thus cutting down on the number of ports we need.

	// Since there may be multiple CCBClients waiting for connections
	// all on the same command socket, we use the unique connection id
	// to tell them apart and to provide a simple check that the other
	// side of the connection is really the one we requested to talk
	// to.

	if( !registered_reverse_connect_command ) {
		registered_reverse_connect_command = true;

		// Register this command as ALLOW, because the real security
		// checks will be done later when we pretend that the
		// connection was created the other way around, with this
		// being the client end and the remote side being the server.
		// The other reason for ALLOW is that this allows the
		// command to be sent the way CCBListener does it, using the
		// "raw" command protocol (just a command int), with no
		// security session management.  Again, that stuff all happens
		// later in the reverse direction.

		daemonCore->Register_Command(
			CCB_REVERSE_CONNECT,
			"CCB_REVERSE_CONNECT",
			CCBClient::ReverseConnectCommandHandler,
			"CCBClient::ReverseConnectCommandHandler",
			NULL,
			ALLOW);
	}

	time_t deadline = m_target_sock->get_deadline();
	if( deadline == 0 ) {
			// Having no deadline at all is problematic.  We can end
			// up waiting forever.  This can happen when the target daemon
			// succeeds in its connect() operation to us but then we fail
			// to successfully process the reverse-connect command for some
			// reason (e.g. timeout reading the command).

			// Therefore, if no deadline was specified, set one now.
		deadline = time(NULL) + DEFAULT_SHORT_COMMAND_DEADLINE;
	}
	if( m_deadline_timer == -1 && deadline ) {
		int timeout = deadline - time(NULL) + 1;
		if( timeout < 0 ) {
			timeout = 0;
		}
		m_deadline_timer = daemonCore->Register_Timer (
			timeout,
			(TimerHandlercpp)&CCBClient::DeadlineExpired,
			"CCBClient::DeadlineExpired",
			this );
	}

	int rc = waiting_for_reverse_connect.insert( m_connect_id, this );
	ASSERT( rc == 0 );
}

void
CCBClient::DeadlineExpired()
{
	dprintf(D_ALWAYS,
			"CCBClient: deadline expired for reverse connection to %s.\n",
			m_target_peer_description.Value());

	m_deadline_timer = -1;
	CancelReverseConnect();
}

void
CCBClient::UnregisterReverseConnectCallback()
{
	if( m_deadline_timer != -1 ) {
		daemonCore->Cancel_Timer(m_deadline_timer);
		m_deadline_timer = -1;
	}

	// Remove ourselves from the list of waiting CCB clients.
	// Note that this could be removing the last reference
	// to this class, so it may be destructed as a result.
	int rc = waiting_for_reverse_connect.remove( m_connect_id );
	ASSERT( rc == 0 );
}

int
CCBClient::ReverseConnectCommandHandler(Service *,int cmd,Stream *stream)
{
	// This is a static function called when the command socket
	// receives a reverse connect command.  Our job is to direct
	// the call to the correct CCB client.

	ASSERT( cmd == CCB_REVERSE_CONNECT );

	ClassAd msg;
	if( !getClassAd(stream, msg) || !stream->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCBClient: failed to read reverse connection message from "
				"%s.\n", stream->peer_description());
		return FALSE;
	}

	// Now figure out which CCBClient is waiting for this connection.

	MyString connect_id;
	msg.LookupString(ATTR_CLAIM_ID,connect_id);

	classy_counted_ptr<CCBClient> client;
	int rc = waiting_for_reverse_connect.lookup(connect_id,client);
	if( rc < 0 ) {
		dprintf(D_ALWAYS,
				"CCBClient: failed to find requested connection id %s.\n",
				connect_id.Value());
		return FALSE;
	}
	client->ReverseConnectCallback((Sock *)stream);
	return KEEP_STREAM;
}

void
CCBClient::ReverseConnectCallback(Sock *sock)
{
	// This is called within ReverseConnect_nonblocking().
	// It is the final stage of the process, responsible for
	// attaching the reversed connection to m_target_sock
	// and letting it continue on its way as though this were
	// a normal non-blocking connection attempt that just
	// finished.

	ASSERT( m_target_sock );

	if( sock ) {
		dprintf(D_FULLDEBUG|D_NETWORK,
			"CCBClient: received reversed (non-blocking) connection %s "
			"(intended target is %s)\n",
			sock->peer_description(),
			m_target_peer_description.Value());
	}

	m_target_sock->exit_reverse_connecting_state((ReliSock*)sock);

	if( sock ) {
		delete sock;
	}

	// We are all done creating the connection (or failing to do so).
	// Call any function that may have been registered with
	// DaemonCore::Register_Socket().
	daemonCore->CallSocketHandler(m_target_sock,false);

	// No more use of m_target_sock, because it may have been deleted
	// by now.
	m_target_sock = NULL;

	if( m_ccb_cb ) {
		// still haven't gotten the response from the CCB server.
		// Don't care any longer, so cancel it.
		m_ccb_cb->cancelCallback();
		const bool quiet = true;
		m_ccb_cb->cancelMessage( quiet );
		decRefCount();
	}

	// The following removes what may be the final reference to
	// this class, which would result in its destruction.
	UnregisterReverseConnectCallback();
}
