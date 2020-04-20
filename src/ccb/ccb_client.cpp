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
static HashTable< std::string,classy_counted_ptr<CCBClient> > waiting_for_reverse_connect(hashFunction);



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
		formatstr_cat(m_connect_id,"%02x",keybuf[i]);
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

std::string
CCBClient::myName()
{
	// This is purely for debugging purposes.
	// It is who we say we are when talking to the CCB server.
	std::string name;
	name = get_mySubSystem()->getName();
	if( daemonCore && daemonCore->publicNetworkIpAddr() ) {
		name += " ";
		name += daemonCore->publicNetworkIpAddr();
	}
	return name;
}

bool
CCBClient::ReverseConnect_blocking( CondorError *error )
{
	std::shared_ptr<ReliSock> listen_sock;
	std::shared_ptr<SharedPortEndpoint> shared_listener;
	char const *listener_addr = NULL;

	m_ccb_contacts.rewind();
	char const *ccb_contact;
	while( (ccb_contact = m_ccb_contacts.next()) ) {
		bool success = false;
		std::string ccb_address, ccbid;
		if( !SplitCCBContact( ccb_contact, ccb_address, ccbid, m_target_peer_description, error ) ) {
			continue;
		}

		//
		// Generate a listen port for the appropriate protocol.  It will be
		// distressing that we're passed CCB contact strings rather than
		// sinfuls, when it's time to handle multiple addresses...
		//
		if( SharedPortEndpoint::UseSharedPort() ) {
			shared_listener = std::shared_ptr<SharedPortEndpoint>(new SharedPortEndpoint());
			shared_listener->InitAndReconfig();
			MyString errmsg;
			if( !shared_listener->CreateListener() ) {
				errmsg.formatstr( "Failed to create shared port endpoint for reversed connection from %s.",
					m_target_peer_description.c_str() );
			}
			else if( !(listener_addr = shared_listener->GetMyRemoteAddress()) ) {
				errmsg.formatstr( "Failed to get remote address for shared port endpoint for reversed connection from %s.",
					m_target_peer_description.c_str() );
			}
			if( !listener_addr ) {
				if( error ) {
					error->push( "CCBClient", CEDAR_ERR_CONNECT_FAILED, errmsg.Value() );
				}
				dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());
				return false;
			}
		} else {
			condor_sockaddr ccbSA;
			MyString faked_sinful = "<" + ccb_address + ">";
			if( ! ccbSA.from_sinful( faked_sinful ) ) {
				dprintf( D_FULLDEBUG, "Failed to generate condor_sockaddr from faked sinful '%s', ignoring this broker.\n", faked_sinful.Value() );
				continue;
			}

			listen_sock = std::shared_ptr<ReliSock>( new ReliSock() );

			// Should bind() should accept a condor_sockaddr directly?
			if (!listen_sock->bind( ccbSA.get_protocol(), false, 0, false )) {
				dprintf(D_ALWAYS,"CCBClient: can't bind listen socket\n");
				return false;
			}

			if( ! listen_sock->listen() ) {
				MyString errmsg;
				errmsg.formatstr( "Failed to listen for reversed connection from %s.",
					m_target_peer_description.c_str() );
				if( error ) {
					error->push("CCBClient", CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
				}
				dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.Value());

				return false;
			}

			listener_addr = listen_sock->get_sinful_public();
		}

		ClassAd msg;
		msg.Assign(ATTR_CCBID,ccbid);
		msg.Assign(ATTR_CLAIM_ID,m_connect_id);
		// purely for debugging purposes: identify ourselves
		msg.Assign(ATTR_NAME, myName());
		// ATTR_MY_ADDRESS is functional and subject to address rewriting,
		// but it's not OK to be protocol-blind, because we won't recognize
		// the port number (it's not a command socket).
		msg.Assign(ATTR_MY_ADDRESS, listener_addr);

		dprintf(D_NETWORK|D_FULLDEBUG,
				"CCBClient: requesting reverse connection to %s "
				"via CCB server %s#%s; I am listening at %s.\n",
				m_target_peer_description.c_str(),
				ccb_address.c_str(),
				ccbid.c_str(),
				listener_addr);

		Daemon ccb(DT_COLLECTOR,ccb_address.c_str(),NULL);
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
					ccb_address.c_str());
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
					m_target_peer_description.c_str(),
					ccbid.c_str(),
					ccb_address.c_str());

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
						shared_listener = std::shared_ptr<SharedPortEndpoint>(NULL);
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

