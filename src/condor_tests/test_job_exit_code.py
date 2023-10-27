#!/usr/bin/env pytest

import pytest
import logging

from ornithology import (
    action,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_job(default_condor, test_dir):
    the_job = default_condor.submit(
        {
            "executable":           "/bin/bash",
            "arguments":            '''"-c 'exit 17'"''',
            "log":                  (test_dir / "the_job.log").as_posix(),
            "transfer_executable":  "false",
            "leave_in_queue":       "true",
        }
    )

    assert the_job.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
    )

    return the_job


class TestJobExitCode:

    def test_job_exit_code(self, the_job):
        the_job_ad = the_job.query()[0]
        the_exit_code = the_job_ad['ExitCode']
        assert the_exit_code == 17
