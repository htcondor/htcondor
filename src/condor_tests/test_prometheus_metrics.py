#!/usr/bin/env pytest

#
# Regression test for HTCONDOR-3374: condor_metricd should publish a
# Prometheus text-format file containing only the metrics configured for
# the Prometheus backend.
#
# Also covers HTTP serving of /metrics via DaemonCore's HTTP command
# handler, including unauthenticated access, HTTP Basic auth via an
# htpasswd file, and rejection of invalid credentials / unknown paths.
#

import base64
import hashlib
import http.client
import logging
import re
import socket
import time

from ornithology import (
    config, standup, action,
    Condor,
    write_file,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def _prom_sample_value(prom_text, sample_name):
    # Return the float value of a Prometheus sample line for sample_name, or None.
    # With PROMETHEUS_METRICS_INCLUDE_TIMESTAMP=true a sample line looks like:
    #   name{label="v",...} <value> <timestamp>
    # so the value is the second-to-last whitespace-separated token.
    for line in prom_text.splitlines():
        if line.startswith("#") or not line.strip():
            continue
        name = line.split(" ", 1)[0].split("{", 1)[0]
        if name == sample_name:
            parts = line.split()
            try:
                return float(parts[-2])
            except (IndexError, ValueError):
                return None
    return None


def _ganglia_publish_line(log_text, metric_name):
    # Return the most recent GANGLIA_LIB=NOOP "publishing <name>=..." log line for
    # metric_name, or None. That line carries both the value and a
    # "derivative=<0|1>" field, so callers can check how Ganglia typed the metric.
    needle = "publishing %s=" % metric_name
    found = None
    for line in log_text.splitlines():
        if needle in line:
            found = line
    return found

# NOTE: make both_backend_test_metric the last metric in the list to ensure it gets published to Ganglia before
# we check the Ganglia log, since Metricd processes metrics in the order they are defined in the config directory.
METRIC_DEFS = """
[
  Name = "ganglia_only_test_jobs";
  Value = 7;
  Desc = "Ganglia-only test metric";
  TargetType = "Scheduler";
  ExportMetric = "ganglia";
]
[
  Name = "prometheus_only_test_jobs";
  Value = 11;
  Desc = "Prometheus-only test metric";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
  PrometheusLabels = strcat("machine=\\"",Machine,"\\"");
]
[
  Name = "test_bytes_transferred";
  Value = 42;
  Desc = "Bytes transferred counter";
  Units = "bytes";
  TargetType = "Scheduler";
  Counter = true;
]
[
  Name = "invalid name!";
  Value = 1;
  Desc = "Invalid Prometheus name (should be skipped)";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
[
  Name = "override_test_metric";
  ganglia_name = "override_test_metric_ganglia";
  Value = 17;
  Desc = "Default description";
  Prometheus_Desc = "Prometheus-specific description";
  TargetType = "Scheduler";
]
[
  Name = "test_aggregate_sum_counter";
  Value = 5;
  Desc = "SUM aggregate of a counter";
  TargetType = "Scheduler";
  Aggregate = "SUM";
  Counter = true;
]
[
  Name = "test_aggregate_max_counter";
  Value = 5;
  Desc = "MAX aggregate of a counter";
  TargetType = "Scheduler";
  Aggregate = "MAX";
  Counter = true;
]
[
  Name = "test_aggregate_growth_counter";
  Value = time();
  Desc = "SUM aggregate of an increasing counter";
  TargetType = "Scheduler";
  Aggregate = "SUM";
  Counter = true;
  ExportMetric = "prometheus";
]
[
  Name = "both_backend_test_metric";
  Value = 13;
  Desc = "Default-export-everywhere metric";
  TargetType = "Scheduler";
]
"""


@standup
def condor_with_metricd(test_dir):
    metrics_dir = test_dir / "metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(metrics_dir / "00_test_metrics", METRIC_DEFS)

    prom_file = test_dir / "metrics.prom"

    config = {
        "DAEMON_LIST":                          "$(DAEMON_LIST) METRICD",
        "METRICD":                              "$(LIBEXEC)/condor_metricd",
        "GANGLIA_LIB":                          "NOOP",
        "GANGLIA_SEND_DATA_FOR_ALL_HOSTS":      "true",
        "PROMETHEUS_METRICS_FILE":              str(prom_file),
        "PROMETHEUS_METRICS_INCLUDE_TIMESTAMP": "true",
        "PROMETHEUS_DEFAULT_LABELS":            'pool="testpool"',
        "METRICD_INTERVAL":                     "5",
        "METRICD_METRICS_CONFIG_DIR":           str(metrics_dir),
        "METRICD_WANT_PROJECTION":              "true",
        "METRICD_DEBUG":                        "D_FULLDEBUG",
    }

    with Condor(test_dir / "condor", config=config) as condor:
        yield condor


@action
def prom_file_contents(test_dir, condor_with_metricd):
    prom_file = test_dir / "metrics.prom"
    deadline = time.time() + 60
    contents = None
    while time.time() < deadline:
        if prom_file.exists():
            text = prom_file.read_text()
            if "prometheus_only_test_jobs" in text:
                contents = text
                break
        time.sleep(2)
    return contents


@action
def ganglia_log_contents(condor_with_metricd):
    # In GANGLIA_LIB=NOOP mode, GangliaD::publishMetric() logs a
    # "noop mode: publishing <name>=<value>, ..." line at D_FULLDEBUG for
    # every metric routed to the Ganglia backend. Poll the MetricdLog until
    # at least one such line appears.
    log_file = condor_with_metricd.log_dir / "MetricdLog"
    deadline = time.time() + 60
    contents = None
    while time.time() < deadline:
        if log_file.exists():
            text = log_file.read_text(errors="replace")
            if "noop mode: publishing both_backend_test_metric=13" in text:
                contents = text
                break
        time.sleep(2)
    return contents


@action
def prom_file_with_aggregate(test_dir, condor_with_metricd):
    # Aggregate derivative (counter) metrics are NOT published on the first
    # metricd cycle: computing a per-daemon delta requires a previous value,
    # so they first appear on the second publication cycle. Poll until the
    # SUM aggregate counter shows up (its MAX-aggregate sibling is published in
    # the same cycle, so once one appears both do).
    prom_file = test_dir / "metrics.prom"
    deadline = time.time() + 120
    contents = None
    while time.time() < deadline:
        if prom_file.exists():
            text = prom_file.read_text()
            if "test_aggregate_sum_counter" in text:
                contents = text
                break
        time.sleep(2)
    return contents


@action
def ganglia_log_with_aggregate(condor_with_metricd):
    # Like ganglia_log_contents, but waits for an aggregate counter to be
    # published to Ganglia. Aggregate derivative metrics first appear on the
    # second cycle (they need a previous value to compute the per-daemon delta).
    log_file = condor_with_metricd.log_dir / "MetricdLog"
    deadline = time.time() + 120
    contents = None
    while time.time() < deadline:
        if log_file.exists():
            text = log_file.read_text(errors="replace")
            if "publishing test_aggregate_sum_counter=" in text:
                contents = text
                break
        time.sleep(2)
    return contents


@action
def growth_counter_samples(test_dir, condor_with_metricd):
    # test_aggregate_growth_counter uses Value=time(), which advances every
    # metricd cycle, so its per-daemon delta is positive on every cycle. A
    # correct cumulative counter therefore strictly increases from one cycle to
    # the next, whereas a per-interval gauge would just hover near a single
    # interval's worth. Capture two successive published values so the test can
    # confirm the running total accumulates.
    prom_file = test_dir / "metrics.prom"
    name = "test_aggregate_growth_counter_total"
    deadline = time.time() + 120
    first = None
    while time.time() < deadline:
        if prom_file.exists():
            v = _prom_sample_value(prom_file.read_text(), name)
            if v is not None:
                if first is None:
                    first = v
                elif v > first:
                    return (first, v)
        time.sleep(2)
    return (first, None)


class TestPrometheusMetrics:
    def test_file_exists(self, prom_file_contents):
        assert prom_file_contents is not None

    def test_has_help_and_type_lines(self, prom_file_contents):
        assert "# HELP prometheus_only_test_jobs" in prom_file_contents
        assert "# TYPE prometheus_only_test_jobs" in prom_file_contents

    def test_ganglia_only_metric_absent(self, prom_file_contents):
        assert "ganglia_only_test_jobs" not in prom_file_contents

    def test_prometheus_only_metric_present(self, prom_file_contents):
        assert "prometheus_only_test_jobs" in prom_file_contents

    def test_per_metric_label_present(self, prom_file_contents):
        assert "machine=" in prom_file_contents

    def test_default_label_present(self, prom_file_contents):
        assert 'pool="testpool"' in prom_file_contents

    def test_counter_gets_total_suffix(self, prom_file_contents):
        assert "test_bytes_transferred_bytes_total" in prom_file_contents

    def test_counter_type_annotation(self, prom_file_contents):
        assert "# TYPE test_bytes_transferred_bytes_total counter" in prom_file_contents

    def test_gauge_type_annotation(self, prom_file_contents):
        assert "# TYPE prometheus_only_test_jobs gauge" in prom_file_contents

    def test_invalid_name_metric_absent(self, prom_file_contents):
        assert "invalid name" not in prom_file_contents
        assert "invalid_name" not in prom_file_contents

    def test_timestamps_present(self, prom_file_contents):
        # When PROMETHEUS_METRICS_INCLUDE_TIMESTAMP is true, each sample
        # line ends with a trailing millisecond timestamp (>= 10 digits).
        found_timestamped = False
        for line in prom_file_contents.splitlines():
            if line.startswith("#") or not line.strip():
                continue
            parts = line.rsplit(" ", 1)
            if len(parts) == 2 and parts[1].isdigit() and len(parts[1]) >= 10:
                found_timestamped = True
                break
        assert found_timestamped

    def test_both_backend_metric_present(self, prom_file_contents):
        assert "both_backend_test_metric" in prom_file_contents

    def test_ganglia_log_present(self, ganglia_log_contents):
        assert ganglia_log_contents is not None

    def test_ganglia_publishes_both_backend_metric(self, ganglia_log_contents):
        assert "noop mode: publishing both_backend_test_metric=13" in ganglia_log_contents

    def test_ganglia_publishes_ganglia_only_metric(self, ganglia_log_contents):
        assert "noop mode: publishing ganglia_only_test_jobs=7" in ganglia_log_contents

    def test_ganglia_skips_prometheus_only_metric(self, ganglia_log_contents):
        assert "publishing prometheus_only_test_jobs" not in ganglia_log_contents

    # --- backend-decorated attribute overrides (e.g. Ganglia_Name) ---

    def test_override_prometheus_uses_default_name(self, prom_file_contents):
        # No Prometheus_Name override, so Prometheus uses the generic Name.
        assert "override_test_metric" in prom_file_contents

    def test_override_prometheus_ignores_ganglia_name(self, prom_file_contents):
        # The Ganglia_Name override must NOT leak into the Prometheus backend.
        assert "override_test_metric_ganglia" not in prom_file_contents

    def test_override_prometheus_uses_prometheus_desc(self, prom_file_contents):
        # Prometheus_Desc overrides Desc only for the Prometheus backend.
        assert "Prometheus-specific description" in prom_file_contents

    def test_override_ganglia_uses_ganglia_name(self, ganglia_log_contents):
        # Ganglia_Name overrides Name only for the Ganglia backend.
        assert "noop mode: publishing override_test_metric_ganglia=17" in ganglia_log_contents

    def test_override_ganglia_ignores_default_name(self, ganglia_log_contents):
        # The generic Name must not be used by Ganglia when an override exists.
        assert "publishing override_test_metric=17" not in ganglia_log_contents

    def test_override_ganglia_uses_default_desc(self, ganglia_log_contents):
        # No Ganglia_Desc override, so Ganglia uses the generic Desc, and the
        # Prometheus-specific Desc must not leak into the Ganglia backend.
        assert "desc=Default description" in ganglia_log_contents
        assert "Prometheus-specific description" not in ganglia_log_contents

    # --- aggregate counters ---
    #
    # Only a SUM of a counter is itself a counter; MIN/MAX/AVG of a counter are
    # rate-like quantities and stay gauges. This decision lives in the
    # backend-agnostic Metric::convertToNonAggregateValue(), so it must hold for
    # BOTH the Prometheus and Ganglia backends, and the published values must be
    # correct (the accumulated delta, not the raw summed value).

    def test_aggregate_counter_present(self, prom_file_with_aggregate):
        assert prom_file_with_aggregate is not None

    # Prometheus backend

    def test_aggregate_sum_counter_is_prometheus_counter(self, prom_file_with_aggregate):
        # A SUM of a derivative metric is integrated into a running cumulative
        # total and published as a Prometheus counter (with the _total suffix),
        # not as a per-interval gauge.
        assert "# TYPE test_aggregate_sum_counter_total counter" in prom_file_with_aggregate

    def test_aggregate_sum_counter_prometheus_value(self, prom_file_with_aggregate):
        # The source value is constant (5), so every per-daemon delta is 0 and
        # the accumulated counter is exactly 0 -- NOT the raw summed value (5),
        # which is what would be published if we summed values instead of deltas.
        assert _prom_sample_value(prom_file_with_aggregate, "test_aggregate_sum_counter_total") == 0

    def test_aggregate_max_counter_stays_prometheus_gauge(self, prom_file_with_aggregate):
        assert "# TYPE test_aggregate_max_counter gauge" in prom_file_with_aggregate
        assert "test_aggregate_max_counter_total" not in prom_file_with_aggregate

    # Ganglia backend (GANGLIA_LIB=NOOP logs each published metric, including its
    # value and a derivative=<0|1> field)

    def test_aggregate_sum_counter_is_ganglia_derivative(self, ganglia_log_with_aggregate):
        assert ganglia_log_with_aggregate is not None
        line = _ganglia_publish_line(ganglia_log_with_aggregate, "test_aggregate_sum_counter")
        assert line is not None
        # Published as a derivative (counter) ...
        assert "derivative=1" in line
        # ... with the correct accumulated value of 0 (constant source -> 0 delta).
        assert "publishing test_aggregate_sum_counter=0," in line

    def test_aggregate_max_counter_is_ganglia_gauge(self, ganglia_log_with_aggregate):
        line = _ganglia_publish_line(ganglia_log_with_aggregate, "test_aggregate_max_counter")
        assert line is not None
        # MAX of a counter stays a gauge, so Ganglia must NOT mark it derivative.
        assert "derivative=0" in line

    # Accumulation: an increasing source must yield a strictly increasing counter.
    # (The accumulation code is shared by both backends, so checking Prometheus
    # is sufficient; the constant-source cases above already confirm Ganglia and
    # Prometheus agree on the value.)

    def test_aggregate_counter_accumulates(self, growth_counter_samples):
        first, second = growth_counter_samples
        assert first is not None and second is not None
        # Strictly increasing across cycles confirms the per-interval deltas are
        # integrated into a running total, rather than each cycle's value simply
        # replacing the last (which is what a gauge / per-interval value does).
        assert second > first


# ---------------------------------------------------------------------------
# HTTP serving tests
# ---------------------------------------------------------------------------

def _metricd_http_port(condor):
    """
    Extract the MetricD command-socket port from MetricdLog.
    DaemonCore logs a line like:
        DaemonCore: command socket at <127.0.0.1:PORT?...>
    or (when shared-port is not used):
        DaemonCore: non-shared command socket at <127.0.0.1:PORT?...>
    Returns (host, port) or raises RuntimeError if not found within 30 s.
    """
    log_file = condor.log_dir / "MetricdLog"
    pattern = re.compile(r"DaemonCore:.*command socket at <([^:>]+):(\d+)[?]")
    deadline = time.time() + 30
    while time.time() < deadline:
        if log_file.exists():
            for line in log_file.read_text(errors="replace").splitlines():
                m = pattern.search(line)
                if m:
                    return m.group(1), int(m.group(2))
        time.sleep(0.5)
    raise RuntimeError("Could not determine MetricD HTTP port from log")


def _http_get(host, port, path, headers=None, timeout=10):
    """
    Perform a plain HTTP/1.0 GET and return (status_code, body_text).
    """
    conn = http.client.HTTPConnection(host, port, timeout=timeout)
    conn.request("GET", path, headers=headers or {})
    resp = conn.getresponse()
    body = resp.read().decode(errors="replace")
    conn.close()
    return resp.status, body


def _make_htpasswd_sha1(path, user, password):
    """
    Write an Apache-compatible {SHA} htpasswd entry.
    {SHA} is SHA-1 of the password, base64-encoded.
    """
    digest = hashlib.sha1(password.encode()).digest()
    encoded = base64.b64encode(digest).decode()
    path.write_text(f"{user}:{{SHA}}{encoded}\n")


def _basic_auth_header(user, password):
    token = base64.b64encode(f"{user}:{password}".encode()).decode()
    return {"Authorization": f"Basic {token}"}


# --- Standup: metricd with HTTP serving, no auth ----------------------------

@standup
def condor_with_http(test_dir):
    metrics_dir = test_dir / "http_metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(
        metrics_dir / "00_http_test_metrics",
        """
[
  Name = "http_test_gauge";
  Value = 42;
  Desc = "HTTP test gauge";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
""",
    )
    prom_file = test_dir / "http_metrics.prom"

    cfg = {
        "DAEMON_LIST":                 "$(DAEMON_LIST) METRICD",
        "METRICD":                     "$(LIBEXEC)/condor_metricd",
        "GANGLIA_LIB":                 "NOOP",
        "GANGLIA_SEND_DATA_FOR_ALL_HOSTS": "true",
        "PROMETHEUS_METRICS_FILE":     str(prom_file),
        "METRICD_INTERVAL":            "5",
        "METRICD_METRICS_CONFIG_DIR":  str(metrics_dir),
        "METRICD_DEBUG":               "D_FULLDEBUG D_COMMAND",
        # Give metricd a stable shared-port socket name so that
        # condor_shared_port can forward HTTP connections to it.
        "METRICD_ARGS":                "-sock metricd",
        "SHARED_PORT_HTTP_FORWARDING_ID": "metricd",
        # No PROMETHEUS_HTTP_AUTH_FILE → unauthenticated access allowed.
    }
    with Condor(test_dir / "condor_http", config=cfg) as condor:
        yield condor


