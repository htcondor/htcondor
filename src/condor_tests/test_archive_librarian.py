#!/usr/bin/env pytest

# Tests for condor_librarian: history file ingestion and rotation handling.
#
# Scenario:
#   1. Submit NUM_JOBS jobs and wait for completion.
#   2. Run condor_librarian (first cycle) → check initial DB state.
#   3. Simulate history rotation, submit 1 more job, run condor_librarian again.
#   4. Check final DB state: new job added, original jobs unchanged.

import datetime
import os
from pathlib import Path
import shutil
import sqlite3
import time
import htcondor2

import pytest

from ornithology import *

NUM_JOBS = 10

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _run_librarian_cycle(condor):
    """Run one condor_librarian update cycle with no sleep."""
    result = condor.run_command(
        ["condor_librarian", "-delay", "0", "-break-after", "1"]
    )
    assert result.returncode == 0, (
        f"condor_librarian failed ({result.returncode})\n{result.stderr}"
    )


def _submit_and_wait(condor, log_path, path_to_sleep, count):
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
    ), "Jobs did not complete within the timeout"

    # Wait for ads to be written into archive file
    with condor.use_config():
        schedd = htcondor2.Schedd()
        start = time.time()
        ads = []
        while len(ads) != count:
            assert time.time() - start <= 30, "Failed to see all archived records written"
            ads = schedd.history(constraint=f"ClusterId=={handle.clusterid}", projection=["ProcId"], match=count)

    return handle


# ---------------------------------------------------------------------------
# Pool configuration
# ---------------------------------------------------------------------------

@config
def condor_config():
    return {
        "config": {
            "LIBRARIAN_DATABASE": "$(LOCAL_DIR)/librarian.db",
        }
    }


@standup
def condor(condor_config, test_dir):
    with Condor(local_dir=test_dir / "condor", **condor_config) as condor:
        yield condor


# ---------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------

@action
def completed_job_cluster(condor, test_dir, path_to_sleep):
    return _submit_and_wait(condor, test_dir / "job.log", path_to_sleep, NUM_JOBS)


@action
def librarian_db(completed_job_cluster, condor, test_dir):
    """Run the first librarian cycle and yield an open DB connection."""

    _run_librarian_cycle(condor)

    with condor.use_config():
        db_path = Path(htcondor2.param["LIBRARIAN_DATABASE"])

    assert db_path.exists(), f"Database not created at {db_path}"

    conn = sqlite3.connect(str(db_path))
    yield conn
    conn.close()


@action
def initial_db_snapshot(librarian_db):
    """
    Capture (ClusterId, ProcId) -> {offset, completion_date, file_id} after the
    first librarian run so rotation tests can assert no original rows were mutated.
    """
    rows = librarian_db.execute(
        "SELECT j.ClusterId, j.ProcId, jr.Offset, jr.CompletionDate, jr.FileId "
        "FROM Jobs j JOIN JobRecords jr ON j.JobId = jr.JobId"
    ).fetchall()
    return {(r[0], r[1]): {"offset": r[2], "completion_date": r[3], "file_id": r[4]} for r in rows}


@action
def post_rotation_handle(initial_db_snapshot, condor, test_dir, path_to_sleep):
    """
    Simulate a history rotation, complete one more job, and run a second cycle.

    ArchiveMonitor detects rotation by finding files with mtime > lastStatusTime
    whose inode/hash match the last-read file.  rename() preserves mtime, so
    os.utime() is called after the rename; the preceding sleep() ensures the
    updated mtime is strictly after the first run's TimeOfUpdate (second precision).
    """

    with condor.use_config():
        history_path = Path(htcondor2.param["HISTORY"])

    timestamp = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
    rotated_path = history_path.parent / f"{history_path.name}.{timestamp}"
    history_path.rename(rotated_path)

    # This makes me sad but is required to ensure proper file rotation detection
    time.sleep(1)

    os.utime(str(rotated_path), None)

    # Restart the schedd to close file handle to rotated file
    p = condor.run_command(["condor_restart", "-fast"])

    # Wait for condor to be back up
    start = time.time()
    while True:
        assert time.time() - start <= 30, "Failed to restart condor"
        q = condor.run_command(["condor_ping", "-type", "schedd"])
        if q.returncode == 0:
            break
        time.sleep(1)

    handle = _submit_and_wait(condor, test_dir / "job_post_rotation.log", path_to_sleep, 1)

    return handle


