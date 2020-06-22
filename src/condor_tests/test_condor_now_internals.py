#!/usr/bin/env pytest

#
# This test duplicates cmd_now.run
#

import logging
import htcondor

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
def victim_jobs(condor, job_parameters):
    victim_jobs = condor.submit(job_parameters, count=2)

    assert victim_jobs.wait(
        condition=ClusterState.all_running,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return victim_jobs


# Note that the beneficiary job fixtures depend on the victim job features,
# not because they need to know about them, but to ensure that the victim
# jobs have all started running before submitting the beneficiary.
@action
def beneficiary_job(job_parameters, condor, victim_jobs):
    return condor.submit(job_parameters, count=1)


@action
def job_log(victim_jobs, beneficiary_job, condor, test_dir):
    bID = str(beneficiary_job.job_ids[0])
    vIDs = [str(vID) for vID in victim_jobs.job_ids]
    rv = condor.run_command([ 'condor_now', '--flags', '1', bID, *vIDs ])
    assert rv.returncode == 0

    jel = htcondor.JobEventLog((test_dir / "condor_now_internals.log").as_posix())

    num_evicted = 0;
    for e in jel.events(60):
        if e.type == htcondor.JobEventType.JOB_EVICTED and e.cluster == victim_jobs.clusterid:
            num_evicted += 1
            if num_evicted == len(vIDs):
                break
    assert num_evicted == len(vIDs)

    return jel


@action
def schedd_log(job_log, condor, victim_jobs, beneficiary_job):
    # There's two slots and three jobs in the queue, so at least one of the
    # victim jobs must end up running.
    assert victim_jobs.wait(
        condition=ClusterState.any_running,
        timeout=60,
        fail_condition=ClusterState.any_held,
    )

    # There are two slots in the pool, and three jobs in the queue.
    bID = str(beneficiary_job.job_ids[0])
    if beneficiary_job.state.any_running():
        runningJobID = bID
        for i, status in items(victim_jobs.state):
            if status == JobStatus.IDLE:
                idleJobID = str(victim_jobs.jobs_ids[i])
    else:
        runningJobID = str(victim_jobs.job_ids[0])
        idleJobID = bID

    rv = condor.run_command(['condor_now', '--flags', '2', idleJobID, runningJobID])
    assert rv.returncode != 0

    return condor.schedd_log.open()


class TestCondorNowInternals:
    def test_internals(self, schedd_log):
        assert schedd_log.wait(
            condition=lambda line: line.message == "pcccTest(): Succeeded.",
            timeout=60,
        )
