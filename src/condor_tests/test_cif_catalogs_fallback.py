#!/usr/bin/env pytest

import pytest
import subprocess

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


#
# This is based on `test_cif.py`; these test aren't included there because
# (a) that test already takes close to two minutes to run, and
# (b) the helper functions in this test will have to be a little different.
#

#
# The basic idea is similar: we submit some jobs which stitch together the
# input files to make an output file with known contents.  For these tests,
# the jobs will `cat` _all_ of the input files: this not only confirms that
# we got the _correct_ input files, but that we did _not_ get any of the
# others.  (Unless they're empty, which we'll check for with an `ls` as part
# of the output, I guess.)
#
# In particular, we'll submit two sets of jobs: set 1 will have catalogs A
# and B, and set 2 will have catalogs A and C.  The catalogs will have
# disjoint file membership, and each file will have unique contents.
#
# As in `test_cif.py`, we'll also be verifying that we transferred the
# catalogs the correct number of times.  This leads to two test cases
# for the preceeding scenario: one with two invocations of condor_submit,
# and one with one invocation of DAGMan.  In the former, there must be
# four transfers: A twice (once for each cluster) and B and C once each
# (once for each cluster).  In the latter, A is only transferred once.
#

#
# It remains to be seen how we can test the requirement that catalogs
# named by different submmiters (the Owner attribute, not the misnamed
# Submitter attribute that's actually for accounting) are different.
#

@action
def the_cs_local_dir(test_dir):
    return test_dir / "cs.d"


@action
def the_cs_user_dir(the_cs_local_dir):
    the_cs_user_dir = the_cs_local_dir / "user.d"
    the_cs_user_dir.mkdir(exist_ok=True)
    return the_cs_user_dir


@action
def the_cs_lock_dir(the_cs_local_dir):
    return the_cs_local_dir / "lock.d"


@action
def the_cs_condor(the_cs_local_dir, the_cs_lock_dir):
    with Condor(
        local_dir=the_cs_local_dir,
        config={
            "FORBID_COMMON_FILE_TRANSFER":          "TRUE",

            "STARTER_DEBUG":            "D_CATEGORY D_SUB_SECOND D_PID D_ACCOUNTANT",
            "SHADOW_DEBUG":             "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":                     the_cs_lock_dir.as_posix(),
            "NUM_CPUS":                 4,
            "STARTER_NESTED_SCRATCH":   True,
        },
    ) as the_cs_condor:
        yield the_cs_condor


