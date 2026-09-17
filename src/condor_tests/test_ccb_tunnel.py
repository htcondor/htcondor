#!/usr/bin/env pytest

# Test the inbound CCB tunnel: a daemon reached through *two* chained brokers.
#
# Topology (all on one host, two personal pools):
#
#   outside CCB (pool "outside") --- the public broker + central manager
#        ^                    ^
#        | registers          | client queries here, then reaches the schedd
#        | upstream           |
#   inside CCB (pool "inside", USE_OUTBOUND_CCB) --- a local collector that
#        ^                                            registers with the outside
#        | registers                                  CCB and so is itself
#        |                                             reachable through it
#   schedd (pool "inside") --- CCB_ADDRESS points at the local inside CCB, so
#        the contact it advertises is nested: <outside>#<inside_id>#<schedd_id>
#
# A firewalled condor_q (which cannot accept a reverse connection, so it must
# ask the brokers to stream) queried against the outside pool finds the schedd
# and connects to it.  Reaching it requires peeling both brokers: connect the
# outside CCB, stream to the inside CCB, then stream to the schedd.  Both
# collectors therefore log a streaming (proxy) relay for the one connection.

import logging
import time
from collections import namedtuple

from ornithology import action, Condor, ClusterState, write_file, format_script

logger = logging.getLogger(__name__)


def proxy_session_count(condor):
    # The broker logs one "started streaming (proxy) session" per relayed hop.
    return condor.collector_log.path.read_text().count(
        "started streaming (proxy) session"
    )


def collector_address(condor):
    return condor._get_address_file("COLLECTOR").read_text().splitlines()[0]


