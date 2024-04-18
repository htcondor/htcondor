#!/usr/bin/env pytest

import os
import time
from pathlib import Path

from ornithology import (
    action,
    Condor,
    ClusterState,
)


#
# (A) Touch the output files,
# sleep(1) to make sure the timestamps are different,
# then touch the executable,
# then touch the input files.
#
# This should clearly result in the job running.
#
# (B) Touch the output files,
# sleep(1) to make sure the timestamps are different,
# then touch the executable,
# then touch the stdin file,
# then touch the input files.
#
# This should clearly result in the job running.
#


@action
def test_a_dir( test_dir ):
    test_a_dir = test_dir / "test_a"
    test_a_dir.mkdir()

    (test_a_dir / "out1").touch()
    (test_a_dir / "out2").touch()

    time.sleep(1)

    (test_a_dir / "in1").touch()
    (test_a_dir / "in2").touch()

    time.sleep(1)

    (test_a_dir / "exe.sh").touch()
    (test_a_dir / "exe.sh").write_text(
"""#!/bin/bash
touch out1
touch out2
exit 0
"""
    )
    (test_a_dir / "exe.sh").chmod(0o755)

    return test_a_dir


@action
def test_b_dir( test_dir ):
    test_b_dir = test_dir / "test_b"
    test_b_dir.mkdir()

    (test_b_dir / "out1").touch()
    (test_b_dir / "out2").touch()

    time.sleep(1)

    # Avoid triggering the bug from test A by making sure the
    # executable is older than the input files.
    (test_b_dir / "exe.sh").touch()
    (test_b_dir / "exe.sh").write_text(
"""#!/bin/bash
touch out1
touch out2
exit 0
"""
    )
    (test_b_dir / "exe.sh").chmod(0o755)

    time.sleep(1)

    (test_b_dir / "in1").touch()
    (test_b_dir / "in2").touch()

    time.sleep(1)

    (test_b_dir / "stdin.file").touch()

    return test_b_dir


@action
def the_condor(default_condor):
    return default_condor


the_job_description = {
    "LeaveJobInQueue":              "true",

    "executable":                   "exe.sh",
    "transfer_executable":          "true",
    "should_transfer_files":        "true",
    "when_to_transfer_output":      "ON_EXIT",

    "transfer_input_files":         "in1, in2",
    "transfer_output_files":        "out1, out2",

    "skip_if_dataflow":             "true",
}


@action
def completed_test_a_job(test_dir, test_a_dir, the_condor):
    complete_job_description = {
        ** the_job_description,
        ** {
            "log": (test_dir / "test_a.log").as_posix(),
        }
    }

    os.chdir(test_a_dir)
    the_handle = the_condor.submit(
        complete_job_description,
        count=1,
    )

    assert the_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held
    )

    return the_handle


@action
def completed_test_b_job(test_dir, test_b_dir, the_condor):
    complete_job_description = {
        ** the_job_description,
        ** {
            "log": (test_b_dir / "test_b.log").as_posix(),
            "input": (test_b_dir / "stdin.file").as_posix(),
        }
    }

    os.chdir(test_b_dir)
    the_handle = the_condor.submit(
        complete_job_description,
        count=1,
    )

    assert the_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held
    )

    return the_handle


class TestDataflowMore:

    def test_a_skipped_job(self, the_condor, completed_test_a_job):
        schedd = the_condor.get_local_schedd()
        constraint = f'ClusterID == {completed_test_a_job.clusterid}'
        result = schedd.query(
            constraint=constraint,
            projection=["DataflowJobSkipped"],
        )
        skipped = result[0]["DataflowJobSkipped"]
        assert(not skipped)


    def test_b_skipped_job(self, the_condor, completed_test_b_job):
        schedd = the_condor.get_local_schedd()
        constraint = f'ClusterID == {completed_test_b_job.clusterid}'
        result = schedd.query(
            constraint=constraint,
            projection=["DataflowJobSkipped"],
        )
        skipped = result[0]["DataflowJobSkipped"]
        assert(not skipped)
