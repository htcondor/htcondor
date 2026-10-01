#!/usr/bin/env pytest

# Tests that condor_librarian garbage collection actually shrinks the database
# file toward LIBRARIAN_LOW_WATER_MARK, deleting the oldest eligible files first
# and no more than it needs to.
#
# Scenario:
#   1. Write NUM_ROTATED synthetic rotated history files (RECORDS_PER_FILE
#      records each) plus a one-record active history file, and wait for the
#      librarian to index them all. Synthetic files keep ingest fast; no jobs run.
#   2. Remove the NUM_TO_REMOVE oldest rotated files from disk one update cycle
#      apart, so each gets a distinct DateOfDeletion (GC collects files in
#      DateOfDeletion order).
#   3. Reconfig LIBRARIAN_MAX_DATABASE_SIZE to the current database size so it is
#      over the high water mark, and wait for GC to settle. Sizes are measured as
#      page_count * page_size (as the librarian does), since in WAL mode the main
#      file on disk lags until a checkpoint.
#   4. Check the database shrank to at or below the low water mark (both the
#      page_count * page_size size and, after GC's WAL checkpoint, the main file
#      on disk), only the earliest-removed files were collected, and files still
#      on disk are intact.

import os
from pathlib import Path
import sqlite3
import time
import htcondor2

from ornithology import *

NUM_ROTATED      = 5
RECORDS_PER_FILE = 1000
NUM_TO_REMOVE    = 4   # oldest rotated files removed from disk (GC eligible)

UPDATE_INTERVAL  = 2   # seconds -- matches LIBRARIAN_UPDATE_INTERVAL
DB_POLL_INTERVAL = 1
DB_POLL_TIMEOUT  = 120
SETTLE_CYCLES    = 5   # consecutive update cycles with no GC change before measuring

HIGH_WATER_MARK  = 0.97
LOW_WATER_MARK   = 0.80

OWNER = "gcsizeuser"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _param(condor, name):
    with condor.use_config():
        return htcondor2.param[name]


def _query(db_path, sql, params=()):
    try:
        conn = sqlite3.connect(str(db_path))
        try:
            return conn.execute(sql, params).fetchall()
        finally:
            conn.close()
    except sqlite3.OperationalError:
        return []


def _scalar(db_path, sql, params=()):
    rows = _query(db_path, sql, params)
    return rows[0][0] if rows else None


def _wait_for(description, predicate, timeout=DB_POLL_TIMEOUT):
    start = time.time()
    while True:
        result = predicate()
        if result:
            return result
        assert time.time() - start <= timeout, f"Timed out after {timeout}s waiting for {description}"
        time.sleep(DB_POLL_INTERVAL)


def _file_size(db_path):
    """Size of the main database file on disk (lags the real size until a WAL checkpoint)."""
    return os.path.getsize(str(db_path))


def _logical_size(db_path):
    """
    page_count * page_size as seen by a reader, including pages still in the WAL.
    This is the size the librarian's GC measures.
    """
    return _scalar(db_path, "PRAGMA page_count") * _scalar(db_path, "PRAGMA page_size")


def _write_history_file(path, first_cluster, count, completion_date):
    """Write `count` minimal job records in the archive format ArchiveReader parses."""
    with path.open("w") as f:
        for i in range(count):
            cluster = first_cluster + i
            f.write(
                f"ClusterId = {cluster}\n"
                f"ProcId = 0\n"
                f"Owner = \"{OWNER}\"\n"
                f"JobStatus = 4\n"
                f"CompletionDate = {completion_date}\n"
                f"*** Offset = 0 ClusterId = {cluster} ProcId = 0 Owner = \"{OWNER}\" "
                f"CompletionDate = {completion_date} CurrentTime = {completion_date}\n"
            )


