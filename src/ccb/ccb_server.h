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

#ifndef __CCB_H__
#define __CCB_H__

/* CCBServer: the Condor Connection Broker
   This is instantiated by the collector to provide connection brokering
   service.  Daemons (called "targets") register with the server and keep
   a TCP connection to it to exchange messages with the server.  Clients
   send requests to the server to get a target daemon to do a reverse
   connect to the client.
 */

#include "dc_service.h"
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

class StatisticsPool;
void AddCCBStatsToPool(StatisticsPool& pool, int publevel);

class Sock;
class ReliSock;
class Stream;
namespace classad { class ClassAd; }

class CCBTarget;
class CCBServerRequest;
class CCBReconnectInfo;
class CCBProxySession;
class CCBListeners;
struct OldClientRelay;

typedef unsigned long CCBID;

// condor/cedar connection broker
class CCBServer: Service {
 public:
	CCBServer();
	~CCBServer();

	void InitAndReconfig();

	static void CCBIDToContactString( char const * my_address, CCBID ccbid, std::string & ccb_contact );
	static bool CCBIDFromString( CCBID & ccbid, char const * ccbid_str );

	friend class CCBTarget;
		// The upstream registration listener hands routed (tunneled) reverse-connect
		// requests back to us via StartInboundRelay (which takes a CCBRouteContext).
	friend class CCBListener;
 private:
	bool m_registered_handlers;
	std::map<CCBID,CCBTarget *> m_targets;        // ccbid --> target
	std::map<CCBID,CCBReconnectInfo *> m_reconnect_info;
		// Our own direct address; the address local registrants reverse-connect to,
		// and the default address stamped into the CCBIDs we hand out.  Always the
		// direct address -- never overwritten with a tunnel contact.
	std::string m_address;
		// Tunneling (CCB_OUTBOUND_NEXT_HOP): when this broker is an "inside" CCB it
		// registers with the next-hop ("outside") broker so it is itself reachable
		// through the tunnel.  Empty until that registration completes; then it holds
		// the derived tunnel contact (<next_hop>#<inside_id>, bracket-stripped) that
		// is stamped into the CCBIDs we hand out so they nest
		// (<next_hop>#<inside_id>#<local_id>) and route inbound through both brokers.
		//
		// CCB_OUTBOUND_NEXT_HOP may name SEVERAL brokers, so this is in general a
		// SPACE-SEPARATED LIST of such contacts -- one tunnel path per next hop (each
		// itself possibly nested, if that next hop is in turn tunneled).  Every
		// consumer must treat it as a list and act on each element (see
		// CCBIDToContactString, which stamps the ccbid onto each path); a single next
		// hop is just the one-element case.  Its non-emptiness is the authoritative
		// "the inbound tunnel is ready" flag.
	std::string m_tunnel_contacts;
	CCBListeners *m_upstream_ccb;
		// Registrations whose CCB_REGISTER reply we deferred because this inside CCB
		// was not yet tunnel-ready (their contact would have been non-nested).
		// OnUpstreamRegistered flushes these once m_tunnel_contacts is derived.
	std::vector<CCBID> m_pending_registration_replies;
	std::string m_reconnect_fname;
	FILE *m_reconnect_fp;
	time_t m_last_reconnect_info_sweep;
	int m_reconnect_info_sweep_interval;
	bool m_reconnect_allowed_from_any_ip;
	CCBID m_next_ccbid;
	CCBID m_next_request_id;
	int m_read_buffer_size;
	int m_write_buffer_size;

		// we hold onto client requests so we can propagate failures
		// to them if things go wrong
	std::map<CCBID,CCBServerRequest *> m_requests;// request_id --> req

