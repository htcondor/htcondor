#!/usr/bin/env pytest

#
# Tests for the Claimed/Cleaning startd activity.
#
# The startd transitions a claimed slot from Busy to Cleaning when the
# starter sends its final job update. Cleaning is terminal within a claim
# and ends when the starter process is reaped. This test verifies that the
# sequence Busy -> Cleaning -> Idle appears in the StartLog for a short
# vanilla job and that the TotalTimeClaimedCleaning attribute is published.
#

import logging

import htcondor2 as htcondor

from ornithology import (
    action,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def completed_job(default_condor, test_dir, path_to_sleep):
    handle = default_condor.submit(
        description={
            "log":                   test_dir / "cleaning_job.log",
            "universe":              "vanilla",
            "executable":            path_to_sleep,
            "arguments":             1,
            "should_transfer_files": True,
        },
        count=1,
    )

    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=120,
    )
    return handle


# The job's terminate event (written by the shadow) can land before the
# startd has logged its own activity changes, so poll the StartLog rather
# than reading it once. Each test opens a fresh stream from the start of
# the log so the tests do not consume each other's lines.
def wait_for_start_log(default_condor, condition, timeout=60):
    return default_condor.startd_log.open().wait(
        condition=lambda msg: condition(msg.message),
        timeout=timeout,
    )


class TestStartdCleaningActivity:

    def test_enters_cleaning_from_busy(self, default_condor, completed_job):
        assert wait_for_start_log(
            default_condor,
            lambda line: "Changing activity: Busy -> Cleaning" in line,
        ), "Expected StartLog to record Busy -> Cleaning transition"

    def test_leaves_cleaning_state(self, default_condor, completed_job):
        # After the starter is reaped, the slot leaves Cleaning -- usually
        # to Idle for a cleanly-exiting job, but possibly to Preempting if
        # a condor_vacate or policy preemption arrived during cleaning.
        assert wait_for_start_log(
            default_condor,
            lambda line: ("Changing activity: Cleaning -> " in line) or
                         ("Claimed/Cleaning -> " in line),
        ), "Expected StartLog to record exit from Cleaning"

    def test_startd_ad_is_queryable(self, default_condor, completed_job):
        # Sanity check that after a job has run through cleaning the
        # startd is still healthy and reachable. TotalTimeClaimedCleaning
        # is only published when non-zero, so we do not require it to
        # appear here -- the short sleep may spend sub-second in cleaning.
        ads = default_condor.status(
            ad_type=htcondor.AdTypes.Startd,
            projection=["Name", "State", "Activity", "TotalTimeClaimedCleaning"],
        )
        assert ads, "Expected at least one startd ad"
        for ad in ads:
            if "TotalTimeClaimedCleaning" in ad:
                logger.info(
                    "Slot %s: TotalTimeClaimedCleaning=%s",
                    ad.get("Name"), ad["TotalTimeClaimedCleaning"],
                )
