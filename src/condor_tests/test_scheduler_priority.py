#!/usr/bin/env pytest

import logging

import htcondor2 as htcondor

from ornithology import (
    action,
    JobID,
    SCRIPTS,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

"""
This test submits two clusters of scheduler universe jobs, one where all jobs
have equal priority, the other where they have different priority, and makes
sure they are executed in the correct order.

This test also demonstrates two different approaches to using the pytest +
ornithology testing framework. The equal priority cluster is designed to be as
basic as possible, whereas the unequal priority cluster uses more special
functionality from both pytest and ornithology for more concise code.
"""

NUM_JOBS = 5


@action
def submit_equal_priority_jobs(default_condor, path_to_sleep):
    """
    Submit a cluster of 5 scheduler universe jobs with equal priority,
    and wait until they finish running.
    """
    cluster = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "1",
            "universe": "scheduler",
            "log": "scheduler_priority-equal.log",
        },
        count=NUM_JOBS,
    )
    cluster.wait(condition=ClusterState.all_terminal)
    return cluster


@action
def submit_unequal_priority_jobs(default_condor, path_to_sleep):
    """
    Submit a cluster of 5 scheduler universe jobs with unequal priority,
    (proc 1 has priority = 1, proc 2 has priority = 2, etc.) and wait until
    they finish running.
    """
    cluster = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "1",
            "universe": "scheduler",
            "log": "scheduler_priority-unequal.log",
            "priority": "$(process)",
        },
        count=NUM_JOBS,
    )
    cluster.wait(condition=ClusterState.all_terminal)
    return cluster


@action
def equal_priority_execute_events(submit_equal_priority_jobs):
    """
    Simple approach to retrieving execute events. Open the job event log,
    iterate over the events in order and add all execute events to a list.
    """
    jel = htcondor.JobEventLog("scheduler_priority-equal.log")
    execute_events = []
    for event in jel.events(0):
        if event.type == htcondor.JobEventType.EXECUTE:
            execute_events.append(event)
    return execute_events


@action
def unequal_priority_execute_events(submit_unequal_priority_jobs):
    """
    Ornithology approach to retrieving execute events, using a filter handle
    with a lambda.
    """
    return submit_unequal_priority_jobs.event_log.filter(
        lambda event: event.type is htcondor.JobEventType.EXECUTE
    )


class TestSchedulerPriority:
    def test_ran_all_equal_priority_jobs(self, equal_priority_execute_events):
        assert len(equal_priority_execute_events) == NUM_JOBS

    def test_equal_priority_jobs_run_in_submit_order(
        self, equal_priority_execute_events
    ):
        """
        We expect equal priority jobs to run in the order they were submitted,
        which means they should run in job-id-order.
        Simple approach, just iterate over the list of events in a for-loop
        and make sure proc ids appear in ascending order.
        """
        for i in range(1, NUM_JOBS):
            assert (
                JobID.from_job_event(equal_priority_execute_events[i]).proc
                > JobID.from_job_event(equal_priority_execute_events[i - 1]).proc
            )

    def test_ran_all_unequal_priority_jobs(self, unequal_priority_execute_events):
        assert len(unequal_priority_execute_events) == NUM_JOBS

    def test_unequal_priority_jobs_run_in_priority_order(
        self, unequal_priority_execute_events
    ):
        """
        We expect unequal priority jobs to run in the order of priority,
        which for the set up above, means they should run in reverse-job-id-order.
        Josh's Pythonic approach using the sorted() function.
        """
        assert (
            sorted(
                unequal_priority_execute_events,
                key=lambda event: JobID.from_job_event(event),
                reverse=True,
            )
            == unequal_priority_execute_events
        )
