#!/usr/bin/env pytest

# Regression test for condor_librarian: a daemon restart landing between
# "rotation detected" and "rotation fully drained".
#
# Scenario:
#   1. Index ROTATION_RECOVERY_JOBS_READ jobs from the active history file so
#      the file has a non-zero LastOffset/RecordsRead.
#   2. Pin the librarian's per-cycle record cap to 0 (via a librarian-only
#      restart, so the new cap is in effect before its first update cycle)
#      and complete ROTATION_RECOVERY_JOBS_UNREAD more jobs it cannot read.
#   3. Rotate the history file and confirm the librarian recorded the
#      rotation (DateOfRotation set) without draining it. This is a stable
#      fixed point, not a timing window: Phase 3 of update() reads zero
#      records per cycle no matter how many cycles run while the cap is 0,
#      so there's nothing to race against.
#   4. Raise the cap back up and restart the whole pool (including
#      LIBRARIAN) while the rotated file is partially drained.
#   5. Confirm the file eventually reaches FullyRead with every record indexed
#      exactly once -- a lost LastOffset would re-index the first batch, and
#      a lost RecordsRead would undercount -- and that removing it from disk
#      afterwards doesn't corrupt its recorded name/rotation date.
#
# Second scenario (separate pool): per-file read statistics survive a
# librarian restart on a partially read active file.
#   1. Index ACTIVE_RECOVERY_JOBS_BEFORE jobs into the active history file.
#   2. Restart only the librarian (the schedd keeps appending to the same file).
#   3. Index ACTIVE_RECOVERY_JOBS_AFTER more jobs.
#   4. Confirm the same Files row now has RecordsRead == before + after and an
#      AvgRecordSize consistent with the pre-restart value. If recovery lost
#      these, the Welford mean would restart from zero and RecordsRead would
#      only count post-restart records.

import datetime
import os
from pathlib import Path
import sqlite3
import time
import htcondor2

import pytest

from ornithology import *

DB_POLL_INTERVAL = 2    # seconds between DB polls
DB_POLL_TIMEOUT  = 120  # seconds before giving up

ROTATION_RECOVERY_JOBS_READ       = 3  # indexed before the cap is pinned to 0
ROTATION_RECOVERY_JOBS_UNREAD     = 2  # left unread in the rotated file at restart
ROTATION_RECOVERY_NUM_JOBS        = ROTATION_RECOVERY_JOBS_READ + ROTATION_RECOVERY_JOBS_UNREAD
ROTATION_RECOVERY_UPDATE_INTERVAL = 2  # seconds — matches LIBRARIAN_UPDATE_INTERVAL

ACTIVE_RECOVERY_JOBS_BEFORE = 3
ACTIVE_RECOVERY_JOBS_AFTER  = 2

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_db_path(condor):
    """Return the LIBRARIAN_DATABASE path from the running pool config."""
    with condor.use_config():
        return Path(htcondor2.param["LIBRARIAN_DATABASE"])


def _restart_pool_and_wait_for_new_schedd(condor, timeout=60):
    """
    Restart every daemon under condor_master (including LIBRARIAN, since it's
    in DAEMON_LIST) and block until a new SCHEDD process is confirmed alive.
    SCHEDD's pid is used as the restart signal since condor_who reports it
    reliably; LIBRARIAN restarts as part of the same condor_restart call.
    """
    who = condor.run_command(["condor_who", "-quick"])
    assert who.returncode == 0, "Failed to query daemon information with condor_who"
    old_pid = None
    for line in who.stdout.split("\n"):
        if line.startswith("SCHEDD_PID"):
            old_pid = line.split("=")[1]
            break
    assert old_pid is not None, "Failed to get schedd pid before restart"

    condor.run_command(["condor_restart", "-fast"])

    start = time.time()
    while True:
        assert time.time() - start <= timeout, "Failed to restart condor"

        who = condor.run_command(["condor_who", "-quick"])
        if who.returncode == 0:
            alive = False
            pid = None
            for line in who.stdout.split("\n"):
                if line.startswith("SCHEDD_PID"):
                    pid = line.split("=")[1]
                elif line.startswith("SCHEDD =") and '"alive"' in line.lower():
                    alive = True

            if alive and pid is not None and pid != old_pid:
                return

        time.sleep(1)


