#!/usr/bin/env pytest

# Regression test for LVM-cleanup-failure "broken slots". See
# src/condor_starter.V6.1/starter.cpp (post-job lvremove failure ->
# STARTER_EXIT_IMMORTAL_LVM), src/condor_startd.V6/claim.cpp (marks the
# d-slot broken instead of freeing it), and ResMgr::RestoreBrokenResources
# (reclaims it once cleanup succeeds).
#
# Recipe: submit a short job, hold its LV mount busy from a subprocess
# (LVM_HIDE_MOUNT=False makes the mount visible outside the job's
# namespace) cd'd into its scratch dir, let the job finish, confirm the
# slot is marked broken, then release the mount and confirm reclaim.
#
# The holder runs as a subprocess rather than os.chdir() in pytest itself,
# to avoid disturbing the shared per-test working directory.

import logging
import os
import subprocess
import time
from pathlib import Path

import pytest

import htcondor2 as htcondor
from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config, LV_BROKEN_REASON

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

SCRATCH_PATH_POLL_TIMEOUT = 120
HOLDER_CWD_POLL_TIMEOUT = 60
BROKEN_SLOT_POLL_TIMEOUT = 120
RECLAIM_POLL_TIMEOUT = 120
# Must outlast BROKEN_SLOT_POLL_TIMEOUT so the mount stays busy throughout.
MOUNT_HOLDER_LIFETIME = 150
# Marker file the job polls for in its own scratch dir, signaling the
# holder subprocess has cd'd in and it's safe to exit.
HOLDER_READY_FILENAME = "holder_ready"


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@standup
def condor(test_dir, thin_provisioning):
    with Condor(
        test_dir / "condor",
        config=lvm_config(
            thin_provisioning,
            test_dir / "condor" / "lvm_backing.img",
            extra={
                "LVM_HIDE_MOUNT": "False",
                # Default cleanup-reminder retry timer is 62s; speed it up
                # so reclaim-after-release is fast enough for a test.
                "STARTD_CLEANUP_REMINDER_TIMER_INTERVAL": "3",
            },
        ),
    ) as condor:
        yield condor


@action
def scratch_path_file(test_dir):
    return test_dir / "scratch_path.txt"


@action
def short_job_hash(scratch_path_file):
    # Report our scratch dir, then wait for HOLDER_READY_FILENAME before
    # exiting, so we never race the holder subprocess's setup.
    return {
        "shell": (
            f"echo $_CONDOR_SCRATCH_DIR > {scratch_path_file}; "
            f'while [ ! -f "$_CONDOR_SCRATCH_DIR/{HOLDER_READY_FILENAME}" ]; do sleep 0.2; done'
        ),
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "broken_slot_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
    }


def _wait_for_scratch_path(scratch_path_file, timeout=SCRATCH_PATH_POLL_TIMEOUT):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if scratch_path_file.exists():
            contents = scratch_path_file.read_text().strip()
            if contents:
                return contents
        time.sleep(1)
    return None


def _wait_for_holder_cwd(pid, expected_cwd, timeout=HOLDER_CWD_POLL_TIMEOUT):
    # Confirms the holder subprocess's cd actually landed, rather than
    # assuming it completes within some guessed delay.
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            if os.readlink(f"/proc/{pid}/cwd") == expected_cwd:
                return True
        except OSError:
            pass  # process not there yet -- keep polling
        time.sleep(0.1)
    return False


def _broken_slot_ads(condor):
    # The d-slot's own SlotBrokenReason disappears when Resource.cpp
    # deletes/merges it back into its parent right after marking it broken.
    # ResMgr tracks broken resources separately (ResMgr::broken_things),
    # published as BrokenReasons on the startd's daemon ad -- the same data
    # `condor_status -startd -broken` reads -- so query that instead.
    ads = condor.status(
        ad_type=htcondor.AdTypes.StartDaemon,
        constraint="size(BrokenReasons) > 0",
    )
    matches = []
    for ad in ads:
        for key in ad.keys():
            if key.endswith("BrokenReason") and ad.get(key) == LV_BROKEN_REASON:
                matches.append(ad)
                break
    return matches


def _poll_until(predicate, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(2)
    return None


@action
def broken_slot_scenario(condor, short_job_hash, scratch_path_file):
    job = condor.submit({**short_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.any_running,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )

    scratch_dir = _wait_for_scratch_path(scratch_path_file)
    assert scratch_dir, "job never reported its _CONDOR_SCRATCH_DIR"

    # Park a background shell with its cwd inside the job's LV mount, so
    # the starter's post-job lvremove hits EBUSY.
    holder = subprocess.Popen(
        ["/bin/sh", "-c", f"cd '{scratch_dir}' && exec sleep {MOUNT_HOLDER_LIFETIME}"]
    )
    try:
        assert _wait_for_holder_cwd(holder.pid, os.path.realpath(scratch_dir)), (
            "holder subprocess never actually cd'd into the job's scratch dir"
        )
        # Signal the job it's safe to exit now the mount is held busy.
        Path(scratch_dir, HOLDER_READY_FILENAME).touch()

        # Let the job finish naturally while the mount is held busy.
        assert job.wait(condition=ClusterState.all_complete, timeout=120, verbose=True)

        found_broken = _poll_until(
            lambda: _broken_slot_ads(condor), BROKEN_SLOT_POLL_TIMEOUT
        )
    finally:
        holder.terminate()
        holder.wait(timeout=60)

    reclaimed = _poll_until(lambda: not _broken_slot_ads(condor), RECLAIM_POLL_TIMEOUT)

    return {
        "found_broken": bool(found_broken),
        "reclaimed": bool(reclaimed),
    }


class TestLVMBrokenSlot:
    def test_slot_marked_broken_while_lv_is_busy(self, broken_slot_scenario):
        assert broken_slot_scenario["found_broken"], (
            "expected the startd's daemon ad to report a BrokenReasons "
            f"entry == '{LV_BROKEN_REASON}' while the LV mount was held busy"
        )

    def test_broken_slot_eventually_reclaimed(self, broken_slot_scenario):
        assert broken_slot_scenario["reclaimed"], (
            "broken slot was never reclaimed after the busy mount was released"
        )
