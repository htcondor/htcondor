#!/usr/bin/env pytest

# Test CCB streaming/proxy mode, in which a connection that the standard CCB
# connection-reversal scheme cannot make is instead relayed (spliced) through
# the broker.  Two scenarios are covered:
#
#   * Daemon-to-daemon (TestCCBStreaming): the submit side (schedd/shadow) and
#     the execute side (startd) are placed on different CCB-routed private
#     networks, so the shadow->startd connection that carries the job is
#     private-to-private and the broker must proxy it.  A job that runs to
#     completion proves the path worked, and the broker logs the relay.
#
#   * Firewalled tool (TestCCBStreamingTool): a command-line tool that cannot
#     accept a reverse connection -- TOOLS_ASSUME_FIREWALLS is set and it has no
#     writable daemon socket directory for shared port -- reaches a CCB-routed
#     schedd by asking the broker to proxy on its own request socket.
#
# CCB is forced between processes on a single host with the same mechanism as
# the existing lib_ccb_* tests: a mismatch in PRIVATE_NETWORK_NAME.

import logging
import os
import tempfile
import time

import pytest

from ornithology import (
    action,
    Condor,
    ClusterState,
    write_file,
    format_script,
)

logger = logging.getLogger(__name__)


def proxy_session_count(condor):
    # The broker logs one "started streaming (proxy) session" per connection it
    # relays; counting them is an immediate, reliable streaming-usage signal.
    return condor.collector_log.path.read_text().count(
        "started streaming (proxy) session"
    )


def collector_stat(condor, attr):
    # Read a single attribute from the collector's own (self) ad.  Returns the
    # stripped stdout, which is "" when the attribute is not (yet) published.
    p = condor.run_command(["condor_status", "-collector", "-af", attr])
    assert p.returncode == 0, "condor_status %s failed: %s" % (attr, p.stderr)
    return p.stdout.strip()


def wait_for_collector_stat(condor, attr, timeout=60):
    # The CCBStreaming* stats live in the collector process and increment the
    # instant a relay happens, but they only become visible in the collector's
    # advertised self ad when the collector rebuilds it on its periodic self-ad
    # timer (COLLECTOR_UPDATE_INTERVAL; the first publish is ~1s after startup).
    # A firewalled tool can therefore stream successfully -- and the relay be
    # logged -- a beat before the self ad carrying the stat exists, so poll for
    # the published value rather than racing that publish.
    deadline = time.time() + timeout
    while True:
        value = collector_stat(condor, attr)
        if value not in ("", "undefined"):
            return value
        if time.time() >= deadline:
            return value
        time.sleep(1)

# A unique payload the job must receive via HTCondor file transfer and echo back.
TRANSFER_SENTINEL = "ccb-streaming-file-transfer-payload-2f9c1a7b"


@action
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            # IPv4 only, for a deterministic single-host network setup.
            "ENABLE_IPV6": "FALSE",
            # Force CCB between daemons on a single host using the same mechanism
            # as lib_ccb_schedd.run: the collector/negotiator stay on the default
            # network (no CCB) so local tools and pushed updates reach them
            # directly; the submit side (schedd/shadow) is on NET_SUBMIT and
            # CCB-routed, and the execute side (startd) is on NET_EXEC and
            # CCB-routed, so the shadow->startd job connection is private-to-
            # private and must be relayed by the broker.
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
            "SHADOW.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
            "STARTD.PRIVATE_NETWORK_NAME": "NET_EXEC",
            "SCHEDD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            "SHADOW.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            "STARTD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            # CCB's reverse-connect listener relies on the shared port being
            # off (matching the existing lib_ccb_* tests).
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR STARTD SCHEDD",
            # Make the broker's streaming-relay message visible in the collector
            # log (the second test asserts on it).
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def transfer_input_file(test_dir):
    # A real input file the job must receive via HTCondor file transfer.
    return write_file(test_dir / "ccb_input.dat", TRANSFER_SENTINEL + "\n")


@action
def transfer_job_executable(test_dir):
    # The job reads its transferred input file and writes it to stdout (which
    # file transfer brings back to the submit node).  If the input did not
    # arrive, cat fails and the job exits non-zero -- so a clean, content-
    # matching run proves the sandbox transfer completed end to end.
    return write_file(
        test_dir / "ccb_xfer_job.sh",
        format_script(
            """
            #!/bin/sh
            cat ccb_input.dat
            """
        ),
    )


