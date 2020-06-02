#!/usr/bin/env pytest

import htcondor
import logging


from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "DAGMAN_USE_CONDOR_SUBMIT": "False",
            "DAGMAN_USE_STRICT": "0"
        },
    ) as condor:
        yield condor


@action(params={"expr":"(JobExitStatus > 0)", "int":"1", "string":'"Hello, inline submit!"'})
def custom_attribute_value(request):
    return request.param

sequence_number = 0

@action
def submit_inline_dag(test_dir, condor, custom_attribute_value):
    global sequence_number
    sequence_number += 1
    dag_description = f"""
        JOB HelloInlineSubmit {{
            executable = /bin/echo
            arguments = Hello, inline submit!
            output = inline.out
            MY.CustomAttribute = {custom_attribute_value}
        }}
    """

    dag_file = write_file(test_dir / f"dagman-{sequence_number}" / "inline.dag", dag_description)
    dag = htcondor.Submit.from_dag(str(dag_file))
    dag_job = condor.submit(dag)
    
    # TODO: Check here if something went wrong and abort
    # dag_job.wait(condition=ClusterState.???)

    # Now wait for the job to complete before returning
    dag_job.wait(condition=ClusterState.all_terminal)
    return dag_job

@action
def dagman_terminate_event(submit_inline_dag):
    terminate_event = submit_inline_dag.event_log.filter(
        lambda event: event.type == htcondor.JobEventType.JOB_TERMINATED
    )
    assert len(terminate_event) == 1
    return terminate_event[0]

@action
def finished_inline_jobid(submit_inline_dag, test_dir):
    jel = htcondor.JobEventLog(str(test_dir / f"dagman-{sequence_number}" / "inline.dag.nodes.log"))
    for event in jel.events(0):
        if event.type == htcondor.JobEventType.SUBMIT:
            return JobID.from_job_event(event)
    assert False

@action
def job_queue_events_for_inline_job(condor, finished_inline_jobid):
    list(condor.job_queue.read_transactions())
    return condor.job_queue.by_jobid[finished_inline_jobid]


class TestDagmanInlineSubmit:
    def test_inline_dagman_completed_successfully(self, submit_inline_dag):
        assert submit_inline_dag.state[0] == JobStatus.COMPLETED

    def test_inline_dagman_returned_zero(self, dagman_terminate_event):
        assert dagman_terminate_event['ReturnValue'] == 0

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
    def test_inline_job_has_custom_attribute(self, condor, custom_attribute_value, job_queue_events_for_inline_job, finished_inline_jobid):  
        assert (JobID(finished_inline_jobid.cluster, -1), SetAttribute("CustomAttribute", custom_attribute_value)) in condor.job_queue.events()

