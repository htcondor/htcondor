#!/usr/bin/env pytest

# Additional CCB streaming/proxy coverage for network paths that the core
# test_ccb_streaming.py does not exercise:
#
#   * condor_chirp          -- the running job's chirp/remote-syscall traffic to
#     the shadow, which rides the (streamed) shadow<->starter connection.
#   * job reconnect         -- a fresh shadow reconnecting to a still-running
#     starter, a brand-new private-to-private connection.
#   * condor_ssh_to_job     -- the interactive session reaching the execute side.
#
# (USE_SHARED_PORT coverage lives with the firewalled-tool test in
# test_ccb_streaming.py: under shared port a daemon delegates its CCB listener to
# the condor_shared_port server, so a single pool has no daemon-to-daemon mismatch
# to stream; the shared-port code path that does stream is a firewalled client
# whose shared-port reverse-connect listener cannot be created.)
#
# All three use the same private-to-private topology as test_ccb_streaming.py
# (collector/negotiator public so local tools reach them directly; schedd/shadow
# on NET_SUBMIT and startd/starter on NET_EXEC, each CCB-routed).  Every test
# asserts the broker actually relayed in streaming mode -- the collector-as-CCB
# logs one "started streaming (proxy) session" per relayed connection -- so a
# pass means the path genuinely went through the proxy, not a direct connection.

import logging
import os
import re
import signal
import time
from pathlib import Path

from ornithology import (
    standup,
    action,
    Condor,
    ClusterState,
    write_file,
    format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# The private-to-private topology, shared by every pool below.  USE_SHARED_PORT
# is set per-pool because it is itself one of the variables under test.
STREAMING_CONFIG = {
    # IPv4 only, for a deterministic single-host network setup.
    "ENABLE_IPV6": "FALSE",
    "PRIVATE_NETWORK_NAME": "NET_DEFAULT",
    "SCHEDD.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
    "SHADOW.PRIVATE_NETWORK_NAME": "NET_SUBMIT",
    "STARTD.PRIVATE_NETWORK_NAME": "NET_EXEC",
    "SCHEDD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
    "SHADOW.CCB_ADDRESS": "$(COLLECTOR_HOST)",
    "STARTD.CCB_ADDRESS": "$(COLLECTOR_HOST)",
    "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR STARTD SCHEDD",
    # The broker logs each relay it establishes in its collector log.
    "COLLECTOR_DEBUG": "D_FULLDEBUG",
}

# The broker logs exactly one of these per private-to-private connection it
# relays, so counting them is an immediate, reliable streaming-usage signal
# (no metric-publication delay).
_SESSION_MARK = "started streaming (proxy) session"


def streaming_session_count(condor):
    return condor.collector_log.path.read_text().count(_SESSION_MARK)


def wait_for_new_streaming_session(condor, baseline, timeout=120):
    # Poll the broker log until it relays a session beyond `baseline`, i.e. the
    # action under test opened a new private-to-private connection.
    deadline = time.time() + timeout
    while time.time() < deadline:
        if streaming_session_count(condor) > baseline:
            return True
        time.sleep(2)
    return False


def newest_pid_in_logs(log_dir, glob):
    # DaemonCore logs "** PID = <pid>" at startup; return the most recent one
    # across all matching log files (e.g. the current shadow or starter).
    newest_pid, newest_mtime = None, -1.0
    for path in Path(log_dir).glob(glob):
        pids = re.findall(r"\*\* PID = (\d+)", path.read_text())
        if pids and path.stat().st_mtime > newest_mtime:
            newest_pid, newest_mtime = int(pids[-1]), path.stat().st_mtime
    return newest_pid


def log_dir_of(condor):
    return Path(condor.run_command(["condor_config_val", "LOG"]).stdout.strip())


# ---------------------------------------------------------------------------
# condor_chirp (rides the streamed shadow<->starter connection)
# ---------------------------------------------------------------------------


@standup
def noshared_condor(test_dir):
    # CCB's reverse-connect listener relies on shared port being off (matching
    # the existing lib_ccb_* tests), so the non-shared-port scenarios share one
    # pool with shared port disabled.
    with Condor(
        local_dir=test_dir / "noshared",
        config={**STREAMING_CONFIG, "USE_SHARED_PORT": "FALSE"},
    ) as condor:
        yield condor


@action
def chirp_job(noshared_condor, test_dir):
    # A job that round-trips a value through condor_chirp: set_job_attr writes it
    # to the job ad via the shadow, get_job_attr reads it back -- both over the
    # remote-syscall connection, which here is the streamed shadow<->starter
    # relay.  The value is echoed to stdout (transferred back) for verification.
    # condor_chirp is not on the job's PATH, so invoke it by full path (the
    # execute side shares this host's install).
    libexec = noshared_condor.run_command(
        ["condor_config_val", "LIBEXEC"]
    ).stdout.strip()
    chirp = os.path.join(libexec, "condor_chirp")
    exe = write_file(
        test_dir / "chirp_job.sh",
        format_script(
            """
            #!/bin/sh
            "{chirp}" set_job_attr CCBChirpProof 4242
            echo "chirp_roundtrip=$("{chirp}" get_job_attr CCBChirpProof)"
            """.format(chirp=chirp)
        ),
    )
    handle = noshared_condor.submit(
        description={
            "executable": exe.as_posix(),
            "output": (test_dir / "chirp_job.out").as_posix(),
            "error": (test_dir / "chirp_job.err").as_posix(),
            "log": (test_dir / "chirp_job.log").as_posix(),
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "want_io_proxy": "true",
            "request_memory": "16MB",
            "request_disk": "16MB",
        },
        count=1,
    )
    assert handle.wait(
        condition=ClusterState.all_complete, timeout=240, verbose=True
    )
    return handle


class TestCCBStreamingChirp:
    def test_chirp_round_trip(self, chirp_job, test_dir):
        # Seeing the value come back proves both the set_job_attr and the
        # get_job_attr chirp calls reached the shadow and returned -- the chirp
        # traffic crossed the broker relay (the only shadow<->starter path).
        out = (test_dir / "chirp_job.out").read_text()
        assert "chirp_roundtrip=4242" in out

    def test_broker_streamed_for_chirp_job(self, noshared_condor, chirp_job):
        assert streaming_session_count(noshared_condor) >= 1


# ---------------------------------------------------------------------------
# job reconnect (a fresh shadow reconnects to a live starter)
# ---------------------------------------------------------------------------


@action
def reconnect_job(noshared_condor, path_to_sleep, test_dir):
    condor = noshared_condor
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "120",
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "log": (test_dir / "reconnect.log").as_posix(),
            # Let the starter wait for a reconnect rather than killing the job
            # when its shadow dies.
            "+JobLeaseDuration": "40",
        },
        count=1,
    )
    assert handle.wait(condition=ClusterState.all_running, timeout=120, verbose=True)
    time.sleep(3)  # let the shadow finish establishing/lease-renewing

    before = streaming_session_count(condor)
    shadow_pid = newest_pid_in_logs(log_dir_of(condor), "ShadowLog*")
    assert shadow_pid is not None, "could not find a shadow PID"

    # Kill the shadow (NOT the starter): the schedd spawns a fresh shadow that
    # reconnects to the still-running starter, which is a brand-new private-to-
    # private connection the broker must stream.
    os.kill(shadow_pid, signal.SIGKILL)

    streamed_after_reconnect = wait_for_new_streaming_session(condor, before)
    completed = handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=300,
        verbose=True,
    )
    return {
        "handle": handle,
        "streamed_after_reconnect": streamed_after_reconnect,
        "completed": completed,
    }


