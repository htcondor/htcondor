#!/usr/bin/env pytest

#   ====test_condor_history_librarian_fastpath====
#   Regression/coverage test for condor_history's librarian-backed direct-seek
#   path (src/condor_tools/history.cpp: readHistoryFromLibrarian()), exercised
#   against a real running LIBRARIAN daemon rather than raw SQL against the DB
#   (test_archive_librarian.py already covers the daemon/DB side directly).
#
#   Covers:
#     - LibrarianClient::GetRecordsByUser(): a bare owner-name restriction
#       resolves via the DB and returns every job for that owner.
#     - LibrarianClient::GetRecordsByCluster(): a bare cluster-ID restriction
#       resolves via the DB and returns only that cluster's job(s).
#     - The union+dedup fix in printLibrarianRecords(): a restriction list that
#       mixes a full cluster.proc, a bare cluster ID, and an owner name -- all
#       partially overlapping -- must print every matching job exactly once,
#       not once per matching restriction-list term.
#     - Output order and limits: records span a rotated and an active archive
#       file, and the direct-seek path must match the file-scan path's order
#       (-backwards default, -forwards) and honor -limit.

import datetime
import os
import sqlite3
import time
from pathlib import Path

import pytest

import htcondor2

from ornithology import *

DB_POLL_INTERVAL = 2    # seconds between DB polls
DB_POLL_TIMEOUT  = 120  # seconds before giving up

# ---------------------------------------------------------------------------
# Helpers (mirrors test_archive_librarian.py's polling helpers)
# ---------------------------------------------------------------------------

def _get_db_path(condor):
    with condor.use_config():
        return Path(htcondor2.param["LIBRARIAN_DATABASE"])


def _safe_job_count(db_path):
    try:
        conn = sqlite3.connect(str(db_path))
        count = conn.execute("SELECT COUNT(*) FROM JobRecords").fetchone()[0]
        conn.close()
        return count
    except sqlite3.OperationalError:
        return 0


def _wait_for_db_job_count(condor, expected_count, timeout=DB_POLL_TIMEOUT):
    db_path = _get_db_path(condor)
    start = time.time()
    while True:
        assert time.time() - start <= timeout, (
            f"Timed out after {timeout}s waiting for {expected_count} "
            f"JobRecord(s) in librarian DB (last seen: {_safe_job_count(db_path)})"
        )
        if _safe_job_count(db_path) >= expected_count:
            return
        time.sleep(DB_POLL_INTERVAL)


def _submit_and_wait(condor, log_path, path_to_sleep, count=1):
    """Submit `count` instant-exit jobs and wait for all to complete."""
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "universe": "vanilla",
            "arguments": "0",
            "log": str(log_path),
        },
        count=count,
    )
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=120,
        verbose=True,
    ), "Job did not complete within the timeout"

    # Wait for the ad to be written into the archive file (same as
    # test_archive_librarian.py) so a subsequent librarian DB poll isn't
    # racing the schedd's own history write.
    with condor.use_config():
        schedd = htcondor2.Schedd()
        start = time.time()
        ads = []
        while len(ads) < count:
            assert time.time() - start <= 30, "Failed to see archived record(s) written"
            ads = schedd.history(constraint=f"ClusterId=={handle.clusterid}", projection=["Owner"], match=count)

    return handle


def _rotate_history(condor):
    """Rename the active history file the same way the schedd's rotation would,
    then reconfig the schedd so it starts a fresh active file. The rotated name
    (history.<timestamp>) sorts after the active name (history), so ordering by
    path instead of by rotation would put the newest records first."""
    with condor.use_config():
        history_path = Path(htcondor2.param["HISTORY"])

    timestamp = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
    rotated_path = history_path.parent / f"{history_path.name}.{timestamp}"
    history_path.rename(rotated_path)
    os.utime(str(rotated_path), None)

    p = condor.run_command(["condor_reconfig", "-schedd"])
    assert p.returncode == 0, f"condor_reconfig -schedd failed: {p.stderr}"


