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
#include <condor_daemon_core.h>
#include "ccb_server.h"
#include "ccb_listener.h"
#include "condor_sinful.h"
#include "condor_netaddr.h"
#include "condor_sockaddr.h"
#include "condor_crypt.h"
#include "daemon.h"
#include "dc_message.h"
#include "subsystem_info.h"
#include "stl_string_utils.h"
#include "util_lib_proto.h"
#include "condor_open.h"
#include "generic_stats.h"

#ifdef CONDOR_HAVE_EPOLL
#include <sys/epoll.h>
#endif

struct CCBStats {
	stats_entry_abs<int> CCBEndpointsConnected;
	stats_entry_abs<int> CCBEndpointsRegistered;
	stats_entry_recent<int> CCBReconnects;
	stats_entry_recent<int> CCBRequests;
	stats_entry_recent<int> CCBRequestsNotFound;
	stats_entry_recent<int> CCBRequestsSucceeded;
	stats_entry_recent<int> CCBRequestsFailed;
	// Streaming/proxy mode: proxy requests received; sessions the broker
	// successfully spliced; relays that failed *after* the splice was established
	// (a clean shutdown / EOF is not counted as a failure); how many are relaying
	// right now; and the total bytes relayed (summed over both directions).
	stats_entry_recent<int> CCBStreamingRequests;
	stats_entry_recent<int> CCBStreamingSessions;
	stats_entry_recent<int> CCBStreamingFailed;
	stats_entry_abs<int> CCBStreamingActive;
	stats_entry_recent<int64_t> CCBStreamingBytes;
	// Proxy sessions still in the handshake (asked the target to connect back, not
	// yet relaying).  An absolute gauge, so it also publishes its peak -- the
	// high-water mark of concurrent handshakes, useful for sizing
	// CCB_SERVER_MAX_STREAMING_HANDSHAKES.
	stats_entry_abs<int> CCBStreamingHandshakesActive;
	// Outbound-proxy mode (CCB_PROXY_CONNECT): requests received; how many this
	// broker forwarded to a next-hop broker (rather than dialing the target itself,
	// i.e. this broker is an inside hop of an outbound tunnel); and requests that
	// failed.  The spliced relay itself is counted by the CCBStreaming* stats above,
	// shared with the inbound streaming path.
	stats_entry_recent<int> CCBOutboundRequests;
	stats_entry_recent<int> CCBOutboundForwarded;
	stats_entry_recent<int> CCBOutboundFailed;
	// Inbound tunnel: intermediate relay hops this broker set up on behalf of an
	// upstream broker (StartInboundRelay) -- i.e. how often this broker acted as a
	// middle hop of an inbound tunnel rather than the endpoint broker.  ...Failed
	// counts relays we could not begin because the next hop was unreachable (its
	// ccbid is not registered here); a relay that fails *after* the splice is
	// counted by CCBStreamingFailed, shared with the streaming path.
	stats_entry_recent<int> CCBTunnelRelays;
	stats_entry_recent<int> CCBTunnelRelaysFailed;

	void AddStatsToPool(StatisticsPool& pool, int publevel)
	{
		STATS_POOL_ADD(pool, "", CCBEndpointsConnected, publevel);
		STATS_POOL_ADD(pool, "", CCBEndpointsRegistered, publevel);
		STATS_POOL_ADD(pool, "", CCBReconnects, publevel);
		STATS_POOL_ADD(pool, "", CCBRequests, publevel);
		STATS_POOL_ADD(pool, "", CCBRequestsNotFound, publevel);
		STATS_POOL_ADD(pool, "", CCBRequestsSucceeded, publevel);
		STATS_POOL_ADD(pool, "", CCBRequestsFailed, publevel);
		STATS_POOL_ADD(pool, "", CCBStreamingRequests, publevel);
		STATS_POOL_ADD(pool, "", CCBStreamingSessions, publevel);
		STATS_POOL_ADD(pool, "", CCBStreamingFailed, publevel);
		STATS_POOL_ADD(pool, "", CCBStreamingActive, publevel);
		STATS_POOL_ADD(pool, "", CCBStreamingHandshakesActive, publevel);
		STATS_POOL_ADD(pool, "", CCBStreamingBytes, publevel);
		STATS_POOL_ADD(pool, "", CCBOutboundRequests, publevel);
		STATS_POOL_ADD(pool, "", CCBOutboundForwarded, publevel);
		STATS_POOL_ADD(pool, "", CCBOutboundFailed, publevel);
		STATS_POOL_ADD(pool, "", CCBTunnelRelays, publevel);
		STATS_POOL_ADD(pool, "", CCBTunnelRelaysFailed, publevel);
	}
};

static CCBStats ccb_stats;

void AddCCBStatsToPool(StatisticsPool& pool, int publevel)
{
	ccb_stats.AddStatsToPool(pool, publevel);
}

bool
CCBServer::CCBIDFromString(CCBID &ccbid,char const *ccbid_str)
{
		// A CCBID is a bare unsigned integer.  Require a pure run of decimal digits:
		// reject an empty string, a leading sign/whitespace, and any trailing junk
		// (e.g. "42#17" from a malformed nested contact, or "42x"), so a bad value is
		// not silently accepted as its numeric prefix.  This strictness belongs in
		// the parser, so every entry point (registration, reconnect, request, and
		// nested-contact splitting) is protected, not just individual call sites.
	if( !ccbid_str || *ccbid_str == '\0' ) {
		return false;
	}
	for( char const *p = ccbid_str; *p; ++p ) {
		if( *p < '0' || *p > '9' ) {
			return false;
		}
	}
	errno = 0;
	char *endptr = nullptr;
	unsigned long val = strtoul( ccbid_str, &endptr, 10 );
	if( errno != 0 || *endptr != '\0' ) {
		return false;   // overflow or (already excluded) trailing junk
	}
	ccbid = val;
	return true;
}

static char const *
CCBIDToString(CCBID ccbid,std::string &ccbid_str)
{
	formatstr(ccbid_str,"%lu",ccbid);
	return ccbid_str.c_str();
}

static bool
CCBIDFromContactString(CCBID &ccbid,char const *ccb_contact)
{
	// Format is "<address>#CCBID"; for a tunneled (nested) contact the address is
	// itself a contact, so split on the LAST '#' to get this daemon's own id.
	ccb_contact = strrchr(ccb_contact,'#');
	if( !ccb_contact ) {
		return false;
	}
	return CCBServer::CCBIDFromString(ccbid,ccb_contact+1);
}

void
CCBServer::CCBIDToContactString(char const *my_address,CCBID ccbid,std::string &ccb_contact)
{
		// my_address may be a space-separated list of (possibly nested) broker
		// addresses -- one per tunnel path when this is a tunneling inside CCB
		// registered through several next hops.  Stamp the ccbid onto each, so the
		// registrant advertises the whole list of nested contacts and is reachable via
		// any path.  A single address is just the one-element case.
	ccb_contact.clear();
	for( const auto &addr : StringTokenIterator( my_address, " " ) ) {
		if( !ccb_contact.empty() ) {
			ccb_contact += " ";
		}
		formatstr_cat( ccb_contact, "%s#%lu", addr.c_str(), ccbid );
	}
}

	// Upper bound on inbound-tunnel route length (registration-channel recursion),
	// mirroring the outbound hop cap; bounds a misconfigured or looping nested
	// contact so a single request cannot fan out through an unbounded chain.
static const int CCB_MAX_INBOUND_HOPS = 8;

	// An inbound tunnel route is a whitespace-separated list of bare CCBIDs (the
	// remaining downstream hops after this one).  Valid if empty, or every token is
	// a bare unsigned integer and the chain is within the depth bound.
static bool
ValidRoute( const std::string &route )
{
	int n = 0;
	for( const auto &tok : StringTokenIterator( route, " " ) ) {
		CCBID id;
		if( !CCBServer::CCBIDFromString( id, tok.c_str() ) ) {
			return false;
		}
		if( ++n > CCB_MAX_INBOUND_HOPS ) {
			return false;
		}
	}
	return true;
}

	// Split a route "id0 id1 ... idN" into its head (id0) and tail ("id1 ... idN").
	// Returns false if the route is empty.
static bool
SplitRoute( const std::string &route, std::string &head, std::string &tail )
{
	StringTokenIterator it( route, " " );
	const char *first = it.next();
	if( !first ) {
		return false;
	}
	head = first;
	tail.clear();
	for( const char *t = it.next(); t; t = it.next() ) {
		if( !tail.empty() ) { tail += " "; }
		tail += t;
	}
	return true;
}

	// Build an audit identity for a requester we authenticated: "<user> at <addr>"
	// (or just the address when unauthenticated).  Stamped as the original requester
	// at the entry CCB and propagated (for logging only) to inner CCBs.
static std::string
RequesterIdentity( Sock *sock )
{
	char const *fqu = sock->getFullyQualifiedUser();
	char const *addr = sock->get_sinful_peer();
	if( !addr || !*addr ) { addr = sock->peer_description(); }
	std::string id;
	if( fqu && *fqu ) {
		formatstr( id, "%s at %s", fqu, addr ? addr : "unknown" );
	} else {
		id = addr ? addr : "unknown";
	}
	return id;
}

CCBServer::CCBServer():
	m_registered_handlers(false),
	m_upstream_ccb(nullptr),
	m_reconnect_fp(NULL),
	m_last_reconnect_info_sweep(0),
	m_reconnect_info_sweep_interval(0),
	m_reconnect_allowed_from_any_ip(false),
	m_next_ccbid(1),
	m_next_request_id(1),
	m_read_buffer_size(0),
	m_write_buffer_size(0),
	m_polling_timer(-1),
	m_epfd(-1)
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
	if( m_upstream_ccb ) {
		delete m_upstream_ccb;
		m_upstream_ccb = nullptr;
	}
	while (!m_targets.empty()) {
		RemoveTarget(m_targets.begin()->second);
	}
	
	// Clean up reconnect info objects
	for (auto &entry : m_reconnect_info) {
		delete entry.second;
	}
	m_reconnect_info.clear();

	if (-1 != m_epfd)
	{
		daemonCore->Close_Pipe(m_epfd);
		m_epfd = -1;
	}
}

void
CCBServer::RegisterHandlers()
{
	if( m_registered_handlers ) {
		return;
	}
	m_registered_handlers = true;

		// Note that we allow several different permission levels to
		// register; however, the DAEMON permission level is the primary
		// in terms of determining policy.
	std::vector<DCpermission> alternate_perms{ADVERTISE_STARTD_PERM, ADVERTISE_SCHEDD_PERM, ADVERTISE_MASTER_PERM};
	int rc = daemonCore->Register_CommandWithPayload(
		CCB_REGISTER,
		"CCB_REGISTER",
		(CommandHandlercpp)&CCBServer::HandleRegistration,
		"CCBServer::HandleRegistration",
		this,
		DAEMON,
		false,
		STANDARD_COMMAND_PAYLOAD_TIMEOUT,
		&alternate_perms);
	ASSERT( rc >= 0 );

	rc = daemonCore->Register_CommandWithPayload(
		CCB_REQUEST,
		"CCB_REQUEST",
		(CommandHandlercpp)&CCBServer::HandleRequest,
		"CCBServer::HandleRequest",
		this,
		READ);
	ASSERT( rc >= 0 );

	if( param_boolean("CCB_SERVER_STREAMING",true) ) {
			// In streaming/proxy mode, targets reverse-connect to *us* and we
			// splice them to the requester.  This inbound connection is the
			// raw CCB_REVERSE_CONNECT command (no security handshake), so it is
			// registered with ALLOW, exactly as CCBClient does on its end.
		rc = daemonCore->Register_CommandWithPayload(
			CCB_REVERSE_CONNECT,
			"CCB_REVERSE_CONNECT",
			(CommandHandlercpp)&CCBServer::HandleReverseConnect,
			"CCBServer::HandleReverseConnect",
			this,
			ALLOW);
		ASSERT( rc >= 0 );
	}

		// An "inside" CCB (one with a next hop) is by definition a forwarding
		// outbound proxy: it forwards CCB_PROXY_CONNECT to its next hop rather than
		// dialing.  So enable outbound-proxy mode whenever CCB_OUTBOUND_PROXY is set
		// OR a next hop is configured -- otherwise a tunneling inside CCB would
		// register upstream for inbound but silently reject the outbound requests its
		// own node's daemons send it.  This is safe: a forwarding broker only ever
		// reaches its single configured next hop (not arbitrary targets), and the
		// command stays DAEMON-authorized; the SSRF-sensitive allow-list is enforced
		// by whichever broker finally dials.
	std::string outbound_next_hop;
	bool is_inside_ccb = param( outbound_next_hop, "CCB_OUTBOUND_NEXT_HOP" )
		&& !outbound_next_hop.empty();
	if( param_boolean("CCB_OUTBOUND_PROXY",false) || is_inside_ccb ) {
			// Outbound-proxy mode: a requester asks us to dial a target on its
			// behalf.  Unlike CCB_REVERSE_CONNECT (a raw rendezvous), this is an
			// authenticated, authorized command -- registered at DAEMON, never
			// ALLOW -- because it makes this broker an outbound proxy.
		rc = daemonCore->Register_CommandWithPayload(
			CCB_PROXY_CONNECT,
			"CCB_PROXY_CONNECT",
			(CommandHandlercpp)&CCBServer::HandleProxyConnect,
			"CCBServer::HandleProxyConnect",
			this,
			DAEMON);
		ASSERT( rc >= 0 );
	}
}

