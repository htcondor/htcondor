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


def _outbound_broker_config(use_shared_port):
    return {
        "ENABLE_IPV6": "FALSE",
        "USE_SHARED_PORT": "TRUE" if use_shared_port else "FALSE",
        "DAEMON_LIST": "MASTER COLLECTOR"
        + (" SHARED_PORT" if use_shared_port else ""),
        "COLLECTOR_DEBUG": "D_FULLDEBUG",
        # Enable the outbound proxy and let it dial any (non-loopback) target;
        # CCB_PROXY_CONNECT is authenticated at DAEMON, so permit the local
        # requester at that level.
        "CCB_OUTBOUND_PROXY": "TRUE",
        "CCB_OUTBOUND_TARGET_ALLOWLIST": "0.0.0.0/0",
    }


@action
def outbound_condor(test_dir):
    with Condor(
        local_dir=test_dir / "outbound",
        config=_outbound_broker_config(use_shared_port=False),
    ) as condor:
        yield condor


@action
def outbound_condor_sharedport(test_dir):
    # Same outbound-proxy broker, but with shared port enabled -- the default
    # deployment mode.  The requester reaches the broker at its shared-port sinful
    # and the broker still dials + splices the target.
    with Condor(
        local_dir=test_dir / "outbound_sp",
        config=_outbound_broker_config(use_shared_port=True),
    ) as condor:
        yield condor


class TestCCBOutboundProxyBench:
    def _run_outbound_bench(self, condor):
        # The broker dials a plain listener on the requester's behalf
        # (CCB_PROXY_CONNECT) and splices the two sockets; ccb_proxy_bench
        # --outbound blasts raw bytes both ways over the relay and reports the
        # throughput each direction.  This exercises the server dial + splice +
        # backpressure path end-to-end.
        result = condor.run_command(
            [
                ccb_proxy_bench_path(condor),
                broker_sinful(condor),
                str(BENCH_SECONDS),
                "--outbound",
            ],
            timeout=BENCH_SECONDS + 30,
        )
        logger.info("ccb_proxy_bench --outbound output:\n%s", result.stdout)
        print("\n" + result.stdout)

        assert result.returncode == 0, (
            "outbound bench failed (rc=%s): %s" % (result.returncode, result.stderr)
        )
        assert "outbound-proxy relay throughput" in result.stdout

        # Both directions of the splice must actually carry bytes (guards against a
        # relay that establishes but pumps only one way).  This is enforced INSIDE the
        # bench, which primes each direction with a byte before the throughput blast
        # and exits non-zero if either direction never delivers -- so the returncode
        # check above is the bidirectional-liveness assertion.  We deliberately do NOT
        # assert on the printed per-direction rates or byte counts: the blast is a
        # saturating, winner-take-all measurement (systematically lopsided under
        # shared port), so the losing direction can deliver zero bytes *within the
        # fixed window* even though the relay carries it -- the priming, not the
        # blast, is the reliable liveness signal.

        # Confirm the broker actually dialed the target and relayed (guards against a
        # silent loopback bypass making the test pass without using the proxy).
        collector_log = condor.collector_log.open()
        assert collector_log.wait(
            condition=lambda line: "outbound proxy" in line or "established relay" in line,
            timeout=30,
        )

    def test_outbound_proxy_throughput(self, outbound_condor):
        self._run_outbound_bench(outbound_condor)

    def test_outbound_proxy_throughput_sharedport(self, outbound_condor_sharedport):
        # Outbound proxying must also work with shared port (the default mode).
        self._run_outbound_bench(outbound_condor_sharedport)


@action
def tunnel_outside(test_dir):
    # The exit broker: it dials targets directly.
    with Condor(
        local_dir=test_dir / "tunnel_outside",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
            "CCB_OUTBOUND_PROXY": "TRUE",
            "CCB_OUTBOUND_TARGET_ALLOWLIST": "0.0.0.0/0",
            # On a single host the inside broker would try the process-family
            # session (local peer) reaching this one, which -- being a separate
            # pool -- rejects it; a real deployment's exit broker is on another
            # host, so disable it here to emulate that.
            "SEC_USE_FAMILY_SESSION": "False",
        },
    ) as condor:
        yield condor