		// streaming/proxy sessions (private-to-private), keyed by connect id.
		// Held by shared_ptr: once relaying, each of the two relay sockets' handlers
		// also captures a shared_ptr, so the session lives until both the map entry
		// is detached (BeginRelayTeardown) and both relay sockets are gone.
	std::map<std::string,std::shared_ptr<CCBProxySession>> m_proxy_sessions;
		// connect ids of the sessions in m_proxy_sessions that are still in the
		// handshake (not yet relaying), so CCB_SERVER_MAX_STREAMING_HANDSHAKES can
		// be enforced in O(1) on the connection path.  A session is erased here
		// when it starts relaying and (idempotently) when it is destroyed; because
		// erase-of-absent is a no-op, a missed transition self-heals at teardown
		// rather than wedging the count forever the way a bare counter would.
	std::unordered_set<std::string> m_pending_handshakes;

	int m_polling_timer;
		// The epoll file descriptor.  Only used on platforms where
		// epoll is available.
	int m_epfd;

	void AddTarget( CCBTarget *target );
	void RemoveTarget( CCBTarget *target );
	CCBTarget *GetTarget( CCBID ccbid );
	bool ReconnectTarget( CCBTarget *target, CCBID reconnect_cookie );

	void AddRequest( CCBServerRequest *request, CCBTarget *target );
	void RemoveRequest( CCBServerRequest *request );
	CCBServerRequest *GetRequest( CCBID request_id );

	void SendHeartbeatResponse( CCBTarget *target );

	void ForwardRequestToTarget( CCBServerRequest *request, CCBTarget *target );
	void RequestReply( Sock *sock, bool success, char const *error_msg, CCBID request_cid, CCBID target_cid );

		// --- streaming/proxy mode (CCB_STREAMING) ---
		// True if the requester is itself behind a CCB (its return address is
		// CCB-routed), so it cannot accept a direct reverse connection.
	bool RequestNeedsProxy( classad::ClassAd &msg, char const *return_addr );
		// Add/remove a connect id to the in-handshake set, keeping the
		// CCBStreamingHandshakesActive gauge (and thus its published peak) in sync.
	void pendingHandshakeInsert( const std::string &connect_id );
	void pendingHandshakeErase( const std::string &connect_id );
		// Recursive inbound-tunnel routing/audit carried alongside a proxy request.
		// `route` is the space-separated CCBIDs still to be reached *after* this
		// hop's target (empty at the final hop -- the target is then the real EP).
		// When `route` is non-empty the target is itself a downstream CCB and is
		// asked to recurse (set up the next hop, then reverse-connect and relay).
		// The audit trail (original end requester and the immediately-prior hop) is
		// forwarded so inner CCBs can log who a tunneled connection is for without
		// authenticating the client.  reply_to_requester is true only on the
		// client-facing (outermost) hop; inner relay hops just splice, since the
		// outermost CCB already answered the client.
	struct CCBRouteContext {
		std::string route;
		std::string original_requester;
		std::string prior_hop;
		bool reply_to_requester{true};
	};
		// Begin a proxy session: register a rendezvous keyed by connect_id and
		// ask the target to reverse-connect to this broker, forwarding `route_ctx`
		// so the target recurses when there are more hops to reach.
	void StartProxyRequest( ReliSock *requester, CCBTarget *target, char const *connect_id, char const *name, const CCBRouteContext &route_ctx );
		// Intermediate-CCB entry point: this broker was asked (over its upstream
		// registration) to relay a tunneled connection whose remaining route is
		// `route_ctx.route`.  `upstream` is our just-established reverse connection
		// to the requesting broker; set up the next hop toward the route's head and
		// splice it to `upstream`.  Called from the CCBListener reverse-connect path.
	void StartInboundRelay( ReliSock *upstream, const CCBRouteContext &route_ctx );
		// Back-compat for a client that does not understand tunneling: it mis-splits
		// a nested contact "<outside>#42#17" on the first '#' and sends us the chained
		// CCBID "42#17".  Such a client cannot relay, but it is directly reachable, so
		// we (the outermost broker) reverse-connect to it ourselves -- as if we were
		// the target -- and relay that connection down the tunnel to the real
		// endpoint; the client just sees an ordinary reverse-connect.  request_sock is
		// its CCB_REQUEST socket (we own it); client_addr is where to dial it back;
		// connect_id is the secret it will match on the reverse connection.
	void HandleOldClientTunnelRequest( ReliSock *request_sock,
			const std::string &nested_id, const std::string &client_addr,
			const std::string &connect_id );
		// Completion of the non-blocking reverse dial to the old client: send the
		// reverse-connect hello, answer its request, and hand the socket to
		// StartProxyRequest to relay down the tunnel.
	int OldClientReverseConnected( std::shared_ptr<OldClientRelay> ctx, ReliSock *client_sock );
		// Raw command handler for the target's inbound reverse connection.
	int HandleReverseConnect( int cmd, Stream *stream );
		// daemonCore socket handler that relays bytes for an active proxy.
	int ProxyRelayData( CCBProxySession *session );
		// Common relay-startup tail: disable crypto, go non-blocking, register the
		// relay sockets with the shared relay handler, and count the session.  Used
		// by both the inbound-streaming (HandleReverseConnect) and outbound-proxy
		// paths; both hand over the connected target in session->target first.
		// Returns false (and tears the session down) if it cannot start.
	bool StartRelay( std::shared_ptr<CCBProxySession> session );
		// Tear down a proxy session that is not yet relaying (handshake phase):
		// no relay sockets are registered, so this frees it synchronously.
	void DestroyProxySession( CCBProxySession *session );
		// Begin tearing down a *relaying* session: detach it from m_proxy_sessions
		// (dropping that shared_ptr) and shut down both relay sockets so each wakes,
		// returns non-KEEP_STREAM, and is cancelled+deleted by DaemonCore.  The
		// session is freed once the last relay handler (holding a shared_ptr) is
		// destroyed.  Idempotent.
	void BeginRelayTeardown( CCBProxySession *session );
		// Periodic maintenance: reap proxy sessions whose handshake never
		// completed (called from PollSockets, the existing CCB poll timer).
	void SweepProxySessions();