@action
def http_host_port(test_dir, condor_with_http):
    return _metricd_http_port(condor_with_http)


@action
def http_metrics_ready(test_dir, condor_with_http):
    """Wait until metricd has written the prom file at least once."""
    prom_file = test_dir / "http_metrics.prom"
    deadline = time.time() + 60
    while time.time() < deadline:
        if prom_file.exists() and "http_test_gauge" in prom_file.read_text():
            return True
        time.sleep(2)
    return False


# --- Standup: metricd with HTTP Basic auth ----------------------------------

@standup
def condor_with_http_auth(test_dir):
    metrics_dir = test_dir / "auth_metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(
        metrics_dir / "00_auth_test_metrics",
        """
[
  Name = "auth_test_gauge";
  Value = 7;
  Desc = "Auth HTTP test gauge";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
""",
    )
    prom_file  = test_dir / "auth_metrics.prom"
    passwd_file = test_dir / "test.htpasswd"
    _make_htpasswd_sha1(passwd_file, "prometheus", "s3cr3t")

    cfg = {
        "DAEMON_LIST":                     "$(DAEMON_LIST) METRICD",
        "METRICD":                         "$(LIBEXEC)/condor_metricd",
        "GANGLIA_LIB":                     "NOOP",
        "GANGLIA_SEND_DATA_FOR_ALL_HOSTS": "true",
        "PROMETHEUS_METRICS_FILE":         str(prom_file),
        "PROMETHEUS_HTTP_AUTH_FILE":       str(passwd_file),
        "METRICD_INTERVAL":                "5",
        "METRICD_METRICS_CONFIG_DIR":      str(metrics_dir),
        "METRICD_DEBUG":                   "D_FULLDEBUG D_COMMAND",
        # Give metricd a stable shared-port socket name so that
        # condor_shared_port can forward HTTP connections to it.
        "METRICD_ARGS":                    "-sock metricd",
        "SHARED_PORT_HTTP_FORWARDING_ID":  "metricd",
    }
    with Condor(test_dir / "condor_auth", config=cfg) as condor:
        yield condor


