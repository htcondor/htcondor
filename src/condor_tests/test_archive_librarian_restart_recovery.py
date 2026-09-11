#!/usr/bin/env pytest

# Regression test for condor_librarian: a daemon restart landing between
# "rotation detected" and "rotation fully drained".
#
# Scenario:
#   1. Submit ROTATION_RECOVERY_NUM_JOBS jobs into the active history file,
#      with the librarian's per-cycle record cap pinned to 0 so it cannot
#      drain any of them yet.
#   2. Rotate the history file and confirm the librarian recorded the
#      rotation (DateOfRotation set) without draining it. This is a stable
#      fixed point, not a timing window: Phase 3 of update() reads zero
#      records per cycle no matter how many cycles run while the cap is 0,
#      so there's nothing to race against.
#   3. Raise the cap back up and restart the whole pool (including
#      LIBRARIAN) while the rotated file is still fully undrained.
#   4. Confirm the file eventually reaches FullyRead with no duplicated or
#      lost records, and that removing it from disk afterwards doesn't
#      corrupt its recorded name/rotation date.

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

ROTATION_RECOVERY_NUM_JOBS        = 3
ROTATION_RECOVERY_UPDATE_INTERVAL = 2  # seconds — matches LIBRARIAN_UPDATE_INTERVAL

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_db_path(condor):
    """Return the LIBRARIAN_DATABASE path from the running pool config."""
    with condor.use_config():
        return Path(htcondor2.param["LIBRARIAN_DATABASE"])


def _wait_for_active_file_registered(condor, expected_name, timeout=DB_POLL_TIMEOUT):
    """
    Poll until a Files row exists for `expected_name` with DateOfRotation
    still NULL -- i.e. the librarian has run a reconcile cycle and adopted it
    as the active file. Deliberately does not wait on any JobRecords being
    read: with LIBRARIAN_MAX_UPDATES_PER_CYCLE pinned to 0, none ever will be
    before we rotate below.
    """
    db_path = _get_db_path(condor)
    start = time.time()
    while True:
        assert time.time() - start <= timeout, (
            f"Timed out after {timeout}s waiting for the librarian to "
            f"register {expected_name!r} as the active archive file"
        )
        try:
            conn = sqlite3.connect(str(db_path))
            rows = conn.execute(
                "SELECT FileName FROM Files WHERE DateOfRotation IS NULL"
            ).fetchall()
            conn.close()
        except sqlite3.OperationalError:
            rows = []
        if any(Path(row[0]).name == expected_name for row in rows):
            return
        time.sleep(DB_POLL_INTERVAL)


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
            # Pin ingestion to 0 records/cycle up front. This makes "rotated
            # but not yet drained" a stable fixed point instead of a narrow
            # window we'd otherwise have to catch mid-drain: with the cap at
            # 0, Phase 3 of update() never reads a single record no matter how
            # many cycles run, so FullyRead is guaranteed to stay 0 until we
            # deliberately raise the cap back up (see
            # rotated_file_after_restart). File discovery and rotation
            # detection (Phase 1/1.5) aren't gated by this cap and keep
            # running every cycle regardless.
            "LIBRARIAN_MAX_UPDATES_PER_CYCLE": 0,
        }
    }


@standup
def rotation_recovery_condor(rotation_recovery_config, test_dir):
    with Condor(local_dir=test_dir / "condor_rotation_recovery", **rotation_recovery_config) as condor:
        yield condor


@action
def rotated_file_mid_drain(rotation_recovery_condor, test_dir, path_to_sleep):
    """
    Rotate the active history file while ingestion is capped at 0
    records/cycle, and confirm the librarian recorded the rotation
    (DateOfRotation set) without draining it -- the DB state that must
    survive a daemon restart intact. With the cap at 0 this is a stable
    fixed point rather than a narrow timing window, so there's nothing to
    race against.
    """
    _submit_and_wait(
        rotation_recovery_condor, test_dir / "rr_job.log", path_to_sleep, ROTATION_RECOVERY_NUM_JOBS
    )

    with rotation_recovery_condor.use_config():
        history_path = Path(htcondor2.param["HISTORY"])

    # Confirms the librarian has run a reconcile cycle and adopted "history"
    # as the active file -- not that it has read anything from it.
    _wait_for_active_file_registered(rotation_recovery_condor, history_path.name)

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
def rotated_file_after_restart(rotated_file_mid_drain, rotation_recovery_condor):
    """
    Restart the whole pool (including LIBRARIAN) while the rotated file from
    `rotated_file_mid_drain` still has unread records, then wait for the
    librarian to finish draining it post-restart. Returns the file's final
    Files row: (FileName, DateOfRotation, FullyRead, RecordsRead, DateOfDeletion).
    """
    # Ingestion was pinned to 0/cycle purely to land rotation in a
    # deterministically undrained state; raise it back up so the post-restart
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

    def test_no_duplicate_or_lost_records(self, rotated_file_after_restart):
        """RecordsRead must equal exactly the original job count -- no records
        re-read (duplicated) or skipped across the restart."""
        _, _, _, records_read, _ = rotated_file_after_restart
        assert records_read == ROTATION_RECOVERY_NUM_JOBS, (
            f"Expected exactly {ROTATION_RECOVERY_NUM_JOBS} records read from the "
            f"rotated file, got {records_read} (duplicate or missed reads across restart)"
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
