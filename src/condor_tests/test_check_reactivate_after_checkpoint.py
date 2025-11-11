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


expected_output = '''Starting counting from 0
0
1
2
3
4
Checkpointing because I feel like it...
Starting counting from 5
5
6
7
8
9
Checkpointing because I feel like it...
Starting counting from 10
... finished computing.
'''


TEST_CASES = {
    # You really want this case first, so we can call condor_reschedule
    # while it's running and save ourselves a bunch of time.
    "true": {
        "config": {
            "CLAIM_WORKLIFE":           0,
            "REACTIVATE_ON_RESTART":    "TRUE",
        },
        "expected": True,
    },
    "true_but_useless": {
        "config": {
            "CLAIM_WORKLIFE":           999999,
            "REACTIVATE_ON_RESTART":    "TRUE",
        },
        "expected": False,
    },
    "false": {
        "config": {
            "CLAIM_WORKLIFE":           0,
            "REACTIVATE_ON_RESTART":    "FALSE",
        },
        "expected": False,
    },
    "unset": {
        "config": {
            "CLAIM_WORKLIFE":           0,
            # This can't be completely unset because it inherits from places.
            "REACTIVATE_ON_RESTART":    "",
        },
        "expected": False,
    }
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
    path = test_dir / "self-checkpoint.py"
    contents = format_script(
    '''
        #!/usr/bin/python3

        import sys
        import time
        from pathlib import Path


        def SaveState():
            global i
            Path("saved-state").write_text(str(i))


        def VacateJob():
            print("Saving state on vacate...")
            SaveState()
            sys.exit(85)


        def SpontaneousCheckpoint():
            print("Checkpointing because I feel like it...")
            SaveState()
            sys.exit(85)


        i = 0
        saved_state_file = Path("saved-state")
        if saved_state_file.exists():
            i = int(saved_state_file.read_text())

        print(f"Starting counting from {i}")
        while i != 10:
            time.sleep(1)

            print(i)

            i = i + 1

            if i % 5 == 0:
                SpontaneousCheckpoint()

        print( "... finished computing.")
        Path("output").touch()
    '''
    )
    write_file(path, contents)

    return path


@action
def the_jobs(test_dir, the_condors, the_test_script):
    description = {
        "universe":                 "vanilla",
        "executable":               the_test_script.as_posix(),
        "checkpoint_exit_code":     85,
        "when_to_transfer_output":  "ON_EXIT",
        "transfer_executable":      True,
        "should_transfer_files":    True,
        "request_cpus":             1,
        "request_memory":           1,
    }

    the_jobs = {}
    for case, the_condor in the_condors.items():
        the_job = the_condor.submit(
            description={
                ** description,
                "log":              (test_dir / f"{case}.log").as_posix(),
                "output":           (test_dir / f"{case}.out").as_posix(),
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
    # For unknown reasons(s), the schedd doesn't actually reschedule jobs
    # that were evicted immediately, so in some cases we have to kick it.

    we_should_wait = the_case == "true"
    while we_should_wait:

        print("Waiting for job to become idle...")
        assert the_job.wait(
            timeout=60,
            condition=ClusterState.all_idle,
            fail_condition=ClusterState.any_held,
        )

        print("... rescheduling...")
        the_condors[the_case].run_command(['condor_reschedule'])

        print("... waiting for job to start running...")
        assert the_job.wait(
            timeout=60,
            condition=ClusterState.all_running,
            fail_condition=ClusterState.any_held,
        )

        print("... waiting to see if the job becomes idle again...");
        we_should_wait = the_job.wait(
            timeout=60,
            condition=ClusterState.all_idle,
            fail_condition=ClusterState.any_terminal,
        )


    assert the_job.wait(
        timeout=300,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job


class TestRestartIsReactivate():

    def test_job_completed_successfully(self, test_dir, the_case, the_completed_job):
        # I'd love to tunnel this filename though the fixture system...
        actual_output_path = test_dir / f"{the_case}.out"
        actual_output = actual_output_path.read_text()
        assert actual_output == expected_output

        # I'd love to tunnel this filename though the fixture system...
        actual_log_path = test_dir / f"{the_case}.log"
        actual_log = actual_log_path.read_text()
        assert TEST_CASES[the_case]['expected'] == ('Rescheduling self-checkpoint job after checkpoint upload because reactivating the claim would have failed.' in actual_log)
