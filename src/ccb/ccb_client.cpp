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

#include <memory>
#include <random>
#include <map>
#include "condor_sinful.h"
#include "shared_port_endpoint.h"
#include "condor_config.h"
#include "condor_version.h"

static bool registered_reverse_connect_command = false;

// Minimum broker version assumed to support CCB streaming/proxy mode, used as
// the pre-send gate when a private client wants to reach another private peer.
// This is the first HTCondor release whose CCB broker implements streaming;
// against an older broker a private requester fails fast rather than sending a
// request the broker would mishandle.
static const int CCB_STREAMING_VERSION_MAJOR = 25;
static const int CCB_STREAMING_VERSION_MINOR = 12;
static const int CCB_STREAMING_VERSION_SUB   = 0;

// hash of CCBClients waiting for a reverse connect command
// indexed by connection id
static std::map<std::string,classy_counted_ptr<CCBClient>> waiting_for_reverse_connect;


CCBClient::CCBClient( char const *ccb_contact, ReliSock *target_sock ):
	m_ccb_contact(ccb_contact),
	m_ccb_contacts(split(ccb_contact," ")),
	m_target_sock(target_sock),
	m_target_peer_description(m_target_sock->peer_description()),
	m_ccb_sock(NULL),
	m_ccb_cb(),
	m_deadline_timer(-1),
	m_proxy_state(PROXY_NONE),
	m_proxy_sock(NULL),
	m_proxy_registered(false),
	m_streaming_phase(true)
{
	// balance load across the CCB servers by randomizing order
	std::random_device rd;
	std::default_random_engine rng(rd());
	std::shuffle(m_ccb_contacts.begin(), m_ccb_contacts.end(), rng);

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

void
CCBClient::SetOutboundTarget( char const *target, int ttl )
{
	m_outbound_mode = true;
	m_outbound_target = target ? target : "";
		// ttl < 0 means "originator default": this client is the endpoint starting the
		// outbound request, so seed the TTL from CCB_OUTBOUND_TTL.  A broker forwarding
		// a request instead passes the already-decremented value (>= 0) explicitly.
	m_outbound_ttl = (ttl < 0) ? param_integer("CCB_OUTBOUND_TTL", 8, 1) : ttl;
}

CCBClient::~CCBClient()
{
	if( m_ccb_sock ) {
		delete m_ccb_sock;
	}
	CleanupProxySock();
	if( m_deadline_timer != -1 ) {
		daemonCore->Cancel_Timer(m_deadline_timer);
		m_deadline_timer = -1;
	}
}


bool
CCBClient::ReverseConnect( CondorError *error, bool non_blocking )
{
	if( m_outbound_mode ) {
		if( non_blocking ) {
			if( !daemonCore ) {
				dprintf(D_ALWAYS,"Can't do non-blocking outbound CCB connect without DaemonCore!\n");
				return false;
			}
				// Same reverse-connecting-state protection + async lifetime
				// machinery as the streaming non-blocking path; the exchange is
				// driven by OutboundConnect_nonblocking and finishes (or fails) via
				// ReverseConnectCallback, notifying the caller's registered handler.
			m_target_sock->enter_reverse_connecting_state();
			m_ccb_contacts_nb = m_ccb_contacts;
			RegisterReverseConnectCallback();
			return OutboundConnect_nonblocking();
		}
			// Blocking outbound proxy connect adopts the socket into m_target_sock,
			// which must be in the reverse-connecting state first (as in the
			// streaming path) so it is not deleted out from under us.
		m_target_sock->enter_reverse_connecting_state();
		bool ok = OutboundConnect_blocking( error );
		if( !ok ) {
			m_target_sock->exit_reverse_connecting_state( NULL );
		}
		return ok;
	}

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

		m_ccb_contacts_nb = m_ccb_contacts;
		m_streaming_phase = true;  // try streaming first; fall back to direct later
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

std::string
CCBClient::streamingReturnAddr()
{
	// Streaming/proxy mode only applies when this client is itself behind a
	// CCB (our own command socket is CCB-routed) and streaming is enabled.
	if( !param_boolean("CCB_CLIENT_STREAMING", true) ) {
		return "";
	}
	if( !daemonCore || !daemonCore->publicNetworkIpAddr() ) {
		return "";
	}
	Sinful s( daemonCore->publicNetworkIpAddr() );
	if( s.getCCBContact() ) {
		return daemonCore->publicNetworkIpAddr();
	}
	return "";
}

ReliSock *
CCBClient::establishStreamingPipe( const std::string &ccb_contact,
		char const *my_ccb_return, unsigned /*depth*/, CondorError *error )
{
	// Split the (possibly nested/tunneled) contact "<entry>#id0#...#idN" into the
	// flat entry broker "<entry>" and the ordered route [id0, ..., idN].  We contact
	// only the entry broker and hand it the whole route in one CCB_REQUEST; the entry
	// broker and each inner broker recurse server-side over their registration
	// channels (each asks the next hop to reverse-connect and splices), so we do
	// exactly one CEDAR handshake with the entry broker and one end-to-end handshake
	// with the target -- never a per-hop handshake with an inner broker.
	std::string entry_addr, id_chain;
	if( !SplitCCBContact( ccb_contact.c_str(), entry_addr, id_chain, m_target_peer_description, error, false /*outermost*/ ) ) {
		return nullptr;
	}
	// Peel the id chain "id0#id1#...#idN": ccbid = id0 (the entry broker's target),
	// route = "id1 ... idN" (the remaining hops it forwards inward).
	std::string ccbid, route;
	size_t hash = id_chain.find('#');
	if( hash == std::string::npos ) {
		ccbid = id_chain;
	} else {
		ccbid = id_chain.substr(0, hash);
		route = id_chain.substr(hash + 1);
		for( char &c : route ) { if( c == '#' ) c = ' '; }
	}

	Daemon ccb(DT_COLLECTOR, entry_addr.c_str(), NULL);
	Sock *sock = ccb.startCommand( CCB_REQUEST, Stream::reli_sock, DEFAULT_CEDAR_TIMEOUT, error );
	if( !sock ) {
		return nullptr;
	}

	// Pre-send version gate: do not send a proxy request to a broker that
	// cannot honor it (an old broker would forward our CCB-routed address
	// to the target and the connection would silently fail).
	CondorVersionInfo const *ver = sock->get_peer_version();
	if( !ver || !ver->built_since_version(
			CCB_STREAMING_VERSION_MAJOR,
			CCB_STREAMING_VERSION_MINOR,
			CCB_STREAMING_VERSION_SUB) )
	{
		dprintf(D_ALWAYS,
				"CCBClient: broker %s does not support CCB streaming mode, "
				"which is required to reach %s (private-to-private).\n",
				entry_addr.c_str(), m_target_peer_description.c_str());
		delete sock;
		return nullptr;
	}

	ClassAd msg;
	msg.Assign(ATTR_CCBID,ccbid);
	if( !route.empty() ) {
		msg.Assign(ATTR_CCB_ROUTE, route);
	}
	msg.Assign(ATTR_CLAIM_ID,m_connect_id);
	msg.Assign(ATTR_NAME, myName());
	// Send our own CCB-routed return address so the broker engages proxy
	// mode, and flag the request explicitly.  The flag forces streaming even
	// when we (the requester) are directly reachable, which is required to
	// reach a tunneled target: the broker cannot dial us back.
	msg.Assign(ATTR_MY_ADDRESS, my_ccb_return ? my_ccb_return : "");
	msg.Assign("CCBStreamingRequired", true);

	dprintf(D_NETWORK|D_FULLDEBUG,
			"CCBClient: requesting streaming (proxied) connection to %s "
			"via CCB server %s#%s (route '%s').\n",
			m_target_peer_description.c_str(),
			entry_addr.c_str(), ccbid.c_str(), route.c_str());

	sock->encode();
	if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
		delete sock;
		return nullptr;
	}

	// Read the broker's reply ({Result, ProxyMode}).
	sock->decode();
	ClassAd reply;
	if( !getClassAd( sock, reply ) || !sock->end_of_message() ) {
		delete sock;
		return nullptr;
	}
	bool result = false;
	reply.LookupBool(ATTR_RESULT,result);
	if( !result ) {
		std::string remote_errmsg;
		reply.LookupString(ATTR_ERROR_STRING,remote_errmsg);
		dprintf(D_ALWAYS,
				"CCBClient: broker %s refused streaming request to %s: %s\n",
				entry_addr.c_str(), m_target_peer_description.c_str(),
				remote_errmsg.c_str());
		delete sock;
		return nullptr;
	}

	// The broker now splices the target's reverse-connect hello onto this
	// socket. Read and validate it exactly as AcceptReversedConnection does.
	int cmd = 0;
	ClassAd hello;
	if( !sock->get(cmd) || !getClassAd( sock, hello ) || !sock->end_of_message() ) {
		delete sock;
		return nullptr;
	}
	std::string connect_id;
	hello.LookupString(ATTR_CLAIM_ID,connect_id);
	if( cmd != CCB_REVERSE_CONNECT || connect_id != m_connect_id ) {
		dprintf(D_ALWAYS,
				"CCBClient: invalid proxied hello from broker %s "
				"(intended target is %s)\n",
				entry_addr.c_str(), m_target_peer_description.c_str());
		delete sock;
		return nullptr;
	}

	// The socket is now a transparent pipe to the target (through the whole tunnel).
	// Reset the header MD so the caller's end-to-end CEDAR session runs over it
	// cleanly.
	static_cast<ReliSock*>(sock)->resetHeaderMD();

	dprintf(D_FULLDEBUG|D_NETWORK,
			"CCBClient: established streaming (proxied) pipe to %s "
			"via CCB server %s.\n",
			m_target_peer_description.c_str(), entry_addr.c_str());
	return static_cast<ReliSock*>(sock);
}

bool
CCBClient::ProxyConnect_blocking( CondorError *error, char const *my_ccb_return )
{
	// Precondition: m_target_sock is in the reverse-connecting state so that on
	// success we can adopt the proxied broker socket into it.
	for( const auto& ccb_contact: m_ccb_contacts ) {
		ReliSock *sock = establishStreamingPipe( ccb_contact, my_ccb_return, 0, error );
		if( !sock ) {
			continue;
		}

		// Success: adopt the proxied socket as the connection to the target.
		// From here on the socket is a transparent pipe to the target and the
		// normal end-to-end CEDAR handshake proceeds over it.
		//
		// exit_reverse_connecting_state() clears the socket's m_ccb_client
		// pointer, which in the blocking path is the only reference keeping this
		// CCBClient alive -- without the reference below we would be deleted out
		// from under ourselves and crash dereferencing m_target_sock.
		classy_counted_ptr<CCBClient> self(this);
		m_target_sock->exit_reverse_connecting_state(sock);
		static_cast<ReliSock*>(m_target_sock)->resetHeaderMD();
		delete sock; // exit_reverse_connecting_state has taken over the fd
		return true;
	}
	return false;
}

bool
CCBClient::OutboundConnect_blocking( CondorError *error )
{
	// Precondition: m_target_sock is in the reverse-connecting state so that on
	// success we can adopt the broker socket into it.
	for( const auto& ccb_address: m_ccb_contacts ) {
			// Pre-build and connect the broker socket ourselves with the
			// outbound-CCB bypass set, so this control connection is NOT itself
			// routed through the outbound CCB (which would recurse infinitely).
			// startCommand() would otherwise build the socket internally, leaving
			// us no place to set the flag before it connects.
		ReliSock *sock = new ReliSock;
		sock->set_bypass_outbound_ccb( true );
		sock->timeout( DEFAULT_CEDAR_TIMEOUT );
		if( !sock->connect( ccb_address.c_str(), 0, false /*blocking*/ ) ) {
			dprintf(D_ALWAYS,"CCBClient: failed to connect to outbound CCB %s.\n",
					ccb_address.c_str());
			delete sock;
			continue;
		}

		Daemon ccb(DT_COLLECTOR,ccb_address.c_str(),NULL);
		if( !ccb.startCommand( CCB_PROXY_CONNECT, sock, DEFAULT_CEDAR_TIMEOUT, error ) ) {
			delete sock;
			continue;
		}

			// Pre-send version gate: an old broker has no outbound-proxy command
			// handler; fail fast rather than hang.
		CondorVersionInfo const *ver = sock->get_peer_version();
		if( !ver || !ver->built_since_version(
				CCB_STREAMING_VERSION_MAJOR,
				CCB_STREAMING_VERSION_MINOR,
				CCB_STREAMING_VERSION_SUB) )
		{
			dprintf(D_ALWAYS,
					"CCBClient: outbound CCB broker %s is too old to proxy the "
					"connection to %s.\n",
					ccb_address.c_str(), m_target_peer_description.c_str());
			delete sock;
			continue;
		}

		ClassAd msg;
		msg.Assign(ATTR_MY_ADDRESS, m_outbound_target);   // the address to dial
		msg.Assign(ATTR_CLAIM_ID, m_connect_id);
		msg.Assign(ATTR_CCB_TTL, m_outbound_ttl);
		msg.Assign(ATTR_NAME, myName());

		dprintf(D_NETWORK|D_FULLDEBUG,
				"CCBClient: asking outbound CCB %s to dial %s.\n",
				ccb_address.c_str(), m_outbound_target.c_str());

		sock->encode();
		if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
			delete sock;
			continue;
		}

			// Read the broker's reply ({Result[, ErrorString]}).
		sock->decode();
		ClassAd reply;
		if( !getClassAd( sock, reply ) || !sock->end_of_message() ) {
			delete sock;
			continue;
		}
		bool result = false;
		reply.LookupBool(ATTR_RESULT,result);
		if( !result ) {
			std::string remote_errmsg;
			reply.LookupString(ATTR_ERROR_STRING,remote_errmsg);
			dprintf(D_ALWAYS,
					"CCBClient: outbound CCB %s refused to dial %s: %s\n",
					ccb_address.c_str(), m_outbound_target.c_str(),
					remote_errmsg.c_str());
			delete sock;
			continue;
		}

			// Success: the socket is now a transparent pipe to the target.  Unlike
			// the streaming path there is no reverse-connect hello to read -- we are
			// the CEDAR connector -- so adopt the socket directly and let the normal
			// end-to-end handshake proceed over it.
			//
			// exit_reverse_connecting_state() clears m_target_sock's m_ccb_client
			// pointer, the only reference keeping this CCBClient alive in the
			// blocking path; hold a reference across the call so we are not deleted
			// out from under ourselves.
		classy_counted_ptr<CCBClient> self(this);
		sock->resetHeaderMD();
		m_target_sock->exit_reverse_connecting_state(sock);
		static_cast<ReliSock*>(m_target_sock)->resetHeaderMD();
		delete sock; // exit_reverse_connecting_state has taken over the fd

		dprintf(D_FULLDEBUG|D_NETWORK,
				"CCBClient: established outbound (proxied) connection to %s via %s.\n",
				m_outbound_target.c_str(), ccb_address.c_str());
		return true;
	}
	return false;
}

