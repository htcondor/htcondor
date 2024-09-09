#!/usr/bin/env pytest

import logging
import textwrap

import htcondor2 as htcondor

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={"DAGMAN_USE_CONDOR_SUBMIT": "False", "DAGMAN_USE_STRICT": "0"},
    ) as condor:
        yield condor


@action
def valid_submitdesc_dag_description(path_to_sleep):
    # the {{ and }} are escaped { and } because they are f-string delimiters
    return textwrap.dedent(
        """
        # Create a simple SUBMIT-DESCRIPTION statement and attach it to a single job
        SUBMIT-DESCRIPTION ValidSubmitDescription {{
            executable = {}
            arguments = 1
        }}
        JOB Valid ValidSubmitDescription

        # Create another SUBMIT-DESCRIPTION statement and attach it to multiple jobs
        SUBMIT-DESCRIPTION AnotherSubmitDescription {{
            executable = {}
            arguments = 1
        }}
        JOB AlsoValid1 AnotherSubmitDescription
        JOB AlsoValid2 AnotherSubmitDescription
        JOB AlsoValid3 AnotherSubmitDescription

        # Create a third SUBMIT-DESCRIPTION statement with a name that is already taken
        # This should produce a warning in the .dagman.out file, but still run successfully
        SUBMIT-DESCRIPTION ValidSubmitDescription {{
            executable = /bin/echo
            arguments = "The name ValidSubmitDescription is already taken, this should produce a warning"
        }}
        """.format(path_to_sleep, path_to_sleep)
    )


@action
def invalid_submitdesc_dag_description(path_to_sleep):
    # the {{ and }} are escaped { and } because they are f-string delimiters
    return textwrap.dedent(
        """
        # Create an invalid SUBMIT-DESCRIPTION statement, which is missing a closing bracket
        SUBMIT-DESCRIPTION InvalidSubmitDescription {{
            executable = {}
            arguments = 1
        JOB Invalid InvalidSubmitDescription
        """.format(path_to_sleep)
    )


@action
def dag_dir(test_dir, valid_submitdesc_dag_description):
    return test_dir / "dagman"


@action
def valid_submitdesc_dagman_job(condor, valid_submitdesc_dag_description, dag_dir):
    dag_file = write_file(dag_dir / "valid_submitdesc.dag", valid_submitdesc_dag_description)
    dag = htcondor.Submit.from_dag(str(dag_file))
    dagman_job = condor.submit(dag)

    # Now wait for the condor_dagman job to complete before returning
    dagman_job.wait(condition=ClusterState.all_terminal)
    # Advance the job queue log as well
    condor.job_queue.wait_for_job_completion(dagman_job.job_ids)

    return dagman_job


@action
def invalid_submitdesc_dagman_job(condor, invalid_submitdesc_dag_description, dag_dir):
    dag_file = write_file(dag_dir / "invalid_submitdesc.dag", invalid_submitdesc_dag_description)
    dag = htcondor.Submit.from_dag(str(dag_file))
    dagman_job = condor.submit(dag)

    # Now wait for the condor_dagman job to complete before returning
    dagman_job.wait(condition=ClusterState.all_terminal)
    # Advance the job queue log as well
    condor.job_queue.wait_for_job_completion(dagman_job.job_ids)

    return dagman_job


@action
def valid_submitdesc_dagman_terminate_event(valid_submitdesc_dagman_job):
    terminate_event = valid_submitdesc_dagman_job.event_log.filter(
        lambda event: event.type == htcondor.JobEventType.JOB_TERMINATED
    )
    assert len(terminate_event) == 1
    return terminate_event[0]


@action
def finished_valid_submitdesc_jobid(valid_submitdesc_dag_job, dag_dir):
    jel = htcondor.JobEventLog(str(dag_dir / "valid_submitdesc.dag.nodes.log"))
    for event in jel.events(0):
        if event.type == htcondor.JobEventType.SUBMIT:
            return JobID.from_job_event(event)
    assert False


@action
def job_queue_events_for_valid_submitdesc_dag(dag_dir):
    jel = htcondor.JobEventLog(str(dag_dir / "valid_submitdesc.dag.nodes.log"))
    return jel.events(0)


@action
def debug_output_for_invalid_submitdesc_dag(dag_dir, invalid_submitdesc_dagman_job):
    output_log = open(str(dag_dir / "invalid_submitdesc.dag.dagman.out"), "r")
    contents = output_log.readlines()
    output_log.close()
    return contents



class TestDagmanSubmitDescription:
    # Verify the valid condor_dagman job completed
    def test_valid_submitdesc_dagman_job_completed(self, valid_submitdesc_dagman_job):
        assert valid_submitdesc_dagman_job.state[0] == JobStatus.COMPLETED

    # Verify the valid condor_dagman job returned 0
    def test_valid_submitdesc_dagman_job_returned_zero(self, valid_submitdesc_dagman_terminate_event, test_dir):
        for file in (test_dir / "dagman").iterdir():
            print("Contents of {}".format(file))
            print(textwrap.indent(file.read_text(), prefix=" " * 4))
            print()
        assert valid_submitdesc_dagman_terminate_event["ReturnValue"] == 0

    # We also want to verify all the jobs within the valid DAGMan job completed
    def test_valid_submitdesc_jobs_completed(self, job_queue_events_for_valid_submitdesc_dag):
        completed_events = 0
        for event in job_queue_events_for_valid_submitdesc_dag:
           if event.type == htcondor.JobEventType.JOB_TERMINATED:
               completed_events = completed_events + 1
        # We expect to see 4 completed events:
        # 1 event for the ValidSubmitDescription SUBMIT-DESCRIPTION, which was attached to a single job
        # 3 events for the AnotherSubmitDescription SUBMIT-DESCRIPTION, which was attached to three jobs
        assert completed_events == 4

    # Verify the invalid condor_dagman job failed with a parse error
    def test_invalid_submitdesc_dagman_job_failed(self, debug_output_for_invalid_submitdesc_dag):
        parse_error_in_log = False
        for line in debug_output_for_invalid_submitdesc_dag:
            if "Failed to parse" in line:
                parse_error_in_log = True
                break
        assert parse_error_in_log
