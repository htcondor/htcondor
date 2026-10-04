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

import sqlite3
import time
from pathlib import Path

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


def _submit_and_wait(condor, log_path, path_to_sleep):
    """Submit a single instant-exit job and wait for it to complete."""
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "universe": "vanilla",
            "arguments": "0",
            "log": str(log_path),
        },
        count=1,
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
        while not ads:
            assert time.time() - start <= 30, "Failed to see archived record written"
            ads = schedd.history(constraint=f"ClusterId=={handle.clusterid}", projection=["Owner"], match=1)

    return handle


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
    """Depend on this fixture to guarantee both clusters are in the librarian DB
    before running any condor_history command against it."""
    _wait_for_db_job_count(condor, 2)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestCondorHistoryLibrarianFastpath:

    def test_owner_only_returns_all_owners_jobs(self, condor, both_jobs_indexed, owner, cluster_a, cluster_b):
        """GetRecordsByUser(): a bare owner name resolves both clusters via the DB."""
        jobids = _run_history(condor, [owner])
        assert set(jobids) == {f"{cluster_a.clusterid}.0", f"{cluster_b.clusterid}.0"}

    def test_cluster_only_returns_just_that_cluster(self, condor, both_jobs_indexed, cluster_a, cluster_b):
        """GetRecordsByCluster(): a bare cluster ID resolves only its own job via the DB."""
        jobids = _run_history(condor, [str(cluster_a.clusterid)])
        assert jobids == [f"{cluster_a.clusterid}.0"]

    def test_mixed_restriction_list_dedups_overlapping_job(self, condor, both_jobs_indexed, owner, cluster_a, cluster_b):
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
        assert set(jobids) == {f"{a_id}.0", f"{b_id}.0"}
