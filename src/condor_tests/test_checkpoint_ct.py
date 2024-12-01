#!/usr/bin/env pytest

import os
import time
import subprocess

from ornithology import (
    action,
    write_file,
    format_script,
    ClusterState,
)

import htcondor2 as htcondor
from htcondor2 import (
    JobEventType,
    FileTransferEventType,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def path_to_the_job_script(default_condor, test_dir):
    with default_condor.use_config():
        condor_bin = htcondor.param["BIN"]

    # This script can't have embedded newlines for stupid reasons.
    script = f"""
    #!/usr/bin/python3

    import os
    import sys
    import time
    import math
    import subprocess

    from pathlib import Path

    os.environ['CONDOR_CONFIG'] = '{default_condor.config_file}'
    condor_bin = '{condor_bin}'

""" + """
    os.environ['PATH'] = os.environ.get('PATH', '') + os.pathsep + condor_bin


    num_shadow_starts = 0
    rv = subprocess.run(
        ["condor_q", sys.argv[1], "-af", "NumShadowStarts"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        universal_newlines=True,
    )
    if rv.returncode == 0:
        num_shadow_starts = int(rv.stdout.strip())

    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
            print(f"Resuming after step {num_completed_steps}.")
    except IOError:
        pass

    total_steps = 19
    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")
        num_completed_steps += 1

        if num_completed_steps % 5 == 0:
            print(f"Checkpointing after {num_completed_steps}.")
            try:
                with open("saved-state", "w") as saved_state:
                    saved_state.write(f'{num_completed_steps}')
            except IOError:
                print("Failed to write checkpoint.", file=sys.stderr);
                sys.exit(1)

            sys.exit(85)

        print(f"num_completed_steps = {num_completed_steps}", file=sys.stderr);
        print(f"num_shadow_starts = {num_shadow_starts}", file=sys.stderr);

        if (num_shadow_starts * 7) == num_completed_steps:
            rv = subprocess.run(
                ["condor_vacate_job", sys.argv[1]],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=20,
            )
            if rv.returncode != 0:
                print(f'Failed to vacate myself:')
                print(f'{rv.stdout}')
                print(f'{rv.stderr}')

            # Don't exit before we've been vacated.
            time.sleep(60)

            # Signal a failure if we weren't vacated.
            print("Failed to evict after sixty seconds, aborting.")
            sys.exit(1)

        time.sleep(1)


    print(f"Completed all {total_steps} steps.")
    sys.exit(0)
    """

    path = test_dir / "counting.py"
    write_file(path, format_script(script))

    return path


@action
def the_job_description(test_dir, path_to_the_job_script):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_the_job_script.as_posix(),
        "arguments":                    "$(CLUSTER).$(PROCESS)",
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "log":                          test_dir / "test_job_$(CLUSTER).log",
        "output":                       test_dir / "test_job_$(CLUSTER).out",
        "error":                        test_dir / "test_job_$(CLUSTER).err",
        "want_io_proxy":                "True",

        "checkpoint_exit_code":         "85",
        "transfer_checkpoint_files":    "saved-state",
        "transfer_output_files":        "",
    }


# This construction allows us to run test jobs concurrently.  It's clumsy,
# in that the test cases should be directly parameterizing a fixture, but
# there's no native support for concurrency in pytest, so this will do.
TEST_CASES = {
    "spool": {},
}

@action
def the_job_handles(test_dir, default_condor, the_job_description):
    job_handles = {}
    for name, test_case in TEST_CASES.items():
        complete_job_description = {
            ** the_job_description,
            ** test_case,
        }
        job_handle = default_condor.submit(
            description=complete_job_description,
            count=1,
        )

        job_handles[name] = job_handle

    yield job_handles

    for job_handle in job_handles:
        job_handles[job_handle].remove()


@action(params={name: name for name in TEST_CASES})
def the_job_pair(request, the_job_handles):
    return (request.param, the_job_handles[request.param])


@action
def the_job_name(the_job_pair):
    return the_job_pair[0]


@action
def the_job_handle(the_job_pair):
    return the_job_pair[1]


@action
def the_completed_job(default_condor, the_job_handle):
    assert the_job_handle.wait(
        timeout=300,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_held,
    )

    assert the_job_handle.wait(
        timeout=300,
        condition=ClusterState.all_idle,
        fail_condition=ClusterState.any_held,
    )

    default_condor.run_command(['condor_reschedule'])


    assert the_job_handle.wait(
        timeout=300,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_held,
    )

    assert the_job_handle.wait(
        timeout=300,
        condition=ClusterState.all_idle,
        fail_condition=ClusterState.any_held,
    )

    default_condor.run_command(['condor_reschedule'])


    the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle


