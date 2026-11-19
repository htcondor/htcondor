#!/usr/bin/env pytest

# HTCONDOR-4096: if a local universe starter crashes, it can't remove its
# own scratch directory (LOCAL_UNIV_EXECUTE/dir_<starter pid>), so the
# schedd must remove it when it reaps the starter.

import logging
import os
import signal
import time

from ornithology import (
    action,
    ClusterState,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def wait_for(predicate, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(0.5)
    return predicate()


def scratch_dirs(condor):
    return sorted(condor.local_univ_execute_dir.glob("dir_*"))


@action
def running_local_job(default_condor, path_to_sleep, test_dir):
    handle = default_condor.submit(
        {
            "universe": "local",
            "executable": path_to_sleep,
            "arguments": "600",
            "log": (test_dir / "local.log").as_posix(),
        }
    )
    assert handle.wait(condition=ClusterState.all_running, timeout=60)
    yield handle
    handle.remove()


@action
def crashed_starter_scratch_dir(default_condor, running_local_job):
    # The schedd marks the job running when it spawns the starter, which
    # may be slightly before the starter creates its scratch directory.
    assert wait_for(lambda: len(scratch_dirs(default_condor)) == 1)
    scratch_dir = scratch_dirs(default_condor)[0]
    starter_pid = int(scratch_dir.name[len("dir_"):])

    logger.info(f"SIGKILLing local starter pid {starter_pid}, scratch dir {scratch_dir}")
    os.kill(starter_pid, signal.SIGKILL)

    return scratch_dir, starter_pid


class TestLocalUniverseScratchCleanup:
    def test_schedd_logs_cleanup(self, default_condor, crashed_starter_scratch_dir):
        _, starter_pid = crashed_starter_scratch_dir
        assert default_condor.schedd_log.open().wait(
            condition=lambda msg: f"Local starter pid {starter_pid} left behind" in msg.message,
            timeout=60,
        )

    def test_scratch_dir_removed(self, default_condor, crashed_starter_scratch_dir):
        scratch_dir, _ = crashed_starter_scratch_dir
        assert wait_for(lambda: not scratch_dir.exists())
