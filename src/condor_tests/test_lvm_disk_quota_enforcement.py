#!/usr/bin/env pytest

# Regression test for STARTD_ENFORCE_DISK_LIMITS. A job whose scratch dir
# grows past its LV size is held with HoldReasonCode=JobOutOfResources (34),
# HoldReasonSubCode=Disk (104). See src/condor_starter.V6.1/starter.cpp
# CheckLVUsage() and src/condor_utils/condor_holdcodes.h.

import logging
import pytest

from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config, SMALL_BACKING_FILE_MB

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

HOLD_REASON_CODE_JOB_OUT_OF_RESOURCES = 34
HOLD_REASON_SUBCODE_DISK = 104


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@standup
def condor(test_dir, thin_provisioning):
    # Thick mode needs a bigger LV (and backing store) than thin to reliably
    # trigger the hold -- see overfill_job_hash() below.
    backing_size_mb = SMALL_BACKING_FILE_MB if thin_provisioning else 1200
    extra = None
    if not thin_provisioning:
        # Default LVM_THICK_LV_MARGIN (0.98) is above real ext4 usable space
        # at this LV size; lower it so the hold is actually reachable.
        extra = {"LVM_THICK_LV_MARGIN": "0.90"}
    with Condor(
        test_dir / "condor",
        config=lvm_config(
            thin_provisioning,
            test_dir / "condor" / "lvm_backing.img",
            backing_size_mb=backing_size_mb,
            extra=extra,
        ),
    ) as condor:
        yield condor


@action
def overfill_job_hash(thin_provisioning):
    if thin_provisioning:
        request_disk_mb = 48
        overfill_mb = 96
    else:
        # A small thick LV's ext4 overhead leaves too little margin to
        # reach the hold threshold before ENOSPC; use a bigger LV instead.
        request_disk_mb = 512
        overfill_mb = 600
    return {
        "shell": f"(fallocate -l {overfill_mb}M overfill.bin || dd if=/dev/zero of=overfill.bin bs=1M count={overfill_mb}); sleep 60",
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "overfill_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": f"{request_disk_mb}m",
        # Forces the job to actually run in the LVM-mounted scratch dir;
        # same-host IF_NEEDED transfer would otherwise use the submit dir.
        "should_transfer_files": "YES",
    }


@action
def overfilled_job(condor, overfill_job_hash):
    job = condor.submit({**overfill_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_held,
        timeout=180,
        verbose=True,
        fail_condition=ClusterState.any_complete,
    )
    return job


@action
def underquota_job_hash():
    return {
        "shell": "(fallocate -l 8M smallfile.bin || dd if=/dev/zero of=smallfile.bin bs=1M count=8)",
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "underquota_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
        "should_transfer_files": "YES",
    }


@action
def underquota_completed_job(condor, underquota_job_hash):
    job = condor.submit({**underquota_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=180,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job


class TestLVMDiskQuotaEnforcement:
    def test_overfill_job_held_out_of_resources(self, overfilled_job):
        assert overfilled_job.state.all_held()
        ads = overfilled_job.query(
            projection=["HoldReasonCode", "HoldReasonSubCode", "HoldReason"]
        )
        assert len(ads) == 1
        for ad in ads:
            assert ad["HoldReasonCode"] == HOLD_REASON_CODE_JOB_OUT_OF_RESOURCES
            assert ad["HoldReasonSubCode"] == HOLD_REASON_SUBCODE_DISK
            assert "disk" in ad["HoldReason"].lower()

    def test_underquota_job_completes(self, underquota_completed_job):
        assert underquota_completed_job.state.all_complete()