def _wait_for_rotation_recorded(condor, fully_read, timeout=DB_POLL_TIMEOUT):
    """Poll until some Files row has DateOfRotation set and FullyRead == `fully_read` (0 or 1)."""
    db_path = _get_db_path(condor)
    start = time.time()
    while True:
        assert time.time() - start <= timeout, (
            f"Timed out waiting for a rotated file with FullyRead={fully_read}"
        )
        try:
            conn = sqlite3.connect(str(db_path))
            row = conn.execute(
                r"SELECT FileId FROM Files WHERE DateOfRotation IS NOT NULL AND FullyRead = ?",
                (fully_read,),
            ).fetchone()
            conn.close()
        except sqlite3.OperationalError:
            row = None
        if row is not None:
            return
        time.sleep(DB_POLL_INTERVAL)


def _active_file_row(condor):
    """Return (FileId, RecordsRead, AvgRecordSize, JobRecords count) for the active file, or None."""
    try:
        conn = sqlite3.connect(str(_get_db_path(condor)))
        try:
            row = conn.execute(
                r"SELECT FileId, RecordsRead, AvgRecordSize FROM Files WHERE DateOfRotation IS NULL"
            ).fetchone()
            records = conn.execute(r"SELECT COUNT(*) FROM JobRecords").fetchone()[0]
        finally:
            conn.close()
    except sqlite3.OperationalError:
        return None
    return None if row is None else (*row, records)


def _wait_for_indexed_records(condor, expected, timeout=DB_POLL_TIMEOUT):
    """Poll until JobRecords holds `expected` rows; return _active_file_row()."""
    start = time.time()
    row = None
    while True:
        assert time.time() - start <= timeout, (
            f"Timed out waiting for {expected} indexed records (last active file row: {row})"
        )
        row = _active_file_row(condor)
        if row is not None and row[3] >= expected:
            return row
        time.sleep(DB_POLL_INTERVAL)


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
# Pool configuration + fixtures: restart while a rotated file is mid-drain
# ---------------------------------------------------------------------------

@config
def rotation_recovery_config():
    return {
        "config": {
            "DAEMON_LIST": "$(DAEMON_LIST) LIBRARIAN",
            "LIBRARIAN_UPDATE_INTERVAL": ROTATION_RECOVERY_UPDATE_INTERVAL,
        }
    }


@standup
def rotation_recovery_condor(rotation_recovery_config, test_dir):
    with Condor(local_dir=test_dir / "condor_rotation_recovery", **rotation_recovery_config) as condor:
        yield condor


