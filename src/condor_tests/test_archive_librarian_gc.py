#!/usr/bin/env pytest

# Tests for condor_librarian garbage collection and user retention.
#
# The database size limit is set far below the size of an empty database so
# every update cycle is over the high water mark, and GC backoff is disabled so
# GC is attempted every cycle. GC only removes files already gone from disk, so
# each scenario deletes the active history file to make its records eligible.
#
# Scenario:
#   1. Submit NUM_JOBS jobs; wait for them to be indexed (user DateOfLastJob NULL).
#   2. Delete the history file; GC removes the file, jobs, and records, and
#      timestamps the user (LIBRARIAN_USER_RETENTION_DAYS < 0 so it is kept).
#   3. Submit 1 more job; once indexed, the user's timestamp is cleared.
#   4. Delete the history file again; the user is timestamped again.
#   5. Reconfig LIBRARIAN_USER_RETENTION_DAYS = 0; next GC pass removes the user.

import os
from pathlib import Path
import sqlite3
import time
import htcondor2

from ornithology import *

NUM_JOBS = 3

DB_POLL_INTERVAL = 2    # seconds between DB polls
DB_POLL_TIMEOUT  = 120  # seconds before giving up
UPDATE_INTERVAL  = 2    # seconds -- matches LIBRARIAN_UPDATE_INTERVAL

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_db_path(condor):
    with condor.use_config():
        return Path(htcondor2.param["LIBRARIAN_DATABASE"])


def _query(condor, sql, params=()):
    """Run a query against a fresh connection; returns [] if the DB isn't ready."""
    try:
        conn = sqlite3.connect(str(_get_db_path(condor)))
        try:
            return conn.execute(sql, params).fetchall()
        finally:
            conn.close()
    except sqlite3.OperationalError:
        return []


def _wait_for(condor, description, predicate, timeout=DB_POLL_TIMEOUT):
    """Poll until predicate() is truthy, returning its value, or fail on timeout."""
    start = time.time()
    while True:
        result = predicate()
        if result:
            return result
        assert time.time() - start <= timeout, f"Timed out after {timeout}s waiting for {description}"
        time.sleep(DB_POLL_INTERVAL)


def _users(condor):
    """Return {UserName: (UserId, DateOfLastJob)}."""
    return {r[0]: (r[1], r[2]) for r in _query(condor, r"SELECT UserName, UserId, DateOfLastJob FROM Users")}


def _count(condor, table):
    rows = _query(condor, f"SELECT COUNT(*) FROM {table}")
    return rows[0][0] if rows else -1


def _submit_and_wait(condor, log_path, path_to_sleep, count):
    """Submit `count` instant-exit jobs, wait for completion and their history records."""
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

    with condor.use_config():
        schedd = htcondor2.Schedd()
        start = time.time()
        ads = []
        while len(ads) != count:
            assert time.time() - start <= 30, "Failed to see all archived records written"
            ads = schedd.history(constraint=f"ClusterId=={handle.clusterid}", projection=["ProcId"], match=count)

    return handle


def _delete_history_and_wait_for_gc(condor):
    """
    Remove the active history file so the librarian marks it deleted and GC
    collects its records. The schedd is reconfigured so it closes its cached
    handle to the unlinked file (Scheduler::Init() -> InitJobHistoryFile()).
    """
    with condor.use_config():
        history_path = Path(htcondor2.param["HISTORY"])
    os.remove(str(history_path))
    p = condor.run_command(["condor_reconfig", "-schedd"])
    assert p.returncode == 0, f"condor_reconfig -schedd failed: {p.stderr}"

    _wait_for(condor, "GC to remove all indexed jobs and deleted files",
              lambda: _count(condor, "Jobs") == 0
                  and _count(condor, "JobRecords") == 0
                  and _count(condor, "Files WHERE DateOfDeletion IS NOT NULL") == 0)


def _set_user_retention_days(condor, days):
    with condor.config_file.open(mode="a") as f:
        f.write(f"\nLIBRARIAN_USER_RETENTION_DAYS = {days}\n")
    p = condor.run_command(["condor_reconfig"])
    assert p.returncode == 0, f"condor_reconfig failed: {p.stderr}"


