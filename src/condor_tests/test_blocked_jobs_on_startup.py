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
        config={
            "KILLING_TIMEOUT":      1,
        },
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

    # All seven jobs should be BLOCKED for 300 seconds because of the
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

        # We only need one, but we do expect all jobs to be BLOCKED.
        statuses = [result.get('JobStatus', -1) == 9 for result in results]
        if all(statuses):
            print(f"\nAll jobs in cluster {job_handle.clusterid} are blocked.")
            break

        assert time.time() < deadline;

    # Turn off the schedd.
    the_condor.run_command(
        ['condor_off', '-schedd'],
        timeout=10,
        echo=False,
    )

    # Wait for it to die.
    cp = the_condor.run_command(
        ['condor_who', '-quick', '-wait:30', 'SCHEDD == "Hold"'],
        timeout=30,
        echo=False,
    )

    return job_handle


class TestBlockedJobs:


    def test_on_startup(self, the_condor, blocked_jobs_on_disk):
        clusterID = blocked_jobs_on_disk.clusterid

        # We only need one, but we expect that all jobs in the cluster
        # will be in blocked state on disk.
        with the_condor.job_queue_log.open(mode="r") as jql:
            for line in jql.readlines():
                if line.startswith("103 ") and " JobStatus " in line:
                    (_, jobid, __, status) = line.split()
                    if jobid.startswith(f"{clusterID}."):
                        # Job 1.0 switches to idle because it doesn't have
                        # a match record reserved for it when the transfer
                        # shadow dies (because the transfer shadow is using
                        # it); it must therefore be handled a special case.
                        #
                        # The non-prompting jobs don't leave the BLOCKED
                        # state because of a bug.
                        print(line)

        # Turn off the startd; if we don't, the newly-idled jobs will
        # try to start again.
        the_condor.run_command(
            ['condor_off', '-startd', '-fast'],
            timeout=10,
            echo=False,
        )

        # Wait for it to die.
        cp = the_condor.run_command(
            ['condor_who', '-quick', '-wait:30', 'STARTD == "Hold"'],
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
            ['condor_who', '-quick', '-wait:30', 'SCHEDD == "Alive"'],
            timeout=30,
            echo=False,
        )

        # Helpfully, the security subsystem holds onto our session from
        # the previous incarnation of the schedd; but for some reason, it
        # doesn't just retry if the save session has become invalid.
        schedd = the_condor.get_local_schedd()
        try:
            results = schedd.query(
                constraint=f'ClusterID == {blocked_jobs_on_disk.clusterid}',
                projection=["ProcID", "JobStatus"],
            )
        except htcondor2.HTCondorException:
            pass

        results = schedd.query(
            constraint=f'ClusterID == {blocked_jobs_on_disk.clusterid}',
            projection=["ProcID", "JobStatus"],
        )

        assert results is not None

        for result in results:
            assert result.get('JobStatus', -1) != 9