@action
def rotated_file_mid_drain(rotation_recovery_condor, test_dir, path_to_sleep):
    """
    Index a first batch of jobs, pin ingestion to 0 records/cycle, complete a
    second batch the librarian can't read, then rotate the active history
    file and confirm the librarian recorded the rotation (DateOfRotation set)
    without draining it -- the partially read DB state that must survive a
    daemon restart intact. With the cap at 0 this is a stable fixed point
    rather than a narrow timing window, so there's nothing to race against.
    """
    _submit_and_wait(
        rotation_recovery_condor, test_dir / "rr_job_read.log", path_to_sleep, ROTATION_RECOVERY_JOBS_READ
    )
    _wait_for_indexed_records(rotation_recovery_condor, ROTATION_RECOVERY_JOBS_READ)

    # Pin ingestion to 0 records/cycle. File discovery and rotation detection
    # (Phase 1/1.5) aren't gated by this cap and keep running every cycle.
    # A librarian-only restart (rather than condor_reconfig, which gives no
    # signal of when it has been applied) guarantees the cap is in effect
    # before the next batch of records is written. The schedd is untouched,
    # so it keeps appending to the same "history" file.
    with rotation_recovery_condor.config_file.open("a") as f:
        f.write("\nLIBRARIAN_MAX_UPDATES_PER_CYCLE = 0\n")
    rotation_recovery_condor.restart_daemon("librarian")

    _submit_and_wait(
        rotation_recovery_condor, test_dir / "rr_job_unread.log", path_to_sleep, ROTATION_RECOVERY_JOBS_UNREAD
    )

    with rotation_recovery_condor.use_config():
        history_path = Path(htcondor2.param["HISTORY"])

    timestamp = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
    rotated_path = history_path.parent / f"{history_path.name}.{timestamp}"
    history_path.rename(rotated_path)
    os.utime(str(rotated_path), None)

    # NOTE: deliberately no job submitted here. The schedd's file descriptor
    # for "history" stays open across the rename (POSIX renames don't affect
    # already-open fds), so any job completed now would be appended to the
    # just-rotated file's old inode rather than a fresh "history" -- rotation
    # detection itself doesn't need a new active file to exist on disk; it
    # matches the rotated path against the in-memory/recovered active entry.
    _wait_for_rotation_recorded(rotation_recovery_condor, fully_read=0)

    return rotated_path


@action
def rotated_file_before_restart(rotated_file_mid_drain, rotation_recovery_condor):
    """(FileId, RecordsRead, LastOffset, JobRecords count) of the rotated file before the pool restart."""
    conn = sqlite3.connect(str(_get_db_path(rotation_recovery_condor)))
    try:
        row = conn.execute(
            r"SELECT FileId, RecordsRead, LastOffset FROM Files WHERE FileName LIKE ?",
            (f"%{rotated_file_mid_drain.name}%",),
        ).fetchone()
        records = conn.execute(r"SELECT COUNT(*) FROM JobRecords").fetchone()[0]
    finally:
        conn.close()
    assert row is not None, f"No Files row for rotated file {rotated_file_mid_drain.name}"
    return (*row, records)


@action
def rotated_file_after_restart(rotated_file_mid_drain, rotated_file_before_restart, rotation_recovery_condor):
    """
    Restart the whole pool (including LIBRARIAN) while the rotated file from
    `rotated_file_mid_drain` still has unread records, then wait for the
    librarian to finish draining it post-restart. Returns the file's final
    Files row: (FileName, DateOfRotation, FullyRead, RecordsRead, DateOfDeletion).
    """
    # Ingestion was pinned to 0/cycle purely to land rotation in a
    # deterministically partially drained state; raise it back up so the post-restart
    # librarian actually drains the backlog instead of holding at 0 forever.
    # condor_restart re-reads the on-disk config file, so editing it here
    # (rather than e.g. `condor_config_val -rset`, which wouldn't survive a
    # full daemon restart) is what actually takes effect.
    with rotation_recovery_condor.config_file.open("a") as f:
        f.write("\nLIBRARIAN_MAX_UPDATES_PER_CYCLE = 100000\n")

    _restart_pool_and_wait_for_new_schedd(rotation_recovery_condor)

    db_path = _get_db_path(rotation_recovery_condor)
    start = time.time()
    row = None
    while True:
        assert time.time() - start <= DB_POLL_TIMEOUT, (
            f"Rotated file never reached FullyRead after librarian restart (last row: {row})"
        )
        conn = sqlite3.connect(str(db_path))
        row = conn.execute(
            r"SELECT FileName, DateOfRotation, FullyRead, RecordsRead, DateOfDeletion "
            r"FROM Files WHERE FileName LIKE ?",
            (f"%{rotated_file_mid_drain.name}%",),
        ).fetchone()
        conn.close()
        if row is not None and row[2] == 1:
            return row
        time.sleep(DB_POLL_INTERVAL)


