#!/usr/bin/env pytest


import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Plan for this test is to stand up a condor with 20
# static slots, and configure the NEGOTIATOR_SLOT_CONSTRAINT
# to only look at slot 7 (randomly chosen).  We then
# submit a sleep 0 job, wait for it to complete, leave
# it in the queue, and check to see that it ran on slot7.

@standup
def condor(test_dir):
    raw_config = "USE FEATURE : StaticSlots"
    with Condor(test_dir / "condor", 
            raw_config=raw_config,
            config={
                "NUM_CPUS": "20",
                "NEGOTIATOR_SLOT_CONSTRAINT": "SlotID == 7",
            }) as condor:
        yield condor

@action
def job_one_handle(condor, test_dir, path_to_sleep):
    handle = condor.submit(
        description={
            "log":                      test_dir / "job.log",
            "leave_in_queue":           True,

            "universe":                 "vanilla",
            "should_transfer_files":    False,
            "executable":               path_to_sleep,
            "arguments":                0,
            "Request_Cpus":             1,
            "Request_Memory":           32,
            "Request_Disk":             10240,
        },
        count=1,
    )

    handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        verbose=True,
        timeout=60,
    )

    yield handle

@action
def job_one_ad(job_one_handle):
    for ad in job_one_handle.query():
        return ad

class TestNegotiatorSlotConstraint:
    def test_internal_consistency(self, job_one_ad):
        # There's a race condition when the sched moves
        # RemoteHost to LastRemoteHost. If the latter
        # doesn't exist, try the former
        if "LastRemoteHost" in job_one_ad:
            host = job_one_ad["LastRemoteHost"]
        else:
            host = job_one_ad["RemoteHost"]
        assert host.startswith("slot7@")
