#!/usr/bin/env pytest

#
# Test CurrentWastedMemoryGBHours and LastWastedMemoryGBHours
# (HTCONDOR-4062).  CurrentWastedMemoryGBHours is updated as the job runs to
#
#   ((ActivationRequestMemory - MemoryUsage) / 1024) * (activation time so far / 3600)
#
# clamped to zero, and set to its final value when the activation ends, at
# which point it is also copied to LastWastedMemoryGBHours.  (For local
# universe jobs, both are set only when the activation ends.)  To make
# MemoryUsage deterministic, we configure the starter's MEMORY_USAGE_METRIC
# to use the job attribute TestMemoryUsage when the job defines it.
#
# The cases tested are:
#
#   - completed: a vanilla job that runs to completion, where the final
#     value is exactly what the formula gives for ActivationDuration, and
#     Current equals Last.
#   - overused: a job that uses more memory than it requested, where the
#     value is clamped to zero.
#   - measured: a job without TestMemoryUsage, so MemoryUsage is computed
#     from the measured ResidentSetSize.
#   - retried: a job with two activations (max_retries), where each
#     epoch record in the job epoch history has the value for its own
#     activation, and the job ad has the value for the last one.
#   - lifetime max: a job (vanilla, and local universe) whose first
#     activation uses much more memory than its second, where the second
#     activation's value is computed from the memory usage of the first,
#     since MemoryUsage is the max over the lifetime of the job.  These
#     jobs use the default MEMORY_USAGE_METRIC, so they test the real
#     ResidentSetSize bookkeeping rather than TestMemoryUsage.
#   - running: while the job runs, Current is updated and grows, and Last
#     isn't set (this is the job's first activation); then the job is
#     removed, and Last is set to Current's final value.
#   - held: the job is put on hold with condor_hold, so the starter reports
#     that the job has exited before the shadow sets the final value.
#   - policy held: the shadow puts the job on hold itself, because of a
#     periodic_hold on CurrentWastedMemoryGBHours, so it brings the values
#     up to date then, before the starter reports that the job has exited.
#   - slow exit: like policy held, but the job takes several seconds to
#     exit after it's told to, so the activation goes on well after the
#     shadow puts it on hold.  The final value must be measured until the
#     starter reports that the activation is over, like ActivationDuration,
#     not until the shadow put it on hold.
#   - local universe: Current and Last are not set while the job runs (the
#     local starter doesn't update the job queue then), but are set when the
#     job completes, is removed, or is put on hold.
#
# Where we can't know exactly how long the activation lasted (jobs we remove
# or hold, and local universe jobs, which have no ActivationDuration), we
# check that the value is between two bounds.  The test may run on a heavily
# loaded machine, so the bounds never assume that anything happens quickly:
#
#   - The lower bound comes from something that must have happened before
#     the activation ended: a CurrentWastedMemoryGBHours we saw while the
#     job ran, the periodic_hold threshold, or (for local universe, where
#     the local starter measures from JobCurrentStartDate) how long the job
#     sleeps or we let it run.
#   - The upper bound comes from a time that must be after the activation
#     ended: when we saw that the job had ended, or (for completed local
#     universe jobs) when the schedd marked it completed.
#
# To keep the test quick, all of the jobs are submitted at once (see the
# jobs fixture) and run concurrently; each case then waits for its own job.
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

# How much memory (MiB) the first activation of the lifetime max jobs uses,
# how much less than that we'll accept as measured, and the most we expect
# to be measured.
PEAK_MB = 200
PEAK_MIN_MB = 190
PEAK_MAX_MB = 400
LIFETIME_REQUEST_MB = 1024
# How long the second activation of the local universe lifetime max job
# runs.  It must be long enough for the test to tell the lifetime max from
# the second activation's own memory usage; see
# test_local_universe_lifetime_max_memory_usage().  The vanilla job's
# second activation can be short, since we know its exact duration.
LOCAL_SECOND_RUN_SECONDS = 20
VANILLA_SECOND_RUN_SECONDS = 2


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
            # Enough room for every vanilla job to run at once, whatever
            # the size of the machine.
            "NUM_CPUS": 16,
            "MEMORY": 16384,
            # The schedd starts local universe jobs, including restarting
            # them (for max_retries), on this interval.
            "SCHEDD_INTERVAL": 5,
            # Give the slow_exit job time to exit when told to, rather than
            # killing it (the test configuration's default is 2 seconds).
            "MachineMaxVacateTime": 60,
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


