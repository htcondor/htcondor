/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

// //////////////////////////////////////////////////////////////////////
//
// Implementation of DaemonCore.
//
//
// //////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "authentication.h"
#include "reli_sock.h"
#include "condor_daemon_core.h"
#include "condor_io.h"
#include "condor_rw.h"
#include "condor_config.h"
#include "condor_threads.h"
#include "condor_version.h"
#include "ipv6_hostname.h"
#include "daemon_command.h"


static unsigned int ZZZZZ = 0;
static int ZZZ_always_increase() {
	return ZZZZZ++;
}

const std::string DaemonCommandProtocol::WaitForSocketDataString = "DaemonCommandProtocol::WaitForSocketData";

DaemonCommandProtocol::DaemonCommandProtocol( Stream * sock, bool is_command_sock, bool isSharedPortLoopback ) :
	m_isSharedPortLoopback( isSharedPortLoopback ),
	m_nonblocking(!is_command_sock), // cannot re-register command sockets for non-blocking read
	m_delete_sock(!is_command_sock), // must not delete registered command sockets
	m_sock_had_no_deadline(false),
	m_is_tcp(0),
	m_req(0),
	m_reqFound(FALSE),
	m_result(FALSE),
	m_perm(USER_AUTH_FAILURE),
	m_allow_empty(false),
	m_policy(NULL),
	m_key(NULL),
	m_sid(NULL),
	m_prev_sock_ent(NULL),
	m_async_waiting_time(0),
	m_comTable(daemonCore->comTable),
	m_real_cmd(0),
	m_auth_cmd(0),
	m_cmd_index(0),
	m_errstack(NULL),
	m_new_session(false),
	m_will_enable_encryption(SecMan::SEC_FEAT_ACT_UNDEFINED),
	m_will_enable_integrity(SecMan::SEC_FEAT_ACT_UNDEFINED)
{
	m_sock = dynamic_cast<Sock *>(sock);

	m_sec_man = daemonCore->getSecMan();

	condor_gettimestamp( m_handle_req_start_time );
	timerclear( &m_async_waiting_start_time );

	ASSERT(m_sock);

	switch ( m_sock->type() ) {
		case Stream::reli_sock :
			m_is_tcp = TRUE;
			m_state = CommandProtocolAcceptTCPRequest;
			break;
		case Stream::safe_sock :
			m_is_tcp = FALSE;
			m_state = CommandProtocolAcceptUDPRequest;
			break;
		default:
			// unrecognized Stream sock
			EXCEPT("DaemonCore: HandleReq(): unrecognized Stream sock");
	}
}

DaemonCommandProtocol::~DaemonCommandProtocol()
{
	if (m_errstack) {
		delete m_errstack;
		m_errstack = NULL;
	}
	if (m_policy) {
		delete m_policy;
	}
	if (m_key) {
		delete m_key;
	}
	if (m_sid) {
		free(m_sid);
	}
}

int DaemonCommandProtocol::doProtocol()
{

	CommandProtocolResult what_next = CommandProtocolContinue;

	if( m_sock ) {
		if( m_sock->deadline_expired() ) {
			dprintf(D_ALWAYS,"DaemonCommandProtocol: deadline for security handshake with %s has expired.\n",
					m_sock->peer_description());

			m_result = FALSE;
			what_next = CommandProtocolFinished;
		}
		else if( m_nonblocking && m_sock->is_connect_pending() ) {
			dprintf(D_SECURITY, "DaemonCommandProtocol: Waiting for connect.\n");
			what_next = WaitForSocketData();
		}
		else if( m_is_tcp && !m_sock->is_connected()) {
			dprintf(D_ALWAYS,"DaemonCommandProtocol: TCP connection to %s failed.\n",
					m_sock->peer_description());

			m_result = FALSE;
			what_next = CommandProtocolFinished;
		}
	}

	while( what_next == CommandProtocolContinue ) {
		switch(m_state) {
		case CommandProtocolAcceptTCPRequest:
			what_next = AcceptTCPRequest();
			break;
		case CommandProtocolAcceptUDPRequest:
			what_next = AcceptUDPRequest();
			break;
		case CommandProtocolReadHeader:
			what_next = ReadHeader();
			break;
		case CommandProtocolReadCommand:
			what_next = ReadCommand();
			break;
		case CommandProtocolAuthenticate:
			what_next = Authenticate();
			break;
		case CommandProtocolAuthenticateContinue:
			what_next = AuthenticateContinue();
			break;
		case CommandProtocolEnableCrypto:
			what_next = EnableCrypto();
			break;
		case CommandProtocolVerifyCommand:
			what_next = VerifyCommand();
			break;
		case CommandProtocolSendResponse:
			what_next = SendResponse();
			break;
		case CommandProtocolExecCommand:
			what_next = ExecCommand();
			break;
		}
	}

	if( what_next == CommandProtocolInProgress ) {
		return KEEP_STREAM;
	}

	return finalize();
}

// Register the socket to call us back when data is available to read.
DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::WaitForSocketData()
{
	if( m_sock->get_deadline() == 0 ) {
		int TCP_SESSION_DEADLINE = param_integer("SEC_TCP_SESSION_DEADLINE",120);
		m_sock->set_deadline_timeout(TCP_SESSION_DEADLINE);
		m_sock_had_no_deadline = true;
	}

	int reg_rc = daemonCore->Register_Socket(
			m_sock,
			m_sock->peer_description(),
			(SocketHandlercpp)&DaemonCommandProtocol::SocketCallback,
			WaitForSocketDataString.c_str(),
			this,
			ALLOW,
			HANDLE_READ,
			&m_prev_sock_ent);

	if(reg_rc < 0) {
		dprintf(D_ALWAYS, "DaemonCommandProtocol failed to process command from %s because "
				"Register_Socket returned %d.\n",
				m_sock->get_sinful_peer(),
				reg_rc);

		m_result = FALSE;
		return CommandProtocolFinished;
	}

		// Do not allow ourselves to be deleted until after
		// SocketCallback is called.
	incRefCount();

	condor_gettimestamp( m_async_waiting_start_time );

	return CommandProtocolInProgress;
}

// This function is called by DaemonCore when incoming data is
// available to read, or the socket has become disconnected,
// or the deadline has expired.
int
DaemonCommandProtocol::SocketCallback( Stream *stream )
{
	struct timeval async_waiting_stop_time;
	condor_gettimestamp( async_waiting_stop_time );
	m_async_waiting_time += timersub_double( async_waiting_stop_time, m_async_waiting_start_time );

	daemonCore->Cancel_Socket( stream, m_prev_sock_ent );
	m_prev_sock_ent = NULL;

	int rc = doProtocol();

		// get rid of ref counted when callback was registered
	decRefCount();

	return rc;
}

// This is the first thing we do on an incoming TCP command socket.
// Once this function is finished, m_sock will point to the socket
// from which we should read the command.
DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::AcceptTCPRequest()
{

	m_state = CommandProtocolReadHeader;

		// we have just accepted a socket or perhaps been given a
		// socket from HandleReqAsync().  if there is nothing
		// available yet to read on this socket, we don't want to
		// block when reading the command, so register it
	if ( m_nonblocking && ((ReliSock *)m_sock)->bytes_available_to_read() < 4 ) {
		dprintf(D_SECURITY, "DaemonCommandProtocol: Not enough bytes are ready for read.\n");
		return WaitForSocketData();
	}

	return CommandProtocolContinue;
}

