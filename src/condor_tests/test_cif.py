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
import time
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
            # Set both of these to require the use COPY mapping.
            # "FORBID_HARDLINK_MAPPING":      True,
            # "FORBID_BINDMOUNT_MAPPING":     True,
            # This only matters when testing as root.
            # "STARTD_ENFORCE_DISK_LIMITS":   True,
            # "LVM_AUTO_VG_NAME":             "alpha",
            # This must be AUTO for cxfer to work when enforcing disk limits.
            "LVM_HIDE_MOUNT":               "AUTO",
            "STARTER_NESTED_SCRATCH":       True,
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
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
            # Set both of these to require the use COPY mapping.
            # "FORBID_HARDLINK_MAPPING":      True,
            # "FORBID_BINDMOUNT_MAPPING":     True,
            # This only matters when testing as root.
            # "STARTD_ENFORCE_DISK_LIMITS":   True,
            # "LVM_AUTO_VG_NAME":             "beta",
            # This must be AUTO for cxfer to work when enforcing disk limits.
            "LVM_HIDE_MOUNT":               "AUTO",
            # This is not test-specific.
            "STARTER_NESTED_SCRATCH":       False,
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":             the_big_lock_dir.as_posix(),
            "NUM_CPUS":         4,
            "STARTER_ALLOW_RUNAS_OWNER":    False,
            "SLOT1_1_USER":     "glump",
            "SLOT1_2_USER":     "sorcy",
            "SLOT1_3_USER":     "kittie",
            "SLOT1_4_USER":     "jrandom",
            "KEEP_DATA_CLAIM_IDLE": 5,
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

    #
    # The 'multi' test below verifies that sequential re-use works if
    # KEEP_DATA_CLAIM_IDLE doesn't expire, so we'll use this test to verify
    # that common files work if it does expire.
    #
    # It can take a _long_ time for the last shadow to be reaped after
    # the last job's termination event is written.
    #
    time.sleep(20)

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

    #
    # It can take a _long_ time for the last shadow to be reaped after
    # the last job's termination event is written.
    #
    time.sleep(20)

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
            # Set both of these to require the use COPY mapping.
            # "FORBID_HARDLINK_MAPPING":      True,
            # "FORBID_BINDMOUNT_MAPPING":     True,
            # This only matters when testing as root.
            # "STARTD_ENFORCE_DISK_LIMITS":   True,
            # "LVM_AUTO_VG_NAME":             "gamma",
            # This must be AUTO for cxfer to work when enforcing disk limits.
            "LVM_HIDE_MOUNT":               "AUTO",
            "STARTER_NESTED_SCRATCH":       True,
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
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


def count_shadow_log_lines(the_condor, the_phrase, the_filter = None):
    shadow_log = the_condor.shadow_log.open()
    lines = list(filter(
        lambda line: the_phrase in line,
        shadow_log.read(),
    ))
    if the_filter is not None:
        lines = [line for line in lines if the_filter(line)]
    return len(lines)


def shadow_log_is_as_expected(the_condor, count, cf_xfers, cf_waits, ignore_evictions = None):
    staging_commands_sent = count_shadow_log_lines(
        the_condor, "StageCommonFiles"
    )
    assert staging_commands_sent == count

    successful_staging_commands = count_shadow_log_lines(
        the_condor, "Staging successful"
    )
    assert successful_staging_commands == count

    job_evictions = count_shadow_log_lines(
        the_condor, "is being evicted from",
        ignore_evictions
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


def starter_log_is_as_expected(the_condor, cf_xfers, cf_waits, cf_maps):
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

    common_transfer_maps = count_starter_log_lines(
        the_condor, "Mapping common files into job's initial working"
    )
    assert common_transfer_maps == cf_maps


# ---- the tests --------------------------------------------------------------


class TestCIF:

    def test_one_cif_job(self, the_lock_dir, the_condor, completed_cif_job):
        output_is_as_expected(
            completed_cif_job, "input A\nII input\ninput line 3\n"
        )

        shadow_log_is_as_expected(the_condor,
            count=1, cf_xfers=1, cf_waits=0
        )
        error_is_as_expected(
            completed_cif_job, "fake credential information"
        )

        starter_log_is_as_expected(the_condor,
                     cf_xfers=1, cf_waits=0, cf_maps=1
        )
        assert epoch_log_has_common_ad(the_condor)


    def test_many_cif_jobs(self, the_big_lock_dir, the_big_condor, completed_cif_jobs):
        output_is_as_expected(
            completed_cif_jobs, "input A\nII input\ninput line 3\n"
        )
        shadow_log_is_as_expected(the_big_condor,
            count=2, cf_xfers=2, cf_waits=0,
            ignore_evictions=lambda l: ".-10" not in l
        )
        starter_log_is_as_expected(the_big_condor,
                     cf_xfers=2, cf_waits=0, cf_maps=8
        )

        # We also want to verify that the shadows die before we shut down
        # `the_big_condor` in this test's (implicit) clean-up phase.
        shadow_exits = count_shadow_log_lines(
            the_big_condor, "EXITING WITH STATUS"
        )
        assert(shadow_exits == 10)



    #
    # The point of this test is to verify that we handled multiple catalogs
    # correctly (that each job got the right answer).  It also verifies that
    # doing so doesn't cause any loss of staging efficiency.
    #
    def test_multi_cif_jobs(self, the_multi_lock_dir, the_multi_condor, completed_multi_jobs):
        output_is_as_expected(
            completed_multi_jobs[0],
            "input A\nII input\ninput line 3\n"
        )
        output_is_as_expected(
            completed_multi_jobs[1],
            "-- input A\n-- II input\n-- input line 3\n"
        )

        shadow_log_is_as_expected(the_multi_condor,
            count=2, cf_xfers=2, cf_waits=0
        )
        starter_log_is_as_expected(the_multi_condor,
                     cf_xfers=2, cf_waits=0, cf_maps=8
        )


    #
    # We could add a more-complicated test, where we have two sets of common
    # files each of which consists of more than one catalog.
    #
    # We could also add a further twist, where that cluster's two sets of
    # jobs have a common catalog in common, but that's not presently supported.
    #
