#!/usr/bin/env pytest

#
# There's a race condition in the negotiator's handling of concurrency limits:
# while the negotiator won't hand out more concurrency tokens than are
# available in any one cycle, it depends on seeing all of those tokens in the
# collector to avoid handing them out again in the next cycle.
#
# We could try to control the negoatiator cycle by setting a very short
# NEGOTIATOR_CYCLE_DELAY, an "infinitely" long NEGOTIATOR_INTERVAL, and
# then calling condor_reschedule, but it seems easier to control the number
# of runnable jobs, instead.
#

import re
import time
import logging
import datetime
import htcondor

from ornithology import (
    config,
    standup,
    action,

    Condor,
    SetJobStatus,
    JobStatus,
    in_order,
    track_quantity,
    ClusterState,
)

# This is awful.
concurrency_limits_hit_first = None

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# the effective number of slots should always be much larger than the number of
# jobs you plan to submit
SLOT_CONFIGS = {
    "static_slots": {"NUM_CPUS": "12", "NUM_SLOTS": "12"},
    "partitionable_slot": {
        "NUM_CPUS": "12",
        "SLOT_TYPE_1": "cpus=100%,memory=100%,disk=100%",
        "SLOT_TYPE_1_PARTITIONABLE": "True",
        "NUM_SLOTS_TYPE_1": "1",
    },
}


@config(params=SLOT_CONFIGS)
def slot_config(request):
    return request.param


CONCURRENCY_LIMITS = {
    "named_limit": {
        "config-key": "XSW_LIMIT",
        "config-value": "4",
        "submit-value": "XSW",
        "max-running": 4,
    },
    "default_limit": {
        "config-key": "CONCURRENCY_LIMIT_DEFAULT",
        "config-value": "2",
        "submit-value": "UNDEFINED:2",
        "max-running": 1,
    },
    "default_small": {
        "config-key": "CONCURRENCY_LIMIT_DEFAULT_SMALL",
        "config-value": "3",
        "submit-value": "small.license",
        "max-running": 3,
    },
    "default_large": {
        "config-key": "CONCURRENCY_LIMIT_DEFAULT_LARGE",
        "config-value": "1",
        "submit-value": "large.license",
        "max-running": 1,
    },
}


@config(params=CONCURRENCY_LIMITS)
def concurrency_limit(request):
    return request.param


@standup
def condor(test_dir, slot_config, concurrency_limit):
    # We don't want to share a startd for different concurrency limit tests,
    # because doing so introduces another race condition where we need to
    # startd to be totally idle before starting the next test.
    concurrency_limit_config = {
        concurrency_limit["config-key"]: concurrency_limit["config-value"]
    }

    with Condor(
        local_dir=test_dir / "condor",
        config={
            **slot_config,
            **concurrency_limit_config,
            # The negotiator determines if a concurrency limit is in use by
            # checking the machine ads, so it will overcommit tokens if its
            # cycle time is shorter than the claim-and-report cycle.
            #
            # I'm not sure why the claim-and-report cycle is so long.
            "NEGOTIATOR_INTERVAL": "1",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "UPDATE_INTERVAL": "1",
            # This MUST include D_MATCH, which is the default.
            "NEGOTIATOR_DEBUG": "D_MATCH D_CATEGORY D_SUB_SECOND",
            "SCHEDD_DEBUG": "D_FULLDEBUG",
            # Don't delay or decline to update the runnable job count for
            # any reason after receiving the reschedule command.
            "SCHEDD_MIN_INTERVAL": "0",
            "SCHEDD_INTERVAL_TIMESLICE": "1",
        },
    ) as condor:
        yield condor


# This can't be the best way to do this.
def wait_for_busy_slots_in_collector(condor, count, timeout=120):
    deadline = time.time() + timeout
    while True:
        result = condor.status(
            constraint='State == "Claimed" && Activity == "Busy"',
            projection=["Name", "GlobalJobID"],
        )
        if count == len(result):
            return
        if count < len(result):
            return
        assert time.time() < deadline
        time.sleep(1)


