#!/usr/bin/env pytest

# test_dagman_survives_double_restart.py
#
# TestDAGManSurvivesDoubleRestart complements test_dagman_killed_during_
# recovery.py's deterministic, synthetic-log version of "DAGMan gets
# interrupted a second time while still recovering from the first
# interruption": this drives the same nested-interruption scenario with
# two *real* restarts back to back, using ordinary timing rather than a
# stretched recovery log, as an "organic" check that doesn't depend on
# the synthetic-log mechanism.
#
# TestDAGManSurvivesBackToBackRestarts covers the remaining GRACEFUL/FAST
# combinations, which can't land a genuine interruption (see
# BACK_TO_BACK_RESTART_CASES below) but still verify two administrative
# restarts in a row never trip up DAGMan on their own.

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

# CRASH must come *first* in each pair: it's the only mode of the three
# that actually interrupts an in-flight local-universe node job (it
# SIGKILLs startd -- see Condor._crash() -- forcibly evicting whatever's
# running under it). GRACEFUL/FAST restarts don't touch startd this way,
# so a node already running (e.g. B, blocked on its proceed file -- see
# write_recoverability_dag()) just keeps running, undisturbed, no matter
# how many GRACEFUL/FAST restarts happen around it. Putting CRASH second
# (e.g. graceful-then-crash) means the first restart never interrupts
# anything, the DAG can complete during it, and there's nothing left
# running for the second restart to land on
# -- defeating the point of this test. Putting CRASH first guarantees a
# genuine interruption to recover from, then the second restart (any
# mode) tests surviving a restart landing during that recovery.
DOUBLE_RESTART_CASES = [
    pytest.param(
        RestartMode.CRASH, RestartMode.GRACEFUL, id="crash-then-graceful",
        marks=NEEDS_KILL9,
    ),
    pytest.param(
        RestartMode.CRASH, RestartMode.FAST, id="crash-then-fast",
        marks=NEEDS_KILL9,
    ),
]

# The remaining four combinations -- neither mode ever CRASH -- can't
# exercise "recover from a second interruption while still recovering
# from the first" (see above: GRACEFUL/FAST never interrupt B at all).
# What they *do* cover, explicitly, is that two purely administrative
# restarts back to back -- neither of which touches the running DAG's
# job processes -- never trip up DAGMan on their own (e.g. via the
# schedd respawning it unnecessarily, lock-file contention, or
# address-resolution flakiness), even though there's no interruption
# for it to actually recover from. Kept in a separate test class (see
# TestDAGManSurvivesBackToBackRestarts) since the assertions differ:
# there's no guaranteed second run of B to wait for or require.
BACK_TO_BACK_RESTART_CASES = [
    pytest.param(RestartMode.GRACEFUL, RestartMode.GRACEFUL, id="graceful-then-graceful"),
    pytest.param(RestartMode.GRACEFUL, RestartMode.FAST, id="graceful-then-fast"),
    pytest.param(RestartMode.FAST, RestartMode.GRACEFUL, id="fast-then-graceful"),
    pytest.param(RestartMode.FAST, RestartMode.FAST, id="fast-then-fast"),
]


# --------------------------------------------------------------------------
# Function-scoped (not @standup's class scope) -- see
# test_dagman_survives_ap_restart.py: isolates each parametrized restart
# case (across both classes below) into its own fresh pool.
@pytest.fixture
def condor(test_dir):
    # See test_dagman_survives_ap_restart.py: without this, a restarted
    # DAGMan (a scheduler-universe job) sits in a 5-minute cooldown before
    # the schedd will resubmit it -- twice as costly here, since this test
    # restarts the pool twice. Shrunk, not zeroed, so it doesn't also remove
    # the only throttle on the duplicate DAGMan a restarted schedd spawns
    # for the same job id (see test_dagman_survives_ap_restart.py).
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SCHEDULER_UNIVERSE_COOL_DOWN_DURATION": 3,
            # See test_dagman_survives_ap_restart.py: speed up DAGMan's own
            # orphan self-detection so a crashed/restarted schedd is
            # noticed quickly instead of after up to ~135s.
            "DAGMAN.PARENT_CHECK_FIRST_INTERVAL": 2,
            "DAGMAN.PARENT_CHECK_INTERVAL": 2,
            # Same reasoning, for shared_port: CRASH mode deliberately
            # doesn't kill it, so an orphaned instance from an earlier
            # restart otherwise lingers for up to ~135s (the default)
            # before noticing its parent is gone and self-shutting-down --
            # a real window for it to collide with a later case's freshly
            # started instance at the same SHARED_PORT_DAEMON_AD_FILE path.
            "SHARED_PORT.PARENT_CHECK_FIRST_INTERVAL": 1,
            "SHARED_PORT.PARENT_CHECK_INTERVAL": 1,
            # See test_dagman_survives_ap_restart.py: master's default
            # per-restart backoff adds a flat ~10s unrelated to this test
            # -- doubly costly here since this test restarts twice.
            "MASTER_BACKOFF_CONSTANT": 1,
            # See test_dagman_survives_ap_restart.py: widen DAGMan's
            # node-submit retry budget so a schedd restart that's slower
            # than usual under CI load doesn't cause a premature give-up.
            "DAGMAN_MAX_SUBMIT_ATTEMPTS": 16,
        },
    ) as condor:
        yield condor


