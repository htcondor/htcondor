#!/usr/bin/env pytest

# Verifies _CONDOR_JOB_AD/_CONDOR_MACHINE_AD/_CONDOR_SCRATCH_DIR point at
# real files/dirs when the scratch dir is an LVM logical volume.
#
# Modeled on tests/check_ad_files from
# https://github.com/ColeBollig/htcondor-lvm-integration.

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
def job_ad_env_job_hash():
    # Report via a transferred-back file rather than the job's exit code --
    # querying the schedd right after all_complete() can race the job
    # leaving the queue.
    return {
        "shell": (
            'result=0; '
            'test -n "$_CONDOR_JOB_AD" || result=1; '
            'test -n "$_CONDOR_MACHINE_AD" || result=1; '
            'test -f "$_CONDOR_JOB_AD" || result=1; '
            'test -f "$_CONDOR_MACHINE_AD" || result=1; '
            'test -n "$_CONDOR_SCRATCH_DIR" || result=1; '
            # $(DOLLAR) escapes a literal "$" past condor_submit's own
            # macro expansion of "shell" (SubmitHash::SetArguments()).
            'test "$_CONDOR_SCRATCH_DIR" = "$(DOLLAR)(pwd)" || result=1; '
            'echo $result > result.txt'
        ),
        "universe": "vanilla",
        "output": "output",
        "error": "error",
        "log": "job_ad_env_log",
        "request_cpus": "1",
        "request_memory": "64m",
        "request_disk": "48m",
        "transfer_output_files": "result.txt",
        # Forces the job to actually run in the LVM-mounted scratch dir;
        # same-host IF_NEEDED transfer would otherwise use the submit dir.
        "should_transfer_files": "YES",
    }


@action
def job_ad_env_job(condor, job_ad_env_job_hash):
    job = condor.submit({**job_ad_env_job_hash}, count=1)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=120,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job


class TestLVMJobAdEnv:
    def test_job_ad_env_vars_valid(self, job_ad_env_job):
        with open("result.txt") as f:
            result = f.read().strip()
        assert result == "0"
