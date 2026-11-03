#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import htcondor2

from ornithology import (
    Condor,
    action,
    ClusterState,
    format_script,
    write_file,
    JobStatus,
)


@action
def the_condor(test_dir):
    with Condor(
        local_dir = test_dir / "condor.d",
        config = {
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
        },
    ) as the_condor:
        yield the_condor


# The first two should pass, the second two should fail.
TEST_CASES = {
    "none": {
    },
    "container": {
        "container_is_common":      True,
        "container_image":          "missing-container-image-is-fine.sif",
    },
    "files": {
        "transfer_common_input":    "common-file",
    },
    "both": {
        "container_is_common":      True,
        "container_image":          "missing-container-image-is-fine.sif",
        "transfer_common_input":    "common-file",
    },
}


FAILURE_EXPECTED = {
    "none":         False,
    "container":    False,
    "files":        True,
    "both":         True,
}


@action(params={name: name for name in TEST_CASES})
def the_remote_job_description(request):
    default_job_description = {
        "universe":                 "vanilla",
        "shell":                    "cat common-file; cat input-file-$(ProcID); sleep 12",

        "transfer_input_files":     "input-file-$(ProcID)",
        "should_transfer_files":    "YES",
        "request_cpus":             4,
        "request_memory":           "128M",
        "request_disk":             "1024M",
    }
    job_description = {
        ** default_job_description,
        ** TEST_CASES[request.param]
    }
    return (request.param, job_description)


@action(params={name: name for name in TEST_CASES})
def the_parallel_job_description(request):
    default_job_description = {
        "universe":                 "parallel",
        "machine_count":            4,
        "shell":                    "cat common-file; cat input-file-$(ProcID); sleep 12",

        "transfer_input_files":     "input-file-$(ProcID)",
        "should_transfer_files":    "YES",
        "request_cpus":             4,
        "request_memory":           "128M",
        "request_disk":             "1024M",
    }
    job_description = {
        ** default_job_description,
        ** TEST_CASES[request.param]
    }
    return (request.param, job_description)


class TestCIF:

    def test_remote_submit(self, the_condor, the_remote_job_description):
        test_name = the_remote_job_description[0]
        should_except = FAILURE_EXPECTED[test_name]
        submit = htcondor2.Submit(the_remote_job_description[1])

        schedd = the_condor.get_local_schedd()

        try:
            result = schedd.submit(
                description=submit,
                count=1,
                spool=True
            )
        except htcondor2.HTCondorException as e:
            message = str(e)
            if "Remote submission is not compatible with common file transfer" in message:
                assert should_except
                return
            else:
                print(e)
                assert False
        assert not should_except


    def test_parallel_submit(self, the_condor, the_parallel_job_description):
        test_name = the_parallel_job_description[0]
        should_except = FAILURE_EXPECTED[test_name]
        submit = htcondor2.Submit(the_parallel_job_description[1])

        schedd = the_condor.get_local_schedd()
        try:
            result = schedd.submit(
                description=submit,
                count=1,
                spool=True
            )
        except htcondor2.HTCondorException as e:
            message = str(e)
            if "Common file transfer only works in vanilla or container universe" in message:
                assert should_except
                return
            else:
                print(e)
                assert False
        assert not should_except
