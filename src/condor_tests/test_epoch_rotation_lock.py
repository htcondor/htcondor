#!/usr/bin/env pytest

# Verify the shadow takes the epoch history rotation lock before rotating
# JOB_EPOCH_HISTORY. The test holds the lock itself, forces the shadow's first
# epoch write to need a rotation, and checks that no rotation happens until the
# lock is released.

import os
import sys
import time
from pathlib import Path

from ornithology import *

import htcondor2

# Large enough that one job's epoch records do not trigger a second rotation
MAX_EPOCH_HISTORY_LOG = 256 * 1024
FILLER = ("# filler record to force rotation\n" * ((MAX_EPOCH_HISTORY_LOG // 34) + 64)).encode()

# How long to watch a blocked shadow for an (incorrect) rotation
BLOCKED_OBSERVATION_SECONDS = 10


class ExternalLock:
    """
    Hold the same lock the shadow takes via FileLock::obtain(WRITE_LOCK).
    On Windows FILE_LOCK_VIA_MUTEX is disabled in the test config so the
    shadow uses _locking() on the first 4 bytes, which msvcrt.locking() matches.
    Elsewhere the shadow uses fcntl() record locks, which fcntl.lockf() matches.
    """

    def __init__(self, path):
        self.path = path
        self.fd = None

    def acquire(self):
        self.fd = os.open(str(self.path), os.O_RDWR | os.O_CREAT, 0o666)
        # Ensure the shadow (condor user when run as root) can open it to lock
        os.chmod(str(self.path), 0o666)
        if sys.platform == "win32":
            import msvcrt
            os.lseek(self.fd, 0, os.SEEK_SET)
            msvcrt.locking(self.fd, msvcrt.LK_NBLCK, 4)
        else:
            import fcntl
            fcntl.lockf(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def release(self):
        if self.fd is None:
            return
        if sys.platform == "win32":
            import msvcrt
            os.lseek(self.fd, 0, os.SEEK_SET)
            msvcrt.locking(self.fd, msvcrt.LK_UNLCK, 4)
        else:
            import fcntl
            fcntl.lockf(self.fd, fcntl.LOCK_UN)
        os.close(self.fd)
        self.fd = None


def wait_for_file(path, timeout=120):
    start = time.time()
    while not path.exists():
        if time.time() - start > timeout:
            return False
        time.sleep(0.1)
    return True


def rotated_files(epoch_file):
    return sorted(p for p in epoch_file.parent.glob(f"{epoch_file.name}.*"))


def is_rotation_lock_msg(msg, text, lock_file):
    return text in msg and lock_file.name in msg


@action
def the_condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "MAX_EPOCH_HISTORY_LOG": MAX_EPOCH_HISTORY_LOG,
            "MAX_EPOCH_HISTORY_ROTATIONS": 10,
            "SHADOW_DEBUG": "D_FULLDEBUG",
            # Use a filesystem lock on Windows so the test can hold it
            "FILE_LOCK_VIA_MUTEX": False,
        },
    ) as condor:
        yield condor


@action
def the_epoch_file(the_condor):
    # Use whatever JOB_EPOCH_HISTORY resolves to (default $(SPOOL)/epoch_history)
    with the_condor.use_config():
        return Path(htcondor2.param["JOB_EPOCH_HISTORY"])


@action
def the_lock_file(the_condor):
    # Use whatever EPOCH_HISTORY_LOCK resolves to (default $(LOCK)/_shadow_epoch_history.lock)
    with the_condor.use_config():
        return Path(htcondor2.param["EPOCH_HISTORY_LOCK"])


@action
def the_blocked_run(the_condor, the_epoch_file, the_lock_file, path_to_sleep, test_dir):
    # Stuff the epoch file so the shadow's first (SPAWN) write must rotate
    the_epoch_file.write_bytes(FILLER)
    assert rotated_files(the_epoch_file) == []

    lock = ExternalLock(the_lock_file)
    lock.acquire()
    observed = {}
    try:
        handle = the_condor.submit(
            description={
                "executable": path_to_sleep,
                "arguments": "1",
                "transfer_executable": False,
                "log": (test_dir / "job.log").as_posix(),
            },
            count=1,
        )

        shadow_log_path = the_condor.shadow_log.path
        assert wait_for_file(shadow_log_path), "Shadow never started"
        shadow_log = the_condor.shadow_log.open()

        # The FileLock timestamp update is logged just before obtain(), so the
        # shadow is now blocked on our lock
        observed["reached_lock"] = shadow_log.wait(
            condition=lambda msg: is_rotation_lock_msg(msg, "FileLock object is updating timestamp on:", the_lock_file),
            timeout=120,
        )

        time.sleep(BLOCKED_OBSERVATION_SECONDS)
        observed["rotated_while_held"] = rotated_files(the_epoch_file)
        observed["size_while_held"] = the_epoch_file.stat().st_size
        observed["obtained_while_held"] = any(
            is_rotation_lock_msg(msg, "now WRITE", the_lock_file) for msg in shadow_log.read()
        )
    finally:
        lock.release()

    observed["obtained_after_release"] = shadow_log.wait(
        condition=lambda msg: is_rotation_lock_msg(msg, "now WRITE", the_lock_file),
        timeout=120,
    )
    observed["completed"] = handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=180,
    )
    return observed


class TestEpochRotationLock:

    def test_shadow_reaches_rotation_lock(self, the_blocked_run):
        assert the_blocked_run["reached_lock"]

    def test_no_rotation_while_lock_held(self, the_blocked_run):
        assert the_blocked_run["rotated_while_held"] == []
        assert not the_blocked_run["obtained_while_held"]

    def test_no_append_while_lock_held(self, the_blocked_run):
        assert the_blocked_run["size_while_held"] == len(FILLER)

    def test_shadow_obtains_lock_after_release(self, the_blocked_run):
        assert the_blocked_run["obtained_after_release"]

    def test_job_completes(self, the_blocked_run):
        assert the_blocked_run["completed"]

    def test_rotated_exactly_once(self, the_blocked_run, the_epoch_file):
        backups = rotated_files(the_epoch_file)
        assert len(backups) == 1
        assert backups[0].read_bytes() == FILLER

    def test_records_written_after_rotation(self, the_blocked_run, the_epoch_file):
        contents = the_epoch_file.read_text()
        assert "# filler record" not in contents
        assert "*** SPAWN " in contents