// CleanupProxySock tears down the broker socket used by the non-blocking proxy
// exchange, cancelling its DaemonCore registration if it was registered.
void
CCBClient::CleanupProxySock()
{
	if( m_proxy_sock ) {
		if( m_proxy_registered ) {
			daemonCore->Cancel_Socket( m_proxy_sock );
			m_proxy_registered = false;
		}
		delete m_proxy_sock;
		m_proxy_sock = NULL;
	}
	m_proxy_state = PROXY_NONE;
}

bool
CCBClient::ProxyConnect_nonblocking( char const *ccbid, char const *my_ccb_return )
{
	// Precondition: m_target_sock is in the reverse-connecting state (set by
	// ReverseConnect()), and m_cur_ccb_address is the broker chain to use.  The
	// object is kept alive for the duration of the async exchange by
	// waiting_for_reverse_connect (added in RegisterReverseConnectCallback at the
	// top of try_next_ccb), so no extra reference is needed here.
	//
	// Follow a (possibly nested) contact <entry>#id0#...#idN: connect the flat
	// entry broker once, then request each id in turn over the pipe each hop
	// splices.  m_cur_ccb_address is the broker chain (<entry>#id0#...#id(N-1))
	// and ccbid is the innermost id (idN); peel the chain into the ordered id
	// list [id0, ..., idN] and the flat entry address.
	m_proxy_ids.clear();
	m_proxy_ids.push_back( ccbid );
	std::string addr = m_cur_ccb_address;
	while( addr.find('#') != std::string::npos ) {
		std::string inner_addr, inner_id;
		if( !SplitCCBContact( addr.c_str(), inner_addr, inner_id, m_target_peer_description, NULL ) ) {
			return false;
		}
		m_proxy_ids.push_back( inner_id );
		addr = inner_addr;
	}
	std::reverse( m_proxy_ids.begin(), m_proxy_ids.end() );
	m_proxy_entry = addr;
	m_proxy_hop = 0;
	m_proxy_return = my_ccb_return;

	Daemon ccb( DT_COLLECTOR, m_proxy_entry.c_str(), NULL );
	Sock *sock = ccb.makeConnectedSocket( Stream::reli_sock, DEFAULT_CEDAR_TIMEOUT, 0, NULL, true /*nonblocking*/ );
	if( !sock ) {
		return false;
	}
	m_proxy_sock = sock;
	m_proxy_registered = false;
	m_proxy_state = PROXY_NONE;

	dprintf( D_NETWORK|D_FULLDEBUG,
			 "CCBClient: requesting streaming (proxied) connection to %s via CCB "
			 "server %s over %zu hop(s) (non-blocking).\n",
			 m_target_peer_description.c_str(), m_proxy_entry.c_str(),
			 m_proxy_ids.size() );

	startProxyHop();
	return true;
}

