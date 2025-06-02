#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    action,
    Condor,
    ClusterState,
    DaemonLog,
)


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "STARTD_DEBUG":     "D_SUB_SECOND D_CATEGORY D_FULLDEBUG",
            "SHADOW_DEBUG":     "D_SUB_SECOND D_CATEGORY D_FULLDEBUG",
            "STARTER_DEBUG":    "D_SUB_SECOND D_CATEGORY D_FULLDEBUG",
        },
    ) as condor:
        yield condor


@action
def the_job_description(test_dir):
    print("the_job_description(): enter")
    (test_dir / "input.txt").write_text("aBcDeF 98765")

    return {
        "LeaveJobInQueue":              True,
        "should_transfer_files":        True,
        "when_to_transfer_output":      "ON_EXIT",
        "transfer_input_files":         "input.txt",
        "transfer_output_files":        "output.txt",

        "output":                       test_dir / "the_job.out",
        "error":                        test_dir / "the_job.err",
        "log":                          test_dir / "the_job.log",

        "shell":                        "cat input.txt"
    }


@action
def the_job_handle(the_condor, the_job_description):
    print("the_job_handle(): enter")
    job_handle = the_condor.submit(
        description=the_job_description,
        count=1,
    )

    assert job_handle.wait(
        timeout=60,
        condition = ClusterState.all_held,
    )

    yield job_handle

    job_handle.remove()


class TestOutputFailure:

    def test_shadow_log(self, the_job_handle, the_condor):
        shadow_log = the_condor.shadow_log.open()
        lines = list(filter(
            lambda line: 'condor_read(): timeout reading' in line,
            shadow_log.read(),
        ))
        assert len(lines) == 0


    def test_starter_log(self, the_job_handle, the_condor, test_dir):
        # See test_guidance_commands for how to make this reliable
        # in the presence of multiple jobs.
        starter_log_path= test_dir / "condor" / "log" / f"StarterLog.slot1_1"
        starter_log = DaemonLog(starter_log_path).open()

        lines = list(filter(
            lambda line: 'Connection to shadow may be lost' in line,
            starter_log.read(),
        ))
        assert len(lines) == 0