def _run_history(condor, args):
    """Run condor_history with the given restriction-list/option args, requesting
    a plain "cluster proc" line per job (no header) so output is trivially
    parseable. Returns the list of "cluster.proc" strings found, in output order."""
    p = condor.run_command(["condor_history"] + args + ["-af", "ClusterId", "ProcId"])
    assert p.stderr == "", f"condor_history {args} wrote to stderr: {p.stderr}"
    jobids = []
    for line in p.stdout.strip().split("\n"):
        line = line.strip()
        if not line:
            continue
        cluster, proc = line.split()
        jobids.append(f"{cluster}.{proc}")
    return jobids


# ---------------------------------------------------------------------------
# Pool configuration: enable both the daemon and USING_LIBRARIAN, so
# condor_history's LibrarianClient actually takes the fast path. (Note
# test_archive_librarian.py only enables the daemon, since its tests inspect
# the DB directly and never go through LibrarianClient/condor_history.)
# ---------------------------------------------------------------------------

@config
def condor_config():
    return {
        "raw_config": "use feature : Librarian",
        "config": {
            # Short update interval so the test doesn't wait the 30-second default.
            "LIBRARIAN_UPDATE_INTERVAL": 2,
        },
    }


@standup
def condor(condor_config, test_dir):
    with Condor(local_dir=test_dir / "condor", **condor_config) as condor:
        yield condor


# ---------------------------------------------------------------------------
# Actions: two single-proc clusters under the same (real) owner.
# ---------------------------------------------------------------------------

@action
def cluster_a(condor, test_dir, path_to_sleep):
    return _submit_and_wait(condor, test_dir / "a.log", path_to_sleep)


@action
def cluster_b(condor, test_dir, path_to_sleep, cluster_a):
    # Depending on cluster_a just keeps submission (and DB ingest) order
    # predictable; not required for correctness of the assertions below.
    return _submit_and_wait(condor, test_dir / "b.log", path_to_sleep)


@action
def owner(condor, cluster_a):
    with condor.use_config():
        ads = htcondor2.Schedd().history(constraint=f"ClusterId=={cluster_a.clusterid}", projection=["Owner"], match=1)
    return ads[0]["Owner"]


@action
def both_jobs_indexed(condor, cluster_a, cluster_b):
    """Guarantee both pre-rotation clusters are in the librarian DB."""
    _wait_for_db_job_count(condor, 2)


@action
def history_rotated(condor, both_jobs_indexed):
    _rotate_history(condor)


NUM_C_PROCS = 3

@action
def cluster_c(condor, test_dir, path_to_sleep, history_rotated):
    """Multi-proc cluster written to the new active archive file after rotation."""
    return _submit_and_wait(condor, test_dir / "c.log", path_to_sleep, NUM_C_PROCS)


@action
def all_jobs_indexed(condor, cluster_a, cluster_b, cluster_c):
    """Depend on this fixture to guarantee every cluster is in the librarian DB
    before running any condor_history command against it."""
    _wait_for_db_job_count(condor, 2 + NUM_C_PROCS)


@action
def all_jobids(cluster_a, cluster_b, cluster_c):
    return {f"{cluster_a.clusterid}.0", f"{cluster_b.clusterid}.0"} | {
        f"{cluster_c.clusterid}.{p}" for p in range(NUM_C_PROCS)
    }


@action
def file_scan_backwards(condor, all_jobs_indexed, owner):
    """Reference output order from the normal file-scan path (newest first)."""
    return _run_history(condor, ["-no-librarian", owner])


