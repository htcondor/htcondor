#!/usr/bin/env pytest

import logging

from ornithology import (
    action,
    ClusterState,
    condor,
    format_script,
    write_file,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_script(test_dir):
    the_script_file = test_dir / "the_script.py"
    contents = format_script(
    """
        #!/usr/bin/python3

        import sys
        from pathlib import Path

        Path("./failure-file-one").touch()
        Path("./failure-file-two").touch()

        sys.exit(1)
    """
    )
    write_file(the_script_file, contents)
    return the_script_file


@action
def empty_test_dir(test_dir):
        failure_file_one = test_dir / "failure-file-one"
        assert(not failure_file_one.exists())

        failure_file_two = test_dir / "failure-file-two"
        assert(not failure_file_two.exists())


@action
def the_failure_job(default_condor, the_script, test_dir):
    the_failure_job = default_condor.submit(
        {
            "executable":               the_script.as_posix(),
            "log":                      (test_dir / "job.log").as_posix(),
            "should_transfer_files":    "YES",
            "when_to_transfer_output":  "ON_SUCCESS",
            "+FailureFiles":            '"failure-file-one, failure-file-two"',
            "success_exit_code":        "0",
            "max_retries":              "0",
            "transfer_output_files":    "success-file-one",
        }
    )

    return the_failure_job


@action
def the_completed_job(the_failure_job):
    assert(the_failure_job.wait(
        condition=ClusterState.all_terminal,
        timeout=60
    ))
    return the_failure_job


class TestFailureFiles:

    def test_failure_files(empty_test_dir, the_completed_job, test_dir):
        failure_file_one = test_dir / "failure-file-one"
        assert(failure_file_one.exists())

        failure_file_two = test_dir / "failure-file-two"
        assert(failure_file_two.exists())
