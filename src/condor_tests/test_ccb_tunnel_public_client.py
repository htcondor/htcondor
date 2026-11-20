#!/usr/bin/env pytest

# A publicly reachable client must still be able to reach a daemon behind an
# inbound CCB tunnel (a nested contact "<entry-broker>#id1#id2").
#
# Topology (one host, two personal pools) -- the same two-broker inbound tunnel
# as test_ccb_tunnel.py:
#
#   outside CCB (pool "outside") --- the public broker + central manager
#        ^                   ^
#        | registers         | the client talks to this broker directly
#        | upstream          |
#   inside CCB (pool "inside", USE_OUTBOUND_CCB) --- a local collector that
#        ^                                           registers with the outside
#        | registers                                 CCB, so it is itself only
#        |                                           reachable through it
#   schedd (pool "inside") --- advertises <outside>#<inside_id>#<schedd_id>
#
# test_ccb_tunnel.py covers the case where the *client* cannot accept a reverse
# connection (_CONDOR_TOOLS_ASSUME_FIREWALLS=TRUE), and test_ccb_tunnel_ap.py the
# case where the client is itself behind a CCB.
#
# This test covers the remaining case: a client that is publicly reachable.  It
# must still stream, because the schedd cannot reach the client in an outbound
# manner.
#
# The schedd-behind-a-tunnel case is contrived but it's the best way to test the
# blocking connect code paths (which are exclusively used in the client; the
# non-blocking case will be tested separately).

import logging
import os
import re
import time

from ornithology import action, Condor

logger = logging.getLogger(__name__)

# The broker logs one of these per relayed hop.
_SESSION_MARK = "started streaming (proxy) session"


def proxy_session_count(condor):
    return condor.collector_log.path.read_text().count(_SESSION_MARK)


def collector_address(condor):
    return condor._get_address_file("COLLECTOR").read_text().splitlines()[0]


