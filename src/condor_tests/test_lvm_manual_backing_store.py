#!/usr/bin/env pytest

# Verifies a manually pre-configured LVM backing store (LVM_VOLUME_GROUP_NAME
# pointed at a pre-existing VG) is used instead of the startd's automatic
# loopback/PV/VG setup. See src/condor_startd.V6/VolumeManager.cpp's manual
# vs. auto-loopback branches.

import logging
import pytest

from ornithology import *

from liblvm import (
    LVMTestable,
    LVM_SKIP_REASON,
    ManualLVMStore,
    SMALL_BACKING_FILE_MB,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@standup
def manual_store(test_dir, thin_provisioning):
    vg_name = "condor_test_manual_vg"
    thinpool_name = "condor_test_manual_thinpool" if thin_provisioning else None

    with ManualLVMStore(
        test_dir / "manual_lvm_backing.img",
        vg_name,
        size_mb=SMALL_BACKING_FILE_MB,
        thinpool_name=thinpool_name,
    ) as store:
        yield store


@standup
def condor(test_dir, thin_provisioning, manual_store):
    lvm_conf = {
        "STARTD_ENFORCE_DISK_LIMITS": "True",
        "LVM_VOLUME_GROUP_NAME": manual_store.vg_name,
    }
    if thin_provisioning:
        lvm_conf["LVM_USE_THIN_PROVISIONING"] = "True"
        lvm_conf["LVM_THINPOOL_NAME"] = manual_store.thinpool_name

    with Condor(test_dir / "condor", config=lvm_conf) as condor:
        yield condor


@action
def manual_store_job_hash():
    return {
        "shell": "(fallocate -l 8M smallfile.bin || dd if=/dev/zero of=smallfile.bin bs=1M count=8)",
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "manual_store_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
    }


@action
def manual_store_job(condor, manual_store_job_hash):
    job = condor.submit({**manual_store_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job


class TestLVMManualBackingStore:
    def test_job_runs_against_manual_store(self, manual_store_job):
        assert manual_store_job.state.all_complete()

    def test_no_auto_backing_file_was_created(self, test_dir, condor, manual_store_job):
        # Manual mode never calls CreateLoopback(); this must not exist.
        auto_backing_file = test_dir / "condor" / "spool" / "startd_disk.img"
        assert not auto_backing_file.exists()

    def test_advertised_disk_matches_manual_store_size(self, condor, manual_store_job):
        import htcondor2 as htcondor

        ads = condor.status(
            ad_type=htcondor.AdTypes.Startd,
            projection=["Name", "PartitionableSlot", "TotalSlotDisk"],
        )
        matching = [ad for ad in ads if ad.get("PartitionableSlot")]
        assert len(matching) >= 1

        # Confirms the manual VG was picked up, not some unrelated default.
        backing_kb = SMALL_BACKING_FILE_MB * 1024
        assert matching[0]["TotalSlotDisk"] < backing_kb
