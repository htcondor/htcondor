#!/usr/bin/env pytest

# Test the inbound CCB tunnel through THREE chained brokers (a depth-3 route).
# The existing test_ccb_tunnel.py exercises depth-2 tunnels only: the outer
# broker peels the head ccbid and the inner CCB relays with an EMPTY route tail.
# This test adds a third broker so that reaching the schedd forces the MIDDLE
# broker's StartInboundRelay to split and forward a NON-EMPTY route tail (a
# route with more than one token), exercising the recursion + ValidRoute for a
# multi-hop route.
#
# Topology (all on one host, three personal pools):
#
#   outer CCB (pool "outer") --- the public broker + central manager
#        ^                    ^
#        | middle registers    | a firewalled tool queries here first, then peels
#        | upstream            | the outer#middle#inner#schedd nesting
#   middle CCB (pool "middle", CCB_OUTBOUND_NEXT_HOP=outer) --- a pure relay
#        ^                       collector: it registers upstream with the outer
#        | inner registers        CCB, so its own derived contact is <outer>#middle_id
#        | upstream
#   inner CCB (pool "inner", USE_OUTBOUND_CCB, CCB_OUTBOUND_NEXT_HOP=middle) ---
#        ^   a local collector that registers upstream with the middle CCB, so its
#        |   derived contact is <outer>#middle_id#inner_id
#   schedd (pool "inner") --- CCB_ADDRESS points at the local inner CCB, so the
#        contact it advertises is triply nested:
#        <outer>#middle_id#inner_id#schedd_id
#
# A firewalled condor_q (which cannot accept a reverse connection, so it must ask
# the brokers to stream) queried against the inner pool finds the schedd and
# connects to it.  Reaching it requires peeling all three brokers: the outer CCB
# relays to the middle CCB with route "inner_id schedd_id", the middle CCB relays
# to the inner CCB with the remaining route "schedd_id" (a NON-EMPTY tail -- the
# case depth-2 tests never reach), and the inner CCB relays to the schedd with an
# empty tail.  All three collectors therefore log a streaming (proxy) relay.
#
# NOTE ON RUNTIME: this stands up three independent personal pools and waits for a
# three-layer upstream-registration cascade to complete, so it is heavier than the
# depth-2 tunnel tests (roughly 1-2 minutes locally).  It is still comfortably
# within the quick-test budget.

import logging
import re
import time

from ornithology import action, Condor

logger = logging.getLogger(__name__)


def proxy_session_count(condor):
    # The broker logs one "started streaming (proxy) session" per relayed hop.
    return condor.collector_log.path.read_text().count(
        "started streaming (proxy) session"
    )


def collector_address(condor):
    return condor._get_address_file("COLLECTOR").read_text().splitlines()[0]


def schedd_name(condor):
    # The tunneled schedd's name as advertised to its (local inner) collector.
    deadline = time.time() + 60
    while time.time() < deadline:
        p = condor.run_command(["condor_status", "-schedd", "-af", "Name"])
        if p.returncode == 0 and p.stdout.strip():
            return p.stdout.strip().splitlines()[0]
        time.sleep(1)
    return ""


# Every fixture disables the process-family session (cross-pool daemons share one
# host here, but a real deployment's brokers are on other hosts, so the family
# session is never attempted across them) and FS channel binding (a broker-relayed
# tunnel hop does not carry the daemon's own network address that single-host FS
# auth binds to); real tunnels span hosts, so neither is used across them.
_COMMON = {
    "ENABLE_IPV6": "FALSE",
    "USE_SHARED_PORT": "FALSE",
    "SEC_USE_FAMILY_SESSION": "False",
    "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
    "COLLECTOR_DEBUG": "D_FULLDEBUG",
}


@action
def outer(test_dir):
    # The public broker and central manager (broker only for this test).
    with Condor(
        local_dir=test_dir / "outer",
        config={
            **_COMMON,
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "DAEMON_LIST": "MASTER COLLECTOR",
        },
    ) as condor:
        yield condor


@action
def middle(test_dir, outer):
    # The middle broker: a pure relay collector that registers upstream with the
    # outer CCB (CCB_OUTBOUND_NEXT_HOP).  Its own derived contact is <outer>#id, so
    # anything registering through IT becomes doubly nested -- which is what makes
    # the inner CCB (and thus the schedd) triply nested.
    with Condor(
        local_dir=test_dir / "middle",
        config={
            **_COMMON,
            "CCB_OUTBOUND_NEXT_HOP": collector_address(outer),
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "DAEMON_LIST": "MASTER COLLECTOR",
        },
    ) as condor:
        yield condor


