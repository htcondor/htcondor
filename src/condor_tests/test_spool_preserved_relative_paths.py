#!/usr/bin/env pytest

import htcondor2

from ornithology import (
    action,
    ClusterState,
    ClusterHandle,
)

from pathlib import Path

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(default_condor):
    return default_condor


@action
def the_inputs():
    Path("input_1").touch()
    Path("inputs").mkdir()
    Path("inputs/inputs_2").touch()
    return "input_1, inputs/"

@action
def the_job_handle(the_condor, path_to_sleep, test_dir, the_inputs):
    the_description = htcondor2.Submit({
        "executable":               path_to_sleep,
        "arguments":                "1",
        "log":                      (test_dir / "the_job.log").as_posix(),
        "transfer_input_files":     the_inputs,
        "should_transfer_files":    "YES",
    })

    schedd = the_condor.get_local_schedd()
    sr = schedd.submit(the_description, 1, spool=True)
    schedd.spool(sr)

    # This is a hack, although it duplicates what the_condor.submit() would
    # do if that function supported spooling.
    ch = ClusterHandle(the_condor, sr)
    # This is a really egregious hack because the EventLog class (and company)
    # don't understand that a log file with a relative path may mean "in SPOOL".
    ch.event_log._event_log_path = Path(the_condor.spool_dir / f"{sr.cluster()}" / "0" / f"cluster{sr.cluster()}.proc0.subproc0" / "the_job.log")

    return ch


class TestSpoolPreservedRelativePaths:

    def test_job_completion(self, the_job_handle):
        assert the_job_handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=20,
            verbose=True,
        )
