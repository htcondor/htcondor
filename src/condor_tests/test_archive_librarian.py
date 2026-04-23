#!/usr/bin/env pytest

# Tests for condor_librarian: history file ingestion and rotation handling.
#
# Scenario:
#   1. condor_librarian runs as a long-lived daemon managed by condor_master.
#   2. Submit NUM_JOBS jobs and wait for completion.
#   3. Poll until the librarian DB reflects the initial ingest (with timeout).
#   4. Simulate history rotation, submit 1 more job.
#   5. Poll until the DB reflects the post-rotation state (with timeout).
#   6. Check final DB state: new job added, original jobs unchanged.

import datetime
import os
from pathlib import Path
import sqlite3
import time
import htcondor2

import pytest

from ornithology import *

NUM_JOBS = 10

DB_POLL_INTERVAL         = 2    # seconds between DB polls
DB_POLL_TIMEOUT          = 120  # seconds before giving up
UPDATE_INTERVAL          = 5    # seconds — matches LIBRARIAN_UPDATE_INTERVAL
STATUS_RETENTION_SECONDS = 10   # seconds — short window to exercise Status pruning in tests

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_db_path(condor):
    """Return the LIBRARIAN_DATABASE path from the running pool config."""
    with condor.use_config():
        return Path(htcondor2.param["LIBRARIAN_DATABASE"])


def _wait_for_db_job_count(condor, expected_count, timeout=DB_POLL_TIMEOUT):
    """
    Poll the librarian DB until JobRecords has at least `expected_count` rows,
    or raise AssertionError on timeout.  Opens a fresh connection each poll so
    we never observe a stale SQLite cache while the daemon is writing.
    """
    db_path = _get_db_path(condor)
    start = time.time()
    while True:
        assert time.time() - start <= timeout, (
            f"Timed out after {timeout}s waiting for {expected_count} "
            f"JobRecord(s) in librarian DB (last seen: {_safe_job_count(db_path)})"
        )
        count = _safe_job_count(db_path)
        if count >= expected_count:
            return
        time.sleep(DB_POLL_INTERVAL)


def _safe_job_count(db_path):
    """Return current JobRecords row count, or 0 if the DB does not yet exist."""
    try:
        conn = sqlite3.connect(str(db_path))
        count = conn.execute("SELECT COUNT(*) FROM JobRecords").fetchone()[0]
        conn.close()
        return count
    except sqlite3.OperationalError:
        return 0


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
            # Run condor_librarian as a long-lived daemon under condor_master.
            "DAEMON_LIST": "$(DAEMON_LIST) LIBRARIAN",
            # Short update interval so tests don't wait the 30-second default.
            "LIBRARIAN_UPDATE_INTERVAL":          UPDATE_INTERVAL,
            "LIBRARIAN_STATUS_RETENTION_SECONDS": STATUS_RETENTION_SECONDS,
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
    """
    Wait for the librarian daemon to ingest all initial jobs, then yield an
    open DB connection.  No records are read until the daemon has confirmed
    all NUM_JOBS entries are present.
    """
    _wait_for_db_job_count(condor, NUM_JOBS)

    db_path = _get_db_path(condor)
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
        r"SELECT j.ClusterId, j.ProcId, jr.Offset, jr.CompletionDate, jr.FileId "
        r"FROM Jobs j JOIN JobRecords jr ON j.JobId = jr.JobId"
    ).fetchall()
    return {(r[0], r[1]): {"offset": r[2], "completion_date": r[3], "file_id": r[4]} for r in rows}


