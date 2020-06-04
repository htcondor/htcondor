#!/usr/bin/env pytest

#
# This test duplicates cmd_now.run
#

import logging
import htcondor

from ornithology import (
    config, standup, action, JobStatus, ClusterState, Condor, path_to_sleep
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
        "time_out": ("1", "[now job {0}]: coalesce command timed out, failing"),
        #        "transient_failure": ("2", "[now job {0}]: coalesce failed"),
        #        "permanent_failure": ("3", "[now job {0}]: coalesce failed last retry, giving up"),
        #        "no_claim_ID": ("4", "[now job {0}]: coalesce did not return a claim ID, failing"),
        #        "too_slow": (
        #            "5",
        #            "[now job {0}]: targeted job(s) did not vacate quickly enough, failing",
        #        ),
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
        "log": f"cmd_now-failure-{failure_config_value}.log",
    }


@action
def victim_jobs(
    failure_injection_condor, failure_injection_job_parameters, max_victim_jobs
):
    victim_jobs = failure_injection_condor.submit(
        failure_injection_job_parameters, count=max_victim_jobs
    )
    assert victim_jobs.wait(
        condition=ClusterState.all_running, timeout=60, verbose=True
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
    return failure_log_message.format(f"{beneficiary_job.clusterid}.0")


@action
def sched_log_containing_failure(failure_log_message_with_job_id, sched_log, failure_injection_condor):
    # Wait for the injected failure to happen.
    assert sched_log.wait(
        # The documentation of DaemonLogMessage looks incomplete, but since
        # this doesn't start with an underscore, I'm assuming it's public.
        condition=lambda line: line.message == failure_log_message_with_job_id,
        timeout=60
    )

    # Once the injected failure happens, we want to check the schedd's
    # pcccTable.  It will dump it when asked for its machine ads.
    failure_injection_condor.direct_status(
        htcondor.DaemonTypes.Schedd,
        htcondor.AdTypes.Startd
    )

    return sched_log


class TestCondorNow:
    def test_failure_cleanup(self, sched_log_containing_failure):
        # FIXME: asserting that lines exist in order in a log, possibly
        # without any gaps, should certainly be a helper function.
        match = 0
        # Read the remainder of the log.
        lines = list(sched_log_containing_failure.read())
        for line in lines:
            if match == 0 and "pcccDumpTable(): dumping table..." in line:
                match = 1
                continue
            if (
                match == 1
                and "pcccDumpTable(): ... done dumping PCCC table." in line
            ):
                match = 2
                continue
            if (
                match == 2
                and "Dumping match records (with now jobs)..." in line
            ):
                match = 3
                continue
            if match == 3 and "... done dumping match records." in line:
                match = 4
                break
            match = 0
        assert match == 4

    """
    def test_success(self, job_event_log, beneficiaryID, victimIDs):
        # assert that certain job events happened in order
        assert False
    """
