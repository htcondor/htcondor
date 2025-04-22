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
from pathlib import Path


@action
def the_lock_dir(test_dir):
    return test_dir / "lock.d"


@action
def the_condor(test_dir, the_lock_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "SHADOW_DEBUG":     "D_CATEGORY D_ZKM D_SUB_SECOND D_PID",
            "LOCK":             the_lock_dir.as_posix(),
        },
    ) as the_condor:
        yield the_condor


@action
def completed_cif_job(the_condor, path_to_sleep, test_dir):
    os.chdir(test_dir.as_posix())
    (test_dir / "input1.txt").write_text("input A\n")
    (test_dir / "input2.txt").write_text("II input\n");
    (test_dir / "input3.txt").write_text("input line 3\n" );

    job_description = {
        "universe":                 "vanilla",

        "shell":                    "cat input1.txt input2.txt input3.txt; sleep 5",
        "transfer_executable":      False,
        "should_transfer_files":    True,

        "arguments":                5,
        "log":                      "cif_job.log",
        "output":                   "cif_job.output",
        "error":                    "cif_job.error",

        "request_cpus":             1,
        "request_memory":           1,

        "MY.CommonInputFiles":      '"input1.txt, input2.txt"',
        "transfer_input_files":     "input3.txt",

        "leave_in_queue":           True,
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=1,
    )

    assert job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
    )

    return job_handle


def lock_dir_is_clean(the_lock_dir):
    files = list(the_lock_dir.iterdir())
    for file in files:
        assert file.name == "shared_port_ad" or file.name == "InstanceLock"
    assert len(files) == 2


def output_is_as_expected(completed_cif_job):
    ad = completed_cif_job.query(
        projection=['Out'],
        limit=1,
    )[0];

    out = Path(ad['Out'])
    text = out.read_text()
    assert text == "input A\nII input\ninput line 3\n"


def count_shadow_log_lines(the_condor, the_phrase):
    shadow_log = the_condor.shadow_log.open()
    lines = list(filter(
        lambda line: the_phrase in line,
        shadow_log.read(),
    ))
    return len(lines)


def shadow_log_is_as_expected(the_condor):
    staging_commands_sent = count_shadow_log_lines(
        the_condor, "StageCommonFiles"
    )
    assert staging_commands_sent == 1

    successful_staging_commands = count_shadow_log_lines(
        the_condor, "Staging successful"
    )
    assert successful_staging_commands == 1

    keyfile_touches = count_shadow_log_lines(
        the_condor, "Elected producer touch"
    )
    assert keyfile_touches == 1


class TestCIF:

    def test_one_cif_job(self, the_lock_dir, the_condor, completed_cif_job):
        output_is_as_expected(completed_cif_job)
        shadow_log_is_as_expected(the_condor)
        lock_dir_is_clean(the_lock_dir)