void
CCBServer::InitAndReconfig()
{
		// construct the CCB address to be advertised by CCB listeners
	Sinful sinful(daemonCore->publicNetworkIpAddr());
		// strip out <>'s, private address, and CCB listener info
	sinful.setPrivateAddr(NULL);
	sinful.setCCBContact(NULL);
		// We rely on the Sinful constructor recognizing sinfuls
		// without brackets.  Not sure why we bother stripping them off
		// in the first place, but we can't change that without
		// breaking backwards compabitility.
	m_address = sinful.getCCBAddressString();

		// Tunneling: if a next-hop broker is configured, register with it and, once
		// that completes, derive m_tunnel_contacts so the CCBIDs this broker hands
		// out are reachable through the tunnel.
	std::string next_hop;
	if( param( next_hop, "CCB_OUTBOUND_NEXT_HOP" ) && !next_hop.empty() ) {
		RegisterUpstream( next_hop );
	}

	m_read_buffer_size = param_integer("CCB_SERVER_READ_BUFFER",2*1024);
	m_write_buffer_size = param_integer("CCB_SERVER_WRITE_BUFFER",2*1024);

	m_last_reconnect_info_sweep = time(NULL);

	m_reconnect_info_sweep_interval = param_integer("CCB_SWEEP_INTERVAL",1200);

	CloseReconnectFile();

	m_reconnect_allowed_from_any_ip = param_boolean("CCB_RECONNECT_ALLOWED_FROM_ANY_IP", false);

	std::string old_reconnect_fname = m_reconnect_fname;
	char *fname = param("CCB_RECONNECT_FILE");
	if( fname ) {
		m_reconnect_fname = fname;
		if( m_reconnect_fname.find(".ccb_reconnect") == std::string::npos ) {
			// required for preen to ignore this file
			m_reconnect_fname += ".ccb_reconnect";
		}
		free( fname );
	}
	else {
		char *spool = param("SPOOL");
		ASSERT( spool );

		// IPv6 "hostnames" may be address literals, and Windows really
		// doesn't like colons in its filenames.
		char * myHost = NULL;
		Sinful my_addr( daemonCore->publicNetworkIpAddr() );
		if( my_addr.getHost() ) {
			myHost = strdup( my_addr.getHost() );
			for( unsigned i = 0; i < strlen( myHost ); ++i ) {
				if( myHost[i] == ':' ) { myHost[i] = '-'; }
			}
		} else {
			myHost = strdup( "localhost" );
		}

		formatstr(m_reconnect_fname, "%s%c%s-%s.ccb_reconnect",
			spool,
			DIR_DELIM_CHAR,
			myHost,
			my_addr.getSharedPortID() ?	my_addr.getSharedPortID() :
				my_addr.getPort() ? my_addr.getPort() : "0" );

		free( myHost );
		free( spool );
	}

	if( old_reconnect_fname != m_reconnect_fname &&
		!old_reconnect_fname.empty() &&
		!m_reconnect_fname.empty() )
	{
		// reconnect filename changed
		// not worth freaking out on error here
		IGNORE_RETURN remove( m_reconnect_fname.c_str() );
		IGNORE_RETURN rename( old_reconnect_fname.c_str(), m_reconnect_fname.c_str() );
	}
	if( old_reconnect_fname.empty() &&
		!m_reconnect_fname.empty() &&
		m_reconnect_info.size() == 0 )
	{
		// we are starting up from scratch, so load saved info
		LoadReconnectInfo();
	}

#ifdef CONDOR_HAVE_EPOLL
	// Keep our existing epoll fd, if we have one.
	if (m_epfd == -1) {
		if (-1 == (m_epfd = epoll_create1(EPOLL_CLOEXEC)))
		{
			dprintf(D_ALWAYS, "epoll file descriptor creation failed; will use periodic polling techniques: %s (errno=%d).\n", strerror(errno), errno);
		}

		int pipes[2] = { -1, -1 };
		int fd_to_replace = -1;
		if (m_epfd >= 0)
		{
			// Fool DC into talking to the epoll fd; note we only register the read side.
			// Yes, this is fairly gross - the decision was taken to do this instead of having
			// DC track arbitrary FDs just for this use case.
			if (daemonCore->Create_Pipe(pipes, true) == FALSE) {
				dprintf(D_ALWAYS, "Unable to create a DC pipe for watching the epoll FD\n");
				close(m_epfd);
				m_epfd = -1;
			}
		}
		if (m_epfd >= 0) {
			daemonCore->Close_Pipe(pipes[1]);
			if (daemonCore->Get_Pipe_FD(pipes[0], &fd_to_replace) == FALSE) {
				dprintf(D_ALWAYS, "Unable to lookup pipe's FD\n");
				close(m_epfd);
				m_epfd = -1;
				daemonCore->Close_Pipe(pipes[0]);
			}
		}
		if (m_epfd >= 0) {
			dup2(m_epfd, fd_to_replace);
			fcntl(fd_to_replace, F_SETFL, FD_CLOEXEC);
			close(m_epfd);
			m_epfd = pipes[0];

			// Inform DC we want to receive notifications from this FD.
			daemonCore->Register_Pipe(m_epfd,"CCB epoll FD", static_cast<PipeHandlercpp>(&CCBServer::EpollSockets),"CCB Epoll Handler", this, HANDLE_READ);
		}
	}
#endif

		// Whether or not we can use epoll, we want to set up periodic
		// polling for SweepReconnectInfo()
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
CCBServer::RegisterUpstream( const std::string &next_hop )
{
		// Register with the next-hop ("outside") broker, reusing the standard CCB
		// client machinery, so this "inside" broker is itself reachable through the
		// tunnel.  Driven directly here (not via the daemon's CCB_ADDRESS) so it is
		// not suppressed under USE_SHARED_PORT.
	if( !m_upstream_ccb ) {
		m_upstream_ccb = new CCBListeners;
	}
		// Let the upstream listener hand routed (tunneled) reverse-connect requests
		// back to us so we can set up the next hop and relay (StartInboundRelay).
	m_upstream_ccb->SetCCBServer( this );
		// Learn when registration completes via a callback; it fires again on each
		// reconnect, so a changed ccbid is picked up too.  (Set before Configure so
		// the listener it creates inherits the callback.)
	m_upstream_ccb->SetRegistrationCallback( [this]() { OnUpstreamRegistered(); } );
	m_upstream_ccb->Configure( next_hop.c_str() );
		// Register non-blocking so a slow or unreachable next hop does not stall this
		// collector's startup.  The CCB listener drives the connect and retries it
		// (CCB_RECONNECT_TIME) on its own until it succeeds; OnUpstreamRegistered
		// then derives the tunnel address from the ccbid it is assigned.
	m_upstream_ccb->RegisterWithCCBServer( false /*non-blocking*/ );

	dprintf(D_ALWAYS,
			"CCB: registering with next-hop CCB %s (non-blocking); the inbound "
			"tunnel path becomes available once it completes.\n", next_hop.c_str());
}

void
CCBServer::OnUpstreamRegistered()
{
	if( !m_upstream_ccb ) {
		return;
	}

		// CCB_OUTBOUND_NEXT_HOP may be a list of brokers, so we may register upstream
		// through several of them; GetCCBContactString space-joins one contact per
		// listener.  Each such contact is "<next_hop>#<inside_id>", where <next_hop>
		// may itself be nested if that upstream broker is in turn tunneled -- so the
		// list of paths grows recursively at every layer.  Turn each contact into the
		// bracket-stripped "<addr>#<id>" convention the CCBIDs we hand out stamp, and
		// keep them all: a local registrant then advertises the whole list of nested
		// contacts ("nextA#idA#local_id nextB#idB#local_id ..."), reachable via any
		// path.
	std::string contacts;
	m_upstream_ccb->GetCCBContactString( contacts );
	std::string tunnel_list;
	for( const auto &contact : StringTokenIterator( contacts, " " ) ) {
		std::string c = contact;
		size_t hash = c.rfind('#');
		if( hash == std::string::npos ) {
			continue;   // this listener has no ccbid yet
		}
		std::string broker = c.substr(0, hash);       // "<next_hop>" (maybe nested)
		std::string ids    = c.substr(hash + 1);      // "inside_id"
		Sinful bsin( broker.c_str() );
		if( !bsin.valid() ) {
			dprintf(D_ALWAYS,"CCB: next-hop contact %s is not a valid Sinful; skipping "
					"that tunnel path.\n", c.c_str());
			continue;
		}
		if( !tunnel_list.empty() ) {
			tunnel_list += " ";
		}
		tunnel_list += bsin.getCCBAddressString() + "#" + ids;
	}
	if( tunnel_list.empty() ) {
			// No upstream ccbid yet (should not happen from this callback, but be safe).
		return;
	}
	m_tunnel_contacts = tunnel_list;

	dprintf(D_ALWAYS,
			"CCB: registered with next-hop CCB(s); tunnel address(es) for local "
			"registrants: %s\n", m_tunnel_contacts.c_str());

		// Flush any registrations we deferred while the tunnel was coming up: now
		// their replies carry a reachable, nested contact.  (A registrant that
		// disconnected in the meantime has been removed, so skip a stale ccbid.)
	if( !m_pending_registration_replies.empty() ) {
		std::vector<CCBID> pending;
		pending.swap( m_pending_registration_replies );
		for( CCBID ccbid : pending ) {
			if( CCBTarget *t = GetTarget( ccbid ) ) {
				SendRegistrationReply( t );
			}
		}
		dprintf(D_FULLDEBUG,
				"CCB: flushed %zu deferred registration reply(ies) after upstream "
				"registration completed.\n", pending.size());
	}

		// Tell the master the inbound tunnel is ready so it can start the daemons
		// that depend on it.  We signal readiness with DC_SET_READY: the master gates
		// those daemons on this collector signalling ready (see
		// WaitBeforeStartingOtherDaemons), and readiness clears automatically if we
		// later restart/re-register.
	NotifyMasterTunnelReady();
}

void
CCBServer::NotifyMasterTunnelReady()
{
	char const *master_sinful = daemonCore->InfoCommandSinfulString(-2);
	if( !master_sinful ) {
			// Not started by a master (e.g. a standalone collector); nothing to do.
		return;
	}
	ClassAd readyAd;
	readyAd.Assign("DaemonPID", (int)getpid());
	readyAd.Assign("DaemonName", get_mySubSystem()->getName());
	readyAd.Assign("DaemonState", "Ready");
	classy_counted_ptr<Daemon> dmn = new Daemon( DT_ANY, master_sinful );
	classy_counted_ptr<ClassAdMsg> msg = new ClassAdMsg( DC_SET_READY, readyAd );
	dmn->sendMsg( msg.get() );
	dprintf(D_ALWAYS,"CCB: notified master (%s) that the inbound tunnel is ready.\n",
			master_sinful);
}


int
CCBServer::EpollSockets(int)
{
	if (m_epfd == -1)
	{
		return -1;
	}
#ifdef CONDOR_HAVE_EPOLL
	int epfd = -1;
	if (daemonCore->Get_Pipe_FD(m_epfd, &epfd) == FALSE || epfd == -1) {
		dprintf(D_ALWAYS, "Unable to lookup epoll FD\n");
		daemonCore->Close_Pipe(m_epfd);
		m_epfd = -1;
		return -1;
	}
	struct epoll_event events[10];
	bool needs_poll = true;
	unsigned counter = 0;
	while (needs_poll && counter++ < 100)
	{
		needs_poll = false;
		int result = epoll_wait(epfd, events, 10, 0);
		if (result > 0)
		{
			for (int idx=0; idx<result; idx++)
			{
				CCBID id = events[idx].data.u64;
				auto targetit = m_targets.find(id);
				if (targetit == m_targets.end()) {
					dprintf(D_FULLDEBUG, "No target found for CCBID %ld.\n", id);
					continue;
				}
				CCBTarget *target = targetit->second;
				if (target->getSock()->readReady())
				{
					HandleRequestResultsMsg(target);
				}
			}
			// We always want to drain out the queue of events.
			needs_poll = true;
		}
		else if ((result == -1) && (errno != EINTR))
		{
			dprintf(D_ALWAYS, "Error when waiting on epoll: %s (errno=%d).\n", strerror(errno), errno);
		}
			// Fall through silently on timeout or signal interrupt; DC will call us later.
	}
#endif
	return 0;
}

void
CCBServer::EpollAdd(CCBTarget *target)
{
	if ((-1 == m_epfd) || !target) {return;}
#ifdef CONDOR_HAVE_EPOLL
	int epfd = -1;
	if (daemonCore->Get_Pipe_FD(m_epfd, &epfd) == FALSE || epfd == -1) {
		dprintf(D_ALWAYS, "Unable to lookup epoll FD\n");
		daemonCore->Close_Pipe(m_epfd);
		m_epfd = -1;
		return;
	}       
		// We have epoll maintain the map of FD -> CCBID for us by taking
		// advantage of the data field of the epoll event.  This way, when the
		// epoll watch fires, we can do a hash table lookup for the target object.
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.u64 = target->getCCBID();
	dprintf(D_NETWORK, "Registering file descriptor %d with CCBID %ld.\n", target->getSock()->get_file_desc(), event.data.u64);
	if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, target->getSock()->get_file_desc(), &event))
	{
		dprintf(D_ALWAYS, "CCB: failed to add watch for target daemon %s with ccbid %lu: %s (errno=%d).\n", target->getSock()->peer_description(), target->getCCBID(), strerror(errno), errno);
	}
