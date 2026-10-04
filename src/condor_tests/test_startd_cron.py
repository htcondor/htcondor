#!/usr/bin/env pytest

#
# Test startd cron and STARTD_CRON_LOG_NON_ZERO_EXIT

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
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


goodcronscript = """#!/usr/bin/env python3
import sys
print("GoodCron = 123\\n")
sys.stderr.write("GoodCron writes to stderr\\n")
sys.exit(0)
"""


badcronscript = """#!/usr/bin/env python3
import sys
print("BadCron = 123\\n")
sys.stderr.write("BadCron writes to stderr\\n")
sys.exit(37)
"""


@standup
def cronscripts(test_dir):
    write_file(test_dir / "goodcron.py", goodcronscript)
    write_file(test_dir / "badcron.py",  badcronscript)


@standup
def condor(test_dir, cronscripts):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "STARTD_CRON_LOG_NON_ZERO_EXIT": True,
            "STARTD_CRON_JOBLIST": "BADCRON GOODCRON",
            "STARTD_CRON_GOODCRON_EXECUTABLE": f"{test_dir}/goodcron.py",
            "STARTD_CRON_GOODCRON_MODE": "Oneshot",
            "STARTD_CRON_BADCRON_EXECUTABLE": f"{test_dir}/badcron.py",
            "STARTD_CRON_BADCRON_MODE": "Oneshot",
        },
    ) as condor:
        yield condor


@action
def check_logs(condor, test_dir):
    # Wait until we see the cron job in the collector, to avoid races
    ready = False
    while not ready:
        results = condor.status(ad_type=htcondor.AdTypes.Startd, projection=["GoodCron", "BadCron"])
        if "BadCron" in results[0]:
            ready = True
        else:
            time.sleep(2)

    with open(test_dir / "condor/log/StartLog") as f:
        found_bad_cron = False
        for line in f:
            assert("GoodCron writes to stderr" not in line)
            if ("BadCron writes to stderr" in line):
                found_bad_cron = True
    return found_bad_cron


@action
def startd_ads(condor):
    # Query the collector rather than the startd directly (condor_who),
    # which can stall on Windows.  The first startd update can go out
    # before the cron jobs finish, so poll until both attributes appear.
    deadline = time.time() + 60
    while True:
        results = condor.status(ad_type=htcondor.AdTypes.Startd, projection=["GoodCron", "BadCron"])
        if results and "GoodCron" in results[0] and "BadCron" in results[0]:
            return results[0]
        if time.time() > deadline:
            return results[0] if results else {}
        time.sleep(1)


class TestStartdCron:

    def test_startd_cron_logs(self, check_logs):
        assert check_logs

    def test_startd_cron_ads(self, startd_ads):
        assert startd_ads.get("GoodCron") == 123
        # This is counter-intuitive, but we don't have a knob to fix it.
        assert startd_ads.get("BadCron") == 123
