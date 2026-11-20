#!/usr/bin/env pytest

# A publicly reachable AP must be able to run a job on an EP behind a two-CCB
# tunnel.
#
#   central manager + outside CCB (pool "cm") --- public
#        ^                     ^                      ^
#        | inside CCB          |                      | startd + schedd report here
#        | registers upstream  |                      |
#   inside CCB (pool "inside_ccb")            AP: schedd (pool "public_ap")
#        ^  CCB_OUTBOUND_NEXT_HOP = cm            NO CCB_ADDRESS, NET_DEFAULT:
#        |  stamps the tunnel into its CCBIDs     directly reachable by anyone
#   startd (pool "ep"), private (NET_EXEC)
#        CCB_ADDRESS = inside CCB, so its contact is doubly nested
#
# test_ccb_tunnel_ap.py is this topology with the AP *also* behind a CCB.
# Here the schedd is perfectly dialable, but all attempts to contact the startd
# must go through streaming mode.
#
# The connections under test are daemon-to-daemon and non-blocking.
#

import logging
import re
import time
from collections import namedtuple

from ornithology import action, Condor, ClusterState, write_file, format_script

logger = logging.getLogger(__name__)

TRANSFER_SENTINEL = "ccb-tunnel-public-ap-payload-2c7d45f1"

# The broker logs one of these per relayed hop.
_SESSION_MARK = "started streaming (proxy) session"


def collector_address(condor):
    return condor._get_address_file("COLLECTOR").read_text().splitlines()[0]


def proxy_session_count(condor):
    return condor.collector_log.path.read_text().count(_SESSION_MARK)


# "CCBClient: requesting reverse connection to <peer> via CCB server <addr>#<id>;"
# is the plain (non-streaming) path.  Capture the broker address handed to the
# Daemon object.
_DIRECT_REQUEST_RE = re.compile(
    r"CCBClient: requesting reverse connection to .*? via CCB server (\S+?)#(\d+);"
)

# "CCB: received request id 7 from SCHEDD on <...> for target ccbid 1 (registered
# as COLLECTOR <...>)".  CCBServer logs this only on the non-streaming path (a
# streaming request returns earlier) so a match from one of the AP's
# daemons means the broker is trying a non-streaming path.
_BROKER_SERVED_RE = re.compile(
    r"CCB: received request id \d+ from (SCHEDD|SHADOW|STARTD|TOOL)\b[^\n]*?"
    r" for target ccbid (\d+) \(registered as ([^)]*)\)"
)


@action
def cm(test_dir):
    # Central manager (collector + negotiator) and the public "outside" CCB.
    with Condor(
        local_dir=test_dir / "cm",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
            "NEGOTIATOR_INTERVAL": "5",
        },
    ) as condor:
        yield condor


