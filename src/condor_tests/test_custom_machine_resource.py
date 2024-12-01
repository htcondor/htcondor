#!/usr/bin/env pytest

#
# Test one custom machine resource.  These tests are the model for the tests
# in test_custom_machine_{resources,intances}.py.  This test is run against
# static slots for simplicity.
#
# These tests verify that a custom machine resource is assigned properly (to
# slots), that no more slots are ever busy simultaneously than there are
# resources, that the peak number of simultaneously running jobs equals the
# number of resources, that the SUM- and PEAK- style usage is the same for the
# job and the slot, and that the reported usage is correct.
#
# This test SKIPS testing the SUM- and PEAK- style usages separately because
# I've never seen the tests fails independently.  Remove `marks=...` from
# `the_config` if you ever want to run them separately.
#

import pytest
import logging
import time

import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor,
    write_file,
    JobID,
    SetJobStatus,
    JobStatus,
    track_quantity,
    format_script,
)

from libcmr import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


monitor_period = 5
resource = "SQUID"
usages = [1, 4, 5, 9]
resources = { f"{resource}{i}": j for i, j in enumerate(usages) }
peaks = [
    [ 51, 51, 91, 11, 41, 41 ],
    [ 42, 42, 92, 12, 52, 52 ],
    [ 53, 53, 13, 93, 43, 43 ],
    [ 44, 44, 14, 94, 54, 54 ],
]
sequences = { f"{resource}{i}": j for i, j in enumerate(peaks) }


@config(
    params={
        "SQUIDsUsage": pytest.param({
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",
                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "SUM:SQUIDs",
                "START": "SQUIDsMonitorData == true",
            },
            "monitor": sum_monitor_script(resource, resources),
            "uptime_check": lambda c, h: sum_check_correct_uptimes(c, h, resource, resources),
            "matching_check": lambda h: sum_check_matching_usage(h, resource),
            "job": lambda t: sum_job(t, monitor_period, resource),
        }, marks=pytest.mark.skip),
        # }),

        "SQUIDsMemory": pytest.param({
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",
                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "PEAK:SQUIDsMemory",
                "START": "SQUIDsMonitorData == true",
            },
            "monitor": peak_monitor_script(resource, sequences),
            "uptime_check": lambda c, h: peak_check_correct_uptimes(c, h, resource, sequences),
            "matching_check": lambda h: peak_check_matching_usage(h, resource),
            "job": lambda t: peak_job(t, resource),
        }, marks=pytest.mark.skip),
        # }),

        "SQUIDsBoth": {
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",
                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "SUM:SQUIDs, PEAK:SQUIDsMemory",
                "START": "SQUIDsMonitorData == true",
            },
            "monitor": both_monitor_script(resource, resources, sequences),
            "uptime_check": lambda c, h: both_check_correct_uptimes(c, h, resource, resources, sequences),
            "matching_check": lambda h: both_check_matching_usage(h, resource),
            "job": lambda t: peak_job(t, resource),
        },
    }
)
def the_config(request):
    return request.param


@config
def discovery_script():
    return format_script(discovery_script_for(resource, resources))


@config
def the_job(the_config):
    return the_config["job"]


@config
def slot_config(the_config):
    return the_config["config"]


@config
def monitor_script(the_config):
    return format_script(the_config["monitor"])


@config
def uptime_check(the_config):
    return the_config["uptime_check"]


@config
def matching_check(the_config):
    return the_config["matching_check"]


@config
def num_resources():
    return len(resources)


@standup
def condor(test_dir, slot_config, discovery_script, monitor_script):
    write_file(test_dir / "discovery.py", discovery_script)
    write_file(test_dir / "monitor.py", monitor_script)

    with Condor(
        local_dir=test_dir / "condor",
        config={**slot_config, "TEST_DIR": test_dir.as_posix()},
    ) as condor:
        # Ornithology will run condor_who to verify that all the daemons are running,
        # but occasionally, not all 16 slots will have made it to the collector

        loop_count = 0
        while 16 != len(condor.status(ad_type=htcondor.AdTypes.Startd, projection=["SlotID"])):
            loop_count = loop_count + 1
            assert(loop_count < 20)
            time.sleep(1)
        yield condor


@action
def handle(test_dir, condor, num_resources, the_job):
    handle = condor.submit(
        description=the_job(test_dir),
        count=num_resources * 2,
    )

    # we must wait for both the handle and the job queue here,
    # because we want to use both later
    assert(handle.wait(verbose=True, timeout=240))
    assert(condor.job_queue.wait_for_job_completion(handle.job_ids))

    yield handle

    handle.remove()


@action
def num_jobs_running_history(condor, handle, num_resources):
    return track_quantity(
        condor.job_queue.filter(lambda j, e: j in handle.job_ids),
        increment_condition=lambda id_event: id_event[-1]
            == SetJobStatus(JobStatus.RUNNING),
        decrement_condition=lambda id_event: id_event[-1]
            == SetJobStatus(JobStatus.COMPLETED),
        max_quantity=num_resources,
        expected_quantity=num_resources,
    )


@action
def startd_log_file(condor):
    return condor.startd_log.open()


@action
def num_busy_slots_history(startd_log_file, handle, num_resources):
    logger.debug("Checking Startd log file...")
    logger.debug("Expected Job IDs are: {}".format(handle.job_ids))

    active_claims_history = track_quantity(
        startd_log_file.read(),
        increment_condition=lambda msg: "Changing activity: Idle -> Busy" in msg,
        decrement_condition=lambda msg: "Changing activity: Busy -> Idle" in msg,
        max_quantity=num_resources,
        expected_quantity=num_resources,
    )

    return active_claims_history


class TestCustomMachineResources:
    def test_correct_number_of_resources_assigned(self, condor, num_resources):
        result = condor.status(
            ad_type=htcondor.AdTypes.Startd, projection=["SlotID", f"Assigned{resource}s"]
        )

        # if a slot doesn't have a resource, it simply has no entry in its ad
        assert len([ad for ad in result if f"Assigned{resource}s" in ad]) == num_resources

    def test_num_jobs_running_hits_num_resources(
        self, num_jobs_running_history, num_resources
    ):
        assert num_resources in num_jobs_running_history

    def test_never_more_jobs_running_than_num_resources(
        self, num_jobs_running_history, num_resources
    ):
        assert max(num_jobs_running_history) <= num_resources

    def test_num_busy_slots_hits_num_resources(
        self, num_busy_slots_history, num_resources
    ):
        assert num_resources in num_busy_slots_history

    def test_never_more_busy_slots_than_num_resources(
        self, num_busy_slots_history, num_resources
    ):
        assert max(num_busy_slots_history) <= num_resources

    def test_correct_uptimes_from_monitor(self, condor, handle, uptime_check):
        uptime_check(condor, handle)

    def test_reported_usage_in_job_ads_and_event_log_match(self, handle, matching_check):
        matching_check(handle)