#endif
}

void
CCBServer::EpollRemove(CCBTarget *target)
{
	if ((-1 == m_epfd) || !target) {return;}
#ifdef CONDOR_HAVE_EPOLL
	int epfd = -1;
	if (daemonCore->Get_Pipe_FD(m_epfd, &epfd) == FALSE || epfd == -1) {
		dprintf(D_ALWAYS, "Unable to lookup epoll FD\n");
		daemonCore->Close_Pipe(m_epfd);
		m_epfd = -1;
		return;
	}       
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.u64 = target->getCCBID();
	if (-1 == epoll_ctl(epfd, EPOLL_CTL_DEL, target->getSock()->get_file_desc(), &event))
	{       
		dprintf(D_ALWAYS, "CCB: failed to delete watch for target daemon %s with ccbid %lu: %s (errno=%d).\n", target->getSock()->peer_description(), target->getCCBID(), strerror(errno), errno);
	}
#endif
}

void
CCBServer::PollSockets(int /* timerID */)
{
		// Find out if any of our registered target daemons have
		// disconnected or sent us a response to a request.
		// We don't just register all of these sockets with DaemonCore
		// out of fear that the overhead of dealing with all of these
		// sockets in every iteration of the select loop may be
		// too much.
	if (m_epfd == -1)
	{
		auto it = m_targets.begin();
		while (it != m_targets.end()) {
			// HandleRequestResultsMsg() may erase the element from
			// m_targets, so advance our iterator before calling it.
			auto next_it = it;
			next_it++;
			if (it->second->getSock()->readReady()) {
				HandleRequestResultsMsg(it->second);
			}
			it = next_it;
		}
	}

	// periodically call the following
	SweepReconnectInfo();
	SweepProxySessions();
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
	if( !getClassAd( sock, msg ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to receive registration "
				"from %s.\n", sock->peer_description() );
		return FALSE;
	}

	SetSmallBuffers(sock);

	std::string name;
	if( msg.LookupString(ATTR_NAME,name) ) {
			// target daemon name is purely for debugging purposes
		formatstr_cat(name, " on %s", sock->peer_description());
		sock->set_peer_description(name.c_str());
	}

	CCBTarget *target = new CCBTarget(sock);

	std::string reconnect_cookie_str,reconnect_ccbid_str;
	CCBID reconnect_cookie,reconnect_ccbid;
	bool reconnected = false;
	if( msg.LookupString(ATTR_CLAIM_ID,reconnect_cookie_str) &&
		CCBIDFromString(reconnect_cookie,reconnect_cookie_str.c_str()) &&
		msg.LookupString( ATTR_CCBID,reconnect_ccbid_str) &&
		CCBIDFromContactString(reconnect_ccbid,reconnect_ccbid_str.c_str()) )
	{
		target->setCCBID( reconnect_ccbid );
		reconnected = ReconnectTarget( target, reconnect_cookie );
	}

	if( !reconnected ) {
		AddTarget( target );
	}

		// A tunneling inside CCB that has not yet completed its own upstream
		// registration would stamp a bare (non-tunnel, unreachable) contact into the
		// reply.  Defer the reply until then: OnUpstreamRegistered flushes these once
		// m_tunnel_contacts is derived, so a registrant only ever learns a reachable,
		// nested contact.  This is what lets the master learn a daemon's tunnel
		// address from ordinary registration, with no separate tunnel-address query.
	if( m_upstream_ccb && m_tunnel_contacts.empty() ) {
		m_pending_registration_replies.push_back( target->getCCBID() );
		dprintf(D_FULLDEBUG,
				"CCB: deferring registration reply to %s until this inside CCB has "
				"registered upstream.\n", sock->peer_description());
		return KEEP_STREAM;
	}

	SendRegistrationReply( target );
	return KEEP_STREAM;
}

void
CCBServer::SendRegistrationReply( CCBTarget *target )
{
	CCBReconnectInfo *reconnect_info = GetReconnectInfo( target->getCCBID() );
	ASSERT( reconnect_info );

	Sock *sock = target->getSock();
	sock->encode();

	ClassAd reply_msg;
	std::string ccb_contact, reconnect_cookie_str;

		// We send our address as part of the CCB contact string, rather
		// than letting the target daemon fill it in.  This is to give us
		// potential flexibility on the CCB server side to do things like
		// assign different targets to different CCB server sub-processes,
		// each with their own command port.
	CCBIDToContactString( StampAddresses().c_str(), target->getCCBID(), ccb_contact );

	CCBIDToString( reconnect_info->getReconnectCookie(),reconnect_cookie_str );

	reply_msg.Assign(ATTR_CCBID,ccb_contact);
	reply_msg.Assign(ATTR_COMMAND,CCB_REGISTER);
	reply_msg.Assign(ATTR_CLAIM_ID,reconnect_cookie_str);

	if( !putClassAd( sock, reply_msg ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to send registration response "
				"to %s.\n", sock->peer_description() );

		RemoveTarget( target );
	}
}

void
CCBServer::SetSmallBuffers(Sock *sock) const
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
	if( !getClassAd( sock, msg ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to receive request "
				"from %s.\n", sock->peer_description() );
		return FALSE;
	}

	std::string name;
	if( msg.LookupString(ATTR_NAME,name) ) {
			// client name is purely for debugging purposes
		formatstr_cat(name, " on %s", sock->peer_description());
		sock->set_peer_description(name.c_str());
	}
	std::string target_ccbid_str;
	std::string return_addr;
	std::string connect_id; // id target daemon should present to requester
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
		std::string ad_str;
		sPrintAd(ad_str, msg);
		dprintf(D_ALWAYS,
				"CCB: invalid request from %s: %s\n",
				sock->peer_description(), ad_str.c_str() );
		return FALSE;
	}
		// A requested CCBID must be a bare unsigned integer.  CCBIDFromString rejects
		// anything with extra characters -- in particular a nested/chained id such as
		// "42#17", which is what an old client produces when it mis-splits a tunneled
		// (nested) contact on the first '#'.  A new client resolves the nested contact
		// itself and sends the bare id (plus a CCBRoute) for each hop.
	if( !CCBIDFromString(target_ccbid,target_ccbid_str.c_str()) )
	{
			// An old, non-tunneling client that cannot relay (did not ask for
			// streaming) but IS directly reachable: reverse-connect to it ourselves
			// and relay that connection down the tunnel (see
			// HandleOldClientTunnelRequest).  A malformed id with no '#', or a
			// streaming client that still sent a chained id, is a real error.
		bool streaming = false;
		msg.LookupBool( "CCBStreamingRequired", streaming );
		if( !streaming && target_ccbid_str.find('#') != std::string::npos ) {
			HandleOldClientTunnelRequest( sock, target_ccbid_str, return_addr, connect_id );
			return KEEP_STREAM;
		}
		dprintf(D_ALWAYS,
				"CCB: request from %s contains invalid CCBID %s\n",
				sock->peer_description(), target_ccbid_str.c_str() );
		RequestReply( sock, false, "invalid CCBID", 0, 0 );
		return FALSE;
	}

	CCBTarget *target = GetTarget( target_ccbid );
	if( !target ) {
		dprintf(D_ALWAYS,
			"CCB: rejecting request from %s for ccbid %s because no daemon is "
			"currently registered with that id "
			"(perhaps it recently disconnected).\n",
			sock->peer_description(), target_ccbid_str.c_str());

		std::string error_msg;
		formatstr( error_msg,
			"CCB server rejecting request for ccbid %s because no daemon is "
			"currently registered with that id "
			"(perhaps it recently disconnected).", target_ccbid_str.c_str());
		RequestReply( sock, false, error_msg.c_str(), 0, target_ccbid );
		ccb_stats.CCBRequests += 1;
		ccb_stats.CCBRequestsNotFound += 1;
		return FALSE;
	}

		// Streaming/proxy mode: if the requester is itself behind a CCB (its
		// return address is CCB-routed, or it explicitly asked for streaming),
		// it cannot accept a direct reverse connection.  Instead we keep its
		// socket, have the target reverse-connect to us, and splice the two.
		//
		// Do NOT shrink this socket's OS buffers (below): unlike a normal CCB
		// request -- a one-shot exchange of small ClassAds -- a streaming
		// request socket becomes a data relay carrying a peer's entire
		// connection, so it should keep the (autotuned / inherited) socket
		// buffers rather than the tiny control-plane size.
	if( RequestNeedsProxy( msg, return_addr.c_str() ) ) {
		CCBRouteContext route_ctx;
			// The client may hand us a route: the remaining CCBIDs to reach after
			// this target (a nested/tunneled contact).  When present, `target` is a
			// downstream CCB and we ask it to recurse; when absent, `target` is the
			// real endpoint.
		msg.LookupString( ATTR_CCB_ROUTE, route_ctx.route );
		if( !ValidRoute( route_ctx.route ) ) {
			dprintf(D_ALWAYS,
					"CCB: rejecting request from %s: invalid or too-deep tunnel route "
					"'%s'.\n", sock->peer_description(), route_ctx.route.c_str());
			RequestReply( sock, false, "invalid tunnel route", 0, target_ccbid );
			return FALSE;
		}
			// We authenticated the client, so we -- not the (spoofable) request ad --
			// stamp the audit trail: the original requester is this authenticated
			// peer, and each hop we forward to records us as the prior hop.  Inner
			// CCBs cannot authenticate the client, so they trust and propagate this
			// for logging only (never for authorization).
		route_ctx.original_requester = RequesterIdentity( sock );
		route_ctx.prior_hop = m_address;
		route_ctx.reply_to_requester = true;
		StartProxyRequest( sock, target, connect_id.c_str(), sock->peer_description(), route_ctx );
		return KEEP_STREAM;
	}

		// The requester insisted on streaming (it cannot accept a direct reverse
		// connection) but we are not going to proxy -- streaming is disabled here
		// (CCB_SERVER_STREAMING is false).  Reject cleanly with an explanatory
		// message rather than forwarding a request the target cannot satisfy,
		// which would otherwise fail slowly and confusingly.
	bool streaming_required = false;
	if( msg.LookupBool("CCBStreamingRequired", streaming_required) && streaming_required ) {
		dprintf(D_ALWAYS,
				"CCB: rejecting request from %s for ccbid %s: it requires CCB "
				"streaming mode, which is disabled on this broker "
				"(CCB_SERVER_STREAMING is false).\n",
				sock->peer_description(), target_ccbid_str.c_str());
		RequestReply( sock, false,
					  "CCB streaming mode is required to reach this daemon but is "
					  "disabled on the CCB server (CCB_SERVER_STREAMING is false)",
					  0, target_ccbid );
		return FALSE;
	}

		// Normal CCB request: only small ClassAds flow on this socket, and the
		// broker may hold many of them, so shrink its buffers.
	SetSmallBuffers(sock);

	CCBServerRequest *request =
		new CCBServerRequest(
			sock,
			target_ccbid,
			return_addr.c_str(),
			connect_id.c_str() );
	AddRequest( request, target );

	dprintf(D_FULLDEBUG,
			"CCB: received request id %lu from %s for target ccbid %s "
			"(registered as %s)\n",
			request->getRequestID(),
			request->getSock()->peer_description(),
			target_ccbid_str.c_str(),
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
	if( !getClassAd( sock, msg ) || !sock->end_of_message() ) {
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
	std::string error_msg;
	std::string reqid_str;
	CCBID reqid;
	std::string connect_id;
	msg.LookupBool( ATTR_RESULT, success );
	msg.LookupString( ATTR_ERROR_STRING, error_msg );
	msg.LookupString( ATTR_REQUEST_ID, reqid_str );
	msg.LookupString( ATTR_CLAIM_ID, connect_id );

	if( !CCBIDFromString( reqid, reqid_str.c_str() ) ) {
		std::string msg_str;
		sPrintAd(msg_str, msg);
		dprintf(D_ALWAYS,
				"CCB: received reply from target daemon %s with ccbid %lu "
				"without a valid request id: %s\n",
				sock->peer_description(),
				target->getCCBID(),
				msg_str.c_str());
		RemoveTarget( target );
		return;
	}

	CCBServerRequest *request = GetRequest( reqid );
	if( request && request->getSock()->readReady() ) {
		// Request socket must have just closed.  To avoid noise in
		// logs when we fail to write to it, delete the request now.
		RemoveRequest( request );
		request = NULL;
		if (success) {
			ccb_stats.CCBRequestsSucceeded += 1;
		} else {
			ccb_stats.CCBRequestsFailed += 1;
		}
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
				reqid_str.c_str(),
				request_desc);
	}
	else {
		dprintf(D_FULLDEBUG,"CCB: received error from target daemon %s "
				"with ccbid %lu for "
				"request %s from %s: %s\n",
				sock->peer_description(),
				target->getCCBID(),
				reqid_str.c_str(),
				request_desc,
				error_msg.c_str());
	}

	if( !request ) {
		if( success ) {
				// expected: the client has gone away; it got what it wanted
			return;
		}
		dprintf( D_FULLDEBUG,
				 "CCB: client for request %s to target daemon %s with ccbid "
				 "%lu disappeared before receiving error details.\n",
				 reqid_str.c_str(),
				 sock->peer_description(),
				 target->getCCBID());
		return;
	}
	if( connect_id != request->getConnectID() ) {
		dprintf( D_FULLDEBUG,
				 "CCB: received wrong connect id (%s) from target daemon %s "
				 "with ccbid %lu for "
				 "request %s\n",
				 connect_id.c_str(),
				 sock->peer_description(),
				 target->getCCBID(),
				 reqid_str.c_str());
		RemoveTarget( target );
		return;
	}

	RequestFinished( request, success, error_msg.c_str() );
}

