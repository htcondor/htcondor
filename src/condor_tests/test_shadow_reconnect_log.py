#!/usr/bin/env pytest

#
# Test that the shadow writes a reconnect record to the ShadowReconnectLog
# CSV file, both when a reconnect attempt fails because the starter is dead,
# and when a replacement shadow successfully reconnects to a live starter.
#
# Failed-reconnect strategy:
#   1. Start a personal condor with SHADOW_LOG_RECONNECT enabled
#   2. Submit a long sleep job with a job lease duration
#   3. Wait for it to start running
#   4. Kill the starter process to break the shadow-starter connection
#   5. The shadow detects the broken connection and attempts to reconnect
#   6. The startd reports the starter is gone, so reconnect fails
#   7. The shadow logs a CSV record to ShadowReconnectLog before exiting
#   8. The schedd requeues the job which runs to completion
#
# Successful-reconnect strategy:
#   1. Submit a second long sleep job with a job lease duration
#   2. Once it is running, SIGSTOP its shadow, wait, then fast-restart
#      the schedd (which SIGKILLs the shadow).  Merely killing the shadow
#      is not enough: a running schedd treats that as a shadow crash and
#      tries to start the job afresh rather than reconnecting.
#   3. The restarted schedd spawns a shadow in reconnect mode, which
#      reconnects to the still-running starter and logs a successful
#      CSV record
#   4. T2 (last contact) must predate the shadow being stopped, and T3
#      (reconnect time) must follow the kill.  A regression once made
#      T2 always equal to T3 on success.
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
    RestartMode,
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


@action
def reconnected_job(condor, test_dir, path_to_sleep):
    """Submit a job, fast-restart the schedd so that a new shadow
    successfully reconnects to the live starter, and let the job run
    to completion."""

    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "60",
            "log": (test_dir / "reconnect_job.log").as_posix(),
            "+JobLeaseDuration": "40",
        },
    )

    assert handle.wait(
        condition=ClusterState.all_running,
        timeout=60,
        verbose=True,
    )

    # Let the shadow finish activating the claim and pushing its first
    # queue update (which includes LastJobLeaseRenewal).
    time.sleep(3)

    # Every ShadowLog line for a job is prefixed "(cluster.proc) (pid):",
    # so the most recent such line names this job's current shadow.
    log_dir = Path(
        condor.run_command(["condor_config_val", "LOG"]).stdout.strip()
    )
    job_id = f"{handle.clusterid}.0"
    shadow_pids = re.findall(
        rf"\({re.escape(job_id)}\) \((\d+)\):",
        (log_dir / "ShadowLog").read_text(),
    )
    assert shadow_pids, f"Could not find a shadow PID for job {job_id}"
    shadow_pid = int(shadow_pids[-1])
    logger.info(f"Found shadow PID {shadow_pid} for job {job_id}")

    # Freeze the shadow first, so it cannot record any further contact
    # with the starter, then wait so that the reconnect clearly happens
    # later than that last contact.  Then fast-restart the schedd, which
    # SIGKILLs the (stopped) shadow; the new schedd starts a shadow in
    # reconnect mode because the job's lease has not expired.
    stop_time = int(time.time())
    os.kill(shadow_pid, signal.SIGSTOP)
    time.sleep(3)
    kill_time = int(time.time())
    condor.restart_daemon("schedd", mode=RestartMode.FAST)

    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=180,
        verbose=True,
    )

    return {
        "job_id": job_id,
        "stop_time": stop_time,
        "kill_time": kill_time,
    }


def records_for(log_path, job_id):
    """Return the parsed CSV records in log_path for the given job id."""
    records = []
    for line in log_path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        fields = line.split(",")
        assert len(fields) == 8, f"Expected 8 CSV fields, got {len(fields)}: {line}"
        if fields[0] != job_id:
            continue
        records.append({
            "t1": int(fields[1]),   # activation time
            "t2": int(fields[2]),   # last contact time
            "b1": fields[3],        # reconnect succeeded
            "t3": int(fields[4]),   # reconnect time
            "t4": int(fields[5]),   # lease duration
            "id2": fields[6],       # timeout version id
            "b2": fields[7],        # starter known dead
        })
    return records


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

        # The log also holds the successful-reconnect scenario's record,
        # so pick out this job's record.
        job_id = f"{completed_job.clusterid}.0"
        job_lines = [l for l in lines if l.split(",")[0] == job_id]
        assert len(job_lines) >= 1, f"No record for job {job_id}"

        # Parse the CSV line: ID,T1,T2,B1,T3,T4,ID1,B2
        fields = job_lines[0].split(",")
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


class TestShadowReconnectLogSuccess:

    def test_successful_reconnect_record(self, reconnected_job, reconnect_log_path):
        """Verify a successful reconnect logs a record whose last-contact
        time (T2) predates the disconnect, rather than equalling the
        reconnect time (T3)."""
        logger.info(f"ShadowReconnectLog contents:\n{reconnect_log_path.read_text()}")

        records = records_for(reconnect_log_path, reconnected_job["job_id"])
        assert len(records) == 1, \
            f"Expected exactly one record for job {reconnected_job['job_id']}, got {records}"
        r = records[0]

        # The replacement shadow reconnected to a live starter.
        assert r["b1"] == "true", f"B1 should be 'true', got '{r['b1']}'"
        assert r["b2"] == "false", f"B2 should be 'false', got '{r['b2']}'"
        assert r["t4"] == 40, f"T4 should be 40, got {r['t4']}"

        # T1 <= T2: last contact cannot precede activation.
        assert r["t1"] > 0, f"T1 should be set, got {r['t1']}"
        assert r["t2"] >= r["t1"], f"T2 ({r['t2']}) should be >= T1 ({r['t1']})"

        # T2 is the last contact before the disconnect, so it must not be
        # later than when we froze the shadow; T3 must follow the kill.
        assert r["t2"] <= reconnected_job["stop_time"], \
            f"T2 ({r['t2']}) should be <= shadow stop time ({reconnected_job['stop_time']})"
        assert r["t3"] >= reconnected_job["kill_time"], \
            f"T3 ({r['t3']}) should be >= shadow kill time ({reconnected_job['kill_time']})"
        assert r["t3"] > r["t2"], f"T3 ({r['t3']}) should be > T2 ({r['t2']})"
