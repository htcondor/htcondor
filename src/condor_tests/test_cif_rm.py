#!/usr/bin/env pytest

import time
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import htcondor2
from ornithology import (
    action,
    ClusterState,
    Condor,
)


# In this test, we submit a bunch of CIF-enabled jobs, wait for them to get
# started, and then remove them.  This should set a timer to remove the data
# slot created for the CIF-enabled jobs, and when that timer fires, release
# the data slot's claim.  This did not used to happen.
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

            "NUM_CPUS":                 4,
            "KEEP_DATA_CLAIM_IDLE":     20,
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
def the_removed_jobs( the_condor, the_common_file ):
    job_description = {
        "shell":                    "sleep 3000",

        "universe":                 "vanilla",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1,
        "request_disk":             1024,
        "log":                      "the_running_jobs.log.$(ClusterID)",
        "MY.CommonInputFiles":      f'"{the_common_file.as_posix()}"',
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=8,
    )

    assert job_handle.wait(
        timeout=120,
        condition=ClusterState.running_exactly(4),
        fail_condition=ClusterState.any_terminal,
    )

    the_condor.act( htcondor2.JobAction.Remove, job_handle.clusterid )

    print()
    deadline = time.time() + 60
    while time.time() < deadline:
        # What we're actually waiting for is all of the shadows to die.
        results = the_condor.query(
            projection=['ClusterID', 'ProcID', 'JobStatus']
        )
        print(time.time())
        print(results)
        if len(results) == 0:
            return job_handle
        time.sleep(5)

    assert False


class TestCIF:

    def test_data_slot_gone(self, the_condor, the_removed_jobs):
        print()
        results = the_condor.status(
            ad_type = htcondor2.AdType.Startd,
            projection = ['Name', 'Disk'],
        )
        print("Immediately after job removal:")
        print(results)

        num_data_slots = 0
        for slot in results:
            if "data" in slot['Name']:
                num_data_slots += 1
        assert num_data_slots == 1

        # Wait for KEEP_DATA_CLAIM_IDLE to pass and a little slop.
        time.sleep(20+5)

        results = the_condor.status(
            ad_type = htcondor2.AdType.Startd,
            projection = ['Name', 'Disk'],
        )
        print()
        print("After waiting KEEP_DATA_CLAIM_IDLE:")
        print(results)

        assert len(results) == 1
        assert "data" not in results[0]['Name']
