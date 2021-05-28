#!/usr/bin/env pytest

import logging

import time
import textwrap
import itertools

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

# FIXME: Remove when we have a blocking, deterministic method for
# detecting when the start has done discovery.
MONITOR_PERIOD = 5


# make this a fixture again if possible
resources = {"SQUID0": 1, "SQUID1": 4, "SQUID2": 5, "SQUID3": 9}


@config
def discovery_script():
    return format_script(
        """
        #!/usr/bin/python3

        print('DetectedSQUIDs="{res}"')
        """.format(
            res=", ".join(resources.keys())
        )
    )


def sum_monitor_script():
    return "#!/usr/bin/python3\n" + "".join(
        textwrap.dedent(
            """
            print('SlotMergeConstraint = StringListMember( "{name}", AssignedSQUIDs )')
            print('UptimeSQUIDsSeconds = {increment}')
            print('- {name}')
            """.format(name=name, increment=increment)
        ) for name, increment in resources.items()
    )


# Make this a fixture if possible.
sequences = {
    "SQUID0": [ 51, 51, 91, 11, 41, 41 ],
    "SQUID1": [ 42, 42, 92, 12, 52, 52 ],
    "SQUID2": [ 53, 53, 13, 93, 43, 43 ],
    "SQUID3": [ 44, 44, 14, 94, 54, 54 ],
};


def peak_monitor_script():
    return "#!/usr/bin/python3\n" + textwrap.dedent("""
        import math
        import time

        """ + f"sequences = {sequences}" + """

        positionInSequence = math.floor((time.time() % 60) / 10);
        for index in ["SQUID0", "SQUID1", "SQUID2", "SQUID3"]:
            usage = sequences[index][positionInSequence]
            print(f'SlotMergeConstraint = StringListMember("{index}", AssignedSQUIDs)' )
            print(f'UptimeSQUIDsMemoryPeakUsage = {usage}');
            print(f'- {index}')
        """)


def sum_check_correct_uptimes(condor, handle):
    #
    # See HTCONDOR-472 for an extended discussion.
    #

    direct = condor.direct_status(
        htcondor.DaemonTypes.Startd,
        htcondor.AdTypes.Startd,
        constraint="AssignedSQUIDs =!= undefined",
        projection=["SlotID", "AssignedSQUIDs", "UptimeSQUIDsSeconds"],
    )

    measured_uptimes = set(int(ad["UptimeSQUIDsSeconds"]) for ad in direct)

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