@action
def auth_host_port(test_dir, condor_with_http_auth):
    return _metricd_http_port(condor_with_http_auth)


@action
def auth_metrics_ready(test_dir, condor_with_http_auth):
    prom_file = test_dir / "auth_metrics.prom"
    deadline = time.time() + 60
    while time.time() < deadline:
        if prom_file.exists() and "auth_test_gauge" in prom_file.read_text():
            return True
        time.sleep(2)
    return False


# ---------------------------------------------------------------------------
# Test classes
# ---------------------------------------------------------------------------

class TestPrometheusHTTP:
    """Verify that the metricd HTTP command handler serves /metrics correctly
    when no password file is configured (open access)."""

    def test_port_found(self, http_host_port):
        host, port = http_host_port
        assert host and port > 0

    def test_metrics_file_written(self, http_metrics_ready):
        assert http_metrics_ready

    def test_get_metrics_returns_200(self, http_host_port, http_metrics_ready):
        host, port = http_host_port
        status, _ = _http_get(host, port, "/metrics")
        assert status == 200

    def test_get_metrics_content_type(self, http_host_port, http_metrics_ready):
        # A plain HTTP/1.0 response should carry the Prometheus content-type.
        # http.client doesn't expose headers easily for 1.0, so we check via
        # a raw socket to avoid version negotiation surprises.
        host, port = http_host_port
        raw = socket.create_connection((host, port), timeout=10)
        raw.sendall(b"GET /metrics HTTP/1.0\r\nHost: localhost\r\n\r\n")
        response = b""
        while True:
            chunk = raw.recv(4096)
            if not chunk:
                break
            response += chunk
        raw.close()
        header_block = response.split(b"\r\n\r\n", 1)[0].decode(errors="replace")
        assert "text/plain" in header_block
        assert "0.0.4" in header_block

    def test_get_metrics_body_contains_metric(self, http_host_port, http_metrics_ready):
        host, port = http_host_port
        _, body = _http_get(host, port, "/metrics")
        assert "http_test_gauge" in body

    def test_get_metrics_body_has_help_and_type(self, http_host_port, http_metrics_ready):
        host, port = http_host_port
        _, body = _http_get(host, port, "/metrics")
        assert "# HELP http_test_gauge" in body
        assert "# TYPE http_test_gauge" in body

    def test_unknown_path_returns_404(self, http_host_port):
        host, port = http_host_port
        status, _ = _http_get(host, port, "/notfound")
        assert status == 404

    def test_root_path_returns_404(self, http_host_port):
        host, port = http_host_port
        status, _ = _http_get(host, port, "/")
        assert status == 404

    def test_multiple_requests_served(self, http_host_port, http_metrics_ready):
        """The handler must be able to serve more than one request."""
        host, port = http_host_port
        for _ in range(3):
            status, body = _http_get(host, port, "/metrics")
            assert status == 200
            assert "http_test_gauge" in body


