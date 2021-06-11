#!/usr/bin/env pytest

import logging

import htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor
)

from libcmr import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


monitor_period = 5
resources = {
    "SQUID": 4,
    "TAKO": 4,
}
usages = {
    "SQUID": [5, 1, 9, 4],
    "TAKO": [500, 100, 900, 400],
}
peaks = {
    "SQUID": [
        [ 51, 51, 91, 11, 41, 41 ],
        [ 42, 42, 92, 12, 52, 52 ],
        [ 53, 53, 13, 93, 43, 43 ],
        [ 44, 44, 14, 94, 54, 54 ],
    ],
    "TAKO": [
        [ 5100, 5100, 9100, 1100, 4100, 4100 ],
        [ 4200, 4200, 9200, 1200, 5200, 5200 ],
        [ 5300, 5300, 1300, 9300, 4300, 4300 ],
        [ 4400, 4400, 1400, 9400, 5400, 5400 ],
    ],
}


@config(
    params={
        "SQUIDsAndTAKOsUsageAndMemory": {
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",

                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/SQUID-discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/SQUID-monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "SUM:SQUIDs, PEAK:SQUIDsMemory",

                "MACHINE_RESOURCE_INVENTORY_TAKOs": "$(TEST_DIR)/TAKO-discovery.py",
                "STARTD_CRON_TAKOs_MONITOR_EXECUTABLE": "$(TEST_DIR)/TAKO-monitor.py",
                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) TAKOs_MONITOR",
                "STARTD_CRON_TAKOs_MONITOR_MODE": "periodic",
                "STARTD_CRON_TAKOs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_TAKOs_MONITOR_METRICS": "SUM:TAKOs, PEAK:TAKOsMemory",
            },
        },
    }
)
def the_config(request):
    return request.param


@config
def slot_config(the_config):
    return the_config["config"]


@standup
def condor(test_dir, slot_config):
    for resource in resources.keys():
        sequence = { f"{resource}{i}": j for i, j in enumerate(usages[resource]) }
        discovery_script = format_script(discovery_script_for(resource, sequence))
        write_file(test_dir / f"{resource}-discovery.py", discovery_script)

        sequences = { f"{resource}{i}": j for i, j in enumerate(peaks[resource]) }
        monitor_script = both_monitor_script(resource, sequence, sequences)
        write_file(test_dir / f"{resource}-monitor.py", monitor_script)

    with Condor(
        local_dir=test_dir / "condor",
        config={**slot_config, "TEST_DIR": test_dir.as_posix()},
    ) as condor:
        yield condor


class TestCustomMachineResources:

    def test_correct_number_of_resources_assigned(self, condor):
        for resource, number in resources.items():
            print(f"{resource} {number}\n")

            result = condor.status(
                ad_type=htcondor.AdTypes.Startd, projection=["SlotID", f"Assigned{resource}s"]
            )

            assert len([ad for ad in result if f"Assigned{resource}s" in ad]) == number
