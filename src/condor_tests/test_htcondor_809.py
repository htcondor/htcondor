#!/usr/bin/env pytest

import os
import time

from ornithology import (
    action,
    write_file,
    format_script,
    ClusterState,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

#
# See HTCONDOR-815 for a detailed explanation of what we're testing here.
#

# -----------------------------------------------------------------------------

#
# If we specify a file twice, does the second version win?
#

@action
def path_to_ordering_script(test_dir):
    script="""
    #!/bin/bash
    cat a_file
    exit 0
    """

    path = test_dir / "ordering.sh"
    write_file(path, format_script(script))

    return path


@action
def ordering_job_description(path_to_ordering_script, test_dir):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_ordering_script,
        "arguments":                    "1",
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "output":                       test_dir / "ordering.out",
        "error":                        test_dir / "ordering.err",
        "log":                          test_dir / "ordering.log",

        "transfer_input_files":         "b_dir/, a_file",
    }


@action
def ordering_job_handle(default_condor, ordering_job_description):
    os.mkdir("b_dir")
    with open("b_dir/a_file", "w") as first_input:
        first_input.write("the first file in the list")
    with open("a_file", "w") as second_input:
        second_input.write("the second file in the list")

    ordering_job_handle = default_condor.submit(
        description = ordering_job_description,
        count = 1,
    )

    yield ordering_job_handle

    ordering_job_handle.remove()


@action
def completed_ordering_job(ordering_job_handle):
    ordering_job_handle.wait(
        timeout = 60,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    return completed_ordering_job

@action
def ordering_job_output(completed_ordering_job, ordering_job_description):
    output_log_path = ordering_job_description["output"]
    with open(output_log_path, "r") as output_log:
        output = output_log.readline()
    return output


# -----------------------------------------------------------------------------

#
# The idea for this implementation is that the job will log what it sees
# in data_file in a well-known location (a file in `test_dir`) each time it
# starts up.  Because the job is NOT a self-checkpointing job, it won't
# be restarted unless it successfully evicts itself, so as long as all
# of the lines are there, we know everything's OK.  Checking the contents
# of SPOOL would be helpful for debugging, but we can leave that for later,
# if the test starts to fail.
#
# Previous versions of HTCondor would put the job as described on hold
# because they'd sort the transfer of the file in the `data` directory
# before the creation of that directory, which would cause the transfer
# of the file to fail.  We check the contents of `data_file` to test
# HTCONDOR-583 (which resulted in the input file being transferred if
# it had the same name as a spooled file).
#

@action
def path_to_directory_script(default_condor, test_dir):
    script=f"""
    #!/bin/bash
    export CONDOR_CONFIG={default_condor.config_file}
    export PATH=$PATH:{os.environ["PATH"]}
    cat data/data_file >> {test_dir}/directory-test-file
    DATA=`cat data/data_file`
    if [[ $DATA == "first job modification" ]]; then
        echo "second job modification" > data/data_file
        exit 0
    fi
    echo "first job modification" > data/data_file
    condor_vacate_job $1
    # Don't exit before we've been vacated.
    sleep 60
    exit 0
    """

    path = test_dir / "directory.sh"
    write_file(path, format_script(script))

    return path


@action
def directory_job_description(path_to_directory_script, test_dir):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_directory_script,
        "arguments":                    "$(ClusterID).$(ProcID)",
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT_OR_EVICT",

        "output":                       test_dir / "directory.out",
        "error":                        test_dir / "directory.err",
        "log":                          test_dir / "directory.log",

        "transfer_input_files":         "output/data",
        "transfer_output_files":        "data",
    }


@action
def directory_job_handle(default_condor, directory_job_description):
    os.mkdir("output")
    os.mkdir("output/data")

    with open("output/data/data_file", "w") as data_file:
        data_file.write("original input file\n")

    directory_job_handle = default_condor.submit(
        description = directory_job_description,
        count = 1,
    )

    yield directory_job_handle

    directory_job_handle.remove()


@action
def directory_job_lines(directory_job_handle, default_condor, test_dir):
    # Waiting until the job has been evicted beens waiting for it to
    # start running and then waiting for it to go idle.  We can't just
    # wait for the job to finish because the schedd pretends that
    # vacated jobs shouldn't be rescheduled for a very long time.

    directory_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_held,
    )

    directory_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_idle,
        fail_condition=ClusterState.any_held,
    )

    default_condor.run_command(['condor_reschedule'])

    directory_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    with open(test_dir / "directory-test-file", "r") as output:
        return output.readlines()

    return None