void
CCBServer::SendHeartbeatResponse( CCBTarget *target )
{
	Sock *sock = target->getSock();

	ClassAd msg;
	msg.Assign( ATTR_COMMAND, ALIVE );
	sock->encode();
	if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
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

	std::string reqid_str;
	CCBIDToString( request->getRequestID(), reqid_str);
	msg.Assign( ATTR_REQUEST_ID, reqid_str );

	sock->encode();
	if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
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
	if( !putClassAd( sock, msg ) || !sock->end_of_message() ) {
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
	if (success) {
		ccb_stats.CCBRequestsSucceeded += 1;
	} else {
		ccb_stats.CCBRequestsFailed += 1;
	}
}

// ---------------------------------------------------------------------------
// Streaming/proxy mode (CCB_STREAMING)
//
// When the requester is itself behind a CCB it cannot accept a direct reverse
// connection.  Instead the broker keeps the requester's CCB_REQUEST socket,
// tells the target to reverse-connect to the broker, and then splices the two
// sockets together as a transparent TCP relay.
// ---------------------------------------------------------------------------

class CCBProxySession {
public:
	// One direction of the relay.  The userspace buffer starts small and grows
	// on demand up to RELAY_BUF_MAX, so a broker carrying many mostly-idle proxy
	// sessions does not pay the full per-direction footprint for each.  It only
	// holds bytes read from the source that the destination could not yet accept;
	// the in-flight headroom that decouples a fast sender from a slow receiver
	// lives in the kernel socket buffers.  We stop reading the source while this
	// buffer is non-empty; the source's data then backs up in the kernel TCP
	// buffer, which provides backpressure for the TCP sender.
	static const int RELAY_BUF_START = 4 * 1024;          // initial shuttle size
	static const int RELAY_BUF_MAX   = 32 * 1024;         // cap it grows to
	struct Dir {
		std::vector<char> buf;  // grows from RELAY_BUF_START up to RELAY_BUF_MAX
		int  len;           // bytes currently buffered
		int  off;           // bytes already written to the destination
		bool src_eof;       // source closed its sending side
		bool dst_shutdown;  // we have shut down the destination's write side
		Dir() : len(0), off(0), src_eof(false), dst_shutdown(false) {}
		bool hasData() const { return off < len; }
		void clear() { len = 0; off = 0; }
	};

	std::string connect_id;
	std::string request_id;
	ReliSock *requester;  // endpoint "A"
	ReliSock *target;     // endpoint "B"
	// Outbound-proxy only: the socket dialing the target while the connect is still
	// in progress.  It becomes `target` once connected; until then DaemonCore has
	// not taken ownership, so DestroyProxySession deletes it.
	ReliSock *connecting;
	bool relaying;
	// True on the client-facing (outermost) hop: when the target reverse-connects
	// we answer the requester ({Result,ProxyMode} + replayed hello) before relaying.
	// False on an intermediate relay hop (StartInboundRelay), whose requester is the
	// raw upstream pipe to the outer broker -- that broker already answered the
	// client, so we must NOT send a second reply; we only splice.
	bool reply_to_requester;
	// Set once a relaying session is being torn down.  Both relay sockets share a
	// handler that captures a shared_ptr to this session; the first to fail flips
	// this flag and wakes the other, and every relay callback that sees it simply
	// returns non-KEEP_STREAM so DaemonCore cancels+deletes its socket (see
	// ProxyRelayData / BeginRelayTeardown).  The session itself is owned by the
	// shared_ptrs captured in the two relay handlers (plus the m_proxy_sessions
	// entry until teardown detaches it), so it is freed automatically once the
	// last relay socket -- and thus its handler -- is gone.  This guarantees the
	// session outlives every still-pending sibling callback without a manual
	// reference count.
	bool dying;
	// When the session was created (request forwarded to the target).  The
	// periodic maintenance sweep reaps a session that is still not relaying after
	// the handshake timeout, so a target that never connects back cannot pin the
	// requester's socket forever.
	time_t created;
	Dir a_to_b;   // bytes read from the requester, to be written to the target
	Dir b_to_a;   // bytes read from the target, to be written to the requester

	CCBProxySession() : requester(nullptr), target(nullptr), connecting(nullptr), relaying(false), reply_to_requester(true), dying(false), created(0) {}
};

// Relay helpers (defined below; forward-declared for use in HandleReverseConnect).
static bool ccbSetNonBlocking( int fd );

void
CCBServer::pendingHandshakeInsert( const std::string &connect_id )
{
	m_pending_handshakes.insert( connect_id );
	ccb_stats.CCBStreamingHandshakesActive = (int)m_pending_handshakes.size();
}

void
CCBServer::pendingHandshakeErase( const std::string &connect_id )
{
	m_pending_handshakes.erase( connect_id );
	ccb_stats.CCBStreamingHandshakesActive = (int)m_pending_handshakes.size();
}

bool
CCBServer::RequestNeedsProxy( ClassAd &msg, char const *return_addr )
{
	if( !param_boolean("CCB_SERVER_STREAMING",true) ) {
		return false;
	}
	bool required = false;
	if( msg.LookupBool("CCBStreamingRequired",required) && required ) {
		return true;
	}
	if( return_addr ) {
		Sinful s(return_addr);
		if( s.getCCBContact() ) {
			return true;
		}
	}
	return false;
}

void
CCBServer::StartProxyRequest( ReliSock *requester, CCBTarget *target, char const *connect_id, char const *name, const CCBRouteContext &route_ctx )
{
	ccb_stats.CCBStreamingRequests += 1;

		// On an intermediate relay hop the requester is the raw upstream pipe -- the
		// outer broker already answered the client -- so a CEDAR failure reply would
		// corrupt the relay; just let the caller tear the pipe down (the client then
		// sees the connection fail).  Only the client-facing hop replies.
	auto failReply = [&]( char const *errmsg ) {
		if( route_ctx.reply_to_requester ) {
			RequestReply( requester, false, errmsg, 0, target->getCCBID() );
		}
	};

		// Enforce the configured limits before committing any resources: an
		// overall cap on concurrent proxy sessions, and a tighter cap on sessions
		// still in the handshake (the more easily abused, never-completing kind).
		// 0 means unlimited.  Reject over-limit requests with a clear failure.
	int max_sessions = param_integer("CCB_SERVER_MAX_STREAMING_SESSIONS", 0, 0);
	if( max_sessions > 0 && (int)m_proxy_sessions.size() >= max_sessions ) {
		dprintf(D_ALWAYS,
				"CCB: refusing streaming request from %s: at the streaming "
				"session limit (CCB_SERVER_MAX_STREAMING_SESSIONS=%d).\n",
				requester->peer_description(), max_sessions);
		failReply( "CCB streaming session limit reached on the broker" );
		ccb_stats.CCBRequestsFailed += 1;
		delete requester;
		return;
	}
	int max_handshakes = param_integer("CCB_SERVER_MAX_STREAMING_HANDSHAKES", 0, 0);
	if( max_handshakes > 0 && (int)m_pending_handshakes.size() >= max_handshakes ) {
		dprintf(D_ALWAYS,
				"CCB: refusing streaming request from %s: at the streaming "
				"handshake limit (CCB_SERVER_MAX_STREAMING_HANDSHAKES=%d).\n",
				requester->peer_description(), max_handshakes);
		failReply( "CCB streaming handshake limit reached on the broker" );
		ccb_stats.CCBRequestsFailed += 1;
		delete requester;
		return;
	}

	auto session = std::make_shared<CCBProxySession>();
	session->connect_id = connect_id;
	session->requester = requester;
	session->created = time(nullptr);  // handshake deadline is relative to this
	CCBIDToString( m_next_request_id++, session->request_id );

		// If a session with this connect id already exists, reject (the connect
		// id is a fresh random token, so this should not normally happen).
	if( m_proxy_sessions.find(session->connect_id) != m_proxy_sessions.end() ) {
		failReply( "duplicate connect id" );
		ccb_stats.CCBRequestsFailed += 1;
		delete requester;  // session (shared_ptr) is freed on return; it never owns the socket
		return;
	}
	m_proxy_sessions[session->connect_id] = session;
	pendingHandshakeInsert( session->connect_id );  // pending until relaying

	session->reply_to_requester = route_ctx.reply_to_requester;

		// Ask the target to reverse-connect to *this broker* directly.  Use our
		// direct address (m_address), never the tunnel contact: a local target must
		// dial us directly rather than route back out through the next-hop broker to
		// reach us.
	ClassAd msg;
	msg.Assign( ATTR_COMMAND, CCB_REQUEST );
	std::string myaddr = "<" + m_address + ">";
	msg.Assign( ATTR_MY_ADDRESS, myaddr );
	msg.Assign( ATTR_CLAIM_ID, connect_id );
	msg.Assign( ATTR_REQUEST_ID, session->request_id );
	if( name ) {
		msg.Assign( ATTR_NAME, name );
	}
		// Recursive inbound tunnel: when there are more hops beyond this target, tell
		// it the remaining route so it recurses (sets up the next hop and relays);
		// carry the audit trail so it can log who this connection is really for.
	if( !route_ctx.route.empty() ) {
		msg.Assign( ATTR_CCB_ROUTE, route_ctx.route );
	}
	if( !route_ctx.original_requester.empty() ) {
		msg.Assign( ATTR_CCB_ORIGINAL_REQUESTER, route_ctx.original_requester );
	}
	if( !route_ctx.prior_hop.empty() ) {
		msg.Assign( ATTR_CCB_PRIOR_HOP, route_ctx.prior_hop );
	}

	Sock *tsock = target->getSock();
	tsock->encode();
	if( !putClassAd( tsock, msg ) || !tsock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to forward proxy request to target ccbid %lu\n",
				target->getCCBID());
		failReply( "failed to forward proxy request to target" );
		ccb_stats.CCBRequestsFailed += 1;
		DestroyProxySession( session.get() );
		return;
	}

	dprintf(D_FULLDEBUG,
			"CCB: started streaming (proxy) session for target ccbid %lu, "
			"request id %s\n",
			target->getCCBID(), session->request_id.c_str());
}

