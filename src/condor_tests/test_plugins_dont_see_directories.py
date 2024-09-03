#!/usr/bin/env pytest

import os
import time
import classad2

from pathlib import Path

from ornithology import (
    action,
    write_file,
    format_script,
    Condor,
    ClusterState,
    daemons,
)


import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir = local_dir,
        config = {
            # Strictly speaking, the relevant dprintf() should be
            # duplicated into D_TEST, as well, instead.
            'STARTER_DEBUG':    'D_CATEGORY D_FULLDEBUG',
        },
    ) as the_condor:
        yield the_condor


@action
def the_test_script(test_dir):
    test_script_path = test_dir / "test_script.sh"

    # This shouldn't be hard to conver to Python,
    # which would let us run this test on Windows.
    script = """
        #!/bin/bash
        mkdir -p 1/2/3/4
        touch    1/2/3/4/_file0
        mkdir -p 1/2/a/b
        touch    1/2/a/_file1
        touch    1/2/a/_file2
        touch    1/2/a/b/_file3
        touch    1/2/a/b/_file4
        mkdir -p 1/c/5
        touch    1/c/5/_file5
        mkdir -p 6
        exit 0
    """

    write_file(test_script_path, format_script(script))
    return test_script_path


@action
def the_completed_test_job(the_condor, test_dir, the_test_script, path_to_null_plugin):
    description = {
        "executable":               str(the_test_script),
        "should_transfer_files":    "yes",
        "transfer_executable":      "yes",
        "log":                      "log",
        "output":                   "output",
        "error":                    "error",
        "transfer_output_files":    "1",
        "output_destination":       f"null://{test_dir}",
        "transfer_plugins":         f"null={path_to_null_plugin}",
    }

    the_test_job = the_condor.submit(
        description = description,
        count = 1
    )

    the_test_job.wait(
        verbose = True,
        timeout = 60,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    yield the_test_job

    the_test_job.remove()


class TestPluginsDontSeeDirectories:

    # Theoretically, this could find the wrong log entry, and as such, it
    # it would be better to rewrite the test to use the local plug-in and
    # have it transfer its own input file to someplace we can examine after
    # the test job completes.  But we should really make the local plugin
    # a fixture if we do that, and then change the other tests using it, etc.
    #
    # Also, a bunch of this should be moved into its own fixture because
    # it's actually a set-up failure.
    def test_no_directories_in_input(self,
        the_completed_test_job, the_condor
    ):
        serializedNewClassAds = None
        starter_log_path = the_condor.log_dir / "StarterLog.slot1_1"
        with open(starter_log_path, "r") as log:
            for line in log.readlines():
                if "FILETRANSFER: with plugin input file" in line:
                    left_apo = line.find("'")
                    assert left_apo != -1
                    serializedNewClassAds = line[left_apo + 1:-2]
        assert serializedNewClassAds is not None
        print(serializedNewClassAds)

        file_count = 0
        for ad in classad2.parseAds(serializedNewClassAds, classad2.ParserType.New):
            path = ad["LocalFileName"]
            assert not Path(path).is_dir()
            logger.debug(f"Found non-directory '{path}'")
            file_count += 1
        assert(file_count == 6)
