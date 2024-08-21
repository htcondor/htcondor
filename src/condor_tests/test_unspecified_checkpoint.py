#!/usr/bin/env pytest

import os

from ornithology import (
    action,
    write_file,
    format_script,
    ClusterState,
    Condor,
)

import htcondor2


import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def condor_files():
    return (
        ".job.ad",
        ".machine.ad",
        ".docker_sock",
        ".docker_stdout",
        ".docker_stderr",
        ".update.ad",
        ".update.ad.tmp",
        ".execution_overlay.ad",
    )


@action
def the_test_script(test_dir):
    test_script_path = test_dir / "test_script.sh"

    script = """
        #!/bin/bash

        if [ ! -f checkpoint-file-1 ]; then
            touch checkpoint-file-1
            exit 85
        fi
        sleep 3600
        touch output-file-1
        exit 0
    """

    write_file(test_script_path, format_script(script))
    return test_script_path


@action
def the_running_test_job(default_condor, the_test_script):
    description = {
        "executable":               str(the_test_script),
        "should_transfer_files":    "yes",
        "transfer_executable":      "yes",
        "log":                      "log",
        "output":                   "output",
        "error":                    "error",
        "checkpoint_exit_code":     85,
    }

    the_test_job = default_condor.submit(
        description = description,
        count = 1
    )

    the_test_job.wait(
        verbose = True,
        timeout = 60,
        condition = ClusterState.all_running,
        fail_condition = ClusterState.any_held,
    )

    yield the_test_job

    the_test_job.remove()


@action
def files_in_spool(default_condor, the_running_test_job):
    clusterID = the_running_test_job.clusterid
    job_spool_dir = default_condor.spool_dir / f"{clusterID}" / "0" / f"cluster{clusterID}.proc0.subproc0"
    if os.path.exists(job_spool_dir):
         return os.listdir(job_spool_dir)
    else:
        return None


class TestUnspecifiedCheckpoint:

    def test_no_condor_files(self, files_in_spool, condor_files):
        for file in condor_files:
            assert file not in files_in_spool