class TestCCBStreamingReconnect:
    def test_reconnect_used_streaming(self, reconnect_job):
        # A new streaming session appeared after we killed the shadow, i.e. the
        # replacement shadow reconnected to the starter through the broker.
        assert reconnect_job["streamed_after_reconnect"]

    def test_job_survived_reconnect(self, reconnect_job):
        assert reconnect_job["completed"]


# ---------------------------------------------------------------------------
# condor_ssh_to_job
# ---------------------------------------------------------------------------


@action
def ssh_target_job(noshared_condor, path_to_sleep, test_dir):
    handle = noshared_condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "600",
            "should_transfer_files": "YES",
            "when_to_transfer_output": "ON_EXIT",
            "log": (test_dir / "ssh.log").as_posix(),
        },
        count=1,
    )
    assert handle.wait(condition=ClusterState.all_running, timeout=120, verbose=True)
    return handle


class TestCCBStreamingSSH:
    def test_ssh_to_job_uses_streaming(self, noshared_condor, ssh_target_job):
        condor = noshared_condor
        job_id = "{}.0".format(ssh_target_job.clusterid)

        before = streaming_session_count(condor)
        # We assert on the connection being streamed, not on the remote command:
        # condor_ssh_to_job has to reach the (CCB-routed) execute side to run the
        # command at all, and that reach is the private-to-private connection the
        # broker must relay.  (The trivial remote command's own exit can vary by
        # platform and is not what is under test here.)
        cp = condor.run_command(
            ["condor_ssh_to_job", "-auto-retry", job_id, "/usr/bin/true"],
            timeout=120,
        )
        logger.info("condor_ssh_to_job rc=%s stderr=%s", cp.returncode, cp.stderr)

        assert wait_for_new_streaming_session(condor, before), (
            "condor_ssh_to_job did not establish a streamed connection "
            "(rc=%s, stderr=%s)" % (cp.returncode, cp.stderr)
        )
