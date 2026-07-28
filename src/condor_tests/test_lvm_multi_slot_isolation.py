#!/usr/bin/env pytest

# Verifies concurrent jobs each get a distinct logical volume, and that one
# job overrunning its disk quota doesn't affect a sibling on another slot.
# See src/condor_starter.V6.1/starter.cpp (per-claim LV naming, s_lv_name).

import logging
import pytest

from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config, SMALL_BACKING_FILE_MB

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)


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
def job_a_mount_file(test_dir):
    return test_dir / "job_a_mount_source.txt"


@action
def job_b_mount_file(test_dir):
    return test_dir / "job_b_mount_source.txt"


@action
def overfill_job_hash(job_a_mount_file, thin_provisioning):
    if thin_provisioning:
        request_disk_mb = 48
        overfill_mb = 96
    else:
        # A small thick LV's ext4 overhead leaves too little margin to
        # reach the hold threshold before ENOSPC; use a bigger LV instead.
        request_disk_mb = 512
        overfill_mb = 600
    return {
        "shell": (
            f"findmnt -no SOURCE . > {job_a_mount_file}; "
            f"(fallocate -l {overfill_mb}M overfill.bin || dd if=/dev/zero of=overfill.bin bs=1M count={overfill_mb}); "
            "sleep 60"
        ),
        "universe": "vanilla",
        "output": "output_a",
        "error": "error_a",
        "log": "multi_slot_a_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": f"{request_disk_mb}m",
        # Forces the job to actually run in the LVM-mounted scratch dir;
        # same-host IF_NEEDED transfer would otherwise use the submit dir.
        "should_transfer_files": "YES",
    }


@action
def wellbehaved_job_hash(job_b_mount_file):
    return {
        "shell": (
            f"findmnt -no SOURCE . > {job_b_mount_file}; "
            "(fallocate -l 8M smallfile.bin || dd if=/dev/zero of=smallfile.bin bs=1M count=8)"
        ),
        "universe": "vanilla",
        "output": "output_b",
        "error": "error_b",
        "log": "multi_slot_b_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
        "should_transfer_files": "YES",
    }


@action
def multi_slot_jobs(condor, overfill_job_hash, wellbehaved_job_hash):
    job_a = condor.submit({**overfill_job_hash}, count=1)
    job_b = condor.submit({**wellbehaved_job_hash}, count=1)

    assert job_a.wait(
        condition=ClusterState.all_held,
        timeout=180,
        verbose=True,
        fail_condition=ClusterState.any_complete,
    )
    assert job_b.wait(
        condition=ClusterState.all_complete,
        timeout=180,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job_a, job_b


class TestLVMMultiSlotIsolation:
    def test_jobs_get_distinct_logical_volumes(
        self, multi_slot_jobs, job_a_mount_file, job_b_mount_file
    ):
        assert job_a_mount_file.exists()
        assert job_b_mount_file.exists()
        a_source = job_a_mount_file.read_text().strip()
        b_source = job_b_mount_file.read_text().strip()
        assert a_source and b_source
        assert a_source != b_source

    def test_overfilling_job_is_held(self, multi_slot_jobs):
        overfill_job, _ = multi_slot_jobs
        assert overfill_job.state.all_held()

    def test_sibling_job_unaffected(self, multi_slot_jobs):
        _, wellbehaved_job = multi_slot_jobs
        assert wellbehaved_job.state.all_complete()
