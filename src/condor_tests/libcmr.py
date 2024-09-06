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


def discovery_script_for(resource, resources):
    script = """
    #!/usr/bin/python3

    print('Detected{resource}s="{list}"')
    """.format(
        resource=resource,
        list=", ".join(resources.keys())
    )
    return script


#
# Define the job, monitor, and checks for SUM metrics.
#


def sum_job(test_dir, monitor_period, resource):
    return {
                "executable": "/bin/sleep",
                "arguments": str((monitor_period * 3) + 2),
               f"request_{resource}s": "1",
                "log": (test_dir / "events.log").as_posix(),
                "LeaveJobInQueue": "true",
    }


def sum_monitor_script(resource, resources):
    return "#!/usr/bin/python3\n" + "".join(
        textwrap.dedent(
            """
            print('SlotMergeConstraint = StringListMember( "{name}", Assigned{resource}s )')
            print('Uptime{resource}sSeconds = {increment}')
            print('- {name}')
            """.format(name=name, increment=increment, resource=resource)
        ) for name, increment in resources.items()
    ) + f"print('{resource}sMonitorData = true\\n-GLOBAL update:true')"


def sum_check_correct_uptimes(condor, handle, resource, resources):
    #
    # See HTCONDOR-472 for an extended discussion.
    #

    direct = condor.direct_status(
        htcondor.DaemonTypes.Startd,
        htcondor.AdTypes.Startd,
        constraint=f"Assigned{resource}s =!= undefined",
        projection=["SlotID", f"Assigned{resource}s", f"Uptime{resource}sSeconds"],
    )

    try:
      measured_uptimes = set(int(ad[f"Uptime{resource}sSeconds"]) for ad in direct)
    except KeyError:
      # There's a race condition in the test where rarely we haven't gotten
      # the update with the value in it, in which case there is no
      # Uptime{resource}sSeconds in the add.  Just skip in that case.
      return 
    logger.info(f"Measured uptimes were {measured_uptimes} (may be out of order)")

    increments = {}
    assigned_resources_set = set(ad[f"Assigned{resource}s"] for ad in direct)
    if all( rs.count(",") == 0 for rs in assigned_resources_set):
        increments = resources
    else:
        for assigned_resources in assigned_resources_set:
            the_sum = 0
            names = assigned_resources.split(",")
            for name in names:
                the_sum += resources[name]

            increments[assigned_resources] = the_sum
    logger.info(f"Allowed increments are {increments} (may be out of order)")

    # the uptimes are increasing over time, so we
    # assert that we have some reasonable multiple of the increments being
    # emitted by the monitor script
    assert any(
        {multiplier * u for u in increments.values()} == measured_uptimes
        for multiplier in range(2, 100)
    )


