#!/usr/bin/env pytest


#
# Submit a job whose plug-in takes long enough to trigger the callback
# mechanism, then check to make sure that the D_TEST output from doing
# so appears in the starter log.
#
# Submit a job whose plug-in sleeps for long enough to trigger
# STARTER_FILE_XFER_STALL_TIMEOUT (which must be less than
# MAX_FILE_TRANSFER_PLUGIN_LIFETIME), then check to verify that the
# callback was succesful in preventing the starter from EXCEPT()ing.
#


import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import htcondor2
from pathlib import Path


@action
def the_condor(test_dir):
    local_dir = test_dir / "the_condor"
    with Condor(
        local_dir=local_dir,
        config={
            "STARTER_DEBUG": "D_CATEGORY D_SUB_SECOND D_TEST D_PID",
            "STARTER_XFER_LAST_ALIVE_DELAY": 5,
            "STARTER_XFER_LAST_ALIVE_INTERVAL": 5,
            "MY_POPEN_KEEPALIVE_INTERVAL": 15,
            "STARTER_FILE_XFER_STALL_TIMEOUT": 25,
            "MAX_FILE_TRANSFER_PLUGIN_LIFETIME": 75,
        }
    ) as the_condor:
        yield the_condor


@action
def the_completed_job(the_condor):
    job_description = {
        "universe":                 "vanilla",
        "shell":                    "sleep 1",

        "log":                      "the_job.log",

        "should_transfer_files":    True,
        "transfer_input_files":     "debug://sleep/50",
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=1
    )

    assert job_handle.wait(
        timeout=100,
        condition=ClusterState.all_terminal,
    )

    return job_handle


# Stolen from `test_cif.py`.
def count_starter_log_lines(the_condor, the_phrase):
    with the_condor.use_config():
        LOG = htcondor2.param["LOG"]

    count = 0
    for log in Path(LOG).iterdir():
        if log.name.startswith("StarterLog.slot1_"):
            for line in log.read_text().split("\n"):
                if the_phrase in line:
                    count = count + 1

    return count


class TestHungPlugin:

    # If we got to this test at all, that means the job finished (and
    # finished the first time it ran), which probably means things are
    # all OK, but let's be explicit.
    def test_callback_fires(self, the_condor, the_completed_job):
        num_multi_callbacks = count_starter_log_lines(
            the_condor, "wait_for_output() callback (multi)"
        )
        assert num_multi_callbacks >= 1

        num_starter_exceptions = count_starter_log_lines(
            the_condor, "EXCEPT"
        )
        assert num_starter_exceptions == 0