@action
def the_completed_job_stdout(test_dir, the_job_name, the_completed_job):
    cluster = the_completed_job.clusterid
    output_log_path = test_dir / f"test_job_{cluster}.out"

    with open(test_dir / output_log_path) as output:
        return output.readlines()


@action
def the_completed_job_stderr(test_dir, the_completed_job):
    cluster = the_completed_job.clusterid
    with open(test_dir / f"test_job_{cluster}.err" ) as error:
        return error.readlines()


#
# Assertion functions for the tests.  From test_allowed_execute_duration.py,
# which means we should probably drop them in Ornithology somewhere.
#

def types_in_events(types, events):
    return any(t in [e.type for e in events] for t in types)


def event_types_in_order(types, events):
    t = 0
    for i in range(0, len(events)):
        event = events[i]

        if event.type == types[t]:
            t += 1
            if t == len(types):
                return True
    else:
        return False


#
# Making this a fixture rather than a test makes all other problems with
# self-checkpointing show up as an error rather than a failure, which
# makes sense because test_committed_time() assumes a successful job.
#

@action
def the_successful_job(the_completed_job, the_completed_job_stdout):
    assert not types_in_events(
        [JobEventType.JOB_HELD], the_completed_job.event_log.events
    )

    assert event_types_in_order(
        [
            JobEventType.SUBMIT,
            JobEventType.EXECUTE,
            JobEventType.FILE_TRANSFER,
            JobEventType.EXECUTE,
            JobEventType.FILE_TRANSFER,
            JobEventType.EXECUTE,
            JobEventType.FILE_TRANSFER,
            JobEventType.JOB_TERMINATED,
        ],
        the_completed_job.event_log.events
    )

    expected_stdout = [
        "Starting step 0.\n",
        "Starting step 1.\n",
        "Starting step 2.\n",
        "Starting step 3.\n",
        "Starting step 4.\n",
        "Checkpointing after 5.\n",
        "Resuming after step 5.\n",
        "Starting step 5.\n",
        "Starting step 6.\n",
        "Starting step 7.\n",
        "Starting step 8.\n",
        "Starting step 9.\n",
        "Checkpointing after 10.\n",
        "Resuming after step 10.\n",
        "Starting step 10.\n",
        "Starting step 11.\n",
        "Starting step 12.\n",
        "Starting step 13.\n",
        "Starting step 14.\n",
        "Checkpointing after 15.\n",
        "Resuming after step 15.\n",
        "Starting step 15.\n",
        "Starting step 16.\n",
        "Starting step 17.\n",
        "Starting step 18.\n",
        "Completed all 19 steps.\n"
    ]
    assert expected_stdout == the_completed_job_stdout

    return the_completed_job


class TestCheckpointCommittedTime:


    def test_committed_time(self, test_dir, default_condor, the_successful_job):
        jobID = f"{the_successful_job.clusterid}.0"

        rv = default_condor.run_command(
            ["condor_q", jobID, "-af", "CommittedTime"],
            timeout=20,
        )
        assert(rv.returncode == 0)
        committed_time = int(rv.stdout.strip())

        # There are three intervals of nominally 5 seconds and one interval
        # of nominally 4 seconds.  Because of rounding, each interval
        # can be recorded as a full second shorter or longer.
        # And on our slower platforms, there is even more slop
        # In particular, on the macOS build machines, we're seeing job
        # processes taking 3-4 seconds to begin execution after the
        # fork/exec by the starter.
        # TODO A better check here would be to read the event log to see
        #   how long condor reported each execution taking, and compare
        #   that to the CommittedTime in the job ad.
        assert(15 <= committed_time and committed_time <= 60)

        # We also look at the job queue log, which will not have been rotated
        # because with the default_condor as a parameter, it can't be garbage-
        # collected until after this function exits.
        spool = test_dir / "condor" / "spool"
        job_queue_log_path = spool / "job_queue.log"

        num_updates = 0
        line_prefix = f"103 {jobID} CommittedTime "
        with open(job_queue_log_path, "r") as job_queue_log:
            for line in job_queue_log:
                if line.startswith(line_prefix):
                    num_updates +=1

        # This ensures that we're counting the three checkpoints
        # and the final job exit, and nothing else.
        assert(num_updates == 4)
