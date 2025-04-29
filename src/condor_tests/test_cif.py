#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    format_script,
    write_file,
    JobStatus,
)

import os
from pathlib import Path
import shutil

import htcondor2


def try_shutil_chown( * p, ** v ):
    try:
        shutil.chown( * p, ** v )
    except PermissionError as pe:
        logger.debug(pe)
    except LookupError as le:
        logger.debug(le)


# ---- test_one_cif_job -------------------------------------------------------

@action
def user_dir(test_dir):
    the_user_dir = test_dir / "user"
    the_user_dir.mkdir(exist_ok=True)
    try_shutil_chown( the_user_dir.as_posix(), user='tlmiller', group='tlmiller' )
    return the_user_dir


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
            "SHADOW_DEBUG":     "D_CATEGORY D_ZKM D_SUB_SECOND D_PID",
            "LOCK":             the_lock_dir.as_posix(),
        },
    ) as the_condor:
        yield the_condor


@action
def completed_cif_job(the_condor, path_to_sleep, user_dir):
    os.chdir(user_dir.as_posix())
    (user_dir / "input1.txt").write_text("input A\n")
    (user_dir / "input2.txt").write_text("II input\n");
    (user_dir / "input3.txt").write_text("input line 3\n" );

    job_description = {
        "universe":                 "vanilla",

        "shell":                    "cat input1.txt input2.txt input3.txt; sleep 5",
        "transfer_executable":      False,
        "should_transfer_files":    True,

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


# ---- test_many_cif_jobs -----------------------------------------------------


@action
def the_big_lock_dir(test_dir):
    return test_dir / "big.lock.d"


@action
def the_big_condor(test_dir, the_big_lock_dir):
    local_dir = test_dir / "big-condor"

    with Condor(
        submit_user='tlmiller',
        condor_user='condor',
        local_dir=local_dir,
        config={
            "SHADOW_DEBUG":     "D_CATEGORY D_ZKM D_SUB_SECOND D_PID",
            "LOCK":             the_big_lock_dir.as_posix(),
            "NUM_CPUS":         4,
        },
    ) as the_condor:
        yield the_condor


@action
def cif_jobs_script(test_dir):
    script_path = test_dir / "big_job_script"

    script_text = format_script(
    """
        #!/bin/bash

        cat big_input1.txt big_input2.txt big_input3.txt

        while true; do
            sleep 1
            if [ -e $1 ]; then
                exit 0
            fi
        done

        exit 1
    """
    )
    write_file(script_path, script_text)
    return script_path


@action
def completed_cif_jobs(the_big_condor, user_dir, cif_jobs_script):
    os.chdir(user_dir.as_posix())
    (user_dir / "big_input1.txt").write_text("input A\n")
    (user_dir / "big_input2.txt").write_text("II input\n");
    (user_dir / "big_input3.txt").write_text("input line 3\n" );

    job_description = {
        "universe":                 "vanilla",

        "executable":               cif_jobs_script.as_posix(),
        "arguments":                (user_dir / "kill-many-$(ClusterID).$(ProcID)").as_posix(),
        "transfer_executable":      True,
        "should_transfer_files":    True,

        "log":                      "cif_job.log.$(CLUSTER)",
        "output":                   "cif_job.output.$(CLUSTER).$(PROCESS)",
        "error":                    "cif_job.error.$(CLUSTER).$(PROCESS)",

        "request_cpus":             1,
        "request_memory":           1,

        "MY.CommonInputFiles":      '"big_input1.txt, big_input2.txt"',
        "transfer_input_files":     "big_input3.txt",

        "leave_in_queue":           True,
        "hold":                     True,
    }

    # Submit eight jobs on hold.
    job_handle = the_big_condor.submit(
        description=job_description,
        count=8,
    )

    # Release four of the jobs.
    jobIDs = [ f"{job_handle.clusterid}.{x}" for x in range(0,4) ]
    the_big_condor.act( htcondor2.JobAction.Release, jobIDs )

    # Wait for them to start.
    assert job_handle.wait(
        timeout=60,
        condition=ClusterState.running_exactly(4),
        fail_condition=ClusterState.any_terminal
    )

    # Release the other four jobs.
    jobIDs = [ f"{job_handle.clusterid}.{x}" for x in range(4,8) ]
    the_big_condor.act( htcondor2.JobAction.Release, jobIDs )

    # Touch the kill files on the first four jobs, causing them to finish.
    for i in range(0,4):
        (user_dir / f"kill-many-{job_handle.clusterid}.{i}").touch(exist_ok=True)

    # Wait for the last four jobs to start.
    assert job_handle.wait(
        timeout=60,
        condition=ClusterState.none_idle,
    )

    # Touch the kill files on the last four jobs, causing them to finish.
    for i in range(4,8):
        (user_dir / f"kill-many-{job_handle.clusterid}.{i}").touch(exist_ok=True)

    # Wait for the last four jobs to finish.
    assert job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
    )

    return job_handle


