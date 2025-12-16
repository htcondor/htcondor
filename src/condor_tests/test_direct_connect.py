#!/usr/bin/env pytest


# Test that a startd can directly connect to a schedd
# and run a job.  Do this by setting up a personal
# condor without a negotiator, and configuring the
# startd to connect directly to the schedd.

import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Set a personal condor without a negotiator.  Name the schedd
# a fixed name to make it easy to find.
@standup
def condor(test_dir):
    with Condor(test_dir / "condor", config={
        "DAEMON_LIST": "MASTER SCHEDD STARTD COLLECTOR",
        "STARTD_DIRECT_ATTACH_SCHEDD_NAME": "$(USERNAME)@$(FULL_HOSTNAME)",
        "STARTD_DIRECT_ATTACH_INTERVAL": "3",
        }) as condor:
        yield condor

@action
def sleep_job_parameters(path_to_sleep):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "0",
        "request_cpus": "1",
        "request_disk": "100",
        "request_memory": "1024",
        "log": "sleep.log",
    }

@action
def sleep_job(sleep_job_parameters, condor):
    job = condor.submit( sleep_job_parameters)
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=600,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return job

class TestDirectConnect:
    def test_starter_log(self, condor, sleep_job):
        assert True