# ---------------------------------------------------------------------------
# Pool configuration + fixtures: librarian restart with a partially read active file
# ---------------------------------------------------------------------------

@config
def active_recovery_config():
    return {
        "config": {
            "DAEMON_LIST": "$(DAEMON_LIST) LIBRARIAN",
            "LIBRARIAN_UPDATE_INTERVAL": ROTATION_RECOVERY_UPDATE_INTERVAL,
        }
    }


@standup
def active_recovery_condor(active_recovery_config, test_dir):
    with Condor(local_dir=test_dir / "condor_active_recovery", **active_recovery_config) as condor:
        yield condor


@action
def active_file_before_restart(active_recovery_condor, test_dir, path_to_sleep):
    """Index the first batch of jobs; return the active file's row before the restart."""
    _submit_and_wait(
        active_recovery_condor, test_dir / "ar_job_before.log", path_to_sleep, ACTIVE_RECOVERY_JOBS_BEFORE
    )
    return _wait_for_indexed_records(active_recovery_condor, ACTIVE_RECOVERY_JOBS_BEFORE)


@action
def active_file_after_restart(active_file_before_restart, active_recovery_condor, test_dir, path_to_sleep):
    """
    Restart only the librarian, index a second batch into the same active
    file, and return the active file's row afterwards. The Files row is
    updated in the same transaction as the JobRecords insert, so once the
    record count is reached the row reflects the post-restart reads.
    """
    active_recovery_condor.restart_daemon("librarian")
    _submit_and_wait(
        active_recovery_condor, test_dir / "ar_job_after.log", path_to_sleep, ACTIVE_RECOVERY_JOBS_AFTER
    )
    return _wait_for_indexed_records(
        active_recovery_condor, ACTIVE_RECOVERY_JOBS_BEFORE + ACTIVE_RECOVERY_JOBS_AFTER
    )


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestLibrarianRestartMidRotation:
    """
    Regression coverage for a librarian restart landing between "rotation
    detected" and "rotation fully drained". Before the fix, recovery wiped
    ArchiveFile::rotation_time, so the rotated file was mistaken for the
    still-active file: it never reached FullyRead, and removing it from disk
    misrenamed it to "<name>.REMOVED" with DateOfRotation reset to 0.
    """

    def test_rotation_time_survives_restart(self, rotated_file_after_restart):
        filename, date_of_rotation, fully_read, _, _ = rotated_file_after_restart
        assert date_of_rotation is not None and date_of_rotation > 0, (
            "DateOfRotation was lost across a librarian restart -- the rotated "
            "file was mistaken for the active file on recovery"
        )
        assert fully_read == 1, "Rotated file never reached FullyRead after restart"
        assert not filename.endswith(".REMOVED"), (
            f"Rotated file name was corrupted to {filename!r}"
        )

    def test_partially_read_before_restart(self, rotated_file_before_restart):
        """Precondition: the rotated file must be partially read when the pool restarts,
        otherwise the checks below can't tell recovered progress from lost progress."""
        _, records_read, last_offset, records = rotated_file_before_restart
        assert records_read == ROTATION_RECOVERY_JOBS_READ
        assert last_offset > 0
        assert records == ROTATION_RECOVERY_JOBS_READ, (
            f"Expected {ROTATION_RECOVERY_JOBS_READ} JobRecords with ingestion pinned to 0, got {records}"
        )

    def test_no_duplicate_or_lost_records(self, rotated_file_after_restart, rotation_recovery_condor):
        """Every record in the rotated file must be indexed exactly once across
        the restart: a lost LastOffset re-indexes the first batch (duplicates),
        and a lost RecordsRead undercounts."""
        _, _, _, records_read, _ = rotated_file_after_restart
        assert records_read == ROTATION_RECOVERY_NUM_JOBS, (
            f"Expected exactly {ROTATION_RECOVERY_NUM_JOBS} records read from the "
            f"rotated file, got {records_read} (duplicate or missed reads across restart)"
        )
        conn = sqlite3.connect(str(_get_db_path(rotation_recovery_condor)))
        try:
            records = conn.execute(r"SELECT COUNT(*) FROM JobRecords").fetchone()[0]
        finally:
            conn.close()
        assert records == ROTATION_RECOVERY_NUM_JOBS, (
            f"Expected exactly {ROTATION_RECOVERY_NUM_JOBS} JobRecords, got {records} "
            "(records re-indexed or dropped across restart)"
        )

    def test_removal_after_restart_keeps_rotation_metadata(
        self, rotated_file_mid_drain, rotated_file_after_restart, rotation_recovery_condor
    ):
        """
        Once the rotated file is fully drained and later removed from disk, it
        must be marked deleted under its real name and rotation date -- not
        misrenamed as if it were the active file (the symptom seen in production).
        """
        rotated_file_mid_drain.unlink()

        db_path = _get_db_path(rotation_recovery_condor)
        start = time.time()
        row = None
        while True:
            assert time.time() - start <= DB_POLL_TIMEOUT, (
                f"Deleted rotated file was never marked DateOfDeletion (last row: {row})"
            )
            conn = sqlite3.connect(str(db_path))
            row = conn.execute(
                r"SELECT FileName, DateOfRotation, DateOfDeletion FROM Files WHERE FileName LIKE ?",
                (f"%{rotated_file_mid_drain.name}%",),
            ).fetchone()
            conn.close()
            if row is not None and row[2] is not None:
                break
            time.sleep(DB_POLL_INTERVAL)

        filename, date_of_rotation, date_of_deletion = row
        assert date_of_rotation is not None and date_of_rotation > 0, (
            "DateOfRotation was lost by the time the rotated file was removed from disk"
        )
        assert date_of_deletion is not None
        assert not filename.endswith(".REMOVED"), (
            f"A rotated file removed from disk was misrenamed to {filename!r} -- "
            "treated as an unexpectedly-removed active file instead of a rotated one"
        )


