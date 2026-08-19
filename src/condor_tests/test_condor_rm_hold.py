#!/usr/bin/env pytest

"""
Test condor_rm -held, which restricts the removal to jobs in the held state.
("-hold" is accepted as a synonym for -held.)

The -held option can be combined with the usual constraints (a cluster, a
cluster.proc, a user, or -constraint); only the held jobs among those selected
are removed.  These tests submit jobs that stay idle (unsatisfiable
requirements), hold a subset of them with condor_hold, and then verify that
condor_rm -held removes exactly the held jobs that match the given constraint
and leaves everything else untouched.
"""

from ornithology import (
    action,
    ClusterState,
    JobStatus,
)


def _submit_idle(condor, test_dir, name, count, extra=None):
    """Submit count jobs that will sit idle forever (requirements never match)."""
    description = {
        "executable": "/bin/sleep",
        "arguments": "3600",
        # Unsatisfiable requirement so the jobs stay idle and never run.
        "requirements": "False",
        "log": (test_dir / f"{name}.log").as_posix(),
    }
    if extra:
        description.update(extra)
    handle = condor.submit(description, count=count)
    assert handle.wait(condition=ClusterState.all_idle, timeout=60)
    return handle


def _procs_in_queue(handle):
    """Return the set of ProcIds still present in the queue for this cluster."""
    return {int(ad["ProcId"]) for ad in handle.query(projection=["ProcId"])}


# =============================================================================
# condor_rm -hold <cluster>
# =============================================================================

@action
def cluster_mixed(default_condor, test_dir):
    """A single cluster of 4 idle jobs; procs 2 and 3 are then held."""
    handle = _submit_idle(default_condor, test_dir, "cluster_mixed", 4)
    cid = handle.clusterid

    result = default_condor.run_command(
        ["condor_hold", f"{cid}.2", f"{cid}.3"]
    )
    assert result.returncode == 0
    assert handle.wait(
        condition=lambda st: st.count_status(JobStatus.HELD) == 2,
        timeout=60,
    )
    return handle


@action
def rm_hold_cluster(default_condor, cluster_mixed):
    """Remove held jobs in the cluster with condor_rm -hold <cluster>."""
    cid = cluster_mixed.clusterid
    result = default_condor.run_command(["condor_rm", "-held", str(cid)])
    # Wait for the two held jobs to be removed.
    cluster_mixed.wait(
        condition=lambda st: st.count_status(JobStatus.REMOVED) == 2,
        timeout=60,
    )
    return result


class TestCondorRmHoldByCluster:
    def test_command_succeeds(self, rm_hold_cluster):
        assert rm_hold_cluster.returncode == 0

    def test_reports_held(self, rm_hold_cluster):
        assert "held jobs" in rm_hold_cluster.stdout

    def test_only_held_removed(self, cluster_mixed, rm_hold_cluster):
        # The two idle jobs (procs 0 and 1) survive; the held ones are gone.
        assert _procs_in_queue(cluster_mixed) == {0, 1}


# =============================================================================
# condor_rm -hold <cluster.proc>
# =============================================================================

@action
def proc_mixed(default_condor, test_dir):
    """A cluster of 2 idle jobs; proc 0 is then held, proc 1 stays idle."""
    handle = _submit_idle(default_condor, test_dir, "proc_mixed", 2)
    cid = handle.clusterid

    result = default_condor.run_command(["condor_hold", f"{cid}.0"])
    assert result.returncode == 0
    assert handle.wait(
        condition=lambda st: st.count_status(JobStatus.HELD) == 1,
        timeout=60,
    )
    return handle


@action
def rm_hold_proc(default_condor, proc_mixed):
    """
    Try to remove the idle proc with -hold (a no-op), then remove the held proc.
    """
    cid = proc_mixed.clusterid

    # proc 1 is idle, so -held should not remove it.  Also exercise the
    # "-hold" synonym here to confirm it is still accepted.
    noop = default_condor.run_command(["condor_rm", "-hold", f"{cid}.1"])

    # proc 0 is held, so -held should remove it.
    removed = default_condor.run_command(["condor_rm", "-held", f"{cid}.0"])
    proc_mixed.wait(
        condition=lambda st: st.count_status(JobStatus.REMOVED) == 1,
        timeout=60,
    )
    return noop, removed


class TestCondorRmHoldByProc:
    def test_held_proc_removed(self, proc_mixed, rm_hold_proc):
        _noop, removed = rm_hold_proc
        assert removed.returncode == 0
        # Only the idle proc 1 remains; held proc 0 is gone.
        assert _procs_in_queue(proc_mixed) == {1}


# =============================================================================
# condor_rm -hold -constraint <expr>
# =============================================================================

@action
def constraint_jobs(default_condor, test_dir):
    """
    Three clusters that exercise the AND of the held filter and a constraint:
      A: marked and held    -> should be removed
      B: marked but idle     -> survives (not held)
      C: held but unmarked   -> survives (constraint does not match)
    """
    marked = {"My.RmHoldMarker": "true"}
    a = _submit_idle(default_condor, test_dir, "const_a", 2, extra=marked)
    b = _submit_idle(default_condor, test_dir, "const_b", 2, extra=marked)
    c = _submit_idle(default_condor, test_dir, "const_c", 1)

    # Hold A (marked) and C (unmarked); leave B idle.
    result = default_condor.run_command(
        ["condor_hold", str(a.clusterid), str(c.clusterid)]
    )
    assert result.returncode == 0
    assert a.wait(condition=ClusterState.all_held, timeout=60)
    assert c.wait(condition=ClusterState.all_held, timeout=60)
    return a, b, c


@action
def rm_hold_constraint(default_condor, constraint_jobs):
    a, _b, _c = constraint_jobs
    result = default_condor.run_command(
        ["condor_rm", "-held", "-constraint", "RmHoldMarker == true"]
    )
    a.wait(
        condition=lambda st: st.count_status(JobStatus.REMOVED) == 2,
        timeout=60,
    )
    return result


class TestCondorRmHoldByConstraint:
    def test_command_succeeds(self, rm_hold_constraint):
        assert rm_hold_constraint.returncode == 0

    def test_marked_and_held_removed(self, constraint_jobs, rm_hold_constraint):
        a, _b, _c = constraint_jobs
        assert _procs_in_queue(a) == set()

    def test_marked_but_idle_survives(self, constraint_jobs, rm_hold_constraint):
        _a, b, _c = constraint_jobs
        assert _procs_in_queue(b) == {0, 1}

    def test_held_but_unmarked_survives(self, constraint_jobs, rm_hold_constraint):
        _a, _b, c = constraint_jobs
        assert _procs_in_queue(c) == {0}
