#!/usr/bin/env pytest

#
# Test CurrentWastedMemoryGBHours and LastWastedMemoryGBHours
# (HTCONDOR-4062).  CurrentWastedMemoryGBHours is updated as the job runs to
#
#   ((ActivationRequestMemory - MemoryUsage) / 1024) * (activation time so far / 3600)
#
# clamped to zero, and set to its final value when the activation ends, at
# which point it is also copied to LastWastedMemoryGBHours.  To make
# MemoryUsage deterministic, we configure the starter's MEMORY_USAGE_METRIC
# to use the job attribute TestMemoryUsage when the job defines it.
#

import time
import logging

import pytest

import htcondor2 as htcondor

from ornithology import (
    standup, action,
    Condor, ClusterState,
    write_file, format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

CURRENT = "CurrentWastedMemoryGBHours"
LAST = "LastWastedMemoryGBHours"


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "MEMORY_USAGE_METRIC": "ifThenElse(isUndefined(TestMemoryUsage), ((ResidentSetSize+1023)/1024), TestMemoryUsage)",
            # So we can watch CurrentWastedMemoryGBHours change while
            # the job runs.
            "STARTER_INITIAL_UPDATE_INTERVAL": 1,
            "STARTER_UPDATE_INTERVAL": 2,
            "SHADOW_QUEUE_UPDATE_INTERVAL": 2,
        },
    ) as condor:
        yield condor


@action
def fail_script(test_dir):
    return write_file(test_dir / "fail.sh", format_script(
        """
        #!/bin/sh
        sleep 2
        exit 1
        """
    ))


def expected_gb_hours(request_memory, memory_usage, duration):
    return max((request_memory - memory_usage) / 1024.0, 0.0) * duration / 3600.0


def submit_job(condor, test_dir, executable, name, request_memory, sleep, extra=None):
    description = {
        "log":                      test_dir / f"{name}.log",
        "leave_in_queue":           True,
        "should_transfer_files":    True,
        "executable":               executable,
        "arguments":                sleep,
        "request_memory":           request_memory,
    }
    if extra:
        description.update(extra)
    return condor.submit(description=description, count=1)


def wait_for_attr(handle, attr, timeout=60):
    # The final update to the job queue may land slightly after the
    # event that tells us the job is done.
    deadline = time.time() + timeout
    ad = handle.query()[0]
    while attr not in ad and time.time() < deadline:
        time.sleep(1)
        ad = handle.query()[0]
    return ad


def complete_job(condor, test_dir, executable, name, request_memory, extra=None):
    handle = submit_job(condor, test_dir, executable, name, request_memory, 5, extra)
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        verbose=True,
        timeout=120,
    )
    return wait_for_attr(handle, LAST)


def start_long_job(condor, test_dir, path_to_sleep, name, extra=None):
    job_extra = {"+TestMemoryUsage": 128}
    if extra:
        job_extra.update(extra)
    handle = submit_job(condor, test_dir, path_to_sleep, name, 512, 600, job_extra)
    assert handle.wait(
        condition=ClusterState.any_running,
        fail_condition=ClusterState.any_terminal,
        verbose=True,
        timeout=120,
    )
    return handle


def end_long_job(handle, end, condition):
    end()
    assert handle.wait(condition=condition, verbose=True, timeout=120)
    ad = wait_for_attr(handle, LAST)
    return ad, time.time()


@action
def completed_ad(condor, test_dir, path_to_sleep):
    return complete_job(condor, test_dir, path_to_sleep, "completed", 512,
                        {"+TestMemoryUsage": 128})


@action
def overused_ad(condor, test_dir, path_to_sleep):
    return complete_job(condor, test_dir, path_to_sleep, "overused", 128,
                        {"+TestMemoryUsage": 1024})


@action
def measured_ad(condor, test_dir, path_to_sleep):
    # No TestMemoryUsage, so MemoryUsage is the measured RSS.
    return complete_job(condor, test_dir, path_to_sleep, "measured", 512)


@action
def retried_ad(condor, test_dir, fail_script):
    # Two activations, each of which exits with code 1.
    return complete_job(condor, test_dir, fail_script, "retried", 512,
                        {"+TestMemoryUsage": 128, "max_retries": 1})


@action
def retried_epoch_ads(condor, retried_ad):
    with condor.use_config():
        epoch_ads = htcondor.Schedd().jobEpochHistory(
            f"ClusterId == {retried_ad['ClusterId']}",
            ["NumShadowStarts", "ActivationDuration", CURRENT, LAST],
        )
    return sorted(epoch_ads, key=lambda a: a["NumShadowStarts"])


