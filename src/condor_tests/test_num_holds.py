#!/usr/bin/env pytest

# Test that when a job goes on hold that attributes such as
# NumHolds, NumHoldsByReason, and HoldReasonCode are set properly.
# To allow this test to run faster, just setup a pool with a schedd alone
# and submit a scheduler universe job.

from ornithology import *

#
# Setup a personal condor with just a schedd - no need for anything else
#
@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor", 
        config={
            "DAEMON_LIST": "SCHEDD MASTER",
            "USE_SHARED_PORT": False
        }
    ) as condor:
        yield condor

#
# Submit a job that will immediately go on hold due to job policy expressions.
# We use scheduler universe so it runs quickly; no need to wait for matches etc.
#
@action
def job(condor, path_to_sleep):
    handle = condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "0",
                "universe": "scheduler",
                "on_exit_hold": "true",
                "log": "job_events.log"
                },
        count=1,
    )

    # Wait for job to go on hold the first time...
    assert handle.wait(condition=ClusterState.any_held,timeout=60)
    # ...and then release it....
    handle.release()
    # ...and now wait for the job to be released in the event log....
    handle.wait(condition=ClusterState.none_held,timeout=60)
    # and finally wait for job to be held a second time... this way we can confirm hold counters
    # are incrementing as they should.
    assert handle.wait(condition=ClusterState.any_held,timeout=60)

    # Return the first (and only) job ad in the cluster for testing class to reference
    return handle.query()[0]

#
# The tests.... assertions on hold aggregates in the job ad
#
class TestJobHoldAggregates:
    # Methods that begin with test_* are tests.

    def test_holdreasoncode(self, job):
        assert job["HoldReasonCode"] == 3

    def test_lastholdreasoncode(self, job):
        assert job["LastHoldReasonCode"] == 3

    def test_holdreasonsubcode(self, job):
        assert job["HoldReasonSubCode"] == 0

    def test_lastholdsubreasoncode(self, job):
        assert job["LastHoldReasonSubCode"] == 0

    def test_hold_numholds(self, job):
        assert job["NumHolds"] == 2

    def test_hold_numholdsbyreason_was_policy(self, job):
        assert dict(job["NumHoldsByReason"]) == { 'JobPolicy' : 2 }
