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


def _query_ceiling(condor, submitter):
    """Return the submitter's current ceiling, or None if not present yet."""
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
            return int(ad.get("Ceiling", "-1"))
    return None


def _wait_for_ceiling(condor, submitter, expected, timeout=30):
    """Poll until the submitter's ceiling matches expected, or fail."""
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = _query_ceiling(condor, submitter)
        if last == expected:
            return
        time.sleep(0.5)
    raise AssertionError(
        f"timed out waiting for {submitter} ceiling to be {expected}; "
        f"last value was {last!r}"
    )


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
        # And the original lease value is still in effect.
        assert _query_ceiling(condor, SUBMITTER) == 42

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
