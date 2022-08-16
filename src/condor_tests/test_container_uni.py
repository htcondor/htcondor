#!/usr/bin/env pytest


# Test that we can run simple container universe
# jobs with "singularity", by configuring condor
# to mock the singularity executable with a simple
# wrapper script.

import logging
import htcondor
import os,sys,stat

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Write out a file that mocks the singularity cli
@standup
def mock_singularity_file(test_dir):
    mock_singularity = """#!/bin/bash

# ignore leading - options, except for --version
while [[ "$1" =~ "-".* ]]
do
	if [ "$1" == "--version" ]
	then
		echo "3.0.0"
		exit 0
	fi

	shift
done

#  Have test command always return true
if [ "$1" = "test" ]
then
	echo 0
	exit 0
fi

# Just run the command without a container
if [ "$1" = "exec" ]
then
	# Shift off the exec
	shift

	while [[ "$1" =~ "-".* ]]
	do
		case $1 in
		 -S)
			 # scratch dir has an argument
			 shift
			 shift
			 ;;
		 -B)
			 # scratch dir has an argument
			 shift
			 shift
			 ;;
		 --pwd)
			 shift
			 shift
			 ;;
		 --no-home)
			 shift
			 ;;
		 -C)
			 shift
			 ;;
		 -*)
			 echo "Unknown argument at $@"
			 exit 1
			 break
			 ;;
		esac
	done

	# Shift off the container name
	shift

	# special case for test job
	if [ "$1" = "/exit_37" ]
	then
		exit 37
	fi

	# And run the command
	eval "$@"
	exit $?
fi

echo "Unknown/unsupported singularity command: $@"
exit 0
    """
    filename = test_dir / 'singularity_tester.sh'
    write_file(filename, mock_singularity)
    os.chmod(filename, stat.S_IRWXU)
    return filename

# Setup a personal condor with SINGULARITY pointing at the mocking wrapper
@standup
def condor(test_dir, mock_singularity_file):
    # Singularity wrapper is in the root of the per-test directory, two
    # levels above the condor config
    with Condor(test_dir / "condor", config={"SINGULARITY": mock_singularity_file}) as condor:
        yield condor

# The actual job will cat a file
@action
def test_job_hash():
    return {
            "universe": "container",
            "container_image": "ignored.sif",
            "executable": "/bin/cat",
            "arguments": "/etc/hosts",
            "should_transfer_files": "yes",
            "when_to_transfer_output": "on_exit",
            "output": "output",
            "error": "error",
            "log": "log",
            }

@action
def completed_test_job(condor, test_job_hash):

    ctj = condor.submit(
        {**test_job_hash}, count=1
    )
    assert ctj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return ctj

@action
def test_job_hash_with_target_dir(test_job_hash):
    test_job_hash["container_target_dir"] = "/foo"
    return test_job_hash

@action
def completed_test_job_with_target_dir(condor, test_job_hash_with_target_dir):

    ctj = condor.submit(
        {**test_job_hash_with_target_dir}, count=1
    )
    assert ctj.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )

class TestContainerUni:
    def test_container_uni(self, completed_test_job):
            assert True
    def test_container_uni_with_target_dir(self, completed_test_job_with_target_dir):
            assert True