@action
def peak_script(test_dir):
    # The first time it runs (when the marker file doesn't exist yet), use
    # about PEAK_MB of memory for long enough that the starter measures it,
    # then fail, so the job runs again.  The second time, use little
    # memory, run for the given number of seconds, and succeed.
    return write_file(test_dir / "peak.py", format_script(
        f"""
        #!/usr/bin/python3
        import os
        import sys
        import time

        marker = sys.argv[1]
        if not os.path.exists(marker):
            open(marker, "w").close()
            data = b"x" * ({PEAK_MB} * 1024 * 1024)
            # Long enough for the starter to measure it, even on a heavily
            # loaded machine.
            time.sleep(15)
            sys.exit(1)
        time.sleep(int(sys.argv[2]))
        sys.exit(0)
        """
    ))


@action
def slow_exit_script(test_dir):
    # Run until told to stop (SIGTERM), then take SLOW_EXIT_SECONDS to exit.
    return write_file(test_dir / "slow_exit.py", format_script(
        f"""
        #!/usr/bin/python3
        import signal
        import sys
        import time

        def on_term(signum, frame):
            time.sleep({SLOW_EXIT_SECONDS})
            sys.exit(0)

        signal.signal(signal.SIGTERM, on_term)
        time.sleep(600)
        """
    ))


# How long the slow_exit job takes to exit after it's told to.  It must be
# less than the job's job_max_vacate_time and the slot's
# MachineMaxVacateTime (both set in this test), after which the job is
# killed.
SLOW_EXIT_SECONDS = 6


def expected_gb_hours(request_memory, memory_usage, duration):
    return max((request_memory - memory_usage) / 1024.0, 0.0) * duration / 3600.0


def implied_memory_usage(request_memory, gb_hours, duration):
    # The MemoryUsage that, by the formula, gives gb_hours.
    return request_memory - gb_hours * 1024.0 * 3600.0 / duration


def local_activation_upper_bound(ad):
    # The local starter measures the activation from JobCurrentStartDate
    # until the job exits, after which the schedd marks the job completed,
    # setting EnteredCurrentStatus.  (Allow a second for rounding.)
    return ad["EnteredCurrentStatus"] - ad["JobCurrentStartDate"] + 1


# The jobs we end ourselves must have run at least this long first.
MIN_RUN_SECONDS = 5
# The policy_held job's periodic_hold fires when its wasted memory reaches
# this.  Using CurrentWastedMemoryGBHours, rather than a time, means we
# know the final value is at least this, however late the activation
# started.
POLICY_HOLD_THRESHOLD = expected_gb_hours(512, 128, MIN_RUN_SECONDS)


def submit_job(condor, test_dir, executable, name, request_memory, arguments, extra=None, universe="vanilla"):
    description = {
        "log":                      test_dir / f"{name}.log",
        "leave_in_queue":           True,
        "universe":                 universe,
        "should_transfer_files":    True,
        "executable":               executable,
        "arguments":                arguments,
        "request_memory":           request_memory,
    }
    if extra:
        description.update(extra)
    return condor.submit(description=description, count=1)