bool CCBClient::SplitCCBContact( char const *ccb_contact, std::string &ccb_address, std::string &ccbid, const std::string & peer, CondorError *error )
{
	// expected format: "<address>#ccbid"
	char const *ptr = strchr(ccb_contact,'#');
	if( !ptr ) {
		MyString errmsg;
		errmsg.formatstr("Bad CCB contact '%s' when connecting to %s.",
					   ccb_contact, peer.c_str());

		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.Value());
		}
		else {
			dprintf(D_ALWAYS,"%s\n",errmsg.Value());
		}
		return false;
	}
	ccb_address.assign(ccb_contact, ptr-ccb_contact);
	ccbid = ptr+1;
	return true;
}

bool
CCBClient::AcceptReversedConnection(std::shared_ptr<ReliSock> listen_sock,std::shared_ptr<SharedPortEndpoint> shared_listener)
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
					m_target_peer_description.c_str());
			return false;
		}
	}
	else if( !listen_sock->accept( m_target_sock ) ) {
		dprintf(D_ALWAYS,
				"CCBClient: failed to accept() reversed connection "
				"(intended target is %s)\n",
				m_target_peer_description.c_str());
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
				m_target_peer_description.c_str());
		m_target_sock->close();
		return false;
	}

	std::string connect_id;
	msg.LookupString(ATTR_CLAIM_ID,connect_id);

	if( cmd != CCB_REVERSE_CONNECT || connect_id != m_connect_id ) {
		dprintf(D_ALWAYS,
				"CCBClient: invalid hello message from reversed "
				"connection %s (intended target is %s)\n",
				m_target_sock->default_peer_description(),
				m_target_peer_description.c_str());
		m_target_sock->close();
		return false;
	}

	dprintf(D_FULLDEBUG|D_NETWORK,
			"CCBClient: received reversed connection %s "
			"(intended target is %s)\n",
			m_target_sock->default_peer_description(),
			m_target_peer_description.c_str());

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
					   m_target_peer_description.c_str());
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
		std::string remote_errmsg;
		msg.LookupString(ATTR_ERROR_STRING,remote_errmsg);

		errmsg.formatstr(
			"received failure message from CCB server %s in response to "
			"request for reversed connection to %s: %s",
			m_ccb_sock->peer_description(),
			m_target_peer_description.c_str(),
			remote_errmsg.c_str());

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
				m_target_peer_description.c_str());
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
				m_target_peer_description.c_str());
		ReverseConnectCallback(NULL);
		return false;
	}

	std::string ccbid;
	if( !SplitCCBContact( ccb_contact, m_cur_ccb_address, ccbid, m_target_peer_description, NULL ) ) {
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
				m_target_peer_description.c_str());

		// strip off CCB contact info in the return address
		sinful_return.setCCBContact(NULL);
		return_address = sinful_return.getSinful();
	}

	dprintf(D_NETWORK|D_FULLDEBUG,
			"CCBClient: requesting reverse connection to %s "
			"via CCB server %s#%s; "
			"I am listening on my command socket %s.\n",
			m_target_peer_description.c_str(),
			m_cur_ccb_address.c_str(),
			ccbid.c_str(),
			return_address);

	classy_counted_ptr<Daemon> ccb_server = new Daemon(DT_COLLECTOR,m_cur_ccb_address.c_str(),NULL);

	ClassAd ad;
	ad.Assign(ATTR_CCBID,ccbid);
	ad.Assign(ATTR_CLAIM_ID,m_connect_id);
	// purely for debugging purposes: identify ourselves
	ad.Assign(ATTR_NAME, myName());
	// ATTR_MY_ADDRESS is functional and subject to address rewriting, so
	// it's OK to use the protocol-blind publicNetworkIpAddr() above.
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
		std::string remote_errmsg;
		msg.LookupBool(ATTR_RESULT,result);
		msg.LookupString(ATTR_ERROR_STRING,remote_errmsg);

		if( !result ) {
			dprintf(D_ALWAYS,"CCBClient:"
				"received failure message from CCB server %s in response to "
				"(non-blocking) request for reversed connection to %s: %s\n",
				m_cur_ccb_address.c_str(),
				m_target_peer_description.c_str(),
				remote_errmsg.c_str());
			UnregisterReverseConnectCallback();
			try_next_ccb();
		}
		else {
			dprintf(D_FULLDEBUG|D_NETWORK,
				"CCBClient: received 'success' in reply from CCB server %s "
				"in response to (non-blocking) request for reversed connection"
				" to %s\n",
				m_cur_ccb_address.c_str(),
				m_target_peer_description.c_str());

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
			m_target_peer_description.c_str());

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
CCBClient::ReverseConnectCommandHandler(int cmd,Stream *stream)
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

	std::string connect_id;
	msg.LookupString(ATTR_CLAIM_ID,connect_id);

	classy_counted_ptr<CCBClient> client;
	int rc = waiting_for_reverse_connect.lookup(connect_id,client);
	if( rc < 0 ) {
		dprintf(D_ALWAYS,
				"CCBClient: failed to find requested connection id %s.\n",
				connect_id.c_str());
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
			m_target_peer_description.c_str());
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


