#!/usr/bin/env pytest

# Benchmark for CCB streaming (proxy) mode: stand up a collector-as-CCB and run
# ccb_proxy_bench, which establishes one streamed (proxied) connection through
# the broker and blasts raw bytes in both directions, reporting the relay's
# average throughput each way.  This is a performance probe rather than a strict
# pass/fail test: it asserts the proxied connection was established and prints
# the measured rates.

import logging
from pathlib import Path

import pytest

from ornithology import action, Condor

logger = logging.getLogger(__name__)

BENCH_SECONDS = 5


def broker_sinful(condor):
    addr_file = condor.run_command(
        ["condor_config_val", "COLLECTOR_ADDRESS_FILE"]
    ).stdout.strip()
    return Path(addr_file).read_text().splitlines()[0].strip()


def ccb_proxy_bench_path(condor):
    libexec = condor.run_command(["condor_config_val", "LIBEXEC"]).stdout.strip()
    return str(Path(libexec) / "ccb_proxy_bench")


@action
def bench_condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            # IPv4 only, for a deterministic single-host network setup.
            "ENABLE_IPV6": "FALSE",
            # With shared port, the reverse-connect socket is established via
            # fd-passing whose buffering makes the bidirectional measurement
            # lopsided; a plain TCP reverse connect keeps the two directions even.
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR",
            # So the test can confirm the bytes really traversed the relay.
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
        },
    ) as condor:
        yield condor


class TestCCBProxyBench:
    def test_proxy_throughput(self, bench_condor):
        condor = bench_condor

        libexec = condor.run_command(
            ["condor_config_val", "LIBEXEC"]
        ).stdout.strip()
        bench = str(Path(libexec) / "ccb_proxy_bench")

        addr_file = condor.run_command(
            ["condor_config_val", "COLLECTOR_ADDRESS_FILE"]
        ).stdout.strip()
        broker = Path(addr_file).read_text().splitlines()[0].strip()

        result = condor.run_command(
            [bench, broker, str(BENCH_SECONDS)],
            timeout=BENCH_SECONDS + 30,
        )
        logger.info("ccb_proxy_bench output:\n%s", result.stdout)
        # Make the numbers visible in the test output.
        print("\n" + result.stdout)

        assert result.returncode == 0
        assert "throughput" in result.stdout

        # Confirm the broker actually proxied the connection, so the measured
        # rates are the relay's and not some direct path.
        collector_log = condor.collector_log.open()
        assert collector_log.wait(
            condition=lambda line: "streaming (proxy)" in line,
            timeout=30,
        )


@action
def reaper_condor(test_dir):
    with Condor(
        local_dir=test_dir / "reaper",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
            # Reap a never-completed streaming handshake quickly: short timeout
            # and a tight poll interval so the maintenance sweep runs often.
            "CCB_SERVER_STREAMING_HANDSHAKE_TIMEOUT": "3",
            "CCB_POLLING_INTERVAL": "1",
        },
    ) as condor:
        yield condor


class TestCCBProxyHandshakeTimeout:
    def test_broker_reaps_incomplete_handshake(self, reaper_condor):
        # A streaming request whose target registers but never connects back must
        # not pin the requester's socket forever: the broker reaps the half-open
        # session after the handshake timeout and replies with a failure, which
        # ccb_proxy_bench --no-reverse-connect verifies.
        condor = reaper_condor

        libexec = condor.run_command(
            ["condor_config_val", "LIBEXEC"]
        ).stdout.strip()
        bench = str(Path(libexec) / "ccb_proxy_bench")

        addr_file = condor.run_command(
            ["condor_config_val", "COLLECTOR_ADDRESS_FILE"]
        ).stdout.strip()
        broker = Path(addr_file).read_text().splitlines()[0].strip()

        result = condor.run_command(
            [bench, broker, "--no-reverse-connect"],
            timeout=90,
        )
        logger.info("ccb_proxy_bench --no-reverse-connect output:\n%s", result.stdout)

        # The requester received the broker's failure reply (the reap).
        assert result.returncode == 0, (
            "reaper test failed (rc=%s): %s" % (result.returncode, result.stderr)
        )
        assert "handshake reaped as expected" in result.stdout

        # And the broker logged that it timed the handshake out.
        collector_log = condor.collector_log.open()
        assert collector_log.wait(
            condition=lambda line: "handshake" in line and "timed out" in line,
            timeout=30,
        )


class TestCCBProxyConnectionLimits:
    # Each cap, set to 1, must make the broker refuse a second concurrent
    # streaming request.  A long handshake timeout keeps the first (pending)
    # session alive while ccb_proxy_bench --cap-test probes the limit.
    @pytest.mark.parametrize(
        "cap_knob",
        ["CCB_SERVER_MAX_STREAMING_SESSIONS", "CCB_SERVER_MAX_STREAMING_HANDSHAKES"],
    )
    def test_broker_enforces_streaming_limit(self, test_dir, cap_knob):
        with Condor(
            local_dir=test_dir / ("cap_" + cap_knob),
            config={
                "ENABLE_IPV6": "FALSE",
                "USE_SHARED_PORT": "FALSE",
                "DAEMON_LIST": "MASTER COLLECTOR",
                "COLLECTOR_DEBUG": "D_FULLDEBUG",
                cap_knob: "1",
                # Keep the first (pending) session from being reaped mid-test.
                "CCB_SERVER_STREAMING_HANDSHAKE_TIMEOUT": "60",
            },
        ) as condor:
            result = condor.run_command(
                [ccb_proxy_bench_path(condor), broker_sinful(condor), "--cap-test"],
                timeout=60,
            )
            logger.info(
                "ccb_proxy_bench --cap-test (%s) output:\n%s", cap_knob, result.stdout
            )
            assert result.returncode == 0, (
                "cap test failed (rc=%s): %s" % (result.returncode, result.stderr)
            )
            assert "limit enforced" in result.stdout