@action
def file_scan_forwards(condor, all_jobs_indexed, owner):
    """Reference output order from the normal file-scan path (oldest first)."""
    return _run_history(condor, ["-no-librarian", "-forwards", owner])


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestCondorHistoryLibrarianFastpath:

    def test_owner_only_returns_all_owners_jobs(self, condor, all_jobs_indexed, owner, all_jobids):
        """GetRecordsByUser(): a bare owner name resolves every cluster via the DB."""
        jobids = _run_history(condor, [owner])
        assert sorted(jobids) == sorted(all_jobids)

    def test_cluster_only_returns_just_that_cluster(self, condor, all_jobs_indexed, cluster_a):
        """GetRecordsByCluster(): a bare cluster ID resolves only its own job via the DB."""
        jobids = _run_history(condor, [str(cluster_a.clusterid)])
        assert jobids == [f"{cluster_a.clusterid}.0"]

    def test_mixed_restriction_list_dedups_overlapping_job(self, condor, all_jobs_indexed, owner, cluster_a, cluster_b, all_jobids):
        """
        Regression test: restriction list mixes an owner name (matches both
        clusters), a bare cluster ID, and a full cluster.proc (both matching
        cluster_a only). Before the dedup fix, cluster_a's job -- returned by
        all three of GetRecordsByUser/GetRecordsByCluster/GetRecords -- would
        be printed three times; cluster_b's job (found only via the owner
        term) must still appear, exactly once.
        """
        a_id = cluster_a.clusterid
        b_id = cluster_b.clusterid
        jobids = _run_history(condor, [owner, str(a_id), f"{a_id}.0"])
        assert jobids.count(f"{a_id}.0") == 1, f"cluster_a job duplicated in output: {jobids}"
        assert jobids.count(f"{b_id}.0") == 1, f"cluster_b job missing/duplicated in output: {jobids}"
        assert sorted(jobids) == sorted(all_jobids)

    @pytest.mark.parametrize("direction", [[], ["-forwards"]], ids=["backwards", "forwards"])
    def test_mixed_restriction_list_does_not_stop_at_found_cluster(self, condor, all_jobs_indexed, owner, cluster_a, all_jobids, direction):
        """
        Regression test: once every cluster/cluster.proc term has been found,
        owner terms in the same restriction list may still match more jobs, so
        the scan must not stop early. Reading forwards, cluster_a is found
        first, which used to end the search before any later owner jobs.
        """
        a_id = cluster_a.clusterid
        jobids = _run_history(condor, direction + [owner, f"{a_id}.0"])
        assert sorted(jobids) == sorted(all_jobids)

    def test_backwards_order_matches_file_scan(self, condor, owner, all_jobids, file_scan_backwards):
        """Default (-backwards) order is newest first across rotated and active files."""
        assert sorted(file_scan_backwards) == sorted(all_jobids)
        assert _run_history(condor, [owner]) == file_scan_backwards
        assert _run_history(condor, ["-backwards", owner]) == file_scan_backwards

    def test_forwards_order_matches_file_scan(self, condor, owner, all_jobids, file_scan_forwards):
        """-forwards order is oldest first across rotated and active files."""
        assert sorted(file_scan_forwards) == sorted(all_jobids)
        assert _run_history(condor, ["-forwards", owner]) == file_scan_forwards

    def test_order_follows_rotation_not_path(self, condor, owner, cluster_a, cluster_b, cluster_c):
        """Pre-rotation clusters come before the post-rotation cluster reading forwards."""
        clusters = [int(j.split(".")[0]) for j in _run_history(condor, ["-forwards", owner])]
        a_id, b_id, c_id = cluster_a.clusterid, cluster_b.clusterid, cluster_c.clusterid
        assert clusters == [a_id, b_id] + [c_id] * NUM_C_PROCS

    @pytest.mark.parametrize("limit", [1, 2, 4])
    def test_owner_limit_backwards(self, condor, owner, file_scan_backwards, limit):
        """Regression test: -limit with an owner returns the newest N jobs."""
        assert _run_history(condor, ["-limit", str(limit), owner]) == file_scan_backwards[:limit]

    @pytest.mark.parametrize("limit", [1, 2, 4])
    def test_owner_limit_forwards(self, condor, owner, file_scan_forwards, limit):
        """Regression test: -limit -forwards with an owner returns the oldest N jobs."""
        assert _run_history(condor, ["-forwards", "-limit", str(limit), owner]) == file_scan_forwards[:limit]

    @pytest.mark.parametrize("direction", [[], ["-forwards"]], ids=["backwards", "forwards"])
    def test_cluster_limit(self, condor, all_jobs_indexed, cluster_c, direction):
        """Regression test: -limit with a bare cluster ID is honored."""
        c_id = str(cluster_c.clusterid)
        reference = _run_history(condor, ["-no-librarian"] + direction + [c_id])
        assert len(reference) == NUM_C_PROCS
        assert _run_history(condor, direction + ["-limit", "1", c_id]) == reference[:1]
        assert _run_history(condor, direction + ["-limit", "2", c_id]) == reference[:2]