// This is the first thing we do on an incoming UDP command socket.
// Once this function is finished, m_sock will point to the socket
// from which we should read the command.  If the Condor UDP
// packet header indicates that this command is using an existing
// security session, we handle that here.
DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::AcceptUDPRequest()
{
	std::string who;

		// in UDP we cannot display who the command is from until
		// we read something off the socket, so we display who from
		// after we read the command below...

		dprintf ( D_SECURITY,
				  "DC_AUTHENTICATE: received UDP packet from %s.\n",
				  m_sock->peer_description());


		// get the info, if there is any
		const char * cleartext_info = ((SafeSock*)m_sock)->isIncomingDataHashed();
		char * sess_id = NULL;
		char * return_address_ss = NULL;

		if (cleartext_info) {
			StringList info_list(cleartext_info);
			char * tmp = NULL;

			info_list.rewind();
			tmp = info_list.next();
			if (tmp) {
				sess_id = strdup(tmp);
				tmp = info_list.next();
				if (tmp) {
					return_address_ss = strdup(tmp);
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet from %s uses hash session %s.\n",
							return_address_ss, sess_id);
				} else {
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet uses hash session %s.\n", sess_id);
				}

			} else {
				// protocol violation... StringList didn't give us anything!
				// this is unlikely to work, but we may as well try... so, we
				// don't fail here.
			}
		}

		if (sess_id) {
			KeyCacheEntry *session = NULL;
			bool found_sess = m_sec_man->session_cache->lookup(sess_id, session);

			if (!found_sess) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s NOT FOUND; this session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
				// no session... we outta here!

				// but first, we should be nice and send a message back to
				// the people who sent us the wrong session id.
				daemonCore->send_invalidate_session ( return_address_ss, sess_id );

				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				m_result = FALSE;
				return CommandProtocolFinished;
			}

			session->renewLease();

			if (!session->key()) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s is missing the key! This session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
				// uhm, there should be a key here!
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				m_result = FALSE;
				return CommandProtocolFinished;
			}

			if (!m_sock->set_MD_mode(MD_ALWAYS_ON, session->key())) {
				dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on message authenticator for session %s, failing; this session was requested by %s with return address %s\n",sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				m_result = FALSE;
				return CommandProtocolFinished;
			} else {
				dprintf (D_SECURITY, "DC_AUTHENTICATE: message authenticator enabled with key id %s.\n", sess_id);
				m_sec_man->key_printf (D_SECURITY, session->key());
			}

			// Lookup remote user
			session->policy()->LookupString(ATTR_SEC_USER, who);

			free( sess_id );

			if (return_address_ss) {
				free( return_address_ss );
			}
		}


		// get the info, if there is any
		cleartext_info = ((SafeSock*)m_sock)->isIncomingDataEncrypted();
		sess_id = NULL;
		return_address_ss = NULL;

		if (cleartext_info) {
			StringList info_list(cleartext_info);
			char * tmp = NULL;

			info_list.rewind();
			tmp = info_list.next();
			if (tmp) {
				sess_id = strdup(tmp);

				tmp = info_list.next();
				if (tmp) {
					return_address_ss = strdup(tmp);
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet from %s uses crypto session %s.\n",
							return_address_ss, sess_id);
				} else {
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet uses crypto session %s.\n", sess_id);
				}

			} else {
				// protocol violation... StringList didn't give us anything!
				// this is unlikely to work, but we may as well try... so, we
				// don't fail here.
			}
		}


		if (sess_id) {
			KeyCacheEntry *session = NULL;
			bool found_sess = m_sec_man->session_cache->lookup(sess_id, session);

			if (!found_sess) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s NOT FOUND; this session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
				// no session... we outta here!

				// but first, send a message to whoever provided us with incorrect session id
				daemonCore->send_invalidate_session( return_address_ss, sess_id );

				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				m_result = FALSE;
				return CommandProtocolFinished;
			}

			session->renewLease();

			if (!session->key()) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s is missing the key! This session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
				// uhm, there should be a key here!
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				m_result = FALSE;
				return CommandProtocolFinished;
			}

				// NOTE: prior to 7.1.3, we _always_ set the encryption
				// mode to "on" here, regardless of what was previously
				// negotiated.  However, as of now, the sender never
				// sends an encryption id in the UDP packet header unless
				// we did negotiate to use encryption by default.  Once we
				// no longer care about backwards compatibility with
				// versions older than 7.1.3, we could allow the sender to
				// set the encryption id and trust that we will set the mode
				// as was previously negotiated.
			SecMan::sec_feat_act will_enable_encryption = m_sec_man->sec_lookup_feat_act(*session->policy(), ATTR_SEC_ENCRYPTION);
			bool turn_encryption_on = will_enable_encryption == SecMan::SEC_FEAT_ACT_YES;

			if (!m_sock->set_crypto_key(turn_encryption_on, session->key())) {
				dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on encryption for session %s, failing; this session was requested by %s with return address %s\n",sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				m_result = FALSE;
				return CommandProtocolFinished;
			} else {
				dprintf (D_SECURITY,
					"DC_AUTHENTICATE: encryption enabled with key id %s%s.\n",
					sess_id,
					turn_encryption_on ? "" : " (but encryption mode is off by default for this packet)");
				m_sec_man->key_printf (D_SECURITY, session->key());
			}
			// Lookup user if necessary
			if (who.empty()) {
				session->policy()->LookupString(ATTR_SEC_USER, who);
			}
			bool tried_authentication = false;
			session->policy()->LookupBool(ATTR_SEC_TRIED_AUTHENTICATION,tried_authentication);
			m_sock->setTriedAuthentication(tried_authentication);
			m_sock->setSessionID(sess_id);

			free( sess_id );
			if (return_address_ss) {
				free( return_address_ss );
			}
		}

		if (!who.empty()) {
			m_sock->setFullyQualifiedUser(who.c_str());
			dprintf (D_SECURITY, "DC_AUTHENTICATE: UDP message is from %s.\n", who.c_str());
		}

		m_state = CommandProtocolReadHeader;
		return CommandProtocolContinue;
}

// Read the header.  Soap requests are handled here.
// If this is not a soap request and is a registered command,
// pass on to ReadCommand.
// Soap requests are also handled here.
DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::ReadHeader()
{
	m_sock->decode();

		// Determine if incoming socket is HTTP over TCP, or if it is CEDAR.
		// For better or worse, we figure this out by seeing if the socket
		// starts w/ a GET or POST.  Hopefully this does not correspond to
		// a daemoncore command int!  [not ever likely, since CEDAR ints are
		// exapanded out to 8 bytes]  Still, in a perfect world we would replace
		// with a more foolproof method.
	char tmpbuf[6];
	memset(tmpbuf,0,sizeof(tmpbuf));
	if ( m_is_tcp ) {
			// TODO Should we be ignoring the return value of condor_read?
		condor_read(m_sock->peer_description(), m_sock->get_file_desc(),
			tmpbuf, sizeof(tmpbuf) - 1, 1, MSG_PEEK);
	}

		// This was not a soap request; next, see if we have a special command
		// handler for unknown command integers.
		//
		// We do manual CEDAR parsing here to look at the command int directly
		// without consuming data from the socket.  The first few bytes are the
		// size of the message, followed by the command int itself.
	int tmp_req; memcpy(static_cast<void*>(&tmp_req), tmpbuf+1, sizeof(int));
	tmp_req = ntohl(tmp_req);
	if (daemonCore->HandleUnregistered() && (tmp_req >= 8)) {
			// Peek at the command integer if one exists.
		char tmpbuf2[8+5]; memset(tmpbuf2, 0, sizeof(tmpbuf2));
		condor_read(m_sock->peer_description(), m_sock->get_file_desc(),
			tmpbuf2, 8+5, 1, MSG_PEEK);
		char *tmpbuf3 = tmpbuf2 + 5;
		if (8-sizeof(int) > 0) { tmpbuf3 += 8-sizeof(int); } // Skip padding
		memcpy(static_cast<void*>(&tmp_req), tmpbuf3, sizeof(int));
		tmp_req = ntohl(tmp_req);

			// Lookup the command integer in our command table to see if it is unregistered
		int tmp_cmd_index;
		if(	   (!m_isSharedPortLoopback)
			&& (! daemonCore->CommandNumToTableIndex( tmp_req, &tmp_cmd_index ))
			&& ( daemonCore->HandleUnregisteredDCAuth()
				|| (tmp_req != DC_AUTHENTICATE) ) ) {
			ScopedEnableParallel(false);

			if( m_sock_had_no_deadline ) {
					// unset the deadline we assigned in WaitForSocketData
				m_sock->set_deadline(0);
			}

			m_result = daemonCore->CallUnregisteredCommandHandler(tmp_req, m_sock);
			return CommandProtocolFinished;
		}
	}

	m_state = CommandProtocolReadCommand;
	return CommandProtocolContinue;
}

