import logging
import textwrap

import htcondor

from ornithology import (
    write_file,
    JobID,
    format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


#
# Define the discovery script.
#


def discovery_script_for(resources):
    script = """
    #!/usr/bin/python3

    print('DetectedSQUIDs="{res}"')
    """.format(
        res=", ".join(resources.keys())
    )
    return script


#
# Define the job, monitor, and checks for SUM metrics.
#


def sum_job(test_dir, monitor_period):
    return {
                "executable": "/bin/sleep",
                "arguments": str((monitor_period * 3) + 2),
                "request_SQUIDs": "1",
                "log": (test_dir / "events.log").as_posix(),
                "LeaveJobInQueue": "true",
    }


def sum_monitor_script(resources):
    return "#!/usr/bin/python3\n" + "".join(
        textwrap.dedent(
            """
            print('SlotMergeConstraint = StringListMember( "{name}", AssignedSQUIDs )')
            print('UptimeSQUIDsSeconds = {increment}')
            print('- {name}')
            """.format(name=name, increment=increment)
        ) for name, increment in resources.items()
    )


def sum_check_correct_uptimes(condor, handle, resources):
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


#
# Define the job, monitor, and checks for PEAK metrics.
#


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


def peak_monitor_script(sequences):
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


def peak_check_correct_uptimes(condor, handle, sequences):
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


#
# Define the job, monitor, and checks for simultaneous SUM and PEAK metrics.
#


def both_check_correct_uptimes(condor, handle, resources, sequences):
    sum_check_correct_uptimes(condor, handle, resources)
    peak_check_correct_uptimes(condor, handle, sequences)


def both_check_matching_usage(handle):
    sum_check_matching_usage(handle)
    peak_check_matching_usage(handle)


def both_monitor_script(resources, sequences):
    return sum_monitor_script(resources) + "\n" + peak_monitor_script(sequences);