@action
def inside_ccb(test_dir, cm):
    # The inside broker of the two-CCB tunnel: a collector that registers upstream
    # with the cm's CCB, on its own distinct port.  Daemons that use it as their
    # CCB advertise a contact nested through both brokers.
    with Condor(
        local_dir=test_dir / "inside_ccb",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "CCB_OUTBOUND_NEXT_HOP": collector_address(cm),
            "SEC_USE_FAMILY_SESSION": "False",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def ep(test_dir, cm, inside_ccb):
    # Execute point behind the two-CCB tunnel: the startd's CCB_ADDRESS is the
    # inside CCB (itself reachable only through the cm's CCB), so its advertised
    # contact is doubly nested.  It reports on a network distinct from the AP's.
    with Condor(
        local_dir=test_dir / "ep",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "COLLECTOR_HOST": collector_address(cm),
            "CCB_ADDRESS": collector_address(inside_ccb),
            "PRIVATE_NETWORK_NAME": "NET_EXEC",
            "SEC_USE_FAMILY_SESSION": "False",
            "DAEMON_LIST": "MASTER STARTD",
            "START": "TRUE",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def public_ap(test_dir, cm):
    # The access point under test.  Deliberately NO CCB_ADDRESS and on the same
    # network as the cm.  Streaming is not used by default.
    #
    # D_NETWORK:2 (verbose) puts CCBClient's tracing in the logs.
    with Condor(
        local_dir=test_dir / "public_ap",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "COLLECTOR_HOST": collector_address(cm),
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SEC_USE_FAMILY_SESSION": "False",
            "DAEMON_LIST": "MASTER SCHEDD",
            "SCHEDD_DEBUG": "D_ALWAYS D_FULLDEBUG D_NETWORK:2",
            "SHADOW_DEBUG": "D_ALWAYS D_FULLDEBUG D_NETWORK:2",
        },
    ) as condor:
        yield condor


def startd_address(cm):
    # The startd's contact as the central manager sees it -- what the schedd will
    # dial.  May need to wait: the EP registers with the inside CCB, which must
    # be tunnel-ready first.
    deadline = time.time() + 120
    while time.time() < deadline:
        p = cm.run_command(["condor_status", "-startd", "-af", "MyAddress"])
        if p.returncode == 0 and p.stdout.strip():
            return p.stdout.strip().splitlines()[0]
        time.sleep(1)
    return ""


PublicAPJobResult = namedtuple(
    "PublicAPJobResult",
    ["handle", "completed", "addr", "before_cm", "before_inside_ccb", "sched_log"],
)


@action
def public_ap_job(test_dir, cm, inside_ccb, ep, public_ap):
    # Run a job from the public AP onto the tunneled EP.
    # The fixture does NOT assert completion.
    addr = startd_address(cm)
    assert addr, "tunneled startd never advertised to the central manager"
    assert addr.count("#") >= 2, "startd contact is not doubly nested: %s" % addr

    payload = write_file(test_dir / "public_ap_in.dat", TRANSFER_SENTINEL + "\n")
    exe = write_file(
        test_dir / "public_ap_job.sh",
        format_script("#!/bin/sh\ncat public_ap_in.dat\n"),
    )
    # Baseline each broker's relay count BEFORE the job runs, so the relay test can
    # assert a per-job increase.
    before_cm = proxy_session_count(cm)
    before_inside_ccb = proxy_session_count(inside_ccb)
    sched_log_mark = len(public_ap.schedd_log.path.read_text())

    handle = public_ap.submit(
        description={
            "executable": exe.as_posix(),
            "output": (test_dir / "public_ap_job.out").as_posix(),
            "error": (test_dir / "public_ap_job.err").as_posix(),
            "log": "test_ccb_tunnel_public_ap.log",
            "transfer_input_files": payload.as_posix(),
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "request_memory": "16MB",
            "request_disk": "16MB",
        },
        count=1,
    )
    completed = handle.wait(
        condition=ClusterState.all_complete, timeout=300, verbose=True
    )
    return PublicAPJobResult(
        handle,
        completed,
        addr,
        before_cm,
        before_inside_ccb,
        public_ap.schedd_log.path.read_text()[sched_log_mark:],
    )


class TestPublicAPReachesTunneledEP:
    def test_startd_contact_is_nested(self, public_ap_job):
        # Sanity: the thing under test really is a two-hop tunnel contact.
        assert public_ap_job.addr.count("#") >= 2

    def test_job_ran_from_public_ap_to_tunneled_ep(self, public_ap_job):
        # The job completed: matchmaking, the schedd->startd claim and the
        # shadow<->starter connection all crossed the tunnel.
        assert public_ap_job.completed, (
            "a job from a publicly reachable AP never ran on the tunneled EP "
            "(startd contact %s)" % public_ap_job.addr
        )

    def test_sandbox_delivered(self, public_ap_job, test_dir):
        # File transfer carried the payload back through the tunnel.
        out = test_dir / "public_ap_job.out"
        assert out.exists(), "the job produced no output at all"
        assert TRANSFER_SENTINEL in out.read_text()

    def test_schedd_did_not_hand_a_nested_contact_to_daemon(self, public_ap_job):
        # Pre-fix, the plain reverse-connect path peeled only the innermost id and
        # passes the remainder to Daemon(DT_COLLECTOR, ...).
        bad = [
            broker
            for broker, _id in _DIRECT_REQUEST_RE.findall(public_ap_job.sched_log)
            if "#" in broker
        ]
        assert not bad, (
            "the schedd treated a nested CCB contact as a broker address and asked "
            "for a plain reverse connection via %r; the remaining hop(s) are "
            "silently dropped, so the request goes to the wrong broker" % (bad,)
        )

    def test_entry_broker_did_not_serve_an_inner_hop_itself(self, cm, public_ap_job):
        # The original problem.  Having peeled only the INNERMOST id, the AP sent it
        # to the outside broker, which resolved it against its own server table,
        # potentially sending the client to the wrong daemon.
        served = _BROKER_SERVED_RE.findall(cm.collector_log.path.read_text())
        assert not served, (
            "the entry broker served a plain CCB request for an inner tunnel hop "
            "and rendezvoused the AP with the wrong daemon: %r\n"
            "(requester, ccbid, registered-as)" % (served,)
        )

    def test_both_brokers_relayed_for_the_job(self, cm, inside_ccb, public_ap_job):
        # The job's connections traverse both brokers, so each must log a NEW
        # streaming relay.  A strict increase over the pre-submit baseline.
        assert proxy_session_count(cm) > public_ap_job.before_cm, (
            "cm CCB relayed nothing new for the job (before=%d, after=%d) -- the "
            "job may have used a direct connection"
            % (public_ap_job.before_cm, proxy_session_count(cm))
        )
        assert proxy_session_count(inside_ccb) > public_ap_job.before_inside_ccb, (
            "inside CCB relayed nothing new for the job (before=%d, after=%d) -- "
            "the job may have used a direct connection"
            % (public_ap_job.before_inside_ccb, proxy_session_count(inside_ccb))
        )
