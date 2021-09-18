#!/usr/bin/env pytest

import logging

from htcondor import (
    JobEventType,
)

from ornithology import (
    config,
    standup,
    action,

    write_file,
    ClusterState,
    format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


#
# 1) Does a self-checkpointing job that should complete do so when
#    max_checkpoint_interval is set?
# 2) Does a self-checkpointing job that should complete do so when
#    max_checkpoint_interval is set if it's vacated?
#    [FIXME: Consider carefully if the timing of the vacate matters]
# 3) Does a self-checkpointing job that should be held before its first
#    checkpoint become so?
# 4) Does a self-checkpointing job that should be held before its second
#    checkpoint become so?
# 5) Does a self-checkpointing job that should be held before its second
#    checkpoint become so if it's restarted after its first checkpoint?
#

#
# Please don't try to edit the constants to speed this test up.
#
# If you must, make sure, among other things, that this job takes
# longer to complete than twice the specified MaxCheckpointInterval.
#

@action
def path_to_the_job_script(test_dir):
    script = """
    #!/usr/bin/python3

    import sys
    import time
    import getopt

    opt_slow_step = -1
    (list, remainder) = getopt.getopt(sys.argv[1:], '', ['normal', 'slow='])
    for option, value in list:
        if option == "--slow":
            opt_slow_step = int(value)

    total_steps = 24
    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
    except IOError:
        pass

    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")

        if num_completed_steps == opt_slow_step:
            time.sleep(60)
        else:
            time.sleep(3)
        num_completed_steps += 1

        if num_completed_steps % 5 == 0:
            print(f"Checkpointing after {num_completed_steps}.")
            try:
                with open("saved-state", "w") as saved_state:
                    saved_state.write(f"{num_completed_steps}")
                sys.exit(17)
            except IOError:
                print("Failed to write checkpoint.", file=sys.stderr);
                sys.exit(1)

    print(f"Completed all {total_steps} steps.")
    sys.exit(0)
    """

    path = test_dir / "counting.py"
    write_file(path, format_script(script))

    return path


@action
def the_job_description(path_to_the_job_script):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_the_job_script.as_posix(),
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "checkpoint_exit_code":         "17",
        "transfer_checkpoint_files":    "saved-state",

        # Desired "first-class" implementation.
        # "max_checkpoint_interval":    "30",

        "+MaxCheckpointInterval":       "30",
        "+HasCheckpointedThisTime":     "TransferOutFinished =!= undefined && TransferOutFinished > EnteredCurrentStatus",
        "+IsRunning":                   "JobStatus == 2",
        "+TimeSinceLastCheckpoint":     "CurrentTime - TransferOutFinished",
        "+TimeSinceThisTimeStarted":    "CurrentTime - EnteredCurrentStatus",
        "+OverMaxCheckpointInterval":   "IsRunning && (ifthenelse(HasCheckpointedThisTime, TimeSinceLastCheckpoint, TimeSinceThisTimeStarted) > MaxCheckpointInterval)",

        "periodic_hold":                "OverMaxCheckpointInterval",
    }


#
# We submit the jobs for tests #1, #3, and #4, at the same time,
# because we don't need to interact with them while they're running, just
# check their log when they're done.
#
# Tests #2 and #5 require that the job be vacated at specific times.
# FIXME: If those jobs can vacate themselves (once), that could immensely
# simplify writing this test.
#

#
# FIXME: We should check at the end of every otherwise-successful test
# to see if the final the output log is what we expect.
#

#
# FIXME: For jobs which are (deliberately) interruped, we should check if
# they actually resumed from where they left off.  I don't know how to do
# so right at the moment.
#

#
# This fixture depends on all of the job_*_handle fixtures we want to run
# concurrently.  Since each of the final_job_*_handle fixtures depend
# on this fixture, each job will have been submitted before we start
# waiting on the first one, whichever one that might be.
#

@action
def all_job_handles(job_one_handle, job_three_handle, job_four_handle):
    pass


@action
def job_one_handle(default_condor, test_dir, the_job_description):
    job_one_description = {
        ** the_job_description,
        ** {
            "arguments":    "--normal",
            "log":          test_dir / "job_one.log",
            "output":       test_dir / "job_one.out",
            "error":        test_dir / "job_one.err",
        }
    }

    job_one_handle = default_condor.submit(
        description = job_one_description,
        count = 1,
    )

    yield job_one_handle

    job_one_handle.remove()


@action
def final_job_one_handle(default_condor, job_one_handle, all_job_handles):
    job_one_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    return job_one_handle


@action
def job_one_events(final_job_one_handle):
    return final_job_one_handle.event_log.events


@action
def job_three_handle(default_condor, test_dir, the_job_description):
    job_three_description = {
        ** the_job_description,
        ** {
            "arguments":    "--slow 0",
            "log":          test_dir / "job_three.log",
            "output":       test_dir / "job_three.out",
            "error":        test_dir / "job_three.err",
       }
    }

    job_three_handle = default_condor.submit(
        description = job_three_description,
        count = 1,
    )

    yield job_three_handle

    job_three_handle.remove()


@action
def final_job_three_handle(default_condor, job_three_handle, all_job_handles):
    job_three_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.any_held,
        fail_condition = ClusterState.all_complete,
    )

    return job_three_handle


@action
def job_three_events(final_job_three_handle):
    return final_job_three_handle.event_log.events


@action
def job_four_handle(default_condor, test_dir, the_job_description):
    job_four_description = {
        ** the_job_description,
        ** {
            "arguments":    "--slow 6",
            "log":          test_dir / "job_four.log",
            "output":       test_dir / "job_four.out",
            "error":        test_dir / "job_four.err",
       }
    }

    job_four_handle = default_condor.submit(
        description = job_four_description,
        count = 1,
    )

    yield job_four_handle

    job_four_handle.remove()


@action
def final_job_four_handle(default_condor, job_four_handle, all_job_handles):
    job_four_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.any_held,
        fail_condition = ClusterState.all_complete,
    )

    return job_four_handle


@action
def job_four_events(final_job_four_handle):
    return final_job_four_handle.event_log.events


#
# Utility functions for the tests.
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


class TestMaxCheckpointInterval:
    def test_job_one_completed(self, job_one_events):
        assert not types_in_events(
            [JobEventType.JOB_HELD], job_one_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.JOB_TERMINATED,
            ],
            job_one_events
        )


    def test_job_three_held(self, job_three_events):
        assert not types_in_events(
            [JobEventType.JOB_TERMINATED], job_three_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.JOB_HELD,
            ],
            job_three_events
        )


    def test_job_four_held(self, job_four_events):
        assert not types_in_events(
            [JobEventType.JOB_TERMINATED], job_four_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.JOB_HELD,
            ],
            job_four_events
        )