void
CCBServer::StartInboundRelay( ReliSock *upstream, const CCBRouteContext &route_ctx )
{
		// We (an intermediate CCB) were asked -- over our upstream registration, so
		// implicitly trusted -- to relay a tunneled connection.  `upstream` is our
		// just-established reverse connection to the requesting (outer) broker.  Set
		// up the next hop toward the route head and splice it to `upstream`.  The
		// outer broker already answered the client, so this hop only relays; it never
		// replies (reply_to_requester = false).
	std::string head, tail;
	if( !SplitRoute( route_ctx.route, head, tail ) ) {
			// Should not happen: the listener only routes us here for a non-empty
			// route.  Drop the pipe so the client sees the failure.
		dprintf(D_ALWAYS,"CCB: inbound relay handed an empty route; dropping.\n");
		ccb_stats.CCBTunnelRelaysFailed += 1;
		delete upstream;
		return;
	}
	CCBID target_ccbid;
	CCBTarget *target = nullptr;
	if( CCBIDFromString( target_ccbid, head.c_str() ) ) {
		target = GetTarget( target_ccbid );
	}
	if( !target ) {
		dprintf(D_ALWAYS,
				"CCB: inbound relay cannot continue: no daemon registered as next-hop "
				"ccbid %s (original requester %s); dropping.\n",
				head.c_str(), route_ctx.original_requester.c_str());
		ccb_stats.CCBTunnelRelaysFailed += 1;
		delete upstream;
		return;
	}

		// Fresh secret for the downstream rendezvous: the next hop presents it back
		// when it reverse-connects, so we can match it to this relay session.
	char *cid = Condor_Crypt_Base::randomHexKey(20);
	std::string connect_id = cid ? cid : "";
	free( cid );

	std::string name;
	formatstr( name, "inbound tunnel relay for %s",
			   route_ctx.original_requester.empty() ? "(unknown)"
			   : route_ctx.original_requester.c_str() );

		// Audit trail: log who this tunneled connection is really for (the original,
		// authenticated-at-the-entry requester) and which broker handed it to us,
		// even though we never authenticate the client ourselves.
	dprintf(D_FULLDEBUG,
			"CCB: relaying inbound tunnel to next-hop ccbid %s (remaining route '%s') "
			"for original requester %s via prior hop %s.\n",
			head.c_str(), tail.c_str(),
			route_ctx.original_requester.c_str(), route_ctx.prior_hop.c_str());

	ccb_stats.CCBTunnelRelays += 1;

	CCBRouteContext next;
	next.route = tail;                                      // hops beyond this target
	next.original_requester = route_ctx.original_requester; // propagate unchanged
	next.prior_hop = m_address;                             // we are now the prior hop
	next.reply_to_requester = false;                        // relay hop: only splice
	StartProxyRequest( upstream, target, connect_id.c_str(), name.c_str(), next );
}

// State carried across the non-blocking reverse dial to an old (non-tunneling)
// client.  We own request_sock until we answer it and delete it; client_sock (the
// dialed connection) is passed to StartProxyRequest once the dial completes.
struct OldClientRelay {
	ReliSock *request_sock{nullptr};   // the client's CCB_REQUEST socket
	std::string connect_id;            // secret the client matches on its reverse connect
	CCBID target_ccbid{0};             // entry hop toward the tunnel (route head)
	std::string route;                 // remaining hops after target (space-separated)
	std::string original_requester;    // audit: who this is really for
	std::string prior_hop;             // audit: the broker that set up the next hop (us)
};

	// Split a chained CCBID "42#17#99" into the entry hop "42" and the remaining
	// route "17 99" (the space-separated form the relay path expects).  Returns
	// false if there is no non-empty head.
static bool
SplitNestedCCBID( const std::string &nested, std::string &head, std::string &route )
{
	size_t h = nested.find('#');
	if( h == std::string::npos ) {
		head = nested;
		route.clear();
	} else {
		head = nested.substr(0, h);
		route = nested.substr(h + 1);
		for( char &c : route ) { if( c == '#' ) c = ' '; }
	}
	return !head.empty();
}

void
CCBServer::HandleOldClientTunnelRequest( ReliSock *request_sock,
		const std::string &nested_id, const std::string &client_addr,
		const std::string &connect_id )
{
	std::string head, route;
	CCBID target_ccbid = 0;
	CCBTarget *target = nullptr;
	if( SplitNestedCCBID( nested_id, head, route ) &&
		CCBIDFromString( target_ccbid, head.c_str() ) &&
		ValidRoute( route ) )
	{
		target = GetTarget( target_ccbid );
	}
	if( !target ) {
		dprintf(D_ALWAYS,
				"CCB: old client %s requested tunneled contact %s, but its entry hop "
				"is not registered here (or the id is malformed); refusing.\n",
				request_sock->peer_description(), nested_id.c_str());
		RequestReply( request_sock, false, "no route to tunneled target", 0, target_ccbid );
		delete request_sock;
		return;
	}

		// Reverse-connect to the client ourselves, as if we were the target: dial it
		// back non-blocking so the CCB main loop is never stalled.  Completion is
		// delivered to OldClientReverseConnected, which sends the reverse-connect hello
		// and relays the socket down the tunnel.
	Daemon client( DT_ANY, client_addr.c_str() );
	CondorError errstack;
	int ccb_timeout = param_integer("CCB_TIMEOUT", 300);
	Sock *client_sock = client.makeConnectedSocket(
			Stream::reli_sock, ccb_timeout, 0, &errstack, true /*nonblocking*/ );
	if( !client_sock ) {
		dprintf(D_ALWAYS,
				"CCB: failed to initiate reverse connection to old client %s: %s\n",
				client_addr.c_str(), errstack.getFullText().c_str());
		RequestReply( request_sock, false, "failed to reverse-connect to requester",
				0, target_ccbid );
		delete request_sock;
		return;
	}

	auto ctx = std::make_shared<OldClientRelay>();
	ctx->request_sock = request_sock;
	ctx->connect_id = connect_id;
	ctx->target_ccbid = target_ccbid;
	ctx->route = route;
	ctx->original_requester = RequesterIdentity( request_sock );
	ctx->prior_hop = m_address;

	int rc = daemonCore->Register_Socket(
			client_sock, "ccb old-client reverse dial",
			[this, ctx](Stream *s){ return OldClientReverseConnected( ctx, (ReliSock *)s ); },
			"CCBServer::OldClientReverseConnected" );
	if( rc < 0 ) {
		RequestReply( request_sock, false, "failed to register reverse connection",
				0, target_ccbid );
		delete request_sock;
		delete client_sock;
		return;
	}
	dprintf(D_FULLDEBUG,
			"CCB: reverse-connecting to old client %s to relay it down the tunnel to "
			"ccbid %s (route '%s').\n", client_addr.c_str(), head.c_str(), route.c_str());
}

int
CCBServer::OldClientReverseConnected( std::shared_ptr<OldClientRelay> ctx, ReliSock *client_sock )
{
	if( client_sock ) {
		daemonCore->Cancel_Socket( client_sock );
	}
	if( !client_sock || !client_sock->is_connected() ) {
		dprintf(D_ALWAYS,"CCB: reverse connection to old client failed to connect.\n");
		RequestReply( ctx->request_sock, false, "failed to reverse-connect to requester",
				0, ctx->target_ccbid );
		delete ctx->request_sock;
		delete client_sock;   // delete of nullptr is a no-op
		return KEEP_STREAM;
	}

		// Send the reverse-connect hello (a raw command, so it looks like a normal
		// CEDAR command socket to the client).  The client's accept logic checks
		// cmd==CCB_REVERSE_CONNECT && ClaimId==connect_id, then runs its end-to-end
		// handshake over this socket -- which we relay to the real endpoint.
	client_sock->encode();
	int cmd = CCB_REVERSE_CONNECT;
	ClassAd hello;
	hello.Assign( ATTR_CLAIM_ID, ctx->connect_id );
	hello.Assign( ATTR_MY_ADDRESS, std::string("<") + m_address + ">" );
	if( !client_sock->put(cmd) || !putClassAd( client_sock, hello ) ||
		!client_sock->end_of_message() )
	{
		dprintf(D_ALWAYS,"CCB: failed to send reverse-connect hello to old client.\n");
		RequestReply( ctx->request_sock, false, "failed to send reverse-connect",
				0, ctx->target_ccbid );
		delete ctx->request_sock;
		delete client_sock;
		return KEEP_STREAM;
	}
	client_sock->isClient(false);
	client_sock->resetHeaderMD();

		// The entry hop may have deregistered during the dial; re-look it up.
	CCBTarget *target = GetTarget( ctx->target_ccbid );
	if( !target ) {
		RequestReply( ctx->request_sock, false, "tunneled target deregistered",
				0, ctx->target_ccbid );
		delete ctx->request_sock;
		delete client_sock;
		return KEEP_STREAM;
	}

		// Tell the client its request succeeded (traditional-style), then relay the
		// client-facing socket down the tunnel.  A fresh downstream connect id keys
		// the next hop's rendezvous; the client keeps using its own connect id on the
		// reverse connection we just handed it.
	RequestReply( ctx->request_sock, true, nullptr, 0, ctx->target_ccbid );
	delete ctx->request_sock;

	char *cid = Condor_Crypt_Base::randomHexKey(20);
	std::string down_connect_id = cid ? cid : "";
	free( cid );

	CCBRouteContext rctx;
	rctx.route = ctx->route;
	rctx.original_requester = ctx->original_requester;
	rctx.prior_hop = ctx->prior_hop;
	rctx.reply_to_requester = false;   // client_sock is a raw relay endpoint, not a request socket
	StartProxyRequest( client_sock, target, down_connect_id.c_str(),
			client_sock->peer_description(), rctx );
	return KEEP_STREAM;
}

int
CCBServer::HandleReverseConnect( int cmd, Stream *stream )
{
	ReliSock *sock = (ReliSock *)stream;
	ASSERT( cmd == CCB_REVERSE_CONNECT );
	sock->timeout(1);

	ClassAd hello;
	sock->decode();
	if( !getClassAd( sock, hello ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"CCB: failed to read reverse-connect hello from %s\n",
				sock->peer_description());
		return FALSE;
	}

	std::string connect_id;
	hello.LookupString( ATTR_CLAIM_ID, connect_id );

	auto it = m_proxy_sessions.find( connect_id );
	if( it == m_proxy_sessions.end() ) {
		dprintf(D_ALWAYS,
				"CCB: no pending proxy session for reverse connect from %s\n",
				sock->peer_description());
		return FALSE;
	}
	std::shared_ptr<CCBProxySession> session = it->second;
	if( session->target ) {
			// already satisfied; reject the duplicate
		return FALSE;
	}
	session->target = sock;

		// On the client-facing (outermost) hop, answer the requester before relaying;
		// on an intermediate relay hop the requester is the raw upstream pipe and the
		// outer broker already answered the client, so we skip straight to splicing.
	if( session->reply_to_requester ) {
			// Tell the requester its connection will be proxied on this socket.
		ClassAd reply;
		reply.Assign( ATTR_RESULT, true );
		reply.Assign( "ProxyMode", true );
		session->requester->encode();
		if( !putClassAd( session->requester, reply ) || !session->requester->end_of_message() ) {
			dprintf(D_ALWAYS,"CCB: failed to send proxy reply to requester %s\n",
					session->requester->peer_description());
			ccb_stats.CCBRequestsFailed += 1;
			DestroyProxySession( session.get() );
			return KEEP_STREAM; // we took ownership of sock via session->target
		}

			// Replay the target's reverse-connect hello to the requester so its
			// accept logic (cmd==CCB_REVERSE_CONNECT && ClaimId==connect_id) passes.
		session->requester->encode();
		int rcmd = CCB_REVERSE_CONNECT;
		if( !session->requester->put(rcmd) ||
			!putClassAd( session->requester, hello ) ||
			!session->requester->end_of_message() )
		{
			dprintf(D_ALWAYS,"CCB: failed to replay reverse-connect hello to requester\n");
			ccb_stats.CCBRequestsFailed += 1;
			DestroyProxySession( session.get() );
			return KEEP_STREAM;
		}
	}

		// Both ends are in hand; disable crypto, go non-blocking, and start the
		// byte relay (the tail shared with the outbound-proxy path).
	StartRelay( session );
	return KEEP_STREAM;
}

