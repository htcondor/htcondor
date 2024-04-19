#!/usr/bin/env pytest

from pathlib import Path

from ornithology import (
    action,
    Condor,
    ClusterState,
)

import logging


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            'SCHEDD_DEBUG':                             'D_TEST',
            'BYTES_REQUIRED_TO_QUEUE_FOR_TRANSFER':     256,
        },
    ) as the_condor:
        (test_dir / 'small.txt').write_text("one two three")
        (test_dir / 'large.txt').write_text(
"""
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
one two three four five six seven eight nine ten
"""
        )
        yield the_condor


@action
def the_job_description(path_to_sleep, test_dir):
    return {
        "executable":                   path_to_sleep,
        "arguments":                    5,
        "transfer_executable":          "false",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",
        "log":                          test_dir / "test_job_$(CLUSTER).log",
    }


@action
def completed_small_job(test_dir, the_condor, the_job_description):
    complete_job_description = {
        ** the_job_description,
        ** { 'transfer_input_files': (test_dir / 'small.txt').as_posix(), },
    }

    the_job_handle = the_condor.submit(
        description=complete_job_description,
        count=1,
    )

    assert the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle;


@action
def completed_large_job(test_dir, the_condor, the_job_description):
    complete_job_description = {
        ** the_job_description,
        ** { 'transfer_input_files': (test_dir / 'large.txt').as_posix(), },
    }

    the_job_handle = the_condor.submit(
        description=complete_job_description,
        count=1,
    )

    assert the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle;


@action
def the_schedd_log(the_condor, completed_small_job, completed_large_job):
    return the_condor.schedd_log.open()


@action
def enqueued_jobs(the_schedd_log):
    jobs = []

    for message in the_schedd_log.read():
        if 'TransferQueueManager: enqueueing' in message:
            words = str(message).split(' ')
            for i in range(len(words)):
                if words[i] == "job":
                    jobs.append(words[i+1])

    return jobs


class TestDontQueueSmallSandboxes:

    def test_queue_skipped(self, completed_small_job, enqueued_jobs):
        jobID = f"{completed_small_job.clusterid}.0"
        assert not jobID in enqueued_jobs

    def test_queue_entered(self, completed_large_job, enqueued_jobs):
        jobID = f"{completed_large_job.clusterid}.0"
        assert jobID in enqueued_jobs
