#!/usr/bin/env pytest

# test_dagman_node_status_recovery.py
#
# LIGO's own monitoring tooling polls DAGMan's NODE_STATUS_FILE to track
# workflow progress. A DAG that *eventually* finishes correctly isn't good
# enough if that file reports a stale or wrong snapshot immediately after
# an AP restart -- this checks the status file stays truthful right after
# each restart, not just once the whole DAG is done.
#
# The second class below covers the way that used to break hardest: an
# interrupted run can leave a job proc event in the node log *behind* that
# proc's terminal event, and writing a status snapshot in that state used to
# EXCEPT DAGMan rather than report anything at all.

import re
import time

import pytest
import htcondor2 as htcondor
import classad2

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
    try:
        for ad in classad2.parseAds(open(status_path)):
            if ad.get("MyType") == "NodeStatus":
                statuses[ad["Node"]] = ad["NodeStatus"]
    except (FileNotFoundError, OSError):
        pass
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
            # See test_dagman_survives_ap_restart.py: this test's "crash"
            # case is a full-pool restart, so shared_port (deliberately
            # spared by _crash()) can be orphaned by a crashed-and-relaunched
            # master the same way.
            "SHARED_PORT.PARENT_CHECK_FIRST_INTERVAL": 1,
            "SHARED_PORT.PARENT_CHECK_INTERVAL": 1,
            # See test_dagman_survives_ap_restart.py: master's default
            # per-restart backoff adds a flat ~10s unrelated to this test.
            "MASTER_BACKOFF_CONSTANT": 1,
            # See test_dagman_survives_ap_restart.py: widen DAGMan's
            # node-submit retry budget so a schedd restart that's slower
            # than usual under CI load doesn't cause a premature give-up.
            "DAGMAN_MAX_SUBMIT_ATTEMPTS": 16,
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
        # timeout left at wait_for_all_attempts()'s own 900s default: see
        # its docstring for why 300s isn't enough headroom given
        # DAGMAN_MAX_SUBMIT_ATTEMPTS=16's worst-case backoff below.
        wait_for_all_attempts(attempts_log, ("A", "B", "C"))

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


# --------------------------------------------------------------------------
# A node's job can pick up a proc event *after* its terminal event. The way
# this shows up in the wild is the CRASH case above: the orphaned starter
# outlives the replacement starter that actually finished the job, so its own
# "terminated and requeued" eviction lands in the node log behind the real
# terminate event.
#
# By then DAGMan has run Node::Cleanup() and thrown away the node's per-proc
# info. The late event must not resurrect that tracking, and above all the
# NODE_STATUS_FILE writer must not EXCEPT over it: it used to, which killed
# DAGMan and left the schedd respawning it in a crash loop until the DAG hit
# this test's timeout, with the status file frozen on a stale snapshot.
#
# That event ordering is a race that only loses on a loaded machine, so don't
# wait for one -- append a well-formed eviction event for an already-terminated
# node directly while DAGMan is still working on the rest of the DAG.
def write_late_event_dag(dag_path, script_path, status_path, proceed_file):
    # A finishes immediately; B blocks on proceed_file so DAGMan is guaranteed
    # to still be running (and still updating the status file) when the late
    # event for A is appended.
    with open(dag_path, "w") as f:
        f.write(
            f'''
JOB A {{
    executable = {script_path}
    arguments  = "A 0"
    universe   = local
    log        = $(JOB).log
}}

JOB B {{
    executable = {script_path}
    arguments  = "B {proceed_file}"
    universe   = local
    log        = $(JOB).log
}}

PARENT A CHILD B

NODE_STATUS_FILE {status_path} 1 ALWAYS-UPDATE
'''
        )


def find_node_job_id(nodes_log, node_name, timeout: int = 120):
    """
    Scrape a node's cluster.proc.subproc out of the DAG's default node log by
    finding the submit event whose body names it. DAGMan monitors this log, not
    the per-node $(JOB).log files, so this is the log a late event has to land
    in to be seen.

    Returns the *last* such submit event: DAGMan can legitimately resubmit a
    node (DAGMAN_MAX_SUBMIT_ATTEMPTS), and only the newest job id is the one it
    still associates with the node, so an older one would be ignored on lookup.
    """
    submit_re = re.compile(r"^000 \((\d+)\.(\d+)\.(\d+)\)")
    deadline = time.time() + timeout
    while True:
        job_id = None
        found = None
        try:
            with open(nodes_log) as f:
                for line in f:
                    match = submit_re.match(line)
                    if match:
                        job_id = match.groups()
                    elif job_id and f'DAGNodeName = "{node_name}"' in line:
                        found = job_id
                        job_id = None
        except FileNotFoundError:
            pass
        if found:
            return found
        if time.time() >= deadline:
            raise TimeoutError(
                "No submit event for node {} in {} within {}s".format(
                    node_name, nodes_log, timeout
                )
            )
        time.sleep(0.5)


