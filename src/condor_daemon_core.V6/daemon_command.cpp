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
#include "condor_base64.h"


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
}

int DaemonCommandProtocol::doProtocol()
{

	CommandProtocolResult what_next = CommandProtocolContinue;

	if( m_sock ) {
		if( m_sock->deadline_expired() ) {
			dprintf(D_ERROR,"DaemonCommandProtocol: deadline for security handshake with %s has expired.\n",
					m_sock->peer_description());

			m_result = FALSE;
			what_next = CommandProtocolFinished;
		}
		else if( m_nonblocking && m_sock->is_connect_pending() ) {
			dprintf(D_SECURITY, "DaemonCommandProtocol: Waiting for connect.\n");
			what_next = WaitForSocketData();
		}
		else if( m_is_tcp && !m_sock->is_connected()) {
			dprintf(D_ERROR,"DaemonCommandProtocol: TCP connection to %s failed.\n",
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
			HANDLE_READ,
			&m_prev_sock_ent);

	if(reg_rc < 0) {
		dprintf(D_ERROR, "DaemonCommandProtocol failed to process command from %s because "
				"Register_Socket returned %d.\n",
				m_sock->get_sinful_peer(),
				reg_rc);

		m_result = FALSE;
		return CommandProtocolFinished;
	}

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
			StringTokenIterator info_list(cleartext_info);
			const char * tmp = nullptr;

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
				// protocol violation... We didn't get anything!
				// this is unlikely to work, but we may as well try... so, we
				// don't fail here.
			}
		}

		if (sess_id) {
			KeyCacheEntry *session = NULL;
			auto sess_itr = m_sec_man->session_cache->find(sess_id);
			if (sess_itr == m_sec_man->session_cache->end()) {
				dprintf ( D_ERROR, "DC_AUTHENTICATE: session %s NOT FOUND; this session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
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
			} else {
				session = &sess_itr->second;
			}

			session->renewLease();

			if (!session->key()) {
				dprintf ( D_ERROR, "DC_AUTHENTICATE: session %s is missing the key! This session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
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
				dprintf (D_ERROR, "DC_AUTHENTICATE: unable to turn on message authenticator for session %s, failing; this session was requested by %s with return address %s\n",sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
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
			StringTokenIterator info_list(cleartext_info);
			const char* tmp = nullptr;

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
				// protocol violation... We didn't get anything!
				// this is unlikely to work, but we may as well try... so, we
				// don't fail here.
			}
		}


		if (sess_id) {
			KeyCacheEntry *session = NULL;
			auto sess_itr = m_sec_man->session_cache->find(sess_id);
			if (sess_itr == m_sec_man->session_cache->end()) {
				dprintf ( D_ERROR, "DC_AUTHENTICATE: session %s NOT FOUND; this session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
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
			} else {
				session = &sess_itr->second;
			}

			session->renewLease();

			if (!session->key()) {
				dprintf ( D_ERROR, "DC_AUTHENTICATE: session %s is missing the key! This session was requested by %s with return address %s\n", sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
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

			KeyInfo* key_to_use;
			KeyInfo* fallback_key;

			// There's no AES support for UDP, so fall back to
			// BLOWFISH (default) or 3DES if specified.  3DES would
			// be preferred in FIPS mode.
			std::string fallback_method_str = "BLOWFISH";
			Protocol fallback_method = CONDOR_BLOWFISH;
			if (param_boolean("FIPS", false)) {
				fallback_method_str = "3DES";
				fallback_method = CONDOR_3DES;
			}
			dprintf(D_SECURITY|D_VERBOSE, "SESSION: fallback crypto method would be %s.\n", fallback_method_str.c_str());

			key_to_use = session->key();
			fallback_key = session->key(fallback_method);

			dprintf(D_NETWORK|D_VERBOSE, "UDP: server normal key (proto %i): %p\n", key_to_use->getProtocol(), key_to_use);
			dprintf(D_NETWORK|D_VERBOSE, "UDP: server %s key (proto %i): %p\n",
				fallback_method_str.c_str(),
				(fallback_key ? fallback_key->getProtocol() : 0), fallback_key);
			dprintf(D_NETWORK|D_VERBOSE, "UDP: server m_is_tcp: 0\n");

			// this is UDP.  if we were going to use AES, use fallback instead (if it exists)
			if((key_to_use->getProtocol() == CONDOR_AESGCM) && (fallback_key)) {
				dprintf(D_NETWORK, "UDP: SWITCHING FROM AES TO %s.\n", fallback_method_str.c_str());
				key_to_use = fallback_key;
			}

			if (!m_sock->set_crypto_key(turn_encryption_on, key_to_use)) {
				dprintf (D_ERROR, "DC_AUTHENTICATE: unable to turn on encryption for session %s, failing; this session was requested by %s with return address %s\n",sess_id, m_sock->peer_description(), return_address_ss ? return_address_ss : "(none)");
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
		// Note: We no longer support soap, but this peek is part of the
		//   code below that checks if this is a command that shared port
		//   should transparently hand off to the collector.
	char tmpbuf[6];
	memset(tmpbuf,0,sizeof(tmpbuf));
	if ( m_is_tcp && daemonCore->HandleUnregistered() ) {
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
		dprintf(D_ERROR,
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
			dprintf (D_ERROR, "ERROR: DC_AUTHENTICATE unable to "
					 "receive auth_info from %s!\n", m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		if ( m_is_tcp && !m_sock->end_of_message()) {
			dprintf (D_ERROR, "ERROR: DC_AUTHENTICATE is TCP, unable to "
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

			dprintf(D_ERROR,
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
		std::string incoming_cookie;
		if( m_auth_info.LookupString(ATTR_SEC_COOKIE, incoming_cookie)) {
			// compare it to the one we have internally

			valid_cookie = daemonCore->cookie_is_valid((const unsigned char *)incoming_cookie.c_str());

			if ( valid_cookie ) {
				// we have a match... trust this command.
				using_cookie = true;
			} else {
				// bad cookie!!!
				dprintf ( D_ERROR, "DC_AUTHENTICATE: received invalid cookie from %s!!!\n", m_sock->peer_description());
				m_result = FALSE;
				return CommandProtocolFinished;
			}
		}

		// check if we are restarting a cached session

		if (!using_cookie) {

			if ( m_sec_man->sec_lookup_feat_act(m_auth_info, ATTR_SEC_USE_SESSION) == SecMan::SEC_FEAT_ACT_YES) {

				KeyCacheEntry *session = NULL;

				if( ! m_auth_info.LookupString(ATTR_SEC_SID, m_sid)) {
					dprintf (D_ERROR, "ERROR: DC_AUTHENTICATE unable to "
							 "extract auth_info.%s from %s!\n", ATTR_SEC_SID,
							 m_sock->peer_description());
					m_result = FALSE;
					return CommandProtocolFinished;
				}
				m_auth_info.Delete(ATTR_SEC_NONCE);

				// lookup the suggested key
				auto sess_itr = m_sec_man->session_cache->find(m_sid);
				if (sess_itr == m_sec_man->session_cache->end()) {

					// the key id they sent was not in our cache.  this is a
					// problem.

					std::string return_addr;
					m_auth_info.LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, return_addr);
					dprintf (D_ERROR, "DC_AUTHENTICATE: attempt to open "
					         "invalid session %s, failing; this session was requested by %s with return address %s\n", m_sid.c_str(), m_sock->peer_description(), return_addr.empty() ? "(none)" : return_addr.c_str());
					if( !strncmp( m_sid.c_str(), "family:", strlen("family:") ) ) {
						dprintf(D_ERROR, "  The remote daemon thinks that we are in the same family of Condor daemon processes as it, but I don't recognize its family security session.\n");
						dprintf(D_ERROR, "  If we are in the same family of processes, you may need to change how the configuration parameter SEC_USE_FAMILY_SESSION is set.\n");
					}

					bool want_resume_response = false;
					m_auth_info.LookupBool(ATTR_SEC_RESUME_RESPONSE, want_resume_response);
					if (want_resume_response) {
						// New client, tell them here that we don't like
						// their session id.
						ClassAd resp_ad;
						resp_ad.Assign(ATTR_SEC_RETURN_CODE, "SID_NOT_FOUND");

						m_sock->encode();
						if (!putClassAd(m_sock, resp_ad) || !m_sock->end_of_message()) {
							dprintf(D_ERROR, "DC_AUTHENTICATE: Failed to send unknown session reply to peer at %s.\n", m_sock->peer_description());
						}
					} else {
						// Old client (pre-9.9.0), send out-of-band
						// notification of invalid session.
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

						if( !return_addr.empty() ) {
							daemonCore->send_invalidate_session( return_addr.c_str(), m_sid.c_str(), &info_ad );
						}

						// consume the rejected message
						m_sock->decode();
						m_sock->end_of_message();
					}

					// close the connection.
					m_result = FALSE;
					return CommandProtocolFinished;

				} else {
					// the session->id() and the_sid strings should be identical.
					session = &sess_itr->second;

					if (IsDebugLevel(D_SECURITY)) {
						std::string return_addr;
						if(session->policy()) {
							session->policy()->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK,return_addr);
						}
						dprintf (D_SECURITY, "DC_AUTHENTICATE: resuming session id %s%s%s:\n",
								 session->id().c_str(),
								 !return_addr.empty() ? " with return address " : "",
								 return_addr.c_str());
					}
				}

				session->renewLease();

				// For a non-negotiated session, remember the version of
				// our peer in case we use the session later as a client.
				// TODO We only need this for non-negotiated sessions.
				//   Should we limit this to them?
				if (!peer_version.empty()) {
					session->setLastPeerVersion(peer_version);
				} else {
					// Old verions (pre-9.9.0) don't send their version
					// in the resume session ad.
					// The important thing here is that they're older than
					// 9.9.0 and thus don't know about the server response
					// ad when resuming a session.
					CondorVersionInfo ver_info(9, 8, 0, "FakeOldVersion");
					char* ver_str = ver_info.get_version_string();
					session->setLastPeerVersion(ver_str);
					free(ver_str);
				}

				// If the session has multiple crypto methods, we need to
				// figure out which one to use.
				// If the client's resume session ad gives a crypto method
				// list, use the first entry in it.
				// Otherwise, the client is 8.9, so if we have a list, use
				// the first entry that's BLOWFISH or 3DES.
				// Whichever one we use, set it as the preferred method
				// for the session, in case we need to connect to our
				// peer in the future.
				std::string session_crypto_methods;
				session->policy()->LookupString(ATTR_SEC_CRYPTO_METHODS, session_crypto_methods);
				if (session_crypto_methods.find(',') != std::string::npos) {
					std::string client_crypto_methods;
					Protocol active_crypto = CONDOR_NO_PROTOCOL;
					if (m_auth_info.LookupString(ATTR_SEC_CRYPTO_METHODS, client_crypto_methods)) {
						active_crypto = SecMan::getCryptProtocolNameToEnum(client_crypto_methods.c_str());
					} else {
						std::string pick_crypto;
						pick_crypto = SecMan::getPreferredOldCryptProtocol(session_crypto_methods);
						active_crypto = SecMan::getCryptProtocolNameToEnum(pick_crypto.c_str());
					}
					session->setPreferredProtocol(active_crypto);
				}

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
					std::string tmp;
					m_policy->LookupString( ATTR_SEC_USER, tmp);
					if (!tmp.empty()) {
						// copy this to the HandleReq() scope
						m_user = tmp;
						tmp.clear();
					}
					m_policy->LookupString( ATTR_SEC_AUTHENTICATED_NAME, tmp);
					if (!tmp.empty()) {
						// copy this to the HandleReq() scope
						m_sock->setAuthenticatedName(tmp.c_str());
						tmp.clear();
					}
					m_policy->LookupString( ATTR_SEC_AUTHENTICATION_METHODS, tmp);
					if (!tmp.empty()) {
						// copy this to the HandleReq() scope
						m_sock->setAuthenticationMethodUsed(tmp.c_str());
						tmp.clear();
					}

					m_policy->LookupString( ATTR_SEC_REMOTE_VERSION, peer_version );

					bool tried_authentication=false;
					m_policy->LookupBool(ATTR_SEC_TRIED_AUTHENTICATION,tried_authentication);
					m_sock->setTriedAuthentication(tried_authentication);
					m_sock->setSessionID(session->id());
					m_sock->setPolicyAd(*m_policy);
				}

				// If the cached policy doesn't have a version, then
				// we're dealing with a non-negotaited session from an
				// old peer (pre-9.9.0).
				// In that case, clear out the socket's peer version.
				// This maintains symmetry of version info between
				// client and server.
				if ( peer_version.empty() ) {
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
					dprintf( D_ERROR, "DC_AUTHENTICATE: "
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
					dprintf(D_ERROR, "DC_AUTHENTICATE: Unable to reconcile!\n");
					m_result = FALSE;
					return CommandProtocolFinished;
				} else {
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf ( D_SECURITY, "DC_AUTHENTICATE: the_policy:\n" );
						dPrintAd(D_SECURITY, *m_policy);
					}
				}

				m_policy->Assign(ATTR_SEC_NEGOTIATED_SESSION, true);

				// add our version to the policy to be sent over
				m_policy->Assign(ATTR_SEC_REMOTE_VERSION, CondorVersion());

				// handy policy vars
				SecMan::sec_feat_act will_authenticate      = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_AUTHENTICATION);

				if (m_sec_man->sec_lookup_feat_act(m_auth_info, ATTR_SEC_NEW_SESSION) == SecMan::SEC_FEAT_ACT_YES) {

					// generate a new session

					// generate a unique ID.
					formatstr( m_sid, "%s:%i:%lld:%i",
									get_local_hostname().c_str(), daemonCore->mypid,
							   (long long)time(0), ZZZ_always_increase() );

					std::string peer_ec;
					m_auth_info.EvaluateAttrString(ATTR_SEC_ECDH_PUBLIC_KEY, peer_ec);

					if (will_authenticate == SecMan::SEC_FEAT_ACT_YES || !peer_ec.empty()) {
						std::string crypto_method;
						if (!m_policy->LookupString(ATTR_SEC_CRYPTO_METHODS, crypto_method)) {
							dprintf ( D_ERROR, "DC_AUTHENTICATE: tried to enable encryption for request from %s, but we have none!\n", m_sock->peer_description() );
							m_result = false;
							return CommandProtocolFinished;
						}
						Protocol method = SecMan::getCryptProtocolNameToEnum(crypto_method.c_str());

							// If the client provided a EC key, we'll respond in kind.
						if (!peer_ec.empty()) {
							m_peer_pubkey_encoded = peer_ec;
							if (!(m_keyexchange = SecMan::GenerateKeyExchange(m_errstack))) {
								dprintf(D_ERROR, "DC_AUTHENTICATE: Error in generating key: %s\n", m_errstack->getFullText().c_str());
								return CommandProtocolFinished;
							}
							std::string encoded_pubkey;
							if (!SecMan::EncodePubkey(m_keyexchange.get(), encoded_pubkey, m_errstack)) {
								dprintf(D_ERROR, "DC_AUTHENTICATE: Error in encoded key: %s\n", m_errstack->getFullText().c_str());
								return CommandProtocolFinished;
							}
							if (!m_policy->InsertAttr(ATTR_SEC_ECDH_PUBLIC_KEY, encoded_pubkey)) {
								dprintf(D_ERROR, "DC_AUTHENTICATE: Failed to add pubkey to policy ad.\n");
								return CommandProtocolFinished;
							}

								// We will derive the key post-authentication.
							m_key = nullptr;
						} else {
								// Server will explicitly send the key to the client during the authentication; we need
								// to generate it first.

							unsigned char* rkey = Condor_Crypt_Base::randomKey(SEC_SESSION_KEY_LENGTH_V9);
							unsigned char  rbuf[SEC_SESSION_KEY_LENGTH_V9];
							if (rkey) {
								memcpy (rbuf, rkey, SEC_SESSION_KEY_LENGTH_V9);
								// this was malloced in randomKey
								free (rkey);
							} else {
								memset (rbuf, 0, SEC_SESSION_KEY_LENGTH_V9);
								dprintf ( D_ERROR, "DC_AUTHENTICATE: unable to generate key for request from %s - no crypto available!\n", m_sock->peer_description() );
								m_result = FALSE;
								return CommandProtocolFinished;
							}
							switch (method) {
								case CONDOR_BLOWFISH:
									dprintf (D_SECURITY, "DC_AUTHENTICATE: generating BLOWFISH key for session %s...\n", m_sid.c_str());
									m_key = new KeyInfo(rbuf, SEC_SESSION_KEY_LENGTH_OLD, CONDOR_BLOWFISH, 0);
									break;
								case CONDOR_3DES:
									dprintf (D_SECURITY, "DC_AUTHENTICATE: generating 3DES key for session %s...\n", m_sid.c_str());
									m_key = new KeyInfo(rbuf, SEC_SESSION_KEY_LENGTH_OLD, CONDOR_3DES, 0);
									break;
								case CONDOR_AESGCM: {
									dprintf (D_SECURITY, "DC_AUTHENTICATE: generating AES-GCM key for session %s...\n", m_sid.c_str());
									m_key = new KeyInfo(rbuf, SEC_SESSION_KEY_LENGTH_V9, CONDOR_AESGCM, 0);
									}
									break;
								default:
									dprintf (D_SECURITY, "DC_AUTHENTICATE: generating RANDOM key for session %s...\n", m_sid.c_str());
									m_key = new KeyInfo(rbuf, SEC_SESSION_KEY_LENGTH_OLD, CONDOR_NO_PROTOCOL, 0);
									break;
							}

							if (!m_key) {
								m_result = FALSE;
								return CommandProtocolFinished;
							}

							m_sec_man->key_printf (D_SECURITY, m_key);
						}

						// Update the session policy to reflect the
						// crypto method we're using
						m_policy->Assign(ATTR_SEC_CRYPTO_METHODS, SecMan::getCryptProtocolEnumToName(method));
					} else {
						m_policy->Delete(ATTR_SEC_CRYPTO_METHODS);
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
						dprintf (D_ERROR, "SECMAN: Error sending response classad to %s!\n", m_sock->peer_description());
						dPrintAd (D_ERROR, m_auth_info);
						m_result = FALSE;
						return CommandProtocolFinished;
					}
					// We don't need our encoded pubkey anymore
					m_policy->Delete(ATTR_SEC_ECDH_PUBLIC_KEY);
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
					dprintf(D_SECURITY, "DC_AUTHENTICATE: encryption enabled with session key id %s (but encryption mode is off by default for this packet).\n", m_sid.c_str());
				}
			}

			if (m_is_tcp) {

				// do what we decided

				// handy policy vars
				SecMan::sec_feat_act will_authenticate      = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_AUTHENTICATION);
				m_will_enable_encryption = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_ENCRYPTION);
				m_will_enable_integrity  = m_sec_man->sec_lookup_feat_act(*m_policy, ATTR_SEC_INTEGRITY);


				// When resuming an existing security session, will_authenticate
				// reflects the original decision about whether to authenticate.
				// We don't want to re-authenticate when resuming, so force
				// will_authenticate to SEC_FEAT_ACT_NO in that case.
				if ( will_authenticate == SecMan::SEC_FEAT_ACT_YES ) {
					if ((!m_new_session)) {
						dprintf( D_SECURITY, "SECMAN: resume, NOT reauthenticating.\n" );
						will_authenticate = SecMan::SEC_FEAT_ACT_NO;

					} else {
						dprintf( D_SECURITY, "SECMAN: new session, doing initial authentication.\n" );
					}
				}

				if (!m_new_session) {
					bool want_resume_response = false;
					m_auth_info.LookupBool(ATTR_SEC_RESUME_RESPONSE, want_resume_response);
					if (want_resume_response) {
						dprintf(D_SECURITY|D_FULLDEBUG, "DC_AUTHENTICATE: sending response to client's resume session request.\n");
						std::unique_ptr<unsigned char, decltype(&free)> random_bytes(Condor_Crypt_Base::randomKey(33), &free);
						std::unique_ptr<char, decltype(&free)> encoded_bytes(
							condor_base64_encode(random_bytes.get(), 33, false),
							&free);

						ClassAd ad;
						ad.Assign(ATTR_SEC_RETURN_CODE, "AUTHORIZED");
						ad.Assign(ATTR_SEC_REMOTE_VERSION, CondorVersion());
						if (!ad.InsertAttr(ATTR_SEC_NONCE, encoded_bytes.get())) {
							dprintf(D_ERROR, "DC_AUTHENTICATE: Failed to generate nonce to send for session resumption.\n");
							m_result = false;
							return CommandProtocolFinished;
						}
						m_sock->encode();
						if (!putClassAd(m_sock, ad) || !m_sock->end_of_message()) {
							dprintf(D_ERROR, "DC_AUTHENTICATE: Failed to send nonce to peer at %s.\n", m_sock->peer_description());
							m_result = false;
							return CommandProtocolFinished;
						}
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

	dprintf( D_DAEMONCORE, "DAEMONCORE: Leaving ReadCommand(m_req==%i)\n", m_req);

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
	std::string auth_methods;
	m_policy->LookupString(ATTR_SEC_AUTHENTICATION_METHODS_LIST, auth_methods);

	if (auth_methods.empty()) {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: no auth methods in response ad from %s, failing!\n", m_sock->peer_description());
		m_result = FALSE;
		return CommandProtocolFinished;
	}

	if (IsDebugVerbose(D_SECURITY)) {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: authenticating RIGHT NOW.\n");
	}

	int auth_timeout = daemonCore->getSecMan()->getSecTimeout( m_comTable[m_cmd_index].perm );

	m_sock->setAuthenticationMethodsTried(auth_methods.c_str());

	char *method_used = NULL;
	m_sock->setPolicyAd(*m_policy);
	int auth_success = m_sock->authenticate(m_key, auth_methods.c_str(), m_errstack, auth_timeout, m_nonblocking, &method_used);
	m_sock->getPolicyAd(*m_policy);

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

		// For CLAIMTOBE, explicitly limit the authorized permission
		// levels to that of the current command and any implied ones.
		if ( !strcasecmp(method_used, "CLAIMTOBE") ) {
			std::string perm_list;
			DCpermission perm = m_comTable[m_cmd_index].perm;
			for ( ; perm < LAST_PERM; perm = DCpermissionHierarchy::nextImplied(perm)) {
				if (!perm_list.empty()) {
					perm_list += ',';
				}
				perm_list += PermString(perm);
			}
			m_policy->Assign(ATTR_SEC_LIMIT_AUTHORIZATION, perm_list);
		}
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
		dprintf(D_ERROR, "DC_AUTHENTICATE: authentication of %s did not result in a valid mapped user name, which is required for this command (%d %s), so aborting.\n",
				m_sock->peer_description(),
				m_auth_cmd,
				m_comTable[m_cmd_index].command_descrip );
		if( !auth_success ) {
			dprintf( D_ERROR,
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
			dprintf( D_ERROR,
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

	// If we're doing ECDH, derive the session key now
	if (m_keyexchange) {
		std::string crypto_method;
		if (!m_policy->LookupString(ATTR_SEC_CRYPTO_METHODS, crypto_method)) {
			dprintf ( D_ERROR, "DC_AUTHENTICATE: No crypto methods enabled for request from %s.\n", m_sock->peer_description() );
			m_result = false;
			return CommandProtocolFinished;
		}
		Protocol method = SecMan::getCryptProtocolNameToEnum(crypto_method.c_str());
		size_t keylen = method == CONDOR_AESGCM ? SEC_SESSION_KEY_LENGTH_V9 : SEC_SESSION_KEY_LENGTH_OLD;
		std::unique_ptr<unsigned char, decltype(&free)> rbuf(
			static_cast<unsigned char *>(malloc(keylen)),
			&free);
		if (!SecMan::FinishKeyExchange(std::move(m_keyexchange), m_peer_pubkey_encoded.c_str(), rbuf.get(), keylen, m_errstack)) {
			dprintf(D_ERROR, "DC_AUTHENTICATE: Failed to generate a symmetric key for session with %s: %s.\n", m_sock->peer_description(), m_errstack->getFullText().c_str());
			m_result = false;
			return CommandProtocolFinished;
		}

		dprintf (D_SECURITY, "DC_AUTHENTICATE: generating %s key for session %s...\n", crypto_method.c_str(), m_sid.c_str());
		m_key = new KeyInfo(rbuf.get(), keylen, method, 0);
	}

	// We must configure encryption before integrity on the socket
	// when using AES over TCP. If we do integrity first, then ReliSock
	// will initialize an MD5 context and then tear it down when it
	// learns it's doing AES. This breaks FIPS compliance.
	if (m_will_enable_encryption == SecMan::SEC_FEAT_ACT_YES) {

		if (!m_key) {
			// uhm, there should be a key here!
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		m_sock->decode();
		if (!m_sock->set_crypto_key(true, m_key) ) {
			dprintf (D_ERROR, "DC_AUTHENTICATE: unable to turn on encryption, failing request from %s.\n", m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		} else {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: encryption enabled for session %s\n", m_sid.c_str());
		}
	} else {
		m_sock->set_crypto_key(false, m_key);
	}

	if (m_will_enable_integrity == SecMan::SEC_FEAT_ACT_YES) {

		if (!m_key) {
			// uhm, there should be a key here!
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		m_sock->decode();

		// if the encryption method is AES, we don't actually want to enable the MAC
		// here as that instantiates an MD5 object which will cause FIPS to blow up.
		bool result;
		if(m_key->getProtocol() != CONDOR_AESGCM) {
			result = m_sock->set_MD_mode(MD_ALWAYS_ON, m_key);
		} else {
			dprintf(D_SECURITY|D_VERBOSE, "SECMAN: because protocal is AES, not using other MAC.\n");
			result = m_sock->set_MD_mode(MD_OFF, m_key);
		}

		if (!result) {
			dprintf (D_ERROR, "DC_AUTHENTICATE: unable to turn on message authenticator, failing request from %s.\n", m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		} else {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: message authenticator enabled with key id %s.\n", m_sid.c_str());
			m_sec_man->key_printf (D_SECURITY, m_key);
		}
	} else {
		m_sock->set_MD_mode(MD_OFF, m_key);
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
					dprintf( D_ERROR, "DC_AUTHENTICATE: "
							 "Our security policy is invalid!\n" );
					m_result = FALSE;
					return CommandProtocolFinished;
				}

				// Well, they didn't authenticate. See if that was required.
				// Also, if security negotiation is required, see if they
				// have a security session.

				if (  (m_sec_man->sec_lookup_req(*our_policy, ATTR_SEC_NEGOTIATION)
					   == SecMan::SEC_REQ_REQUIRED && m_sock->getSessionID().empty())
				   || (m_sec_man->sec_lookup_req(*our_policy, ATTR_SEC_AUTHENTICATION_NEW)
					   == SecMan::SEC_REQ_REQUIRED) ) {

					// yep, negotiation or authentication was required and
					// they didn't do it.  deny.

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
			dprintf(D_ERROR, "DC_AUTHENTICATE: authentication of %s did not result in a valid mapped user name, which is required for this command (%d %s), so aborting.\n",
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
			const ClassAd* policy_ad = m_policy ? m_policy : m_sock->getPolicyAd();
			if (policy_ad && policy_ad->EvaluateAttrString(ATTR_SEC_LIMIT_AUTHORIZATION, authz_policy)) {
				std::set<DCpermission> authz_limits;
				for (const auto& limit_str: StringTokenIterator(authz_policy)) {
					DCpermission limit_perm = getPermissionFromString(limit_str.c_str());
					if (limit_perm != NOT_A_PERM) {
						authz_limits.insert(limit_perm);
						while ((limit_perm = DCpermissionHierarchy::nextImplied(limit_perm)) < LAST_PERM) {
							authz_limits.insert(limit_perm);
						}
					}
				}
				bool found_limit = authz_limits.count(m_comTable[m_cmd_index].perm) > 0;
				const char *perm_cstr = PermString(m_comTable[m_cmd_index].perm);
				bool has_allow_perm = !strcmp(perm_cstr, "ALLOW");
					// If there was no match, iterate through the alternates table.
				if (!found_limit && m_comTable[m_cmd_index].alternate_perm) {
					for (auto perm : *m_comTable[m_cmd_index].alternate_perm) {
						perm_cstr = PermString(perm);
						has_allow_perm |= !strcmp(perm_cstr, "ALLOW");
						found_limit = authz_limits.count(perm) > 0;
						if (found_limit) {break;}
					}
				}
				if (!found_limit && !has_allow_perm) {
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
			pa_ad.Assign(ATTR_SEC_TRIED_AUTHENTICATION,m_sock->triedAuthentication());
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
			dprintf (D_ERROR, "DC_AUTHENTICATE: unable to send session %s info to %s!\n", m_sid.c_str(), m_sock->peer_description());
			m_result = FALSE;
			return CommandProtocolFinished;
		} else {
			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "DC_AUTHENTICATE: sent session %s info!\n", m_sid.c_str());
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
		std::string dur;
		m_policy->LookupString(ATTR_SEC_SESSION_DURATION, dur);

		std::string return_addr;
		m_policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, return_addr);

		// we add 20 seconds for "slop".  the idea is that if the client were
		// to start a session just as it was expiring, the server will allow a
		// window of 20 seconds to receive the command before throwing out the
		// cached session.
		int slop = param_integer("SEC_SESSION_DURATION_SLOP", 20);
		int durint = atoi(dur.c_str()) + slop;
		time_t now = time(0);
		time_t expiration_time = now + durint;

		// extract the session lease time (max unused time)
		int session_lease = 0;
		m_policy->LookupInteger(ATTR_SEC_SESSION_LEASE, session_lease);
		if( session_lease ) {
				// Add some slop on the server side to avoid
				// expiration right before the client tries
				// to renew the lease.
			session_lease += slop;
		}


		// add the key(s) to the cache

		// if this is AES, we'll also create a derived fallback key
		// (for UDP), if fallback is allowed.
		//
		// BLOWFISH is default, 3DES would be preferred in FIPS mode.
		std::string fallback_method_str = "BLOWFISH";
		Protocol fallback_method = CONDOR_BLOWFISH;
		if (param_boolean("FIPS", false)) {
			fallback_method_str = "3DES";
			fallback_method = CONDOR_3DES;
		}
		dprintf(D_SECURITY|D_VERBOSE, "SESSION: fallback crypto method would be %s.\n", fallback_method_str.c_str());

		std::vector<KeyInfo> keyvec;
		dprintf(D_SECURITY|D_VERBOSE, "SESSION: server checking key type: %i\n", (m_key ? m_key->getProtocol() : -1));
		if (m_key) {
			// put the normal key into the vector
			keyvec.emplace_back(*m_key);

			// now see if we want to (and are allowed) to add a fallback key in addition to AES
			if (m_key->getProtocol() == CONDOR_AESGCM) {
				std::string all_methods;
				if (m_policy->LookupString(ATTR_SEC_CRYPTO_METHODS_LIST, all_methods)) {
					dprintf(D_SECURITY|D_VERBOSE, "SESSION: found list: %s.\n", all_methods.c_str());
					std::vector<std::string> sl = split(all_methods);
					if (contains_anycase(sl, fallback_method_str)) {
						keyvec.emplace_back(m_key->getKeyData(), 24, fallback_method, 0);
						dprintf(D_SECURITY, "SESSION: server duplicated AES to %s key for UDP.\n",
							fallback_method_str.c_str());
					} else {
						dprintf(D_SECURITY, "SESSION: %s not allowed.  UDP will not work.\n",
							fallback_method_str.c_str());
					}
				} else {
					dprintf(D_ERROR, "SESSION: no crypto methods list\n");
				}
			}
		}

		// This is a session for incoming connections, so
		// do not pass in m_sock->peer_addr() as addr,
		// because then this key would get confused for an
		// outgoing session to a daemon with that IP and
		// port as its command socket.
		m_sec_man->session_cache->emplace(m_sid, KeyCacheEntry(m_sid, "", keyvec, *m_policy, expiration_time, session_lease));
		dprintf (D_SECURITY, "DC_AUTHENTICATE: added incoming session id %s to cache for %i seconds (lease is %ds, return address is %s).\n", m_sid.c_str(), durint, session_lease, return_addr.c_str());
		if (IsDebugVerbose(D_SECURITY)) {
			dPrintAd(D_SECURITY, *m_policy);
		}

		dur.clear();
		return_addr.clear();
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
			dprintf (D_ERROR, "SECMAN: Error sending DC_SEC_QUERY reply to %s!\n", m_sock->peer_description());
			dPrintAd (D_ERROR, q_response);
			m_result = FALSE;
			return CommandProtocolFinished;
		}

		dprintf (D_COMMAND, "SECMAN: Succesfully sent DC_SEC_QUERY reply to %s!\n", m_sock->peer_description());
		dPrintAd (D_COMMAND, q_response);

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


	if ( m_result == KEEP_STREAM || m_sock == NULL ) {
		delete this;
		return KEEP_STREAM;
	} else {
		delete this;
		return TRUE;
	}
}
