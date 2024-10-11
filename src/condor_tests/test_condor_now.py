#!/usr/bin/env pytest

#
# This test duplicates cmd_now.run
#

import logging
import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    JobStatus,
    ClusterState,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config
def max_victim_jobs():
    return 3


@config(
    params={
        # PyTest and HTCondor can both handle spaces in the parameter name,
        # but it's easier debugging in the shell without them.
        "time_out": ("1", "Failed to send COALESCE_SLOTS to startd"),
        "transient_failure": (
            "2",
            "[now job {0}]: coalesce failed: FAILURE INJECTION: 2",
        ),
        "permanent_failure": (
            "3",
            "[now job {0}]: coalesce failed last retry, giving up.",
        ),
        "no_claim_ID": (
            "4",
            "[now job {0}]: coalesce did not return a claim ID, failing",
        ),
        "too_slow": (
            "5",
            "[now job {0}]: targeted job(s) did not vacate quickly enough, failing",
        ),
    }
)
def failure_injection(request):
    return request.param


@config
def failure_config_value(failure_injection):
    return failure_injection[0]


@config
def failure_log_message(failure_injection):
    return failure_injection[1]


@config
def failure_injection_config(max_victim_jobs, failure_config_value):
    config = {
        "NUM_CPUS": max_victim_jobs,
        "STARTD_DEBUG": "D_CATEGORY D_SUB_SECOND D_TEST",
        "SCHEDD_DEBUG": "D_CATEGORY D_SUB_SECOND D_TEST",
        "COALESCE_FAILURE_MODE": failure_config_value,
    }
    raw_config = "use feature: PartitionableSlot"
    return {"config": config, "raw_config": raw_config}


@standup
def failure_injection_condor(test_dir, failure_injection_config):
    with Condor(test_dir / "condor", **failure_injection_config) as condor:
        yield condor


@action
def failure_injection_job_parameters(failure_config_value, path_to_sleep):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "600",
        "log": "cmd_now-failure-{}.log".format(failure_config_value),
    }


@action
def victim_jobs(
    failure_injection_condor, failure_injection_job_parameters, max_victim_jobs
):
    victim_jobs = failure_injection_condor.submit(
        failure_injection_job_parameters, count=max_victim_jobs
    )
    assert victim_jobs.wait(
        condition=ClusterState.all_running,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return victim_jobs


@action
def beneficiary_job(
    failure_injection_job_parameters, failure_injection_condor, victim_jobs
):
    return failure_injection_condor.submit(failure_injection_job_parameters, count=1)


@action
def sched_log(failure_injection_condor, beneficiary_job, victim_jobs):
    bID = str(beneficiary_job.job_ids[0])
    vIDs = [str(vID) for vID in victim_jobs.job_ids]
    rv = failure_injection_condor.run_command(["condor_now", bID] + vIDs)
    assert rv.returncode == 0
    return failure_injection_condor.schedd_log.open()


@action
def failure_log_message_with_job_id(failure_log_message, beneficiary_job):
    return failure_log_message.format("{}.0".format(beneficiary_job.clusterid))


@action
def sched_log_containing_failure(
    failure_log_message_with_job_id, sched_log, failure_injection_condor
):
    # Wait for the injected failure to happen.
    assert sched_log.wait(
        # The documentation of DaemonLogMessage looks incomplete, but since
        # this doesn't start with an underscore, I'm assuming it's public.
        condition=lambda line: failure_log_message_with_job_id in line.message,
        timeout=60,
    )

    # Once the injected failure happens, we want to check the schedd's
    # pcccTable.  It will dump it when asked for its machine ads.
    failure_injection_condor.direct_status(
        htcondor.DaemonTypes.Schedd, htcondor.AdTypes.Startd
    )

    # We presume that this won't result in a race condition becase the schedd
    # forces its log to disk before replying to the direct status query above.
    return sched_log


@config(params={"1_victim_job": 1, "2_victim_jobs": 2, "3_victim_jobs": 3})
def successful_max_victim_jobs(request):
    return request.param


@action
def successful_job_parameters(path_to_sleep, successful_max_victim_jobs):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "600",
        "log": "cmd_now-success.log",
    }


