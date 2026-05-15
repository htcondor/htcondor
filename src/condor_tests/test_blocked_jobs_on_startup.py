#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    JobStatus,
    JobQueue,
)

import time
import htcondor2


@action
def the_condor(test_dir):
    local_dir = test_dir / "the_condor.d"
    with Condor(
        local_dir=local_dir,
    ) as the_condor:
        yield the_condor


@action
def blocked_jobs_on_disk(the_condor):
    job_description = {
        "universe":                 "vanilla",
        "shell":                    "sleep 5",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1024,
        "log":                      "the_job.log",
        "LeaveInQueue":             True,
        # This is the magic.
        "MY.CommonINputFiles":      '"debug://sleep/300"',
    }

    job_handle = the_condor.submit(
       description=job_description,
       count=7,
    )

    # All seven jobs should be BLOCKed for 300 seconds because of the
    # debug plug-in's sleep, giving us a wide window to detect when we
    # can shoot the schedd in the head.

    schedd = the_condor.get_local_schedd()

    deadline = time.time() + 300
    while True:
        time.sleep(5)

        # Unfortunately, Ornithology's ClusterState doesn't know about
        # BLOCKED yet, so we have to ask the schedd.
        results = schedd.query(
            constraint=f'ClusterID == {job_handle.clusterid}',
            projection=["ProcID", "JobStatus"],
        )

        # We'd prefer every, but we only need one.
        for result in results:
            job_status = result.get('JobStatus', 0)
            if job_status == 9:
                break;

        assert time.time() < deadline;

        # Turn off the schedd.
        the_condor.run_command(
            ['condor_off', '-schedd'],
            timeout=10,
            echo=False,
        )

        # Wait for it to die.
        cp = the_condor.run_command(
            ['condor_wait', '-quick', '-wait', 'SCHEDD == "Hold"'],
            timeout=30,
            echo=False,
        )

        return job_handle


class TestBlockedJobs:


    def test_on_startup(self, the_condor, blocked_jobs_on_disk):
        # Assert that there's at least one blocked job on disk.
        with the_condor.job_queue_log.open(mode="r") as jql:
            for line in jql.readlines():
                if line.startswith("103 ") and "JobStatus" in line:
                    (_, jobid, __, status) = line.split()
                    assert status == 9

        # Turn off the startd; if we don't, the newly-idled jobs will
        # try to start again.
        the_condor.run_command(
            ['condor_off', '-startd'],
            timeout=10,
            echo=False,
        )

        # Wait for it to die.
        cp = the_condor.run_command(
            ['condor_wait', '-quick', '-wait', 'STARTD == "Hold"'],
            timeout=30,
            echo=False,
        )

        # Restart the schedd.
        the_condor.run_command(
            ['condor_on', '-schedd'],
            timeout=10,
            echo=False,
        )

        # Wait for it to live.
        cp = the_condor.run_command(
            ['condor_wait', '-quick', '-wait', 'STARTD == "Alive"'],
            timeout=30,
            echo=False,
        )

        schedd = the_condor.get_local_schedd()
        results = schedd.query(
            constraint=f'ClusterID == {blocked_jobs_on_disk.clusterid}',
            projection=["ProcID", "JobStatus"],
        )
        assert results is not None

        for result in results:
            assert result.get('JobStatus', -1) != 9