		// --- outbound-CCB / tunneling mode (CCB_OUTBOUND_PROXY) ---
		// Command handler: a requester asks this broker to dial a target on its
		// behalf and splice the two sockets into a relay.
	int HandleProxyConnect( int cmd, Stream *stream );
		// Create the session and start a non-blocking outbound dial to the target.
		// ttl is the decrementing outbound time-to-live set by the originator; when we
		// forward to a next hop we pass ttl-1 and refuse once it reaches 0, so the
		// chain length is bounded by the endpoint rather than by this broker.
	void StartOutgoingProxyRequest( ReliSock *requester,
			const std::string &target, const std::string &connect_id,
			int ttl );
		// daemonCore handler driving the non-blocking outbound connect; on success
		// replies to the requester and starts the relay, else tears the session down.
	int OutgoingConnectComplete( std::shared_ptr<CCBProxySession> session, ReliSock *out );
		// True if this broker's outbound proxy is permitted to dial the target
		// (CCB_OUTBOUND_TARGET_ALLOWLIST; loopback/link-local always refused).
	bool OutboundTargetAllowed( const std::string &target );

		// The address(es) to stamp into the CCBIDs we hand out: the derived tunnel
		// contact(s) once upstream registration completes, else our direct address.
		// This may be a SPACE-SEPARATED LIST (one tunnel path per next hop; see
		// m_tunnel_contacts), so callers must stamp the ccbid onto each element
		// rather than treating the whole string as one address -- CCBIDToContactString
		// does exactly that.
	const std::string &StampAddresses() const {
		return m_tunnel_contacts.empty() ? m_address : m_tunnel_contacts;
	}

		// --- tunneling: inside-CCB upstream registration (CCB_OUTBOUND_NEXT_HOP) ---
		// Begin registering with the next-hop broker (non-blocking, so a slow or
		// down broker does not stall our startup; the CCB listener retries the
		// connection on its own).  Completion is delivered via a callback.
	void RegisterUpstream( const std::string &next_hop );
		// Registration-completion callback: once the upstream listener is assigned a
		// ccbid, derive m_tunnel_contacts and publish the ready file.  Fires again on
		// reconnect, so a changed ccbid is picked up.
	void OnUpstreamRegistered();
		// Signal the master (DC_SET_READY) that the inbound tunnel is ready, so it can
		// start the daemons that depend on this inside CCB's tunnel address.
	void NotifyMasterTunnelReady();

