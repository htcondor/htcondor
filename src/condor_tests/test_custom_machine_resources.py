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

    def test_correct_uptimes_from_monitor(self, condor, resources):
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
            for multiplier in range(1000)
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

    def test_reported_usage_in_job_ads_makes_sense(self, handle, resources):
        ads = handle.query(
            projection=[
                "ClusterID",
                "ProcID",
                "AssignedXXX",
                "XXXAverageUsage",
                "RemoteWallClockTime",
            ]
        )

        # Here's the deal: XXXAverageUsage is
        #
        #   (increment amount * number of periods)
        # -----------------------------------------
        #    (monitor period * number of periods)
        #
        # BUT in practice, you usually get the monitor period wrong by a
        # second due to rounding.  What we observe is that very often,
        # some increments will be a second longer or shorter
        # than the increment period.  So we could get something like
        #
        #          (increment amount * number of periods)
        # -----------------------------------------------------------
        # (monitor period * number of periods) +/- (number of periods)
        #
        # The other complication is that we could get one more increment
        # than expected.  So we could something like
        #
        #          (increment amount * (number of periods + 1))
        # ----------------------------------------------------------------
        # (monitor period * (number of periods+1)) +/- (number of periods)
        #
        # In some cases we can actually get one fewer period than expected!

        print() # If we actually see the output, we'll need the line break.
        all_options = []
        for ad in ads:
            increment = resources[ad["AssignedXXX"]]
            usage = fractions.Fraction(float(ad["XXXAverageUsage"])).limit_denominator(
                30
            )

            print(
                "Job {}.{}, resource {}, increment {}, usage {} ({})".format(
                    ad["ClusterID"],
                    ad["ProcID"],
                    ad["AssignedXXX"],
                    increment,
                    usage,
                    float(usage),
                )
            )

            exact = [fractions.Fraction(increment, MONITOR_PERIOD)]
            dither_periods = [
                fractions.Fraction(
                    increment * NUM_PERIODS,
                    ((MONITOR_PERIOD * NUM_PERIODS) + extra_periods),
                )
                # range() is not inclusive.
                for extra_periods in range(-1, +3 + 1)
            ]
            extra_period = [
                fractions.Fraction(
                    increment * (NUM_PERIODS + 1),
                    ((MONITOR_PERIOD * (NUM_PERIODS + 1)) + extra_periods),
                )
                # range is not inclusive
                for extra_periods in range(-1, +3 + 1)
            ]
            two_extra_period = [
                fractions.Fraction(
                    increment * (NUM_PERIODS + 2),
                    ((MONITOR_PERIOD * (NUM_PERIODS + 2)) + extra_periods),
                )
                # range is not inclusive
                for extra_periods in range(-1, +3 + 1)
            ]
            missed_period = [
                fractions.Fraction(
                    increment * (NUM_PERIODS - 1),
                    ((MONITOR_PERIOD * (NUM_PERIODS - 1)) + extra_periods),
                )
                # range is not inclusive
                for extra_periods in range(-1, +3 + 1)
            ]

            print(
                "*" if usage in exact else " ",
                "exact".ljust(25),
                ",".join(str(f) for f in exact),
            )
            print(
                "*" if usage in dither_periods else " ",
                "dither periods".ljust(25),
                ",".join(str(f) for f in dither_periods),
            )
            print(
                "*" if usage in extra_period else " ",
                "dither, extra increment".ljust(25),
                ",".join(str(f) for f in extra_period),
            )
            print(
                "*" if usage in two_extra_period else " ",
                "dither, two extra increments".ljust(25),
                ",".join(str(f) for f in two_extra_period),
            )
            print(
                "*" if usage in missed_period else " ",
                "dither, missed increment".ljust(25),
                ",".join(str(f) for f in missed_period),
            )
            print()

            # build the list of possibilities here, but delay assertions until we've printed all the debug messages
            all_options.append(exact + dither_periods + extra_period + two_extra_period + missed_period)

        assert all(
            fractions.Fraction(float(ad["XXXAverageUsage"])).limit_denominator(30)
            in options
            for ad, options in zip(ads, all_options)
        )

