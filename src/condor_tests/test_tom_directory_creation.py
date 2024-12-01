#!/usr/bin/env pytest

import htcondor2 as htcondor
import logging
import os
from pathlib import Path
import subprocess

from ornithology import *


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


TEST_CASES = {
    "file_to_new_dir": {
        "submit_commands": {
            "transfer_output_files":    "inputA",
            "transfer_output_remaps":   '"inputA=$(CLUSTER)/dirA/dirB/inputA"',
        },
    },
    "dir_to_new_dir": {
        "submit_commands": {
            "transfer_output_files":    "dirA",
            "transfer_output_remaps":   '"dirA=$(CLUSTER)/dirC/dirD/dirA"',
        },
    },
    "fileA,B_to_dirA,B": {
        "submit_commands": {
            "transfer_output_files":    "inputA, inputB",
            "transfer_output_remaps":   '"inputA=$(CLUSTER)/dirA/inputA;inputB=$(CLUSTER)/dirB/inputB"',
        },
    },
    "dirA,B_to_dirC,D": {
        "submit_commands": {
            "transfer_output_files":    "dirA, dirB",
            "transfer_output_remaps":   '"dirA=$(CLUSTER)/dirC/dirA;dirB=$(CLUSTER)/dirD/dirB"',
        },
    },
}


@action
def permissive_condor(test_dir, path_to_sleep):
    permissive_config = {
        "LIMIT_DIRECTORY_ACCESS": f"{str(test_dir)}, {str(Path(path_to_sleep).parent)}",
    }
    with Condor(test_dir / "condor", permissive_config) as condor:
        yield condor


@action
def the_job_description(test_dir, path_to_sleep):
    Path("inputA").write_text("a")
    Path("inputB").write_text("b")
    Path("dirA").mkdir(exist_ok=False)
    Path("dirB").mkdir(exist_ok=False)

    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "universe":                     "vanilla",
        "executable":                   str(path_to_sleep),
        "transfer_executable":          "yes",
        "arguments":                    "1",

        "transfer_input_files":         "inputA, inputB, dirA, dirB",
        "should_transfer_files":        "yes",

        "output":                       "$(CLUSTER)/out",
        "error":                        "$(CLUSTER)/err",
        "log":                          "log.$(CLUSTER)",
    }


@action
def the_job_handles(test_dir, permissive_condor, the_job_description):
    job_handles = {}

    for name, test_case in TEST_CASES.items():
        complete_job_description = {
            ** the_job_description,
            ** test_case["submit_commands"],
        }

        job_handle = permissive_condor.submit(
            description=complete_job_description,
            count=1,
        )

        job_handles[name] = job_handle

    yield job_handles

    for name, job_handle in job_handles.items():
        job_handle.remove()


@action(params={name: name for name in TEST_CASES})
def a_job_pair(request, the_job_handles):
    return (request.param, the_job_handles[request.param])


@action
def a_job_name(a_job_pair):
    return a_job_pair[0]


@action
def a_job_handle(a_job_pair):
    return a_job_pair[1]


@action
def completed_test_job(permissive_condor, a_job_handle):
    a_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return a_job_handle


@action
def dotdot_test_job(permissive_condor, the_job_description):
    bad_dir = "../$(CLUSTER)"

    more_job_description = {
        "transfer_output_files":    "inputA, dirB",
        "transfer_output_remaps":   f'"inputA={bad_dir}/inputA;dirB={bad_dir}/dirB"',
    }

    complete_job_description = {
        ** the_job_description,
        ** more_job_description,
    }

    job_handle = permissive_condor.submit(
        description=complete_job_description,
        count=1,
    )

    job_handle.wait(
        timeout=60,
        condition=ClusterState.all_held,
        fail_condition=ClusterState.any_terminal,
    )

    yield job_handle

    job_handle.remove()


@action
def symlink_test_job(permissive_condor, the_job_description, test_dir):
    (test_dir / "dotdot").symlink_to(test_dir.parent, True)
    bad_dir = "dotdot/$(CLUSTER)"

    more_job_description = {
        "transfer_output_files":    "inputA, dirB",
        "transfer_output_remaps":   f'"inputA={bad_dir}/inputA;dirB={bad_dir}/dirB"',
    }

    complete_job_description = {
        ** the_job_description,
        ** more_job_description,
    }

    job_handle = permissive_condor.submit(
        description=complete_job_description,
        count=1,
    )

    job_handle.wait(
        timeout=60,
        condition=ClusterState.all_held,
        fail_condition=ClusterState.any_terminal,
    )

    yield job_handle

    job_handle.remove()


class TestTransferOutputRemapsDirectoryCreation:
    def test_output_log_in_new_directory(self, completed_test_job):
        output_log = Path(str(completed_test_job.clusterid)) / "out"
        assert output_log.exists()


    def test_error_log_in_new_directory(self, completed_test_job):
        error_log = Path(str(completed_test_job.clusterid)) / "err"
        assert error_log.exists()


    def test_transfer_output_remaps_permissive(self, test_dir, completed_test_job):
        # This probably won't work in general, but with only one proc
        # per cluster and no usage of $(PROC), this is fine.
        remap_string = completed_test_job.clusterad["TransferOutputRemaps"]
        remap_list = remap_string.split(";")
        remap_pairs = [ item.split("=") for item in remap_list ]
        remaps = { item[0]: item[1] for item in remap_pairs }

        for name, target in remaps.items():
            assert (test_dir / target).exists()


    def test_transfer_output_remaps_strict(self, test_dir, dotdot_test_job):
        # This probably won't work in general, but with only one proc
        # per cluster and no usage of $(PROC), this is fine.
        remap_string = dotdot_test_job.clusterad["TransferOutputRemaps"]
        remap_list = remap_string.split(";")
        remap_pairs = [ item.split("=") for item in remap_list ]
        remaps = { item[0]: item[1] for item in remap_pairs }

        for name, target in remaps.items():
            assert not (test_dir / target).exists()

        # An old implementation actually created directories outside
        # of LIMIT_DIRECTORY_ACCESS because allow_shadow_access() can't
        # answer questions about that directories tha don't exist.  Make
        # sure that we cleaned up.
        assert not (test_dir / ".." / str(dotdot_test_job.clusterid)).exists()


    def test_transfer_output_remaps_strict(self, test_dir, symlink_test_job):
        # This probably won't work in general, but with only one proc
        # per cluster and no usage of $(PROC), this is fine.
        remap_string = symlink_test_job.clusterad["TransferOutputRemaps"]
        remap_list = remap_string.split(";")
        remap_pairs = [ item.split("=") for item in remap_list ]
        remaps = { item[0]: item[1] for item in remap_pairs }

        for name, target in remaps.items():
            assert not (test_dir / target).exists()

        # The current implementation actually creates directories outside
        # of LIMIT_DIRECTORY_ACCESS because allow_shadow_access() can't
        # answer questions about that directories tha don't exist.  Make
        # sure that we cleaned up.
        assert not (test_dir.parent / str(symlink_test_job.clusterid)).exists()
