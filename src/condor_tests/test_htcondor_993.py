#!/usr/bin/env pytest

#
# The problem prompting HTCONDOR-993 was that a sufficiently-deep and
# sufficiently-populated directory being transferred with
# preserve_relative_paths set would fail, with the shadow spending longer
# pruning duplicate directory-creation entries than the startd was willing
# to wait.  Rather than come up with an efficient way to remove duplicate
# entries, it seemed better to avoid creating duplicate entries in the
# first place.
#
# So the first test is to make sure that preserve_relative_paths still
# works; this is done by test_htcondor_809.py, although the reviewer
# may decide that we need a test with a more-complicated directory
# structure.
#
# The second test is to make sure that we don't end up sending duplicate
# paths.  For that, in practice, we need to log what we're sending.
#

# There's some duplication of the tree structure we're testing in
# this file.  If we really wanted to test a bunch of different tree
# structures, we could generate the on-disk structure, the submit file,
# and the test assertions from the same nested dictionary structure
# (generated as a fixture, of course).

import re
import os
import logging
from pathlib import Path

import htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config
def condor_config():
    config = {
        "TEST_HTCONDOR_933": "TRUE",
    }
    raw_config = None
    return {"config": config, "raw_config": raw_config}


@standup
def debug_condor(test_dir, condor_config):
    with Condor(test_dir / "condor", **condor_config) as condor:
        yield condor


@action
def completed_job(debug_condor, path_to_sleep):
    # Create the input sandbox that we'll be preserving.
    os.mkdir("input")
    os.mkdir("input/a")
    os.mkdir("input/a/AA")
    Path("input/a/AA/file-1").touch()
    Path("input/a/AA/not-input-1").touch()
    os.mkdir("input/b")
    os.mkdir("input/b/BB")
    Path("input/b/BB/file-2").touch()
    Path("input/b/BB/file-3").touch()
    Path("input/b/BB/not-input-2").touch()

    # Submit a job that preserves the relative paths of a few of those files.
    job_parameters = {
        "arguments": 1,
        "log": "job.log",
        "universe": "vanilla",
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "preserve_relative_paths": "true",
        "transfer_input_files": "input/a/AA/file-1, input/b/BB/file-2, input/b/BB/file-3",
    }
    job = debug_condor.submit(job_parameters, count=1)

    # Wait for that job to finish.
    assert job.wait(
        condition=ClusterState.all_complete,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )

    return job


@action
def path_cache(debug_condor, completed_job):
    paths = []
    pattern = re.compile(r"path cache includes: '(.*)'$")
    for line in debug_condor.shadow_log.open().read():
        match = pattern.search(line.message)
        if match:
            paths.append(match.group(1))
    return paths


@action
def directory_list(debug_condor, completed_job):
    paths = []
    pattern = re.compile(r"directory list includes: '(.*)'$")
    for line in debug_condor.shadow_log.open().read():
        match = pattern.search(line.message)
        if match:
            paths.append(match.group(1))
    return paths


class TestPreserveRelativePaths:
    correct_list = [
        'input',
        'input/a', 'input/a/AA',
        'input/b', 'input/b/BB',
    ]


    def test_only_directories_in_path_cache(self, path_cache):
        assert sorted(path_cache) == sorted(self.correct_list)


    def test_no_duplicate_paths_in_directory_list(self, directory_list):
        assert sorted(directory_list) == sorted(self.correct_list)


    def test_patch_cache_agrees_with_directory_list(self, path_cache, directory_list):
        assert sorted(path_cache) == sorted(directory_list)