@action
def completed_job(condor, test_dir, transfer_job_executable, transfer_input_file):
    handle = condor.submit(
        description={
            "executable": transfer_job_executable.as_posix(),
            "output": (test_dir / "ccb_xfer_job.out").as_posix(),
            "error": (test_dir / "ccb_xfer_job.err").as_posix(),
            "log": "test_ccb_streaming.log",
            "transfer_input_files": transfer_input_file.as_posix(),
            # Force file transfer even though the daemons share a host (and thus
            # a FileSystemDomain): with should_transfer_files=YES the shadow and
            # starter do a real sandbox transfer over a *separate* connection
            # from the job's syscall socket.  Because the shadow (NET_SUBMIT) and
            # starter (NET_EXEC) are both CCB-routed, that transfer connection is
            # private-to-private and must be relayed by the broker too.
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "request_memory": "16MB",
            "request_disk": "16MB",
        },
        count=1,
    )
    assert handle.wait(
        condition=ClusterState.all_complete,
        timeout=240,
        verbose=True,
    )
    return handle


class TestCCBStreaming:
    def test_job_ran_via_streaming_ccb(self, completed_job):
        # Reaching this point means the private-to-private connection worked.
        assert completed_job.state.all_complete()

    def test_broker_established_streaming_relay(self, condor, completed_job):
        # The collector-as-CCB should have logged a streaming/proxy relay.
        collector_log = condor.collector_log.open()
        assert collector_log.wait(
            condition=lambda line: "streaming (proxy)" in line,
            timeout=60,
        )

    def test_file_transfer_delivered_via_streaming(self, completed_job, test_dir):
        # The job catted its transferred input file to stdout, which file
        # transfer carried back.  Seeing the sentinel confirms the sandbox
        # transfer -- a distinct shadow<->starter connection from the job's
        # syscall socket -- completed over the private-to-private relay.
        out = (test_dir / "ccb_xfer_job.out").read_text()
        assert TRANSFER_SENTINEL in out


@action
def ccb_tool_condor(test_dir):
    # A minimal pool whose schedd is CCB-routed on a private network, so a client
    # on the default network must reach it through the collector-acting-as-CCB.
    with Condor(
        local_dir=test_dir / "tool_condor",
        config={
            "ENABLE_IPV6": "FALSE",
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
            "SCHEDD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR SCHEDD",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


class TestCCBStreamingTool:
    # A firewalled client can be unable to accept a reverse connection in two
    # distinct ways, exercising different client code paths -- both must fall
    # back to asking the broker to stream the connection:
    #
    #   * shared_port=True : the client tries to receive the reverse connection on
    #     a shared-port endpoint, but creating it fails (its DAEMON_SOCKET_DIR is
    #     not writable -- the access(W_OK) check root bypasses).
    #   * shared_port=False: the client would open a plain listen socket, but
    #     TOOLS_ASSUME_FIREWALLS says it cannot accept inbound connections at all.
    @pytest.mark.parametrize("tool_uses_shared_port", [True, False])
    def test_firewalled_tool_reaches_schedd_via_streaming(
        self, ccb_tool_condor, monkeypatch, tool_uses_shared_port
    ):
        # The shared-port variant relies on access(W_OK) failing, which root
        # bypasses, so this unprivileged-client scenario must skip as root.
        if getattr(os, "geteuid", lambda: 1)() == 0:
            pytest.skip("simulating a tool that cannot use shared port requires non-root")

        condor = ccb_tool_condor

        monkeypatch.setenv("_CONDOR_TOOLS_ASSUME_FIREWALLS", "TRUE")
        ro_dir = None
        if tool_uses_shared_port:
            # Point the tool's DAEMON_SOCKET_DIR at an existing read-only directory
            # so its shared-port writability check fails with "cannot write".  The
            # directory must be short: the daemon socket path it stands in for has
            # to fit in a unix-domain sockaddr (~108 chars), and the per-test
            # directory is already long, so use a brief /tmp path.
            ro_dir = tempfile.mkdtemp(prefix="ccbro", dir="/tmp")
            os.chmod(ro_dir, 0o555)
            monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "TRUE")
            monkeypatch.setenv("_CONDOR_DAEMON_SOCKET_DIR", ro_dir)
        else:
            monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "FALSE")

        try:
            before = proxy_session_count(condor)

            # condor_q connects to the (CCB-routed) schedd to read its queue.
            p = condor.run_command(["condor_q"], timeout=60)
            assert p.returncode == 0, "condor_q failed: %s" % p.stderr

            # A new relay appeared for this run: had the tool instead reached the
            # schedd directly, the broker would log no streaming session.
            deadline = time.time() + 60
            while time.time() < deadline and proxy_session_count(condor) <= before:
                time.sleep(1)
            assert proxy_session_count(condor) > before, (
                "firewalled condor_q (tool_uses_shared_port=%s) was not streamed"
                % tool_uses_shared_port
            )

            # The broker's streaming statistics (published in its collector ad)
            # should account for the session and the bytes it relayed.  Wait for
            # the collector to publish the self ad carrying the stat rather than
            # racing that publish; once it appears, all CCBStreaming* attributes
            # are present together.
            sessions = wait_for_collector_stat(condor, "CCBStreamingSessions")
            assert sessions not in ("", "undefined"), (
                "CCBStreamingSessions not published: %r" % sessions
            )
            assert int(sessions) >= 1

            assert int(collector_stat(condor, "CCBStreamingBytes")) > 0

            # The request that produced the session is counted, and an orderly
            # close (EOF) is not counted as a relay failure.
            assert int(collector_stat(condor, "CCBStreamingRequests")) >= 1
            assert int(collector_stat(condor, "CCBStreamingFailed")) == 0
        finally:
            if ro_dir is not None:
                os.chmod(ro_dir, 0o755)
                os.rmdir(ro_dir)