def wait_for_next_negotiation_cycle(condor):
    now = datetime.datetime.now()
    negotiator_log = condor.negotiator_log.open()

    assert negotiator_log.wait(
        lambda entry: entry.timestamp > now
    )
    assert negotiator_log.wait(
        lambda entry: "Started Negotiation Cycle" in entry
    )
    assert negotiator_log.wait(
        lambda entry: "Finished Negotiation Cycle" in entry
    )


def wait_for_log_to_catch_up(log, when):
    return log.wait(
        lambda entry: entry.timestamp > when
    )


@action
def limit_exceeded(condor, concurrency_limit, path_to_sleep):
    max_running = concurrency_limit["max-running"]

    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "3600",
            "request_memory": "100MB",
            "request_disk": "10MB",
            "hold": "true",
            "concurrency_limits": concurrency_limit["submit-value"],
            "log": "test_concurrency_limits-{}.log".format(concurrency_limit["config-key"]),
        },
        count=(max_running + 1) * 2,
    )

    #
    # Release exactly the right number of jobs.
    # FIXME: why is this the easiest way to do this?
    #
    condor.run_command(["condor_release", *handle.job_ids[:max_running]])

    #
    # Wait for enough jobs to be running.
    #
    assert handle.wait(
        condition=ClusterState.running_exactly(max_running),
        timeout=180,
        verbose=True,
        fail_condition=lambda cs: cs.counts()[JobStatus.RUNNING] > max_running
    )

    #
    # Wait for those jobs' slots to report to the collector.
    #
    wait_for_busy_slots_in_collector(condor, max_running)

    #
    # Release the next job.
    #
    condor.run_command(["condor_release", handle.job_ids[max_running]])

    #
    # Because the schedd only periodically updates its table of runnable
    # jobs, we need to force it to do so before we can assume that the
    # negotiator will see the newly-idled job.
    #
    # FIXME: wait for the schedd log to say that it got this command?
    #
    condor.run_command(["condor_reschedule"])

    #
    # Wait until we've seen a start and then the end of a negotiation cycle.
    #
    wait_for_next_negotiation_cycle(condor)

    #
    # Put the extra job back on hold.
    # FIXME: Why is this the easiest way to do this?
    #
    condor.run_command(["condor_hold", handle.job_ids[max_running]])

    #
    # Put one of the running jobs on hold.
    # FIXME: Why is this the easiest way to do this?
    #
    condor.run_command(["condor_hold", handle.job_ids[0]])

    # Avoid a race condition where we could successfully wait for four jobs
    # to be running because the event log hadn't recorded this one going on
    # hold yet.
    assert handle.wait(
        condition=lambda cs: cs[0] == JobStatus.HELD,
        timeout=180,
        verbose=True,
    )

    #
    # At this point, we know that the negotiator won't hit the concurrency
    # limit again until we release the job, so look for it doing so now.
    #
    now = datetime.datetime.now()
    assert wait_for_log_to_catch_up(condor.negotiator_log.open(), now)
    print("\nFirst concurrency limit hit should have happened by {}".format(now))

    global concurrency_limits_hit_first
    concurrency_limits_hit_first = find_concurrency_limits_hit(
        condor.negotiator_log.open()
    )

    #
    # Release the next job.
    # FIXME: Why is this the easiest way to do this?
    #
    condor.run_command(["condor_release", handle.job_ids[max_running]])

    #
    # Wait for enough jobs to be running.
    #
    assert handle.wait(
        condition=ClusterState.running_exactly(max_running),
        timeout=180,
        verbose=True,
    )

    #
    # Wait for those job's slots to report to the collector.
    #
    wait_for_busy_slots_in_collector(condor, max_running)

    #
    # Release the first job.
    # FIXME: Why is this the easiest way to do this?
    #
    condor.run_command(["condor_release", handle.job_ids[0]])

    #
    # Wait for job to be reported as released, to make sure the negotiator
    # has a chance to play with it.
    #
    assert handle.wait(
        condition=lambda cs: cs[0] == JobStatus.IDLE,
        timeout=180,
        verbose=True,
    )

    #
    # Wait until we've seen a start and then the end of a negotiation cycle.
    #
    wait_for_next_negotiation_cycle(condor)
    now = datetime.datetime.now()
    print("Second concurrency limit hit should have happened by {}".format(now))

    yield handle

    handle.remove()