# ---- test_multi_cif_jobs ----------------------------------------------------


@action
def the_multi_lock_dir(test_dir):
    return test_dir / "multi.lock.d"


@action
def the_multi_condor(test_dir, the_multi_lock_dir):
    local_dir = test_dir / "multi-condor"

    with Condor(
        submit_user='tlmiller',
        condor_user='condor',
        local_dir=local_dir,
        config={
            "SHADOW_DEBUG":     "D_CATEGORY D_ZKM D_SUB_SECOND D_PID",
            "LOCK":             the_multi_lock_dir.as_posix(),
            "NUM_CPUS":         4,
        },
    ) as the_condor:
        yield the_condor


@action
def multi_job_script(test_dir):
    script_path = test_dir / "multi_job_script"

    script_text = format_script(
    """
        #!/bin/bash

        killfile=$1; shift
        cat "$@"

        while true; do
            sleep 1
            if [ -e $killfile ]; then
                exit 0
            fi
        done

        exit 1
    """
    )
    write_file(script_path, script_text)
    return script_path


@action
def completed_multi_jobs(the_multi_condor, user_dir, multi_job_script):
    os.chdir(user_dir.as_posix())

    #
    # In this test, we're validating that different CIFs stay different.
    #

    (user_dir / "multi_input1.txt").write_text("input A\n")
    (user_dir / "multi_input2.txt").write_text("II input\n");
    (user_dir / "multi_input3.txt").write_text("input line 3\n" );

    kill_file = (user_dir / "kill-multi-$(ClusterID).$(ProcID)").as_posix()
    job_description_a = {
        "universe":                 "vanilla",

        "executable":               multi_job_script.as_posix(),
        "arguments":
            f'{kill_file} multi_input1.txt multi_input2.txt multi_input3.txt',
        "transfer_executable":      True,
        "should_transfer_files":    True,

        "log":                      "multi_job.log.$(CLUSTER)",
        "output":                   "multi_job.output.$(CLUSTER).$(PROCESS)",
        "error":                    "multi_job.error.$(CLUSTER).$(PROCESS)",

        "request_cpus":             1,
        "request_memory":           1,

        "MY.CommonInputFiles":      '"multi_input1.txt, multi_input2.txt"',
        "transfer_input_files":     "multi_input3.txt",

        "leave_in_queue":           True,
        "hold":                     True,
    }


    (user_dir / "multi_input4.txt").write_text("-- input A\n")
    (user_dir / "multi_input5.txt").write_text("-- II input\n");
    (user_dir / "multi_input6.txt").write_text("-- input line 3\n" );

    job_description_b = { ** job_description_a,
        "arguments":
            f'{kill_file} multi_input4.txt multi_input5.txt multi_input6.txt',
        "MY.CommonInputFiles":      '"multi_input4.txt, multi_input5.txt"',
        "transfer_input_files":     "multi_input6.txt",
    }


    # Submit eight jobs on hold.
    job_handle_a = the_multi_condor.submit(
        description=job_description_a,
        count=4,
    )

    job_handle_b = the_multi_condor.submit(
        description=job_description_b,
        count=4,
    )


    # Release four of the jobs.
    jobIDs = [ f"{job_handle_a.clusterid}.{x}" for x in range(0,2) ]
    the_multi_condor.act( htcondor2.JobAction.Release, jobIDs )
    jobIDs = [ f"{job_handle_b.clusterid}.{x}" for x in range(0,2) ]
    the_multi_condor.act( htcondor2.JobAction.Release, jobIDs )


    # Wait for them to start.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_terminal
    )

    # Touch the kill files on the first four jobs, causing them to finish.
    for i in range(0,2):
        (user_dir / f"kill-multi-{job_handle_a.clusterid}.{i}").touch(exist_ok=True)
    for i in range(0,2):
        (user_dir / f"kill-multi-{job_handle_b.clusterid}.{i}").touch(exist_ok=True)

    # Wait for the first four jobs to finish.
    # Due to a bug in Ornithology, jobs submitted on hold but not yet
    # released are considered IDLE, which is... not helpful.
    assert job_handle_a.wait(
        timeout=60,
        condition=lambda s: s.all_status(JobStatus.IDLE, JobStatus.COMPLETED),
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=lambda s: s.all_status(JobStatus.IDLE, JobStatus.COMPLETED),
    )

    # Release the other four jobs.
    jobIDs = [ f"{job_handle_a.clusterid}.{x}" for x in range(2,4) ]
    the_multi_condor.act( htcondor2.JobAction.Release, jobIDs )
    jobIDs = [ f"{job_handle_b.clusterid}.{x}" for x in range(2,4) ]
    the_multi_condor.act( htcondor2.JobAction.Release, jobIDs )

    # Wait for the last four jobs to start.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.none_idle,
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.none_idle,
    )

    # Touch the kill files on the last four jobs, causing them to finish.
    for i in range(2,4):
        (user_dir / f"kill-multi-{job_handle_a.clusterid}.{i}").touch(exist_ok=True)
    for i in range(2,4):
        (user_dir / f"kill-multi-{job_handle_b.clusterid}.{i}").touch(exist_ok=True)

    # Wait for the last four jobs to finish.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_complete,
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.all_complete,
    )

    return job_handle_a, job_handle_b


