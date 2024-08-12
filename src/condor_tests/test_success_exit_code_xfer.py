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

@action
def path_to_the_job_script(test_dir):
    script = """
    #!/usr/bin/python3

    import sys

    print("standard output", file=sys.stdout)
    print("standard error", file=sys.stderr)
    sys.exit(0)
    """

    path = test_dir / "logging.py"
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
        "success_exit_code":            1,
        "max_retries":                  0,
    }


@action
def the_job_handle(default_condor, test_dir, the_job_description):
    job_description = {
        ** the_job_description,
        ** {
            "log":              test_dir / "logged_job.log",
            "output":           test_dir / "the_output_log",
            "error":            test_dir / "the_error_log",
        }
    }

    job_handle = default_condor.submit(
        description = job_description,
        count = 1,
    )

    yield job_handle

    job_handle.remove()


@action
def the_completed_job_handle(the_job_handle):
    the_job_handle.wait(
        timeout = 60,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    return the_job_handle


class TestSuccessExitCode:

    def test_transfer(self, test_dir, the_completed_job_handle):
        the_output_log = test_dir / "the_output_log"
        assert the_output_log.is_file()
        assert the_output_log.read_text() == "standard output\n"

        the_error_log = test_dir / "the_error_log"
        assert the_error_log.is_file()
        assert the_error_log.read_text() == "standard error\n"
