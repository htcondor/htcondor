#!/usr/bin/env pytest

import logging
import re
import time

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# We reproduce the dominant real-world leak with partitionable slots: a p-slot
# hands out several *dynamic* slots, each with a unique, non-recurring name.
# When the jobs finish the d-slots are destroyed and disappear from the
# collector.  With the bug, the negotiator keeps their resource records forever;
# with the fix, the next negotiation cycle (which always runs CheckMatches)
# reaps every record whose slot is no longer in the resource list.
#
# Holding several d-slots concurrently guarantees several *distinct* names, so
# the leak can't be masked by the startd reusing a freed dynamic-slot name.
#
NUM_SLOTS = 4


@standup
def condor(test_dir):
    with Condor(
        test_dir / "condor",
        raw_config="use feature : PartitionableSlot",
        config={
            "NUM_CPUS": str(NUM_SLOTS),
            # Short cycles so create + reap happen quickly, and so an explicit
            # condor_reschedule isn't throttled by the cycle gates.
            "NEGOTIATOR_INTERVAL": "10",
            "NEGOTIATOR_CYCLE_DELAY": "1",
            "NEGOTIATOR_MIN_INTERVAL": "1",
        },
    ) as condor:
        yield condor


def _run_userprio(condor, args, timeout=30):
    """Run condor_userprio, retrying transient failures (the negotiator can be
    briefly unresponsive to command-socket connections while inside a cycle)."""
    deadline = time.time() + timeout
    rv = None
    while time.time() < deadline:
        rv = condor.run_command(["condor_userprio"] + list(args))
        if rv is not None and rv.returncode == 0:
            return rv
        time.sleep(0.5)
    return rv


def _submitter_names(condor):
    """All submitter names (user@domain) currently known to the accountant."""
    rv = _run_userprio(condor, ["-allusers", "-l"])
    names = []
    if rv is None or rv.returncode != 0:
        return names
    for line in rv.stdout.splitlines():
        key, sep, val = line.partition("=")
        if sep and key.strip() == "Name":
            name = val.strip().strip('"')
            if "@" in name:  # skip group rows like "<none>"
                names.append(name)
    return names


def _reslist_count(condor, submitter):
    """Number of resource records the accountant holds for `submitter`, or None
    if the query failed."""
    rv = _run_userprio(condor, ["-getreslist", submitter])
    if rv is None or rv.returncode != 0:
        return None
    m = re.search(r"Number of Resources Used:\s*(\d+)", rv.stdout)
    return int(m.group(1)) if m is not None else None


def _wait_for_reslist(condor, submitter, predicate, timeout=90):
    """Poll the reslist count until `predicate(count)` holds, nudging a
    negotiation cycle each iteration.  Returns the last observed count."""
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = _reslist_count(condor, submitter)
        if last is not None and predicate(last):
            return last
        condor.run_command(["condor_reschedule"])
        time.sleep(1)
    return last


@action
def running_jobs(condor, test_dir, path_to_sleep):
    handle = condor.submit(
        description={
            "executable":             path_to_sleep,
            "arguments":              300,
            "universe":               "vanilla",
            "should_transfer_files":  False,
            "Request_Cpus":           1,
            "Request_Memory":         32,
            "Request_Disk":           1024,
            "log":                    (test_dir / "job.log").as_posix(),
        },
        count=NUM_SLOTS,
    )
    assert handle.wait(
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_held,
        timeout=120,
    )
    return handle


@action
def submitter(condor, running_jobs):
    # Discover the submitter name from the accountant itself, so it matches the
    # name GET_RESLIST keys on.  Jobs are already running (running_jobs), so the
    # submitter exists; poll briefly in case the accountant hasn't recorded it
    # yet.
    deadline = time.time() + 60
    while time.time() < deadline:
        names = _submitter_names(condor)
        if names:
            return names[0]
        condor.run_command(["condor_reschedule"])
        time.sleep(1)
    raise AssertionError("no submitter ever appeared in condor_userprio")


class TestNegotiatorReapsResourceRecords:

    def test_records_created_then_reaped(self, condor, running_jobs, submitter):
        # Positive control: with NUM_SLOTS jobs running on distinct dynamic
        # slots, the accountant must hold a record per claimed slot.  This both
        # proves records get created and guarantees several distinct d-slot
        # names exist, so the leak (if present) is unambiguous.  We assert
        # ">=" rather than "==": the initial job->p-slot match also leaves a
        # record against the partitionable parent ("slot1@..."), so the live
        # count is the NUM_SLOTS dynamic slots plus possibly that parent record.
        count = _wait_for_reslist(
            condor, submitter, lambda n: n >= NUM_SLOTS, timeout=60
        )
        assert count is not None and count >= NUM_SLOTS, (
            f"expected at least {NUM_SLOTS} resource records while jobs run, "
            f"got {count}"
        )

        # Tear down: remove the jobs so every dynamic slot is destroyed and
        # disappears from the collector's view.
        rv = condor.run_command(["condor_rm", "-all"])
        assert rv.returncode == 0, rv.stderr

        # The reap runs in Accountant::CheckMatches(), once per negotiation
        # cycle.  Once the d-slots are gone, the record count must drain to
        # zero.  Under the regression it stays pinned at NUM_SLOTS and this
        # times out.
        final = _wait_for_reslist(
            condor, submitter, lambda n: n == 0, timeout=90
        )
        assert final == 0, (
            f"stale resource records not reaped after jobs were removed: "
            f"still {final} (regression: Accountant::CheckMatches no longer "
            f"removes records for slots that have gone away)"
        )