CCBClient *
CCBClientFactory::make( bool nonBlocking, const char * ccbContact, ReliSock * target ) {
	if(! (nonBlocking && get_mySubSystemType() == SUBSYSTEM_TYPE_SCHEDD) ) {
		return new CCBClient( ccbContact, target );
	}

	dprintf( D_NETWORK | D_FULLDEBUG, "Using batched CCB client from SCHEDD for %s.\n", ccbContact );
	return new BatchedCCBClient( ccbContact, target );
}


BatchedCCBClient::BatchedCCBClient( char const * c, ReliSock * t ) :
	CCBClient(c, t)	{ }

BatchedCCBClient::~BatchedCCBClient() { }

bool
BatchedCCBClient::ReverseConnect( CondorError *, bool nonBlocking ) {
	// You can only batch connections in nonblocking mode.
	ASSERT(nonBlocking);

	m_ccb_contacts.rewind();
	return try_next_ccb();
}

bool
BatchedCCBClient::try_next_ccb() {
	const char * nextContact = m_ccb_contacts.next();
	if(! nextContact) {
		dprintf( D_ALWAYS, "BatchedCCBClient: no more brokers to try for "
		                   "reversing connection to %s; giving up.\n",
		         m_target_peer_description.c_str() );
		return false;
	}

	if(! SplitCCBContact( nextContact, m_cur_ccb_address, m_ccb_id,
	                     m_target_peer_description, NULL ) ) {
		dprintf( D_ALWAYS, "BatchedCCBClient: failed to split CCB contact "
		                   "%s; trying next one.\n",
		         nextContact );
		return try_next_ccb();
	}

	m_target_sock->enter_reverse_connecting_state();
	return CCBConnectionBatcher::make( this );
}

std::map< std::string, CCBConnectionBatcher::Broker > CCBConnectionBatcher::brokers;