bool
CCBServer::StartRelay( std::shared_ptr<CCBProxySession> session )
{
		// Both the inbound-streaming and outbound-proxy paths hand us the connected
		// peer already in session->target (the outbound path adopts its dialed
		// socket there and clears session->connecting before calling us).
	ReliSock *target = session->target;

		// From here on, both sockets carry opaque end-to-end bytes.  Disable any
		// broker-session crypto so the relay reads the raw bytes each peer sends
		// after it adopts the connection.
	session->requester->set_crypto_key( false, NULL );
	target->set_crypto_key( false, NULL );

		// Put both sockets in non-blocking mode for the relay.
	if( !ccbSetNonBlocking(session->requester->get_file_desc()) ||
		!ccbSetNonBlocking(target->get_file_desc()) )
	{
		dprintf(D_ALWAYS,"CCB: failed to set proxy sockets non-blocking\n");
		ccb_stats.CCBRequestsFailed += 1;
		DestroyProxySession( session.get() );
		return false;
	}

		// Begin the non-blocking byte relay.  Both sockets share one handler that
		// captures a shared_ptr to the session (so the session stays alive as long
		// as either relay socket -- and thus its handler -- is registered).  The
		// handler attempts non-blocking I/O in both directions and adjusts each
		// socket's read/write interest to avoid triggering reads when we cannot
		// write.
	session->relaying = true;
	pendingHandshakeErase( session->connect_id );  // handshake done -> relaying

	StdSocketHandler relay_handler =
		[this, session](Stream *) { return ProxyRelayData( session.get() ); };

	int rc = daemonCore->Register_Socket(
		session->requester,
		"ccb proxy (requester)",
		relay_handler,
		"CCBServer::ProxyRelayData" );
	ASSERT( rc >= 0 );

	rc = daemonCore->Register_Socket(
		target,
		"ccb proxy (target)",
		relay_handler,
		"CCBServer::ProxyRelayData" );
	ASSERT( rc >= 0 );

		// The relay now owns the target socket.
	session->target = target;
	session->connecting = nullptr;

		// Start both sockets watching for readable data.  In the outbound path the
		// target arrived here from a connect-completion callback (interest was
		// HANDLE_WRITE while the connect was pending), so make its read interest
		// explicit; ProxyRelayData re-evaluates both on the first event.
	daemonCore->Set_Socket_Handler_Type( session->requester, HANDLE_READ );
	daemonCore->Set_Socket_Handler_Type( session->target, HANDLE_READ );

		// The broker's job -- connecting the two peers -- is done; the splice is
		// live.  Count it as a succeeded request and as an active relay.
	ccb_stats.CCBStreamingSessions += 1;
	ccb_stats.CCBStreamingActive += 1;
	ccb_stats.CCBRequestsSucceeded += 1;

	dprintf(D_FULLDEBUG|D_NETWORK,
			"CCB: established relay for request id %s\n",
			session->request_id.c_str());
	return true;
}

// ---- outbound-CCB / tunneling mode (CCB_OUTBOUND_PROXY) ----------------------

// Send a ClassAd {Result[, ErrorString]} reply to an outbound-proxy requester.
static bool
ccbProxyConnectReply( ReliSock *sock, bool result, char const *errmsg )
{
	ClassAd reply;
	reply.Assign( ATTR_RESULT, result );
	if( errmsg ) {
		reply.Assign( ATTR_ERROR_STRING, errmsg );
	}
	sock->encode();
	return putClassAd( sock, reply ) && sock->end_of_message();
}

	// Does host/addr match any entry in `list` (a space/comma-separated set)?  An
	// IP target matches a CIDR/network entry; any target matches a hostname entry by
	// case-insensitive wildcard via matches_anycase_withwildcard -- the same matcher
	// HTCondor's ALLOW/DENY host authorization uses, so "*.example.com" works.
static bool
ccbTargetMatchesList( const std::string &list, char const *host,
                      const condor_sockaddr &addr, bool is_ip )
{
	for( const auto &entry : StringTokenIterator( list ) ) {
		if( is_ip ) {
			condor_netaddr net;
			if( net.from_net_string( entry.c_str() ) && net.match( addr ) ) {
				return true;
			}
		}
		if( matches_anycase_withwildcard( entry.c_str(), host ) ) {
			return true;
		}
	}
	return false;
}

bool
CCBServer::OutboundTargetAllowed( const std::string &target )
{
	Sinful s( target.c_str() );
	char const *host = s.getHost();
	if( !s.valid() || !host ) {
		return false;
	}

	condor_sockaddr addr;
	bool is_ip = addr.from_ip_string( host );

		// Deny-list wins: a target matching CCB_OUTBOUND_TARGET_DENYLIST is refused
		// even when the allow-list would permit it.  Its default is the
		// loopback/link-local set, so by default the broker refuses to be used to
		// reach services bound to its own host (an SSRF guard) -- but that refusal is
		// now a visible, overridable config value rather than hard-coded.
	std::string denylist;
	if( param( denylist, "CCB_OUTBOUND_TARGET_DENYLIST" ) && !denylist.empty() &&
		ccbTargetMatchesList( denylist, host, addr, is_ip ) )
	{
		return false;
	}

		// Allow-list: its default is "*" (permit everything not denied above); an
		// empty list permits nothing.
	std::string allowlist;
	if( !param( allowlist, "CCB_OUTBOUND_TARGET_ALLOWLIST" ) ) {
		return false;
	}
	return ccbTargetMatchesList( allowlist, host, addr, is_ip );
}

int
CCBServer::HandleProxyConnect( int cmd, Stream *stream )
{
	ASSERT( cmd == CCB_PROXY_CONNECT );
	ReliSock *sock = (ReliSock *)stream;
	sock->timeout(1);

	ccb_stats.CCBOutboundRequests += 1;

	ClassAd msg;
	sock->decode();
	if( !getClassAd( sock, msg ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,"CCB: failed to read proxy-connect request from %s\n",
				sock->peer_description());
		return FALSE;
	}

	std::string target, connect_id;
	msg.LookupString( ATTR_MY_ADDRESS, target );
	msg.LookupString( ATTR_CLAIM_ID, connect_id );
		// Decrementing TTL (like TCP): the originator set it; we only decrement and
		// refuse at 0 when forwarding, so the chain length is bounded by the endpoint
		// rather than any intermediate broker's own maximum.  Default to the local
		// originator TTL if the request carries none.
	int ttl = param_integer("CCB_OUTBOUND_TTL", 8, 1);
	msg.LookupInteger( ATTR_CCB_TTL, ttl );
	if( target.empty() || connect_id.empty() ) {
		ccbProxyConnectReply( sock, false, "missing target address or connect id" );
		return FALSE;
	}

	if( !OutboundTargetAllowed( target ) ) {
		dprintf(D_ALWAYS,
				"CCB: refusing proxy-connect from %s to disallowed target %s\n",
				sock->peer_description(), target.c_str());
		ccbProxyConnectReply( sock, false, "target not permitted by this broker" );
		return FALSE;
	}

	StartOutgoingProxyRequest( sock, target, connect_id, ttl );
	return KEEP_STREAM;   // we took ownership of the requester socket
}

void
CCBServer::StartOutgoingProxyRequest( ReliSock *requester,
		const std::string &target, const std::string &connect_id,
		int ttl )
{
		// The request itself is counted in HandleProxyConnect (CCBOutboundRequests);
		// here we only track the outbound-specific outcomes below.

		// Same caps as the inbound streaming path (shared session tables).
	int max_sessions = param_integer("CCB_SERVER_MAX_STREAMING_SESSIONS", 0, 0);
	if( max_sessions > 0 && (int)m_proxy_sessions.size() >= max_sessions ) {
		dprintf(D_ALWAYS,
				"CCB: refusing proxy-connect from %s: at the session limit "
				"(CCB_SERVER_MAX_STREAMING_SESSIONS=%d).\n",
				requester->peer_description(), max_sessions);
		ccbProxyConnectReply( requester, false, "CCB session limit reached on the broker" );
		ccb_stats.CCBRequestsFailed += 1;
		delete requester;
		return;
	}
	int max_handshakes = param_integer("CCB_SERVER_MAX_STREAMING_HANDSHAKES", 0, 0);
	if( max_handshakes > 0 && (int)m_pending_handshakes.size() >= max_handshakes ) {
		dprintf(D_ALWAYS,
				"CCB: refusing proxy-connect from %s: at the handshake limit "
				"(CCB_SERVER_MAX_STREAMING_HANDSHAKES=%d).\n",
				requester->peer_description(), max_handshakes);
		ccbProxyConnectReply( requester, false, "CCB handshake limit reached on the broker" );
		ccb_stats.CCBRequestsFailed += 1;
		delete requester;
		return;
	}

	auto session = std::make_shared<CCBProxySession>();
	session->connect_id = connect_id;
	session->requester = requester;
	session->created = time(nullptr);  // handshake deadline is relative to this
	CCBIDToString( m_next_request_id++, session->request_id );

	if( m_proxy_sessions.find(session->connect_id) != m_proxy_sessions.end() ) {
		ccbProxyConnectReply( requester, false, "duplicate connect id" );
		ccb_stats.CCBRequestsFailed += 1;
		delete requester;
		return;
	}
	m_proxy_sessions[session->connect_id] = session;
	pendingHandshakeInsert( session->connect_id );  // pending until relaying

		// Obtain a connection to the target on the requester's behalf, non-blocking
		// so the CCB main loop is never stalled.  Either we are the exit point and
		// dial the target directly, or -- if a next-hop broker is configured -- we
		// forward the proxy request to it (which dials the target, or forwards
		// again), composing the outbound tunnel across brokers.  Either way `out`
		// becomes a transparent pipe to the target and completion is delivered to
		// OutgoingConnectComplete.
	ReliSock *out = new ReliSock;
	int rc;
	std::string next_hop;
	bool forward = param( next_hop, "CCB_OUTBOUND_NEXT_HOP" ) && !next_hop.empty();
	if( forward && ttl <= 0 ) {
			// The originator's TTL is exhausted: forwarding again would exceed the
			// chain length it allowed (or indicates a next-hop loop).  Refuse.
		dprintf(D_ALWAYS,
				"CCB: refusing to forward proxy-connect from %s to %s: outbound TTL "
				"expired (possible next-hop loop).\n",
				requester->peer_description(), target.c_str());
		delete out;
		ccbProxyConnectReply( requester, false, "outbound proxy TTL expired" );
		ccb_stats.CCBRequestsFailed += 1;
		ccb_stats.CCBOutboundFailed += 1;
		DestroyProxySession( session.get() );
		return;
	}
	if( forward ) {
			// Reach the target through the next-hop broker using the outbound-proxy
			// client exchange; on success it adopts the spliced pipe into `out`.
			// Pass the decremented TTL so the next hop refuses once the originator's
			// budget runs out.
		ccb_stats.CCBOutboundForwarded += 1;
		rc = out->do_outbound_ccb_connect( next_hop.c_str(), target.c_str(),
				true /*non_blocking*/, NULL, ttl - 1 );
	} else {
			// Dial directly.  The standard connect path (so the target may itself be
			// direct / shared-port / CCB-routed) but bypass any outbound-CCB
			// interception on this broker -- we are the exit point.
		out->set_bypass_outbound_ccb( true );
		rc = out->connect( target.c_str(), 0, true /*non_blocking*/ );
	}

	if( rc == CEDAR_EWOULDBLOCK ) {
			// The dial is in progress; finish in OutgoingConnectComplete.
		session->connecting = out;
		int reg = daemonCore->Register_Socket(
			out,
			"ccb outbound proxy dial",
			[this, session](Stream *s){ return OutgoingConnectComplete( session, (ReliSock *)s ); },
			"CCBServer::OutgoingConnectComplete" );
		ASSERT( reg >= 0 );

		dprintf(D_FULLDEBUG,
				"CCB: %s target %s for outbound proxy request id %s\n",
				forward ? "forwarding to next hop for" : "dialing",
				target.c_str(), session->request_id.c_str());
		return;
	}

	if( rc == TRUE ) {
			// The direct dial completed synchronously (a fast local target can
			// connect immediately even in non-blocking mode).  Adopt `out` and relay
			// now, mirroring OutgoingConnectComplete's success tail -- but with no
			// Cancel_Socket, since `out` was never registered for a connect callback.
			// (The forward path never returns TRUE; it is inherently asynchronous.)
		session->target = out;
		if( !ccbProxyConnectReply( session->requester, true, nullptr ) ) {
			dprintf(D_ALWAYS,"CCB: failed to send proxy-connect reply to requester %s\n",
					session->requester->peer_description());
			ccb_stats.CCBRequestsFailed += 1;
			DestroyProxySession( session.get() );
			return;
		}
		dprintf(D_FULLDEBUG,
				"CCB: dialed target %s (immediately) for outbound proxy request id %s\n",
				target.c_str(), session->request_id.c_str());
		StartRelay( session );
		return;
	}

		// rc == FALSE: immediate failure to even start the dial.
	dprintf(D_ALWAYS,"CCB: outbound proxy connection to %s failed to start\n",
			target.c_str());
	delete out;
	ccbProxyConnectReply( requester, false, "outbound connection to target failed" );
	ccb_stats.CCBRequestsFailed += 1;
	ccb_stats.CCBOutboundFailed += 1;
	DestroyProxySession( session.get() );
	return;
}