@action
def inner(test_dir, outer, middle):
    # The inner node: a local collector that is this pool's inside CCB
    # (USE_OUTBOUND_CCB) and registers upstream with the MIDDLE CCB, plus a schedd
    # whose CCB is that local inner collector.  The schedd is on a private network
    # distinct from the querying tool (NET_DEFAULT), so the tool cannot use its
    # direct address and must follow the triply-nested CCB contact.
    with Condor(
        local_dir=test_dir / "inner",
        config={
            **_COMMON,
            "USE_OUTBOUND_CCB": "TRUE",
            "CCB_OUTBOUND_NEXT_HOP": collector_address(middle),
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_TARGET",
            "DAEMON_LIST": "MASTER SCHEDD",
        },
    ) as condor:
        yield condor


class TestCCBTunnel3Hop:
    def test_schedd_advertises_triply_nested_contact(self, outer, middle, inner):
        # Three chained brokers -> three '#'-separated ids stamped onto the schedd's
        # CCB contact (<outer>#middle_id#inner_id#schedd_id), proving each layer
        # nested the one below it.
        deadline = time.time() + 120
        addr = ""
        while time.time() < deadline:
            p = inner.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
            if p.returncode == 0 and p.stdout.strip():
                addr = p.stdout.strip().splitlines()[0]
                if addr.count("#") >= 3:
                    break
            time.sleep(1)
        assert addr, "tunneled schedd never advertised to its collector"
        assert addr.count("#") >= 3, (
            "schedd address is not a triply-nested tunnel contact: %s" % addr
        )

    def test_firewalled_tool_reaches_schedd_through_three_brokers(
        self, outer, middle, inner, monkeypatch
    ):
        # The firewall is simulated purely by the two env vars below (see the note in
        # test_ccb_tunnel.py's firewalled-tool test); it runs as root too.
        name = schedd_name(inner)
        assert name, "tunneled schedd never advertised to its collector"

        # Wait for the triply-nested contact to be live before probing.
        deadline = time.time() + 120
        addr = ""
        while time.time() < deadline:
            p = inner.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
            if p.returncode == 0 and p.stdout.strip():
                addr = p.stdout.strip().splitlines()[0]
                if addr.count("#") >= 3:
                    break
            time.sleep(1)
        assert addr.count("#") >= 3, (
            "schedd contact was not triply nested before the probe: %s" % addr
        )

        before_outer = proxy_session_count(outer)
        before_middle = proxy_session_count(middle)
        before_inner = proxy_session_count(inner)

        # Force the tool to stream: it cannot accept a reverse connection.
        monkeypatch.setenv("_CONDOR_TOOLS_ASSUME_FIREWALLS", "TRUE")
        monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "FALSE")

        p = inner.run_command(["condor_q", "-name", name], timeout=120)
        assert p.returncode == 0, (
            "firewalled condor_q to the triply-tunneled schedd failed: "
            "rc=%s stdout=%r stderr=%r" % (p.returncode, p.stdout, p.stderr)
        )

        # All three brokers must have relayed a hop for this one connection.
        deadline = time.time() + 90
        while time.time() < deadline and (
            proxy_session_count(outer) <= before_outer
            or proxy_session_count(middle) <= before_middle
            or proxy_session_count(inner) <= before_inner
        ):
            time.sleep(1)
        assert proxy_session_count(outer) > before_outer, (
            "outer CCB did not relay the first tunnel hop"
        )
        assert proxy_session_count(middle) > before_middle, (
            "middle CCB did not relay the second tunnel hop"
        )
        assert proxy_session_count(inner) > before_inner, (
            "inner CCB did not relay the third tunnel hop"
        )

        # The core of this test: the MIDDLE broker's StartInboundRelay must have
        # split and forwarded a NON-EMPTY route tail (the remaining "schedd_id" after
        # peeling inner_id), which depth-2 tunnels never produce.  Its log line is
        #   "relaying inbound tunnel to next-hop ccbid <id> (remaining route '<tail>')"
        # so a non-empty '<digits>' remaining route proves the >1-token recursion.
        middle_log = middle.collector_log.path.read_text()
        assert re.search(
            r"relaying inbound tunnel to next-hop ccbid \d+ \(remaining route '\d+'\)",
            middle_log,
        ), (
            "middle broker never forwarded a non-empty route tail (the depth-3 "
            "recursion was not exercised):\n...%s" % middle_log[-2000:]
        )