@action
def outside(test_dir):
    # The public broker and central manager.  The client is on NET_DEFAULT with
    # the outside CCB, so it reaches that broker directly.
    with Condor(
        local_dir=test_dir / "outside",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed hop does not
            # carry.  Real tunnels span hosts, so FS auth is not used across them.
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def inside(test_dir, outside):
    # The tunneled node: a local collector acting as the inside CCB
    # (USE_OUTBOUND_CCB) plus a schedd whose contact is nested through both
    # brokers.  The schedd is on a private network distinct from the
    # client, so the client cannot shortcut to its direct address.
    outside_addr = collector_address(outside)
    with Condor(
        local_dir=test_dir / "inside",
        config={
            "ENABLE_IPV6": "FALSE",
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            "USE_OUTBOUND_CCB": "TRUE",
            "CCB_OUTBOUND_NEXT_HOP": outside_addr,
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_TARGET",
            "DAEMON_LIST": "MASTER SCHEDD",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


def schedd_name(inside):
    deadline = time.time() + 60
    while time.time() < deadline:
        p = inside.run_command(["condor_status", "-schedd", "-af", "Name"])
        if p.returncode == 0 and p.stdout.strip():
            return p.stdout.strip().splitlines()[0]
        time.sleep(1)
    return ""


def schedd_address(inside):
    p = inside.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
    if p.returncode != 0 or not p.stdout.strip():
        return ""
    return p.stdout.strip().splitlines()[0]


# "CCBClient: requesting reverse connection to <peer> via CCB server <addr>#<id>;"
# is the plain (non-streaming) path.  Capture the broker address it handed to the
# Daemon object: for a nested contact that address still contains a '#'.  That's
# wrong as it's a CCB *contact* passed off as a server address.
_DIRECT_REQUEST_RE = re.compile(
    r"CCBClient: requesting reverse connection to .*? via CCB server (\S+?)#(\d+);"
)

_STREAM_MARK = "requesting streaming (proxied) connection"


# "CCB: received request id 7 from TOOL on <...> for target ccbid 1 (registered
# as COLLECTOR <...> on <...>)" is what the outside broker thinks it was asked for.
# For a nested contact the outside broker must never serve a plain request itself;
# the inner ids belong to the inner broker.
_BROKER_SERVED_RE = re.compile(
    r"CCB: received request id \d+ from (TOOL) on \S+ for target ccbid (\d+) "
    r"\(registered as (\S+) "
)


@action
def public_client_query(test_dir, outside, inside):
    # Run an ordinary, reachable condor_q against the tunneled schedd and keep
    # everything the run produced, so each assertion below can report on it.
    name = schedd_name(inside)
    assert name, "tunneled schedd never advertised to its collector"
    addr = schedd_address(inside)
    assert addr.count("#") >= 2, "schedd contact is not doubly nested: %s" % addr

    before_outside = proxy_session_count(outside)
    before_inside = proxy_session_count(inside)
    outside_log_mark = len(outside.collector_log.path.read_text())

    # Deliberately NOT setting _CONDOR_TOOLS_ASSUME_FIREWALLS: this client can
    # accept a reverse connection.
    saved = os.environ.get("_CONDOR_TOOL_DEBUG")
    # D_NETWORK:2 (verbose), not plain D_NETWORK: the CCBClient tracing we want
    # is logged at D_NETWORK|D_FULLDEBUG, which IsDebugCatAndVerbosity resolves to
    # the *verbose* D_NETWORK listener.
    os.environ["_CONDOR_TOOL_DEBUG"] = "D_ALWAYS D_FULLDEBUG D_NETWORK:2"
    try:
        p = inside.run_command(["condor_q", "-debug", "-name", name], timeout=120)
    finally:
        if saved is None:
            del os.environ["_CONDOR_TOOL_DEBUG"]
        else:
            os.environ["_CONDOR_TOOL_DEBUG"] = saved

    # Give the brokers a moment to flush any relay they did start.
    deadline = time.time() + 30
    while time.time() < deadline and (
        proxy_session_count(outside) <= before_outside
        or proxy_session_count(inside) <= before_inside
    ):
        time.sleep(1)

    debug = (p.stderr or "") + "\n" + (p.stdout or "")
    # Keep the whole client trace next to the test's other artifacts; the
    # assertions below can only quote a tail of it.
    (test_dir / "public_client_condor_q.debug").write_text(debug)

    return {
        "name": name,
        "addr": addr,
        "proc": p,
        "debug": debug,
        "outside_log_tail": outside.collector_log.path.read_text()[outside_log_mark:],
        "before_outside": before_outside,
        "before_inside": before_inside,
        "after_outside": proxy_session_count(outside),
        "after_inside": proxy_session_count(inside),
    }


class TestPublicClientReachesTunneledDaemon:
    def test_contact_is_nested(self, public_client_query):
        # Sanity: the thing under test really is a two-hop tunnel contact.
        assert public_client_query["addr"].count("#") >= 2

    def test_query_succeeds(self, public_client_query):
        p = public_client_query["proc"]
        assert p.returncode == 0, (
            "a publicly reachable condor_q could not reach the tunneled schedd "
            "(rc=%s)\n--- stdout ---\n%s\n--- stderr (tail) ---\n%s"
            % (p.returncode, p.stdout, (p.stderr or "")[-4000:])
        )

    def test_client_asked_the_broker_to_stream(self, public_client_query):
        # The decision to stream must come from the client
        debug = public_client_query["debug"]
        assert _STREAM_MARK in debug, (
            "the client never requested a streaming (proxied) connection for a "
            "nested CCB contact %s; it is reachable, so it chose a plain reverse "
            "connection instead\n--- client debug (tail) ---\n%s"
            % (public_client_query["addr"], debug[-4000:])
        )

    def test_client_did_not_hand_a_nested_contact_to_daemon(
        self, public_client_query
    ):
        # Pre-fix, the plain reverse-connect path peeled only the innermost id and
        # passes the REMAINDER to the daemon.
        bad = [
            broker
            for broker, _id in _DIRECT_REQUEST_RE.findall(public_client_query["debug"])
            if "#" in broker
        ]
        assert not bad, (
            "the client treated a nested CCB contact as a broker address and "
            "asked for a plain reverse connection via %r; the remaining hop(s) "
            "are silently dropped, so the request goes to the wrong broker"
            % (bad,)
        )

    def test_both_brokers_relayed_a_hop(self, public_client_query):
        q = public_client_query
        assert q["after_outside"] > q["before_outside"], (
            "outside CCB did not relay the first tunnel hop for a publicly "
            "reachable client (%d -> %d)" % (q["before_outside"], q["after_outside"])
        )
        assert q["after_inside"] > q["before_inside"], (
            "inside CCB did not relay the second tunnel hop for a publicly "
            "reachable client (%d -> %d)" % (q["before_inside"], q["after_inside"])
        )

    def test_entry_broker_did_not_serve_an_inner_hop_itself(
        self, public_client_query
    ):
        # The original symptom.  The client peeled the inner id and sent it to the
        # outside broker, a mismatch.  The client then ran a full CEDAR handshake
        # with potentially the wrong daemon.
        served = _BROKER_SERVED_RE.findall(public_client_query["outside_log_tail"])
        assert not served, (
            "the entry broker served a plain CCB request for an inner tunnel hop "
            "and rendezvoused the client with the wrong daemon: %r\n"
            "(requester, ccbid, registered-as)" % (served,)
        )