void
CCBClient::startProxyHop()
{
		// Send the (single) streaming request to the entry broker.  The entry broker
		// forwards the remaining route inward and each broker recurses server-side,
		// so from the client there is only ever this one hop -- no per-hop re-keying.
	ReliSock *sock = static_cast<ReliSock*>( m_proxy_sock );
	Daemon ccb( DT_COLLECTOR, m_proxy_entry.c_str(), NULL );
	ccb.startCommand_nonblocking(
		CCB_REQUEST, sock, DEFAULT_CEDAR_TIMEOUT, NULL,
		[this]( bool success, Sock *s, CondorError * /*errstack*/,
				const std::string & /*trust_domain*/, bool /*try_token*/ ) {
			ProxyHopCommandCallback( success, s );
		} );
}

void
CCBClient::ProxyHopCommandCallback( bool success, Sock *sock )
{
	// If the reverse-connect was cancelled (e.g. deadline expired) while we were
	// connecting, m_target_sock is gone; just clean up the broker socket.
	if( !m_target_sock ) {
		CleanupProxySock();
		return;
	}

	if( !success ) {
		ProxyConnectFailed();
		return;
	}

	// Pre-send version gate: do not send a proxy request to a broker that cannot
	// honor it.
	CondorVersionInfo const *ver = sock->get_peer_version();
	if( !ver || !ver->built_since_version(
			CCB_STREAMING_VERSION_MAJOR,
			CCB_STREAMING_VERSION_MINOR,
			CCB_STREAMING_VERSION_SUB) )
	{
		dprintf( D_ALWAYS,
				 "CCBClient: a broker on the path to %s does not support CCB "
				 "streaming mode, which is required (private-to-private).\n",
				 m_target_peer_description.c_str() );
		ProxyConnectFailed();
		return;
	}

	// Send one proxy request to the entry broker: its target (the first id) plus the
	// rest of the route it forwards inward.  The brokers recurse server-side over
	// their registration channels, so this single request reaches the whole tunnel.
	std::string ccbid = m_proxy_ids.empty() ? std::string() : m_proxy_ids[0];
	std::string route;
	for( size_t k = 1; k < m_proxy_ids.size(); k++ ) {
		if( !route.empty() ) { route += " "; }
		route += m_proxy_ids[k];
	}
	ClassAd msg;
	msg.Assign( ATTR_CCBID, ccbid );
	if( !route.empty() ) {
		msg.Assign( ATTR_CCB_ROUTE, route );
	}
	msg.Assign( ATTR_CLAIM_ID, m_connect_id );
	msg.Assign( ATTR_NAME, myName() );
	msg.Assign( ATTR_MY_ADDRESS, m_proxy_return );
	msg.Assign( "CCBStreamingRequired", true );

	sock->encode();
	if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
		ProxyConnectFailed();
		return;
	}

	// Wait for the broker's reply, and then the proxied hello, asynchronously.
	m_proxy_state = PROXY_WAIT_REPLY;
	int rc = daemonCore->Register_Socket(
		sock, sock->peer_description(),
		[this]( Stream *s ) { return ProxyMsgHandler( s ); },
		"CCBClient::ProxyMsgHandler" );
	if( rc < 0 ) {
		ProxyConnectFailed();
		return;
	}
	m_proxy_registered = true;
}

