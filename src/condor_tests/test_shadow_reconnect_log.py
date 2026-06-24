#!/usr/bin/env pytest

#
# Test that the shadow writes a reconnect record to the ShadowReconnectLog
# CSV file when a reconnect attempt fails because the starter is dead.
#
# Strategy:
#   1. Start a personal condor with SHADOW_LOG_RECONNECT enabled
#   2. Submit a long sleep job with a job lease duration
#   3. Wait for it to start running
#   4. Kill the starter process to break the shadow-starter connection
#   5. The shadow detects the broken connection and attempts to reconnect
#   6. The startd reports the starter is gone, so reconnect fails
#   7. The shadow logs a CSV record to ShadowReconnectLog before exiting
#   8. The schedd requeues the job which runs to completion
#

import os
import re
import signal
import time
import logging

from pathlib import Path

from ornithology import (
    standup,
    action,
    Condor,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SHADOW_LOG_RECONNECT": "true",
            "SHADOW_DEBUG": "D_FULLDEBUG",
            "STARTER_DEBUG": "D_FULLDEBUG",
            "NEGOTIATOR_INTERVAL": "2",
            "STARTER_UPDATE_INTERVAL": "5",
        },
    ) as condor:
        yield condor


@action
def reconnect_log_path(condor):
    """Get the path to the ShadowReconnectLog file."""
    p = condor.run_command(["condor_config_val", "LOG"])
    return Path(p.stdout.strip()) / "ShadowReconnectLog"


@action
def completed_job(condor, test_dir, path_to_sleep):
    """Submit a job, kill its starter to trigger a failed reconnect,
    then let the job be requeued and run to completion."""

    # Sleep long enough that the starter is guaranteed to still be alive
    # by the time we look up its PID and SIGKILL it. On slow runners (e.g.
    # ppc64le) the sequence "wait-for-running -> sleep(3) -> condor_config_val
    # LOG -> read StarterLog -> os.kill" can take well over 5s, so a
    # short-sleep job will exit on its own and the kill fails with ESRCH.
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "60",
            "log": (test_dir / "job.log").as_posix(),
            "+JobLeaseDuration": "40",
        },
    )

    # Wait for the job to start running
    assert handle.wait(
        condition=ClusterState.all_running,
        timeout=60,
        verbose=True,
    )

    # Give the shadow time to establish contact and do a lease renewal
    time.sleep(3)

    # Find the starter PID from the StarterLog for this personal condor.
    # The starter log has a slot suffix (e.g. StarterLog.slot1_1),
    # so glob for all StarterLog* files in the log directory.
    # DaemonCore logs "** PID = <pid>" at startup.
    result = condor.run_command(["condor_config_val", "LOG"])
    log_dir = Path(result.stdout.strip())
    starter_logs = sorted(log_dir.glob("StarterLog.slot*"))
    assert len(starter_logs) > 0, f"No StarterLog.slot* files found in {log_dir}"
    starter_pid = None
    for slog in starter_logs:
        text = slog.read_text()
        pid_matches = re.findall(r"\*\* PID = (\d+)", text)
        if pid_matches:
            starter_pid = int(pid_matches[-1])
            logger.info(f"Found starter PID {starter_pid} from {slog}")
            break
    assert starter_pid is not None, "Could not find starter PID in any StarterLog"

    # Kill the starter to break the shadow-starter connection.
    # The shadow will detect the broken socket, attempt to reconnect,
    # and the startd will report the starter is gone (CA_FAILURE),
    # triggering reconnectFailed() with starter_known_dead=true.
    os.kill(starter_pid, signal.SIGKILL)

    # The job should be requeued and eventually re-run to completion.
    # Allowance: JobLeaseDuration (40s) for the shadow to give up, plus
    # re-match + a fresh 60s sleep on a slow runner.
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=240,
        verbose=True,
    )

    return handle


class TestShadowReconnectLog:

    def test_reconnect_log_exists(self, completed_job, reconnect_log_path):
        """Verify the ShadowReconnectLog file was created."""
        assert reconnect_log_path.exists(), \
            f"ShadowReconnectLog not found at {reconnect_log_path}"

    def test_reconnect_log_has_record(self, completed_job, reconnect_log_path):
        """Verify the log contains a valid CSV failed-reconnect record."""
        content = reconnect_log_path.read_text()
        lines = [l.strip() for l in content.splitlines() if l.strip()]
        logger.info(f"ShadowReconnectLog contents:\n{content}")

        assert len(lines) >= 1, "ShadowReconnectLog should have at least one record"

        # Parse the CSV line: ID,T1,T2,B1,T3,T4,ID1,B2
        fields = lines[0].split(",")
        assert len(fields) == 8, f"Expected 8 CSV fields, got {len(fields)}: {lines[0]}"

        job_id = fields[0]   # cluster.proc
        t1 = int(fields[1])  # activation time
        t2 = int(fields[2])  # last contact time
        b1 = fields[3]       # reconnect succeeded
        t3 = int(fields[4])  # reconnect time
        t4 = int(fields[5])  # lease duration
        # fields[6] is ID1 (timeout version, may be empty)
        b2 = fields[7]       # starter known dead

        # T1 should be a recent epoch time (within last 2 minutes)
        now = int(time.time())
        assert now - t1 < 120, f"T1 ({t1}) should be recent"

        # T2 should be >= T1
        assert t2 >= t1, f"T2 ({t2}) should be >= T1 ({t1})"

        # B1 should be false (reconnect failed because starter was killed)
        assert b1 == "false", f"B1 should be 'false', got '{b1}'"

        # T4 should be the job lease duration we set (40)
        assert t4 == 40, f"T4 should be 40, got {t4}"

        # B2 should be true (starter is known to be dead)
        assert b2 == "true", f"B2 should be 'true', got '{b2}'"
