#!/usr/bin/env pytest


# Test that a startd that reports to a collector whose name
# doesn't exist neither crashes nor hangs

import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Setup the startd to also report to a bogus collector
@standup
def condor(test_dir):
    with Condor(test_dir / "condor", config={"NUM_CPUS": "4", "COLLECTOR_HOST": "$(COLLECTOR_HOST), bogus.example.com"}) as condor:
        yield condor

@action
def submit_sleep_job_cmd(test_dir, condor, path_to_sleep):
    sub_description = """
        executable = {exe}
        arguments = 1
        
        queue
    """.format(
        exe=path_to_sleep
    )
    submit_file = write_file(test_dir / "submit" / "job.sub", sub_description)

    return condor.run_command(["condor_submit", submit_file])


@action
def finished_sleep_jobid(condor, submit_sleep_job_cmd):
    clusterid, num_procs = parse_submit_result(submit_sleep_job_cmd)

    jobid = JobID(clusterid, 0)

    condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
    )

    return jobid


@action
def job_queue_events_for_sleep_job(condor, finished_sleep_jobid):
    return condor.job_queue.by_jobid[finished_sleep_jobid]


class TestBogusCollector:
    def test_submit_cmd_succeeded(self, submit_sleep_job_cmd):
        assert submit_sleep_job_cmd.returncode == 0

    def test_job_queue_events_in_correct_order(self, job_queue_events_for_sleep_job):
        assert in_order(
            job_queue_events_for_sleep_job,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.COMPLETED),
            ],
        )

    def test_job_executed_successfully(self, job_queue_events_for_sleep_job):
        assert SetAttribute("ExitCode", "0") in job_queue_events_for_sleep_job
