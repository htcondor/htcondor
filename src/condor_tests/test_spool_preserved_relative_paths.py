#!/usr/bin/env pytest

import htcondor2

from ornithology import (
    action,
    ClusterState,
    ClusterHandle,
)

from pathlib import Path
import platform

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
    # File lists are comma-separated; names may contain spaces (HTCONDOR-4103).
    Path("input with space").touch()
    Path("dir with space").mkdir()
    Path("dir with space/file with space").touch()
    return "input_1, inputs/, input with space, dir with space/file with space"

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


@action
def the_spool_dir(the_condor):
    with the_condor.use_config():
        return Path(htcondor2.param['SPOOL'])


class TestSpoolPreservedRelativePaths:

    def test_spooled_input_list(self, the_condor, the_job_handle):
        schedd = the_condor.get_local_schedd()
        ads = schedd.query(
            constraint=f"ClusterID == {the_job_handle.clusterid}",
            projection=["TransferInput", "SUBMIT_TransferInput"],
        )
        assert len(ads) == 1
        # At submit, directories with a trailing slash are expanded; then,
        # after spooling, the schedd rewrites each entry to its basename.
        # Neither step may split on the spaces in the file names.
        submit_list = ads[0]["SUBMIT_TransferInput"].split(",")
        spool_list = ads[0]["TransferInput"].split(",")
        if platform.system() == "Windows":
            # On Windows, submit only treats a trailing backslash as a
            # directory to expand, so "inputs/" is passed through as-is
            # and expanded later, when spooled.  Ignore that entry.
            submit_list = [f for f in submit_list if not f.startswith("inputs")]
            spool_list = [f for f in spool_list if not f.startswith("inputs")]
            assert submit_list == [
                "input_1", "input with space", "dir with space/file with space",
            ]
            assert spool_list == [
                "input_1", "input with space", "file with space",
            ]
        else:
            assert submit_list == [
                "input_1", "inputs/inputs_2", "input with space",
                "dir with space/file with space",
            ]
            assert spool_list == [
                "input_1", "inputs_2", "input with space", "file with space",
            ]

    def test_job_completion(self, the_job_handle, the_spool_dir):
        assert the_job_handle.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=80,
            verbose=True,
        )

        the_zero_dir = the_spool_dir / "0"
        assert not the_zero_dir.exists()