@config
def successful_condor_config(successful_max_victim_jobs):
    config = {
        "NUM_CPUS": successful_max_victim_jobs,
        "SCHEDD_DEBUG": "D_CATEGORY D_SUB_SECOND D_TEST",
    }
    raw_config = "use feature : PartitionableSlot"
    return {"config": config, "raw_config": raw_config}


@standup
def successful_condor(successful_condor_config, test_dir):
    with Condor(test_dir / "condor", **successful_condor_config) as condor:
        yield condor


@action
def successful_victim_jobs(
    successful_job_parameters, successful_condor, successful_max_victim_jobs
):
    victim_jobs = successful_condor.submit(
        successful_job_parameters, count=successful_max_victim_jobs
    )
    assert victim_jobs.wait(
        condition=ClusterState.all_running,
        timeout=60,
        verbose=True,
        fail_condition=ClusterState.any_held,
    )
    return victim_jobs


# Note that the beneficiary job fixtures depend on the victim job features,
# not because they need to know about them, but to ensure that the victim
# jobs have all started running before submitting the beneficiary.
@action
def successful_beneficiary_job(
    successful_job_parameters, successful_condor, successful_victim_jobs
):
    return successful_condor.submit(successful_job_parameters, count=1)


@action
def successful_job_log(
    successful_condor, successful_beneficiary_job, successful_victim_jobs, test_dir
):
    bID = str(successful_beneficiary_job.job_ids[0])
    vIDs = [str(vID) for vID in successful_victim_jobs.job_ids]
    rv = successful_condor.run_command(["condor_now", bID] + vIDs)
    assert rv.returncode == 0

    assert successful_beneficiary_job.wait(
        condition=ClusterState.all_running,
        timeout=60,
        fail_condition=ClusterState.any_held,
    )

    # This seems like something I should be able to get from the cluster handle.
    return htcondor.JobEventLog((test_dir / "cmd_now-success.log").as_posix())


@action
def sched_log_containing_success(successful_job_log, successful_condor):
    successful_condor.direct_status(
        htcondor.DaemonTypes.Schedd, htcondor.AdTypes.Startd
    )

    # We presume that this won't result in a race condition becase the schedd
    # forces its log to disk before replying to the direct status query above.
    return successful_condor.schedd_log.open()


def empty_pccc_table_and_no_match_records(log):
    expected_lines = [
        "pcccDumpTable(): dumping table...",
        "pcccDumpTable(): ... done dumping PCCC table.",
        "Dumping match records (with now jobs)...",
        "... done dumping match records.",
    ]

    # Iterate until we find the first expected line, then check that
    # all expected lines are in the log.  This avoids reading and
    # parsing the whole log in the (expected) case where we find what
    # we're looking for.  [FIXME: this should be an assert helper.]
    iter = log.read()
    for line in iter:
        if expected_lines[0] in line:
            break

    if not expected_lines[0] in line:
        return False

    for expected, line in zip(expected_lines[1:], iter):
        if not expected in line:
            return False

    return True


class TestCondorNow:
    def test_success(
        self,
        successful_job_log,
        successful_beneficiary_job,
        successful_victim_jobs,
        successful_max_victim_jobs,
        successful_condor,
        sched_log_containing_success,
    ):
        # We can't assert ornithology.in_order() because the evictions can
        # (and do) happen in any order.
        num_evicted_jobs = 0
        saw_beneficiary_execute = False
        for event in successful_job_log.events(0):
            if (
                event.type == htcondor.JobEventType.JOB_EVICTED
                and event.cluster == successful_victim_jobs.clusterid
            ):
                num_evicted_jobs += 1
                continue
            if (
                event.type == htcondor.JobEventType.EXECUTE
                and event.cluster == successful_beneficiary_job.clusterid
            ):
                saw_beneficiary_execute = True
                break
        assert (
            num_evicted_jobs == successful_max_victim_jobs and saw_beneficiary_execute
        )

    def test_success_cleanup(
        self, sched_log_containing_success,
    ):
        assert empty_pccc_table_and_no_match_records(sched_log_containing_success)

    def test_failure_cleanup(self, sched_log_containing_failure):
        assert empty_pccc_table_and_no_match_records(sched_log_containing_failure)