int
CCBServer::OutgoingConnectComplete( std::shared_ptr<CCBProxySession> session, ReliSock *out )
{
		// DaemonCore drives do_connect_finish() to completion before invoking us
		// (see the is_connect_pending path in DaemonCore::Driver), so the dial has
		// resolved -- we only check the outcome.
	if( !out->is_connected() ) {
		dprintf(D_ALWAYS,"CCB: outbound proxy dial to %s failed\n",
				out->peer_description());
		ccbProxyConnectReply( session->requester, false, "outbound connection to target failed" );
		ccb_stats.CCBRequestsFailed += 1;
		ccb_stats.CCBOutboundFailed += 1;
			// DestroyProxySession cancels and frees `out` via session->connecting.
		DestroyProxySession( session.get() );
		return KEEP_STREAM;
	}

		// Connected.  Detach `out` from its connect-completion registration and
		// adopt it as the target.  Cancelling + letting StartRelay re-register it is
		// required to clear DaemonCore's connect-pending state on the socket (a
		// lingering connect-pending entry keeps DaemonCore driving do_connect_finish
		// and the relay would send to a socket it still thinks is connecting).
	daemonCore->Cancel_Socket( out );
	session->connecting = nullptr;
	session->target = out;

		// Reply {Result:true} to the requester BEFORE StartRelay disables crypto,
		// matching the ordering of the inbound streaming reply.
	if( !ccbProxyConnectReply( session->requester, true, nullptr ) ) {
		dprintf(D_ALWAYS,"CCB: failed to send proxy-connect reply to requester %s\n",
				session->requester->peer_description());
		ccb_stats.CCBRequestsFailed += 1;
		DestroyProxySession( session.get() );
		return KEEP_STREAM;
	}

	StartRelay( session );
	return KEEP_STREAM;
}

#ifndef SHUT_WR
#define SHUT_WR SD_SEND   // POSIX name for Winsock's "stop sending" shutdown
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH // POSIX name for Winsock's "stop both" shutdown
#endif

// ccbSetNonBlocking puts a socket fd into non-blocking mode.
static bool ccbSetNonBlocking( int fd )
{
#ifdef WIN32
	unsigned long mode = 1;
	return ioctlsocket( (SOCKET)fd, FIONBIO, &mode ) == 0;
#else
	int flags = fcntl( fd, F_GETFL, 0 );
	if( flags < 0 ) {
		return false;
	}
	return fcntl( fd, F_SETFL, flags | O_NONBLOCK ) == 0;
#endif
}

// ccbHandlerType maps desired read/write interest to a daemonCore HandlerType.
static HandlerType ccbHandlerType( bool want_read, bool want_write )
{
	if( want_read && want_write ) return HANDLE_READ_WRITE;
	if( want_read )               return HANDLE_READ;
	if( want_write )              return HANDLE_WRITE;
	return HANDLE_NONE;
}

// The relay does raw non-blocking recv()/send(), so it must classify socket
// errors portably: on Windows they live in WSAGetLastError() (not errno) and use
// WSAE* names.  Capture the error immediately after the failing call.
static int ccbSockErrno()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}
static bool ccbWouldBlock( int e )
{
#ifdef WIN32
	return e == WSAEWOULDBLOCK;
#else
	return e == EAGAIN || e == EWOULDBLOCK;
#endif
}
static bool ccbInterrupted( int e )
{
#ifdef WIN32
	return e == WSAEINTR;
#else
	return e == EINTR;
#endif
}

// ccbFlush writes as much of a direction's buffer to the destination fd as it
// can without blocking.  Returns false on a hard (non-retryable) error.
static bool ccbFlush( CCBProxySession::Dir &d, int dstfd )
{
	while( d.hasData() ) {
		ssize_t w = send( dstfd, d.buf.data() + d.off, d.len - d.off, 0 );
		if( w > 0 ) {
			d.off += (int)w;
		}
		else {
			int e = ccbSockErrno();
			if( w < 0 && ccbWouldBlock(e) ) {
				break; // destination not ready; keep the remainder buffered
			}
			else if( w < 0 && ccbInterrupted(e) ) {
				continue;
			}
			else {
				return false; // hard error (e.g. EPIPE)
			}
		}
	}
	if( !d.hasData() ) {
		d.clear();
	}
	return true;
}

// ccbPumpDirection runs one direction of the relay: flush any buffered bytes to
// the destination, then -- while our small userspace buffer is empty -- read
// from the source and push it toward the destination, looping so that one
// wakeup drains everything the kernel currently has rather than a single chunk.
// We stop reading the source while the buffer holds unflushed bytes: the
// source's data then backs up in the kernel socket buffer and, beyond that,
// throttles the real sender via TCP.  The two directions are independent, and
// the relay only adds buffering to the path, so it cannot deadlock a stream that
// a direct connection would not.  Once the source has closed and the buffer is
// drained, the destination's write side is shut down to propagate EOF.  Returns
// false on a hard error.
static bool ccbPumpDirection( CCBProxySession::Dir &d, ReliSock *src, ReliSock *dst, const char *label )
{
	int srcfd = src->get_file_desc();
	int dstfd = dst->get_file_desc();

	if( !ccbFlush( d, dstfd ) ) {
		dprintf(D_NETWORK,"CCB RELAY[%s]: flush hard error (errno=%d)\n",label,ccbSockErrno());
		return false;
	}

	while( !d.src_eof && !d.hasData() ) {
		if( d.buf.empty() ) { d.buf.resize( CCBProxySession::RELAY_BUF_START ); }
		int cap = (int)d.buf.size();
		ssize_t n = recv( srcfd, d.buf.data(), cap, 0 );
		if( n > 0 ) {
			ccb_stats.CCBStreamingBytes += (int64_t)n;
			d.len = (int)n;
			d.off = 0;
			if( !ccbFlush( d, dstfd ) ) {
				dprintf(D_NETWORK,"CCB RELAY[%s]: post-recv flush hard error (errno=%d)\n",label,ccbSockErrno());
				return false;
			}
			// We filled the buffer and drained it, so the source likely has more
			// queued; grow (up to the cap) so the next read drains more per
			// syscall.  Idle/light sessions never fill it and stay small.
			if( n == cap && !d.hasData() && cap < CCBProxySession::RELAY_BUF_MAX ) {
				int next = cap * 2;
				d.buf.resize( next < CCBProxySession::RELAY_BUF_MAX ? next : CCBProxySession::RELAY_BUF_MAX );
			}
		}
		else if( n == 0 ) {
			d.src_eof = true;
			break;
		}
		else {
			int e = ccbSockErrno();
			if( ccbWouldBlock(e) ) {
				break; // source drained for now
			}
			else if( ccbInterrupted(e) ) {
				continue;
			}
			else {
				return false; // hard error
			}
		}
	}

	if( d.src_eof && !d.hasData() && !d.dst_shutdown ) {
		shutdown( dstfd, SHUT_WR );
		d.dst_shutdown = true;
		dprintf(D_NETWORK,"CCB RELAY[%s]: source EOF, shut down dst fd=%d write side\n",label,dstfd);
	}
	return true;
}

int
CCBServer::ProxyRelayData( CCBProxySession *s )
{
	if( !s ) {
		return KEEP_STREAM;
	}
	if( s->dying ) {
		// The session is already being torn down: the other socket's callback (or
		// this socket's own teardown) flipped it dying and woke us only so that we
		// retire this socket.  Do not touch the splice; returning non-KEEP_STREAM
		// makes DaemonCore cancel+delete this socket, which drops this handler's
		// shared_ptr and frees the session once the other socket is gone too.
		return FALSE;
	}

	// Non-blocking I/O makes it safe to attempt both directions on every
	// callback regardless of which socket woke us; an unready socket simply
	// returns EAGAIN.
	if( !ccbPumpDirection( s->a_to_b, s->requester, s->target, "req->tgt" ) ||
		!ccbPumpDirection( s->b_to_a, s->target, s->requester, "tgt->req" ) )
	{
		// A hard send/recv error after the splice was established (a clean
		// shutdown / EOF does not get here -- it sets src_eof and is handled
		// below as an orderly close).
		ccb_stats.CCBStreamingFailed += 1;
		dprintf(D_NETWORK,"CCB RELAY: tearing down session (request id %s) on pump error\n",
				s->request_id.c_str());
		BeginRelayTeardown( s );
		return FALSE;
	}

	// Both directions fully closed: tear down.
	if( s->a_to_b.dst_shutdown && s->b_to_a.dst_shutdown ) {
		BeginRelayTeardown( s );
		return FALSE;
	}

	// Re-evaluate each socket's read/write interest.  A socket is the source of
	// one direction and the destination of the other:
	//   - read it while its outbound buffer is empty (once it holds unflushed
	//     bytes we stop reading and let the source back up in the kernel)
	//   - write it while the other direction has buffered bytes for it
	HandlerType ht_req = ccbHandlerType(
		!s->a_to_b.hasData() && !s->a_to_b.src_eof,
		s->b_to_a.hasData() );
	HandlerType ht_tgt = ccbHandlerType(
		!s->b_to_a.hasData() && !s->b_to_a.src_eof,
		s->a_to_b.hasData() );
	daemonCore->Set_Socket_Handler_Type( s->requester, ht_req );
	daemonCore->Set_Socket_Handler_Type( s->target, ht_tgt );

	return KEEP_STREAM;
}

void
CCBServer::SweepProxySessions()
{
		// Reap streaming (proxy) sessions whose handshake never completed: the
		// target was asked to connect back to be spliced but never did, so the
		// requester's socket would otherwise be pinned indefinitely.
	int handshake_timeout =
		param_integer("CCB_SERVER_STREAMING_HANDSHAKE_TIMEOUT", 300, 0);
	if( handshake_timeout <= 0 ) {
		return;
	}
	time_t now = time(nullptr);
	auto it = m_proxy_sessions.begin();
	while( it != m_proxy_sessions.end() ) {
			// DestroyProxySession erases the current element, so advance first.
		auto next_it = it;
		++next_it;
		CCBProxySession *session = it->second.get();
		if( !session->relaying && now - session->created >= handshake_timeout ) {
			dprintf(D_ALWAYS,
					"CCB: streaming (proxy) handshake for request id %s timed out "
					"after %d seconds; the target never connected back to be "
					"relayed.\n",
					session->request_id.c_str(), handshake_timeout);
			RequestReply( session->requester, false,
					"CCB streaming handshake timed out: the target did not "
					"connect back to the broker", 0, 0 );
			ccb_stats.CCBRequestsFailed += 1;
			DestroyProxySession( session );
		}
		it = next_it;
	}
}

