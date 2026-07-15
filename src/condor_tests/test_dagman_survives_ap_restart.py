#!/usr/bin/env pytest

# test_dagman_survives_ap_restart.py
#
# Regression test for LIGO: DAGMan must survive the Access Point (the
# schedd's host) being restarted -- gracefully, abruptly ("fast"), or via
# an outright crash -- whether the whole pool is restarted or just the
# schedd is targeted. A DAG interrupted mid-run must still finish
# successfully afterward, without re-running a node that had already
# completed, and without skipping the dependency ordering.
#
# This generalizes the older, graceful-only job_dagman_condor_restart.run
# into a cross-platform matrix that also covers fast-shutdown and crash
# (kill -9) restarts, none of which had regression coverage before.

import pytest
import htcondor2 as htcondor

from ornithology import *

from dagman_recovery_utils import (
    write_attempt_script,
    write_recoverability_dag,
    wait_for_attempt,
    wait_for_all_attempts,
    count_attempts,
    submit_with_retry,
    NEEDS_KILL9,
)

# CRASH mode used to be skipped here: a hard schedd CRASH (SIGKILL, no
# chance for schedd to run any of its own shutdown code) left DAGMan --
# a scheduler-universe job -- running as an orphan, because scheduler/
# local-universe jobs have no reconnect protocol on schedd restart
# (unlike vanilla/java/parallel/vm -- see universeCanReconnect() in
# condor_utils/condor_universe.cpp). The freshly-restarted schedd would
# blindly respawn the same job id, that duplicate would lose the DAG
# lock-file race against the still-alive orphan and exit immediately,
# and the new schedd's own OtherJobRemoveRequirements cleanup would then
# remove the DAG's node jobs out from under the orphan.
#
# Fixed upstream: the duplicate now recognizes the lock is held by this
# same job (via a cluster id stamped in the lock file, dagman_main.cpp)
# and exits EXIT_RESTART (stays queued) instead of EXIT_ERROR (removed);
# and DAGMan's own orphan self-detection (check_parent(), tunable via
# PARENT_CHECK_INTERVAL) can now be sped up per-test so the real orphan
# exits promptly instead of racing the respawn at all.
RESTART_CASES = [
    pytest.param(RestartMode.GRACEFUL, "pool", id="pool-graceful"),
    pytest.param(RestartMode.FAST, "pool", id="pool-fast"),
    pytest.param(
        RestartMode.CRASH, "pool", id="pool-crash",
        marks=NEEDS_KILL9,
    ),
    pytest.param(RestartMode.GRACEFUL, "schedd", id="schedd-graceful"),
    pytest.param(RestartMode.FAST, "schedd", id="schedd-fast"),
    pytest.param(
        RestartMode.CRASH, "schedd", id="schedd-crash",
        marks=NEEDS_KILL9,
    ),
]


# --------------------------------------------------------------------------
# Function-scoped (not @standup's class scope) deliberately: this fixture
# is shared across every parametrized restart case, and a class-scoped
# pool would let one case's daemons (e.g. an orphan left behind by a
# still-in-progress self-shutdown) linger into the next, unrelated case --
# exactly how the shared_port ad-file-deletion bug this suite's fix
# addresses was originally found. Isolating each case into its own fresh
# pool costs more standup time but removes that cross-case interference
# entirely rather than just narrowing the window for it.
@pytest.fixture
def condor(test_dir):
    # DAGMan is itself a scheduler-universe job: any restart that evicts it
    # (or kills it) makes it exit non-zero, which by default puts it in a
    # 5-minute cooldown (SCHEDULER_UNIVERSE_COOL_DOWN_DURATION) before the
    # schedd will resubmit it -- disable that so recovery happens promptly,
    # the same fix job_dagman_condor_restart.run/job_dagman_large_dag.run use.
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SCHEDULER_UNIVERSE_COOL_DOWN_DURATION": 0,
            # Speed up DAGMan's own orphan self-detection (normally a
            # generic, shared 15s/120s DaemonCore cadence -- see
            # daemon_core_main.cpp's check_parent()) so a crashed/restarted
            # schedd is noticed quickly instead of after up to ~135s.
            "DAGMAN.PARENT_CHECK_FIRST_INTERVAL": 2,
            "DAGMAN.PARENT_CHECK_INTERVAL": 2,
            # This test restarts daemons in every parametrized case;
            # master's default backoff before respawning an exited daemon
            # (MASTER_BACKOFF_CONSTANT, default 9 -- see masterDaemon.cpp)
            # adds a flat ~10s per restart unrelated to anything this test
            # is exercising. Clamped to a minimum of 1 by param_integer().
            "MASTER_BACKOFF_CONSTANT": 1,
        },
    ) as condor:
        yield condor


# --------------------------------------------------------------------------
class TestDAGManSurvivesAPRestart:
    @pytest.mark.parametrize("mode, target", RESTART_CASES)
    def test_dag_completes_after_restart(self, condor, test_dir, mode, target):
        attempts_log = test_dir / f"attempts-{target}-{mode.value}.log"
        script = test_dir / f"attempt-{target}-{mode.value}.py"
        write_attempt_script(script, attempts_log)

        # B blocks until this file appears, instead of sleeping a fixed
        # duration -- so it's guaranteed still running (or blocked, if
        # this restart mode doesn't actually interrupt it) no matter how
        # long the restart itself takes on a given machine. We create it
        # ourselves only once the restart has definitely landed.
        proceed_file = test_dir / f"b-proceed-{target}-{mode.value}"

        dag_path = test_dir / f"test-{target}-{mode.value}.dag"
        write_recoverability_dag(dag_path, script, long_node_proceed_file=proceed_file)

        dag = htcondor.Submit.from_dag(str(dag_path))
        dagman_job = submit_with_retry(condor, dag)

        # Wait until node B is actually running before interrupting anything.
        wait_for_attempt(attempts_log, "B", timeout=120)

        if target == "pool":
            condor.restart(mode=mode, timeout=120)
        else:
            condor.restart_daemon("schedd", mode=mode, timeout=120)

        # Let B (or its post-interruption replacement) actually finish now
        # that the restart has landed.
        proceed_file.touch()

        # Ground truth, not condor.submit()'s ClusterState: after a CRASH,
        # the *new* schedd's view of the submitted DAGMan job can go
        # "complete" (with a failure) almost immediately -- from a
        # duplicate attempt racing the still-alive orphaned original --
        # while the real DAG keeps running, invisibly, in that orphan. So
        # wait on what actually ran, not on the schedd's job-queue view.
        wait_for_all_attempts(attempts_log, ("A", "B", "C"), timeout=300)

        # Node A finished before the interruption -- it must not re-run.
        assert count_attempts(attempts_log, "A") == 1
        # Node C only becomes eligible once B has *really* terminated, so
        # its presence (and count) verifies dependency ordering held.
        assert count_attempts(attempts_log, "C") == 1
        # Node B was caught mid-run, so re-running it (from scratch, since
        # it wasn't checkpointed) is expected/correct recovery behavior.
        assert count_attempts(attempts_log, "B") >= 1