int
CCBClient::ProxyMsgHandler( Stream *stream )
{
	Sock *sock = (Sock *)stream;
	sock->timeout( DEFAULT_CEDAR_TIMEOUT );

	// If the reverse-connect was cancelled while we were waiting, clean up.
	if( !m_target_sock ) {
		CleanupProxySock();
		return KEEP_STREAM;
	}

	if( m_proxy_state == PROXY_WAIT_REPLY ) {
		ClassAd reply;
		sock->decode();
		if( !getClassAd( sock, reply ) || !sock->end_of_message() ) {
			ProxyConnectFailed();
			return KEEP_STREAM;
		}
		bool result = false;
		reply.LookupBool( ATTR_RESULT, result );
		if( !result ) {
			std::string remote_errmsg;
			reply.LookupString( ATTR_ERROR_STRING, remote_errmsg );
			dprintf( D_ALWAYS,
					 "CCBClient: broker %s refused streaming request to %s: %s\n",
					 m_cur_ccb_address.c_str(), m_target_peer_description.c_str(),
					 remote_errmsg.c_str() );
			ProxyConnectFailed();
			return KEEP_STREAM;
		}
		// Success so far; the broker will splice in the target's reverse-connect
		// hello when the target connects.  Keep the socket registered and wait.
		m_proxy_state = PROXY_WAIT_HELLO;
		return KEEP_STREAM;
	}

	if( m_proxy_state == PROXY_WAIT_HELLO ) {
		int cmd = 0;
		ClassAd hello;
		sock->decode();
		if( !sock->get(cmd) || !getClassAd( sock, hello ) || !sock->end_of_message() ) {
			ProxyConnectFailed();
			return KEEP_STREAM;
		}
		std::string connect_id;
		hello.LookupString( ATTR_CLAIM_ID, connect_id );
		if( cmd != CCB_REVERSE_CONNECT || connect_id != m_connect_id ) {
			dprintf( D_ALWAYS,
					 "CCBClient: invalid proxied hello from broker %s "
					 "(intended target is %s)\n",
					 m_cur_ccb_address.c_str(), m_target_peer_description.c_str() );
			ProxyConnectFailed();
			return KEEP_STREAM;
		}

		// The target's reverse-connect hello has arrived spliced through the whole
		// tunnel (the brokers recursed server-side); adopt the socket as the
		// connection to the target and let the end-to-end CEDAR handshake proceed.
		dprintf( D_FULLDEBUG|D_NETWORK,
				 "CCBClient: established streaming (proxied) connection to %s "
				 "via CCB server %s.\n",
				 m_target_peer_description.c_str(), m_cur_ccb_address.c_str() );
		adoptProxiedSocket( sock );
		return KEEP_STREAM; // socket already cancelled/deleted above
	}

	return KEEP_STREAM;
}

