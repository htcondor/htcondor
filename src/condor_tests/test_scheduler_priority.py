#!/usr/bin/env pytest

import logging

import htcondor

from ornithology import (
    action,
    JobID,
    SCRIPTS,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

NUM_JOBS = 5

@action
def submit_equal_priority_jobs(default_condor):
    cluster = default_condor.submit(
        {
            "executable": SCRIPTS["sleep"],
            "arguments": "1",
            "universe": "scheduler",
            "log": "scheduler_priority-equal.log",
        },
        count=NUM_JOBS,
    )
    cluster.wait(condition=ClusterState.all_terminal)
    return cluster


@action
def submit_unequal_priority_jobs(default_condor):
    cluster = default_condor.submit(
        {
            "executable": SCRIPTS["sleep"],
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
    return submit_equal_priority_jobs.event_log.filter(
        lambda event: event.type is htcondor.JobEventType.EXECUTE
    )


@action
def unequal_priority_execute_events(submit_unequal_priority_jobs):
    return submit_unequal_priority_jobs.event_log.filter(
        lambda event: event.type is htcondor.JobEventType.EXECUTE
    )


class TestCanRunSleepJob:
    def test_ran_all_equal_priority_jobs(self, equal_priority_execute_events):
        assert len(equal_priority_execute_events) == NUM_JOBS

    def test_equal_priority_jobs_run_in_submit_order(self, equal_priority_execute_events):
        """
        We expect equal priority jobs to run in the order they were submitted,
        which means they should run in job-id-order.
        """
        assert (
            sorted(equal_priority_execute_events, key=lambda event: JobID.from_job_event(event))
            == equal_priority_execute_events
        )

    def test_ran_all_unequal_priority_jobs(self, unequal_priority_execute_events):
        assert len(unequal_priority_execute_events) == NUM_JOBS

    def test_unequal_priority_jobs_run_in_priority_order(self, unequal_priority_execute_events):
        """
        We expect unequal priority jobs to run in the order of priority,
        which for the set up above, means they should run in reverse-job-id-order.
        """
        assert (
            sorted(
                unequal_priority_execute_events,
                key=lambda event: JobID.from_job_event(event),
                reverse=True,
            )
            == unequal_priority_execute_events
        )