def _file_rows(db_path):
    """{basename: (FileId, DateOfDeletion, JobRecords count)} for every Files row."""
    rows = _query(
        db_path,
        r"SELECT f.FileName, f.FileId, f.DateOfDeletion, "
        r"(SELECT COUNT(*) FROM JobRecords jr WHERE jr.FileId = f.FileId) "
        r"FROM Files f",
    )
    return {Path(r[0]).name: (r[1], r[2], r[3]) for r in rows}


# ---------------------------------------------------------------------------
# Pool configuration
# ---------------------------------------------------------------------------

@config
def condor_config():
    return {
        "config": {
            "DAEMON_LIST": "$(DAEMON_LIST) LIBRARIAN",
            "LIBRARIAN_UPDATE_INTERVAL": UPDATE_INTERVAL,
            "LIBRARIAN_HIGH_WATER_MARK": HIGH_WATER_MARK,
            "LIBRARIAN_LOW_WATER_MARK": LOW_WATER_MARK,
        }
    }


@standup
def condor(condor_config, test_dir):
    with Condor(local_dir=test_dir / "condor", **condor_config) as condor:
        yield condor


@action
def db_path(condor):
    return Path(_param(condor, "LIBRARIAN_DATABASE"))


# ---------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------

@action
def rotated_files(condor, db_path):
    """
    Write the synthetic archive files (oldest first) and wait for them to be
    indexed. Returns the rotated file paths, oldest first.
    """
    history = Path(_param(condor, "HISTORY"))
    base = int(time.time()) - 10 * 86400

    paths = []
    for n in range(NUM_ROTATED):
        # Rotation suffix sorts oldest-first; one day apart
        stamp = time.strftime("%Y%m%dT%H%M%S", time.localtime(base + n * 86400))
        path = history.parent / f"{history.name}.{stamp}"
        _write_history_file(path, (n + 1) * 100_000, RECORDS_PER_FILE, base + n * 86400)
        paths.append(path)

    # Active file so the newest data stays on disk
    _write_history_file(history, 900_000, 1, int(time.time()))

    total = NUM_ROTATED * RECORDS_PER_FILE + 1
    _wait_for(f"{total} indexed JobRecords",
              lambda: (_scalar(db_path, "SELECT COUNT(*) FROM JobRecords") or 0) >= total)
    return paths


@action
def removed_files(rotated_files, db_path):
    """Remove the oldest rotated files from disk one cycle apart; return them in removal order."""
    removed = rotated_files[:NUM_TO_REMOVE]
    for path in removed:
        path.unlink()
        _wait_for(f"{path.name} to be marked deleted",
                  lambda: (_file_rows(db_path).get(path.name, (None, None))[1] is not None))
        # DateOfDeletion has one-second resolution; keep removals distinct
        time.sleep(1.1)
    return removed


@action
def before_gc(removed_files, db_path):
    """Snapshot sizes and per-file state before GC is enabled."""
    return {
        "file_size": _file_size(db_path),
        "logical_size": _logical_size(db_path),
        "files": _file_rows(db_path),
    }


