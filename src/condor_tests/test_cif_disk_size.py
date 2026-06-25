#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    write_file,
    JobStatus,
)

# In this test, we submit a six-job cluster; each job requests 256 KiB and a
# 128 KiB common input file.  Since the startd only has 500 KiB of disk, the
# only way it can run more than one job is through common-files sharing.
#
# We submit six jobs, knowing that only three will fit, because at some point
# we'll probably want to make sure that the running total caps out at three
# and that the second set of three runs without the negotiator.
# -----------------------------------------------------------------------------

@action
def the_condor( test_dir ):
    local_dir = test_dir / "the_condor.d"

    with Condor(
        local_dir=local_dir,
        config={
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",

            "NUM_CPUS":         4,
            "CLAIM_WORKLIFE":   0,
            "DISK":             500,

            "MODIFY_REQUEST_EXPR_REQUESTDISK": "quantize(RequestDisk - $(CATALOG_SPACE), (Target.DiskQuantum ?: 1) )",
        },
    ) as the_condor:
        yield the_condor


@action
def the_common_file( test_dir ):
    local_dir = test_dir / "the_condor.d"
    common_file = local_dir / "common-file"

    contents = "1234567890abcdef" * 64 * 128
    common_file.write_text(contents)

    return common_file


@action
def the_running_jobs( the_condor, the_common_file ):
    job_description = {
        "shell":        "cat common-file > output-file-$(ProcID); sleep 3000",

        "universe":                 "vanilla",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1,
        "request_disk":             256,
        "log":                      "the_running_jobs.log.$(ClusterID)",
        "MY.CommonInputFiles":      f'"{the_common_file.as_posix()}"',
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=6,
    )

    assert job_handle.wait(
        timeout=120,
        condition=ClusterState.running_exactly(3),
        fail_condition=ClusterState.any_terminal,
    )

    return job_handle


class TestCIFDiskSize:

    def test_disk_size(self, the_condor, the_running_jobs):
        results = the_condor.status(
            constraint = 'MyType == "Machine"',
            projection = ['Name', 'Disk'],
        )
        print(results)