@action
def jobs(condor, test_dir, path_to_sleep, fail_script, peak_script, slow_exit_script):
    # Submit every job up front, so they run concurrently.  Jobs that
    # complete on their own sleep for 5 seconds; the others sleep for 600
    # seconds, and the test removes or holds them.
    def submit(name, executable, request_memory, arguments, extra=None, universe="vanilla"):
        return submit_job(condor, test_dir, executable, name, request_memory,
                          arguments, extra, universe)

    def lifetime_max(name, seconds, universe):
        return submit(name, peak_script, LIFETIME_REQUEST_MB,
                      f"{test_dir / (name + '.marker')} {seconds}",
                      {"max_retries": 1}, universe)

    test_usage = {"+TestMemoryUsage": 128}
    return {
        "completed":          submit("completed", path_to_sleep, 512, 5, test_usage),
        "overused":           submit("overused", path_to_sleep, 128, 5, {"+TestMemoryUsage": 1024}),
        # No TestMemoryUsage, so MemoryUsage is the measured RSS.
        "measured":           submit("measured", path_to_sleep, 512, 5),
        # Two activations, each of which exits with code 1.
        "retried":            submit("retried", fail_script, 512, "",
                                     {**test_usage, "max_retries": 1}),
        "lifetime_max":       lifetime_max("lifetime_max", VANILLA_SECOND_RUN_SECONDS, "vanilla"),
        "local_lifetime_max": lifetime_max("local_lifetime_max", LOCAL_SECOND_RUN_SECONDS, "local"),
        "running":            submit("running", path_to_sleep, 512, 600, test_usage),
        "held":               submit("held", path_to_sleep, 512, 600, test_usage),
        # The shadow puts the job on hold itself.
        "policy_held":        submit("policy_held", path_to_sleep, 512, 600,
                                     {**test_usage, "periodic_hold": f"{CURRENT} >= {POLICY_HOLD_THRESHOLD!r}"}),
        # Likewise, but the job takes a while to exit.  With graceful
        # removal, the starter tells the job to exit (SIGTERM) and waits
        # for it, rather than killing it.
        "slow_exit":          submit("slow_exit", slow_exit_script, 512, "",
                                     {**test_usage,
                                      "periodic_hold": f"{CURRENT} >= {POLICY_HOLD_THRESHOLD!r}",
                                      "want_graceful_removal": True,
                                      "job_max_vacate_time": 60}),
        "local_completed":    submit("local_completed", path_to_sleep, 512, 5, test_usage, "local"),
        "local_removed":      submit("local_removed", path_to_sleep, 512, 600, test_usage, "local"),
        "local_held":         submit("local_held", path_to_sleep, 512, 600, test_usage, "local"),
    }


def wait_for_attr(handle, attr, timeout=60):
    # The final update to the job queue may land slightly after the
    # event that tells us the job is done.
    deadline = time.time() + timeout
    ad = handle.query()[0]
    while attr not in ad and time.time() < deadline:
        time.sleep(1)
        ad = handle.query()[0]
    return ad


def wait_for_completion(handle):
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        verbose=True,
        timeout=180,
    )
    return wait_for_attr(handle, LAST)


def wait_for_running(handle):
    assert handle.wait(
        condition=ClusterState.any_running,
        fail_condition=ClusterState.any_terminal,
        verbose=True,
        timeout=180,
    )


def let_run(handle, universe):
    # Let the job's activation run for at least MIN_RUN_SECONDS, and return
    # a lower bound for its final wasted memory.  The jobs run concurrently,
    # so by the time we get to it, it may have run that long already.
    #
    # This must not depend on how quickly things happen, since the test
    # machine may be heavily loaded.  For vanilla jobs, the shadow may
    # start the activation well after JobCurrentStartDate, so instead we
    # wait until the job queue has a CurrentWastedMemoryGBHours that covers
    # that long, and use it as the lower bound: the final value can only be
    # bigger.  The local starter measures from JobCurrentStartDate itself.
    wait_for_running(handle)
    if universe == "local":
        start_date = handle.query()[0]["JobCurrentStartDate"]
        time.sleep(max(start_date + MIN_RUN_SECONDS - time.time(), 0))
        return expected_gb_hours(512, 128, MIN_RUN_SECONDS)
    threshold = expected_gb_hours(512, 128, MIN_RUN_SECONDS)
    deadline = time.time() + 120
    while time.time() < deadline:
        current = handle.query()[0].get(CURRENT)
        if current is not None and current >= threshold:
            return current
        time.sleep(1)
    assert False, f"{CURRENT} never reached {threshold}"


def end_long_job(handle, end, condition):
    end()
    assert handle.wait(condition=condition, verbose=True, timeout=120)
    ad = wait_for_attr(handle, LAST)
    return ad, time.time()


def hold_long_job(handle, universe):
    lower = let_run(handle, universe)
    ad, done_time = end_long_job(handle, handle.hold, ClusterState.all_held)
    handle.remove()
    return ad, done_time, lower


def epoch_ads(condor, ad):
    with condor.use_config():
        ads = htcondor.Schedd().jobEpochHistory(
            f"ClusterId == {ad['ClusterId']}",
            ["NumShadowStarts", "ActivationDuration", CURRENT, LAST],
        )
    return sorted(ads, key=lambda a: a["NumShadowStarts"])


