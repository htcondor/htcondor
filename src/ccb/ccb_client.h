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


#ifndef _CONDOR_CCB_CLIENT_H
#define _CONDOR_CCB_CLIENT_H

/*
 CCBClient: the Condor Connection Broker Client
 This is instantiated by CEDAR when a client wants to connect to a daemon
 that is not directly accessible (e.g. because the daemon is in a private
 network).  The client requests the daemon's CCB server to inform the
 daemon of the request.  If all goes well, the daemon connects to the client
 and then CEDAR continues on its way as though it had created the connection
 the normal way from the client to the server.
 */

#include "classy_counted_ptr.h"
#include "reli_sock.h"
#include "dc_message.h"
#include "shared_port_endpoint.h"

class CCBClient: public Service, public ClassyCountedPtr {
 public:
	CCBClient( char const *ccb_contact, ReliSock *target_sock );
	~CCBClient();

	bool ReverseConnect( CondorError *error, bool non_blocking );
	void CancelReverseConnect();

	// Switch this client into outbound-proxy mode: instead of asking a broker to
	// have a registered target reverse-connect, ask the (single) broker in
	// m_ccb_contact to dial `target` directly and splice the connection back.
	// ttl is a decrementing time-to-live (like TCP): the originator sets it, each
	// forwarding broker decrements it and refuses at 0, so a runaway chain (e.g. two
	// brokers whose next hops point at each other) is bounded by the endpoint, not by
	// any intermediate broker's own policy.  ttl < 0 means "use the originator
	// default" (CCB_OUTBOUND_TTL); a broker forwarding a request passes the
	// already-decremented value explicitly.
	void SetOutboundTarget( char const *target, int ttl = -1 );

 private:
	// Outbound-proxy (tunneling) mode; see SetOutboundTarget().
	bool m_outbound_mode = false;
	std::string m_outbound_target;
	int m_outbound_ttl = 0;
	std::string m_ccb_contact;
	std::string m_cur_ccb_address;
	std::vector<std::string> m_ccb_contacts;
	std::vector<std::string> m_ccb_contacts_nb; // contacts remaining to try in non-blocking mode
	ReliSock *m_target_sock; // socket to receive the reversed connection
	std::string m_target_peer_description; // who we are trying to connect to
	Sock *m_ccb_sock;        // socket to the CCB server
	std::string m_connect_id;
	std::shared_ptr<DCMsgCallback> m_ccb_cb; // callback object for async CCB request
	int m_deadline_timer;

	// Non-blocking streaming/proxy connect state (see ProxyConnect_nonblocking).
	enum ProxyConnectState { PROXY_NONE, PROXY_WAIT_REPLY, PROXY_WAIT_HELLO };
	ProxyConnectState m_proxy_state;
	Sock *m_proxy_sock;        // broker socket during the proxy exchange
	bool m_proxy_registered;   // m_proxy_sock is registered with DaemonCore
	// The ccbids to request, outermost first, to follow a (possibly nested) CCB
	// contact <entry>#id0#...#idN: we connect m_proxy_entry once and then request
	// each id in turn over the spliced pipe.  m_proxy_hop is the id in flight.
	std::vector<std::string> m_proxy_ids;
	size_t m_proxy_hop = 0;
	std::string m_proxy_entry; // flat entry-broker address we connected to
	std::string m_proxy_return;// our CCB-routed return address
	// True while try_next_ccb is still attempting streaming (proxy) mode; once
	// every broker has been tried it flips to false and the brokers are retried
	// with a plain reverse connection (CCB contact stripped) as a last resort.
	bool m_streaming_phase;

