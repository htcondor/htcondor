#!/usr/bin/env pytest

import os
import time
import logging
import htcondor2 as htcondor

from ornithology import (
    action,
    config,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def reported_disk_space(test_dir):
    with Condor(test_dir / "condor", { "RESERVED_DISK": "3145728" }) as condor:
        deadline = time.time() + 30
        while True:
            result = condor.status(
                ad_type=htcondor.AdTypes.Startd,
                projection=["TotalDisk"],
            )
            if result and "TotalDisk" in result[0]:
                return result[0]["TotalDisk"]
            assert time.time() < deadline, "startd slot ad never reached collector"
            time.sleep(1)

@action
def actual_disk_space(test_dir):
    stats = os.statvfs(test_dir.as_posix())
    return int(stats.f_bfree/1024.0 * stats.f_bsize)

class TestReserveDisk:
    def test_reserve_disk(self, actual_disk_space, reported_disk_space):
        assert(reported_disk_space < actual_disk_space)