class TestLibrarianRestartActiveFileStats:
    """
    Regression coverage for per-file read statistics (RecordsRead,
    AvgRecordSize) being recovered from the DB when the librarian restarts
    while the active file is only partially read.
    """

    def test_before_restart_stats(self, active_file_before_restart):
        _, records_read, avg_size, _ = active_file_before_restart
        assert records_read == ACTIVE_RECOVERY_JOBS_BEFORE
        assert avg_size > 0

    def test_same_active_file_after_restart(self, active_file_before_restart, active_file_after_restart):
        assert active_file_after_restart[0] == active_file_before_restart[0], (
            "Active file was re-registered under a new FileId after the librarian restart"
        )

    def test_no_records_reindexed(self, active_file_after_restart):
        total = ACTIVE_RECOVERY_JOBS_BEFORE + ACTIVE_RECOVERY_JOBS_AFTER
        assert active_file_after_restart[3] == total, (
            f"Expected exactly {total} JobRecords, got {active_file_after_restart[3]} "
            "(records re-read from the start of the file after restart)"
        )

    def test_records_read_recovered(self, active_file_after_restart):
        total = ACTIVE_RECOVERY_JOBS_BEFORE + ACTIVE_RECOVERY_JOBS_AFTER
        records_read = active_file_after_restart[1]
        assert records_read == total, (
            f"Expected RecordsRead == {total}, got {records_read}; "
            f"{ACTIVE_RECOVERY_JOBS_AFTER} would mean the pre-restart count was lost"
        )

    def test_avg_record_size_recovered(self, active_file_before_restart, active_file_after_restart):
        # Identical sleep jobs produce near-identical record sizes. A mean that
        # restarted from 0 with the count kept would land around 0.4x here.
        before, after = active_file_before_restart[2], active_file_after_restart[2]
        assert 0.8 * before <= after <= 1.2 * before, (
            f"AvgRecordSize changed from {before:.1f} to {after:.1f} across the librarian restart"
        )
