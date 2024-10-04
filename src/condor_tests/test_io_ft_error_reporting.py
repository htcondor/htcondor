#!/usr/bin/env pytest

import htcondor2

import logging
from getpass import getuser

from ornithology import *


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(default_condor):
    return default_condor


@action
def the_held_test_job(the_condor, path_to_sleep, test_dir):
    the_test_job = the_condor.submit(
        {
            "executable":               path_to_sleep,
            "arguments":                1,
            "should_transfer_files":    "yes",
            "transfer_input_files":     "never-exist.file",
            "log":                      (test_dir / "the_test_job.log").as_posix(),
        },
    )

    assert the_test_job.wait(
        condition=ClusterState.all_held,
        timeout=60,
    )

    return the_test_job


class TestFTErrorReporting:

    def test_o_after_i(self, the_held_test_job):
        hold_reasons = the_held_test_job.query(projection=["HoldReason"])
        assert(hold_reasons[0]["HoldReason"].startswith("Transfer input files failure"))