@action
def tunnel_inside(test_dir, tunnel_outside):
    # The entry broker: it forwards proxy requests to the outside broker rather
    # than dialing targets itself (CCB_OUTBOUND_NEXT_HOP).  We deliberately do NOT
    # set CCB_OUTBOUND_PROXY here: an inside CCB (one with a next hop) is a
    # forwarding outbound proxy by definition and must enable that mode on its own,
    # so this fixture also asserts that implication.
    with Condor(
        local_dir=test_dir / "tunnel_inside",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
            "SEC_USE_FAMILY_SESSION": "False",
            "CCB_OUTBOUND_NEXT_HOP": broker_sinful(tunnel_outside),
        },
    ) as condor:
        yield condor


@action
def restrictive_exit(test_dir):
    # An exit broker whose allow-list is a reserved TEST-NET range that cannot be
    # the host's real address, so every outbound-proxy request is refused before any
    # dial.  (CCB_OUTBOUND_PROXY, no next hop -- it dials directly.)
    with Condor(
        local_dir=test_dir / "restrictive_exit",
        config={
            "ENABLE_IPV6": "FALSE",
            "USE_SHARED_PORT": "FALSE",
            "DAEMON_LIST": "MASTER COLLECTOR",
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
            "CCB_OUTBOUND_PROXY": "TRUE",
            "CCB_OUTBOUND_TARGET_ALLOWLIST": "192.0.2.0/24",
            "SEC_USE_FAMILY_SESSION": "False",
        },
    ) as condor:
        yield condor


class TestCCBOutboundTunnelBench:
    def test_outbound_proxy_forwards_through_two_brokers(
        self, tunnel_outside, tunnel_inside
    ):
        # ccb_proxy_bench --outbound sends CCB_PROXY_CONNECT to the inside broker,
        # which forwards to the outside broker, which dials the bench's listener:
        # requester -> inside CCB -> outside CCB -> target.  Both brokers relay.
        result = tunnel_inside.run_command(
            [
                ccb_proxy_bench_path(tunnel_inside),
                broker_sinful(tunnel_inside),
                str(BENCH_SECONDS),
                "--outbound",
            ],
            timeout=BENCH_SECONDS + 40,
        )
        logger.info("two-broker outbound bench output:\n%s", result.stdout)
        assert result.returncode == 0, (
            "two-broker outbound bench failed (rc=%s): %s"
            % (result.returncode, result.stderr)
        )
        assert "outbound-proxy relay throughput" in result.stdout

        # Both directions carried bytes end-to-end.  Liveness is enforced inside the
        # bench (it primes each direction and exits non-zero otherwise), so the
        # returncode check above covers it; the winner-take-all rates/byte counts are
        # not asserted on -- see _run_outbound_bench for why.

        # Confirm it really went through both brokers: the inside broker forwarded
        # to its next hop (rather than dialing) and the outside broker dialed the
        # target.  The end-to-end throughput above could not exist otherwise.
        assert "forwarding to next hop for" in tunnel_inside.collector_log.path.read_text(), (
            "inside broker did not forward the outbound request to its next hop"
        )
        assert "dialing target" in tunnel_outside.collector_log.path.read_text(), (
            "outside broker did not dial the target"
        )

    def test_outbound_ttl_exhausted_is_refused(self, tunnel_outside, tunnel_inside):
        # A forwarding (inside) broker must refuse to forward a request whose TTL is
        # already exhausted, bounding the chain length.  --ttl 0 to the inside broker:
        # the request is well-formed and its target is allow-listed, but ttl <= 0 when
        # it would forward, so it is refused.
        result = tunnel_inside.run_command(
            [
                ccb_proxy_bench_path(tunnel_inside),
                broker_sinful(tunnel_inside),
                str(BENCH_SECONDS),
                "--outbound",
                "--ttl",
                "0",
            ],
            timeout=BENCH_SECONDS + 40,
        )
        assert result.returncode != 0, (
            "outbound request with an exhausted TTL was not refused: %s" % result.stdout
        )
        assert "outbound TTL expired" in tunnel_inside.collector_log.path.read_text(), (
            "inside broker did not refuse the TTL-exhausted forward"
        )

    def test_outbound_target_not_allowlisted_is_refused(self, restrictive_exit):
        # An exit broker refuses (before dialing) a target not permitted by its
        # CCB_OUTBOUND_TARGET_ALLOWLIST.  The bench's target is the host's real IP,
        # which the reserved TEST-NET allow-list cannot contain.
        result = restrictive_exit.run_command(
            [
                ccb_proxy_bench_path(restrictive_exit),
                broker_sinful(restrictive_exit),
                str(BENCH_SECONDS),
                "--outbound",
            ],
            timeout=BENCH_SECONDS + 40,
        )
        assert result.returncode != 0, (
            "outbound request to a non-allow-listed target succeeded: %s" % result.stdout
        )
        assert "disallowed target" in restrictive_exit.collector_log.path.read_text(), (
            "exit broker did not refuse the non-allow-listed target"
        )