def sum_check_matching_usage(handle, resource):
    terminated_events = handle.event_log.filter(
        lambda e: e.type is htcondor.JobEventType.JOB_TERMINATED
    )
    ads = handle.query(projection=["ClusterID", "ProcID", f"{resource}sAverageUsage"])

    # make sure we got the right number of terminate events and ads
    # before doing the real assertion
    assert len(terminated_events) == len(ads)
    assert len(ads) == len(handle)

    jobid_to_usage_via_event = {
        JobID.from_job_event(event): event[f"{resource}sUsage"]
        for event in sorted(terminated_events, key=lambda e: e.proc)
    }

    jobid_to_usage_via_ad = {
        JobID.from_job_ad(ad): round(ad[f"{resource}sAverageUsage"], 2)
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


def peak_job_script(resource):
    return format_script( "#!/usr/bin/python3\n" + textwrap.dedent(f"""
        import os
        import sys
        import time

        elapsed = 0;
        while elapsed < int(sys.argv[1]):
            os.system('condor_status -ads ${{_CONDOR_SCRATCH_DIR}}/.update.ad -af Assigned{resource}s {resource}sMemoryUsage')
            time.sleep(1)
            elapsed += 1
        """)
    )


def peak_job(test_dir, resource):
    script_file = (test_dir / "poll-memory.py")
    write_file(script_file, peak_job_script(resource))

    return {
                "executable": script_file.as_posix(),
                "arguments": "17",
               f"request_{resource}s": "1",
                "log": (test_dir / "events.log").as_posix(),
                "output": (test_dir / "poll-memory.$(Cluster).$(Process).out").as_posix(),
                "error": (test_dir / "poll-memory.$(Cluster).$(Process).err").as_posix(),
                "getenv": "true",
                "LeaveJobInQueue": "true",
    }


def peak_monitor_script(resource, sequences):
    return "#!/usr/bin/python3\n" + textwrap.dedent("""
        import math
        import time

        """ + f"resource = '{resource}'" + """
        """ + f"sequences = {sequences}" + """

        positionInSequence = math.floor((time.time() % 60) / 10);
        for index in sequences.keys():
            usage = sequences[index][positionInSequence]
            print(f'SlotMergeConstraint = StringListMember("{index}", Assigned{resource}s)' )
            print(f'Uptime{resource}sMemoryPeakUsage = {usage}');
            print(f'- {index}')
        print(f'{resource}sMonitorData = true')
        print('-GLOBAL update:true')
        """)


def read_peaks_from_file(filename, resourcename):
    peaks = {}
    with open(filename, "r") as f:
        for line in f:
            [resource, value] = line.split()
            if value == "undefined":
                continue
            if resource != resourcename:
                continue
            if not resource in peaks:
                peaks[resource] = []
            peaks[resource].append(float(value))
    return peaks;


def peak_check_correct_uptimes(condor, handle, resource, sequences):
    #
    # First, assert that the startd reported something sane.
    #

    direct = condor.direct_status(
        htcondor.DaemonTypes.Startd,
        htcondor.AdTypes.Startd,
        constraint=f"Assigned{resource}s =!= undefined",
        projection=["SlotID", f"Assigned{resource}s", f"Device{resource}sMemoryPeakUsage"],
    )

    for ad in direct:
        assigned_resource_str = ad[f"Assigned{resource}s"]
        peak = int(ad[f"Device{resource}sMemoryPeakUsage"])
        logger.info(f"Measured peak for {assigned_resource_str} was {peak}")

        possible_peaks = []
        assigned_resources = assigned_resource_str.split(",")
        for r in assigned_resources:
            sequence = sequences[r]
            if len(possible_peaks) == 0:
                possible_peaks = sequence
            else:
                assert len(possible_peaks) == len(sequence)
                possible_peaks = [max(possible_peaks[i], sequence[i]) for i in range(len(possible_peaks))]

        logger.info(f"Possible peaks are {possible_peaks}")
        assert peak in possible_peaks

    #
    # Then assert that the periodic polling recorded by the job recorded
    # a valid subsequence for its assigned resource, and that the measured
    # peak was properly computed.
    #

    observed_peaks = {}
    ads = handle.query(projection=["ClusterID", "ProcID", "Out"])
    for ad in ads:
        observed_peaks[ad["ProcID"]] = read_peaks_from_file(ad["Out"], resource)

    logger.info(f"Obversed peaks were {observed_peaks}")

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


def peak_check_matching_usage(handle, resource):
    terminated_events = handle.event_log.filter(
        lambda e: e.type is htcondor.JobEventType.JOB_TERMINATED
    )
    ads = handle.query(projection=["ClusterID", "ProcID", f"{resource}sMemoryUsage"])

    # make sure we got the right number of terminate events and ads
    # before doing the real assertion
    assert len(terminated_events) == len(ads)
    assert len(ads) == len(handle)

    jobid_to_usage_via_event = {
        JobID.from_job_event(event): event[f"{resource}sMemoryUsage"]
        for event in sorted(terminated_events, key=lambda e: e.proc)
    }

    jobid_to_usage_via_ad = {
        JobID.from_job_ad(ad): round(ad[f"{resource}sMemoryUsage"], 2)
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


def both_check_correct_uptimes(condor, handle, resource, resources, sequences):
    sum_check_correct_uptimes(condor, handle, resource, resources)
    peak_check_correct_uptimes(condor, handle, resource, sequences)


def both_check_matching_usage(handle, resource):
    sum_check_matching_usage(handle, resource)
    peak_check_matching_usage(handle, resource)


def both_monitor_script(resource, resources, sequences):
    return sum_monitor_script(resource, resources) + "\n" + peak_monitor_script(resource, sequences);
