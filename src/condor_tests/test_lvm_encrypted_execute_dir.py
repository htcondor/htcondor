#!/usr/bin/env pytest

# Regression test for LVM-backed execute directory encryption. See
# src/condor_startd.V6/VolumeManager.cpp (EncryptLV, "-enc" device suffix).
#
# Covers: (1) a job requesting encrypt_execute_directory=True gets a
# LUKS-encrypted LV; (2) a pool-wide ENCRYPT_EXECUTE_DIRECTORY default
# encrypts a job that didn't request it.
#
# Not covered: DISABLE_EXECUTE_DIRECTORY_ENCRYPTION=True + a job explicitly
# requesting encryption -- that's a hard config conflict that fails the
# claim via a timing-sensitive idle/retry path, not reliable to assert on.

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


@standup
def condor_force_encrypt(test_dir, thin_provisioning):
    with Condor(
        test_dir / "condor_force_encrypt",
        config=lvm_config(
            thin_provisioning,
            test_dir / "condor_force_encrypt" / "lvm_backing.img",
            extra={"ENCRYPT_EXECUTE_DIRECTORY": "True"},
        ),
    ) as condor:
        yield condor


def _mount_source_job_hash(output_file, log_name, extra_submit=None):
    # /proc/self/mounts instead of findmnt, which may not be on the job's
    # minimal default PATH. $(DOLLAR) escapes a literal "$" past
    # condor_submit's own macro expansion of "shell".
    shell = f"awk -v d=\"$(DOLLAR)(pwd)\" '$2==d{{print $1}}' /proc/self/mounts > {output_file}"
    job_hash = {
        "shell": shell,
        "universe": "vanilla",
        "output": "output_" + log_name,
        "error": "error_" + log_name,
        "log": log_name,
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
        "transfer_output_files": output_file,
        # Forces the job to actually run in the LVM-mounted scratch dir;
        # same-host IF_NEEDED transfer would otherwise use the submit dir.
        "should_transfer_files": "YES",
    }
    if extra_submit:
        job_hash.update(extra_submit)
    return job_hash


@action
def requested_encryption_job_hash():
    return _mount_source_job_hash(
        "mount_source_requested", "requested_log", {"encrypt_execute_directory": "True"}
    )


@action
def requested_encryption_job(condor, requested_encryption_job_hash):
    job = condor.submit({**requested_encryption_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job


@action
def default_encryption_job_hash():
    return _mount_source_job_hash("mount_source_default", "default_log")


@action
def default_encryption_job(condor_force_encrypt, default_encryption_job_hash):
    job = condor_force_encrypt.submit({**default_encryption_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job


class TestLVMEncryptedExecuteDir:
    def test_job_requested_encryption_is_applied(self, requested_encryption_job):
        with open("mount_source_requested") as f:
            source = f.read().strip()
        assert source.endswith("-enc")

    def test_pool_wide_default_forces_encryption(self, default_encryption_job):
        with open("mount_source_default") as f:
            source = f.read().strip()
        assert source.endswith("-enc")
