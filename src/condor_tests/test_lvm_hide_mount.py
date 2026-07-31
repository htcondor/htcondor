#!/usr/bin/env pytest

# Regression test for LVM_HIDE_MOUNT: the per-job LV mount should be
# invisible on the host when hidden, and visible when not. See
# src/condor_startd.V6/VolumeManager.cpp MountFilesystem/CheckHideMount.

import logging
import time

import pytest

from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

SCRATCH_PATH_POLL_TIMEOUT = 120


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@config(params={"hidden": True, "visible": False})
def hide_mount(request):
    return request.param


@standup
def condor(test_dir, thin_provisioning, hide_mount):
    with Condor(
        test_dir / "condor",
        config=lvm_config(
            thin_provisioning,
            test_dir / "condor" / "lvm_backing.img",
            extra={"LVM_HIDE_MOUNT": str(hide_mount)},
        ),
    ) as condor:
        yield condor


@action
def scratch_path_file(test_dir):
    return test_dir / "scratch_path.txt"


@action
def hide_mount_job_hash(scratch_path_file):
    return {
        "shell": f"echo $_CONDOR_SCRATCH_DIR > {scratch_path_file}; sleep 150",
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "hide_mount_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
    }


@action
def running_hide_mount_job(condor, hide_mount_job_hash):
    job = condor.submit({**hide_mount_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.any_running,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    yield job
    # Wait for removal to finish before pool teardown, or condor_off has to
    # wait out its own drain/kill_after timeout for a mid-teardown claim.
    job.remove()
    job.wait(condition=ClusterState.all_terminal, timeout=120, verbose=True)


class TestLVMHideMount:
    def test_mount_visibility_matches_hide_mount_setting(
        self, running_hide_mount_job, scratch_path_file, hide_mount
    ):
        deadline = time.time() + SCRATCH_PATH_POLL_TIMEOUT
        scratch_dir = None
        while time.time() < deadline:
            if scratch_path_file.exists():
                contents = scratch_path_file.read_text().strip()
                if contents:
                    scratch_dir = contents
                    break
            time.sleep(1)

        assert scratch_dir, "job never reported its _CONDOR_SCRATCH_DIR"

        with open("/proc/mounts") as f:
            mounts = f.read()

        visible_on_host = scratch_dir in mounts

        if hide_mount:
            assert not visible_on_host
        else:
            assert visible_on_host