class TestPrometheusHTTPAuth:
    """Verify HTTP Basic auth enforcement via PROMETHEUS_HTTP_AUTH_FILE."""

    def test_port_found(self, auth_host_port):
        host, port = auth_host_port
        assert host and port > 0

    def test_metrics_file_written(self, auth_metrics_ready):
        assert auth_metrics_ready

    def test_no_credentials_returns_401(self, auth_host_port, auth_metrics_ready):
        host, port = auth_host_port
        status, _ = _http_get(host, port, "/metrics")
        assert status == 401

    def test_no_credentials_has_www_authenticate(self, auth_host_port, auth_metrics_ready):
        host, port = auth_host_port
        raw = socket.create_connection((host, port), timeout=10)
        raw.sendall(b"GET /metrics HTTP/1.0\r\nHost: localhost\r\n\r\n")
        response = b""
        while True:
            chunk = raw.recv(4096)
            if not chunk:
                break
            response += chunk
        raw.close()
        header_block = response.split(b"\r\n\r\n", 1)[0].decode(errors="replace")
        assert "WWW-Authenticate" in header_block
        assert "Basic" in header_block

    def test_valid_credentials_returns_200(self, auth_host_port, auth_metrics_ready):
        host, port = auth_host_port
        status, body = _http_get(
            host, port, "/metrics",
            headers=_basic_auth_header("prometheus", "s3cr3t"),
        )
        assert status == 200
        assert "auth_test_gauge" in body

    def test_wrong_password_returns_401(self, auth_host_port, auth_metrics_ready):
        host, port = auth_host_port
        status, _ = _http_get(
            host, port, "/metrics",
            headers=_basic_auth_header("prometheus", "wrongpassword"),
        )
        assert status == 401

    def test_wrong_user_returns_401(self, auth_host_port, auth_metrics_ready):
        host, port = auth_host_port
        status, _ = _http_get(
            host, port, "/metrics",
            headers=_basic_auth_header("baduser", "s3cr3t"),
        )
        assert status == 401

    def test_valid_credentials_body_has_metric(self, auth_host_port, auth_metrics_ready):
        host, port = auth_host_port
        _, body = _http_get(
            host, port, "/metrics",
            headers=_basic_auth_header("prometheus", "s3cr3t"),
        )
        assert "# HELP auth_test_gauge" in body
        assert "# TYPE auth_test_gauge gauge" in body

    def test_404_path_does_not_leak_on_auth(self, auth_host_port, auth_metrics_ready):
        """A 404 on an unknown path should not require credentials."""
        host, port = auth_host_port
        status, _ = _http_get(host, port, "/notfound")
        assert status == 404

