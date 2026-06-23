/***************************************************************************
 *
 * Benchmark for CCB streaming (proxy) mode.
 *
 * Establishes a single streamed connection through a running CCB broker
 * (a condor_collector with CCB enabled), exactly as a private-to-private
 * pair would: a "target" registers with the broker, a "requester" sends a
 * CCB_REQUEST with CCBStreamingRequired set, the target reverse-connects to
 * the broker, and the broker splices the two sockets.  Once the splice is up,
 * both ends send raw bytes in both directions for a fixed interval and the
 * program prints the average transfer rate each way -- i.e. the throughput of
 * the broker's relay itself.
 *
 * Usage: ccb_proxy_bench <broker-sinful> [seconds]
 *
 ***************************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "condor_crypt.h"
#include "daemon.h"
#include "reli_sock.h"
#include "classad_oldnew.h"
#include "condor_classad.h"

#ifdef WIN32

// This benchmark drives the relay with fork()/poll(), which have no Windows
// equivalent here; build a stub so the test exe still links.
int main( int, char ** )
{
	fprintf(stderr, "ccb_proxy_bench is not supported on this platform.\n");
	return 1;
}

#else

#include <poll.h>
#include <sys/wait.h>
#include <chrono>
#include <memory>
#include <vector>

static const int BLAST_BUF = 256 * 1024;

// Run a bidirectional blast on fd for `seconds`: keep the outbound direction
// full and drain the inbound direction, returning the inbound throughput in
// bytes/sec (i.e. the rate at which the peer's data arrives).
static double blast( int fd, int seconds )
{
	int flags = fcntl(fd, F_GETFL, 0);
	if( flags >= 0 ) { fcntl(fd, F_SETFL, flags | O_NONBLOCK); }

	std::vector<char> out(BLAST_BUF, 'x');
	std::vector<char> in(BLAST_BUF);
	uint64_t received = 0;

	auto start = std::chrono::steady_clock::now();
	auto deadline = start + std::chrono::seconds(seconds);

	while( std::chrono::steady_clock::now() < deadline ) {
		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLIN | POLLOUT;
		pfd.revents = 0;
		int rc = poll(&pfd, 1, 100);
		if( rc < 0 ) { if( errno == EINTR ) { continue; } break; }
		if( pfd.revents & (POLLERR|POLLNVAL) ) { break; }
		if( pfd.revents & POLLOUT ) {
			ssize_t n = send(fd, out.data(), out.size(), 0);
			if( n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) { break; }
		}
		if( pfd.revents & POLLIN ) {
			ssize_t n = recv(fd, in.data(), in.size(), 0);
			if( n > 0 ) { received += (uint64_t)n; }
			else if( n == 0 ) { break; }
			else if( errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) { break; }
		}
	}
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	return secs > 0 ? (double)received / secs : 0.0;
}

// targetRole: register with the broker, wait for the forwarded request, reverse-
// connect to the broker, and blast.  Reports its ccbid and, after the blast, the
// requester->target rate (the inbound rate it measured) to the parent over the
// pipe.  Exits non-zero on failure (which the parent sees as EOF on the pipe).
static int targetRole( const char *broker, int seconds, FILE *to_parent )
{
	Daemon ccb(DT_COLLECTOR, broker, NULL);
	Sock *reg = ccb.startCommand(CCB_REGISTER, Stream::reli_sock, 20, NULL);
	if( !reg ) { fprintf(stderr, "target: startCommand(CCB_REGISTER) failed\n"); return 1; }

	ClassAd ad;
	ad.Assign(ATTR_COMMAND, CCB_REGISTER);
	ad.Assign(ATTR_NAME, "ccb_proxy_bench-target");
	reg->encode();
	if( !putClassAd(reg, ad) || !reg->end_of_message() ) {
		fprintf(stderr, "target: failed to send registration\n"); return 1;
	}
	ClassAd reply;
	reg->decode();
	if( !getClassAd(reg, reply) || !reg->end_of_message() ) {
		fprintf(stderr, "target: failed to read registration reply\n"); return 1;
	}
	std::string contact;
	reply.LookupString(ATTR_CCBID, contact);
	std::string ccbid = contact;            // contact is "<broker-sinful>#<id>"
	size_t hash = contact.rfind('#');
	if( hash != std::string::npos ) { ccbid = contact.substr(hash+1); }

	// Tell the requester (parent) the ccbid so it can request us.
	fprintf(to_parent, "%s\n", ccbid.c_str());
	fflush(to_parent);

	// Wait for the broker to forward the request, then reverse-connect.
	ClassAd fwd;
	reg->decode();
	if( !getClassAd(reg, fwd) || !reg->end_of_message() ) {
		fprintf(stderr, "target: failed to read forwarded request\n"); return 1;
	}
	std::string proxy_addr, cid, rid;
	fwd.LookupString(ATTR_MY_ADDRESS, proxy_addr);
	fwd.LookupString(ATTR_CLAIM_ID, cid);
	fwd.LookupString(ATTR_REQUEST_ID, rid);

	ReliSock rc;
	if( !rc.connect(proxy_addr.c_str(), 0) ) {
		fprintf(stderr, "target: reverse-connect to %s failed\n", proxy_addr.c_str()); return 1;
	}
	ClassAd hello;
	hello.Assign(ATTR_CLAIM_ID, cid);
	hello.Assign(ATTR_REQUEST_ID, rid);
	hello.Assign(ATTR_MY_ADDRESS, proxy_addr);
	rc.encode();
	int cmd = CCB_REVERSE_CONNECT;
	if( !rc.put(cmd) || !putClassAd(&rc, hello) || !rc.end_of_message() ) {
		fprintf(stderr, "target: failed to send reverse-connect hello\n"); return 1;
	}

	double rate = blast(rc.get_file_desc(), seconds);   // requester -> target
	fprintf(to_parent, "%.6f\n", rate);
	fflush(to_parent);
	return 0;
}

// requesterRole: stream-connect to the target (via the broker), blast, and print
// the throughput each direction.
static int requesterRole( const char *broker, int seconds, FILE *from_target )
{
	// First line from the target: its ccbid.
	char line[256];
	if( !fgets(line, sizeof(line), from_target) ) {
		fprintf(stderr, "requester: target failed before registering\n"); return 1;
	}
	std::string ccbid(line);
	while( !ccbid.empty() && (ccbid.back()=='\n' || ccbid.back()=='\r') ) { ccbid.pop_back(); }

	Daemon ccb(DT_COLLECTOR, broker, NULL);
	Sock *req = ccb.startCommand(CCB_REQUEST, Stream::reli_sock, 20, NULL);
	if( !req ) { fprintf(stderr, "requester: startCommand(CCB_REQUEST) failed\n"); return 1; }

	char *connect_id = Condor_Crypt_Base::randomHexKey(20);
	ClassAd reqad;
	reqad.Assign(ATTR_CCBID, ccbid);
	reqad.Assign(ATTR_CLAIM_ID, connect_id);
	reqad.Assign(ATTR_NAME, "ccb_proxy_bench-requester");
	reqad.Assign(ATTR_MY_ADDRESS, "<0.0.0.0:0>"); // unreachable; proxy ignores it
	reqad.Assign("CCBStreamingRequired", true);
	free(connect_id);
	req->encode();
	if( !putClassAd(req, reqad) || !req->end_of_message() ) {
		fprintf(stderr, "requester: failed to send request\n"); return 1;
	}
	ClassAd reply;
	req->decode();
	if( !getClassAd(req, reply) || !req->end_of_message() ) {
		fprintf(stderr, "requester: failed to read reply\n"); return 1;
	}
	bool result = false;
	reply.LookupBool(ATTR_RESULT, result);
	if( !result ) {
		std::string err; reply.LookupString(ATTR_ERROR_STRING, err);
		fprintf(stderr, "requester: broker refused: %s\n", err.c_str()); return 1;
	}
	// Read the proxied reverse-connect hello.
	int cmd = 0;
	ClassAd hello;
	req->decode();
	if( !req->get(cmd) || !getClassAd(req, hello) || !req->end_of_message() ) {
		fprintf(stderr, "requester: failed to read proxied hello\n"); return 1;
	}

	double tgt_to_req = blast(req->get_file_desc(), seconds);

	// Second line from the target: the requester->target rate it measured.
	double req_to_tgt = 0.0;
	if( fgets(line, sizeof(line), from_target) ) { req_to_tgt = atof(line); }

	auto gbits = [](double bps){ return bps * 8.0 / 1e9; };
	auto mib   = [](double bps){ return bps / (1024.0*1024.0); };
	printf("CCB proxy relay throughput over %ds:\n", seconds);
	printf("  requester -> target : %8.1f MiB/s  (%.2f Gbps)\n", mib(req_to_tgt), gbits(req_to_tgt));
	printf("  target -> requester : %8.1f MiB/s  (%.2f Gbps)\n", mib(tgt_to_req), gbits(tgt_to_req));
	printf("  aggregate           : %8.1f MiB/s  (%.2f Gbps)\n",
		   mib(req_to_tgt+tgt_to_req), gbits(req_to_tgt+tgt_to_req));
	return 0;
}

// targetRole_deadbeat: register like targetRole and report our ccbid, but then
// deliberately never reverse-connect.  We stay registered (socket open) so the
// broker forwards the request and opens a proxy session, then block until the
// parent terminates us -- which it does once it has seen the broker reap the
// never-completed handshake.  Used by the handshake-timeout test.
static int targetRole_deadbeat( const char *broker, FILE *to_parent )
{
	Daemon ccb(DT_COLLECTOR, broker, NULL);
	Sock *reg = ccb.startCommand(CCB_REGISTER, Stream::reli_sock, 20, NULL);
	if( !reg ) { fprintf(stderr, "target: startCommand(CCB_REGISTER) failed\n"); return 1; }

	ClassAd ad;
	ad.Assign(ATTR_COMMAND, CCB_REGISTER);
	ad.Assign(ATTR_NAME, "ccb_proxy_bench-deadbeat-target");
	reg->encode();
	if( !putClassAd(reg, ad) || !reg->end_of_message() ) {
		fprintf(stderr, "target: failed to send registration\n"); return 1;
	}
	ClassAd reply;
	reg->decode();
	if( !getClassAd(reg, reply) || !reg->end_of_message() ) {
		fprintf(stderr, "target: failed to read registration reply\n"); return 1;
	}
	std::string contact;
	reply.LookupString(ATTR_CCBID, contact);
	std::string ccbid = contact;
	size_t hash = contact.rfind('#');
	if( hash != std::string::npos ) { ccbid = contact.substr(hash+1); }

	fprintf(to_parent, "%s\n", ccbid.c_str());
	fflush(to_parent);

	// Never reverse-connect; stay alive and registered until the parent kills us.
	pause();
	return 0;
}

// requesterRole_expectReap: ask the broker (streaming mode) to reach the deadbeat
// target.  Since the target never connects back, the broker should reap the
// half-open session after CCB_SERVER_STREAMING_HANDSHAKE_TIMEOUT and send a
// failure reply.  Returns 0 only if that failure reply arrives.
static int requesterRole_expectReap( const char *broker, FILE *from_target )
{
	char line[256];
	if( !fgets(line, sizeof(line), from_target) ) {
		fprintf(stderr, "requester: target failed before registering\n"); return 1;
	}
	std::string ccbid(line);
	while( !ccbid.empty() && (ccbid.back()=='\n' || ccbid.back()=='\r') ) { ccbid.pop_back(); }

	Daemon ccb(DT_COLLECTOR, broker, NULL);
	Sock *req = ccb.startCommand(CCB_REQUEST, Stream::reli_sock, 20, NULL);
	if( !req ) { fprintf(stderr, "requester: startCommand(CCB_REQUEST) failed\n"); return 1; }

	char *connect_id = Condor_Crypt_Base::randomHexKey(20);
	ClassAd reqad;
	reqad.Assign(ATTR_CCBID, ccbid);
	reqad.Assign(ATTR_CLAIM_ID, connect_id);
	reqad.Assign(ATTR_NAME, "ccb_proxy_bench-requester");
	reqad.Assign(ATTR_MY_ADDRESS, "<0.0.0.0:0>"); // unreachable; proxy ignores it
	reqad.Assign("CCBStreamingRequired", true);
	free(connect_id);
	req->encode();
	if( !putClassAd(req, reqad) || !req->end_of_message() ) {
		fprintf(stderr, "requester: failed to send request\n"); return 1;
	}

	// Wait for the broker's reply: the target never connects back, so the broker
	// must reap the handshake (after its timeout plus a poll cycle) and reply with
	// failure.  Allow generous time for that.
	req->timeout(120);
	ClassAd reply;
	req->decode();
	if( !getClassAd(req, reply) || !req->end_of_message() ) {
		fprintf(stderr, "requester: no reply from broker (expected a reap failure)\n");
		return 1;
	}
	bool result = true;
	reply.LookupBool(ATTR_RESULT, result);
	std::string err; reply.LookupString(ATTR_ERROR_STRING, err);
	if( result ) {
		fprintf(stderr, "requester: broker reported success, but the handshake "
						"should have been reaped\n");
		return 1;
	}
	printf("CCB streaming handshake reaped as expected: %s\n", err.c_str());
	return 0;
}

// Send one streaming CCB_REQUEST for `ccbid` on a fresh connection and leave it
// open (so the pending session it creates is not torn down).  Returns the socket,
// or null on failure.  `holder` keeps the Daemon alive for the socket's lifetime.
static Sock *sendStreamingRequest( const char *broker, const std::string &ccbid,
								   std::shared_ptr<Daemon> &holder )
{
	holder = std::make_shared<Daemon>(DT_COLLECTOR, broker, (char *)NULL);
	Sock *req = holder->startCommand(CCB_REQUEST, Stream::reli_sock, 20, NULL);
	if( !req ) { fprintf(stderr, "cap-test: startCommand(CCB_REQUEST) failed\n"); return nullptr; }
	char *cid = Condor_Crypt_Base::randomHexKey(20);
	ClassAd reqad;
	reqad.Assign(ATTR_CCBID, ccbid);
	reqad.Assign(ATTR_CLAIM_ID, cid);
	reqad.Assign(ATTR_NAME, "ccb_proxy_bench-cap-requester");
	reqad.Assign(ATTR_MY_ADDRESS, "<0.0.0.0:0>");
	reqad.Assign("CCBStreamingRequired", true);
	free(cid);
	req->encode();
	if( !putClassAd(req, reqad) || !req->end_of_message() ) {
		fprintf(stderr, "cap-test: failed to send request\n"); return nullptr;
	}
	return req;
}

// capTest: register a deadbeat target, open one streaming session (left pending),
// then send a second streaming request that should be rejected because the broker
// is at its configured streaming limit.  Returns 0 only if the second request is
// refused.  Run with CCB_SERVER_MAX_STREAMING_SESSIONS or _HANDSHAKES set to 1.
static int capTest( const char *broker )
{
	Daemon ccb(DT_COLLECTOR, broker, NULL);
	Sock *reg = ccb.startCommand(CCB_REGISTER, Stream::reli_sock, 20, NULL);
	if( !reg ) { fprintf(stderr, "cap-test: startCommand(CCB_REGISTER) failed\n"); return 1; }
	ClassAd ad;
	ad.Assign(ATTR_COMMAND, CCB_REGISTER);
	ad.Assign(ATTR_NAME, "ccb_proxy_bench-cap-target");
	reg->encode();
	if( !putClassAd(reg, ad) || !reg->end_of_message() ) { fprintf(stderr, "cap-test: register send failed\n"); return 1; }
	ClassAd rreply;
	reg->decode();
	if( !getClassAd(reg, rreply) || !reg->end_of_message() ) { fprintf(stderr, "cap-test: register reply failed\n"); return 1; }
	std::string contact;
	rreply.LookupString(ATTR_CCBID, contact);
	std::string ccbid = contact;
	size_t hash = contact.rfind('#');
	if( hash != std::string::npos ) { ccbid = contact.substr(hash+1); }

	// First request fills the one allowed slot (its session stays pending; we
	// never read its reply).
	std::shared_ptr<Daemon> hold1;
	Sock *req1 = sendStreamingRequest(broker, ccbid, hold1);
	if( !req1 ) { return 1; }

	// Let the broker register the pending session before we probe the limit.
	sleep(2);

	// Second request must be rejected: the broker is at its streaming limit.
	std::shared_ptr<Daemon> hold2;
	Sock *req2 = sendStreamingRequest(broker, ccbid, hold2);
	if( !req2 ) { return 1; }
	req2->timeout(30);
	ClassAd reply;
	req2->decode();
	if( !getClassAd(req2, reply) || !req2->end_of_message() ) {
		fprintf(stderr, "cap-test: no reply to the over-limit request\n"); return 1;
	}
	bool result = true;
	reply.LookupBool(ATTR_RESULT, result);
	std::string err; reply.LookupString(ATTR_ERROR_STRING, err);
	if( result ) {
		fprintf(stderr, "cap-test: over-limit request was accepted; expected rejection\n"); return 1;
	}
	printf("CCB streaming limit enforced: %s\n", err.c_str());
	return 0;
}

int main( int argc, char **argv )
{
	if( argc < 2 ) {
		fprintf(stderr, "usage: %s <broker-sinful> [seconds | --no-reverse-connect | --cap-test]\n", argv[0]);
		return 2;
	}
	const char *broker = argv[1];
	bool reaper_mode = false;
	bool cap_mode = false;
	int seconds = 5;
	for( int i = 2; i < argc; i++ ) {
		if( std::string(argv[i]) == "--no-reverse-connect" ) { reaper_mode = true; }
		else if( std::string(argv[i]) == "--cap-test" ) { cap_mode = true; }
		else { seconds = atoi(argv[i]); }
	}

	if( cap_mode ) {
		set_priv_initialize();
		config();
		signal(SIGPIPE, SIG_IGN);
		return capTest(broker);
	}

	set_priv_initialize();
	config();
	signal(SIGPIPE, SIG_IGN);

	int fds[2];
	if( pipe(fds) != 0 ) { fprintf(stderr, "pipe() failed\n"); return 1; }

	pid_t pid = fork();
	if( pid < 0 ) { fprintf(stderr, "fork() failed\n"); return 1; }

	if( pid == 0 ) {
		// Child: the target.
		close(fds[0]);
		FILE *to_parent = fdopen(fds[1], "w");
		int rc = reaper_mode
			? (to_parent ? targetRole_deadbeat(broker, to_parent) : 1)
			: (to_parent ? targetRole(broker, seconds, to_parent) : 1);
		if( to_parent ) { fclose(to_parent); }
		_exit(rc);
	}

	// Parent: the requester.
	close(fds[1]);
	FILE *from_target = fdopen(fds[0], "r");

	if( reaper_mode ) {
		int rc = from_target ? requesterRole_expectReap(broker, from_target) : 1;
		if( from_target ) { fclose(from_target); }
		kill(pid, SIGTERM);   // release the deadbeat target
		waitpid(pid, NULL, 0);
		return rc;
	}

	int rc = from_target ? requesterRole(broker, seconds, from_target) : 1;
	if( from_target ) { fclose(from_target); }

	int status = 0;
	waitpid(pid, &status, 0);
	if( rc == 0 && !(WIFEXITED(status) && WEXITSTATUS(status) == 0) ) { rc = 1; }
	return rc;
}

#endif // WIN32
