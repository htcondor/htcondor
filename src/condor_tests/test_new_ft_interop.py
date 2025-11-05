#!/usr/bin/env pytest

import time

from ornithology import (
    action,
    ClusterState,
    Condor,
    format_script,
    write_file,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


TEST_CASES = {
    "new_both": {
        "config": {
            "STARTER_USES_NULL_FILE_TRANSFER":  "TRUE",
            "SHADOW_USES_NULL_FILE_TRANSFER":   "TRUE",
        },
        "expected": None,
    },
    "new_starter": {
        "config": {
            "STARTER_USES_NULL_FILE_TRANSFER":  "TRUE",
            "SHADOW_USES_NULL_FILE_TRANSFER":   "FALSE",
        },
        "expected": None,
    },
    "new_shadow": {
        "config": {
            "STARTER_USES_NULL_FILE_TRANSFER":  "FALSE",
            "SHADOW_USES_NULL_FILE_TRANSFER":   "TRUE",
        },
        "expected": None,
    },
    # This should only be useful for testing the test.
    "new_neither": {
        "config": {
            "STARTER_USES_NULL_FILE_TRANSFER":  "FALSE",
            "SHADOW_USES_NULL_FILE_TRANSFER":   "FALSE",
        },
        "expected": None,
    },
}


@action
def the_condors(test_dir):
    the_condors = {}

    for case in TEST_CASES.keys():
        local_dir = test_dir / f"condor.{case}"

        the_condor = Condor(
            local_dir=local_dir,
            config=TEST_CASES[case]['config'],
        )
        the_condor._start()
        the_condors[case] = the_condor

    yield the_condors

    for the_condor in the_condors.values():
        the_condor._cleanup()


@action
def the_test_script(test_dir):
    path = test_dir / "copy_input_to_output.py"
    contents = format_script(
    '''
        #!/usr/bin/python3

        import shutil

        shutil.copyfile( 'input', 'output' )
    '''
    )
    write_file(path, contents)

    return path


@action
def the_input_file(test_dir):
    path = test_dir / "input"
    path.write_text( "input file text" )
    return path


@action
def the_jobs(test_dir, the_condors, the_test_script, the_input_file):
    description = {
        "universe":                 "vanilla",
        "executable":               the_test_script.as_posix(),
        "arguments":                "1",
        "checkpoint_exit_code":     85,
        "when_to_transfer_output":  "ON_EXIT",
        "transfer_executable":      True,
        "should_transfer_files":    True,
        "request_cpus":             1,
        "request_memory":           1,

        "transfer_input_files":     the_input_file.as_posix(),
        "transfer_output_files":    'output',
    }

    the_jobs = {}
    for case, the_condor in the_condors.items():
        initial_dir = test_dir / f"{case}"
        initial_dir.mkdir(parents=True, exist_ok=True)
        the_job = the_condor.submit(
            description={
                ** description,
                "log":              (test_dir / f"{case}.log").as_posix(),
                "output":           (test_dir / f"{case}.out").as_posix(),
                "initialdir":       initial_dir.as_posix(),
            },
            count=1,
        )
        the_jobs[case] = the_job

    return the_jobs


@action(params={case: case for case in TEST_CASES})
def the_job_pair(request, the_jobs):
    return (request.param, the_jobs[request.param])


@action
def the_case(the_job_pair):
    return the_job_pair[0]


@action
def the_job(the_job_pair):
    return the_job_pair[1]


@action
def the_completed_job(the_case, the_condors, the_job):
    assert the_job.wait(
        timeout=300,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job


class TestRestartIsReactivate():

    def test_job_completed_successfully(self, test_dir, the_case, the_completed_job):
        output_path = test_dir / f"{the_case}" / "output"
        assert output_path.read_text() == "input file text"