void
CCBClient::ProxyConnectFailed()
{
	CleanupProxySock();
	// Move on to the next broker (or give up if none remain).
	try_next_ccb();
}

// ---- non-blocking outbound-proxy connect (CCB_PROXY_CONNECT) ----------------
// Mirrors ProxyConnect_nonblocking / ProxyConnectCallback / ProxyMsgHandler, but
// asks the (single) outbound CCB to dial m_outbound_target and, on {Result:true},
// adopts the socket directly -- there is no reverse-connect hello, because we are
// the CEDAR connector.  Object lifetime + caller notification reuse the
// reverse-connecting-state / waiting_for_reverse_connect machinery.

bool
CCBClient::OutboundConnect_nonblocking()
{
	// Precondition: m_target_sock is in the reverse-connecting state and
	// RegisterReverseConnectCallback() has been called (keeping us alive).
	if( m_ccb_contacts_nb.empty() ) {
			// No outbound CCB left to try: fail the connection.
		ReverseConnectCallback( NULL );
		return false;
	}
	m_cur_ccb_address = m_ccb_contacts_nb.back();
	m_ccb_contacts_nb.pop_back();

		// Pre-build the broker socket with the outbound-CCB bypass set so this
		// control connection is not itself routed through the outbound CCB (which
		// would recurse infinitely); startCommand_nonblocking() would otherwise
		// build the socket internally with no place to set the flag.
	ReliSock *sock = new ReliSock;
	sock->set_bypass_outbound_ccb( true );
	sock->timeout( DEFAULT_CEDAR_TIMEOUT );
	int rc = sock->connect( m_cur_ccb_address.c_str(), 0, true /*nonblocking*/ );
	if( rc == 0 ) {
			// Immediate failure to even start the connect; try the next contact.
		dprintf(D_ALWAYS,"CCBClient: failed to start connect to outbound CCB %s.\n",
				m_cur_ccb_address.c_str());
		delete sock;
		return OutboundConnect_nonblocking();
	}

	m_proxy_sock = sock;
	m_proxy_registered = false;
	m_proxy_state = PROXY_WAIT_REPLY;

	dprintf( D_NETWORK|D_FULLDEBUG,
			 "CCBClient: asking outbound CCB %s to dial %s (non-blocking).\n",
			 m_cur_ccb_address.c_str(), m_outbound_target.c_str() );

	Daemon ccb( DT_COLLECTOR, m_cur_ccb_address.c_str(), NULL );
	ccb.startCommand_nonblocking(
		CCB_PROXY_CONNECT, sock, DEFAULT_CEDAR_TIMEOUT, NULL,
		[this]( bool success, Sock *s, CondorError *errstack,
				const std::string & /*trust_domain*/, bool /*try_token*/ ) {
			OutboundConnectCallback( success, s, errstack );
		} );
	return true;
}

void
CCBClient::OutboundConnectCallback( bool success, Sock *sock, CondorError * /*errstack*/ )
{
		// If the reverse-connect was cancelled (e.g. deadline) while connecting,
		// m_target_sock is gone; just clean up the broker socket.
	if( !m_target_sock ) {
		CleanupProxySock();
		return;
	}
	if( !success ) {
		OutboundConnectFailed();
		return;
	}

		// Pre-send version gate: an old broker has no outbound-proxy handler.
	CondorVersionInfo const *ver = sock->get_peer_version();
	if( !ver || !ver->built_since_version(
			CCB_STREAMING_VERSION_MAJOR,
			CCB_STREAMING_VERSION_MINOR,
			CCB_STREAMING_VERSION_SUB) )
	{
		dprintf( D_ALWAYS,
				 "CCBClient: outbound CCB broker %s is too old to proxy the "
				 "connection to %s.\n",
				 m_cur_ccb_address.c_str(), m_target_peer_description.c_str() );
		OutboundConnectFailed();
		return;
	}

		// Send the proxy-connect request (the target to dial + our connect id).
	ClassAd msg;
	msg.Assign( ATTR_MY_ADDRESS, m_outbound_target );
	msg.Assign( ATTR_CLAIM_ID, m_connect_id );
	msg.Assign( ATTR_CCB_TTL, m_outbound_ttl );
	msg.Assign( ATTR_NAME, myName() );

	sock->encode();
	if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
		OutboundConnectFailed();
		return;
	}

		// Wait for the broker's reply asynchronously.
	m_proxy_state = PROXY_WAIT_REPLY;
	int rc = daemonCore->Register_Socket(
		sock, sock->peer_description(),
		[this]( Stream *s ) { return OutboundMsgHandler( s ); },
		"CCBClient::OutboundMsgHandler" );
	if( rc < 0 ) {
		OutboundConnectFailed();
		return;
	}
	m_proxy_registered = true;
}