# -----------------------------------------------------------------------------

#
# Test a self-checkpointing job with preserve_relative_paths set and which
# names the same directory in transfer_*_files.  Like the previous test,
# the job here vacates itself.  At some point in the glorious future, we
# will no longer have to run `condor_reschedule` after that happens, and
# we'll be able to run this test concurrently with the others, in a fire-
# and-forget manner.
#

@action
def path_to_prp_script(default_condor, test_dir):
    script=f"""
    #!/bin/bash
    export CONDOR_CONFIG={default_condor.config_file}
    export PATH=$PATH:{os.environ["PATH"]}

    DATA=`tail -n 1 {test_dir}/prp-test-file`
    echo "Starting up..." >> {test_dir}/prp-test-file

    if [[ $DATA == "" ]]; then
        echo "step one" >> {test_dir}/prp-test-file
        echo "step one" >> prp/data/data_file
        mkdir prp/data/subdir
        echo "step one" >> prp/data/subdir/other_data_file
        exit 85
    fi
    if [[ $DATA == "step one" ]]; then
        echo "step two" >> {test_dir}/prp-test-file
        echo "step two" >> prp/data/data_file
        condor_vacate_job $1
        # Don't exit before we've been vacated.
        sleep 60
        # We did not succeed.
        exit 1
    fi
    if [[ $DATA == "step two" ]]; then
        echo "step three" >> {test_dir}/prp-test-file
        echo "step three" >> prp/data/data_file
        exit 0
    fi
    echo "step never-never" >> {test_dir}/prp-test-file
    echo "step never-never" >> prp/data/data_file
    exit 1
    """

    path = test_dir / "prp.sh"
    write_file(path, format_script(script))

    return path


@action
def prp_job_description(path_to_prp_script, test_dir):
    return {
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_prp_script,
        "arguments":                    "$(ClusterID).$(ProcID)",
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",
        "preserve_relative_paths":      "true",

        "output":                       "prp.out",
        "stream_output":                "true",
        "error":                        "prp.err",
        "stream_error":                 "true",
        "log":                          test_dir / "prp.log",

        "transfer_input_files":         "prp/data",
        "transfer_checkpoint_files":    "prp/data",
        "transfer_output_files":        "prp/data",
        "checkpoint_exit_code":         "85",
    }


@action
def prp_job_handle(default_condor, prp_job_description):
    os.mkdir("prp")
    os.mkdir("prp/data")

    with open("prp/data/data_file", "w") as data_file:
        data_file.write("original input file\n")

    prp_job_handle = default_condor.submit(
        description = prp_job_description,
        count = 1,
    )

    yield prp_job_handle

    prp_job_handle.remove()


@action
def prp_job_lines(prp_job_handle, default_condor, test_dir):
    #
    # With only a little work, this function and directory_job_lines()
    # could share a common implementation.
    #
    prp_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_terminal,
    )

    prp_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_idle,
        fail_condition=ClusterState.any_terminal,
    )

    default_condor.run_command(['condor_reschedule'])

    prp_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    with open(test_dir / "prp-test-file", "r") as output:
        return output.readlines()

    return None


# -----------------------------------------------------------------------------

class TestFileTransfer:

    def test_input_ordering(self, ordering_job_output):
        assert ordering_job_output == "the second file in the list"


    def test_input_then_checkpoint_ordering(self, directory_job_lines):
        assert len(directory_job_lines) == 2
        assert directory_job_lines[0] == "original input file\n"
        assert directory_job_lines[1] == "first job modification\n"

        with open("data/data_file", "r") as final_output:
            line = final_output.readline()
            assert line == "second job modification\n"


    def test_preserve_spool_relative_paths(self, prp_job_lines):
        assert prp_job_lines == [
            "Starting up...\n",
            "step one\n",
            "Starting up...\n",
            "step two\n",
            "Starting up...\n",
            "step three\n",
        ]

        with open("prp/data/data_file", "r") as final_output:
            lines = final_output.readlines()
            assert lines == [
                "original input file\n",
                "step one\n",
                "step three\n",
            ]
