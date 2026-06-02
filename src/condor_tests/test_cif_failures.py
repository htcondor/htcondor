#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
)

import os
import time
from pathlib import Path
import shutil

import htcondor2
from htcondor2 import JobEventType

@action
def the_lock_dir(test_dir):
    return test_dir / "lock.d"


@action
def the_condor(test_dir, the_lock_dir):
    local_dir = test_dir / "condor"

    with Condor(
        submit_user='tlmiller',
        condor_user='condor',
        local_dir=local_dir,
        config={
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":             the_lock_dir.as_posix(),
            "NUM_CPUS":         4,
        },
    ) as the_condor:
        yield the_condor


@action
def held_cif_job(the_condor, path_to_sleep):
    job_description = {
        "universe":                 "vanilla",

        "executable":               path_to_sleep,
        "arguments":                "20",
        "transfer_executable":      True,
        "should_transfer_files":    True,

        "log":                      "cif_job.log",
        "output":                   "cif_job.output",
        "error":                    "cif_job.error",

        "request_cpus":             1,
        "request_memory":           1,

        "MY._x_common_input_catalogs":      '"my_common_files"',
        "MY._x_catalog_my_common_files":    '"debug://error/"',

        "leave_in_queue":           True,
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=1,
    )

    assert job_handle.wait(
        timeout=200,
        condition=ClusterState.all_held,
    )

    return job_handle


class TestCIF:

    def test_transfer_failure(self, test_dir, the_condor, held_cif_job):
        expected_prefix = [
            JobEventType.SUBMIT,
            JobEventType.COMMON_FILES,
            JobEventType.COMMON_FILES,
            JobEventType.FILE_TRANSFER,
            JobEventType.FILE_TRANSFER,
            JobEventType.FILE_TRANSFER,
        ]
        optional = [
            # It's a race condition as to whether or not the output transfer
            # finished event gets written.  It looks like the parent shadow
            # process doesn't reap its forked transfer child if the starter's
            # eviction notice arrives first.
            JobEventType.FILE_TRANSFER,
        ]
        expected_suffix = [
            JobEventType.REMOTE_ERROR,
            JobEventType.JOB_EVICTED,
            JobEventType.JOB_HELD,
        ]

        actual = [ e.type for e in held_cif_job.event_log.events ]

        actual_prefix = actual[0:len(expected_prefix)]
        assert expected_prefix == actual_prefix

        if len(expected_prefix) + len(expected_suffix) != len(actual):
            assert optional == [actual[len(expected_prefix)]]

        actual_suffix = actual[-1 * len(expected_suffix):]
        assert expected_suffix == actual_suffix