int
CCBClient::OutboundMsgHandler( Stream *stream )
{
	Sock *sock = (Sock *)stream;
	sock->timeout( DEFAULT_CEDAR_TIMEOUT );

	if( !m_target_sock ) {
		CleanupProxySock();
		return KEEP_STREAM;
	}

	ClassAd reply;
	sock->decode();
	if( !getClassAd( sock, reply ) || !sock->end_of_message() ) {
		OutboundConnectFailed();
		return KEEP_STREAM;
	}
	bool result = false;
	reply.LookupBool( ATTR_RESULT, result );
	if( !result ) {
		std::string remote_errmsg;
		reply.LookupString( ATTR_ERROR_STRING, remote_errmsg );
		dprintf( D_ALWAYS,
				 "CCBClient: outbound CCB %s refused to dial %s: %s\n",
				 m_cur_ccb_address.c_str(), m_outbound_target.c_str(),
				 remote_errmsg.c_str() );
		OutboundConnectFailed();
		return KEEP_STREAM;
	}

		// Success: the socket is a transparent pipe to the target.  There is no
		// reverse-connect hello -- adopt the socket and notify the waiting caller,
		// mirroring ProxyMsgHandler's success path.
	dprintf( D_FULLDEBUG|D_NETWORK,
			 "CCBClient: established outbound (proxied) connection to %s via %s "
			 "(non-blocking).\n",
			 m_outbound_target.c_str(), m_cur_ccb_address.c_str() );
	adoptProxiedSocket( sock );
	return KEEP_STREAM; // socket already cancelled/deleted above
}

void
CCBClient::adoptProxiedSocket( Sock *sock )
{
		// Detach the proxied socket from its broker handler, make both it and the
		// target socket raw, and hand the fd to the target socket via
		// exit_reverse_connecting_state(); then fire the caller's registered handler
		// so the connection continues as a finished non-blocking connect.  Mirrors
		// ReverseConnectCallback()'s adoption tail.  Shared by ProxyMsgHandler (final
		// streaming hop) and OutboundMsgHandler (outbound proxy).
	daemonCore->Cancel_Socket( sock );
	m_proxy_registered = false;
	m_proxy_sock = NULL;
	m_proxy_state = PROXY_NONE;

	static_cast<ReliSock*>(sock)->resetHeaderMD();
	m_target_sock->exit_reverse_connecting_state( static_cast<ReliSock*>(sock) );
	static_cast<ReliSock*>(m_target_sock)->resetHeaderMD();
	delete sock; // exit_reverse_connecting_state took over the fd

	if( m_ccb_cb ) {
		m_ccb_cb->cancelCallback();
		m_ccb_cb->cancelMessage( true );
		m_ccb_cb = NULL;
		decRefCount();
	}
	daemonCore->CallSocketHandler( m_target_sock, false );
	m_target_sock = NULL;
		// May drop the last reference to this CCBClient (deleting it); do not touch
		// *this afterward.
	UnregisterReverseConnectCallback();
}

void
CCBClient::OutboundConnectFailed()
{
	CleanupProxySock();
	// Try the next outbound CCB, or fail the connection if none remain.
	OutboundConnect_nonblocking();
}

