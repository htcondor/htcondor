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
# the existing lib_ccb_* tests: a mismatch in PRIVATE_NETWORK_NAME keeps the CCB
# contact (daemon.cpp New_addr), and special_connect() then reverse-connects via
# the broker -- it does not depend on address reachability.

import logging
import os
import tempfile

import pytest

from ornithology import (
    action,
    Condor,
    ClusterState,
    write_file,
    format_script,
)

logger = logging.getLogger(__name__)

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
    # The job can only run if the shadow (NET_SUBMIT) successfully reaches the
    # startd/starter (NET_EXEC) through the streaming proxy -- the two sit on
    # different CCB-routed private networks.
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
    def test_firewalled_tool_reaches_schedd_via_streaming(
        self, ccb_tool_condor, monkeypatch
    ):
        # The trick below makes the tool's shared-port writability check fail via
        # access(W_OK), which root bypasses.  This scenario is specifically about
        # an unprivileged client, so skip when running as root.
        if getattr(os, "geteuid", lambda: 1)() == 0:
            pytest.skip("simulating a tool that cannot use shared port requires non-root")

        condor = ccb_tool_condor

        # Point the tool's DAEMON_SOCKET_DIR at an existing read-only directory so
        # its shared-port writability check fails with "cannot write".  Combined
        # with TOOLS_ASSUME_FIREWALLS, that leaves the tool unable to accept a
        # direct reverse connection, so to reach the CCB-routed schedd it must ask
        # the broker to proxy (stream) the connection on its request socket.  The
        # directory must be short: the daemon socket path it stands in for has to
        # fit in a unix-domain sockaddr (~108 chars), and the per-test directory
        # is already long, so use a brief /tmp path.
        ro_dir = tempfile.mkdtemp(prefix="ccbro", dir="/tmp")
        os.chmod(ro_dir, 0o555)
        try:
            monkeypatch.setenv("_CONDOR_TOOLS_ASSUME_FIREWALLS", "TRUE")
            monkeypatch.setenv("_CONDOR_USE_SHARED_PORT", "TRUE")
            monkeypatch.setenv("_CONDOR_DAEMON_SOCKET_DIR", ro_dir)

            # condor_q connects to the (CCB-routed) schedd to read its queue.
            p = condor.run_command(["condor_q"], timeout=60)
            assert p.returncode == 0

            # The broker must have logged that it proxied the tool's connection;
            # had the tool instead reached the schedd directly, no relay appears.
            collector_log = condor.collector_log.open()
            assert collector_log.wait(
                condition=lambda line: "streaming (proxy)" in line,
                timeout=60,
            )
        finally:
            os.chmod(ro_dir, 0o755)
            os.rmdir(ro_dir)