@action
def running_samples(condor, test_dir, path_to_sleep):
    # Watch CurrentWastedMemoryGBHours while the job runs, then remove it.
    handle = start_long_job(condor, test_dir, path_to_sleep, "running")
    samples = []
    deadline = time.time() + 60
    while time.time() < deadline:
        ad = handle.query()[0]
        if CURRENT in ad and (not samples or ad[CURRENT] != samples[-1][0][CURRENT]):
            samples.append((ad, time.time()))
            if len(samples) >= 3:
                break
        time.sleep(1)
    removed = end_long_job(handle, handle.remove, ClusterState.all_terminal)
    return samples, removed


@action
def held_ad(condor, test_dir, path_to_sleep):
    handle = start_long_job(condor, test_dir, path_to_sleep, "held")
    time.sleep(5)
    result = end_long_job(handle, handle.hold, ClusterState.all_held)
    handle.remove()
    return result


@action
def policy_held_ad(condor, test_dir, path_to_sleep):
    # The shadow puts the job on hold itself, without waiting for the
    # starter to report that the job has exited.
    handle = start_long_job(condor, test_dir, path_to_sleep, "policy_held",
                            {"periodic_hold": "time() - JobCurrentStartDate > 5"})
    assert handle.wait(condition=ClusterState.all_held, verbose=True, timeout=120)
    ad = wait_for_attr(handle, LAST)
    done_time = time.time()
    handle.remove()
    return ad, done_time


def check_ended_long_job(result, lower):
    ad, done_time = result
    logger.info(f"{CURRENT} = {ad.get(CURRENT)}, {LAST} = {ad[LAST]}")
    assert ad[LAST] == ad[CURRENT]
    # The activation lasted at least as long as we let it run before
    # ending it, and at most until we saw that it had ended.
    upper = expected_gb_hours(512, 128, done_time - ad["JobCurrentStartDate"] + 1)
    assert lower <= ad[LAST] <= upper


class TestWastedMemory:

    def test_completed_job(self, completed_ad):
        duration = completed_ad["ActivationDuration"]
        logger.info(f"{LAST} = {completed_ad[LAST]}, ActivationDuration = {duration}")
        assert duration > 0
        assert completed_ad[LAST] == pytest.approx(expected_gb_hours(512, 128, duration))
        assert completed_ad[CURRENT] == completed_ad[LAST]

    def test_usage_over_request_clamps_to_zero(self, overused_ad):
        assert overused_ad[LAST] == 0.0
        assert overused_ad[CURRENT] == 0.0

    def test_measured_memory_usage(self, measured_ad):
        duration = measured_ad["ActivationDuration"]
        # The schedd rounds ResidentSetSize in the job queue (see
        # SCHEDD_ROUND_ATTR_ResidentSetSize), keeping the measured value in
        # ResidentSetSize_RAW; the shadow computes from the measured value.
        rss = measured_ad.get("ResidentSetSize_RAW", measured_ad["ResidentSetSize"])
        memory_usage = (rss + 1023) // 1024
        logger.info(f"{LAST} = {measured_ad[LAST]}, MemoryUsage = {memory_usage}")
        assert measured_ad[LAST] == pytest.approx(expected_gb_hours(512, memory_usage, duration))

    def test_each_activation_in_epoch_history(self, retried_ad, retried_epoch_ads):
        assert retried_ad["NumJobStarts"] == 2
        assert len(retried_epoch_ads) == 2
        for epoch_ad in retried_epoch_ads:
            logger.info(f"epoch {epoch_ad['NumShadowStarts']}: {LAST} = {epoch_ad[LAST]}, "
                        f"ActivationDuration = {epoch_ad['ActivationDuration']}")
            assert epoch_ad[LAST] == pytest.approx(
                expected_gb_hours(512, 128, epoch_ad["ActivationDuration"]))
            assert epoch_ad[CURRENT] == epoch_ad[LAST]
        assert retried_ad[LAST] == retried_epoch_ads[-1][LAST]

    def test_current_grows_while_running(self, running_samples):
        samples, _ = running_samples
        values = [ad[CURRENT] for ad, _ in samples]
        logger.info(f"{CURRENT} while running: {values}")
        assert len(samples) >= 3
        assert values == sorted(values)
        for ad, sample_time in samples:
            assert LAST not in ad
            assert ad[CURRENT] <= expected_gb_hours(
                512, 128, sample_time - ad["JobCurrentStartDate"] + 1)

    def test_removed_job(self, running_samples):
        samples, removed = running_samples
        check_ended_long_job(removed, samples[-1][0][CURRENT])

    def test_held_job(self, held_ad):
        check_ended_long_job(held_ad, expected_gb_hours(512, 128, 5))

    def test_policy_held_job(self, policy_held_ad):
        # JobCurrentStartDate is a little before the activation started.
        check_ended_long_job(policy_held_ad, expected_gb_hours(512, 128, 4))