// Read the command.  This function will either be followed by VerifyCommand if
// we aren't using DC_AUTHENTICATE, otherwise either Authenticate or EnableCrypto,
// depending on whether or not authentication was requested.
DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::ReadCommand()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: ReadCommand()\n");

	m_sock->decode();

	if (m_sock->type() == Stream::reli_sock) {
		// While we know data is waiting for us, the CEDAR 'code' implementation will
		// read in all the data to a EOM; this can be an arbitrary number of bytes, depending
		// on which command is used.
		bool read_would_block;
		{
			BlockingModeGuard guard(static_cast<ReliSock*>(m_sock), 1);
			m_result = m_sock->code(m_req);
			read_would_block = static_cast<ReliSock*>(m_sock)->clear_read_block_flag();
		}
		if (read_would_block)
		{
			dprintf(D_NETWORK, "CommandProtocol read would block; waiting for more data to arrive on the socket.\n");
			return WaitForSocketData();
		}
	}
	else
	{
		m_sock->timeout(1);
		m_result = m_sock->code(m_req);
	}
	// For now, lets set a 20 second timeout, so all command handlers are called with
	// a timeout of 20 seconds on their socket.
	if(!m_result) {
		char const *ip = m_sock->peer_ip_str();
		if(!ip) {
			ip = "unknown address";
		}
		dprintf(D_ALWAYS,
			"DaemonCore: Can't receive command request from %s (perhaps a timeout?)\n", ip);
		m_result = FALSE;
		return CommandProtocolFinished;
	}
	m_sock->timeout(20);

	if (m_req == DC_AUTHENTICATE) {

		// Allow thread to yield during all the authentication network round-trips
		ScopedEnableParallel(true);

		m_sock->decode();

		dprintf (D_SECURITY, "DC_AUTHENTICATE: received DC_AUTHENTICATE from %s\n", m_sock->peer_description());

		if( !getClassAd(m_sock, m_auth_info)) {
			dprintf (D_ALWAYS, "ERROR: DC_AUTHENTICATE unable to "
					 "receive auth_info from %s!\n", m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		if ( m_is_tcp && !m_sock->end_of_message()) {
			dprintf (D_ALWAYS, "ERROR: DC_AUTHENTICATE is TCP, unable to "
					   "receive eom!\n");
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: received following ClassAd:\n");
			dPrintAd (D_SECURITY, m_auth_info);
		}

		std::string peer_version;
		if( m_auth_info.LookupString( ATTR_SEC_REMOTE_VERSION, peer_version ) ) {
			CondorVersionInfo ver_info( peer_version.c_str() );
			m_sock->set_peer_version( &ver_info );
		}

		// look at the ad.  get the command number.
		m_real_cmd = 0;
		m_auth_cmd = 0;
		m_auth_info.LookupInteger(ATTR_SEC_COMMAND, m_real_cmd);

		if ((m_real_cmd == DC_AUTHENTICATE) || (m_real_cmd == DC_SEC_QUERY)) {
			// we'll set m_auth_cmd temporarily to
			m_auth_info.LookupInteger(ATTR_SEC_AUTH_COMMAND, m_auth_cmd);
		} else {
			m_auth_cmd = m_real_cmd;
		}

		// get the auth level of this command
		// locate the hash table entry
		m_cmd_index = 0;
		m_reqFound = daemonCore->CommandNumToTableIndex(m_auth_cmd,&m_cmd_index);

		if (!m_reqFound) {
			// we have no idea what command they want to send.
			// too bad, bye bye

			dprintf(D_ALWAYS,
					"Received %s command (%d) (%s) from %s %s\n",
					(m_is_tcp) ? "TCP" : "UDP",
					m_auth_cmd,
					"UNREGISTERED COMMAND!",
					m_user.c_str(),
					m_sock->peer_description());

			m_result = FALSE;
			return CommandProtocolFinished;
		}

		m_new_session        = false;
		bool using_cookie       = false;
		bool valid_cookie		= false;

		// check if we are using a cookie
		char *incoming_cookie   = NULL;
		if( m_auth_info.LookupString(ATTR_SEC_COOKIE, &incoming_cookie)) {
			// compare it to the one we have internally

			valid_cookie = daemonCore->cookie_is_valid((unsigned char*)incoming_cookie);
			free (incoming_cookie);

			if ( valid_cookie ) {
				// we have a match... trust this command.
				using_cookie = true;
			} else {
				// bad cookie!!!
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: received invalid cookie from %s!!!\n", m_sock->peer_description());
				m_result = FALSE;
				return CommandProtocolFinished;
			}
		}

		// check if we are restarting a cached session

		if (!using_cookie) {

			if ( m_sec_man->sec_lookup_feat_act(m_auth_info, ATTR_SEC_USE_SESSION) == SecMan::SEC_FEAT_ACT_YES) {

				KeyCacheEntry *session = NULL;

				if( ! m_auth_info.LookupString(ATTR_SEC_SID, &m_sid)) {
					dprintf (D_ALWAYS, "ERROR: DC_AUTHENTICATE unable to "
							 "extract auth_info.%s from %s!\n", ATTR_SEC_SID,
							 m_sock->peer_description());
					m_result = FALSE;
					return CommandProtocolFinished;
				}

				// lookup the suggested key
				if (!m_sec_man->session_cache->lookup(m_sid, session)) {

					// the key id they sent was not in our cache.  this is a
					// problem.

					char * return_addr = NULL;
					m_auth_info.LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, &return_addr);
					std::string our_sinful;
					m_auth_info.LookupString(ATTR_SEC_CONNECT_SINFUL, our_sinful);
					ClassAd info_ad;
					// Presence of the ConnectSinful attribute indicates
					// that the client understands and wants the
					// extended information ad in the
					// DC_INVALIDATE_KEY message.
					if ( !our_sinful.empty() ) {
						info_ad.Assign(ATTR_SEC_CONNECT_SINFUL, our_sinful);
					}

					dprintf (D_ALWAYS, "DC_AUTHENTICATE: attempt to open "
							   "invalid session %s, failing; this session was requested by %s with return address %s\n", m_sid, m_sock->peer_description(), return_addr ? return_addr : "(none)");
					if( !strncmp( m_sid, "family:", strlen("family:") ) ) {
						dprintf(D_ALWAYS, "  The remote daemon thinks that we are in the same family of Condor daemon processes as it, but I don't recognize its family security session.\n");
						dprintf(D_ALWAYS, "  If we are in the same family of processes, you may need to change how the configuration parameter SEC_USE_FAMILY_SESSION is set.\n");
					}

					if( return_addr ) {
						daemonCore->send_invalidate_session( return_addr, m_sid, &info_ad );
						free (return_addr);
					}

					// consume the rejected message
					m_sock->decode();
					m_sock->end_of_message();

					// close the connection.
					m_result = FALSE;
					return CommandProtocolFinished;

				} else {
					// the session->id() and the_sid strings should be identical.

					if (IsDebugLevel(D_SECURITY)) {
						char *return_addr = NULL;
						if(session->policy()) {
							session->policy()->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK,&return_addr);
						}
						dprintf (D_SECURITY, "DC_AUTHENTICATE: resuming session id %s%s%s:\n",
								 session->id(),
								 return_addr ? " with return address " : "",
								 return_addr ? return_addr : "");
						free(return_addr);
					}
				}

				session->renewLease();

				if (session->key()) {
					// copy this to the HandleReq() scope
					m_key = new KeyInfo(*session->key());
				}

				if (session->policy()) {
					// copy this to the HandleReq() scope
					m_policy = new ClassAd(*session->policy());
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: Cached Session:\n");
						dPrintAd (D_SECURITY, *m_policy);
					}
				}

				std::string peer_version;

				// grab some attributes out of the policy.
				if (m_policy) {
					char *tmp  = NULL;
					m_policy->LookupString( ATTR_SEC_USER, &tmp);
					if (tmp) {
						// copy this to the HandleReq() scope
						m_user = tmp;
						free( tmp );
						tmp = NULL;
					}
					m_policy->LookupString( ATTR_SEC_AUTHENTICATED_NAME, &tmp);
					if (tmp) {
						// copy this to the HandleReq() scope
						m_sock->setAuthenticatedName(tmp);
						free( tmp );
						tmp = NULL;
					}
					m_policy->LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &tmp);
					if (tmp) {
						// copy this to the HandleReq() scope
						m_sock->setAuthenticationMethodUsed(tmp);
						free( tmp );
						tmp = NULL;
					}

					m_policy->LookupString( ATTR_SEC_REMOTE_VERSION, peer_version );

					bool tried_authentication=false;
					m_policy->LookupBool(ATTR_SEC_TRIED_AUTHENTICATION,tried_authentication);
					m_sock->setTriedAuthentication(tried_authentication);
					m_sock->setSessionID(session->id());
				}

				// When using a cached session, only use the version
				// from the session for the socket's peer version.
				// This maintains symmetry of version info between
				// client and server.
				if ( !peer_version.empty() ) {
					CondorVersionInfo ver_info( peer_version.c_str() );
					m_sock->set_peer_version( &ver_info );
				} else {
					m_sock->set_peer_version( NULL );
				}

				m_new_session = false;

			} // end of case: using existing session
			else {
					// they did not request a cached session.  see if they
					// want to start one.  look at our security policy.
				ClassAd *our_policy;
				if( ! m_sec_man->FillInSecurityPolicyAdFromCache(
					m_comTable[m_cmd_index].perm,
					our_policy,
					false,
					false,
					m_comTable[m_cmd_index].force_authentication ) )
				{
						// our policy is invalid even without the other
						// side getting involved.
					dprintf( D_ALWAYS, "DC_AUTHENTICATE: "
							 "Our security policy is invalid!\n" );
					m_result = FALSE;
					return CommandProtocolFinished;
				}

				if (IsDebugVerbose(D_SECURITY)) {
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: our_policy:\n" );
					dPrintAd(D_SECURITY, *our_policy);
				}

				// reconcile.  if unable, close socket.
				m_policy = m_sec_man->ReconcileSecurityPolicyAds( m_auth_info,
																  *our_policy );

				if (!m_policy) {
					dprintf(D_ALWAYS, "DC_AUTHENTICATE: Unable to reconcile!\n");
					m_result = FALSE;
					return CommandProtocolFinished;
				} else {
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf ( D_SECURITY, "DC_AUTHENTICATE: the_policy:\n" );
						dPrintAd(D_SECURITY, *m_policy);
					}
				}

				// add our version to the policy to be sent over
				m_policy->Assign(ATTR_SEC_REMOTE_VERSION, CondorVersion());

				// handy policy vars
				SecMan::sec_feat_act will_authenticate      = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_AUTHENTICATION);

				if (m_sec_man->sec_lookup_feat_act(m_auth_info, ATTR_SEC_NEW_SESSION) == SecMan::SEC_FEAT_ACT_YES) {

					// generate a new session

					// generate a unique ID.
					std::string tmpStr;
					formatstr( tmpStr, "%s:%i:%i:%i",
									get_local_hostname().Value(), daemonCore->mypid,
							 (int)time(0), ZZZ_always_increase() );
					assert (m_sid == NULL);
					m_sid = strdup(tmpStr.c_str());

					if (will_authenticate == SecMan::SEC_FEAT_ACT_YES) {

						char *crypto_method = NULL;
						if (!m_policy->LookupString(ATTR_SEC_CRYPTO_METHODS, &crypto_method)) {
							dprintf ( D_ALWAYS, "DC_AUTHENTICATE: tried to enable encryption for request from %s, but we have none!\n", m_sock->peer_description() );
							m_result = FALSE;
							return CommandProtocolFinished;
						}

						unsigned char* rkey = Condor_Crypt_Base::randomKey(24);
						unsigned char  rbuf[24];
						if (rkey) {
							memcpy (rbuf, rkey, 24);
							// this was malloced in randomKey
							free (rkey);
						} else {
							memset (rbuf, 0, 24);
							dprintf ( D_ALWAYS, "DC_AUTHENTICATE: unable to generate key for request from %s - no crypto available!\n", m_sock->peer_description() );							
							free( crypto_method );
							crypto_method = NULL;
							m_result = FALSE;
							return CommandProtocolFinished;
						}

						switch (toupper(crypto_method[0])) {
							case 'B': // blowfish
								dprintf (D_SECURITY, "DC_AUTHENTICATE: generating BLOWFISH key for session %s...\n", m_sid);
								m_key = new KeyInfo(rbuf, 24, CONDOR_BLOWFISH);
								break;
							case '3': // 3des
							case 'T': // Tripledes
								dprintf (D_SECURITY, "DC_AUTHENTICATE: generating 3DES key for session %s...\n", m_sid);
								m_key = new KeyInfo(rbuf, 24, CONDOR_3DES);
								break;
							default:
								dprintf (D_SECURITY, "DC_AUTHENTICATE: generating RANDOM key for session %s...\n", m_sid);
								m_key = new KeyInfo(rbuf, 24);
								break;
						}

						free( crypto_method );
						crypto_method = NULL;

						if (!m_key) {
							m_result = FALSE;
							return CommandProtocolFinished;
						}

						m_sec_man->key_printf (D_SECURITY, m_key);
					}

					m_new_session = true;
				}

				// if they asked, tell them
				if (m_is_tcp && (m_sec_man->sec_lookup_feat_act(m_auth_info, ATTR_SEC_ENACT) == SecMan::SEC_FEAT_ACT_NO)) {
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf (D_SECURITY, "SECMAN: Sending following response ClassAd:\n");
						dPrintAd( D_SECURITY, *m_policy );
					}
					m_sock->encode();
					if (!putClassAd(m_sock, *m_policy) ||
						!m_sock->end_of_message()) {
						dprintf (D_ALWAYS, "SECMAN: Error sending response classad to %s!\n", m_sock->peer_description());
						dPrintAd (D_ALWAYS, m_auth_info);
						m_result = FALSE;
						return CommandProtocolFinished;
					}
					m_sock->decode();
				} else {
					dprintf( D_SECURITY, "SECMAN: Enact was '%s', not sending response.\n",
						SecMan::sec_feat_act_rev[m_sec_man->sec_lookup_feat_act(m_auth_info, ATTR_SEC_ENACT)] );
				}

			} // end of case: consider establishing a new security session

			if( !m_is_tcp ) {
					// For UDP, if encryption is not on by default,
					// configure it with the session key so that it
					// can be programmatically toggled on and off for
					// portions of the message (e.g. for secret stuff
					// like claimids).  If encryption _is_ on by
					// default, then it will have already been turned
					// on by now, because the UDP header contains the
					// encryption key in that case.

				SecMan::sec_feat_act will_enable_encryption = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_ENCRYPTION);

				if( will_enable_encryption != SecMan::SEC_FEAT_ACT_YES
					&& m_key )
				{
					m_sock->set_crypto_key(false, m_key);
					dprintf(D_SECURITY, "DC_AUTHENTICATE: encryption enabled with session key id %s (but encryption mode is off by default for this packet).\n", m_sid ? m_sid : "(null)");
				}
			}

			if (m_is_tcp) {

				// do what we decided

				// handy policy vars
				SecMan::sec_feat_act will_authenticate      = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_AUTHENTICATION);
				m_will_enable_encryption = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_ENCRYPTION);
				m_will_enable_integrity  = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_INTEGRITY);


				// protocol fix:
				//
				// up to and including 6.6.0, will_authenticate would be set to
				// true if we are resuming a session that was authenticated.
				// this is not necessary.
				//
				// so, as of 6.6.1, if we are resuming a session (as determined
				// by the expression (!m_new_session), AND the other side is
				// 6.6.1 or higher, we will force will_authenticate to
				// SEC_FEAT_ACT_NO.

				if ( will_authenticate == SecMan::SEC_FEAT_ACT_YES ) {
					if ((!m_new_session)) {
						char * remote_version = NULL;
						m_policy->LookupString(ATTR_SEC_REMOTE_VERSION, &remote_version);
						if(remote_version) {
							// this attribute was added in 6.6.1.  it's mere
							// presence means that the remote side is 6.6.1 or
							// higher, so no need to instantiate a CondorVersionInfo.
							dprintf( D_SECURITY, "SECMAN: other side is %s, NOT reauthenticating.\n", remote_version );
							will_authenticate = SecMan::SEC_FEAT_ACT_NO;

							free (remote_version);
						} else {
							dprintf( D_SECURITY, "SECMAN: other side is pre 6.6.1, reauthenticating.\n" );
						}
					} else {
						dprintf( D_SECURITY, "SECMAN: new session, doing initial authentication.\n" );
					}
				}



				if (m_is_tcp && (will_authenticate == SecMan::SEC_FEAT_ACT_YES)) {

					m_state = CommandProtocolAuthenticate;
					return CommandProtocolContinue;


				} else {
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: not authenticating.\n");
					}
					m_state = CommandProtocolEnableCrypto;
					return CommandProtocolContinue;
				}
			} // end is_tcp
		} // end !using_cookie
	} // end DC_AUTHENTICATE

	// This path means they were using "old-school" commands.  No DC_AUTHENTICATE,
	// just raw command numbers.  We still want to do IPVerify on these however, so
	// skip all the Authentication and Crypto and go straight to that phase.
	m_state = CommandProtocolVerifyCommand;
	return CommandProtocolContinue;
}

DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::Authenticate()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: Authenticate()\n");

	if (m_errstack) { delete m_errstack;}
	m_errstack = new CondorError();

	if( m_nonblocking && !m_sock->readReady() ) {
		dprintf(D_SECURITY, "Returning to DC while we wait for socket to authenticate.\n");
		return WaitForSocketData();
	}

	// we know the ..METHODS_LIST attribute exists since it was put
	// in by us.  pre 6.5.0 protocol does not put it in.
	char * auth_methods = NULL;
	m_policy->LookupString(ATTR_SEC_AUTHENTICATION_METHODS_LIST, &auth_methods);

	if (!auth_methods) {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: no auth methods in response ad from %s, failing!\n", m_sock->peer_description());
		m_result = FALSE;
		return CommandProtocolFinished;
	}

	if (IsDebugVerbose(D_SECURITY)) {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: authenticating RIGHT NOW.\n");
	}

	int auth_timeout = daemonCore->getSecMan()->getSecTimeout( m_comTable[m_cmd_index].perm );

	m_sock->setAuthenticationMethodsTried(auth_methods);

	char *method_used = NULL;
	m_sock->setPolicyAd(*m_policy);
	int auth_success = m_sock->authenticate(m_key, auth_methods, m_errstack, auth_timeout, m_nonblocking, &method_used);
	m_sock->getPolicyAd(*m_policy);
	free( auth_methods );

	if (auth_success == 2) {
		m_state = CommandProtocolAuthenticateContinue;
		dprintf(D_SECURITY, "Will return to DC because authentication is incomplete.\n");
		return WaitForSocketData();
	}
        return AuthenticateFinish(auth_success, method_used);
}

DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::AuthenticateContinue()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: AuthenticateContinue()\n");

	char *method_used = NULL;
	int auth_result = m_sock->authenticate_continue(m_errstack, true, &method_used);
	if (auth_result == 2) {
		dprintf(D_SECURITY, "Will return to DC to continue authentication..\n");
		return WaitForSocketData();
	}
	return AuthenticateFinish(auth_result, method_used);
}

DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::AuthenticateFinish(int auth_success, char * method_used)
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: AuthenticateFinish(%i, %s)\n", auth_success, method_used?method_used:"(no authentication)");

	if ( method_used ) {
		m_policy->Assign(ATTR_SEC_AUTHENTICATION_METHODS, method_used);
	}
	if ( m_sock->getAuthenticatedName() ) {
		m_policy->Assign(ATTR_SEC_AUTHENTICATED_NAME, m_sock->getAuthenticatedName() );
	}

	if (!auth_success) {
		// call the auditing callback to record the authentication failure
		if (daemonCore->audit_log_callback_fn) {
			(*(daemonCore->audit_log_callback_fn))( m_auth_cmd, (*m_sock), true );
		}
	}

	free( method_used );

	if( m_comTable[m_cmd_index].force_authentication &&
		!m_sock->isMappedFQU() )
	{
		dprintf(D_ALWAYS, "DC_AUTHENTICATE: authentication of %s did not result in a valid mapped user name, which is required for this command (%d %s), so aborting.\n",
				m_sock->peer_description(),
				m_auth_cmd,
				m_comTable[m_cmd_index].command_descrip );
		if( !auth_success ) {
			dprintf( D_ALWAYS,
					 "DC_AUTHENTICATE: reason for authentication failure: %s\n",
					 m_errstack->getFullText().c_str() );
		}
		m_result = FALSE;
		return CommandProtocolFinished;
	}

	if( auth_success ) {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: authentication of %s complete.\n", m_sock->peer_ip_str());
		m_sock->getPolicyAd(*m_policy);
	}
	else {
		bool auth_required = true;
		m_policy->LookupBool(ATTR_SEC_AUTH_REQUIRED,auth_required);

		if( !auth_required ) {
			dprintf( D_SECURITY|D_FULLDEBUG,
					 "DC_SECURITY: authentication of %s failed but was not required, so continuing.\n",
					 m_sock->peer_ip_str());
			if( m_key ) {
					// Since we did not authenticate, we have not exchanged a key with our peer.
				delete m_key;
				m_key = NULL;
			}
		}
		else {
			dprintf( D_ALWAYS,
					 "DC_AUTHENTICATE: required authentication of %s failed: %s\n",
					 m_sock->peer_ip_str(),
					 m_errstack->getFullText().c_str() );
			m_result = FALSE;
			return CommandProtocolFinished;
		}
	}

	m_state = CommandProtocolEnableCrypto;
	return CommandProtocolContinue;
}

DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::EnableCrypto()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: EnableCrypto()\n");

	if (m_will_enable_integrity == SecMan::SEC_FEAT_ACT_YES) {

		if (!m_key) {
			// uhm, there should be a key here!
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		m_sock->decode();
		if (!m_sock->set_MD_mode(MD_ALWAYS_ON, m_key)) {
			dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on message authenticator, failing request from %s.\n", m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		} else {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: message authenticator enabled with key id %s.\n", m_sid);
			m_sec_man->key_printf (D_SECURITY, m_key);
		}
	} else {
		m_sock->set_MD_mode(MD_OFF, m_key);
	}


	if (m_will_enable_encryption == SecMan::SEC_FEAT_ACT_YES) {

		if (!m_key) {
			// uhm, there should be a key here!
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		m_sock->decode();
		if (!m_sock->set_crypto_key(true, m_key) ) {
			dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on encryption, failing request from %s.\n", m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		} else {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: encryption enabled for session %s\n", m_sid);
		}
	} else {
		m_sock->set_crypto_key(false, m_key);
	}

	m_state = CommandProtocolVerifyCommand;
	return CommandProtocolContinue;
}




DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::VerifyCommand()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: VerifyCommand()\n");

	CondorError errstack;

	if (m_req == DC_AUTHENTICATE) {

		// this is the opposite... we should assume false and set it true when
		// it is appropriate.
		m_result = TRUE;

		if (m_real_cmd == DC_SEC_QUERY) {
			// continue spoofing another command.  just before calling the command
			// handler we will check m_real_cmd again for DC_SEC_QUERY and abort.
			m_req = m_auth_cmd;
		} else {
			m_req = m_real_cmd;
		}

		// fill in the command info
		m_reqFound = TRUE;
		// is that always true?  we didn't even check?!?


		// I feel like we might need special cases for DC_AUTHENTICATE
		// and DC_SEC_QUERY at this point.  However, we must, in both cases
		// send the response ad first, which includes info on whether or not
		// the command was authorized.


		// This command _might_ be one with no further data.
		// Because of the way DC_AUTHENTICATE was implemented,
		// command handlers that call end_of_message() when
		// nothing more was sent by the peer will get an
		// error, because we have already consumed the end of
		// message.  Therefore, we set a flag on the socket:
		m_allow_empty = true;

		dprintf (D_SECURITY, "DC_AUTHENTICATE: Success.\n");
	} else {
		// we received some command other than DC_AUTHENTICATE
		// get the handler function
		m_reqFound = daemonCore->CommandNumToTableIndex(m_req,&m_cmd_index);

			// There are two cases where we get here:
			//  1. receiving unauthenticated command
			//  2. receiving command on previously authenticated socket

			// See if we should force authentication for this command.
			// The client is expected to do the same.
		if (m_reqFound &&
			m_is_tcp &&
			!m_sock->isAuthenticated() &&
			m_comTable[m_cmd_index].force_authentication &&
			!m_sock->triedAuthentication() )
		{
			SecMan::authenticate_sock(m_sock, WRITE, &errstack);
				// we don't check the return value, because the code below
				// handles what to do with unauthenticated connections
		}

		if (m_reqFound && !m_sock->isAuthenticated()) {
			// need to check our security policy to see if this is allowed.

			dprintf (D_SECURITY, "DaemonCore received UNAUTHENTICATED command %i %s.\n", m_req, m_comTable[m_cmd_index].command_descrip);

			// if the command was registered as "ALLOW", then it doesn't matter what the
			// security policy says, we just allow it.
			if (m_comTable[m_cmd_index].perm != ALLOW) {

				ClassAd *our_policy;
				if( ! m_sec_man->FillInSecurityPolicyAdFromCache(
					m_comTable[m_cmd_index].perm,
					our_policy,
					false,
					false,
					m_comTable[m_cmd_index].force_authentication ) )
				{
					dprintf( D_ALWAYS, "DC_AUTHENTICATE: "
							 "Our security policy is invalid!\n" );
					m_result = FALSE;
					return CommandProtocolFinished;
				}

				// well, they didn't authenticate, turn on encryption,
				// or turn on integrity.  check to see if any of those
				// were required.

				if (  (m_sec_man->sec_lookup_req(*our_policy, ATTR_SEC_NEGOTIATION)
					   == SecMan::SEC_REQ_REQUIRED)
				   || (m_sec_man->sec_lookup_req(*our_policy, ATTR_SEC_AUTHENTICATION)
					   == SecMan::SEC_REQ_REQUIRED)
				   || (m_sec_man->sec_lookup_req(*our_policy, ATTR_SEC_ENCRYPTION)
					   == SecMan::SEC_REQ_REQUIRED)
				   || (m_sec_man->sec_lookup_req(*our_policy, ATTR_SEC_INTEGRITY)
					   == SecMan::SEC_REQ_REQUIRED) ) {

					// yep, they were.  deny.

					dprintf(D_ALWAYS,
						"DaemonCore: PERMISSION DENIED for %d (%s) via %s%s%s from host %s (access level %s)\n",
						m_req,
						m_comTable[m_cmd_index].command_descrip,
						(m_is_tcp) ? "TCP" : "UDP",
						!m_user.empty() ? " from " : "",
						m_user.c_str(),
						m_sock->peer_description(),
						PermString(m_comTable[m_cmd_index].perm));

					m_result = FALSE;
					return CommandProtocolFinished;
				}
			}
		}
	}


	if ( m_reqFound == TRUE ) {

		// Check the daemon core permission for this command handler

		// When re-using security sessions, need to set the socket's
		// authenticated user name from the value stored in the cached
		// session.
		if( m_user.size() && !m_sock->isAuthenticated() ) {
			m_sock->setFullyQualifiedUser(m_user.c_str());
		}

		// grab the user from the socket
		if (m_is_tcp) {
			const char *u = m_sock->getFullyQualifiedUser();
			if (u) {
				m_user = u;
			}
		}

		std::string command_desc;
		formatstr(command_desc,"command %d (%s)",m_req,m_comTable[m_cmd_index].command_descrip);

		// this is the final decision on m_perm.  this is what matters.
		if( m_comTable[m_cmd_index].force_authentication &&
			!m_sock->isMappedFQU() )
		{
			dprintf(D_ALWAYS, "DC_AUTHENTICATE: authentication of %s did not result in a valid mapped user name, which is required for this command (%d %s), so aborting.\n",
					m_sock->peer_description(),
					m_req,
					m_comTable[m_cmd_index].command_descrip );

			m_perm = USER_AUTH_FAILURE;
		}
		else {
				// Authentication methods can limit the authorizations associated with
				// a given identity (at time of coding, only TOKEN does this); apply
				// these limits if present.
			std::string authz_policy;
			bool can_attempt = true;
			if (m_policy && m_policy->EvaluateAttrString(ATTR_SEC_LIMIT_AUTHORIZATION, authz_policy)) {
				StringList authz_limits(authz_policy.c_str());
				authz_limits.rewind();
				const char *perm_cstr = PermString(m_comTable[m_cmd_index].perm);
				const char *authz_name;
				bool found_limit = false;
				while ( (authz_name = authz_limits.next()) ) {
					if (!strcmp(perm_cstr, authz_name)) {
						found_limit = true;
						break;
					}
				}
					// If there was no match, iterate through the alternates table.
				if (!found_limit && m_comTable[m_cmd_index].alternate_perm) {
					for (auto perm : *m_comTable[m_cmd_index].alternate_perm) {
						auto perm_cstr = PermString(perm);
						const char *authz_name;
						authz_limits.rewind();
						while ( (authz_name = authz_limits.next()) ) {
							dprintf(D_ALWAYS, "Checking token limit %s\n", perm_cstr);
							if (!strcmp(perm_cstr, authz_name)) {
								found_limit = true;
								break;
							}
						}
						if (found_limit) {break;}
					}
				}
				if (!found_limit && strcmp(perm_cstr, "ALLOW")) {
					can_attempt = false;
				}
			}
			if (can_attempt) {
					// A bit redundant to have the outer conditional,
					// but this gets the log verbosity right and has
					// zero cost in the "normal" case with no alternate
					// permissions.
				if (m_comTable[m_cmd_index].alternate_perm) {
					m_perm = daemonCore->Verify(
						command_desc.c_str(),
						m_comTable[m_cmd_index].perm,
						m_sock->peer_addr(),
						m_user.c_str(),
						D_FULLDEBUG|D_SECURITY);
						// Not allowed in the primary permission?  Try the alternates.
					if (m_perm == USER_AUTH_FAILURE) {
						for (auto perm : *m_comTable[m_cmd_index].alternate_perm) {
							m_perm = daemonCore->Verify(
								command_desc.c_str(),
								perm,
								m_sock->peer_addr(),
								m_user.c_str(),
								D_FULLDEBUG|D_SECURITY);
							if (m_perm != USER_AUTH_FAILURE) {break;}
						}
							// Try it once again on the primary perm just to log
							// things appropriately loudly.
						if (m_perm == USER_AUTH_FAILURE) {
							daemonCore->Verify(
								command_desc.c_str(),
								m_comTable[m_cmd_index].perm,
								m_sock->peer_addr(),
								m_user.c_str());
						}
					}
				} else {
					m_perm = daemonCore->Verify(
						command_desc.c_str(),
						m_comTable[m_cmd_index].perm,
						m_sock->peer_addr(),
						m_user.c_str() );
				}
			} else {
				dprintf(D_ALWAYS, "DC_AUTHENTICATE: authentication of %s was successful but resulted in a limited authorization which did not include this command (%d %s), so aborting.\n",
					m_sock->peer_description(),
					m_req,
					m_comTable[m_cmd_index].command_descrip);
				m_perm = USER_AUTH_FAILURE;
			}
		}

	} else {
		// don't abort yet, we can send back the fact that the command
		// was unregistered in the post-auth classad.

		// also, i really dislike the following code, that modifies the state of the
		// socket in what should be a purely read-only operation of verification.

		// if UDP, consume the rest of this message to try to stay "in-sync"
		if ( !m_is_tcp)
			m_sock->end_of_message();
	}

	// call the auditing callback to record the status of this connection
	if (daemonCore->audit_log_callback_fn) {
		(*(daemonCore->audit_log_callback_fn))( m_req, (*m_sock), (m_perm != USER_AUTH_SUCCESS) );
	}

	m_state = CommandProtocolSendResponse;
	return CommandProtocolContinue;
}

DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::SendResponse()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: SendResponse()\n");

	if (m_new_session) {
		dprintf( D_DAEMONCORE, "DAEMONCORE: SendResponse() : m_new_session\n");

		// clear the buffer
		m_sock->decode();
		m_sock->end_of_message();

		// ready a classad to send
		ClassAd pa_ad;

		// session user
		const char *fully_qualified_user = m_sock->getFullyQualifiedUser();
		if ( fully_qualified_user ) {
			pa_ad.Assign(ATTR_SEC_USER,fully_qualified_user);
		}

		if (m_sock->triedAuthentication()) {
				// Clients older than 7.1.2 behave differently when re-using a
				// security session.  If they reach a point in the code where
				// authentication is forced (e.g. to submit jobs), they will
				// always re-authenticate at that point.  Therefore, we only
				// set TriedAuthentication=True for newer clients which respect
				// that setting.  When the setting is not there or false, the server
				// and client will re-authenticate at such points because
				// triedAuthentication() (or isAuthenticated() in the older code)
				// will be false.
			char * remote_version = NULL;
			m_policy->LookupString(ATTR_SEC_REMOTE_VERSION, &remote_version);
			CondorVersionInfo verinfo(remote_version);
			free(remote_version);

			if (verinfo.built_since_version(7,1,2)) {
				pa_ad.Assign(ATTR_SEC_TRIED_AUTHENTICATION,m_sock->triedAuthentication());
			}

		}

			// remember on the server side what we told the client
		m_sec_man->sec_copy_attribute( *m_policy, pa_ad, ATTR_SEC_TRIED_AUTHENTICATION );

		// session id
		pa_ad.Assign(ATTR_SEC_SID, m_sid);

		// other commands this session is good for
		pa_ad.Assign(ATTR_SEC_VALID_COMMANDS, daemonCore->GetCommandsInAuthLevel(m_comTable[m_cmd_index].perm,m_sock->isMappedFQU()));

		// what happened with command authorization?
		if(m_reqFound) {
			if(m_perm == USER_AUTH_SUCCESS) {
				pa_ad.Assign(ATTR_SEC_RETURN_CODE, "AUTHORIZED");
			} else {
				pa_ad.Assign(ATTR_SEC_RETURN_CODE, "DENIED");
			}
		} else {
			pa_ad.Assign(ATTR_SEC_RETURN_CODE, "CMD_NOT_FOUND");
		}

		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: sending session ad:\n");
			dPrintAd( D_SECURITY, pa_ad );
		}

		m_sock->encode();
		if (! putClassAd(m_sock, pa_ad) ||
			! m_sock->end_of_message() ) {
			dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to send session %s info to %s!\n", m_sid, m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		} else {
			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "DC_AUTHENTICATE: sent session %s info!\n", m_sid);
			}
		}


		// at this point, we can finally bail if we are not planning on
		// continuing (i.e. a succesful authorization).
		if (!m_reqFound || !(m_perm == USER_AUTH_SUCCESS)) {
			dprintf (D_ALWAYS, "DC_AUTHENTICATE: Command not authorized, done!\n");
			m_result = FALSE;
			return CommandProtocolFinished;
		}


		// also put some attributes in the policy classad we are caching.
		m_sec_man->sec_copy_attribute( *m_policy, m_auth_info, ATTR_SEC_SUBSYSTEM );
		m_sec_man->sec_copy_attribute( *m_policy, m_auth_info, ATTR_SEC_SERVER_COMMAND_SOCK );
		m_sec_man->sec_copy_attribute( *m_policy, m_auth_info, ATTR_SEC_PARENT_UNIQUE_ID );
		m_sec_man->sec_copy_attribute( *m_policy, m_auth_info, ATTR_SEC_SERVER_PID );
		// it matters if the version is empty, so we must explicitly delete it
		m_policy->Delete( ATTR_SEC_REMOTE_VERSION );
		m_sec_man->sec_copy_attribute( *m_policy, m_auth_info, ATTR_SEC_REMOTE_VERSION );
		m_sec_man->sec_copy_attribute( *m_policy, pa_ad, ATTR_SEC_USER );
		m_sec_man->sec_copy_attribute( *m_policy, pa_ad, ATTR_SEC_SID );
		m_sec_man->sec_copy_attribute( *m_policy, pa_ad, ATTR_SEC_VALID_COMMANDS );
		m_sock->setSessionID(m_sid);

		// extract the session duration
		char *dur = NULL;
		m_policy->LookupString(ATTR_SEC_SESSION_DURATION, &dur);

		char *return_addr = NULL;
		m_policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, &return_addr);

		// we add 20 seconds for "slop".  the idea is that if the client were
		// to start a session just as it was expiring, the server will allow a
		// window of 20 seconds to receive the command before throwing out the
		// cached session.
		int slop = param_integer("SEC_SESSION_DURATION_SLOP", 20);
		int durint = atoi(dur) + slop;
		time_t now = time(0);
		int expiration_time = now + durint;

		// extract the session lease time (max unused time)
		int session_lease = 0;
		m_policy->LookupInteger(ATTR_SEC_SESSION_LEASE, session_lease);
		if( session_lease ) {
				// Add some slop on the server side to avoid
				// expiration right before the client tries
				// to renew the lease.
			session_lease += slop;
		}


		// add the key to the cache

		// This is a session for incoming connections, so
		// do not pass in m_sock->peer_addr() as addr,
		// because then this key would get confused for an
		// outgoing session to a daemon with that IP and
		// port as its command socket.
		KeyCacheEntry tmp_key(m_sid, NULL, m_key, m_policy, expiration_time, session_lease );
		m_sec_man->session_cache->insert(tmp_key);
		dprintf (D_SECURITY, "DC_AUTHENTICATE: added incoming session id %s to cache for %i seconds (lease is %ds, return address is %s).\n", m_sid, durint, session_lease, return_addr ? return_addr : "unknown");
		if (IsDebugVerbose(D_SECURITY)) {
			dPrintAd(D_SECURITY, *m_policy);
		}

		free( dur );
		dur = NULL;
		free( return_addr );
		return_addr = NULL;
	} else {
		dprintf( D_DAEMONCORE, "DAEMONCORE: SendResponse() : NOT m_new_session\n");

		// at this point, we can finally bail if we are not planning on
		// continuing (i.e. a succesful authorization).
		if (!m_reqFound || !(m_perm == USER_AUTH_SUCCESS)) {
			dprintf (D_ALWAYS, "DC_AUTHENTICATE: Command not authorized, done!\n");
			m_result = FALSE;
			return CommandProtocolFinished;
		}
	}

	// what about DC_QUERY?  we want to stay in encode()
	if(m_allow_empty) {
		m_sock->decode();
		if( m_comTable[m_cmd_index].wait_for_payload == 0 ) {

				// This command _might_ be one with no further data.
				// Because of the way DC_AUTHENTICATE was implemented,
				// command handlers that call end_of_message() when
				// nothing more was sent by the peer will get an
				// error, because we have already consumed the end of
				// message.  Therefore, we set a flag on the socket:

			m_sock->allow_one_empty_message();
		}
	}

	m_state = CommandProtocolExecCommand;
	return CommandProtocolContinue;
}