void
CCBServer::DestroyProxySession( CCBProxySession *session )
{
	if( !session ) {
		return;
	}
		// This path is only for a session that has not begun relaying (it is still
		// in the handshake).  No relay sockets are registered with DaemonCore yet,
		// so there is no sibling callback to race and everything can be freed
		// synchronously.  A relaying session is torn down via BeginRelayTeardown,
		// which lets each socket retire itself from its own callback.
	ASSERT( !session->relaying );

		// We still own these sockets (DaemonCore only takes ownership once they are
		// registered for relaying), so delete them here.  delete on nullptr is fine.
		// A `connecting` socket (outbound proxy, dial still in progress) may be
		// registered with DaemonCore for the connect callback; cancel it first.
		// If the dial is a forward through a next-hop broker, `connecting` also holds
		// an in-flight CCBClient (reverse-connect pending state); deleting it runs
		// ~ReliSock -> Sock::close(), which cancels that CCBClient and nulls its
		// back-pointer, so no later broker callback can touch this freed socket.
	if( session->connecting ) {
		daemonCore->Cancel_Socket( session->connecting );
		delete session->connecting;
		session->connecting = nullptr;
	}
	delete session->requester;
	delete session->target;
	session->requester = nullptr;
	session->target = nullptr;

		// Dropping the m_proxy_sessions entry releases the only shared_ptr to a
		// pre-relaying session (no relay handler holds one yet), so this frees it.
	pendingHandshakeErase( session->connect_id );
	auto it = m_proxy_sessions.find( session->connect_id );
	if( it != m_proxy_sessions.end() && it->second.get() == session ) {
		m_proxy_sessions.erase( it );
	}
}

void
CCBServer::BeginRelayTeardown( CCBProxySession *session )
{
	if( !session || session->dying ) {
		return;  // idempotent: the other socket may have started this already
	}
	session->dying = true;
	ccb_stats.CCBStreamingActive -= 1;

		// Wake both relay sockets so that each retires itself from its own callback
		// -- the only context in which DaemonCore can safely cancel and delete a
		// socket that may currently be in service.  shutdown() forces an EOF/error
		// wakeup even on a socket with no pending I/O, and selecting it for read
		// guarantees it lands in the next select() set.  Each woken callback returns
		// non-KEEP_STREAM, so DaemonCore cancels+deletes its socket and destroys its
		// copy of the relay handler; the session is freed once the last such handler
		// (its shared_ptr) is gone.
	if( session->requester ) {
		daemonCore->Set_Socket_Handler_Type( session->requester, HANDLE_READ );
		shutdown( session->requester->get_file_desc(), SHUT_RDWR );
	}
	if( session->target ) {
		daemonCore->Set_Socket_Handler_Type( session->target, HANDLE_READ );
		shutdown( session->target->get_file_desc(), SHUT_RDWR );
	}

		// Detach from the lookup tables so no new request can match this session and
		// so the shared_ptr the map held is released (the relay handlers keep the
		// session alive until their sockets are gone).  Do this last: the shutdown
		// calls above still need session->requester / session->target.
	pendingHandshakeErase( session->connect_id );  // a no-op once relaying
	auto it = m_proxy_sessions.find( session->connect_id );
	if( it != m_proxy_sessions.end() && it->second.get() == session ) {
		m_proxy_sessions.erase( it );
	}
}

CCBServerRequest *
CCBServer::GetRequest( CCBID request_id )
{
	auto requestsit = m_requests.find(request_id);
	if (requestsit == m_requests.end()) {
		return nullptr;
	}
	return requestsit->second;
}

CCBTarget *
CCBServer::GetTarget( CCBID ccbid )
{
	auto targetsit = m_targets.find(ccbid);
	if (targetsit == m_targets.end()) {
		return nullptr;
	}
	return targetsit->second;
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
		// The reconnect request is coming from a different IP address.
		// Check if this is allowed.
		if ( m_reconnect_allowed_from_any_ip == false ) {
			dprintf(D_ALWAYS,
				"CCB: reconnect request from target daemon %s with ccbid %lu "
				"has wrong IP! (expected IP=%s)  - request denied\n",
				target->getSock()->peer_description(),
				target->getCCBID(),
				previous_ip);
			return false;  // return false now to deny the reconnect
		} else {
			dprintf(D_FULLDEBUG,
				"CCB: reconnect request from target daemon %s with ccbid %lu "
				"moved from previous_ip=%s to new_ip=%s\n",
				target->getSock()->peer_description(),
				target->getCCBID(),
				previous_ip, new_ip);
		}
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

	auto targetsit = m_targets.find(target->getCCBID());
	if (targetsit != m_targets.end()) {
		// perhaps we haven't noticed yet that this existing target socket
		// has become disconnected; get rid of it
		dprintf(D_ALWAYS,
				"CCB: disconnecting existing connection from target daemon "
				"%s with ccbid %lu because this daemon is reconnecting.\n",
				targetsit->second->getSock()->peer_description(),
				target->getCCBID());
		RemoveTarget( targetsit->second);
	}

	m_targets.emplace(target->getCCBID(),target);
	EpollAdd(target);

	ccb_stats.CCBEndpointsConnected += 1;
	ccb_stats.CCBReconnects += 1;

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

		if (!m_targets.contains(target->getCCBID())) {
			m_targets.emplace(target->getCCBID(), target);
			EpollAdd(target);
			break; // success
		}

		// else this ccbid is already taken, so try again
	}

	// generate reconnect info for this new target daemon so that it
	// can reclaim its CCBID
	CCBID reconnect_cookie = get_csrng_uint();
	CCBReconnectInfo *reconnect_info = new CCBReconnectInfo(
		target->getCCBID(),
		reconnect_cookie,
		target->getSock()->peer_ip_str());
	AddReconnectInfo( reconnect_info );
	SaveReconnectInfo( reconnect_info );

	ccb_stats.CCBEndpointsConnected += 1;

	dprintf(D_FULLDEBUG,"CCB: registered target daemon %s with ccbid %lu\n",
			target->getSock()->peer_description(),
			target->getCCBID());
}

void
CCBServer::RemoveTarget( CCBTarget *target )
{
		// hang up on all requests for this target
	std::map<CCBID,CCBServerRequest *> *trequests;
	while ((trequests = target->getRequests())) {
		auto requestit = trequests->begin();
		if (requestit != trequests->end()) {
			CCBServerRequest *request = requestit->second;
			RemoveRequest( request );
			ccb_stats.CCBRequestsFailed += 1;
			// note that trequests may point to a deleted hash table
			// at this point, so do not reference it anymore
		}
		else {
			break;
		}
	}

	m_targets.erase(target->getCCBID());
	EpollRemove(target);

	ccb_stats.CCBEndpointsConnected -= 1;

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

		auto resultpair = m_requests.emplace(request->getRequestID(), request);
		if (resultpair.second) {
			// If we did insert
			break; // success
		}
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

	ccb_stats.CCBRequests += 1;
}

int
CCBServer::HandleRequestDisconnect( Stream * /*stream*/ )
{
	CCBServerRequest *request = (CCBServerRequest *)daemonCore->GetDataPtr();
	RemoveRequest( request );
	ccb_stats.CCBRequestsSucceeded += 1;
	return KEEP_STREAM;
}


void
CCBServer::RemoveRequest( CCBServerRequest *request )
{
	daemonCore->Cancel_Socket( request->getSock() );

	m_requests.erase(request->getRequestID());

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
		m_requests = new std::map<CCBID,CCBServerRequest *>;
		ASSERT( m_requests );
	}
	m_requests->emplace(request->getRequestID(),request);
}

void
CCBTarget::RemoveRequest(CCBServerRequest *request)
{
	if( m_requests ) {
		m_requests->erase(request->getRequestID());

		if( m_requests->size() == 0 ) {
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
	auto it = m_reconnect_info.find(ccbid);
	if (it == m_reconnect_info.end()) {
		return nullptr;
	}
	return it->second;
}

void
CCBServer::AddReconnectInfo( CCBReconnectInfo *reconnect_info )
{
	auto resultpair = m_reconnect_info.emplace(reconnect_info->getCCBID(), reconnect_info);
	if (resultpair.second) {
		// If we did insert
		ccb_stats.CCBEndpointsRegistered += 1;
		return;
	}

	dprintf(D_ALWAYS, "CCBServer::AddReconnectInfo(): Found stale reconnect entry!\n");
	delete resultpair.first->second; // 1st is iterator, 2nd is the reconnect_info
	m_reconnect_info.erase(reconnect_info->getCCBID());
	m_reconnect_info.emplace(reconnect_info->getCCBID(),reconnect_info);
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
	if( m_reconnect_fname.empty() ) {
		return false;
	}
	if( !only_if_exists ) {
		m_reconnect_fp = safe_fcreate_fail_if_exists(m_reconnect_fname.c_str(),"w+",0600);
	}
	if( !m_reconnect_fp ) {
		m_reconnect_fp = safe_fopen_no_create(m_reconnect_fname.c_str(),"r+");
	}
	if( !m_reconnect_fp ) {
		if( only_if_exists && errno == ENOENT ) {
			return false;
		}
		EXCEPT("CCB: Failed to open %s: %s",
			   m_reconnect_fname.c_str(),strerror(errno));
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
					m_reconnect_fname.c_str());
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

	dprintf(D_ALWAYS,"CCB: loaded %zu reconnect records from %s.\n",
			m_reconnect_info.size(), m_reconnect_fname.c_str());
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
				m_reconnect_fname.c_str(), strerror(errno));
		return false;
	}

	std::string ccbid_str,cookie_str;
	rc = fprintf(m_reconnect_fp,"%s %s %s\n",
		reconnect_info->getPeerIP(),
		CCBIDToString(reconnect_info->getCCBID(),ccbid_str),
		CCBIDToString(reconnect_info->getReconnectCookie(),cookie_str));
	if( rc == -1 ) {
		dprintf(D_ALWAYS,"CCB: failed to write reconnect info in %s: %s\n",
				m_reconnect_fname.c_str(), strerror(errno));
		return false;
	}
	return true;
}

void
CCBServer::SaveAllReconnectInfo()
{
	if( m_reconnect_fname.empty() ) {
		return;
	}
	CloseReconnectFile();

	if( m_reconnect_info.size()==0 ) {
		IGNORE_RETURN remove( m_reconnect_fname.c_str() );
		return;
	}

	std::string orig_reconnect_fname = m_reconnect_fname;
	formatstr_cat(m_reconnect_fname, ".new");

	if( !OpenReconnectFile() ) {
		m_reconnect_fname = orig_reconnect_fname;
		return;
	}

	for (const auto &[ccbid, reconnect_info]: m_reconnect_info) {
		if( !SaveReconnectInfo(reconnect_info) ) {
			CloseReconnectFile();
			m_reconnect_fname = orig_reconnect_fname;
			dprintf(D_ALWAYS,"CCB: aborting rewriting of %s\n",
					m_reconnect_fname.c_str());
			return;
		}
	}

	CloseReconnectFile();
	int rc;
	rc = rotate_file( m_reconnect_fname.c_str(),orig_reconnect_fname.c_str() );
	if( rc < 0 ) {
		dprintf(D_ALWAYS,"CCB: failed to rotate rewritten %s\n",
				m_reconnect_fname.c_str());
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
	for (const auto &[ccbid, target]: m_targets) {
		CCBReconnectInfo *reconnect_info = GetReconnectInfo(target->getCCBID());
		ASSERT( reconnect_info );
		reconnect_info->alive();
	}

	unsigned long removed = 0;
	auto it = m_reconnect_info.begin();
	while (it != m_reconnect_info.end()) {
		time_t last = it->second->getLastAlive();
		if( now - last > 2 * m_reconnect_info_sweep_interval ) {
			delete it->second;
			it = m_reconnect_info.erase(it);
			ccb_stats.CCBEndpointsRegistered -= 1;
			removed++;
		} else {
			it++;
		}
	}

	if( removed ) {
		dprintf(D_ALWAYS,
				"CCB: pruning %lu expired reconnect records.\n",removed);

		// rewrite the file to save space, since some records were deleted
		SaveAllReconnectInfo();
	}
}
