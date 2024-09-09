#!/usr/bin/env pytest

#
# To get information about the actual durations being reported by the tests,
# run with the INFO debug level:
#
#  pytest test_activation_metrics.py --log-cli-level INFO
#

import time
import logging

import htcondor

from ornithology import (
    config, standup, action,
    Condor, ClusterState,
    write_file, format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

#
# See HTCONDOR-861 for a detailed description of the metrics and the job
# lifecycle they assume.
#
# For testing purposes, we verify that:
#
# (a) all four activation metric values appear in the job ad
# (b) ActivationDuration >= Activation[SetUp + Execution + Teardown]Duration
# (c) ActivationDuration is less than the shadow lifetime, which is
#     approximately CompletionDate - JobStartDate.
#
# Conditions (b) and (c) are consistency conditions.  These tests don't
# attempt to verify the correctness of the durations in question because
# the durations will vary depending on the load on machine running the
# tests, and trying to compensate for that could be quickly become very
# complicated (see the custom machine resource(s) tests) and very slow
# (in order to always generate timings that are larger than the variances).
#
# However, we would also like to verify that the code notices when a
# self-checkpointing job checkpoints and restarts.  Because the execution
# duration is only updated at exit time [at least for now], we can check
# this by verifying that the execution duration changes during a self-
# checkpointing job (whose checkpoint interval varies).  As a further
# consistency check, we should verify that the ActivationDuration is
# larger than the sum of the two different execution durations that we see.
#

@action
def job_one_handle(default_condor, test_dir, path_to_sleep):
    handle = default_condor.submit(
        description={
            "log":                      test_dir / "job_one.log",
            "leave_in_queue":           True,

            "universe":                 "vanilla",
            "should_transfer_files":    True,
            "executable":               path_to_sleep,
            "arguments":                20,
        },
        count=1,
    )

    handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        verbose=True,
        timeout=60,
    )

    yield handle

    handle.remove()


@action
def job_one_ad(job_one_handle):
    for ad in job_one_handle.query():
        # On rare occasions, the job may be in the
        # 'C'omplete status, but the schedd hasn't
        # written the "CompletionDate" yet.  Check
        # to see if we are in this state, and if so
        # sleep 10 seconds and try again.
        try:
            cl = ad['CompletionDate']
            return ad
        except:
            time.sleep(10);
            return job_one_handle.query()[0]


@action
def path_to_job_two_script(test_dir):
    script="""
    #!/usr/bin/python3

    import sys
    import time
    from htcondor.htchirp import HTChirp

    nap = 0
    nap_lengths = [5, 10, 15]

    try:
        with open("saved-state", "r") as saved_state:
            nap = int(saved_state.readline().strip())
            print(f"Restarting naps from #{nap}")
    except IOError:
        pass

    print(f"Nap #{nap} will be {nap_lengths[nap]} seconds long.")
    time.sleep(nap_lengths[nap])
    nap += 1

    with HTChirp() as chirp:
        chirp.set_job_attr_delayed("ChirpIterationNum", f"{nap}")

    if nap >= len(nap_lengths):
        print(f"Completed all naps.")
        sys.exit(0)

    try:
        with open("saved-state", "w") as saved_state:
            saved_state.write(f"{nap}")
            sys.exit(17)
    except IOError:
        print("Failed to write checkpoint.", file=sys.stderr);
        sys.exit(1)
    """

    path = test_dir / "counting.py"
    write_file(path, format_script(script))

    return path


@action
def job_two_handle(default_condor, test_dir, path_to_job_two_script):
    handle = default_condor.submit(
        description={
            "output":                       test_dir / "job_two.out",
            "error":                        test_dir / "job_two.err",
            "log":                          test_dir / "job_two.log",
            "leave_in_queue":               True,
            "stream_output":                True,
            "stream_error":                 True,

            "universe":                     "vanilla",
            "should_transfer_files":        True,
            "executable":                   path_to_job_two_script,
            "arguments":                    20,
            "checkpoint_exit_code":         17,
            "transfer_checkpoint_files":    "saved-state",

            "hold":                         True,
            "getenv":                       "PYTHONPATH",
        },
        count=1,
    )

    yield handle

    handle.remove()


