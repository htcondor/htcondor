#!/usr/bin/env pytest

#
# This test duplicates cmd_now.run
#

import logging
import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    ClusterState,
    JobStatus,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config
def condor_config():
    config = {
        "NUM_CPUS": 2,
        "SLOT_TYPE_0_PARTITIONABLE": "false",
        "SCHEDD_DEBUG": "D_CATEGORY D_SUB_SECOND D_TEST",
    }
    raw_config = None
    return {"config": config, "raw_config": raw_config}


@standup
def condor(condor_config, test_dir):
    with Condor(test_dir / "condor", **condor_config) as condor:
        yield condor


@action
def job_parameters(path_to_sleep):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "600",
        "log": "condor_now_internals.log",
    }


@action
def jobs(condor, job_parameters):
    jobs = condor.submit(job_parameters, count=3)

    assert jobs.wait(
        condition=ClusterState.running_exactly(2),
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return jobs


@action
def job_log(jobs, condor, test_dir):
    bID = str(jobs.state.by_name[JobStatus.IDLE][0])
    vIDs = [str(vID) for vID in jobs.state.by_name[JobStatus.RUNNING]]

    rv = condor.run_command(["condor_now", "--flags", "1", bID, *vIDs])
    assert rv.returncode == 0

    # Consider converting this into a wait() for these jobs to go idle, and
    # then assert ordering about the eviction event in
    # jobs.event_log.events.
    jel = htcondor.JobEventLog((test_dir / "condor_now_internals.log").as_posix())

    num_evicted = 0
    for e in jel.events(60):
        if e.type == htcondor.JobEventType.JOB_EVICTED and e.cluster == jobs.clusterid:
            num_evicted += 1
            if num_evicted == len(vIDs):
                break
    assert num_evicted == len(vIDs)

    return jel


@action
def schedd_log(job_log, condor, jobs):
    assert jobs.wait(
        condition=ClusterState.running_exactly(2),
        timeout=60,
        fail_condition=ClusterState.any_held,
    )

    idleJobID = str(jobs.state.by_name[JobStatus.IDLE][0])
    runningJobID = str(jobs.state.by_name[JobStatus.RUNNING][0])

    rv = condor.run_command(
        ["condor_now", "--flags", "2", str(idleJobID), str(runningJobID)]
    )
    assert rv.returncode != 0

    return condor.schedd_log.open()


class TestCondorNowInternals:
    def test_internals(self, schedd_log):
        assert schedd_log.wait(
            condition=lambda line: line.message == "pcccTest(): Succeeded.", timeout=60,
        )
