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
    cred_dir = local_dir / "cred.d"
    # I don't think Ornithology knows about this one yet.
    cred_dir.mkdir(parents=True, exist_ok=True)

    with Condor(
        submit_user='tlmiller',
        condor_user='condor',
        local_dir=local_dir,
        config={
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_ACCOUNTANT",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":             the_lock_dir.as_posix(),
            "DAEMON_LIST":      "$(DAEMON_LIST) CREDD",
            "SEC_CREDENTIAL_DIRECTORY_OAUTH": cred_dir.as_posix(),
            # Test for long sinful strings.
            "HOST_ALIAS":       "abcdefghijlmnopqrstuvwxyzabcdefghijlmnopqrstuvwxyz"
                              + "abcdefghijlmnopqrstuvwxyzabcdefghijlmnopqrstuvwxyz"
                              + "abcdefghijlmnopqrstuvwxyzabcdefghijlmnopqrstuvwxyz"
                              + "abcdefghijlmnopqrstuvwxyzabcdefghijlmnopqrstuvwxyz"
                              + "abcdefghijlmnopqrstuvwxyzabcdefghijlmnopqrstuvwxyz"
                              + "abcdefghijlmnopqrstuvwxyzabcdefghijlmnopqrstuvwxyz",
        },
    ) as the_condor:
        yield the_condor