def wait_for_event_processed(dagman_out, event_name, node_name, timeout: int = 300):
    """
    Block until DAGMan's own log shows it has consumed ``event_name`` for
    ``node_name``.

    DAGMan logs every event it processes via Dag::PrintEvent() at
    DEBUG_VERBOSE, which is DAGMAN_VERBOSITY's default level, in both normal
    and recovery mode. Waiting on that -- rather than sleeping a fixed number
    of seconds and hoping -- is what keeps this test meaningful on a loaded
    machine: a sleep that turns out to be too short doesn't fail, it silently
    checks that DAGMan hasn't yet crashed on an event it hasn't yet read.
    """
    marker = "Event: {} for HTCondor Node {} ".format(event_name, node_name)
    deadline = time.time() + timeout
    while True:
        text = dagman_out.read_text()
        # Stop early if DAGMan died on the event: the caller's assertion
        # reports that far better than a timeout here would.
        if marker in text or "Assertion ERROR" in text:
            return
        if time.time() >= deadline:
            raise TimeoutError(
                "DAGMan did not process a {} event for node {} within {}s".format(
                    event_name, node_name, timeout
                )
            )
        time.sleep(1)


def append_evicted_event(nodes_log, job_id):
    """
    Append a "terminated and was requeued" eviction event, the same shape
    condor_starter's LocalUserLog::logRequeueEvent() writes when a job it was
    running is requeued rather than completed.

    This writes the node log directly, without taking the user log lock, so
    callers must only do it while the log is quiescent -- see the call site.
    """
    cluster, proc, subproc = job_id
    with open(nodes_log, "a") as f:
        f.write(
            "004 ({}.{}.{}) {} Job was evicted.\n".format(
                cluster, proc, subproc, time.strftime("%Y-%m-%d %H:%M:%S")
            )
        )
        f.write("\t(0) Job terminated and was requeued\n")
        f.write("\t\tUsr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage\n")
        f.write("\t\tUsr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage\n")
        f.write("\t0  -  Run Bytes Sent By Job\n")
        f.write("\t0  -  Run Bytes Received By Job\n")
        f.write("\t(1) Normal termination (return value 0)\n")
        f.write("...\n")


class TestDAGManNodeStatusSurvivesLateProcEvent:
    def test_proc_event_after_node_terminated(self, condor, test_dir):
        attempts_log = test_dir / "attempts-late.log"
        script = test_dir / "attempt-late.py"
        write_attempt_script(script, attempts_log)

        proceed_file = test_dir / "b-proceed-late"
        dag_path = test_dir / "test-late.dag"
        status_path = test_dir / "test-late.status"
        dagman_out = test_dir / "test-late.dag.dagman.out"
        nodes_log = test_dir / "test-late.dag.nodes.log"
        write_late_event_dag(dag_path, script, status_path, proceed_file)

        dag = htcondor.Submit.from_dag(str(dag_path))
        submit_with_retry(condor, dag)

        # Wait for A to be reported done -- i.e. DAGMan has processed A's
        # terminate event and run Cleanup() on the node.
        deadline = time.time() + 120
        statuses = {}
        while time.time() < deadline:
            statuses = parse_node_statuses(status_path)
            if statuses.get("A") == STATUS_DONE:
                break
            time.sleep(1)
        assert statuses.get("A") == STATUS_DONE

        # Wait for B to actually be running before touching the node log. B
        # then sits blocked on proceed_file, so nothing else is writing events
        # and the unlocked append below can't interleave with condor's own
        # writer -- which on a loaded machine is a real window, not a
        # theoretical one.
        wait_for_attempt(attempts_log, "B", timeout=120)

        # Look the job id up only now, so it's the run of A that actually
        # terminated above rather than one of a retried submit's earlier ids.
        append_evicted_event(nodes_log, find_node_job_id(nodes_log, "A"))

        # Wait for DAGMan to actually consume the event, rather than sleeping a
        # fixed interval that a loaded machine can outrun.
        wait_for_event_processed(dagman_out, "ULOG_JOB_EVICTED", "A")

        # EventSanityCheck() runs *after* the event is logged, so confirm the
        # event actually reached its handler instead of being skipped as a bad
        # event -- otherwise a future change to condor's event checking would
        # quietly turn this case into a no-op that can no longer fail.
        assert "in spite of bad event" not in dagman_out.read_text()

        # The late event must not have killed DAGMan, and must not have dragged
        # A back out of its DONE state.
        assert "Assertion ERROR" not in dagman_out.read_text()
        assert parse_node_statuses(status_path).get("A") == STATUS_DONE

        # And the DAG still has to finish normally afterwards. Ground truth
        # first (see wait_for_all_attempts()'s docstring for why its default
        # timeout is so generous), then let the status file catch up to it.
        proceed_file.touch()
        wait_for_all_attempts(attempts_log, ("A", "B"))

        deadline = time.time() + 120
        final_statuses = {}
        while time.time() < deadline:
            final_statuses = parse_node_statuses(status_path)
            if all(final_statuses.get(n) == STATUS_DONE for n in ("A", "B")):
                break
            time.sleep(1)
        assert final_statuses.get("A") == STATUS_DONE
        assert final_statuses.get("B") == STATUS_DONE
        assert "Assertion ERROR" not in dagman_out.read_text()