@action
def job_two_execution_durations(default_condor, job_two_handle):
    job_two_handle.release()

    assert job_two_handle.wait(
        condition=ClusterState.any_running,
        fail_condition=ClusterState.any_terminal,
        verbose=True,
        timeout=20
    )

    # Because we don't report [Activation]StartExecutionTime, we have to
    # assume/ensure that the job's execution durations are dissimilar.
    start = time.time()
    durations = []
    current_iteration = 0
    job_terminated = False
    while time.time() - start < 60:
        time.sleep(3)

        ad = list(job_two_handle.query())[0]
        try:
            new_iter = ad["ChirpIterationNum"]
            aed = ad["ActivationExecutionDuration"]
            if new_iter != current_iteration:
                current_iteration = new_iter
                durations.append(aed)
        except KeyError:
            pass

        if job_terminated:
            break

        job_two_handle.state.read_events()
        if job_two_handle.state.all_terminal():
            # If job is done, don't just break here; instead, set a flag and go
            # back through the while loop one last time to ensure we get the
            # last execution duration.  I.e. we want to avoid the race condition
            # between seeing ActivationExecutionDuration updated -vs- seeing the
            # job complete.
            job_terminated = True

    job_two_handle.state.read_events()
    assert job_two_handle.state.all_complete()
    return durations


@action
def job_two_ad(job_two_handle, job_two_execution_durations):
    for ad in job_two_handle.query():
        return ad


class TestActivationMetrics:
    def test_internal_consistency(self, job_one_ad):
        ad   = job_one_ad["ActivationDuration"]
        asud = job_one_ad["ActivationSetUpDuration"]
        aed  = job_one_ad["ActivationExecutionDuration"]
        atd  = job_one_ad["ActivationTeardownDuration"]
        assert None not in [ad, asud, aed, atd]

        logger.info(f"ActivationDuration = {ad}")
        logger.info(f"ActivationSetUpDuration = {asud}")
        logger.info(f"ActivationExecutionDuration = {aed}")
        logger.info(f"ActivationTeardownDuration = {atd}")
        assert ad >= asud + aed + atd

    def test_external_consistency(self, job_one_ad):
        ad  = job_one_ad["ActivationDuration"]
        jsd = job_one_ad["JobStartDate"]
        cd  = job_one_ad["CompletionDate"]
        assert None not in [ad, jsd, cd]

        logger.info(f"ActivationDuration = {ad}")
        logger.info(f"JobStartDate = {jsd}")
        logger.info(f"CompletionDate = {cd}")
        assert ad <= cd - jsd

    def test_self_checkpointing_job(self, job_two_execution_durations, job_two_ad):
        assert len(job_two_execution_durations) >= 2

        job_two_duration = job_two_ad["ActivationDuration"]
        assert job_two_duration is not None

        logger.info(f"job_two_duration = {job_two_duration}")
        logger.info(f"first execution duration = {job_two_execution_durations[0]}")
        logger.info(f"second execution duration = {job_two_execution_durations[1]}")
        logger.info(f"third execution duration = {job_two_execution_durations[2]}")
        assert( job_two_execution_durations[0] + job_two_execution_durations[1] ) <= job_two_duration


        # Make sure ActivationSetupDuration and ActivationTeardownDuration numbers are not inflated
        # due to multiple executions (resulting from user checkpoints).  HTCONDOR-1190
        job_two_setup = job_two_ad["ActivationSetupDuration"]
        assert job_two_setup is not None
        job_two_teardown = job_two_ad["ActivationTeardownDuration"]
        assert job_two_teardown is not None
        logger.info(f"job_two_setup = {job_two_setup}")
        logger.info(f"job_two_teardown = {job_two_teardown}")
        # We will allow one second of slop in all this, thus below 'job_two_duration + 1'
        assert( job_two_setup + job_two_execution_durations[0] + job_two_execution_durations[1] + job_two_execution_durations[2] + job_two_teardown ) <= ( job_two_duration + 1 )