# ---------------------------------------------------------------------------
# Pool configuration
# ---------------------------------------------------------------------------

@config
def condor_config():
    return {
        "config": {
            "DAEMON_LIST": "$(DAEMON_LIST) LIBRARIAN",
            "LIBRARIAN_UPDATE_INTERVAL": UPDATE_INTERVAL,
            # Far smaller than an empty DB so every cycle is over the high water mark
            "LIBRARIAN_MAX_DATABASE_SIZE": 4096,
            "LIBRARIAN_GC_BACKOFF_SECONDS": 0,
            # Never remove users until the retention is changed later in the test
            "LIBRARIAN_USER_RETENTION_DAYS": -1,
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
def indexed_user(condor, test_dir, path_to_sleep):
    """Index NUM_JOBS jobs and return (UserName, UserId, DateOfLastJob)."""
    _submit_and_wait(condor, test_dir / "job.log", path_to_sleep, NUM_JOBS)
    _wait_for(condor, f"{NUM_JOBS} indexed JobRecords", lambda: _count(condor, "JobRecords") >= NUM_JOBS)
    users = _users(condor)
    assert len(users) == 1, f"Expected exactly one user, got {users}"
    name, (user_id, last_job) = next(iter(users.items()))
    return name, user_id, last_job


@action
def gc_marked_user(indexed_user, condor):
    """After GC removes all of the user's jobs, return (Users row, time of deletion)."""
    before = int(time.time())
    _delete_history_and_wait_for_gc(condor)
    # Retention is -1 so the user must survive a few more GC passes
    time.sleep(2 * UPDATE_INTERVAL + 1)
    return _users(condor), before


@action
def reindexed_user(gc_marked_user, condor, test_dir, path_to_sleep):
    """Index one new job for the same user; return the Users table afterwards."""
    _submit_and_wait(condor, test_dir / "job_reindex.log", path_to_sleep, 1)
    _wait_for(condor, "new job to be indexed", lambda: _count(condor, "JobRecords") >= 1)
    return _users(condor)


@action
def pruned_users(reindexed_user, condor):
    """
    Remove the user's jobs again, then set retention to 0 so the next GC pass
    (which has no eligible files) prunes the timestamped user.
    """
    _delete_history_and_wait_for_gc(condor)
    remarked = _wait_for(condor, "user to be timestamped again",
                         lambda: [v for v in _users(condor).values() if v[1] is not None])

    _set_user_retention_days(condor, 0)
    _wait_for(condor, "user to be pruned", lambda: _count(condor, "Users") == 0)
    return remarked


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestLibrarianGarbageCollection:

    def test_indexed_user_has_no_last_job_time(self, indexed_user):
        assert indexed_user[2] is None, f"Expected NULL DateOfLastJob for user with jobs, got {indexed_user}"

    def test_gc_removes_deleted_file_records(self, gc_marked_user, condor):
        assert _count(condor, "Jobs") == 0
        assert _count(condor, "JobRecords") == 0
        assert _count(condor, "JobLists") == 0

    def test_gc_marks_user_without_jobs(self, indexed_user, gc_marked_user):
        users, before = gc_marked_user
        name, user_id, _ = indexed_user
        assert name in users, f"User {name} removed despite negative retention: {users}"
        assert users[name][0] == user_id
        last_job = users[name][1]
        assert last_job is not None, "Expected DateOfLastJob to be set after GC removed all jobs"
        assert before - 5 <= last_job <= int(time.time()), f"DateOfLastJob {last_job} not near GC time {before}"

    def test_reindex_clears_last_job_time(self, indexed_user, reindexed_user):
        name, user_id, _ = indexed_user
        assert name in reindexed_user
        assert reindexed_user[name][0] == user_id, "Re-indexed user must keep the same UserId"
        assert reindexed_user[name][1] is None, "Expected DateOfLastJob cleared after a new job was indexed"

    def test_user_remarked_after_second_gc(self, pruned_users):
        assert len(pruned_users) == 1

    def test_zero_retention_prunes_user(self, pruned_users, condor):
        assert _count(condor, "Users") == 0