void
CCBConnectionBatcher::BatchTimerCallback() {
	auto now = time(NULL);
	for( auto & entry : brokers ) {
		Broker & b = entry.second;
		if( b.deadline <= now + 1 /* avoid aliasing bugs */ ) {
			b.timerID = -1;

			//
			// Batch the requests.
			//

			StringList ccbIDs;
			StringList connectIDs;
			for( auto & e : b.clients ) {
				ccbIDs.append( e.second->ccbID().c_str() );
				connectIDs.append( e.second->connectID().c_str() );
			}

			ClassAd command;
			char * ccbIDsString = ccbIDs.print_to_string();
			command.Assign( ATTR_CCBID, ccbIDsString );
			free( ccbIDsString );
			char * connectIDsString = connectIDs.print_to_string();
			command.Assign( ATTR_CLAIM_ID, connectIDsString );
			free( connectIDsString );
			command.Assign( ATTR_NAME, CCBClient::myName() );

			const char * returnAddress = daemonCore->publicNetworkIpAddr();
			ASSERT( returnAddress && * returnAddress );
			Sinful returnSinful( returnAddress );
			if( returnSinful.getCCBContact() ) {
				dprintf( D_ALWAYS, "CCBConnectionBatcher: WARNING: attempting to connect via CCB, but our return address is also CCB, which can't work.  Assuming misconfiguration, and that we're on the same private network.\n" );
				returnSinful.setCCBContact( NULL );
				returnAddress = returnSinful.getSinful();
			}
			command.Assign( ATTR_MY_ADDRESS, returnAddress );

			dprintf( D_NETWORK | D_FULLDEBUG,
				"CCBConnectionBatcher: requesting reverse connections "
				"via CCB server %s; "
				"I am listening on my command socket %s.n",
				entry.first.c_str(), returnAddress );

			//
			// FIXME: Send the batched request using non-blocking i/o.
			// FIXME: Handle the case where entry.first is myself.
			//
			Daemon * ccbServer = new Daemon( DT_COLLECTOR, entry.first.c_str(), NULL );

			CondorError error;
			Sock * socketToBroker = ccbServer->startCommand(
				CCB_BATCH_REQUEST, Stream::reli_sock,
				DEFAULT_CEDAR_TIMEOUT, & error );
			if(! socketToBroker ) {
				dprintf( D_ALWAYS, "CCBConnectionBatcher: failed to connect "
					"to broker %s to make batched request.\n",
					entry.first.c_str() );
				b.HandleBrokerFailure();
				return;
			}

			socketToBroker->encode();
			if(! putClassAd( socketToBroker, command )) {
				dprintf( D_ALWAYS, "CCBConnectionBatcher: failed to send "
					"batched request to broker %s\n", entry.first.c_str() );
				b.HandleBrokerFailure();
				return;
			}
			if(! socketToBroker->end_of_message() ) {
				dprintf( D_ALWAYS, "CCBConnectionBatcher: failed to send "
					"end of message to broker %s\n", entry.first.c_str() );
				b.HandleBrokerFailure();
				return;
			}

			//
			// The broker will reply almost immediately with the list of
			// connection IDs whose corresponding CCB ID wasn't recognized.
			// Don't bother to wait for those.
			//
			// FIXME: Receive the batched reply using non-blocking i/o.
			//
			socketToBroker->decode();

			ClassAd reply;
			if( !getClassAd( socketToBroker, reply ) || !socketToBroker->end_of_message() ) {
				dprintf( D_ALWAYS, "CCBConnectionBatcher: failed to read reply "
					"from broker %s\n", entry.first.c_str() );
				b.HandleBrokerFailure();
				return;
			}

			std::string failedConnectIDsString;
			if(! reply.LookupString( ATTR_CLAIM_ID, failedConnectIDsString )) {
				dprintf( D_ALWAYS, "CCBConnectionBatcher: invalid reply "
					"from broker %s\n", entry.first.c_str() );
				b.HandleBrokerFailure();
				return;
			}

			// Remove the failed connect IDs from the client list.
			StringList failedConnectIDs(failedConnectIDsString.c_str());
			failedConnectIDs.rewind();
			const char * failedConnectID = NULL;
			while( (failedConnectID = failedConnectIDs.next()) != NULL ) {
				b.ForgetClientAndTryNextBroker( failedConnectID );
			}

			/*
			 * Up to this point in the function, each pointer in b.clients
			 * exists only in there, so for memory clean-up it suffices to
			 * call the destructor and then forget the pointer.
			 *
			 * Because each client put its target socket into the reverse-
			 * connecting-state, each client must take it out of that state
			 * and call the target socket's handlers when we give up on (or
			 * fail to make) a reverse connection.
			 *
			 * It's safe to call the BatchedCCBClient destructor here:
			 * - The BatchedCCBClient object has no class-specific pointers.
			 * - The CCBClient object has three: m_target_sock, m_ccb_cb,
			 *   and m_ccb_sock.  It does not own m_target_sock, so we can
			 *   ignore that.  The destructor doesn't delete m_ccb_cb, which
			 *   may be a bug.  (Or just a lack of suspenders.)  However,
			 *   since it's only set in CCBClient::try_next_ccb(), we can
			 *   ignore it for BatchedCCBClient objects.  Likewise,
			 *   only CCBClient::ReverseConnect_blocking() sets m_ccb_sock.
			 *
			 * Once we've registered the reverse connect callbacks, we also
			 * need to unregister those reverse connect callbacks.
			 */

			// Register a reverse connect callback for each client that
			// could get one.  Note that if we weren't trying to receive
			// the replies with non-blocking i/o that we could hold off
			// on doing this until after we had gotten the replies, because
			// we wouldn't re-enter the event loop first.
			for( auto & e : b.clients ) {
				e.second->RegisterReverseConnectCallback();
			}

			// FIXME: Receive the individual replies using non-blocking i/o.
			socketToBroker->decode();
			for( unsigned i = 0; i < b.clients.size(); ++i ) {
				ClassAd reply;

				if( !getClassAd( socketToBroker, reply ) || !socketToBroker->end_of_message() ) {
					dprintf( D_ALWAYS, "CCBConnectionBatcher: failed to read connection-specific reply from %s, giving up on getting called back.\n", entry.first.c_str() );
					for( auto & f : b.clients ) {
						f.second->UnregisterReverseConnectCallback();
						b.ForgetClientAndTryNextBroker( f.first, f.second );
					}
					return;
				}

				bool result = false;
				std::string errorString;
				reply.LookupBool( ATTR_RESULT, result );
				reply.LookupString( ATTR_ERROR_STRING, errorString );

				if(! result) {
					std::string connectID;
					reply.LookupString( ATTR_CLAIM_ID, connectID );
					if( connectID.empty() ) {
						dprintf( D_ALWAYS, "CCBConnectionBatcher: broker reply malformed, some CCB request will time-out.\n" );
						continue;
					}
					if( b.clients.count(connectID) != 1 ) {
						dprintf( D_ALWAYS, "CCBCOnnectionBatcher: broker replied with an error for an unknown connection ID.\n" );
						continue;
					}

					auto * client = b.clients[connectID];
					dprintf( D_ALWAYS, "CCBConnectionBatcher: received "
						"failure message from broker %s in response to "
						"request for reversed connection to %s: %s\n",
						client->currentCCBAddress().c_str(),
						client->targetPeerDescription().c_str(),
						errorString.c_str() );

					// Unregister the reverse connect callback; the next
					// broker we try will register it again as appropriate.
					client->UnregisterReverseConnectCallback();
					b.ForgetClientAndTryNextBroker( connectID, client );
				} else {
					dprintf( D_NETWORK | D_FULLDEBUG,
					    "CCBConnectionBatcher: received "
						"success message from broker %s\n",
						entry.first.c_str() );
				}
			}

			// FIXME: When we switch to non-blocking broker replies, we'll
			// need to remember the list until the broker replies; clients
			// will have to remove themselves from the list when they get
			// their reverse connection or give up waiting for it.

			// We've handled all of our client objects, so forget about them.
			b.clients.clear();

			// Only handle one broker per callback, even if we're expecting
			// multiple brokers to become due in the same second.  This
			// allows event-loop management to work normally.
			return;
		}
	}

	dprintf( D_ALWAYS, "Broker timer fired but no brokers were due.  Carrying on, confused.\n" );
}

