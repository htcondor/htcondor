#!/usr/bin/env pytest

import logging

from ornithology import (
    config,
    standup,
    action,

    in_order,
    JobStatus,
    write_file,
    SetJobStatus,
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


@action
def path_to_the_job_script(test_dir):
    script = """
    #!/usr/bin/python3

    import sys
    import time

    total_steps = 24
    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
    except IOError:
        pass

    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")
        time.sleep(1)
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
def the_job_description(test_dir, path_to_the_job_script):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "output":                       test_dir / "job_one.out",
        "error":                        test_dir / "job_one.err",
        "log":                          test_dir / "job.log",

        "executable":                   path_to_the_job_script.as_posix(),
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "checkpoint_exit_code":         "17",
        "transfer_checkpoint_files":    "saved-state",

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
# FIXME: We submit the jobs for tests #1, #3, and #4, at the same time,
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


@action
def job_one_handle(default_condor, the_job_description):
    job_one_description = {** the_job_description, ** {"arguments": "FIXME"}}

    job_one_handle = default_condor.submit(
        description = job_one_description,
        count = 1,
    )

    yield job_one_handle

    job_one_handle.remove()


#
# This fixture depends on all of the job_*_handle fixtures we want to run
# concurrently.  Since each of the completed_job_*_handle fixtures depend
# on this fixture, each job will have been submitted before we start
# waiting on the first one, whichever one that might be.
#


@action
def all_job_handles(job_one_handle):
    pass


@action
def completed_job_one_handle(default_condor, job_one_handle, all_job_handles):
    job_one_handle.wait(
        verbose = True,
        timeout = 180,
        fail_condition = ClusterState.any_held,
    )

    # We must also wait for the job queue here, because we assume it's in
    # sync with the job handle later.
    default_condor.job_queue.wait_for_job_completion(job_one_handle.job_ids)

    return job_one_handle


@action
def completed_job_one_events(default_condor, completed_job_one_handle):
    return default_condor.job_queue.by_jobid[completed_job_one_handle.job_ids[0]]


class TestMaxCheckpointInterval:
    def test_job_completion(self, completed_job_one_events):
        assert SetJobStatus(JobStatus.HELD) not in completed_job_one_events
        assert in_order(
            completed_job_one_events,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.COMPLETED),
            ],
        )