# --------------------------------------------------------------------------
class TestDAGManSurvivesDoubleRestart:
    @pytest.mark.parametrize("first_mode, second_mode", DOUBLE_RESTART_CASES)
    def test_dag_completes_after_two_restarts(self, condor, test_dir, first_mode, second_mode):
        case_id = f"{first_mode.value}-{second_mode.value}"
        attempts_log = test_dir / f"attempts-{case_id}.log"
        script = test_dir / f"attempt-{case_id}.py"
        write_attempt_script(script, attempts_log)

        # See test_dagman_survives_ap_restart.py: B blocks on this file
        # instead of sleeping a fixed duration, so both its first and
        # second attempts are guaranteed still running/blocked when each
        # restart lands, regardless of how long the restarts themselves
        # take on a given machine.
        proceed_file = test_dir / f"b-proceed-{case_id}"

        dag_path = test_dir / f"test-{case_id}.dag"
        write_recoverability_dag(dag_path, script, long_node_proceed_file=proceed_file)

        dag = htcondor.Submit.from_dag(str(dag_path))
        dagman_job = submit_with_retry(condor, dag)

        # First interruption: land it while node B is genuinely running.
        wait_for_attempt(attempts_log, "B", min_count=1, timeout=120)
        condor.restart(mode=first_mode, timeout=120)

        # Since B wasn't checkpointed, DAGMan must resubmit it once it
        # (and the pool) come back -- catch that second attempt starting,
        # then land the *second* interruption while DAGMan is fresh off
        # its own recovery from the first one.
        wait_for_attempt(attempts_log, "B", min_count=2, timeout=120)
        condor.restart(mode=second_mode, timeout=120)

        # Let B (or whichever instance survived both interruptions)
        # actually finish now that both restarts have landed.
        proceed_file.touch()

        # See test_dagman_survives_ap_restart.py for why ground truth is
        # used here instead of ClusterState.all_complete on the submitted
        # job handle.
        wait_for_all_attempts(attempts_log, ("A", "B", "C"), timeout=300)

        # A and C each finished exactly once; only B (interrupted twice)
        # is expected to have run more than once.
        assert count_attempts(attempts_log, "A") == 1
        assert count_attempts(attempts_log, "C") == 1
        assert count_attempts(attempts_log, "B") >= 2


# --------------------------------------------------------------------------
class TestDAGManSurvivesBackToBackRestarts:
    @pytest.mark.parametrize("first_mode, second_mode", BACK_TO_BACK_RESTART_CASES)
    def test_dag_completes_across_back_to_back_restarts(
        self, condor, test_dir, first_mode, second_mode
    ):
        case_id = f"{first_mode.value}-{second_mode.value}"
        attempts_log = test_dir / f"attempts-{case_id}.log"
        script = test_dir / f"attempt-{case_id}.py"
        write_attempt_script(script, attempts_log)

        # See test_dagman_survives_ap_restart.py: B blocks on this file
        # instead of sleeping a fixed duration. Since neither restart mode
        # here interrupts B anyway, this also removes any timing
        # sensitivity from landing the restarts "right as B starts" --
        # B just stays blocked through both restarts regardless.
        proceed_file = test_dir / f"b-proceed-{case_id}"

        dag_path = test_dir / f"test-{case_id}.dag"
        write_recoverability_dag(dag_path, script, long_node_proceed_file=proceed_file)

        dag = htcondor.Submit.from_dag(str(dag_path))
        dagman_job = submit_with_retry(condor, dag)

        # Land the first restart right as B starts, same as every other
        # restart test in this suite; the second lands immediately after
        # the first completes, back to back, regardless of DAG state --
        # neither mode here disturbs an already-running local-universe
        # job, so there's no "wait for B's next attempt" signal to land
        # the second restart on.
        wait_for_attempt(attempts_log, "B", min_count=1, timeout=120)
        condor.restart(mode=first_mode, timeout=120)
        condor.restart(mode=second_mode, timeout=120)

        # Let B actually finish now that both restarts have landed.
        proceed_file.touch()

        # See test_dagman_survives_ap_restart.py for why ground truth is
        # used here instead of ClusterState.all_complete on the submitted
        # job handle.
        wait_for_all_attempts(attempts_log, ("A", "B", "C"), timeout=300)

        # Unlike TestDAGManSurvivesDoubleRestart, neither restart mode
        # here is expected to interrupt B, so A/B/C should each run
        # exactly once. Still tolerate B running more than once (>= 1,
        # not == 1): if unlucky timing let a restart briefly overlap B's
        # own startup, a rerun would be legitimate recovery behavior, not
        # a regression.
        assert count_attempts(attempts_log, "A") == 1
        assert count_attempts(attempts_log, "C") == 1
        assert count_attempts(attempts_log, "B") >= 1