@action
def completed_cif_job(the_condor, path_to_sleep, user_dir):
    os.chdir(user_dir.as_posix())

    d = user_dir / "d"
    d.mkdir()
    (d / "input1.txt").write_text("input")
    (d / "input4.txt").write_text(" A\n")

    e = user_dir / "d" / "e"
    e.mkdir()
    (e / "input2.txt").write_text("II")

    f = user_dir / "d" / "f"
    f.mkdir()
    (f / "input5.txt").write_text(" input\n")

    (user_dir / "input3.txt").write_text("input line 3\n" );

    # Make sure that credential propogation works, too.
    credential_path = user_dir / "the_credential"
    credential_path.write_text("fake credential information")
    cp = the_condor.run_command(
        ['htcondor', 'credential', 'add', 'oauth2', credential_path.as_posix()],
        timeout=60,
        echo=True,
        as_user='tlmiller',
    )
    assert cp.returncode == 0

    job_description = {
        "universe":                 "vanilla",

        "shell":                    "cat ${_CONDOR_CREDS}/the_credential.use 1>&2; cat d/input1.txt d/input4.txt d/e/input2.txt d/f/input5.txt input3.txt; sleep 5",
        "transfer_executable":      False,
        "should_transfer_files":    True,

        "log":                      "cif_job.log",
        "output":                   "cif_job.output",
        "error":                    "cif_job.error",

        "request_cpus":             1,
        "request_memory":           1,

        "MY._x_common_input_catalogs":      '"my_common_files"',
        "MY._x_catalog_my_common_files":    '"d, null://create-epoch-entry"',

        "transfer_input_files":     "input3.txt",
        "use_oauth_services":       "the_credential",

        "leave_in_queue":           True,
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=1,
    )

    assert job_handle.wait(
        timeout=200,
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
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_ACCOUNTANT",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":             the_big_lock_dir.as_posix(),
            "NUM_CPUS":         4,
            "STARTER_ALLOW_RUNAS_OWNER":    False,
            "SLOT1_1_USER":     "glump",
            "SLOT1_2_USER":     "sorcy",
            "SLOT1_3_USER":     "kittie",
            "SLOT1_4_USER":     "jrandom",
            "STARTER_NESTED_SCRATCH":   False,
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

        # Force a delay; the constant is to ensure that we would print out
        # the waiting message at least twice (if the delay remains at five
        # seconds each time).
        "MY._x_common_input_catalogs":      '"my_common_files"',
        "MY._x_catalog_my_common_files":    '"big_input1.txt, big_input2.txt, debug://sleep/15"',

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
        timeout=200,
        condition=ClusterState.running_exactly(4),
        fail_condition=ClusterState.any_terminal
    )

    # Touch the kill files on the first four jobs, causing them to finish.
    for i in range(0,4):
        (user_dir / f"kill-many-{job_handle.clusterid}.{i}").touch(exist_ok=True)

    # Wait for them to finish.
    assert job_handle.wait(
        timeout=200,
        condition=lambda self: self.status_exactly(4, JobStatus.COMPLETED),
        fail_condition=ClusterState.any_held
    )

    # This test used to overlap releasing some jobs while others were still
    # running, to make sure that "delayed" concurrent re-use worked properly.
    # We could stagger the start of either (or both) halves of the jobs,
    # but we're also using this test to verify obseverability, which means
    # we need to check for a specific number of times what jobs wait for
    # common file transfer to happen.  That leads to a race, because we don't
    # know that the common file transfer has completed by the time wait()
    # returns (which only knows about shadow start-up).
    #
    # Of course, we intend to add common files transfer events to the job
    # event log later on in this PR, so maybe using those (after checking
    # them after the fact in test_one_cif_job) would work well.

    # Release the other four jobs.
    jobIDs = [ f"{job_handle.clusterid}.{x}" for x in range(4,8) ]
    the_big_condor.act( htcondor2.JobAction.Release, jobIDs )

    # Wait for the last four jobs to start.
    assert job_handle.wait(
        timeout=200,
        condition=ClusterState.none_idle,
    )

    # Touch the kill files on the last four jobs, causing them to finish.
    for i in range(4,8):
        (user_dir / f"kill-many-{job_handle.clusterid}.{i}").touch(exist_ok=True)

    # Wait for the last four jobs to finish.
    assert job_handle.wait(
        timeout=200,
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
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_ACCOUNTANT",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":             the_multi_lock_dir.as_posix(),
            "NUM_CPUS":         4,
            "STARTER_NESTED_SCRATCH":   True,
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

        "MY._x_common_input_catalogs":      '"my_common_files"',
        "MY._x_catalog_my_common_files":    '"multi_input1.txt, multi_input2.txt"',

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
        "MY._x_common_input_catalogs":      '"my_common_files"',
        "MY._x_catalog_my_common_files":    '"multi_input4.txt, multi_input5.txt"',
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
        timeout=200,
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_terminal
    )
    assert job_handle_b.wait(
        timeout=200,
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
        timeout=200,
        condition=lambda s: s.all_status(JobStatus.IDLE, JobStatus.COMPLETED),
    )
    assert job_handle_b.wait(
        timeout=200,
        condition=lambda s: s.all_status(JobStatus.IDLE, JobStatus.COMPLETED),
    )

    # Release the other four jobs.
    jobIDs = [ f"{job_handle_a.clusterid}.{x}" for x in range(2,4) ]
    the_multi_condor.act( htcondor2.JobAction.Release, jobIDs )
    jobIDs = [ f"{job_handle_b.clusterid}.{x}" for x in range(2,4) ]
    the_multi_condor.act( htcondor2.JobAction.Release, jobIDs )

    # Wait for the last four jobs to start.
    assert job_handle_a.wait(
        timeout=200,
        condition=ClusterState.none_idle,
    )
    assert job_handle_b.wait(
        timeout=200,
        condition=ClusterState.none_idle,
    )

    # Touch the kill files on the last four jobs, causing them to finish.
    for i in range(2,4):
        (user_dir / f"kill-multi-{job_handle_a.clusterid}.{i}").touch(exist_ok=True)
    for i in range(2,4):
        (user_dir / f"kill-multi-{job_handle_b.clusterid}.{i}").touch(exist_ok=True)

    # Wait for the last four jobs to finish.
    assert job_handle_a.wait(
        timeout=200,
        condition=ClusterState.all_complete,
    )
    assert job_handle_b.wait(
        timeout=200,
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


def error_is_as_expected(completed_cif_job, the_expected_error):
    ads = completed_cif_job.query( projection=['Err'] )

    for ad in ads:
        err = Path(ad['Err'])
        text = err.read_text()
        assert text == the_expected_error


def count_shadow_log_lines(the_condor, the_phrase):
    shadow_log = the_condor.shadow_log.open()
    lines = list(filter(
        lambda line: the_phrase in line,
        shadow_log.read(),
    ))
    return len(lines)


def shadow_log_is_as_expected(the_condor, count, cf_xfers, cf_waits):
    staging_commands_sent = count_shadow_log_lines(
        the_condor, "StageCommonFiles"
    )
    assert staging_commands_sent == count

    successful_staging_commands = count_shadow_log_lines(
        the_condor, "Staging successful"
    )
    assert successful_staging_commands == count

    keyfile_touches = count_shadow_log_lines(
        the_condor, "Producer elected"
    )
    assert keyfile_touches == count

    job_evictions = count_shadow_log_lines(
        the_condor, "is being evicted from"
    )
    assert job_evictions == 0

    common_transfer_begins = count_shadow_log_lines(
        the_condor, "Starting common files transfer."
    )
    assert common_transfer_begins == cf_xfers

    common_transfer_ends = count_shadow_log_lines(
        the_condor, "Finished common files transfer: success."
    )
    assert common_transfer_ends == cf_xfers

    if cf_waits is not None:
        common_transfer_waits = count_shadow_log_lines(
            the_condor, "Waiting for common files to be transferred"
        )
        assert common_transfer_waits == cf_waits


def lock_dir_is_clean(the_lock_dir):
    syndicate_dir = the_lock_dir / "syndicate"

    files = list(syndicate_dir.iterdir())
    assert len(files) == 0


def epoch_log_has_common_ad(the_condor):
    with the_condor.use_config():
        JOB_EPOCH_HISTORY = htcondor2.param["JOB_EPOCH_HISTORY"]

    epoch_log = Path(JOB_EPOCH_HISTORY)
    for line in epoch_log.read_text().split('\n'):
        if '*** COMMON ClusterId' in line:
            return True

    return False


def count_starter_log_lines(the_condor, the_phrase):
    with the_condor.use_config():
        LOG = htcondor2.param["LOG"]

    count = 0
    for log in Path(LOG).iterdir():
        if log.name.startswith("StarterLog.slot1_"):
            for line in log.read_text().split("\n"):
                if the_phrase in line:
                    count = count + 1

    return count


def starter_log_is_as_expected(the_condor, cf_xfers, cf_waits):
    common_transfer_begins = count_starter_log_lines(
        the_condor, "Starting common files transfer."
    )
    assert common_transfer_begins == cf_xfers

    common_transfer_ends = count_starter_log_lines(
        the_condor, "Finished common files transfer: success."
    )
    assert common_transfer_ends == cf_xfers

    common_transfer_waits = count_starter_log_lines(
        the_condor, "Waiting for common files to be transferred"
    )
    assert common_transfer_waits == cf_waits


# ---- the tests --------------------------------------------------------------


class TestCIF:

    def test_one_cif_job(self, the_lock_dir, the_condor, completed_cif_job):
        output_is_as_expected(
            completed_cif_job, "input A\nII input\ninput line 3\n"
        )
        shadow_log_is_as_expected(the_condor, 1, 1, 0)
        lock_dir_is_clean(the_lock_dir)
        error_is_as_expected(
            completed_cif_job, "fake credential information"
        )

        starter_log_is_as_expected(the_condor, 1, 0)
        assert epoch_log_has_common_ad(the_condor)


    def test_many_cif_jobs(self, the_big_lock_dir, the_big_condor, completed_cif_jobs):
        output_is_as_expected(
            completed_cif_jobs, "input A\nII input\ninput line 3\n"
        )
        shadow_log_is_as_expected(the_big_condor, 2, 2, 6)
        lock_dir_is_clean(the_big_lock_dir)
        starter_log_is_as_expected(the_big_condor, 2, 6)


    def test_multi_cif_jobs(self, the_multi_lock_dir, the_multi_condor, completed_multi_jobs):
        output_is_as_expected(
            completed_multi_jobs[0],
            "input A\nII input\ninput line 3\n"
        )
        output_is_as_expected(
            completed_multi_jobs[1],
            "-- input A\n-- II input\n-- input line 3\n"
        )
        # This test doesn't include the delay necessary to make sure that
        # transferring common files takes longer than starting / scheduling
        # a the next shadow, so just ignore wait lines completely.
        shadow_log_is_as_expected(the_multi_condor, 4, 4, None)
        lock_dir_is_clean(the_multi_lock_dir)
