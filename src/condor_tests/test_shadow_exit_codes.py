#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    JobStatus,
)

from pathlib import Path
import re


@action
def the_container_image(test_dir, pytestconfig):
    # This is a gross hack.
    ctest_path = test_dir / ".." / "busybox.sif"
    if ctest_path.exists():
        return ctest_path
    else:
        return Path(pytestconfig.rootdir) / "busybox.sif"


@action
def lock_dir(test_dir):
    return test_dir / "lock.d"


@action
def the_condor(test_dir, lock_dir):
    local_dir = test_dir / "the_condor.d"
    with Condor(
        submit_user='tlmiller',
        condor_user='condor',
        local_dir=local_dir,
        config={
            # Set both of these to require the use COPY mapping.
            # "FORBID_HARDLINK_MAPPING":      True,
            # "FORBID_BINDMOUNT_MAPPING":     True,
            # This only matters when testing as root.
            # "STARTD_ENFORCE_DISK_LIMITS":   True,
            # "LVM_AUTO_VG_NAME":             "alpha",
            # This must be AUTO for cxfer to work when enforcing disk limits.
            "LVM_HIDE_MOUNT":               "AUTO",
            "STARTER_NESTED_SCRATCH":       True,
            "LOCK":                         lock_dir.as_posix(),

            "SINGULARITY":                          "/usr/bin/singularity",
            "SINGULARITY_TEST_SANDBOX_TIMEOUT":      "50",
            "CONTAINER_IMAGES_COMMON_BY_DEFAULT":   False,

            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
        },
    ) as the_condor:
        yield the_condor


@action
def the_completed_jobs(the_condor, the_container_image):
    job_description = {
        "universe":                 "vanilla",
        "shell":                    "sleep 5",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1024,
        "log":                      "the_completed_job.log",
        "container_is_common":      "true",
        "container_image":          the_container_image.as_posix(),
        "LeaveInQueue":             True,
        # This is the magic.
        "MY.ReportMappingFailure":  "debug(MY.NumShadowStarts <= 1)",
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=2
    )

    job_handle.wait(
        timeout=120,
        condition=ClusterState.all_terminal,
        fail_condition=ClusterState.any_held,
    )

    return job_handle


class TestShadowExitCodes:


    def test_mapping_failed(self, the_completed_jobs, the_condor):
        # The jobs must complete.
        assert the_completed_jobs.state.all_terminal()

        # The schedd must have relinquished transfer shadow's claim for
        # having had too many shadow exceptions.
        match = False
        expr = f"Match for {the_completed_jobs.clusterid}.-\\d+ has had \\d+ shadow exceptions, relinquishing."
        shadow_log = the_condor.schedd_log.open()
        for line in shadow_log.read():
            if re.fullmatch(expr, line.message) is not None:
                match = True
                break
        assert match
