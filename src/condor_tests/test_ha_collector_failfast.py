#!/usr/bin/env pytest

# Regression test for the HA-collector fail-fast fix: when COLLECTOR_HOST
# lists more than one collector and one of them silently drops packets
# instead of promptly refusing the connection, condor_status must not block
# for the full (possibly TIMEOUT_MULTIPLIER-scaled) QUERY_TIMEOUT before
# falling over to the working collector. CollectorList::query() now probes
# reachability with a short, unmultiplied HA_COLLECTOR_PROBE_TIMEOUT first.
#
# The silent-drop collector is simulated with an address from 203.0.113.0/24
# (RFC 5737 TEST-NET-3): reserved for documentation, never assigned to a
# live host, so an outbound SYN to it is dropped along the way rather than
# answered immediately with a RST/ICMP the way a closed local port would be.
# This needs real outbound network access, which the project's CI runners
# have on all three platforms. Rather than assume that, the test first does
# a short preflight connect to the black hole address itself: if that fails
# immediately (no route out of this sandbox at all) instead of hanging, the
# environment can't reproduce a silently dropped connection here, and the
# test skips instead of passing (or failing) without having exercised
# anything.

import logging
import socket
import time

import pytest
from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

BLACKHOLE_COLLECTOR = "203.0.113.1:9618"

# How long to wait, during the preflight check, for a connect() attempt to
# the black hole address to *not* be immediately rejected.
EGRESS_CHECK_TIMEOUT = 2

PROBE_TIMEOUT = 3
# Comfortably above (probe timeout + a real query round trip), comfortably
# below the unfixed behavior's QUERY_TIMEOUT (default 60s, unmultiplied).
MAX_ELAPSED = 20


def _blackhole_address_is_reachable_like_a_blackhole():
    """
    True if connecting to BLACKHOLE_COLLECTOR hangs instead of failing
    right away -- i.e. this environment has real outbound network egress,
    so the address behaves like a silently dropped connection instead of
    an immediate "no route"/"network unreachable" error.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(EGRESS_CHECK_TIMEOUT)
    host, port = BLACKHOLE_COLLECTOR.split(":")
    try:
        s.connect((host, int(port)))
        # A real TCP handshake succeeding here would be startling for an
        # RFC 5737 TEST-NET-3 address, but if it happens the premise (a
        # black hole) doesn't hold either way, so treat it the same as
        # an outright failure.
        return False
    except socket.timeout:
        return True
    except OSError:
        return False
    finally:
        s.close()


@action
def ha_status_result(default_condor):
    if not _blackhole_address_is_reachable_like_a_blackhole():
        pytest.skip(
            "No outbound network egress to the TEST-NET-3 black hole address "
            f"({BLACKHOLE_COLLECTOR}) in this environment; can't simulate a silently dropped "
            "collector connection here."
        )

    ENV = {
        "_CONDOR_COLLECTOR_HOST": f"{BLACKHOLE_COLLECTOR}, {default_condor.collector_address}",
        # Force the dead entry to be tried first instead of the default
        # PID-seeded random order (CollectorList::query() picks randomly
        # unless HAD_USE_PRIMARY is set), so the probe path is exercised
        # deterministically every run.
        "_CONDOR_HAD_USE_PRIMARY": "true",
        "_CONDOR_HA_COLLECTOR_PROBE_TIMEOUT": str(PROBE_TIMEOUT),
        # condor_status runs as subsystem TOOL; a nonzero multiplier here
        # makes sure the probe timeout stays unscaled (MAX_ELAPSED would be
        # blown past if the probe were multiplied like QUERY_TIMEOUT is).
        "_CONDOR_TOOL_TIMEOUT_MULTIPLIER": "10",
    }

    with SetEnv(ENV):
        start = time.monotonic()
        result = default_condor.run_command(["condor_status"], timeout=90)
        elapsed = time.monotonic() - start

    return result, elapsed


class TestHACollectorFailFast:
    def test_query_succeeds_via_the_working_collector(self, ha_status_result):
        result, _ = ha_status_result
        assert result.returncode == 0

    def test_query_fails_over_within_probe_timeout(self, ha_status_result):
        _, elapsed = ha_status_result
        assert elapsed < MAX_ELAPSED