	bool ReverseConnect_blocking( CondorError *error );
	// Outbound-proxy connect (blocking): dial the broker, send CCB_PROXY_CONNECT
	// for m_outbound_target, and on {Result:true} adopt the spliced socket.
	bool OutboundConnect_blocking( CondorError *error );
	// Non-blocking outbound-proxy connect: try the next outbound CCB in
	// m_ccb_contacts_nb, or fail the connection if none remain.  The async
	// exchange is driven by the callbacks below.
	bool OutboundConnect_nonblocking();
	void OutboundConnectCallback( bool success, Sock *sock, CondorError *errstack );
	int OutboundMsgHandler( Stream *stream );
	void OutboundConnectFailed();
	// Adopt a spliced/proxied socket as the connection to the target and notify the
	// waiting caller (mirrors ReverseConnectCallback()).  Shared final-hop tail of
	// the streaming (ProxyMsgHandler) and outbound (OutboundMsgHandler) paths.  May
	// drop the last reference to this CCBClient, so callers must not touch *this
	// afterward.
	void adoptProxiedSocket( Sock *sock );
	// Split a (possibly nested) CCB contact "<address>#id[#id...]" into an address
	// and an id.  peel_innermost (the default, for the recursive connect path)
	// splits on the LAST '#', peeling one hop: address="<entry>#id0..#id(N-1)",
	// id="idN".  peel_innermost=false splits on the FIRST '#', keeping the whole
	// remaining chain as the id: address="<entry>", id="id0#..#idN" -- what the v1
	// (addrs=) Sinful serialization needs, where a source route carries the full
	// nested id as a single CCBID.
	static bool SplitCCBContact( char const *ccb_contact, std::string &ccb_address, std::string &ccbid, const std::string & peer, CondorError *error, bool peel_innermost = true );

	// Streaming/proxy mode (CCB_STREAMING): used when this client is itself
	// behind a CCB (i.e. our own return address is CCB-routed), so the target
	// cannot reverse-connect to us directly.  Instead we ask the broker to
	// proxy: the target reverse-connects to the broker, the broker splices the
	// two sockets, and we read the reverse-connect hello on the request socket
	// itself.  Returns true and adopts the proxied socket into m_target_sock.
	bool ProxyConnect_blocking( CondorError *error, char const *my_ccb_return );
	// Establish a streaming (proxied) pipe to the daemon named by ccb_contact,
	// following however many nested broker hops the contact encodes
	// (<entry-broker>#id1#...#idN).  For a flat contact this connects to the
	// broker directly; for a nested contact it first recurses to reach the inner
	// broker over a spliced pipe, then runs CCB_REQUEST over that pipe (end-to-end
	// CEDAR with the broker through the relay).  Returns a raw ReliSock that is a
	// transparent end-to-end pipe to the daemon (caller runs CEDAR over it), or
	// nullptr.  depth guards against a looping/pathological contact.
	ReliSock *establishStreamingPipe( const std::string &ccb_contact,
			char const *my_ccb_return, unsigned depth, CondorError *error );
	// Non-blocking version of the streaming/proxy connect, used from the
	// non-blocking try_next_ccb path.  Drives the broker exchange (connect+auth,
	// request, reply, proxied hello) asynchronously via DaemonCore callbacks and
	// then adopts the proxied socket into m_target_sock, without blocking the
	// DaemonCore main loop.  Returns true if the async operation was initiated.
	bool ProxyConnect_nonblocking( char const *ccbid, char const *my_ccb_return );
	// Start the CCB_REQUEST for the current hop (m_proxy_hop) over m_proxy_sock,
	// resetting the socket's session/crypto state first for hops after the entry
	// broker so a fresh end-to-end handshake runs with the inner broker.
	void startProxyHop();
	// Continuation after the current hop's CCB_REQUEST command handshake: send the
	// request ClassAd and register to await the reply + proxied hello.
	void ProxyHopCommandCallback( bool success, Sock *sock );
	int ProxyMsgHandler( Stream *stream );
	void ProxyConnectFailed();
	void CleanupProxySock();
	// Returns our own CCB-routed return address if streaming is enabled and we
	// are behind a CCB, else empty string.
	std::string streamingReturnAddr();

	bool AcceptReversedConnection(std::shared_ptr<ReliSock> listen_sock,std::shared_ptr<SharedPortEndpoint> shared_listener);
	bool HandleReversedConnectionRequestReply(CondorError *error);

	bool try_next_ccb();
	void CCBResultsCallback(DCMsgCallback *cb);
	void ReverseConnectCallback(Sock *sock);
	void RegisterReverseConnectCallback();
	void UnregisterReverseConnectCallback();
	static int ReverseConnectCommandHandler(int cmd,Stream *stream);
	std::string myName();
	void DeadlineExpired(int timerID = -1);

	// CCB contact information should be an opaque token to everyone, but
	// Sinful needs to be able parse CCB IDs to generate v1 addresses.
	friend class Sinful;
};

#endif