bool
CCBClient::ReverseConnect_blocking( CondorError *error )
{
	// Hold a reference to ourselves for the duration of this call.  Below,
	// exit_reverse_connecting_state() clears the socket's reference to this
	// CCBClient, which would otherwise delete us mid-function -- fatal once we
	// fall through to the plain reverse-connect loop and touch our members.
	classy_counted_ptr<CCBClient> self(this);

	// If we are ourselves behind a CCB, the target cannot reverse-connect to us
	// directly; ask the broker to proxy instead (streaming mode).
	std::string my_ccb_return = streamingReturnAddr();
	if( !my_ccb_return.empty() ) {
		m_target_sock->enter_reverse_connecting_state();
		if( ProxyConnect_blocking( error, my_ccb_return.c_str() ) ) {
			return true;
		}
		m_target_sock->exit_reverse_connecting_state(NULL);
		// Streaming failed (e.g. the broker is too old or has streaming
		// disabled).  As a last resort, fall through to the plain reverse
		// connection below: we open a direct listen socket and offer it (our CCB
		// contact stripped), which succeeds if the two endpoints turn out to be
		// mutually reachable.  This mirrors the non-blocking try_next_ccb path.
		dprintf(D_ALWAYS,
				"CCBClient: streaming (proxy) connection to %s failed; falling "
				"back to a direct reverse connection.\n",
				m_target_peer_description.c_str());
	}

	std::shared_ptr<ReliSock> listen_sock;
	std::shared_ptr<SharedPortEndpoint> shared_listener;
	char const *listener_addr = NULL;

	for (const auto& ccb_contact: m_ccb_contacts) {
		bool success = false;
		std::string ccb_address, ccbid;
		if( !SplitCCBContact( ccb_contact.c_str(), ccb_address, ccbid, m_target_peer_description, error ) ) {
			continue;
		}

		// If this (or any other) CCB (reverse) connection can't use the
		// shared port [to listen for the reversed connection] because
		// it can't write to the daemon socket directory, that implies
		// that the shared port is turned on [true by default] and that
		// the connection is not being made by a daemon (because the
		// daemons use the "anonymous" unix domain socket, instead).  In
		// that case, rather than try to open a listen socket on a
		// random port that's probably firewalled and hang, assume that
		// there's a firewall and just give up.
		//
		// Defaults to false for backwards compatibility worries.
		bool toolsAssumeFirewalls = param_boolean("TOOLS_ASSUME_FIREWALLS", false);

		//
		// Generate a listen port for the appropriate protocol.  It will be
		// distressing that we're passed CCB contact strings rather than
		// sinfuls, when it's time to handle multiple addresses...
		//
		// FIXME: Assumes that shared port knows what it's doing.
		//
		std::string reason;
		if( SharedPortEndpoint::UseSharedPort(& reason) ) {
			shared_listener = std::make_shared<SharedPortEndpoint>();
			shared_listener->InitAndReconfig();
			std::string errmsg;
			if( !shared_listener->CreateListener() ) {
				formatstr( errmsg, "Failed to create shared port endpoint for reversed connection from %s.",
					m_target_peer_description.c_str() );
			}
			else if( !(listener_addr = shared_listener->GetMyRemoteAddress()) ) {
				formatstr( errmsg, "Failed to get remote address for shared port endpoint for reversed connection from %s.",
					m_target_peer_description.c_str() );
			}
			if( !listener_addr ) {
				if( error ) {
					error->push( "CCBClient", CEDAR_ERR_CONNECT_FAILED, errmsg.c_str() );
				}
				dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.c_str());
				return false;
			}
		} else if( toolsAssumeFirewalls ) {
			// We are not using the shared port (reason: <reason> -- either it is
			// disabled or its socket directory is not writable) and
			// TOOLS_ASSUME_FIREWALLS says a random listen port would likely be
			// firewalled.  So we must not open a plain listen socket and offer its
			// (unreachable) address: we cannot accept a direct reverse connection.
			// An unprivileged client behind a firewall or NAT generally cannot, so
			// rather than hang waiting for a reverse connection that never arrives,
			// fall back to streaming if it is enabled: ask the broker to proxy the
			// connection on our existing request socket, so the target never has
			// to reach us.  Our advertised address need not be reachable -- in
			// proxy mode the broker splices the target onto the request socket and
			// never dials it.  ProxyConnect_blocking version-gates each broker, so
			// against an older broker that cannot stream we fall through and give
			// up as before.
			if( param_boolean("CCB_CLIENT_STREAMING", true) ) {
				char const *my_addr =
					daemonCore ? daemonCore->publicNetworkIpAddr() : NULL;
				dprintf( D_NETWORK|D_FULLDEBUG,
						 "CCBClient: cannot accept a direct reverse connection "
						 "(assuming firewall); requesting a streaming (proxied) "
						 "connection to %s instead.\n",
						 m_target_peer_description.c_str() );
				m_target_sock->enter_reverse_connecting_state();
				if( ProxyConnect_blocking( error, my_addr ? my_addr : "" ) ) {
					return true;
				}
				m_target_sock->exit_reverse_connecting_state(NULL);
			}

			std::string errmsg = reason;

			if( error ) {
				error->push( "CCBClient", CEDAR_ERR_NO_SHARED_PORT, errmsg.c_str() );
			}

			dprintf( D_ALWAYS, "%s.\n", errmsg.c_str() );

			return false;
		} else {
			condor_sockaddr ccbSA;
			std::string faked_sinful = "<" + ccb_address + ">";
			if( ! ccbSA.from_sinful( faked_sinful ) ) {
				dprintf( D_FULLDEBUG, "Failed to generate condor_sockaddr from faked sinful '%s', ignoring this broker.\n", faked_sinful.c_str() );
				continue;
			}

			listen_sock = std::make_shared<ReliSock>( );

			// Should bind() should accept a condor_sockaddr directly?
			if (!listen_sock->bind( ccbSA.get_protocol(), false, 0, false )) {
				dprintf(D_ALWAYS,"CCBClient: can't bind listen socket\n");
				return false;
			}

			if( ! listen_sock->listen() ) {
				std::string errmsg;
				formatstr( errmsg, "Failed to listen for reversed connection from %s.",
					m_target_peer_description.c_str() );
				if( error ) {
					error->push("CCBClient", CEDAR_ERR_CONNECT_FAILED,errmsg.c_str());
				}
				dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.c_str());

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
		time_t timeout = m_target_sock->get_timeout_raw();
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
				time_t elapsed = time(nullptr) - start_time;
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
				std::string errmsg;
				formatstr(errmsg,
					"Timed out waiting for response after requesting reversed "
					"connection from %s ccbid %s via CCB server %s.",
					m_target_peer_description.c_str(),
					ccbid.c_str(),
					ccb_address.c_str());

				if( error ) {
					error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.c_str());
				}
				else {
					dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.c_str());
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

bool CCBClient::SplitCCBContact( char const *ccb_contact, std::string &ccb_address, std::string &ccbid, const std::string & peer, CondorError *error, bool peel_innermost )
{
	// Format is "<address>#ccbid", where for a tunneled (nested) contact the
	// address is itself a contact: "<entry-broker>#id1#...#idN".  The recursive
	// connect path peels one hop at a time, so it splits on the LAST '#' (the
	// outermost id) and re-resolves the remaining contact.  The v1 (addrs=) Sinful
	// serialization instead wants the flat entry address with the whole nested id
	// as a single CCBID, so it splits on the FIRST '#'.  A flat single-hop contact
	// has exactly one '#', so the two are identical for the common case.
	char const *ptr = peel_innermost ? strrchr(ccb_contact,'#') : strchr(ccb_contact,'#');
	if( !ptr ) {
		std::string errmsg;
		formatstr(errmsg, "Bad CCB contact '%s' when connecting to %s.",
					   ccb_contact, peer.c_str());

		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.c_str());
		}
		else {
			dprintf(D_ALWAYS,"%s\n",errmsg.c_str());
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

	static_cast<ReliSock*>(m_target_sock)->resetHeaderMD();
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
	std::string errmsg;

	m_ccb_sock->decode();
	if( !getClassAd(m_ccb_sock, msg) || !m_ccb_sock->end_of_message() ) {
		formatstr(errmsg, "Failed to read response from CCB server "
					   "%s when requesting reversed connection to %s",
					   m_ccb_sock->peer_description(),
					   m_target_peer_description.c_str());
		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.c_str());
		}
		else {
			dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.c_str());
		}
		return false;
	}

	msg.LookupBool(ATTR_RESULT,result);
	if( !result ) {
		std::string remote_errmsg;
		msg.LookupString(ATTR_ERROR_STRING,remote_errmsg);

		formatstr( errmsg,
			"received failure message from CCB server %s in response to "
			"request for reversed connection to %s: %s",
			m_ccb_sock->peer_description(),
			m_target_peer_description.c_str(),
			remote_errmsg.c_str());

		if( error ) {
			error->push("CCBClient",CEDAR_ERR_CONNECT_FAILED,errmsg.c_str());
		}
		else {
			dprintf(D_ALWAYS,"CCBClient: %s\n",errmsg.c_str());
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

	if(m_ccb_contacts_nb.empty()) {
		// If we exhausted every broker while attempting streaming (proxy) mode,
		// fall back to a plain reverse connection as a last resort: retry the
		// brokers with our CCB contact stripped.
		char const *ra = daemonCore->publicNetworkIpAddr();
		if( m_streaming_phase && ra && Sinful(ra).getCCBContact()
			&& param_boolean("CCB_CLIENT_STREAMING", true) )
		{
			m_streaming_phase = false;
			m_ccb_contacts_nb = m_ccb_contacts;
			dprintf(D_ALWAYS,
					"CCBClient: streaming (proxy) connection to %s failed on all "
					"brokers; falling back to a direct reverse connection.\n",
					m_target_peer_description.c_str());
		}
		if(m_ccb_contacts_nb.empty()) {
			dprintf(D_ALWAYS,
					"CCBClient: no more CCB servers to try for requesting "
					"reversed connection to %s; giving up.\n",
					m_target_peer_description.c_str());
			ReverseConnectCallback(NULL);
			return false;
		}
	}

	std::string ccb_contact = m_ccb_contacts_nb.back();
	m_ccb_contacts_nb.pop_back();

	std::string ccbid;
	if( !SplitCCBContact( ccb_contact.c_str(), m_cur_ccb_address, ccbid, m_target_peer_description, NULL ) ) {
		return try_next_ccb();
	}

	char const *return_address = daemonCore->publicNetworkIpAddr();

	// For now, we require that this daemon has a command port.
	// If needed, we could add support for opening a listen socket here.
	ASSERT( return_address && *return_address );

	Sinful sinful_return(return_address);
	if( sinful_return.getCCBContact() ) {
		bool streaming_enabled = param_boolean("CCB_CLIENT_STREAMING", true);
		if( m_streaming_phase && streaming_enabled ) {
			// We are behind a CCB too (private-to-private): use streaming/proxy
			// mode.  m_target_sock is already in the reverse-connecting state
			// (set by ReverseConnect).  Drive the broker exchange asynchronously
			// so we do not block the DaemonCore main loop; the rest of the work
			// happens in ProxyConnectCallback / ProxyMsgHandler.
			if( ProxyConnect_nonblocking( ccbid.c_str(), return_address ) ) {
				return true;
			}
			// Failed to even initiate; try the next broker (or give up).
			return try_next_ccb();
		}

		if( streaming_enabled ) {
			// We have exhausted streaming on every broker and are now retrying
			// with a plain reverse connection: strip our CCB contact and offer a
			// direct return address, which succeeds if the endpoints are actually
			// mutually reachable.
			dprintf(D_NETWORK|D_FULLDEBUG,
					"CCBClient: streaming exhausted; attempting a direct reverse "
					"connection to %s with our CCB contact stripped.\n",
					m_target_peer_description.c_str());
		} else {
			// Streaming is disabled.  Our return address is via CCB, so this
			// looks like a private-to-private connection that plain CCB cannot
			// make; assume instead that the private network names are mislabeled
			// and try a direct reverse connection.
			dprintf(D_ALWAYS,
					"CCBClient: WARNING: trying to connect to %s via CCB, but "
					"this appears to be a connection from one private network "
					"to another, which is not supported without CCB streaming.  "
					"Either that, or you have not configured the private network "
					"name to be the same in these two networks when it really "
					"should be.  Assuming the latter.  (CCB_CLIENT_STREAMING "
					"enables private-to-private connections via a streaming-"
					"capable broker.)\n",
					m_target_peer_description.c_str());
		}

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
	auto ccb_cb = std::make_shared<DCMsgCallback>(
	    (DCMsgCallback::CppFunction)&CCBClient::CCBResultsCallback,
	    this );
	m_ccb_cb = ccb_cb;
	msg->setCallback( ccb_cb );

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
			CCBResultsCallback(m_ccb_cb.get());
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
	m_ccb_cb.reset();
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
			dprintf(D_ALWAYS,"CCBClient: "
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

	// Outbound-proxy mode never receives a CCB_REVERSE_CONNECT: the broker dials
	// the target and splices, and we adopt the spliced socket directly (no
	// reverse-connect hello).  So do not register the reverse-connect command
	// handler here -- besides being unnecessary, it collides with the handler a
	// CCB *server* registers for the same command, which matters when a CCB server
	// itself forwards an outbound-proxy request through a next hop (Stage C).
	if( !m_outbound_mode && !registered_reverse_connect_command ) {
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
		time_t timeout = deadline - time(nullptr) + 1;
		if( timeout < 0 ) {
			timeout = 0;
		}
		m_deadline_timer = daemonCore->Register_Timer (
			timeout,
			(TimerHandlercpp)&CCBClient::DeadlineExpired,
			"CCBClient::DeadlineExpired",
			this );
	}

	waiting_for_reverse_connect.emplace(m_connect_id, this);
}

void
CCBClient::DeadlineExpired(int /* timerID */)
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
	waiting_for_reverse_connect.erase(m_connect_id);
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
	auto it = waiting_for_reverse_connect.find(connect_id);
	if (it == waiting_for_reverse_connect.end()) {
		dprintf(D_ALWAYS,
				"CCBClient: failed to find requested connection id %s.\n",
				connect_id.c_str());
		return false;
	}
	it->second->ReverseConnectCallback((Sock *)stream);
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

	// If a non-blocking proxy exchange is still in flight (e.g. this is the
	// deadline/cancel path), tear down its broker socket first so its
	// registered handler cannot fire after we are gone.
	CleanupProxySock();

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
