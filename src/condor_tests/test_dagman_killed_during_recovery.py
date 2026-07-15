#!/usr/bin/env pytest

# test_dagman_killed_during_recovery.py
#
# The specific LIGO-flagged regression: DAGMan itself gets killed while it
# is in recovery mode after a prior AP restart. This must not corrupt or
# duplicate work -- a final recovery attempt must converge to exactly the
# same completed state a normal, uninterrupted run would have reached.
#
# To get a reliable, deterministic multi-second recovery window (instead of
# depending on live-job timing, which would make a real kill mid-replay
# flaky), a large number of synthetic, unrelated submit events are appended
# directly to a real dag.nodes.log before forcing recovery mode. This
# stretches Dag::Bootstrap()'s synchronous recovery replay
# (src/condor_dagman/dag.cpp) long enough to reliably land a kill -9 while
# it's still running, using the same hand-authored-log technique as
# test_dagman_check_q_and_exit.py's write_recovery_log.

import os

import pytest
import htcondor2 as htcondor

from ornithology import *

from dagman_recovery_utils import (
    write_attempt_script,
    write_recoverability_dag,
    count_attempts,
    kill_dagman,
    submit_with_retry,
    NEEDS_KILL9,
)

pytestmark = NEEDS_KILL9

# How many synthetic, unrelated "submit" events to inject into a real
# dag.nodes.log. Each references a throwaway node name unknown to the real
# DAG, so DAGMan just logs a non-fatal "lost track of node" warning per
# event (the same outcome test_dagman_check_q_and_exit.py exercises for a
# single such event) instead of aborting recovery.
SYNTHETIC_EVENT_COUNT = 20000

SYNTHETIC_EVENT = """000 ({jobid}.000.000) 2024-01-01 00:00:00 Job submitted from host: <11.222.33.444:52722?addrs=11.222.33.444-52722&alias=random.host.name&noUDP&sock=schedd_1_1>
    DAG Node: {node}
...
"""


def _stretch_recovery_log(nodes_log_path):
    with open(nodes_log_path, "a") as f:
        for i in range(SYNTHETIC_EVENT_COUNT):
            f.write(SYNTHETIC_EVENT.format(jobid=900000 + i, node="FAKE{}".format(i)))


# --------------------------------------------------------------------------
@standup
def condor(test_dir):
    # See test_dagman_survives_ap_restart.py: without this, a killed DAGMan
    # (a scheduler-universe job) sits in a 5-minute cooldown before the
    # schedd will resubmit it, which would make this test far too slow.
    #
    # DAGMAN_DEBUG defaults to empty (unlike e.g. SCHEDD_DEBUG, which
    # defaults to D_PID -- see param_info.in) so dagman.out never gets a
    # "(pid:NNN)" header on its own; get_dagman_pid()/kill_dagman() need
    # that header, so ask for it explicitly.
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SCHEDULER_UNIVERSE_COOL_DOWN_DURATION": 0,
            "DAGMAN_DEBUG": "D_PID",
        },
    ) as condor:
        yield condor


# --------------------------------------------------------------------------
class TestDAGManKilledDuringRecovery:
    def test_killed_mid_recovery_still_converges(self, condor, test_dir):
        attempts_log = test_dir / "attempts.log"
        script = test_dir / "attempt.py"
        write_attempt_script(script, attempts_log)

        dag_path = test_dir / "test.dag"
        dagman_out = str(dag_path) + ".dagman.out"
        nodes_log_path = str(dag_path) + ".nodes.log"

        # Node timing doesn't matter here -- the synthetic log is what
        # creates the recovery-mode window, not live job execution.
        write_recoverability_dag(dag_path, script, long_node_sleep=0)

        # 1. Run the DAG once, for real, to completion.
        dag = htcondor.Submit.from_dag(str(dag_path))
        first_run = submit_with_retry(condor, dag)
        assert first_run.wait(condition=ClusterState.all_complete, timeout=120)

        for name in ("A", "B", "C"):
            assert count_attempts(attempts_log, name) == 1
        assert os.path.exists(nodes_log_path)

        # 2. Stretch the recovery replay so we get a reliable kill window.
        _stretch_recovery_log(nodes_log_path)

        # 3. Force DAGMan back into recovery mode over that stretched log.
        # "force" is required because condor_submit_dag's auxiliary files
        # (.condor.sub, .lib.out/.err, .dagman.log) already exist from the
        # first run against this same dag file.
        pre_submit_pos = os.path.getsize(dagman_out)
        recovery_dag = htcondor.Submit.from_dag(str(dag_path), {"DoRecov": True, "force": True})
        second_run = submit_with_retry(condor, recovery_dag)

        # 4. Kill DAGMan itself while it's still replaying recovery.
        kill_dagman(dagman_out, timeout=120, after_pos=pre_submit_pos)

        # DAGMan died uncleanly -- wait for the schedd to actually reap it
        # (it writes a requeue/JOB_EVICTED event for the scheduler-universe
        # job once it does -- see WriteRequeueToUserLog() in schedd.cpp)
        # before submitting the next attempt, rather than guessing how
        # long that takes with a fixed sleep.
        assert second_run.wait(condition=ClusterState.all_idle, timeout=120)

        # 5. Submit a final recovery attempt and let it actually finish.
        final_dag = htcondor.Submit.from_dag(str(dag_path), {"DoRecov": True, "force": True})
        final_run = submit_with_retry(condor, final_dag)
        assert final_run.wait(condition=ClusterState.all_complete, timeout=120)

        # 6. None of the real nodes re-ran because of the interruption --
        # they each legitimately completed exactly once, in the first run.
        for name in ("A", "B", "C"):
            assert count_attempts(attempts_log, name) == 1

        with open(dagman_out) as f:
            assert "EXITING WITH STATUS 0" in f.read()
