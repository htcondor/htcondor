#!/usr/bin/env pytest

import datetime
import logging
import os
import time

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
def dataflow_input_file(test_dir):
    return test_dir / "submit" / "dataflow-input.txt"


@action
def dataflow_output_file(test_dir):
    return test_dir / "submit" / "dataflow-output.txt"


@action
def submit_output_job_cmd(
    test_dir, default_condor, dataflow_input_file, dataflow_output_file
):
    # Start by creating an input file
    input_description = "dataflow-input"
    input_file = write_file(dataflow_input_file, input_description)

    # First job in our workflow outputs a basic file to disk.
    # Later jobs will compare timestamps of input and output to determine if dataflow.
    executable_description = """#!/bin/sh

    echo "dataflow-output" > {output}
    """.format(
        output=dataflow_output_file
    )
    executable_file = write_file(
        test_dir / "submit" / "dataflow-output.sh", executable_description
    )

    sub_description = """
        executable = {exe}

        queue
    """.format(
        exe=test_dir / "submit" / "dataflow-output.sh"
    )
    submit_file = write_file(
        test_dir / "submit" / "dataflow-output.sub", sub_description
    )

    return default_condor.run_command(["condor_submit", submit_file])


@action
def finished_output_jobid(default_condor, submit_output_job_cmd):
    clusterid, num_procs = parse_submit_result(submit_output_job_cmd)

    jobid = JobID(clusterid, 0)

    default_condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
    )

    return jobid


@action
def submit_dataflow_skip_job_cmd(
    test_dir,
    default_condor,
    finished_output_jobid,
    path_to_sleep,
    dataflow_input_file,
    dataflow_output_file,
):
    """
    After submit_output_job_cmd() has completed, we now want to send a new job
    with an argument to skip if it's a dataflow job.
    """

    sub_description = """
        executable = {exe}
        arguments = 10
        transfer_input_files = {input}
        transfer_output_files = {output}
        should_transfer_files = YES
        skip_if_dataflow = True
        log = {log}

        queue
    """.format(
        exe=path_to_sleep,
        input=dataflow_input_file,
        output=dataflow_output_file,
        log=test_dir / "submit" / "dataflow-skip.log",
    )
    submit_file = write_file(test_dir / "submit" / "dataflow-skip.sub", sub_description)

    return default_condor.run_command(["condor_submit", submit_file])


@action
def finished_dataflow_skip_jobid(default_condor, submit_dataflow_skip_job_cmd):
    clusterid, num_procs = parse_submit_result(submit_dataflow_skip_job_cmd)

    jobid = JobID(clusterid, 0)

    default_condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
    )

    return jobid


@action
def job_queue_events_for_dataflow_skip_job(
    default_condor, finished_dataflow_skip_jobid
):
    return default_condor.job_queue.by_jobid[finished_dataflow_skip_jobid]


@action
def submit_dataflow_noskip_job_cmd(
    test_dir,
    default_condor,
    finished_output_jobid,
    path_to_sleep,
    dataflow_input_file,
    dataflow_output_file,
):
    """
    After submit_output_job_cmd() has completed, we now want to send a new job
    with an argument to *not* skip if it's a dataflow job.
    """

    sub_description = """
        executable = {exe}
        arguments = 10
        transfer_input_files = {input}
        transfer_output_files = {output}
        should_transfer_files = YES
        skip_if_dataflow = False
        log = {log}

        queue
    """.format(
        exe=path_to_sleep,
        input=dataflow_input_file,
        output=dataflow_output_file,
        log=test_dir / "submit" / "dataflow-noskip.log",
    )
    submit_file = write_file(
        test_dir / "submit" / "dataflow-noskip.sub", sub_description
    )

    return default_condor.run_command(["condor_submit", submit_file])


@action
def finished_dataflow_noskip_jobid(default_condor, submit_dataflow_noskip_job_cmd):
    clusterid, num_procs = parse_submit_result(submit_dataflow_noskip_job_cmd)

    jobid = JobID(clusterid, 0)

    default_condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
    )

    return jobid


@action
def job_queue_events_for_dataflow_noskip_job(
    default_condor, finished_dataflow_noskip_jobid
):
    return default_condor.job_queue.by_jobid[finished_dataflow_noskip_jobid]


class TestDataflowJob:
    def test_submit_output_job_succeeded(self, submit_output_job_cmd):
        assert submit_output_job_cmd.returncode == 0

    def test_submit_dataflow_skip_job_succeeded(self, submit_dataflow_skip_job_cmd):
        assert submit_dataflow_skip_job_cmd.returncode == 0

    def test_dataflow_skip_job_completed_successfully(
        self, finished_dataflow_skip_jobid
    ):
        assert finished_dataflow_skip_jobid is not None

    def test_dataflow_skip_job_events_in_correct_order(
        self, job_queue_events_for_dataflow_skip_job
    ):
        assert (
            SetAttribute("DataflowJobSkipped", "true")
            in job_queue_events_for_dataflow_skip_job
        )

    def test_dataflow_noskip_job_completed_successfully(
        self, finished_dataflow_noskip_jobid
    ):
        assert finished_dataflow_noskip_jobid is not None

    def test_dataflow_noskip_job_events_in_correct_order(
        self, job_queue_events_for_dataflow_noskip_job
    ):
        assert (
            SetAttribute("DataflowJobSkipped", "true")
            not in job_queue_events_for_dataflow_noskip_job
        )
