#!/usr/bin/env pytest

import logging


from ornithology import (
    config,
    standup,
    action,
    write_file,
    parse_submit_result,
    JobID,
    SetAttribute,
    SetJobStatus,
    JobStatus,
    in_order,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def submit_sleep_job_cmd(test_dir, default_condor, path_to_sleep):
    sub_description = """
        executable = {exe}
        arguments = 1
        
        queue
    """.format(
        exe=path_to_sleep
    )
    submit_file = write_file(test_dir / "submit" / "job.sub", sub_description)

    return default_condor.run_command(["condor_submit", submit_file])


@action
def finished_sleep_jobid(default_condor, submit_sleep_job_cmd):
    clusterid, num_procs = parse_submit_result(submit_sleep_job_cmd)

    jobid = JobID(clusterid, 0)

    default_condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
    )

    return jobid


@action
def job_queue_events_for_sleep_job(default_condor, finished_sleep_jobid):
    return default_condor.job_queue.by_jobid[finished_sleep_jobid]


class TestCanRunSleepJob:
    def test_submit_cmd_succeeded(self, submit_sleep_job_cmd):
        assert submit_sleep_job_cmd.returncode == 0

    def test_only_one_proc(self, submit_sleep_job_cmd):
        _, num_procs = parse_submit_result(submit_sleep_job_cmd)
        assert num_procs == 1

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