# ---- assertion helpers ------------------------------------------------------


def output_is_as_expected(completed_cif_job, the_expected_output):
    ads = completed_cif_job.query( projection=['Out'] )

    for ad in ads:
        out = Path(ad['Out'])
        text = out.read_text()
        assert text == the_expected_output


def count_shadow_log_lines(the_condor, the_phrase):
    shadow_log = the_condor.shadow_log.open()
    lines = list(filter(
        lambda line: the_phrase in line,
        shadow_log.read(),
    ))
    return len(lines)


def shadow_log_is_as_expected(the_condor, count):
    staging_commands_sent = count_shadow_log_lines(
        the_condor, "StageCommonFiles"
    )
    assert staging_commands_sent == count

    successful_staging_commands = count_shadow_log_lines(
        the_condor, "Staging successful"
    )
    assert successful_staging_commands == count

    keyfile_touches = count_shadow_log_lines(
        the_condor, "Elected producer touch"
    )
    assert keyfile_touches == count


def lock_dir_is_clean(the_lock_dir):
    files = list(the_lock_dir.iterdir())
    for file in files:
        assert file.name == "shared_port_ad" or file.name == "InstanceLock"
    assert len(files) == 2


# ---- the tests --------------------------------------------------------------


class TestCIF:

    def test_one_cif_job(self, the_lock_dir, the_condor, completed_cif_job):
        output_is_as_expected(
            completed_cif_job, "input A\nII input\ninput line 3\n"
        )
        shadow_log_is_as_expected(the_condor, 1)
        lock_dir_is_clean(the_lock_dir)


    def test_many_cif_jobs(self, the_big_lock_dir, the_big_condor, completed_cif_jobs):
        output_is_as_expected(
            completed_cif_jobs, "input A\nII input\ninput line 3\n"
        )
        shadow_log_is_as_expected(the_big_condor, 2)
        lock_dir_is_clean(the_big_lock_dir)


    def test_multi_cif_jobs(self, the_multi_lock_dir, the_multi_condor, completed_multi_jobs):
        output_is_as_expected(
            completed_multi_jobs[0],
            "input A\nII input\ninput line 3\n"
        )
        output_is_as_expected(
            completed_multi_jobs[1],
            "-- input A\n-- II input\n-- input line 3\n"
        )
        shadow_log_is_as_expected(the_multi_condor, 4)
        lock_dir_is_clean(the_multi_lock_dir)
