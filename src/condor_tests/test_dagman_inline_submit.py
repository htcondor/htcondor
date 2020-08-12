#!/usr/bin/env pytest

import logging
import textwrap

import htcondor

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


@action(
    params={
        "expr": "(JobExitStatus > 0)",
        "int": "1",
        "string": '"Hello, inline submit!"',
    }
)
def custom_attribute_value(request):
    return request.param


@action
def dag_description(path_to_sleep, custom_attribute_value):
    # the {{ and }} are escaped { and } because they are f-string delimiters
    return textwrap.dedent(
        """
        JOB HelloInlineSubmit {{
            executable = {}
            arguments = 1
            output = inline.out
            MY.CustomAttribute = {}
        }}
    """.format(
            path_to_sleep, custom_attribute_value,
        )
    )


sequence_number = 0


@action
def dag_dir(test_dir, dag_description):
    # We depend on dag_description here to force a new dag_dir for each description
    # We use a simple global index to track which dag_description we're working on
    global sequence_number
    sequence_number += 1

    return test_dir / "dagman-{}".format(sequence_number)


@action
def inline_dag_job(condor, dag_description, dag_dir):
    dag_file = write_file(dag_dir / "inline.dag", dag_description)
    dag = htcondor.Submit.from_dag(str(dag_file))
    dag_job = condor.submit(dag)

    # TODO: Check here if something went wrong and abort
    # dag_job.wait(condition=ClusterState.???)

    # Now wait for the job to complete before returning
    dag_job.wait(condition=ClusterState.all_terminal)
    # Advance the job queue log as well
    condor.job_queue.wait_for_job_completion(dag_job.job_ids)

    return dag_job


@action
def dagman_terminate_event(inline_dag_job):
    terminate_event = inline_dag_job.event_log.filter(
        lambda event: event.type == htcondor.JobEventType.JOB_TERMINATED
    )
    assert len(terminate_event) == 1
    return terminate_event[0]


@action
def finished_inline_jobid(inline_dag_job, dag_dir):
    jel = htcondor.JobEventLog(str(dag_dir / "inline.dag.nodes.log"))
    for event in jel.events(0):
        if event.type == htcondor.JobEventType.SUBMIT:
            return JobID.from_job_event(event)
    assert False


@action
def job_queue_events_for_inline_job(condor, finished_inline_jobid):
    return condor.job_queue.by_jobid[finished_inline_jobid]


class TestDagmanInlineSubmit:
    def test_inline_dagman_job_completed(self, inline_dag_job):
        assert inline_dag_job.state[0] == JobStatus.COMPLETED

    def test_inline_dagman_job_returned_zero(self, dagman_terminate_event, test_dir):
        for file in (test_dir / "dagman-{}".format(sequence_number)).iterdir():
            print("Contents of {}".format(file))
            print(textwrap.indent(file.read_text(), prefix=" " * 4))
            print()
        assert dagman_terminate_event["ReturnValue"] == 0

    def test_inline_job_submitted_successfully(self, finished_inline_jobid):
        assert finished_inline_jobid is not None

    def test_inline_job_events_in_correct_order(self, job_queue_events_for_inline_job):
        assert in_order(
            job_queue_events_for_inline_job,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.COMPLETED),
            ],
        )

    def test_inline_job_has_custom_attribute(
        self,
        condor,
        custom_attribute_value,
        job_queue_events_for_inline_job,
        finished_inline_jobid,
    ):
        assert (
            JobID(finished_inline_jobid.cluster, -1),
            SetAttribute("CustomAttribute", custom_attribute_value),
        ) in condor.job_queue.events()