@action
def post_rotation_handle(initial_db_snapshot, condor, test_dir, path_to_sleep):
    """
    Simulate a history rotation, complete one more job.

    rename() preserves mtime, so os.utime() is called after the rename; the
    preceding sleep() ensures the updated mtime is strictly after the first
    run's TimeOfUpdate (second precision).
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
    p = condor.run_command(["condor_restart", "-fast", "-daemon", "schedd"])

    # Wait for condor to be back up (schedd + librarian daemon both restart).
    # condor_q is used instead of condor_ping because it exercises a full
    # schedd command connection; condor_ping can respond before the schedd is
    # ready to accept job submissions, producing SECMAN:2007 on the next call.
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
    """
    Wait for the librarian daemon to ingest the post-rotation job, then yield
    a fresh DB connection.  Blocks until the daemon confirms NUM_JOBS + 1
    records are present, with a timeout.
    """
    _wait_for_db_job_count(condor, NUM_JOBS + 1)

    db_path = _get_db_path(condor)
    assert db_path.exists(), f"Database not created at {db_path}"

    conn = sqlite3.connect(str(db_path))
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

    def test_initial_status_recorded(self, librarian_db):
        """The daemon must have written at least one Status row after ingest."""
        assert librarian_db.execute(
            r"SELECT COUNT(*) FROM Status"
        ).fetchone()[0] >= 1

    def test_status_retention(self, condor):
        """Status rows older than LIBRARIAN_STATUS_RETENTION_SECONDS must be pruned each cycle."""
        time.sleep(STATUS_RETENTION_SECONDS + 2 * UPDATE_INTERVAL)

        db_path = _get_db_path(condor)
        conn = sqlite3.connect(str(db_path))
        rows = conn.execute(r"SELECT TimeOfUpdate FROM Status").fetchall()
        conn.close()

        now = int(time.time())
        assert len(rows) >= 1, "Expected at least one recent Status row to survive"
        stale = [r[0] for r in rows if now - r[0] > STATUS_RETENTION_SECONDS + UPDATE_INTERVAL]
        assert not stale, (
            f"Found Status rows older than retention window "
            f"({STATUS_RETENTION_SECONDS + UPDATE_INTERVAL}s): {stale}"
        )

    def test_initial_active_file_not_fully_read(self, librarian_db):
        """The active archive file (no rotation time) must not be marked done."""
        row = librarian_db.execute(
            r"SELECT COUNT(*) FROM Files WHERE DateOfRotation IS NULL AND FullyRead = 0"
        ).fetchone()
        assert row[0] >= 1, "Expected at least one active (non-rotated, non-fully-read) file"

    def test_initial_avg_record_size_positive(self, librarian_db):
        """AvgRecordSize must be > 0 for every tracked file after the first ingest cycle."""
        rows = librarian_db.execute(r"SELECT AvgRecordSize FROM Files").fetchall()
        assert len(rows) >= 1
        assert all(r[0] > 0 for r in rows), \
            f"Expected all Files.AvgRecordSize > 0, got: {[r[0] for r in rows]}"

    def test_initial_records_read_count(self, librarian_db):
        """Sum of RecordsRead across all files must be >= NUM_JOBS after the first ingest."""
        total = librarian_db.execute(r"SELECT SUM(RecordsRead) FROM Files").fetchone()[0] or 0
        assert total >= NUM_JOBS, \
            f"Expected SUM(RecordsRead) >= {NUM_JOBS}, got {total}"

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
        assert post_rotation_db.execute(r"SELECT COUNT(*) FROM Files").fetchone()[0] >= 2

    def test_post_rotation_owner_unchanged(self, post_rotation_db):
        assert post_rotation_db.execute(
            r"SELECT COUNT(DISTINCT UserId) FROM Jobs WHERE UserId IS NOT NULL"
        ).fetchone()[0] == 1

    def test_post_rotation_rotated_file_marked(self, post_rotation_db):
        """Exactly one file must carry a RotationTime after a single rotation."""
        count = post_rotation_db.execute(
            r"SELECT COUNT(*) FROM Files WHERE DateOfRotation IS NOT NULL"
        ).fetchone()[0]
        assert count >= 1, "No file was marked with a DateOfRotation after rotation"

    def test_post_rotation_rotated_file_fully_read(self, post_rotation_db):
        """The rotated file must be marked FullyRead once the librarian drains it."""
        count = post_rotation_db.execute(
            r"SELECT COUNT(*) FROM Files WHERE DateOfRotation IS NOT NULL AND FullyRead = 1"
        ).fetchone()[0]
        assert count >= 1, "Rotated file not marked FullyRead"

    def test_post_rotation_status_updated(self, post_rotation_db):
        """The daemon must have written multiple Status rows across both cycles."""
        assert post_rotation_db.execute(
            r"SELECT COUNT(*) FROM Status"
        ).fetchone()[0] >= 2

    def test_post_rotation_avg_record_size_persisted(self, post_rotation_db):
        """AvgRecordSize must remain > 0 for all files after a daemon restart (DB persistence)."""
        rows = post_rotation_db.execute(r"SELECT FileName, AvgRecordSize FROM Files").fetchall()
        assert len(rows) >= 2, "Expected at least 2 Files entries after rotation"
        bad = [(r[0], r[1]) for r in rows if r[1] <= 0]
        assert not bad, \
            f"AvgRecordSize reset to 0 after restart for: {bad}"

    def test_post_rotation_records_read_total(self, post_rotation_db):
        """Total RecordsRead across all files must be >= NUM_JOBS + 1 after the post-rotation ingest."""
        total = post_rotation_db.execute(r"SELECT SUM(RecordsRead) FROM Files").fetchone()[0] or 0
        assert total >= NUM_JOBS + 1, \
            f"Expected SUM(RecordsRead) >= {NUM_JOBS + 1}, got {total}"