	void RequestFinished( CCBServerRequest *request, bool success, char const *error_msg );

	int HandleRequestDisconnect( Stream *stream );

	void PollSockets(int timerID = -1);
	int EpollSockets(int);
	void EpollAdd(CCBTarget *);
	void EpollRemove(CCBTarget *);
	void SetSmallBuffers(Sock *sock) const;

	int HandleRegistration(int cmd,Stream *stream);
		// Send a just-registered target its CCB_REGISTER reply (assigned nested
		// contact + reconnect cookie).  Split out so a tunneling inside CCB can defer
		// it until tunnel-ready (see m_pending_registration_replies).
	void SendRegistrationReply( CCBTarget *target );
	int HandleRequest(int cmd,Stream *stream);
	void HandleRequestResultsMsg( CCBTarget *target );
	int HandleRequestResultsMsg( Stream *stream );

	void RegisterHandlers();

	CCBReconnectInfo *GetReconnectInfo(CCBID ccbid);
	void AddReconnectInfo( CCBReconnectInfo *reconnect_info );
	void CloseReconnectFile();
	bool OpenReconnectFileIfExists();
	bool OpenReconnectFile(bool only_if_exists=false);
	void LoadReconnectInfo();
	bool SaveReconnectInfo(CCBReconnectInfo *reconnect_info);
	void SaveAllReconnectInfo();
	void SweepReconnectInfo();
};

// the CCB server's state associated with a target daemon
class CCBTarget {
 public:
	CCBTarget(Sock *sock);
	~CCBTarget();

	CCBID getCCBID() const { return m_ccbid; }
	void setCCBID(CCBID ccbid) { m_ccbid = ccbid; }
	Sock *getSock() { return m_sock; }

	std::map<CCBID,CCBServerRequest *> *getRequests() { return m_requests; }
	void AddRequest(CCBServerRequest *request, CCBServer *ccb_server);
	void RemoveRequest(CCBServerRequest *request);
	int NumRequests() { return m_requests ? m_requests->size() : 0; }

	// expecting a response from this target for a request we just sent it
	void incPendingRequestResults(CCBServer *ccb_server);
	// just received a response from this target
	void decPendingRequestResults();

 private:
	Sock *m_sock;         // used to send CCB messages to this target daemon
	CCBID m_ccbid;        // CCBServer-assigned identifier
	int m_pending_request_results;
	bool m_socket_is_registered;

		// requests directed to this target daemon
	std::map<CCBID,CCBServerRequest *> *m_requests;// request_id --> req
};

// represents a client that connected to CCBServer and requested a connection
// from a registered target
class CCBServerRequest {
 public:
	CCBServerRequest(Sock *sock,CCBID target_ccbid,char const *return_addr,char const *connect_id);
	~CCBServerRequest();

	Sock *getSock() { return m_sock; }
	void setRequestID( CCBID request_id ) { m_request_id = request_id; }
	CCBID getRequestID() const { return m_request_id; }
	CCBID getTargetCCBID() const { return m_target_ccbid; }
	char const *getReturnAddr() { return m_return_addr.c_str(); }
	char const *getConnectID() { return m_connect_id.c_str(); }

 private:
	Sock *m_sock;
	CCBID m_target_ccbid;    // target daemon requested by client
	CCBID m_request_id;      // CCBServer-assigned identifier for this request
	std::string m_return_addr;
	std::string m_connect_id;
};

class CCBReconnectInfo {
public:
	CCBReconnectInfo(CCBID ccbid,CCBID reconnect_cookie,char const *peer_ip);

	void alive()                     { m_last_alive = time(NULL); }
	time_t getLastAlive() const      { return m_last_alive; }
	CCBID getCCBID() const           { return m_ccbid; }
	CCBID getReconnectCookie() const { return m_reconnect_cookie; }
	char const *getPeerIP() const    { return m_peer_ip; }

private:
	CCBID m_ccbid;
	CCBID m_reconnect_cookie;
	time_t m_last_alive;
	char m_peer_ip[IP_STRING_BUF_SIZE];
};

#endif