def sum_check_matching_usage(handle):
    terminated_events = handle.event_log.filter(
        lambda e: e.type is htcondor.JobEventType.JOB_TERMINATED
    )
    ads = handle.query(projection=["ClusterID", "ProcID", "SQUIDsAverageUsage"])

    # make sure we got the right number of terminate events and ads
    # before doing the real assertion
    assert len(terminated_events) == len(ads)
    assert len(ads) == len(handle)

    jobid_to_usage_via_event = {
        JobID.from_job_event(event): event["SQUIDsUsage"]
        for event in sorted(terminated_events, key=lambda e: e.proc)
    }

    jobid_to_usage_via_ad = {
        JobID.from_job_ad(ad): round(ad["SQUIDsAverageUsage"], 2)
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


def read_peaks_from_file(filename):
    peaks = {}
    with open(filename, "r") as f:
        for line in f:
            [resource, value] = line.split()
            if value == "undefined":
                continue
            if not resource in peaks:
                peaks[resource] = []
            peaks[resource].append(float(value))
    return peaks;

def peak_check_correct_uptimes(condor, handle):
    #
    # First, assert that the startd reported something sane.
    #

    direct = condor.direct_status(
        htcondor.DaemonTypes.Startd,
        htcondor.AdTypes.Startd,
        constraint="AssignedSQUIDs =!= undefined",
        projection=["SlotID", "AssignedSQUIDs", "DeviceSQUIDsMemoryPeakUsage"],
    )

    for ad in direct:
        resource = ad["AssignedSQUIDs"]
        peak = int(ad["DeviceSQUIDsMemoryPeakUsage"])
        logger.info(f"Measured peak for {resource} was {peak}")
        assert peak in sequences[resource]

    #
    # Then assert that the periodic polling recorded by the job recorded
    # a valid subsequence for its assigned resource, and that the measured
    # peak was properly computed.
    #

    observed_peaks = {}
    ads = handle.query(projection=["ClusterID", "ProcID", "Out"])
    for ad in ads:
        observed_peaks[ad["ProcID"]] = read_peaks_from_file(ad["Out"])

    logger.info(f"Obversed peaks were {observed_peaks}")

    # Assert that we only logged one resource for each job.
    for ad in ads:
        assert len(observed_peaks[ad["ProcID"]]) == 1

    # Assert that the observed peaks are valid for their resource.
    for ad in ads:
        for resource in observed_peaks[ad["ProcID"]]:
            values = sequences[resource]
            observed = observed_peaks[ad["ProcID"]][resource]
            assert all([value in values for value in observed])

    # Assert that the observed peaks are all monotonically increasing.
    for ad in ads:
        for sequence in observed_peaks[ad["ProcID"]].values():
            for i in range(0, len(sequence) - 1):
                assert sequence[i] <= sequence[i+1]


def peak_check_matching_usage(handle):
    terminated_events = handle.event_log.filter(
        lambda e: e.type is htcondor.JobEventType.JOB_TERMINATED
    )
    ads = handle.query(projection=["ClusterID", "ProcID", "SQUIDsMemoryUsage"])

    # make sure we got the right number of terminate events and ads
    # before doing the real assertion
    assert len(terminated_events) == len(ads)
    assert len(ads) == len(handle)

    jobid_to_usage_via_event = {
        JobID.from_job_event(event): event["SQUIDsMemoryUsage"]
        for event in sorted(terminated_events, key=lambda e: e.proc)
    }

    jobid_to_usage_via_ad = {
        JobID.from_job_ad(ad): round(ad["SQUIDsMemoryUsage"], 2)
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


def sum_job(test_dir):
    return {
                "executable": "/bin/sleep",
                "arguments": "17",
                "request_SQUIDs": "1",
                "log": (test_dir / "events.log").as_posix(),
                "LeaveJobInQueue": "true",
    }


def peak_job_script():
    return format_script( "#!/usr/bin/python3\n" + textwrap.dedent("""
        import os
        import sys
        import time

        elapsed = 0;
        while elapsed < int(sys.argv[1]):
            os.system('condor_status -ads ${_CONDOR_SCRATCH_DIR}/.update.ad -af AssignedSQUIDs SQUIDsMemoryUsage')
            time.sleep(1)
            elapsed += 1
        """)
    )


def peak_job(test_dir):
    script_file = (test_dir / "poll-memory.py")
    write_file(script_file, peak_job_script())

    return {
                "executable": script_file.as_posix(),
                "arguments": "17",
                "request_SQUIDs": "1",
                "log": (test_dir / "events.log").as_posix(),
                "output": (test_dir / "poll-memory.$(Cluster).$(Process).out").as_posix(),
                "error": (test_dir / "poll-memory.$(Cluster).$(Process).err").as_posix(),
                "getenv": "true",
                "LeaveJobInQueue": "true",
    }


@config(
    params={
        "SQUIDsUsage": {
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",
                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(MONITOR_PERIOD),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "SUM:SQUIDs",
            },
            "monitor": sum_monitor_script(),
            "uptime_check": sum_check_correct_uptimes,
            "matching_check": sum_check_matching_usage,
            "job": sum_job,
        },

        "SQUIDsMemory": {
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",
                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(MONITOR_PERIOD),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "PEAK:SQUIDsMemory",
            },
            "monitor": peak_monitor_script(),
            "uptime_check": peak_check_correct_uptimes,
            "matching_check": peak_check_matching_usage,
            "job": peak_job,
        },
    }
)
def the_config(request):
    return request.param


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

    assert(False)

    with Condor(
        local_dir=test_dir / "condor",
        config={**slot_config, "TEST_DIR": test_dir.as_posix()},
    ) as condor:
        yield condor


@action
def handle(test_dir, condor, num_resources, the_job):
    handle = condor.submit(
        description=the_job(test_dir),
        count=num_resources * 2,
    )

    # we must wait for both the handle and the job queue here,
    # because we want to use both later
    assert(handle.wait(verbose=True, timeout=180))
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
            ad_type=htcondor.AdTypes.Startd, projection=["SlotID", "AssignedSQUIDs"]
        )

        # if a slot doesn't have a resource, it simply has no entry in its ad
        assert len([ad for ad in result if "AssignedSQUIDs" in ad]) == num_resources

    def test_correct_uptimes_from_monitor(self, condor, handle, uptime_check):
        uptime_check(condor, handle)

    def test_never_more_jobs_running_than_num_resources(
        self, num_jobs_running_history, num_resources
    ):
        assert max(num_jobs_running_history) <= num_resources

    def test_num_jobs_running_hits_num_resources(
        self, num_jobs_running_history, num_resources
    ):
        assert num_resources in num_jobs_running_history

    def test_never_more_busy_slots_than_num_resources(
        self, num_busy_slots_history, num_resources
    ):
        assert max(num_busy_slots_history) <= num_resources

    def test_num_busy_slots_hits_num_resources(
        self, num_busy_slots_history, num_resources
    ):
        assert num_resources in num_busy_slots_history

    def test_reported_usage_in_job_ads_and_event_log_match(self, handle, matching_check):
        matching_check(handle)