@action
def outside(test_dir):
    # The public broker and central manager.  Tools and the outside CCB are on
    # NET_DEFAULT so the client reaches the outside CCB directly.
    with Condor(
        local_dir=test_dir / "outside",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed tunnel hop does
            # not carry, so a tool's FS auth to a tunneled daemon cannot satisfy the
            # binding.  Real tunnels span hosts (no shared filesystem), so FS auth is
            # not used across them.
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
    # A node behind the tunnel: the master runs a local collector as the inside
    # CCB (USE_OUTBOUND_CCB) that registers with the outside CCB, and a schedd
    # whose CCB_ADDRESS the master points at that local inside CCB.  The schedd
    # reports to the outside collector (the central manager), so a client there
    # can find it -- but only reach it through both brokers.
    outside_addr = collector_address(outside)
    with Condor(
        local_dir=test_dir / "inside",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed tunnel hop does
            # not carry, so a tool's FS auth to a tunneled daemon cannot satisfy the
            # binding.  Real tunnels span hosts (no shared filesystem), so FS auth is
            # not used across them.
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            # The local collector is this pool's own central manager (its own
            # port); it is ALSO the inside CCB, registering upstream with the
            # separate outside CCB.  We deliberately do not point COLLECTOR_HOST
            # at the outside collector -- the schedd's nested contact already
            # encodes the outside->inside route, so the tunnel is exercised no
            # matter where a client obtains that contact.
            "USE_OUTBOUND_CCB": "TRUE",
            "CCB_OUTBOUND_NEXT_HOP": outside_addr,
            # Put the schedd on a private network distinct from the querying
            # client (NET_DEFAULT), so the client cannot use its direct address
            # and must follow the nested CCB contact.
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_TARGET",
            "DAEMON_LIST": "MASTER SCHEDD",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def node_offhost_ccb(test_dir, outside, inside):
    # A node with NO local inside CCB.  It tunnels through an off-host inside CCB
    # -- here the `inside` pool's collector -- named by OUTBOUND_CCB_ADDRESS.  The
    # master injects that CCB's direct address as the daemons' CCB_ADDRESS; the
    # off-host CCB defers each registration reply until it is itself tunnel-ready,
    # so a daemon learns a nested (tunnel) contact without any separate query.
    off_host_ccb = collector_address(inside)
    with Condor(
        local_dir=test_dir / "node_offhost_ccb",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed tunnel hop does
            # not carry, so a tool's FS auth to a tunneled daemon cannot satisfy the
            # binding.  Real tunnels span hosts (no shared filesystem), so FS auth is
            # not used across them.
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            # Report to the top-level collector so the schedd has a reachable CM.
            "COLLECTOR_HOST": collector_address(outside),
            # No USE_OUTBOUND_CCB (no local inside CCB); instead tunnel outbound
            # through the off-host inside CCB, which is also what the master queries.
            "OUTBOUND_CCB_ADDRESS": off_host_ccb,
            "DAEMON_LIST": "MASTER SCHEDD",
            # Cross-pool on one host: skip the process-family session (see the
            # other tunnel tests) so the master's query authenticates cleanly.
            "SEC_USE_FAMILY_SESSION": "False",
            "MASTER_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


TRANSFER_SENTINEL = "ccb-tunnel-file-transfer-payload-6b1e0a4d"


@action
def inside_exec(test_dir, outside):
    # Like `inside`, but also runs a negotiator and a startd so a real job can
    # run.  The schedd, negotiator and collector stay on NET_DEFAULT (so submit
    # and matchmaking reach the schedd directly -- no tunnel needed there), while
    # the startd is on NET_EXEC.  Because every daemon's CCB is the inside CCB
    # (injected by USE_OUTBOUND_CCB), the startd's contact is doubly nested, so
    # the connections that DO cross networks -- the schedd claiming the startd
    # and the shadow talking to the starter -- are relayed through BOTH brokers,
    # over the non-blocking streaming path (daemons connect non-blocking).
    outside_addr = collector_address(outside)
    with Condor(
        local_dir=test_dir / "inside_exec",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed tunnel hop does
            # not carry, so a tool's FS auth to a tunneled daemon cannot satisfy the
            # binding.  Real tunnels span hosts (no shared filesystem), so FS auth is
            # not used across them.
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            "USE_OUTBOUND_CCB": "TRUE",
            "CCB_OUTBOUND_NEXT_HOP": outside_addr,
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "STARTD.PRIVATE_NETWORK_NAME": "NET_EXEC",
            # On a single host every daemon shares an IP, so a daemon reaching a
            # broker through the tunnel would try the process-family security
            # session (used for local peers) -- which a broker in a different pool
            # (the outside CCB) rejects.  A real deployment's outside CCB is on
            # another host, so the family session is never attempted there; disable
            # it here to emulate that and exercise the tunnel's own end-to-end auth.
            "SEC_USE_FAMILY_SESSION": "False",
            "DAEMON_LIST": "MASTER NEGOTIATOR SCHEDD STARTD",
            "START": "TRUE",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


# Carries the submitted job handle alongside each broker's proxy-session count
# captured immediately BEFORE the job was submitted, so a test can assert the
# count strictly increased -- catching a job that silently used a direct
# connection instead of the tunnel.
TunnelJobResult = namedtuple(
    "TunnelJobResult", ["handle", "before_outside", "before_inside"]
)


@action
def tunnel_job(test_dir, outside, inside_exec):
    # A job whose sandbox transfer forces a shadow<->starter connection.  The
    # shadow (NET_DEFAULT) and starter (NET_EXEC) are on different private
    # networks and both behind the inside CCB, so that connection -- and the
    # schedd->startd claim -- are relayed through BOTH brokers over the
    # non-blocking streaming path.  A clean, content-matching run proves it works.
    payload = write_file(test_dir / "tunnel_in.dat", TRANSFER_SENTINEL + "\n")
    exe = write_file(
        test_dir / "tunnel_job.sh",
        format_script("#!/bin/sh\ncat tunnel_in.dat\n"),
    )
    # Baseline each broker's relay count BEFORE the job runs, so the relay test
    # can assert a per-job increase rather than an absolute count (which a job
    # taking a silent direct connection could still satisfy from earlier setup).
    before_outside = proxy_session_count(outside)
    before_inside = proxy_session_count(inside_exec)
    handle = inside_exec.submit(
        description={
            "executable": exe.as_posix(),
            "output": (test_dir / "tunnel_job.out").as_posix(),
            "error": (test_dir / "tunnel_job.err").as_posix(),
            "log": "test_ccb_tunnel_job.log",
            "transfer_input_files": payload.as_posix(),
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "request_memory": "16MB",
            "request_disk": "16MB",
        },
        count=1,
    )
    assert handle.wait(condition=ClusterState.all_complete, timeout=300, verbose=True)
    return TunnelJobResult(handle, before_outside, before_inside)


def schedd_name(inside):
    # Find the tunneled schedd's name as advertised to its (local inside) collector.
    deadline = time.time() + 60
    while time.time() < deadline:
        p = inside.run_command(["condor_status", "-schedd", "-af", "Name"])
        if p.returncode == 0 and p.stdout.strip():
            return p.stdout.strip().splitlines()[0]
        time.sleep(1)
    return ""


class TestCCBTunnel:
    def test_schedd_advertises_nested_contact(self, outside, inside):
        # The schedd's CCB contact should carry two '#'-separated ids (outside
        # then inside), proving the inside CCB stamped its derived tunnel address.
        name = schedd_name(inside)
        assert name, "tunneled schedd never advertised to its collector"
        p = inside.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
        assert p.returncode == 0, "condor_status failed: %s" % p.stderr
        addr = p.stdout.strip().splitlines()[0]
        # The CCB contact is inside the sinful; a two-hop tunnel nests two ids.
        assert addr.count("#") >= 2, "schedd address not doubly nested: %s" % addr

    def test_firewalled_tool_reaches_schedd_through_both_brokers(
        self, outside, inside, monkeypatch
    ):
        # NOTE: this test runs as root too.  The firewall is simulated purely by
        # _CONDOR_USE_SHARED_PORT=FALSE + _CONDOR_TOOLS_ASSUME_FIREWALLS=TRUE
        # (set below).  With USE_SHARED_PORT false, SharedPortEndpoint::UseSharedPort
        # returns false at its "USE_SHARED_PORT=false" check -- BEFORE the
        # can_switch_ids() (root) branch that would otherwise let root use the shared
        # port -- so the CCB client cannot open a reverse-connect listener and falls
        # back to streaming through the brokers.  That fallback path is
        # uid-independent, so there is no root-only reason to skip.  (A prior skip
        # here was over-conservative; the FS-auth channel-binding concern that
        # motivated it is handled separately by SEC_FS_ENFORCE_CHANNEL_BINDING=False.)

        name = schedd_name(inside)
        assert name, "tunneled schedd never advertised to its collector"

        before_outside = proxy_session_count(outside)
        before_inside = proxy_session_count(inside)

        # Force the tool to stream: it cannot accept a reverse connection.
        monkeypatch.setenv("_CONDOR_TOOLS_ASSUME_FIREWALLS", "TRUE")
        monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "FALSE")

        p = inside.run_command(["condor_q", "-name", name], timeout=90)
        assert p.returncode == 0, (
            "firewalled condor_q to tunneled schedd failed: rc=%s stdout=%r stderr=%r"
            % (p.returncode, p.stdout, p.stderr)
        )

        # Both brokers must have relayed a hop for this one connection.
        deadline = time.time() + 60
        while time.time() < deadline and (
            proxy_session_count(outside) <= before_outside
            or proxy_session_count(inside) <= before_inside
        ):
            time.sleep(1)
        assert proxy_session_count(outside) > before_outside, (
            "outside CCB did not relay the first tunnel hop"
        )
        assert proxy_session_count(inside) > before_inside, (
            "inside CCB did not relay the second tunnel hop"
        )

    def test_request_to_nonexistent_tunnel_hop_fails_cleanly(
        self, outside, inside, monkeypatch
    ):
        # A *well-formed* tunneled request whose final next-hop ccbid is not
        # registered must fail cleanly at the inside CCB (StartInboundRelay finds no
        # such target and increments CCBTunnelRelaysFailed), not hang or misroute.  We
        # take the schedd's real nested contact and replace the final ccbid of each
        # tunnel path with a non-existent one, then try to reach it.
        import re

        # Runs as root too: the firewall is simulated by _CONDOR_USE_SHARED_PORT=FALSE
        # + _CONDOR_TOOLS_ASSUME_FIREWALLS=TRUE (set below), which forces the streaming
        # fallback regardless of uid.  See the note in
        # test_firewalled_tool_reaches_schedd_through_both_brokers.

        name = schedd_name(inside)
        assert name, "tunneled schedd never advertised to its collector"
        p = inside.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
        assert p.returncode == 0 and p.stdout.strip(), "schedd never advertised"
        real = p.stdout.strip().splitlines()[0]
        assert real.count("#") >= 2, "schedd contact not nested: %s" % real

        # The last ccbid of each path is the "#<digits>" just before a path separator
        # or the end of the CCB contact; leave the intermediate (inside) id intact.
        bogus = re.sub(r"#\d+(?=($|[ &>]|%20))", "#88888", real)
        assert bogus != real and "#88888" in bogus, (
            "failed to mangle the final ccbid of %s -> %s" % (real, bogus)
        )

        # Force the tool to stream so it exercises the tunnel-recursion path (a
        # directly-reachable tool would ask for an ordinary reverse connection).
        monkeypatch.setenv("_CONDOR_TOOLS_ASSUME_FIREWALLS", "TRUE")
        monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "FALSE")

        p = inside.run_command(["condor_ping", "-addr", bogus, "READ"], timeout=90)
        assert p.returncode != 0, (
            "condor_ping to a non-existent tunnel hop unexpectedly succeeded: %r"
            % (p.stdout,)
        )

        # The inside CCB should have reported the missing next-hop ccbid (the
        # CCBTunnelRelaysFailed path), proving the request was well-formed and routed
        # but the final hop was simply absent.
        deadline = time.time() + 30
        log = ""
        while time.time() < deadline:
            log = inside.collector_log.path.read_text()
            if "no daemon registered as next-hop ccbid 88888" in log:
                break
            time.sleep(1)
        assert "no daemon registered as next-hop ccbid 88888" in log, (
            "inside CCB never reported the missing next-hop ccbid:\n...%s" % log[-2000:]
        )


class TestCCBTunnelJob:
    # Exercises the non-blocking multi-hop path: the daemon-to-daemon connections
    # this job needs (schedd->startd claim, shadow<->starter) cross the tunnel and
    # daemons connect non-blocking, so completing proves the async multi-hop follow.
    def test_job_ran_through_the_tunnel(self, tunnel_job):
        assert tunnel_job.handle.state.all_complete()

    def test_sandbox_transfer_delivered_through_the_tunnel(self, tunnel_job, test_dir):
        # The job catted its transferred input to stdout, which file transfer -- a
        # distinct shadow<->starter connection -- carried back through the tunnel.
        assert TRANSFER_SENTINEL in (test_dir / "tunnel_job.out").read_text()

    def test_both_brokers_relayed_for_the_job(self, outside, inside_exec, tunnel_job):
        # The job's cross-network connections traverse both brokers, so each
        # collector-as-CCB must log a NEW streaming relay for this job.  Asserting a
        # strict increase over the pre-submit baseline (not just count > 0) makes the
        # test fail if the job silently used a direct connection instead of the tunnel.
        assert proxy_session_count(outside) > tunnel_job.before_outside, (
            "outside CCB relayed nothing new for the job (before=%d, after=%d) -- "
            "the job may have used a direct connection"
            % (tunnel_job.before_outside, proxy_session_count(outside))
        )
        assert proxy_session_count(inside_exec) > tunnel_job.before_inside, (
            "inside CCB relayed nothing new for the job (before=%d, after=%d) -- "
            "the job may have used a direct connection"
            % (tunnel_job.before_inside, proxy_session_count(inside_exec))
        )


class TestCCBTunnelOffHostCCB:
    def test_master_injects_offhost_ccb_direct_address(
        self, outside, inside, node_offhost_ccb
    ):
        # The node has no local inside CCB; it tunnels through an off-host inside CCB
        # (the `inside` pool's collector, OUTBOUND_CCB_ADDRESS).  The master injects
        # that CCB's *direct* address as the daemons' CCB_ADDRESS -- the CCB itself
        # stamps the tunnel nesting; the master must NOT inject the CCB's derived
        # nested contact, which the daemons' CCBListener cannot register through.  The
        # master needs no tunnel-address query or readiness gate: the off-host CCB
        # defers each registration reply until it is itself tunnel-ready, so the
        # contact a daemon learns is already nested (verified by the reachability test
        # below).
        off_host_ccb = collector_address(inside)
        master_log = node_offhost_ccb.master_log.path.read_text()

        assert (
            "Injecting CCB_ADDRESS=%s (off-host inside CCB, outbound-CCB tunnel)"
            % off_host_ccb
        ) in master_log, (
            "master did not inject the off-host CCB's direct address as CCB_ADDRESS:\n%s"
            % master_log
        )
        # The injected address is the CCB's *direct* address, not a nested contact.
        assert "#" not in off_host_ccb, (
            "off-host CCB address is unexpectedly nested: %s" % off_host_ccb
        )

    def test_node_schedd_reachable_through_tunnel(self, outside, inside, node_offhost_ccb):
        # Reachability, not just log-scraping: the node's schedd must register with
        # the off-host inside CCB and advertise a doubly-nested contact
        # (<outside>#inside_id#schedd_id), proving the CCB stamped the tunnel nesting
        # onto a daemon whose CCB_ADDRESS was injected as the CCB's direct address.
        deadline = time.time() + 60
        addr = ""
        while time.time() < deadline:
            p = node_offhost_ccb.run_command(
                ["condor_status", "-schedd", "-af", "MyAddress"]
            )
            if p.returncode == 0 and p.stdout.strip():
                addr = p.stdout.strip().splitlines()[0]
                if addr.count("#") >= 2:
                    break
            time.sleep(1)
        assert addr, "node_offhost_ccb schedd never advertised to the collector"
        assert addr.count("#") >= 2, (
            "node_offhost_ccb schedd address is not a doubly-nested tunnel contact "
            "(off-host CCB did not stamp the tunnel nesting): %s" % addr
        )


@action
def outside2(test_dir):
    # A second, independent public broker, so an inside CCB can register upstream
    # through two next hops at once.
    with Condor(
        local_dir=test_dir / "outside2",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed tunnel hop does
            # not carry, so a tool's FS auth to a tunneled daemon cannot satisfy the
            # binding.  Real tunnels span hosts (no shared filesystem), so FS auth is
            # not used across them.
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def inside_multi(test_dir, outside, outside2):
    # An inside CCB whose CCB_OUTBOUND_NEXT_HOP is a *list* of two brokers: it
    # registers upstream through both, so the CCBIDs it hands out carry both tunnel
    # paths and its schedd is reachable via either broker.
    next_hops = "%s %s" % (collector_address(outside), collector_address(outside2))
    with Condor(
        local_dir=test_dir / "inside_multi",
        config={
            "ENABLE_IPV6": "FALSE",
            # Single-host test: FS auth ties its rendezvous directory to the
            # daemon's own network address, which a broker-relayed tunnel hop does
            # not carry, so a tool's FS auth to a tunneled daemon cannot satisfy the
            # binding.  Real tunnels span hosts (no shared filesystem), so FS auth is
            # not used across them.
            "SEC_FS_ENFORCE_CHANNEL_BINDING": "False",
            "USE_SHARED_PORT": "FALSE",
            "USE_OUTBOUND_CCB": "TRUE",
            "CCB_OUTBOUND_NEXT_HOP": next_hops,
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_TARGET",
            "DAEMON_LIST": "MASTER SCHEDD",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


def _broker_hostport(condor):
    # host:port of a collector's command socket (the bracket-stripped sinful head),
    # which is how it appears inside a nested CCB contact.
    import re

    sinful = collector_address(condor)
    m = re.match(r"<([^?>]+)", sinful)
    return m.group(1) if m else sinful


def _hostport_reachable(hostport):
    # True iff a TCP connection to host:port completes.  Takes a captured host:port
    # string (not a live Condor) because a broker taken down with condor_off deletes
    # its collector address file, so it can no longer be read back from the daemon.
    # Used to confirm a broker is really down (so a later success genuinely required
    # failover, not a lucky race against a still-listening socket).
    import socket

    host, _, port = hostport.rpartition(":")
    try:
        with socket.create_connection((host, int(port)), timeout=2):
            return True
    except OSError:
        return False


class TestCCBTunnelMultipleUpstream:
    def test_schedd_advertises_both_tunnel_paths(self, outside, outside2, inside_multi):
        # The inside CCB registered upstream through BOTH brokers, so the schedd's
        # advertised CCB contact must carry a path through each -- a space-separated
        # list of nested contacts, one per next hop.
        deadline = time.time() + 90
        addr = ""
        want_a = _broker_hostport(outside)
        want_b = _broker_hostport(outside2)
        while time.time() < deadline:
            p = inside_multi.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
            if p.returncode == 0 and p.stdout.strip():
                addr = p.stdout.strip().splitlines()[0]
                if want_a in addr and want_b in addr:
                    break
            time.sleep(1)
        assert addr, "schedd never advertised to its collector"
        assert want_a in addr, (
            "schedd contact missing the path through the first broker %s: %s"
            % (want_a, addr)
        )
        assert want_b in addr, (
            "schedd contact missing the path through the second broker %s: %s"
            % (want_b, addr)
        )

    def test_reachable_via_surviving_broker_after_one_fails(
        self, outside, outside2, inside_multi, monkeypatch
    ):
        # Defined LAST in this class on purpose: it brings `outside` down, and the
        # class-scoped fixtures keep it down for anything defined after it.  With the
        # inside CCB registered upstream through BOTH brokers, killing the
        # first-listed broker must not sever the schedd: a firewalled tool -- which
        # cannot accept a reverse connection and so must route through a broker --
        # still reaches the schedd by failing over to the surviving broker, and that
        # survivor relays the hop.
        name = schedd_name(inside_multi)
        assert name, "tunneled schedd never advertised to its collector"

        want_a = _broker_hostport(outside)
        want_b = _broker_hostport(outside2)

        # Precondition: both tunnel paths are live in the schedd's contact before we
        # take a broker down (otherwise "failover" would be vacuous).
        deadline = time.time() + 90
        addr = ""
        while time.time() < deadline:
            p = inside_multi.run_command(["condor_status", "-schedd", "-af", "MyAddress"])
            if p.returncode == 0 and p.stdout.strip():
                addr = p.stdout.strip().splitlines()[0]
                if want_a in addr and want_b in addr:
                    break
            time.sleep(1)
        assert want_a in addr and want_b in addr, (
            "schedd contact did not carry both tunnel paths before failover: %s" % addr
        )

        # Capture the outside broker's host:port BEFORE taking it down: condor_off
        # deletes its collector address file, so it cannot be read back afterward.
        outside_hostport = _broker_hostport(outside)

        # Bring the first broker down and confirm it is truly unreachable, so the
        # later success cannot be a race against a still-listening socket.
        outside.run_command(["condor_off", "-fast", "-collector"], timeout=30)
        down_by = time.time() + 60
        while time.time() < down_by and _hostport_reachable(outside_hostport):
            time.sleep(1)
        assert not _hostport_reachable(outside_hostport), (
            "outside broker's collector never went down after condor_off"
        )

        before_survivor = proxy_session_count(outside2)

        # Firewalled tool: forced to stream through a broker (see the firewalled-tool
        # test earlier).  The path through the dead broker must fail and the client
        # must fall over to the survivor.
        monkeypatch.setenv("_CONDOR_TOOLS_ASSUME_FIREWALLS", "TRUE")
        monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "FALSE")

        p = inside_multi.run_command(["condor_q", "-name", name], timeout=120)
        assert p.returncode == 0, (
            "firewalled condor_q did not fail over to the surviving broker: "
            "rc=%s stdout=%r stderr=%r" % (p.returncode, p.stdout, p.stderr)
        )

        # The survivor must have relayed the hop for this connection, proving the
        # schedd was reached THROUGH the surviving broker and not by a direct path.
        deadline = time.time() + 60
        while (
            time.time() < deadline
            and proxy_session_count(outside2) <= before_survivor
        ):
            time.sleep(1)
        assert proxy_session_count(outside2) > before_survivor, (
            "surviving broker did not relay the failed-over tunnel hop"
        )
