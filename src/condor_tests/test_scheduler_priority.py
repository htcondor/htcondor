#!/usr/bin/env pytest

import htcondor
import logging


from ornithology import (
    config,
    standup,
    action,
    JobID,
    SetAttribute,
    SetJobStatus,
    JobStatus,
    in_order,
    SCRIPTS,
    ClusterState
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def submit_equal_priority_jobs(test_dir, default_condor):
    cluster = default_condor.submit(
        {
            "executable": SCRIPTS["sleep"],
            "arguments": "1",
            "universe": "scheduler",
            "log": "scheduler_priority-equal.log"
        },
        count=5
    )
    cluster.wait(condition=ClusterState.all_terminal)
    return cluster

@action
def submit_unequal_priority_jobs(test_dir, default_condor):
    cluster = default_condor.submit(
        {
            "executable": SCRIPTS["sleep"],
            "arguments": "1",
            "universe": "scheduler",
            "log": "scheduler_priority-unequal.log",
            "priority": "$(process)"
        },
        count=5
    )
    cluster.wait(condition=ClusterState.all_terminal)
    return cluster

@action
def equal_priority_events(submit_equal_priority_jobs):
    jel = htcondor.JobEventLog("scheduler_priority-equal.log")
    execute_events = []
    for event in jel.events(0):
        if event.type == htcondor.JobEventType.EXECUTE:
            execute_events.append(event)
    return execute_events

@action
def unequal_priority_events(submit_unequal_priority_jobs):
    jel = htcondor.JobEventLog("scheduler_priority-unequal.log")
    execute_events = []
    for event in jel.events(0):
        if event.type == htcondor.JobEventType.EXECUTE:
            execute_events.append(event)
    return execute_events


class TestCanRunSleepJob:

    # We expect equal priority jobs to run in the order they were submitted.
    # So each execution procid in the list to be lesser than the one after it.
    def test_equal_priority_jobs_run_in_submit_order(self, equal_priority_events):
        for i in range(1,4):
            assert JobID.from_job_event(equal_priority_events[i]).proc > JobID.from_job_event(equal_priority_events[i-1]).proc


    # We expect unequal priority jobs to run in the order of priority.
    # So each execution procid in the list to be greater than the one after it.
    def test_unequal_priority_jobs_run_in_priority_order(self, unequal_priority_events):
        for i in range(1,4):
            assert JobID.from_job_event(unequal_priority_events[i]).proc < JobID.from_job_event(unequal_priority_events[i-1]).proc

