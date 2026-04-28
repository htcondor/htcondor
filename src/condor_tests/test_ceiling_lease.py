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
            "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR",
        },
    ) as condor:
        yield condor


def _get_ceiling(condor, submitter):
    """Return the submitter's current ceiling (-1 means unlimited)."""
    rv = condor.run_command(["condor_userprio", "-allusers", "-l"])
    assert rv.returncode == 0, rv.stderr
    # Output is one classad per submitter separated by blank lines.
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
    raise AssertionError(f"submitter {submitter} not found:\n{rv.stdout}")


class TestCeilingLease:

    def test_lease_lifecycle(self, condor):
        # Seed a baseline ceiling so the submitter exists and we have a
        # known pre-lease value to revert to.
        rv = condor.run_command(
            ["condor_userprio", "-setceiling", SUBMITTER, "10"]
        )
        assert rv.returncode == 0, rv.stderr
        time.sleep(1)
        assert _get_ceiling(condor, SUBMITTER) == 10

        # Install a lease; ceiling should change to the leased value.
        rv = condor.run_command(
            ["condor_userprio", "-setceiling", SUBMITTER, "42",
             "-duration", "3600"]
        )
        assert rv.returncode == 0, rv.stderr
        assert "Set ceiling lease" in rv.stdout
        assert _get_ceiling(condor, SUBMITTER) == 42

        # A second lease on top of an in-effect lease must be rejected.
        rv = condor.run_command(
            ["condor_userprio", "-setceiling", SUBMITTER, "99",
             "-duration", "3600"]
        )
        assert rv.returncode != 0
        assert "already in effect" in (rv.stderr + rv.stdout)
        # And the original lease value is still in effect.
        assert _get_ceiling(condor, SUBMITTER) == 42

        # Cancel restores the prior (pre-lease) ceiling.
        rv = condor.run_command(
            ["condor_userprio", "-cancelceilinglease", SUBMITTER]
        )
        assert rv.returncode == 0, rv.stderr
        assert _get_ceiling(condor, SUBMITTER) == 10

        # Cancelling when no lease is active must be rejected.
        rv = condor.run_command(
            ["condor_userprio", "-cancelceilinglease", SUBMITTER]
        )
        assert rv.returncode != 0
        assert "no ceiling lease" in (rv.stderr + rv.stdout).lower()
