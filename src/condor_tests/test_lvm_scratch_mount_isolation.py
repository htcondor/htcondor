#!/usr/bin/env pytest

# Verifies that with STARTD_ENFORCE_DISK_LIMITS on, the job's scratch dir
# is a distinct mounted logical volume, not a subdir of the execute
# partition. See src/condor_startd.V6/VolumeManager.cpp SetupLV/MountFilesystem.

import logging
import pytest

from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@standup
def condor(test_dir, thin_provisioning):
    with Condor(
        test_dir / "condor",
        config=lvm_config(thin_provisioning, test_dir / "condor" / "lvm_backing.img"),
    ) as condor:
        yield condor


@action
def mount_check_job_hash():
    return {
        "shell": (
            "echo DEV_HERE=$(stat -c %d .) > devinfo; "
            "echo DEV_PARENT=$(stat -c %d ..) >> devinfo; "
            # /proc/self/mounts instead of findmnt, which may not be on
            # the job's minimal default PATH.
            # $(DOLLAR) escapes a literal "$" past condor_submit's own
            # macro expansion of "shell" (SubmitHash::SetArguments()).
            "awk -v d=\"$(DOLLAR)(pwd)\" '$2==d{print \"FSTYPE=\"$3}' /proc/self/mounts >> devinfo"
        ),
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "mount_check_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
        "transfer_output_files": "devinfo",
        # Forces the job to actually run in the LVM-mounted scratch dir;
        # same-host IF_NEEDED transfer would otherwise use the submit dir.
        "should_transfer_files": "YES",
    }


@action
def mount_check_job(condor, mount_check_job_hash):
    job = condor.submit({**mount_check_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job


class TestLVMScratchMountIsolation:
    def test_scratch_dir_is_separate_mount(self, mount_check_job):
        info = {}
        with open("devinfo") as f:
            for line in f:
                key, _, value = line.strip().partition("=")
                info[key] = value

        assert "DEV_HERE" in info
        assert "DEV_PARENT" in info
        assert info["DEV_HERE"] != info["DEV_PARENT"]

    def test_scratch_dir_is_ext4(self, mount_check_job):
        with open("devinfo") as f:
            contents = f.read()

        assert "FSTYPE=ext4" in contents
