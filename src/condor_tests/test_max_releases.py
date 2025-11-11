#!/usr/bin/env pytest

# Test that the schedd honors SYSTEM_MAX_RELEASES

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
            "USE_SHARED_PORT": False,
            "PERSONAL_CONDOR_IS_SUPER_USER": False,
            "SYSTEM_MAX_RELEASES": 3
        }
    ) as condor:
        yield condor

#
# Submit a job to be put on hold
#
@action
def job(condor, path_to_sleep):
    handle = condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "3600",
                "universe": "scheduler",
                "log": "job_events.log"
                },
        count=1,
    )

    # Hold the job for the first time
    handle.hold()

    # Wait for job to go on hold the first time...
    assert handle.wait(condition=ClusterState.any_held,timeout=60)

    for iteration in range(3):
         handle.release()
         # ...and now wait for the job to be released in the event log....
         assert(handle.wait(condition=ClusterState.none_held,timeout=60))
         #
         handle.hold()
         assert(handle.wait(condition=ClusterState.all_held,timeout=60))

    assert handle.wait(condition=ClusterState.all_held,timeout=60)

    # This release should fail to release it
    handle.release()
    assert(handle.wait(condition=ClusterState.all_held,timeout=60))

    # Return the first (and only) job ad in the cluster for testing class to reference
    return handle.query()[0]

#
class TestMaxReleases:
    # Methods that begin with test_* are tests.

    def test_max_releases(self, job):
        assert job["NumHolds"] == 4