@action
def no_streaming_condor(test_dir):
    # The same private-to-private topology as TestCCBStreaming, but with
    # streaming disabled on the broker.  A CCB-routed requester is refused
    # streaming and then falls back to a plain reverse connection (preserving the
    # pre-streaming behavior); on a single host the endpoints are actually
    # mutually reachable, so the fallback recovers the connection.
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "ENABLE_IPV6": "FALSE",
            # The case under test.
            "CCB_SERVER_STREAMING": "FALSE",
            "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
            "SCHEDD.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
            "SHADOW.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
            "STARTD.PRIVATE_NETWORK_NAME": "NET_EXEC",
            "SCHEDD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            "SHADOW.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            "STARTD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR STARTD SCHEDD",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


class TestCCBStreamingDisabled:
    # Backward compatibility / graceful degradation: a requester that is itself
    # CCB-routed asks a broker that will not proxy because CCB_SERVER_STREAMING is
    # false.  The broker must refuse the streaming request with a clear message
    # and not crash; the requester then falls back to a plain reverse connection,
    # which succeeds here because the single-host endpoints are mutually
    # reachable.  (A genuinely old broker that predates streaming is caught
    # earlier by the requester's pre-send version gate, and falls back the same
    # way.)
    def test_streaming_refused_then_direct_fallback(
        self, no_streaming_condor, path_to_sleep
    ):
        condor = no_streaming_condor
        handle = condor.submit(
            description={
                "executable": path_to_sleep,
                "arguments": "1",
                "request_memory": "16MB",
                "request_disk": "16MB",
                "log": "test_ccb_streaming_disabled.log",
            },
            count=1,
        )

        # The requester is refused streaming and falls back to a direct reverse
        # connection, which recovers the connection here (reachable endpoints),
        # so the job still runs to completion.
        assert handle.wait(
            condition=ClusterState.all_complete,
            timeout=240,
            verbose=True,
        )

        # The broker refused the streaming attempt along the way with a clear
        # message (rather than forwarding a doomed request or faulting).  Read the
        # full log rather than tailing it: the refusal happens early, during the
        # claim, and may be written before we would start watching.
        assert "requires CCB streaming mode" in condor.collector_log.path.read_text()

        # No daemon crashed handling the refused streaming request.
        for daemon_log in (
            condor.collector_log,
            condor.schedd_log,
            condor.startd_log,
            condor.negotiator_log,
        ):
            assert "Stack dump" not in daemon_log.path.read_text()
