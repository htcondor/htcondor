#!/usr/bin/env pytest

# Test a job whose AP (submit side) is behind a *single* CCB (relay/streaming mode)
# and whose EP (execute side) is behind a *two-CCB tunnel*.  Both endpoints are
# private, so the connections between them go through a broker in *both* directions:
#
#   central manager + outside CCB (pool "cm") --- collector + negotiator, public
#        ^                     ^                      ^
#        | schedd registers    | inside CCB           | startd + schedd report here
#        | (1 hop: relay mode)  | registers upstream   |
#   AP: schedd (pool "relay_ap")   inside CCB (pool "inside_ccb")
#        CCB_ADDRESS = cm               ^  CCB_OUTBOUND_NEXT_HOP = cm
#        private (NET_AP)               | startd's CCB_ADDRESS is the inside CCB, so
#                                       | its contact is doubly nested (2-CCB tunnel)
#                               startd (pool "ep"), private (NET_EXEC)
#
# The negotiator (on cm) matches the tunneled startd to the relayed schedd; the
# schedd then claims the startd (AP -> tunneled EP) and the shadow talks to the
# starter (AP <-> EP).  Because the schedd/shadow are themselves behind a CCB, the
# EP cannot reverse-connect to them directly and must stream through the AP's CCB;
# because the startd/starter are tunneled, the AP reaches them through both brokers.
# A clean, content-matching job run proves both directions work.

import logging
import time

from ornithology import action, Condor, ClusterState, write_file, format_script

logger = logging.getLogger(__name__)

TRANSFER_SENTINEL = "ccb-tunnel-ap-payload-9f3c1b7e"


def collector_address(condor):
    return condor._get_address_file("COLLECTOR").read_text().splitlines()[0]


def proxy_session_count(condor):
    return condor.collector_log.path.read_text().count(
        "started streaming (proxy) session"
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
            # Matchmaking must not require the private-net-only shortcut.
            "NEGOTIATOR_INTERVAL": "5",
        },
    ) as condor:
        yield condor


@action
def inside_ccb(test_dir, cm):
    # The inside broker of the two-CCB tunnel: a collector that registers upstream
    # with the cm's CCB (CCB_OUTBOUND_NEXT_HOP), on its own distinct port.  Daemons
    # that use it as their CCB advertise a contact nested through both brokers.  It
    # is a broker only, not the pool's central manager.
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
    # Execute point behind the two-CCB tunnel: the startd's CCB_ADDRESS is the inside
    # CCB (which is itself reachable only through the cm's CCB), so the startd's
    # advertised contact is doubly nested.  It reports to the cm collector for
    # matchmaking, on a private network distinct from the AP's.
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
def relay_ap(test_dir, cm):
    # Access point behind a single CCB (relay mode): the schedd's CCB_ADDRESS is the
    # cm's CCB, and it is on a private network distinct from the startd's, so the
    # startd/starter cannot reverse-connect to it directly and must stream through
    # the CCB.  It reports to the cm collector.
    with Condor(
        local_dir=test_dir / "relay_ap",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "COLLECTOR_HOST": collector_address(cm),
            "CCB_ADDRESS": collector_address(cm),
            "PRIVATE_NETWORK_NAME": "NET_AP",
            "SEC_USE_FAMILY_SESSION": "False",
            "DAEMON_LIST": "MASTER SCHEDD",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def ap_job(test_dir, cm, inside_ccb, ep, relay_ap):
    payload = write_file(test_dir / "ap_in.dat", TRANSFER_SENTINEL + "\n")
    exe = write_file(
        test_dir / "ap_job.sh",
        format_script("#!/bin/sh\ncat ap_in.dat\n"),
    )
    handle = relay_ap.submit(
        description={
            "executable": exe.as_posix(),
            "output": (test_dir / "ap_job.out").as_posix(),
            "error": (test_dir / "ap_job.err").as_posix(),
            "log": "test_ccb_tunnel_ap.log",
            "transfer_input_files": payload.as_posix(),
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "request_memory": "16MB",
            "request_disk": "16MB",
        },
        count=1,
    )
    assert handle.wait(condition=ClusterState.all_complete, timeout=360, verbose=True)
    return handle


class TestCCBTunnelRelayAP:
    def test_job_ran_relay_ap_to_tunneled_ep(self, ap_job):
        # The job completed: matchmaking, the schedd->startd claim (AP -> tunneled
        # EP), and the shadow<->starter connection (private AP <-> tunneled EP) all
        # succeeded across the tunnel and the relay.
        assert ap_job.state.all_complete()

    def test_sandbox_delivered(self, ap_job, test_dir):
        # File transfer -- a shadow<->starter connection -- carried the payload back
        # through the tunnel/relay.
        assert TRANSFER_SENTINEL in (test_dir / "ap_job.out").read_text()

    def test_brokers_relayed(self, cm, inside_ccb, ap_job):
        # Both the cm's CCB and the inside CCB relayed at least one streaming
        # connection for the job (the tunneled/relayed daemon-to-daemon traffic).
        assert proxy_session_count(cm) > 0, "cm CCB relayed nothing"
        assert proxy_session_count(inside_ccb) > 0, "inside CCB relayed nothing"