@action
def completed_ad(jobs):
    return wait_for_completion(jobs["completed"])


@action
def overused_ad(jobs):
    return wait_for_completion(jobs["overused"])


@action
def measured_ad(jobs):
    return wait_for_completion(jobs["measured"])


@action
def retried_ad(jobs):
    return wait_for_completion(jobs["retried"])


@action
def retried_epoch_ads(condor, retried_ad):
    return epoch_ads(condor, retried_ad)


@action
def lifetime_max_ad(jobs):
    return wait_for_completion(jobs["lifetime_max"])


@action
def lifetime_max_epoch_ads(condor, lifetime_max_ad):
    return epoch_ads(condor, lifetime_max_ad)


@action
def local_lifetime_max_ad(jobs):
    return wait_for_completion(jobs["local_lifetime_max"])


@action
def running_samples(jobs):
    # Watch CurrentWastedMemoryGBHours while the job runs, then remove it.
    handle = jobs["running"]
    wait_for_running(handle)
    samples = []
    deadline = time.time() + 120
    while time.time() < deadline:
        ad = handle.query()[0]
        if CURRENT in ad and (not samples or ad[CURRENT] != samples[-1][0][CURRENT]):
            samples.append((ad, time.time()))
            if len(samples) >= 3:
                break
        time.sleep(1)
    ad, done_time = end_long_job(handle, handle.remove, ClusterState.all_terminal)
    # The final value can only be bigger than the last one we saw.
    lower = samples[-1][0][CURRENT] if samples else 0.0
    return samples, (ad, done_time, lower)


@action
def held_ad(jobs):
    return hold_long_job(jobs["held"], "vanilla")


def wait_for_policy_hold(handle):
    assert handle.wait(condition=ClusterState.all_held, verbose=True, timeout=180)
    # The shadow sets Last when it puts the job on hold, and again, to its
    # final value, when the starter reports that the activation is over,
    # which is also when it sets ActivationDuration.
    wait_for_attr(handle, "ActivationDuration")
    return wait_for_attr(handle, LAST)


@action
def policy_held_ad(jobs):
    handle = jobs["policy_held"]
    ad = wait_for_policy_hold(handle)
    done_time = time.time()
    handle.remove()
    # The job's periodic_hold fires once CurrentWastedMemoryGBHours
    # reaches the threshold, so the final value is at least that.
    return ad, done_time, POLICY_HOLD_THRESHOLD


@action
def slow_exit_ads(condor, jobs):
    handle = jobs["slow_exit"]
    ad = wait_for_policy_hold(handle)
    epochs = epoch_ads(condor, ad)
    handle.remove()
    return ad, epochs


@action
def local_completed_ad(jobs):
    return wait_for_completion(jobs["local_completed"])


@action
def local_running_and_removed(jobs):
    handle = jobs["local_removed"]
    lower = let_run(handle, "local")
    # The local starter doesn't update the job queue while the job runs.
    running = handle.query()[0]
    ad, done_time = end_long_job(handle, handle.remove, ClusterState.all_terminal)
    return running, (ad, done_time, lower)


@action
def local_held_ad(jobs):
    return hold_long_job(jobs["local_held"], "local")


def check_ended_long_job(result):
    ad, done_time, lower = result
    logger.info(f"{CURRENT} = {ad.get(CURRENT)}, {LAST} = {ad[LAST]}")
    assert ad[LAST] == ad[CURRENT]
    # The activation lasted at least as long as we let it run before
    # ending it, and at most until we saw that it had ended.
    upper = expected_gb_hours(512, 128, done_time - ad["JobCurrentStartDate"] + 1)
    assert lower <= ad[LAST] <= upper


