#!/usr/bin/env pytest

# test_dagman_node_status_recovery.py
#
# LIGO's own monitoring tooling polls DAGMan's NODE_STATUS_FILE to track
# workflow progress. A DAG that *eventually* finishes correctly isn't good
# enough if that file reports a stale or wrong snapshot immediately after
# an AP restart -- this checks the status file stays truthful right after
# each restart, not just once the whole DAG is done.

import time

import pytest
import htcondor2 as htcondor

from ornithology import *

from dagman_recovery_utils import (
    write_attempt_script,
    write_recoverability_dag,
    count_attempts,
    wait_for_attempt,
    wait_for_all_attempts,
    submit_with_retry,
    NEEDS_KILL9,
)

# NodeStatus codes, per docs/automated-workflows/dagman-information-files.rst
STATUS_DONE = 5

# CRASH mode used to be skipped here (see test_dagman_survives_ap_restart.py
# for the full history): a hard schedd CRASH could orphan DAGMan and the
# freshly-restarted schedd's blind respawn-then-remove-children race
# against that orphan could remove the DAG's node jobs. Fixed upstream --
# see test_dagman_survives_ap_restart.py's RESTART_CASES comment.
RESTART_CASES = [
    pytest.param(RestartMode.GRACEFUL, id="graceful"),
    pytest.param(
        RestartMode.CRASH, id="crash",
        marks=NEEDS_KILL9,
    ),
]


def parse_node_statuses(status_path):
    """{node_name: NodeStatus code} for every NodeStatus ClassAd currently in the file."""
    statuses = {}
    current_node = None
    try:
        with open(status_path) as f:
            lines = f.readlines()
    except FileNotFoundError:
        return statuses

    for line in lines:
        line = line.strip()
        if line.startswith("Node ="):
            current_node = line.split("=", 1)[1].strip().strip('";')
        elif line.startswith("NodeStatus =") and current_node is not None:
            # e.g. 'NodeStatus = 5; /* "STATUS_DONE" */' -- take just the
            # leading integer, ignoring the trailing comment.
            value = line.split("=", 1)[1].split(";", 1)[0].strip()
            statuses[current_node] = int(value)
            current_node = None

    return statuses


def write_status_dag(dag_path, script_path, status_path, long_node_proceed_file):
    write_recoverability_dag(dag_path, script_path, long_node_proceed_file=long_node_proceed_file)
    with open(dag_path, "a") as f:
        f.write(f"\nNODE_STATUS_FILE {status_path} 1 ALWAYS-UPDATE\n")


# --------------------------------------------------------------------------
# Function-scoped (not @standup's class scope) -- see
# test_dagman_survives_ap_restart.py: isolates each parametrized restart
# case into its own fresh pool instead of sharing one across both cases.
@pytest.fixture
def condor(test_dir):
    # See test_dagman_survives_ap_restart.py: without this, a restarted
    # DAGMan (a scheduler-universe job) sits in a 5-minute cooldown before
    # the schedd will resubmit it -- shrunk, not zeroed, so it doesn't also
    # remove the only throttle on the duplicate DAGMan a restarted schedd
    # spawns for the same job id (see test_dagman_survives_ap_restart.py).
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SCHEDULER_UNIVERSE_COOL_DOWN_DURATION": 3,
            # See test_dagman_survives_ap_restart.py: speed up DAGMan's own
            # orphan self-detection so a crashed/restarted schedd is
            # noticed quickly instead of after up to ~135s.
            "DAGMAN.PARENT_CHECK_FIRST_INTERVAL": 2,
            "DAGMAN.PARENT_CHECK_INTERVAL": 2,
            # See test_dagman_survives_ap_restart.py: master's default
            # per-restart backoff adds a flat ~10s unrelated to this test.
            "MASTER_BACKOFF_CONSTANT": 1,
            # See test_dagman_survives_ap_restart.py: widen DAGMan's
            # node-submit retry budget so a schedd restart that's slower
            # than usual under CI load doesn't cause a premature give-up.
            "DAGMAN_MAX_SUBMIT_ATTEMPTS": 16,
            # See test_dagman_survives_ap_restart.py: shrink the schedd's
            # own ad-file-refresh cadence (both, since SCHEDD_MIN_INTERVAL
            # floors SCHEDD_INTERVAL) so a restarted schedd's address info
            # doesn't stay stale for minutes at a time.
            "SCHEDD_INTERVAL": 1,
            "SCHEDD_MIN_INTERVAL": 1,
        },
    ) as condor:
        yield condor


# --------------------------------------------------------------------------
class TestDAGManNodeStatusSurvivesRestart:
    @pytest.mark.parametrize("mode", RESTART_CASES)
    def test_status_file_stays_truthful_across_restart(self, condor, test_dir, mode):
        attempts_log = test_dir / f"attempts-{mode.value}.log"
        script = test_dir / f"attempt-{mode.value}.py"
        write_attempt_script(script, attempts_log)

        # See test_dagman_survives_ap_restart.py: B blocks on this file
        # instead of a fixed sleep, so it's guaranteed still blocked (not
        # racing however long the restart itself takes) when we check the
        # status file immediately after the restart below.
        proceed_file = test_dir / f"b-proceed-{mode.value}"

        dag_path = test_dir / f"test-{mode.value}.dag"
        status_path = test_dir / f"test-{mode.value}.status"
        write_status_dag(dag_path, script, status_path, proceed_file)

        dag = htcondor.Submit.from_dag(str(dag_path))
        dagman_job = submit_with_retry(condor, dag)

        # Wait until node A is reported DONE and B is running, before
        # interrupting anything.
        wait_for_attempt(attempts_log, "B", timeout=120)
        deadline = time.time() + 120
        statuses = {}
        while time.time() < deadline:
            statuses = parse_node_statuses(status_path)
            if statuses.get("A") == STATUS_DONE:
                break
            time.sleep(1)
        assert statuses.get("A") == STATUS_DONE
        assert statuses.get("C") != STATUS_DONE

        condor.restart(mode=mode, timeout=120)

        # Immediately after the restart -- well before the DAG as a whole
        # finishes -- the status file must still agree with ground truth:
        # A is still reported done, and C has definitely not (falsely)
        # become done. B is still reliably blocked on proceed_file here
        # (not yet created), regardless of restart mode or machine speed.
        statuses = parse_node_statuses(status_path)
        assert statuses.get("A") == STATUS_DONE
        assert statuses.get("C") != STATUS_DONE

        # Let B (or its post-interruption replacement) actually finish now
        # that the above has been checked.
        proceed_file.touch()

        # Ground truth, not ClusterState.all_complete on the submitted job
        # handle -- see test_dagman_survives_ap_restart.py for why: after a
        # CRASH, the new schedd's view of that job can go "complete" (with
        # a failure, from a duplicate attempt racing the still-alive
        # orphaned original) long before the real DAG, running invisibly
        # in that orphan, actually finishes.
        wait_for_all_attempts(attempts_log, ("A", "B", "C"), timeout=300)

        # The node-status file only updates on its own ~1s cadence, so
        # give it a moment to catch up with the ground truth above.
        deadline = time.time() + 120
        final_statuses = {}
        while time.time() < deadline:
            final_statuses = parse_node_statuses(status_path)
            if all(final_statuses.get(n) == STATUS_DONE for n in ("A", "B", "C")):
                break
            time.sleep(1)
        assert final_statuses.get("A") == STATUS_DONE
        assert final_statuses.get("B") == STATUS_DONE
        assert final_statuses.get("C") == STATUS_DONE

        assert count_attempts(attempts_log, "A") == 1
        assert count_attempts(attempts_log, "C") == 1