@action
def num_jobs_running_history(condor, limit_exceeded, concurrency_limit):
    job_queue_log = [t for t in condor.job_queue.read_transactions()]
    job_status_events = condor.job_queue.filter(
        lambda j, e: j in limit_exceeded.job_ids and e.attribute == "JobStatus"
    )

    history = [0]
    running_count = 0
    job_status = dict()
    for job_id, event in job_status_events:
        new_status = event.value

        if not job_id in job_status:
            job_status[job_id] = new_status
            continue

        old_status = job_status[job_id]
        job_status[job_id] = new_status

        if new_status == JobStatus.RUNNING:
            running_count += 1
        elif new_status == JobStatus.IDLE:
            if old_status == JobStatus.HELD:
                continue
            running_count -= 1
        elif new_status == JobStatus.HELD:
            if old_status == JobStatus.IDLE:
                continue
            running_count -= 1

        history.append(running_count)

    return history


@action
def startd_log_file(condor):
    return condor.startd_log.open()


@action
def num_busy_slots_history(startd_log_file, limit_exceeded, concurrency_limit):
    logger.debug("Checking Startd log file...")
    logger.debug("Expected Job IDs are: {}".format(limit_exceeded.job_ids))

    # You'd expect the conditions to be mirror-images, but for unknown reasons,
    # putting jobs on hold sometimes skips this step.
    active_claims_history = track_quantity(
        startd_log_file.read(),
        increment_condition=lambda msg: "Changing activity: Idle -> Busy" in msg,
        decrement_condition=lambda msg:
            "Changing activity: Busy -> Idle" in msg or
            "Changing state and activity: Preempting/Vacating -> Owner/Idle" in msg,
        max_quantity=concurrency_limit["max-running"],
        expected_quantity=concurrency_limit["max-running"],
    )

    return active_claims_history


@action
def final_negotiator_log(limit_exceeded, condor):
    return condor.negotiator_log.open()


def find_concurrency_limits_hit(negotiator_log):
    last_line = None
    limits_hit = dict()
    for entry in negotiator_log.read():
        match = re.match(
            r"^\s*Rejected .*: concurrency limit (.*) reached$", entry.message
        )
        if match:
            limit = match.group(1).lower()
            value = limits_hit.setdefault(limit, 0)
            limits_hit[limit] = value + 1
        last_line = entry.line
    print("Last line seen in negotiator log was '{}'".format(last_line))
    return limits_hit


@action
def concurrency_limits_hit(final_negotiator_log):
    return find_concurrency_limits_hit(final_negotiator_log)

class TestConcurrencyLimits:
    def test_never_more_jobs_running_than_limit(
        self, num_jobs_running_history, concurrency_limit
    ):
        assert max(num_jobs_running_history) <= concurrency_limit["max-running"]


    def test_num_jobs_running_hits_limit(
        self, num_jobs_running_history, concurrency_limit
    ):
        assert concurrency_limit["max-running"] in num_jobs_running_history


    def test_never_more_busy_slots_than_limit(
        self, num_busy_slots_history, concurrency_limit
    ):
        assert max(num_busy_slots_history) <= concurrency_limit["max-running"]


    def test_num_busy_slots_hits_limit(
        self, num_busy_slots_history, concurrency_limit
    ):
        assert concurrency_limit["max-running"] in num_busy_slots_history


    def test_negotiator_hits_limit_twice(
        self, concurrency_limit, concurrency_limits_hit
    ):
        limit_name_in_log = concurrency_limit["submit-value"].lower().split(":")[0]

        global concurrency_limits_hit_first
        assert limit_name_in_log in concurrency_limits_hit_first
        initial_count = concurrency_limits_hit_first[limit_name_in_log]
        assert limit_name_in_log in concurrency_limits_hit
        final_count = concurrency_limits_hit[limit_name_in_log]
        assert final_count > initial_count