def check_peak_measured(ad):
    # If this fails, the starter never measured the first activation's
    # peak, so the rest of the lifetime max test would prove nothing.
    rss_kb = ad.get("ResidentSetSize_RAW", ad["ResidentSetSize"])
    logger.info(f"NumJobStarts = {ad['NumJobStarts']}, ResidentSetSize = {rss_kb} KiB")
    assert ad["NumJobStarts"] == 2
    assert rss_kb >= PEAK_MIN_MB * 1024


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

    def test_lifetime_max_memory_usage(self, lifetime_max_ad, lifetime_max_epoch_ads):
        check_peak_measured(lifetime_max_ad)
        assert len(lifetime_max_epoch_ads) == 2
        # Both activations' values must be computed from the first
        # activation's peak.  If the second's were computed from its own
        # memory usage, the implied usage would be far below the peak.
        for epoch_ad in lifetime_max_epoch_ads:
            usage = implied_memory_usage(LIFETIME_REQUEST_MB, epoch_ad[LAST],
                                         epoch_ad["ActivationDuration"])
            logger.info(f"epoch {epoch_ad['NumShadowStarts']}: {LAST} = {epoch_ad[LAST]}, "
                        f"ActivationDuration = {epoch_ad['ActivationDuration']}, "
                        f"implied MemoryUsage = {usage}")
            assert PEAK_MIN_MB <= usage <= PEAK_MAX_MB
        assert lifetime_max_ad[LAST] == lifetime_max_epoch_ads[-1][LAST]

    def test_local_universe_lifetime_max_memory_usage(self, local_lifetime_max_ad):
        ad = local_lifetime_max_ad
        check_peak_measured(ad)
        # Local universe jobs have no ActivationDuration or epoch records,
        # so bound the second activation's duration below by how long it
        # sleeps, and above by when the schedd marked it completed.  The
        # second activation runs long enough that a value computed from its
        # own memory usage would be above the upper bound, unless the
        # schedd takes several seconds to notice that the job exited (in
        # which case the test passes without proving much, but doesn't fail).
        lower = expected_gb_hours(LIFETIME_REQUEST_MB, PEAK_MAX_MB, LOCAL_SECOND_RUN_SECONDS)
        upper = expected_gb_hours(LIFETIME_REQUEST_MB, PEAK_MIN_MB, local_activation_upper_bound(ad))
        logger.info(f"{LAST} = {ad[LAST]}, bounds = [{lower}, {upper}]")
        assert lower <= ad[LAST] <= upper

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
        _, removed = running_samples
        check_ended_long_job(removed)

    def test_held_job(self, held_ad):
        check_ended_long_job(held_ad)

    def test_policy_held_job(self, policy_held_ad):
        check_ended_long_job(policy_held_ad)

    def test_slow_exit_measured_until_starter_reports(self, slow_exit_ads):
        ad, epochs = slow_exit_ads
        duration = ad["ActivationDuration"]
        logger.info(f"{LAST} = {ad[LAST]}, ActivationDuration = {duration}")
        # Measured until the starter reported, the same span as
        # ActivationDuration...
        assert ad[LAST] == pytest.approx(expected_gb_hours(512, 128, duration))
        assert ad[CURRENT] == ad[LAST]
        # ...which includes the time the job took to exit after the hold,
        # which came after at least MIN_RUN_SECONDS of the activation.
        assert ad[LAST] >= expected_gb_hours(512, 128, MIN_RUN_SECONDS + SLOW_EXIT_SECONDS)
        # And the epoch record has the final value too.
        assert len(epochs) == 1
        assert epochs[0][LAST] == pytest.approx(ad[LAST])

    def test_local_universe_completed_job(self, local_completed_ad):
        ad = local_completed_ad
        # Local universe jobs have no ActivationDuration; the local starter
        # measures from JobCurrentStartDate until the job has exited.  The
        # job sleeps for 5 seconds, so it ran at least that long, and the
        # schedd only marked it completed (EnteredCurrentStatus) after it
        # exited.
        lower = expected_gb_hours(512, 128, 5)
        upper = expected_gb_hours(512, 128, local_activation_upper_bound(ad))
        logger.info(f"{LAST} = {ad[LAST]}, bounds = [{lower}, {upper}]")
        assert lower <= ad[LAST] <= upper
        assert ad[CURRENT] == ad[LAST]

    def test_local_universe_not_set_while_running(self, local_running_and_removed):
        running, _ = local_running_and_removed
        assert CURRENT not in running
        assert LAST not in running

    def test_local_universe_removed_job(self, local_running_and_removed):
        _, removed = local_running_and_removed
        check_ended_long_job(removed)

    def test_local_universe_held_job(self, local_held_ad):
        check_ended_long_job(local_held_ad)