@action
def the_cs_job_script(test_dir):
    script_path = test_dir / "cs_job_script"

    script_text = format_script(
    """
        #!/bin/sh

        ls *.txt
        cat *.txt

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
def completed_cs_jobs(the_cs_condor, the_cs_user_dir, the_cs_job_script):
    os.chdir(the_cs_user_dir)

    # We did all the complicated nested-directory stuff
    # in `test_cif.py`, so this is just a pile of files.
    for f in ("A1", "A2", "B1", "B2", "C1", "C2"):
        (the_cs_user_dir / f"{f}.txt").write_text(f"{f}\n")

    kill_file = (the_cs_user_dir / "kill-cs-$(ClusterID).$(ProcID)")
    job_description = {
        "universe":                 "vanilla",
        "executable":               the_cs_job_script.as_posix(),
        "arguments":                kill_file.as_posix(),

        "log":                      "cs_job.log.$(CLUSTER)",
        "output":                   "cs_job.output.$(CLUSTER).$(PROCESS)",
        "error":                    "cs_job.error.$(CLUSTER).$(PROCESS)",

        "request_cpus":             1,
        "request_memory":           1,

        "MY._x_catalog_A":                  '"A1.txt, A2.txt"',

        "should_transfer_files":    True,

        "leave_in_queue":           True,
    }

    job_description_a = {
        ** job_description,
        "MY._x_common_input_catalogs":      '"A, B"',
        "MY._x_catalog_B":                  '"B1.txt, B2.txt"',
    }

    job_description_b = {
        ** job_description,
        "MY._x_common_input_catalogs":      '"A, C"',
        "MY._x_catalog_C":                  '"C1.txt, C2.txt"',
    }


    # Submit two jobs of each type.
    job_handle_a = the_cs_condor.submit(
        description=job_description_a,
        count=2
    )

    job_handle_b = the_cs_condor.submit(
        description=job_description_b,
        count=2
    )


    # Wait for them all to start.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_terminal
    )


    # Touch their kill files.
    (the_cs_user_dir / f"kill-cs-{job_handle_a.clusterid}.0").touch(exist_ok=True)
    (the_cs_user_dir / f"kill-cs-{job_handle_a.clusterid}.1").touch(exist_ok=True)
    (the_cs_user_dir / f"kill-cs-{job_handle_b.clusterid}.0").touch(exist_ok=True)
    (the_cs_user_dir / f"kill-cs-{job_handle_b.clusterid}.1").touch(exist_ok=True)


    # Wait for them to finish.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.all_terminal
    )


    return job_handle_a, job_handle_b


# ---- (fake) DAGMan ------------------------------------------------------


@action
def the_dagman_local_dir(test_dir):
    return test_dir / "dm.d"


@action
def the_dagman_user_dir(the_dagman_local_dir):
    the_dagman_user_dir = the_dagman_local_dir / "user.d"
    the_dagman_user_dir.mkdir(exist_ok=True)
    return the_dagman_user_dir


@action
def the_dagman_lock_dir(the_dagman_local_dir):
    return the_dagman_local_dir / "lock.d"


@action
def the_dagman_condor(the_dagman_local_dir, the_dagman_lock_dir):
    with Condor(
        local_dir=the_dagman_local_dir,
        config={
            "FORBID_COMMON_FILE_TRANSFER":          "TRUE",

            "STARTER_DEBUG":            "D_CATEGORY D_SUB_SECOND D_PID D_ACCOUNTANT",
            "SHADOW_DEBUG":             "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":                     the_dagman_lock_dir.as_posix(),
            "NUM_CPUS":                 4,
            "STARTER_NESTED_SCRATCH":   True,
        },
    ) as the_dagman_condor:
        yield the_dagman_condor


@action
def completed_dagman_jobs(the_dagman_condor, the_dagman_user_dir, the_cs_job_script):
    os.chdir(the_dagman_user_dir)

    # We did all the complicated nested-directory stuff
    # in `test_cif.py`, so this is just a pile of files.
    for f in ("A1", "A2", "B1", "B2", "C1", "C2"):
        (the_dagman_user_dir / f"{f}.txt").write_text(f"{f}\n")

    kill_file = (the_dagman_user_dir / "kill-dm-$(ClusterID).$(ProcID)")
    job_description = {
        "MY.DAGManJobID":           "20",

        "universe":                 "vanilla",
        "executable":               the_cs_job_script.as_posix(),
        "arguments":                kill_file.as_posix(),

        "log":                      "cs_job.log.$(CLUSTER)",
        "output":                   "cs_job.output.$(CLUSTER).$(PROCESS)",
        "error":                    "cs_job.error.$(CLUSTER).$(PROCESS)",

        "request_cpus":             1,
        "request_memory":           1,

        "MY._x_catalog_A":          '"A1.txt, A2.txt"',

        "should_transfer_files":    True,

        "leave_in_queue":           True,
    }

    # FIXME: We should test that:
    #
    # 1.  A, B and A, C with *B != *C produce different output.
    # 2.  A, B and A, B with *B != *C produce different output.
    # 3.  A, B and A, C with *B == *C produce the same output (but
    #     transfer common files twice).
    # 4.  A, B and A, B with *B == *B produce the same output (but
    #     transer common files once).
    job_description_a = {
        ** job_description,
        "MY._x_common_input_catalogs":      '"A, B"',
        "MY._x_catalog_B":                  '"B1.txt, B2.txt"',
    }

    job_description_b = {
        ** job_description,
        "MY._x_common_input_catalogs":      '"A, B"',
        "MY._x_catalog_B":                  '"C1.txt, C2.txt"',
    }


    # Submit two jobs of each type.
    job_handle_a = the_dagman_condor.submit(
        description=job_description_a,
        count=2
    )

    job_handle_b = the_dagman_condor.submit(
        description=job_description_b,
        count=2
    )


    # Wait for them all to start.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_terminal
    )


    # Touch their kill files.
    (the_dagman_user_dir / f"kill-dm-{job_handle_a.clusterid}.0").touch(exist_ok=True)
    (the_dagman_user_dir / f"kill-dm-{job_handle_a.clusterid}.1").touch(exist_ok=True)
    (the_dagman_user_dir / f"kill-dm-{job_handle_b.clusterid}.0").touch(exist_ok=True)
    (the_dagman_user_dir / f"kill-dm-{job_handle_b.clusterid}.1").touch(exist_ok=True)


    # Wait for them to finish.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.all_terminal
    )


    return job_handle_a, job_handle_b


# ---- common containers ------------------------------------------------------


@action
def the_container_image(test_dir, pytestconfig):
    # This is a gross hack.
    ctest_path = test_dir / ".." / "busybox.sif"
    if ctest_path.exists():
        return ctest_path
    else:
        return Path(pytestconfig.rootdir) / "busybox.sif"


@action
def the_container_local_dir(test_dir):
    return test_dir / "cc.d"


@action
def the_container_user_dir(the_container_local_dir):
    the_container_user_dir = the_container_local_dir / "user.d"
    the_container_user_dir.mkdir(exist_ok=True)
    return the_container_user_dir


@action
def the_container_kill_dir(test_dir):
    the_container_kill_dir = test_dir / "kill.d"
    the_container_kill_dir.mkdir(exist_ok=True)
    return the_container_kill_dir


@action
def the_container_lock_dir(the_container_local_dir):
    return the_container_local_dir / "lock.d"


@action
def the_container_condor(the_container_local_dir, the_container_lock_dir, the_container_kill_dir):
    with Condor(
        local_dir=the_container_local_dir,
        config={
            "FORBID_COMMON_FILE_TRANSFER":          "TRUE",

            "STARTER_DEBUG":            "D_CATEGORY D_SUB_SECOND D_PID D_ACCOUNTANT",
            "SHADOW_DEBUG":             "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "LOCK":                     the_container_lock_dir.as_posix(),
            "NUM_CPUS":                 4,
            "STARTER_NESTED_SCRATCH":   True,
            "SINGULARITY_TEST_SANDBOX_TIMEOUT":              "8",
            "SINGULARITY":              "/usr/bin/singularity",
            "SINGULARITY_BIND_EXPR":    f'"{the_container_kill_dir.as_posix()}:{the_container_kill_dir.as_posix()}"',
            "CONTAINER_IMAGES_COMMON_BY_DEFAULT":   True,
        },
    ) as the_container_condor:
        yield the_container_condor


@action
def completed_container_jobs(the_container_condor, the_container_user_dir, the_cs_job_script, the_container_image, the_container_kill_dir):
    os.chdir(the_container_user_dir)

    # We did all the complicated nested-directory stuff
    # in `test_cif.py`, so this is just a pile of files.
    for f in ("A1", "A2"):
        (the_container_user_dir / f"{f}.txt").write_text(f"{f}\n")

    kill_file = the_container_kill_dir / "kill-cc-$(ClusterID).$(ProcID)"
    job_description = {
        "universe":                 "vanilla",
        "executable":               the_cs_job_script.as_posix(),
        "arguments":                kill_file.as_posix(),
        "container_image":          the_container_image.as_posix(),
        "transfer_executable":      True,

        "log":                      "cc_job.log.$(CLUSTER)",
        "output":                   "cc_job.output.$(CLUSTER).$(PROCESS)",
        "error":                    "cc_job.error.$(CLUSTER).$(PROCESS)",

        "request_cpus":             1,
        "request_memory":           1,

        "MY.CommonInputFiles":      '"A1.txt, A2.txt"',

        "should_transfer_files":    True,

        "leave_in_queue":           True,
    }

    job_description_a = {
        ** job_description,
    }

    job_description_b = {
        ** job_description,
        "container_is_common": "False",
    }


    # Submit two jobs of each type.
    job_handle_a = the_container_condor.submit(
        description=job_description_a,
        count=2
    )

    job_handle_b = the_container_condor.submit(
        description=job_description_b,
        count=2
    )


    # Wait for them all to start.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.running_exactly(2),
        fail_condition=ClusterState.any_terminal
    )


    # Touch their kill files.
    (the_container_kill_dir / f"kill-cc-{job_handle_a.clusterid}.0").touch(exist_ok=True)
    (the_container_kill_dir / f"kill-cc-{job_handle_a.clusterid}.1").touch(exist_ok=True)
    (the_container_kill_dir / f"kill-cc-{job_handle_b.clusterid}.0").touch(exist_ok=True)
    (the_container_kill_dir / f"kill-cc-{job_handle_b.clusterid}.1").touch(exist_ok=True)


    # Wait for them to finish.
    assert job_handle_a.wait(
        timeout=60,
        condition=ClusterState.all_terminal
    )
    assert job_handle_b.wait(
        timeout=60,
        condition=ClusterState.all_terminal
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
        the_condor, "Elected producer touch"
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

    if syndicate_dir.exists():
        files = list(syndicate_dir.iterdir())
        assert len(files) == 0


# ---- Singularity checks -----------------------------------------------------
# All stolen from `test_singularity_sif.py`, and which should probably be made
# Ornithology fixtures.

def SingularityIsWorthy():
    result = subprocess.run("singularity --version", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode('utf-8')

    logger.debug(output)
    if "apptainer" in output:
        return True

    if "3." in output:
        return True

    return False


def SingularityIsWorking():
    result = subprocess.run("singularity exec -B/bin:/bin -B/lib:/lib -B/lib64:/lib64 -B/usr:/usr busybox.sif /bin/ls /", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode('utf-8')

    logger.debug(output)

    if result.returncode == 0:
        return True
    else:
        return False


# For the test to work, we need user namespaces to be working
# and enough of them.  This is a race, but better to try
# to test first.
def UserNamespacesFunctional():
    result = subprocess.run(["unshare", "-U", "/bin/sh", "-c", "exit 7"])
    if result.returncode == 7:
        print("unshare seems to work correctly, proceeding with test\n")
        return True
    else:
        print("unshare command failed, test cannot work, skipping test\n")
        return False


# ---- Tests ------------------------------------------------------------------


class TestCIFCatalogs:

    def test_condor_submit(self, the_cs_lock_dir, the_cs_condor, completed_cs_jobs):
        output_is_as_expected(
            completed_cs_jobs[0],
            "A1.txt\nA2.txt\nB1.txt\nB2.txt\nA1\nA2\nB1\nB2\n"
        )
        output_is_as_expected(
            completed_cs_jobs[1],
            "A1.txt\nA2.txt\nC1.txt\nC2.txt\nA1\nA2\nC1\nC2\n"
        )
        # Specifically, A should be transferred twice, B once, and C once.
        # If we later care about how many transfers waited, see `test_cif.py`.
        shadow_log_is_as_expected(the_cs_condor, 0, 0, None)
        lock_dir_is_clean(the_cs_lock_dir)


    def test_dagman(self, the_dagman_lock_dir, the_dagman_condor, completed_dagman_jobs):
        output_is_as_expected(
            completed_dagman_jobs[0],
            "A1.txt\nA2.txt\nB1.txt\nB2.txt\nA1\nA2\nB1\nB2\n"
        )
        output_is_as_expected(
            completed_dagman_jobs[1],
            "A1.txt\nA2.txt\nC1.txt\nC2.txt\nA1\nA2\nC1\nC2\n"
        )
        # Specifically, A should be transferred once, B once, and C once.
        # If we later care about how many transfers waited, see `test_cif.py`.
        shadow_log_is_as_expected(the_dagman_condor, 0, 0, None)
        lock_dir_is_clean(the_dagman_lock_dir)


    @pytest.mark.skipif(not SingularityIsWorthy(), reason="No worthy Singularity/Apptainer found")
    @pytest.mark.skipif(not UserNamespacesFunctional(), reason="User namespaces not working -- some limit hit?")
    @pytest.mark.skipif(not SingularityIsWorking(), reason="Singularity doesn't seem to be working")
    def test_container(self, the_container_lock_dir, the_container_condor, completed_container_jobs):
        output_is_as_expected(
            completed_container_jobs[0],
            "A1.txt\nA2.txt\nA1\nA2\n"
        )
        output_is_as_expected(
            completed_container_jobs[1],
            "A1.txt\nA2.txt\nA1\nA2\n"
        )
        # Specifically, the container should be transferred once,
        # and A should be transferred twice.  (The container will be
        # transferred three times, but the last two won't be common
        # transfers, which is what we're counting here.)
        shadow_log_is_as_expected(the_container_condor, 0, 0, None)
        lock_dir_is_clean(the_container_lock_dir)