def _ssrf_exit_config(test_dir_sub, **overrides):
    # Base config for a directly-dialing exit broker used by the SSRF-guard tests.
    cfg = {
        "ENABLE_IPV6": "FALSE",
        "USE_SHARED_PORT": "FALSE",
        "DAEMON_LIST": "MASTER COLLECTOR",
        "COLLECTOR_DEBUG": "D_FULLDEBUG",
        "CCB_OUTBOUND_PROXY": "TRUE",
        # The CCB_PROXY_CONNECT request authenticates at DAEMON; on one host a
        # separate-pool requester would otherwise try the process-family session,
        # so disable it (as the other tunnel tests do) so the request authenticates
        # cleanly and is refused specifically by the target check, not by auth.
        "SEC_USE_FAMILY_SESSION": "False",
    }
    cfg.update(overrides)
    return cfg


@action
def deny_wins_exit(test_dir):
    # An exit broker whose ALLOW-list would permit a loopback target but whose
    # DENY-list also matches it.  The deny-list is consulted first, so deny must win
    # even though the allow-list says yes -- proving deny-list precedence.
    with Condor(
        local_dir=test_dir / "deny_wins_exit",
        config=_ssrf_exit_config(
            test_dir,
            CCB_OUTBOUND_TARGET_ALLOWLIST="127.0.0.0/8",
            CCB_OUTBOUND_TARGET_DENYLIST="127.0.0.0/8",
        ),
    ) as condor:
        yield condor


@action
def default_denylist_exit(test_dir):
    # An exit broker with the DEFAULT deny-list (loopback/link-local) and the default
    # allow-list ("*").  Proves the built-in SSRF guard refuses a loopback target so
    # the broker cannot be used to reach services bound to its own host.
    with Condor(
        local_dir=test_dir / "default_denylist_exit",
        config=_ssrf_exit_config(test_dir),
    ) as condor:
        yield condor


@action
def resolve_deny_exit(test_dir):
    # An exit broker whose deny-list is a loopback CIDR ONLY (no "localhost" literal)
    # and whose allow-list is "*".  A hostname target that RESOLVES to loopback must
    # still be refused -- exercising the DNS-resolution deny check (only the
    # resolved-address match, not a literal name match, can catch it here).
    with Condor(
        local_dir=test_dir / "resolve_deny_exit",
        config=_ssrf_exit_config(
            test_dir,
            CCB_OUTBOUND_TARGET_DENYLIST="127.0.0.0/8",
            CCB_OUTBOUND_TARGET_ALLOWLIST="*",
        ),
    ) as condor:
        yield condor


class TestCCBOutboundSSRFGuard:
    # SSRF hardening: an outbound-proxy broker must refuse to dial targets that would
    # let a requester reach services on the broker's own host (loopback/link-local),
    # with the deny-list taking precedence over the allow-list.  ccb_proxy_bench
    # --outbound --target <sinful> aims the CCB_PROXY_CONNECT at a chosen target; a
    # refused request never dials, so the listener child is simply released.
    def _expect_refused(self, condor, target):
        result = condor.run_command(
            [
                ccb_proxy_bench_path(condor),
                broker_sinful(condor),
                str(BENCH_SECONDS),
                "--outbound",
                "--target",
                target,
            ],
            timeout=BENCH_SECONDS + 40,
        )
        assert result.returncode != 0, (
            "outbound-proxy request to %s was not refused: %s"
            % (target, result.stdout)
        )
        assert "disallowed target" in condor.collector_log.path.read_text(), (
            "broker did not log refusing the disallowed target %s" % target
        )

    def test_denylist_wins_over_allowlist(self, deny_wins_exit):
        # The same loopback target is on BOTH lists; deny must win.
        self._expect_refused(deny_wins_exit, "<127.0.0.1:1>")

    def test_default_loopback_denied(self, default_denylist_exit):
        # The default deny-list refuses a loopback IP target even with allow-list "*".
        self._expect_refused(default_denylist_exit, "<127.0.0.1:1>")

    def test_hostname_resolving_to_loopback_denied(self, resolve_deny_exit):
        # A hostname target that resolves to 127.0.0.1 is refused by the CIDR
        # deny-list via DNS resolution: the deny-list here carries no "localhost"
        # literal, so only the resolved-address check can catch it.
        self._expect_refused(resolve_deny_exit, "<localhost:1>")


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
