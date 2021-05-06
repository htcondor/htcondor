#!/usr/bin/env pytest

# this test replicates cmr-monitor-basic

import logging

import textwrap
import fractions
import time

import htcondor

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

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# TODO: these are hard-coded based on the parameters below
# TODO: should be possible to un-hard-code them...
MONITOR_PERIOD = 5
NUM_PERIODS = 3


@config(
    params={
        "static_slots": {
            "NUM_CPUS": "16",
            "NUM_SLOTS": "16",
            "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",
            "MACHINE_RESOURCE_INVENTORY_XXX": "$(TEST_DIR)/discovery.py",
            "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) XXX_MONITOR",
            "STARTD_CRON_XXX_MONITOR_MODE": "WaitForExit",
            "STARTD_CRON_XXX_MONITOR_PERIOD": "1",
            "STARTD_CRON_XXX_MONITOR_EXECUTABLE": "$(TEST_DIR)/monitor.py",
            "STARTD_CRON_XXX_MONITOR_METRICS": "SUM:XXX",
        }
    }
)
def slot_config(request):
    return request.param


@config(params={"one_resource_type": {"XXX0": 1, "XXX1": 4, "XXX2": 5, "XXX3": 9}})
def resources(request):
    return request.param


@config
def discovery_script(resources):
    return format_script(
        """
        #!/usr/bin/python3

        print('DetectedXXX="{res}"')
        """.format(
            res=", ".join(resources.keys())
        )
    )


@config
def monitor_script(resources):
    return format_script(
        "#!/usr/bin/python3\n" +
        "import time\n" +
        "while True:\n" +
        textwrap.indent("".join(
            textwrap.dedent(
            """
            print('SlotMergeConstraint = StringListMember( "{name}", AssignedXXX )')
            print('UptimeXXXSeconds = {increment}')
            print('- {name}', flush=True)
            """.format(name=name, increment=increment)
            )
            for name, increment in resources.items()
        ), "    ") +
           "    "  + "time.sleep(5)\n"
    )


@config
def num_resources(resources):
    return len(resources)


@standup
def condor(test_dir, slot_config, discovery_script, monitor_script):
    write_file(test_dir / "discovery.py", discovery_script)
    write_file(test_dir / "monitor.py", monitor_script)

    with Condor(
        local_dir=test_dir / "condor",
        config={**slot_config, "TEST_DIR": test_dir.as_posix()},
    ) as condor:
        # FIXME -- make this deterministic or at least abort testing if not
        # try to make sure the monitor runs before we continue with the test
        time.sleep(MONITOR_PERIOD * 1.5)
        yield condor


@action
def handle(test_dir, condor, num_resources):
    handle = condor.submit(
        description={
            "executable": "/bin/sleep",
            "arguments": "17",
            "request_XXX": "1",
            "log": (test_dir / "events.log").as_posix(),
            "LeaveJobInQueue": "true",
        },
        count=num_resources * 2,
    )

    # we must wait for both the handle and the job queue here,
    # because we want to use both later
    handle.wait(verbose=True)
    condor.job_queue.wait_for_job_completion(handle.job_ids)

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
            ad_type=htcondor.AdTypes.Startd, projection=["SlotID", "AssignedXXX"]
        )

        # if a slot doesn't have a resource, it simply has no entry in its ad
        assert len([ad for ad in result if "AssignedXXX" in ad]) == num_resources

    #
    # See HTCONDOR-472 for an extended discussion.
    #
    def test_correct_uptimes_from_monitor(self, condor, resources, handle):
        direct = condor.direct_status(
            htcondor.DaemonTypes.Startd,
            htcondor.AdTypes.Startd,
            constraint="AssignedXXX =!= undefined",
            projection=["SlotID", "AssignedXXX", "UptimeXXXSeconds"],
        )

        measured_uptimes = set(int(ad["UptimeXXXSeconds"]) for ad in direct)

        logger.info(
            "Measured uptimes were {}, expected multiples of {} (not necessarily in order)".format(
                measured_uptimes, resources.values()
            )
        )

        # the uptimes are increasing over time, so we
        # assert that we have some reasonable multiple of the increments being
        # emitted by the monitor script
        assert any(
            {multiplier * u for u in resources.values()} == measured_uptimes
            for multiplier in range(2, 100)
        )

    def test_never_more_jobs_running_than_num_resources(
        self, num_jobs_running_history, num_resources
    ):
        assert max(num_jobs_running_history) <= num_resources

    def test_num_jobs_running_hits_num_resources(
        self, num_jobs_running_history, resources
    ):
        num_resources = len(resources)
        assert num_resources in num_jobs_running_history

    def test_never_more_busy_slots_than_num_resources(
        self, num_busy_slots_history, num_resources
    ):
        assert max(num_busy_slots_history) <= num_resources

    def test_num_busy_slots_hits_num_resources(
        self, num_busy_slots_history, num_resources
    ):
        assert num_resources in num_busy_slots_history

    def test_reported_usage_in_job_ads_and_event_log_match(self, handle):
        terminated_events = handle.event_log.filter(
            lambda e: e.type is htcondor.JobEventType.JOB_TERMINATED
        )
        ads = handle.query(projection=["ClusterID", "ProcID", "XXXAverageUsage"])

        # make sure we got the right number of terminate events and ads
        # before doing the real assertion
        assert len(terminated_events) == len(ads) == len(handle)

        jobid_to_usage_via_event = {
            JobID.from_job_event(event): event["XXXUsage"]
            for event in sorted(terminated_events, key=lambda e: e.proc)
        }

        jobid_to_usage_via_ad = {
            JobID.from_job_ad(ad): round(ad["XXXAverageUsage"], 2)
            for ad in sorted(ads, key=lambda ad: ad["ProcID"])
        }

        logger.debug(
            "Custom resource usage from job event log: {}".format(
                jobid_to_usage_via_event
            )
        )
        logger.debug(
            "Custom resource usage from job ads: {}".format(jobid_to_usage_via_ad)
        )

        assert jobid_to_usage_via_ad == jobid_to_usage_via_event