// Call the command handle for the requested command.
// Authorization is first verified.
DaemonCommandProtocol::CommandProtocolResult DaemonCommandProtocol::ExecCommand()
{
	dprintf( D_DAEMONCORE, "DAEMONCORE: ExecCommand(m_req == %i, m_real_cmd == %i, m_auth_cmd == %i)\n",
		 m_req, m_real_cmd, m_auth_cmd);

	// There is no command handler for DC_AUTHENTICATE.
	//
	// Sending DC_AUTHENTICATE as the m_real_cmd means a NO-OP.  This will
	// create a session without actually executing any commands.  This is
	// used to create a session using TCP that UDP commands can then resume
	// without any round-trips.
	if (m_real_cmd == DC_AUTHENTICATE) {
		dprintf( D_DAEMONCORE, "DAEMONCORE: ExecCommand : m_real_cmd was DC_AUTHENTICATE. NO-OP.\n");
		m_result = TRUE;
		return CommandProtocolFinished;
	}

	// DC_SEC_QUERY is also a special case.  We are not going to call a command
	// handler, but we need to respond whether or not the authorization for that
	// command would have succeeded.
	if ( m_real_cmd == DC_SEC_QUERY ) {

		// send another classad saying what happened
		ClassAd q_response;
		q_response.Assign( ATTR_SEC_AUTHORIZATION_SUCCEEDED, (m_perm == USER_AUTH_SUCCESS) );

		if (!putClassAd(m_sock, q_response) ||
			!m_sock->end_of_message()) {
			dprintf (D_ALWAYS, "SECMAN: Error sending DC_SEC_QUERY classad to %s!\n", m_sock->peer_description());
			dPrintAd (D_ALWAYS, q_response);
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		dprintf (D_ALWAYS, "SECMAN: Succesfully sent DC_SEC_QUERY classad to %s!\n", m_sock->peer_description());
		dPrintAd (D_ALWAYS, q_response);

		// now, having informed the client about the authorization status,
		// successfully abort before actually calling any command handler.
		m_result = TRUE;
		return CommandProtocolFinished;
	}

	if ( m_reqFound == TRUE ) {
		// Handlers should start out w/ parallel mode disabled by default
		ScopedEnableParallel(false);

		struct timeval handler_start_time;
		condor_gettimestamp( handler_start_time );
		double sec_time = timersub_double( handler_start_time, m_handle_req_start_time );
		sec_time -= m_async_waiting_time;

		if( m_sock_had_no_deadline ) {
				// unset the deadline we assigned in WaitForSocketData
			m_sock->set_deadline(0);
		}

		double begin_time = _condor_debug_get_time_double(); // dc_stats.AddRuntime uses this as a timebase, not UtcTime

		m_result = daemonCore->CallCommandHandler(m_req,m_sock,false /*do not delete m_sock*/,true /*do check for payload*/,sec_time,0);

		// update dc stats for number of commands handled, the time spent in this command handler
		daemonCore->dc_stats.Commands += 1;
		daemonCore->dc_stats.AddRuntime(getCommandStringSafe(m_req), begin_time);
	}

	return CommandProtocolFinished;
}


int DaemonCommandProtocol::finalize()
{
	// the handler is done with the command.  the handler will return
	// with KEEP_STREAM if we should not touch the sock; otherwise, cleanup
	// the sock.  On tcp, we just delete it since the sock is the one we got
	// from accept and our listen socket is still out there.  on udp,
	// however, we cannot just delete it or we will not be "listening"
	// anymore, so we just do an eom flush all buffers, etc.
	// HACK: keep all UDP sockets as well for now.
	if ( m_result != KEEP_STREAM ) {
		if ( m_is_tcp ) {
			m_sock->encode();	// we wanna "flush" below in the encode direction
			m_sock->end_of_message();  // make certain data flushed to the wire
		} else {
			m_sock->decode();
			m_sock->end_of_message();

			// we need to reset the crypto keys
			m_sock->set_MD_mode(MD_OFF);
			m_sock->set_crypto_key(false, NULL);

			// we also need to reset the FQU
			m_sock->setFullyQualifiedUser(NULL);
		}

		if( m_delete_sock ) {
			delete m_sock;
			m_sock = NULL;
		}
	} else {
		if (!m_is_tcp) {
			m_sock->decode();
			m_sock->end_of_message();
			m_sock->set_MD_mode(MD_OFF);
			m_sock->set_crypto_key(false, NULL);
			m_sock->setFullyQualifiedUser(NULL);
		}
	}


	if ( m_result == KEEP_STREAM || m_sock == NULL )
		return KEEP_STREAM;
	else
		return TRUE;
}