@action
def post_rotation_db(post_rotation_handle, condor):
    """Open a fresh DB connection after the second librarian run."""

    _run_librarian_cycle(condor)

    with condor.use_config():
        db_path = Path(htcondor2.param["LIBRARIAN_DATABASE"])

    assert db_path.exists(), f"Database not created at {db_path}"

    conn = sqlite3.connect(db_path)
    yield conn
    conn.close()


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestLibrarianIngestion:

    # --- Initial ingest (first cycle, NUM_JOBS completed jobs) ---

    def test_initial_jobs_count(self, librarian_db):
        assert librarian_db.execute(r"SELECT COUNT(*) FROM Jobs").fetchone()[0] == NUM_JOBS

    def test_initial_job_records_count(self, librarian_db):
        assert librarian_db.execute(r"SELECT COUNT(*) FROM JobRecords").fetchone()[0] == NUM_JOBS

    def test_initial_proc_ids(self, librarian_db, completed_job_cluster):
        rows = librarian_db.execute(
            r"SELECT ProcId FROM Jobs WHERE ClusterId = ? ORDER BY ProcId",
            (completed_job_cluster.clusterid,),
        ).fetchall()
        assert [r[0] for r in rows] == list(range(NUM_JOBS))

    def test_initial_single_owner(self, librarian_db):
        assert librarian_db.execute(
            r"SELECT COUNT(DISTINCT UserId) FROM Jobs WHERE UserId IS NOT NULL"
        ).fetchone()[0] == 1

    def test_initial_files_entry(self, librarian_db):
        assert librarian_db.execute(r"SELECT COUNT(*) FROM Files").fetchone()[0] >= 1

    def test_initial_positive_offsets(self, librarian_db):
        offsets = [r[0] for r in librarian_db.execute(r"SELECT Offset FROM JobRecords").fetchall()]
        assert len(offsets) == NUM_JOBS
        # We expect one job at offset zero and all others to be larger
        assert [o > 0 for o in offsets].count(False) == 1

    def test_initial_records_linked_to_files(self, librarian_db):
        assert librarian_db.execute(
            r"SELECT COUNT(*) FROM JobRecords WHERE FileId IS NULL OR FileId = 0"
        ).fetchone()[0] == 0

    def test_initial_completion_dates(self, librarian_db):
        assert librarian_db.execute(
            r"SELECT COUNT(*) FROM JobRecords WHERE CompletionDate <= 0"
        ).fetchone()[0] == 0

    # --- Post-rotation (second cycle, NUM_JOBS + 1 total) ---

    def test_post_rotation_job_counts(self, post_rotation_db):
        assert post_rotation_db.execute(r"SELECT COUNT(*) FROM Jobs").fetchone()[0] == NUM_JOBS + 1
        assert post_rotation_db.execute(r"SELECT COUNT(*) FROM JobRecords").fetchone()[0] == NUM_JOBS + 1

    def test_post_rotation_new_job_properties(self, post_rotation_handle, initial_db_snapshot, post_rotation_db):
        """New job is present with valid structural data stored in a distinct file."""
        row = post_rotation_db.execute(
            r"SELECT j.ProcId, jr.Offset, jr.FileId, jr.CompletionDate "
            r"FROM Jobs j JOIN JobRecords jr ON j.JobId = jr.JobId "
            r"WHERE j.ClusterId = ?",
            (post_rotation_handle.clusterid,),
        ).fetchone()
        assert row is not None,   "Post-rotation job missing from database"
        assert row[0] == 0,       "ProcId should be 0"
        assert row[1] == 0,        "Offset must zero"
        assert row[2] not in {v["file_id"] for v in initial_db_snapshot.values()}, "Post-rotation job must reside in a different file than the original jobs"
        assert row[3] > 0,        "CompletionDate must be positive"

    def test_post_rotation_original_jobs_unchanged(self, initial_db_snapshot, post_rotation_db):
        """Every original job must retain its exact Offset, CompletionDate, and FileId."""
        rows = post_rotation_db.execute(
            r"SELECT j.ClusterId, j.ProcId, jr.Offset, jr.CompletionDate, jr.FileId "
            r"FROM Jobs j JOIN JobRecords jr ON j.JobId = jr.JobId"
        ).fetchall()
        current = {(r[0], r[1]): {"offset": r[2], "completion_date": r[3], "file_id": r[4]} for r in rows}
        for key, orig in initial_db_snapshot.items():
            assert key in current,       f"Job {key} missing after rotation"
            assert current[key] == orig, f"Job {key} data changed: was {orig}, now {current[key]}"

    def test_post_rotation_files_tracked(self, post_rotation_db):
        assert post_rotation_db.execute("SELECT COUNT(*) FROM Files").fetchone()[0] >= 2

    def test_post_rotation_owner_unchanged(self, post_rotation_db):
        assert post_rotation_db.execute(
            r"SELECT COUNT(DISTINCT UserId) FROM Jobs WHERE UserId IS NOT NULL"
        ).fetchone()[0] == 1
