#!/usr/bin/env pytest

import htcondor2

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    action,
    ClusterState,
    Condor,
)


@action
def job_submitted(default_condor, test_dir):
    job_description = {
        "shell":            "sleep 180",
        "request_CPUs":     1,
        "request_memory":   1,
        "request_disk":     1,
        "log":              test_dir / "job_submitted.log",
    }

    job_handle = default_condor.submit(
        description=job_description,
        count=1
    )

    # Make sure the accountant has something to work with.
    job_handle.wait(
        timeout=120,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_terminal,
    )

    return job_handle

class TestPyPrio:

    def test_all_attributes(self, default_condor, job_submitted):
        with default_condor.use_config():
            n = htcondor2.Negotiator()
        r = n.getPriorities()

        missing = 0
        present = 0
        for ad in r:
            print(ad)
            floor = ad.get('Floor', "None")
            if floor == "None":
                missing += 1
            elif floor is not None:
                present += 1

        assert missing >= 1
        assert present >= 1
