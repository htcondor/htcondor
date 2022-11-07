#!/usr/bin/env pytest

import os
import time

from ornithology import (
    action,
    write_file,
    format_script,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

#
# See HTCONDOR-736 for a detailed explanation of what we're testing here.
#


@action
def path_to_the_job_script(test_dir):
    script = """
    #!/usr/bin/python3

    import sys
    import time
    import getopt

    total_steps = 24
    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
    except IOError:
        pass

    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")

        time.sleep(3)
        num_completed_steps += 1

        if num_completed_steps % 5 == 0:
            print(f"Checkpointing after {num_completed_steps}.")
            try:
                with open("saved-state", "w") as saved_state:
                    saved_state.write(f"{num_completed_steps}")
                sys.exit(17)
            except IOError:
                print("Failed to write checkpoint.", file=sys.stderr);
                sys.exit(1)

    print(f"Completed all {total_steps} steps.")
    sys.exit(0)
    """

    path = test_dir / "counting.py"
    write_file(path, format_script(script))

    return path


@action
def the_job_description(path_to_the_job_script):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_the_job_script.as_posix(),
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "checkpoint_exit_code":         "17",
        "transfer_checkpoint_files":    "saved-state",
        "transfer_output_files":        "",
    }


@action
def logged_job_handle(default_condor, test_dir, the_job_description):
    logged_job_description = {
        ** the_job_description,
        ** {
            "log":              test_dir / "logged_job.log",
            "output":           "subdir/the_output_log",
            "error":            "subdir/the_error_log",
            "stream_output":    "true",
            "stream_error":     "true",
        }
    }

    os.mkdir("subdir")
    logged_job_handle = default_condor.submit(
        description = logged_job_description,
        count = 1,
    )

    yield logged_job_handle

    logged_job_handle.remove()


@action
def unlogged_job_handle(default_condor, test_dir, the_job_description):
    unlogged_job_description = {
        ** the_job_description,
        ** {
            "log":      test_dir / "logged_job.log",
        }
    }

    unlogged_job_handle = default_condor.submit(
        description = unlogged_job_description,
        count = 1,
    )

    yield unlogged_job_handle

    unlogged_job_handle.remove()


# Let both jobs run simultaneously.
@action
def all_job_handles(logged_job_handle, unlogged_job_handle):
    pass


# This not very general, so be careful about trying to re-use it.
@action
def files_in_spool(default_condor, all_job_handles, logged_job_handle, unlogged_job_handle):
    deadline = time.time() + 180

    while time.time() < deadline:
        files_in_spool = {};

        found_spool_dirs = 0
        for clusterID in [ logged_job_handle.clusterid, unlogged_job_handle.clusterid ]:
            job_spool_dir = default_condor.spool_dir / f"{clusterID}" / "0" / f"cluster{clusterID}.proc0.subproc0"
            # Because HTCondor writes the checkpoint to f"{job_spool_dir}.tmp"
            # and then renames the directory -- to make sure the checkpoint is
            # stored atomically -- we don't have to worry about partial reads.
            if os.path.exists(job_spool_dir):
                files_in_spool[clusterID] = os.listdir(job_spool_dir)
                found_spool_dirs += 1

        if found_spool_dirs == 2:
            return files_in_spool

        # If this test starts failing, add a check here to make sure that
        # neither job is a terminal state to get an early out.
        time.sleep(5)

    # This is kind of clumsy.  (I'm not asserting false here because
    # that would have explanatory power when seen in the output.)
    assert(time.time() < deadline)


@action
def logged_job_files_in_spool(files_in_spool, logged_job_handle):
    return files_in_spool[logged_job_handle.clusterid]


@action
def unlogged_job_files_in_spool(files_in_spool, unlogged_job_handle):
    return files_in_spool[unlogged_job_handle.clusterid]


class TestFileTransfer:
    # The observable problem for this bug is that the non-tmp spool directory
    # is never created (because file transfer fails).
    def test_checkpoint_exists(self, logged_job_files_in_spool):
        assert sorted(logged_job_files_in_spool) == sorted(['saved-state'])

    # The bug is here is the presence of [/dev/]null in SPOOL.
    def test_checkpoint_missing_null(self, unlogged_job_files_in_spool):
        assert "null" not in unlogged_job_files_in_spool
