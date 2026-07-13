#!/usr/bin/env pytest

import logging
import time

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Stand up a personal condor with only a negotiator (plus master) and
# exercise condor_userprio ceiling leases: seed a ceiling, set a lease,
# try an illegal double-lease, cancel it, try an illegal double-cancel.

SUBMITTER = "leaseuser@test.local"


@standup
def condor(test_dir):
    with Condor(
        test_dir / "condor",
        config={
            # Push the cycle out so the first commands aren't issued while
            # the negotiator is busy in its initial negotiation cycle.
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR",
            "NEGOTIATOR_INTERVAL": "60",
        },
    ) as condor:
        yield condor


def _query_attr(condor, submitter, attr, cast):
    """Return the submitter's current value for `attr`, or None if not present."""
    rv = condor.run_command(["condor_userprio", "-allusers", "-l"])
    if rv.returncode != 0:
        return None
    current = {}
    ads = [current]
    for line in rv.stdout.splitlines():
        stripped = line.strip()
        if not stripped:
            if current:
                current = {}
                ads.append(current)
            continue
        key, _, val = stripped.partition("=")
        if not _:
            continue
        current[key.strip()] = val.strip()
    for ad in ads:
        name = ad.get("Name", "").strip('"')
        if name.lower() == submitter.lower():
            raw = ad.get(attr)
            if raw is None:
                return None
            return cast(raw)
    return None


def _wait_for_attr(condor, submitter, attr, expected, cast=int, timeout=30):
    """Poll until the submitter's `attr` matches expected, or fail."""
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = _query_attr(condor, submitter, attr, cast)
        if last == expected:
            return
        time.sleep(0.5)
    raise AssertionError(
        f"timed out waiting for {submitter} {attr} to be {expected!r}; "
        f"last value was {last!r}"
    )


def _query_ceiling(condor, submitter):
    val = _query_attr(condor, submitter, "Ceiling", int)
    return -1 if val is None else val


def _wait_for_ceiling(condor, submitter, expected, timeout=30):
    _wait_for_attr(condor, submitter, "Ceiling", expected, int, timeout)


def _run_userprio_with_retry(condor, args, timeout=30):
    """Run a condor_userprio command, retrying through transient failures
    (the negotiator may be inside a negotiation cycle and briefly
    unresponsive to command-socket connections)."""
    deadline = time.time() + timeout
    rv = None
    while time.time() < deadline:
        rv = condor.run_command(["condor_userprio"] + list(args))
        if rv.returncode == 0:
            return rv
        time.sleep(0.5)
    return rv


class TestCeilingLease:

    def test_lease_lifecycle(self, condor):
        # Seed a baseline ceiling so the submitter exists and we have a
        # known pre-lease value to revert to. SET_CEILING is fire-and-forget
        # on the wire, so wait for it to actually take effect rather than
        # relying on a fixed sleep.
        rv = _run_userprio_with_retry(
            condor, ["-setceiling", SUBMITTER, "10"]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_ceiling(condor, SUBMITTER, 10)

        # Install a lease; ceiling should change to the leased value.
        rv = _run_userprio_with_retry(
            condor,
            ["-setceiling", SUBMITTER, "42", "-duration", "3600"],
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set ceiling lease" in rv.stdout
        _wait_for_ceiling(condor, SUBMITTER, 42)

        # A second lease on top of an in-effect lease must be rejected.
        # This command's non-zero exit is the success signal, so don't
        # retry it -- just run once.
        rv = condor.run_command(
            ["condor_userprio", "-setceiling", SUBMITTER, "99",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        # And the original lease value is still in effect. Use the polling
        # helper here -- condor_userprio occasionally returns exit 0 with an
        # empty body when issued back-to-back with a just-rejected command,
        # and a single-shot _query_ceiling would see that transient blip.
        _wait_for_ceiling(condor, SUBMITTER, 42)

        # Cancel restores the prior (pre-lease) ceiling.
        rv = _run_userprio_with_retry(
            condor, ["-cancelceilinglease", SUBMITTER]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_ceiling(condor, SUBMITTER, 10)

        # Cancelling when no lease is active must be rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelceilinglease", SUBMITTER]
        )
        assert rv.returncode != 0
        assert "no ceiling lease" in (rv.stderr + rv.stdout).lower()

    def test_floor_lease_lifecycle(self, condor):
        floor_user = "floorleaseuser@test.local"

        # Seed a baseline floor.
        rv = _run_userprio_with_retry(
            condor, ["-setfloor", floor_user, "5"]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, floor_user, "Floor", 5)

        # Install a lease.
        rv = _run_userprio_with_retry(
            condor,
            ["-setfloor", floor_user, "20", "-duration", "3600"],
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set floor lease" in rv.stdout
        _wait_for_attr(condor, floor_user, "Floor", 20)

        # Double-lease is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-setfloor", floor_user, "30",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        _wait_for_attr(condor, floor_user, "Floor", 20)

        # Cancel restores the prior floor.
        rv = _run_userprio_with_retry(
            condor, ["-cancelfloorlease", floor_user]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, floor_user, "Floor", 5)

        # Double-cancel is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelfloorlease", floor_user]
        )
        assert rv.returncode != 0
        assert "no floor lease" in (rv.stderr + rv.stdout).lower()

    def test_priority_factor_lease_lifecycle(self, condor):
        factor_user = "factorleaseuser@test.local"

        # Seed a baseline priority factor.
        rv = _run_userprio_with_retry(
            condor, ["-setfactor", factor_user, "2.0"]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, factor_user, "PriorityFactor", 2.0, cast=float)

        # Install a lease.
        rv = _run_userprio_with_retry(
            condor,
            ["-setfactor", factor_user, "7.5", "-duration", "3600"],
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set priority-factor lease" in rv.stdout
        _wait_for_attr(condor, factor_user, "PriorityFactor", 7.5, cast=float)

        # Double-lease is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-setfactor", factor_user, "9.0",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        _wait_for_attr(condor, factor_user, "PriorityFactor", 7.5, cast=float)

        # Cancel restores the prior priority factor.
        rv = _run_userprio_with_retry(
            condor, ["-cancelfactorlease", factor_user]
        )
        assert rv.returncode == 0, rv.stderr
        _wait_for_attr(condor, factor_user, "PriorityFactor", 2.0, cast=float)

        # Double-cancel is rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelfactorlease", factor_user]
        )
        assert rv.returncode != 0
        assert "no priority-factor lease" in (rv.stderr + rv.stdout).lower()
