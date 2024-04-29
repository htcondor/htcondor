#!/usr/bin/env pytest


# Test that we can run multi-proc parallel uni
# job on multiple pslots

import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Standup a condor that can run parallel jobs
@standup
def condor(test_dir):
    with Condor(test_dir / "condor", 
            config={
                "NUM_CPUS": "36",
                "NUM_SLOTS_TYPE_1": "1",
                "SLOT_TYPE_1_PARTITIONABLE": True,
                "SLOT_TYPE_1": "cpus=12",
                "NUM_SLOTS_TYPE_2": "1",
                "SLOT_TYPE_2_PARTITIONABLE": True,
                "SLOT_TYPE_2": "cpus=12",
                "NUM_SLOTS_TYPE_3": "1",
                "SLOT_TYPE_3_PARTITIONABLE": True,
                "SLOT_TYPE_3": "cpus=12",

                "SCHEDD_NAME": "schedd",
                "DedicatedScheduler": "\"DedicatedScheduler@schedd@$(FULL_HOSTNAME)\"",
            }) as condor:
        yield condor

# Submit a multi-proc parallel job that will require pslot splitting
# not sure we can submit directly with python bindings.
@action
def submit_parallel_job(test_dir, condor):
    sub_description = """
        universe = parallel
        executable = /bin/sleep 
        arguments = 2

        should_transfer_files = yes
        when_to_transfer_output = on_exit_or_evict
        
        log = job.log
        machine_count = 4
        queue 
        machine_count = 1
        queue 
    """
    submit_file = write_file(test_dir / "submit" / "job.sub", sub_description)

    return condor.run_command(["condor_submit", submit_file])

@action
def finished_parallel_job(condor, submit_parallel_job):
    clusterid, num_procs = parse_submit_result(submit_parallel_job)

    jobid = JobID(clusterid, 0)

    condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
        timeout=300
    )

    return jobid

@action
def events_for_parallel_job(condor, finished_parallel_job):
    return condor.job_queue.by_jobid[finished_parallel_job]

class TestParallelUni:
    def test_job_completes(self, events_for_parallel_job):
            assert in_order(
                events_for_parallel_job,
                [
                    SetJobStatus(JobStatus.COMPLETED),
                ],
                )