@action
def after_gc(before_gc, condor, db_path):
    """
    Set the size limit to the current database size (over the high water
    mark), then wait for GC to run and settle. Returns the post-GC snapshot and
    the configured limit.
    """
    limit = before_gc["logical_size"]
    with condor.config_file.open("a") as f:
        f.write(f"\nLIBRARIAN_MAX_DATABASE_SIZE = {limit}\n")
    p = condor.run_command(["condor_reconfig", "-daemon", "librarian"])
    assert p.returncode == 0, f"condor_reconfig -daemon librarian failed: {p.stderr}"

    def eligible_remaining():
        return sum(1 for (_, deleted, _) in _file_rows(db_path).values() if deleted is not None)

    _wait_for("GC to collect at least one removed file", lambda: eligible_remaining() < NUM_TO_REMOVE)

    # Wait until GC stops changing anything for SETTLE_CYCLES update cycles
    last, stable = None, 0
    start = time.time()
    while stable < SETTLE_CYCLES:
        assert time.time() - start <= DB_POLL_TIMEOUT, "GC never settled"
        state = (eligible_remaining(), _logical_size(db_path))
        stable = stable + 1 if state == last else 0
        last = state
        time.sleep(UPDATE_INTERVAL)

    return {
        "limit": limit,
        "file_size": _file_size(db_path),
        "logical_size": _logical_size(db_path),
        "files": _file_rows(db_path),
    }


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestLibrarianGarbageCollectionSize:

    def test_incremental_auto_vacuum(self, rotated_files, db_path):
        """GC relies on auto_vacuum=INCREMENTAL (2) to return freed pages to the OS."""
        assert _scalar(db_path, "PRAGMA auto_vacuum") == 2

    def test_all_files_indexed(self, before_gc):
        files = before_gc["files"]
        rotated = [n for n in files if n != "history"]
        assert len(rotated) == NUM_ROTATED, f"Expected {NUM_ROTATED} rotated files, got {sorted(files)}"
        assert all(files[n][2] == RECORDS_PER_FILE for n in rotated), files

    def test_database_below_high_water_mark(self, after_gc):
        high_water = after_gc["limit"] * HIGH_WATER_MARK
        assert after_gc["logical_size"] <= high_water, (
            f"Database {after_gc['logical_size']} B still above high water mark {high_water:.0f} B "
            f"(main file on disk {after_gc['file_size']} B)"
        )

    def test_database_shrunk_to_low_water_mark(self, after_gc):
        low_water = after_gc["limit"] * LOW_WATER_MARK
        assert after_gc["logical_size"] <= low_water, (
            f"Database {after_gc['logical_size']} B above low water mark {low_water:.0f} B "
            f"(limit {after_gc['limit']} B, main file on disk {after_gc['file_size']} B)"
        )

    def test_file_on_disk_shrunk_to_low_water_mark(self, before_gc, after_gc):
        """
        GC checkpoints the WAL after reclaiming space, so the main database file
        on disk (not just page_count * page_size) must end up at or below the low
        water mark. Before GC nearly everything is still in the WAL, so compare
        against the limit rather than the pre-GC file size.
        """
        low_water = after_gc["limit"] * LOW_WATER_MARK
        file_size, logical_size = after_gc["file_size"], after_gc["logical_size"]
        details = (f"file on disk {file_size} B, database size {logical_size} B, "
                   f"file before GC {before_gc['file_size']} B, low water mark {low_water:.0f} B")
        # Without the checkpoint the main file stays tiny (data lives in the WAL),
        # which would trivially satisfy "<= low water", so require it to reflect the
        # database. Later cycles' Status writes may grow the database a little.
        assert abs(file_size - logical_size) <= 0.1 * logical_size, (
            f"Main database file was not checkpointed after GC: {details}"
        )
        assert file_size <= low_water, f"Database file on disk above low water mark: {details}"

    def test_collects_earliest_removed_first(self, removed_files, after_gc):
        """Collected files must be a prefix of the removal order (GC orders by DateOfDeletion)."""
        files = after_gc["files"]
        collected = [p.name not in files for p in removed_files]
        assert any(collected), "GC collected no files"
        first_kept = collected.index(False) if False in collected else len(collected)
        assert all(collected[:first_kept]) and not any(collected[first_kept:]), (
            f"GC did not collect in removal order: collected flags {collected} for "
            f"{[p.name for p in removed_files]}"
        )

    def test_does_not_over_collect(self, removed_files, after_gc):
        """Reaching the low water mark needs only part of the eligible data; GC shouldn't take it all."""
        files = after_gc["files"]
        assert any(p.name in files for p in removed_files), (
            "GC collected every eligible file instead of stopping at the low water mark"
        )

    def test_files_on_disk_untouched(self, rotated_files, after_gc):
        files = after_gc["files"]
        for path in rotated_files[NUM_TO_REMOVE:]:
            assert path.name in files and files[path.name][2] == RECORDS_PER_FILE, (
                f"{path.name} (still on disk) lost records: {files.get(path.name)}"
            )
        assert "history" in files and files["history"][2] == 1