bool
CCBConnectionBatcher::make( BatchedCCBClient * client ) {
	ASSERT( client != NULL );
	ASSERT( daemonCore );

	Broker & b = brokers[client->currentCCBAddress()];
	b.clients[client->connectID()] = client;

	// If the broker is busy, delay.  Once the broker becomes busy, it
	// stays busy until proven otherwise; this means that the timer delay
	// sequence (one comma = one second) is 0 0 0 1, 1, 1, 0 instead of
	// 0 0 0 1, 0 0 0 1, 0 0 0 1.
	int delay = b.busy ? 1 : 0;
	time_t now = time(NULL);
	int threshold = param_integer( "CCB_CLIENT_MAX_CONNECTIONS_EACH_SECOND", 5 );

	// For each second, count the number of times the broker was requested.
	if( b.epoch == 0 ) {
	    dprintf( D_ALWAYS, "[rate limiting] Initial case for broker %s\n", client->currentCCBAddress().c_str() );
		b.epoch = now;
		b.recently = 1;
	} else if( b.epoch == now ) {
	    dprintf( D_ALWAYS, "[rate limiting] Incrementing request count for current epoch (%lu) from %d for broker %s.\n", b.epoch, b.recently, client->currentCCBAddress().c_str() );
		++b.recently;
	} else {
		// The broker is not busy if the number of times it was requested
		// in the last second is less than the threshold, or if the broker
		// was not requested at all in the previous second.
		if( (b.recently < threshold) || (b.epoch + 1 < now) ) {
		    dprintf( D_ALWAYS, "[rate limiting] The broker %s is no longer busy.\n", client->currentCCBAddress().c_str() );
			b.busy = false;
			delay = 0;
		}
		dprintf( D_ALWAYS, "[rate limiting] Resetting epoch for broker %s to %lu and request count to 1.\n", client->currentCCBAddress().c_str(), now );
		b.epoch = now;
		b.recently = 1;
	}

	if( b.recently >= threshold ) {
	    if(! b.busy) { dprintf( D_ALWAYS, "[rate limiting] Broker %s is now busy.\n", client->currentCCBAddress().c_str() ); }
		b.busy = true;
		delay = 1;
	}

	if( b.timerID == -1 ) {
	    dprintf( D_ALWAYS, "[rate limiting] Registering a new timer for broker %s with delay %d\n", client->currentCCBAddress().c_str(), delay );
		b.deadline = now + delay;
		b.timerID = daemonCore->Register_Timer( delay,
			(TimerHandler)&CCBConnectionBatcher::BatchTimerCallback,
			"CCBConnectionBatcher::BatchTimerCallback" );
	}

	return true;
}

void
BatchedCCBClient::CancelReverseConnect() {
	CCBClient::CancelReverseConnect();
}

void
BatchedCCBClient::FailReverseConnect() {
	ASSERT( daemonCore );
	ASSERT( m_target_sock );

	m_target_sock->exit_reverse_connecting_state( NULL );
	daemonCore->CallSocketHandler( m_target_sock, false );
}

void
CCBConnectionBatcher::Broker::HandleBrokerFailure() {
	for( auto e : clients ) {
		ForgetClientAndTryNextBroker( e.first, e.second );
	}
}

void
CCBConnectionBatcher::Broker::ForgetClientAndTryNextBroker(
  const std::string & connectID, BatchedCCBClient * client ) {
	if( client == NULL ) {
		if( clients.count( connectID ) != 0 ) {
			client = clients[connectID];
		} else {
			return;
		}
	}

	// Forget about this client (does not call destructor).
	cients.erase( connectID );

	// Try the next broker.  If the client can't, destroy it.
	// Otherwise, it becomes the responsibility of the next broker.
	if(! client->try_next_ccb()) {
		client->FailReverseConnect();
		delete client;
	}
}
