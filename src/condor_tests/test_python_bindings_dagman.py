#!/usr/bin/env pytest

import htcondor
import logging
import textwrap

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor", config={"DAGMAN_USE_STRICT": "0"},
    ) as condor:
        yield condor


@action
def dag_dir(test_dir):
    return test_dir / "dagman"


@action
def valid_dag_job(condor, dag_dir, path_to_sleep):
    sub_description = """
        executable = {exe}
        arguments = 1
        
        queue
    """.format(
        exe=path_to_sleep,
    )
    sub_file = write_file(dag_dir / "sleep.sub", sub_description)

    dag_description = """
        JOB sleep {}
    """.format(
        sub_file
    )
    dag_file = write_file(dag_dir / "valid.dag", dag_description)

    dag = htcondor.Submit.from_dag(str(dag_file))
    dag_job = condor.submit(dag)
    # Now wait for the dag to complete before returning
    dag_job.wait(condition=ClusterState.all_terminal)
    # Advance the job queue log as well
    condor.job_queue.wait_for_job_completion(dag_job.job_ids)

    return dag_job


@action
def invalid_dag_job(condor, dag_dir):
    dag_description = """
        JOB invalid dag syntax
    """
    dag_file = write_file(dag_dir / "invalid.dag", dag_description)

    dag = htcondor.Submit.from_dag(str(dag_file))
    dag_job = condor.submit(dag)
    # Now wait for the dag to complete before returning
    dag_job.wait(condition=ClusterState.all_terminal)
    # Advance the job queue log as well
    condor.job_queue.wait_for_job_completion(dag_job.job_ids)

    return dag_job


@action
def invalid_dagman_binding(condor, dag_dir):
    dag_description = """
        JOB sleep sleep.sub
    """
    dag_file = write_file(dag_dir / "files-exist.dag", dag_description)

    # Now output a dummy files-exist.dag.condor.sub file. This should cause
    # htcondor.Submit.from_dag() to throw an exception.
    write_file(dag_dir / "files-exist.dag.condor.sub", "")

    try:
        dag = htcondor.Submit.from_dag(str(dag_file))
    except:
        return 0

    assert False


@action
def valid_dag_terminate_event(valid_dag_job):
    terminate_event = valid_dag_job.event_log.filter(
        lambda event: event.type == htcondor.JobEventType.JOB_TERMINATED
    )
    assert len(terminate_event) == 1
    return terminate_event[0]


@action
def invalid_dag_terminate_event(invalid_dag_job):
    terminate_event = invalid_dag_job.event_log.filter(
        lambda event: event.type == htcondor.JobEventType.JOB_TERMINATED
    )
    assert len(terminate_event) == 1
    return terminate_event[0]


class TestDagmanInlineSubmit:
    def test_valid_dag_job_completed(self, valid_dag_job):
        assert valid_dag_job.state[0] == JobStatus.COMPLETED

    def test_valid_dag_job_returned_zero(self, valid_dag_terminate_event):
        assert valid_dag_terminate_event["ReturnValue"] == 0

    def test_invalid_dag_job_returned_one(self, invalid_dag_terminate_event):
        assert invalid_dag_terminate_event["ReturnValue"] == 1

    def test_invalid_dagman_binding(self, invalid_dagman_binding):
        assert invalid_dagman_binding == 0
