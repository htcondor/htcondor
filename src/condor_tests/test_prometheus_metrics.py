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
import signal
import socket
import ssl
import subprocess
import time

from contextlib import contextmanager

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
METRIC_DEFS = r"""
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
  PrometheusLabels = [ machine = Machine ];
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
  Name = "label_syntax_test_metric";
  Value = 23;
  Desc = "Exercises the PrometheusLabels ClassAd syntax";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
  PrometheusLabels = [
      /* CondorVersion is a schedd-ad attribute that no other metric in this
         file references and that is not in metricd's hardcoded projection
         set, so this label only resolves if GetExternalReferences() recursed
         into this nested ad when the collector projection was computed. */
      version   = CondorVersion;
      /* value rendering by evaluated type */
      literal   = "plain";
      computed  = strcat("v", string(2 + 3));
      number    = 42;
      flag      = true;
      /* metricd, not the admin, is responsible for quoting and escaping */
      tricky    = "a,b\"q\"c\\d";
      /* UNDEFINED and ERROR omit the label rather than emitting it empty */
      missing   = NoSuchAttributeOnASchedd;
      bad_error = 1 / 0;
      /* illegal label name: rejected and reported once at config-read time */
      __reserved = "nope";
      /* overrides the pool-wide default of the same name */
      pool      = "override";
  ];
]
[
  Name = "legacy_label_syntax_metric";
  Value = 3;
  Desc = "The old string label syntax is no longer accepted";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
  PrometheusLabels = "machine=notalabel";
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
        # New-style label ad. CondorPlatform is deliberately an attribute that
        # nothing else in this test references and that is not in metricd's
        # hardcoded projection set, so the platform label only resolves if
        # PrometheusD::extraProjectionRefs() contributed it to the projection.
        "PROMETHEUS_DEFAULT_LABELS":            '[ pool = "testpool"; platform = CondorPlatform ]',
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

    # --- PrometheusLabels / PROMETHEUS_DEFAULT_LABELS as a ClassAd ---
    #
    # Labels are a nested ClassAd whose attribute names are label names and
    # whose attribute values are expressions evaluated against the daemon ad.

    def _labels(self, prom_text, sample_name):
        # Return the raw label-set text (without the enclosing braces) of the
        # first sample line for sample_name, or None. Label values may contain
        # spaces (CondorVersion does), so match greedily up to the last "} ".
        pattern = re.compile(re.escape(sample_name) + r"\{(.*)\} ")
        for line in prom_text.splitlines():
            if line.startswith("#") or not line.strip():
                continue
            m = pattern.match(line)
            if m:
                return m.group(1)
        return None

    def test_label_syntax_metric_present(self, prom_file_contents):
        assert self._labels(prom_file_contents, "label_syntax_test_metric") is not None

    def test_label_expression_evaluated_against_daemon_ad(self, prom_file_contents):
        # PROJECTION REGRESSION: CondorVersion is referenced only from inside
        # a nested PrometheusLabels ad, and METRICD_WANT_PROJECTION is on. If
        # ClassAd::GetExternalReferences() did not recurse into the nested ad,
        # CondorVersion would be dropped from the collector query, the label
        # would evaluate to UNDEFINED, and it would be omitted entirely.
        labels = self._labels(prom_file_contents, "label_syntax_test_metric")
        m = re.search(r'version="([^"]*)"', labels)
        assert m is not None, "version label missing: " + labels
        assert m.group(1).startswith("$CondorVersion:"), m.group(1)

    def test_default_label_expression_evaluated_against_daemon_ad(self, prom_file_contents):
        # Same check for PROMETHEUS_DEFAULT_LABELS, whose references reach the
        # projection through PrometheusD::extraProjectionRefs() rather than
        # through the metric-definition walk.
        labels = self._labels(prom_file_contents, "prometheus_only_test_jobs")
        m = re.search(r'platform="([^"]*)"', labels)
        assert m is not None, "platform label missing: " + labels
        assert m.group(1).startswith("$CondorPlatform:"), m.group(1)

    def test_label_value_rendering_by_type(self, prom_file_contents):
        labels = self._labels(prom_file_contents, "label_syntax_test_metric")
        assert 'literal="plain"' in labels
        assert 'computed="v5"' in labels     # strcat(...) of a string() of an int
        assert 'number="42"' in labels       # integer
        assert 'flag="true"' in labels       # boolean

    def test_label_value_escaping_is_automatic(self, prom_file_contents):
        # The label value is the 9 characters  a,b"q"c\d  . metricd must quote
        # and escape it; nothing was escaped by hand in the metric definition.
        labels = self._labels(prom_file_contents, "label_syntax_test_metric")
        assert r'tricky="a,b\"q\"c\\d"' in labels

    def test_undefined_label_is_omitted(self, prom_file_contents):
        labels = self._labels(prom_file_contents, "label_syntax_test_metric")
        assert "missing=" not in labels

    def test_error_label_is_omitted(self, prom_file_contents):
        labels = self._labels(prom_file_contents, "label_syntax_test_metric")
        assert "bad_error=" not in labels

    def test_illegal_label_name_not_published(self, prom_file_contents):
        assert "__reserved" not in prom_file_contents

    def test_illegal_label_name_reported_at_config_time(self, ganglia_log_contents):
        assert "'__reserved' in PrometheusLabels" in ganglia_log_contents

    def test_illegal_label_name_reported_only_once(self, ganglia_log_contents):
        # The complaint is issued from the one instance that walks every metric
        # definition, so it must not be repeated once per backend or per cycle.
        assert ganglia_log_contents.count("'__reserved' in PrometheusLabels") == 1

    def test_per_metric_label_overrides_default(self, prom_file_contents):
        labels = self._labels(prom_file_contents, "label_syntax_test_metric")
        assert 'pool="override"' in labels
        assert 'pool="testpool"' not in labels

    def test_default_label_survives_on_other_metrics(self, prom_file_contents):
        labels = self._labels(prom_file_contents, "prometheus_only_test_jobs")
        assert 'pool="testpool"' in labels

    def test_string_valued_labels_rejected(self, prom_file_contents):
        # The old label-set-string syntax is gone; a string-valued
        # PrometheusLabels contributes no labels at all.
        labels = self._labels(prom_file_contents, "legacy_label_syntax_metric")
        assert labels is not None
        assert "notalabel" not in labels
        # ...but the pool-wide defaults still apply to that metric.
        assert 'pool="testpool"' in labels

    def test_string_valued_labels_reported_at_config_time(self, ganglia_log_contents):
        assert "must be a ClassAd of label expressions" in ganglia_log_contents

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


def _make_self_signed_cert(cert_path, key_path):
    """
    Generate a self-signed PEM certificate/key pair for CN=localhost using the
    openssl CLI. metricd enables HTTPS when AUTH_SSL_SERVER_CERTFILE and
    AUTH_SSL_SERVER_KEYFILE point at a valid cert/key.
    """
    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-keyout", str(key_path), "-out", str(cert_path),
            "-days", "1", "-nodes", "-subj", "/CN=localhost",
        ],
        check=True,
        capture_output=True,
    )


# --- Standup: PROMETHEUS_DEFAULT_LABELS in config-heredoc long form ---------
#
# The main standup above writes the default label ad bracketed on one line,
# which metricd parses with ClassAdParser. The long form inside a @=end
# heredoc is a different code path (initAdFromString), and it is the form the
# documentation recommends for more than a couple of labels, so give it its
# own minimal pool.

@standup
def condor_with_heredoc_labels(test_dir):
    metrics_dir = test_dir / "heredoc_metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(
        metrics_dir / "00_heredoc_test_metrics",
        """
[
  Name = "heredoc_test_gauge";
  Value = 5;
  Desc = "Heredoc default-labels test gauge";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
""",
    )
    prom_file = test_dir / "heredoc_metrics.prom"

    cfg = {
        "DAEMON_LIST":                 "$(DAEMON_LIST) METRICD",
        "METRICD":                     "$(LIBEXEC)/condor_metricd",
        "GANGLIA_LIB":                 "NOOP",
        "GANGLIA_SEND_DATA_FOR_ALL_HOSTS": "true",
        "PROMETHEUS_METRICS_FILE":     str(prom_file),
        "METRICD_INTERVAL":            "5",
        "METRICD_METRICS_CONFIG_DIR":  str(metrics_dir),
        "METRICD_WANT_PROJECTION":     "true",
        "METRICD_DEBUG":               "D_FULLDEBUG",
    }
    raw_config = r"""
        PROMETHEUS_DEFAULT_LABELS @=end
           pool     = "heredocpool"
           platform = CondorPlatform
           quoted   = "a,b\"q\"c"
        @end
    """
    with Condor(test_dir / "condor_heredoc", config=cfg, raw_config=raw_config) as condor:
        yield condor


@action
def heredoc_prom_contents(test_dir, condor_with_heredoc_labels):
    prom_file = test_dir / "heredoc_metrics.prom"
    deadline = time.time() + 60
    while time.time() < deadline:
        if prom_file.exists():
            text = prom_file.read_text()
            if "heredoc_test_gauge" in text:
                return text
        time.sleep(2)
    return None


class TestPrometheusHeredocDefaultLabels:
    def test_prom_file_written(self, heredoc_prom_contents):
        assert heredoc_prom_contents is not None

    def test_literal_default_label(self, heredoc_prom_contents):
        assert 'pool="heredocpool"' in heredoc_prom_contents

    def test_expression_default_label(self, heredoc_prom_contents):
        # Evaluated against the daemon ad, and its reference reached the
        # collector projection via PrometheusD::extraProjectionRefs().
        m = re.search(r'platform="([^"]*)"', heredoc_prom_contents)
        assert m is not None, heredoc_prom_contents
        assert m.group(1).startswith("$CondorPlatform:"), m.group(1)

    def test_default_label_value_escaped(self, heredoc_prom_contents):
        assert r'quoted="a,b\"q\"c"' in heredoc_prom_contents


def _https_get(host, port, path, headers=None, timeout=10):
    """
    Perform an HTTPS GET and return (status_code, body_text). The metricd cert
    is self-signed, so certificate verification is disabled.
    """
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    conn = http.client.HTTPSConnection(host, port, timeout=timeout, context=ctx)
    conn.request("GET", path, headers=headers or {})
    resp = conn.getresponse()
    body = resp.read().decode(errors="replace")
    conn.close()
    return resp.status, body


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


# --- Standup: metricd with HTTPS (TLS) serving ------------------------------

@standup
def condor_with_https(test_dir):
    metrics_dir = test_dir / "https_metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(
        metrics_dir / "00_https_test_metrics",
        """
[
  Name = "https_test_gauge";
  Value = 99;
  Desc = "HTTPS test gauge";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
""",
    )
    prom_file = test_dir / "https_metrics.prom"
    cert_file = test_dir / "metricd_cert.pem"
    key_file  = test_dir / "metricd_key.pem"
    _make_self_signed_cert(cert_file, key_file)

    cfg = {
        "DAEMON_LIST":                     "$(DAEMON_LIST) METRICD",
        "METRICD":                         "$(LIBEXEC)/condor_metricd",
        "GANGLIA_LIB":                     "NOOP",
        "GANGLIA_SEND_DATA_FOR_ALL_HOSTS": "true",
        "PROMETHEUS_METRICS_FILE":         str(prom_file),
        # Presence of a readable cert/key pair enables HTTPS on the command port.
        "AUTH_SSL_SERVER_CERTFILE":        str(cert_file),
        "AUTH_SSL_SERVER_KEYFILE":         str(key_file),
        "METRICD_INTERVAL":                "5",
        "METRICD_METRICS_CONFIG_DIR":      str(metrics_dir),
        "METRICD_DEBUG":                   "D_FULLDEBUG D_COMMAND",
        # Give metricd a stable shared-port socket name so that
        # condor_shared_port can forward HTTP(S) connections to it.
        "METRICD_ARGS":                    "-sock metricd",
        "SHARED_PORT_HTTP_FORWARDING_ID":  "metricd",
        # No PROMETHEUS_HTTP_AUTH_FILE → unauthenticated access allowed.
    }
    with Condor(test_dir / "condor_https", config=cfg) as condor:
        yield condor


@action
def https_host_port(test_dir, condor_with_https):
    return _metricd_http_port(condor_with_https)


@action
def https_metrics_ready(test_dir, condor_with_https):
    """Wait until metricd has written the prom file at least once."""
    prom_file = test_dir / "https_metrics.prom"
    deadline = time.time() + 60
    while time.time() < deadline:
        if prom_file.exists() and "https_test_gauge" in prom_file.read_text():
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


class TestPrometheusHTTPS:
    """Verify that metricd serves /metrics over HTTPS when AUTH_SSL_SERVER_CERTFILE
    and AUTH_SSL_SERVER_KEYFILE are configured.

    HTTP and HTTPS share the same command port: the handler peeks the first few
    bytes of each connection and routes a TLS ClientHello through OpenSSL while
    serving everything else as plain HTTP. These tests therefore also confirm
    plain HTTP still works on the same port when TLS is enabled (dual mode)."""

    def test_port_found(self, https_host_port):
        host, port = https_host_port
        assert host and port > 0

    def test_metrics_file_written(self, https_metrics_ready):
        assert https_metrics_ready

    def test_https_get_returns_200(self, https_host_port, https_metrics_ready):
        host, port = https_host_port
        status, _ = _https_get(host, port, "/metrics")
        assert status == 200

    def test_https_body_contains_metric(self, https_host_port, https_metrics_ready):
        host, port = https_host_port
        _, body = _https_get(host, port, "/metrics")
        assert "https_test_gauge" in body

    def test_https_body_has_help_and_type(self, https_host_port, https_metrics_ready):
        host, port = https_host_port
        _, body = _https_get(host, port, "/metrics")
        assert "# HELP https_test_gauge" in body
        assert "# TYPE https_test_gauge gauge" in body

    def test_https_unknown_path_returns_404(self, https_host_port, https_metrics_ready):
        host, port = https_host_port
        status, _ = _https_get(host, port, "/notfound")
        assert status == 404

    def test_https_multiple_requests_served(self, https_host_port, https_metrics_ready):
        """Each HTTPS request gets its own TLS handshake; the handler must
        serve more than one in a row."""
        host, port = https_host_port
        for _ in range(3):
            status, body = _https_get(host, port, "/metrics")
            assert status == 200
            assert "https_test_gauge" in body

    def test_plain_http_still_served_on_same_port(self, https_host_port, https_metrics_ready):
        """With TLS enabled, a plain (non-TLS) HTTP request to the same port is
        detected by the byte-peek and still served as ordinary HTTP."""
        host, port = https_host_port
        status, body = _http_get(host, port, "/metrics")
        assert status == 200
        assert "https_test_gauge" in body


# ---------------------------------------------------------------------------
# PROMETHEUS_HTTP_PORT: dedicated-port serving, and disabling HTTP entirely
# ---------------------------------------------------------------------------
#
# The tests above serve HTTP over metricd's normal command socket. Here we
# cover the two PROMETHEUS_HTTP_PORT special cases:
#
#   * PROMETHEUS_HTTP_PORT = <nonzero port different from SHARED_PORT_PORT>
#     -> metricd opens its OWN listening socket on that exact port and serves
#        /metrics there directly. metricd only takes this path when a shared
#        port is configured (SHARED_PORT_PORT > 0), so the standup sets that up.
#   * PROMETHEUS_HTTP_PORT = <negative> (e.g. -1)
#     -> HTTP is disabled: no handler is registered and no socket is opened,
#        but the metrics FILE is still written normally.
#
# Picking the "specific port" without flaking is the hard part, especially when
# many copies of this test run at once on one host. Safeguards:
#   1. We never hard-code a port. We ask the OS for a currently-free ephemeral
#      port (bind to port 0, read it back, close) so each test instance gets a
#      distinct port.
#   2. There is an unavoidable (tiny) race between us releasing the probed port
#      and metricd binding it, so we do not trust the probe blindly: we read
#      MetricdLog and CONFIRM metricd actually bound the port. If it lost the
#      race (metricd logs a bind failure), we stand the pool back up on a fresh
#      port. This makes the test deterministic rather than occasionally-failing.
#   3. Standing up a pool can wedge, and a retry loop could in principle run for
#      a very long time. Every standup here is wrapped in a HARD wall-clock
#      deadline (_hard_deadline) so these tests can never run longer than ~1
#      minute no matter what goes wrong.


def _pick_free_tcp_port():
    """Return a TCP port that is free right now.

    Binding to port 0 lets the kernel hand us an unused port from the ephemeral
    range; we read it back and immediately close the socket so metricd can claim
    it. Callers must still verify metricd bound it (see the retry loop below),
    because the port could in principle be taken by another process in between.
    We bind to all interfaces (host "") so the port is free on every interface
    metricd might choose, not just loopback.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.bind(("", 0))
        return s.getsockname()[1]
    finally:
        s.close()


class _HardTimeout(BaseException):
    """Raised by _hard_deadline when its wall-clock budget is exceeded.

    Deliberately a BaseException, not Exception: helper code and the ornithology
    Condor bring-up use broad ``except Exception`` blocks, and the deadline must
    not be silently swallowed by them. (Condor._start's ``except BaseException``
    does catch it, but only to clean up and re-raise -- which is what we want.)
    """


@contextmanager
def _hard_deadline(seconds):
    """Guarantee the wrapped block cannot run longer than `seconds` wall-clock.

    Uses a one-shot SIGALRM timer, which fires even while blocked in a syscall,
    subprocess, or poll loop, so a wedged standup can never make this test run
    forever. SIGALRM is delivered on the main thread, which is where pytest
    drives fixtures. On platforms without SIGALRM (e.g. Windows) this degrades
    to a best-effort no-op.
    """
    if not hasattr(signal, "SIGALRM"):
        yield
        return

    def _fire(signum, frame):
        raise _HardTimeout("exceeded hard %d-second deadline" % seconds)

    previous = signal.signal(signal.SIGALRM, _fire)
    signal.setitimer(signal.ITIMER_REAL, seconds)
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, previous)


def _metricd_bound_http_port(condor, port, timeout=15):
    """Poll MetricdLog until metricd reports success or failure binding its
    dedicated HTTP listen socket on `port`.

    Returns True once the success line appears, False on an explicit bind
    failure (lost the port race) or if neither appears within `timeout`.
    """
    log_file = condor.log_dir / "MetricdLog"
    ok_line   = "listening for HTTP requests on port %d" % port
    fail_line = "failed to listen on HTTP port"
    deadline = time.time() + timeout
    while time.time() < deadline:
        if log_file.exists():
            text = log_file.read_text(errors="replace")
            if ok_line in text:
                return True
            if fail_line in text:
                return False
        time.sleep(0.5)
    return False


def _bring_up_condor(condor, ready_timeout):
    """Mirror Condor._start() but with a bounded readiness wait.

    Condor.__enter__ waits up to 600s for the pool to be ready. That is far too
    long here: if a probed port was taken between our probe and a daemon binding
    it, the pool never comes up and we want to fail fast and retry on a fresh
    port, all within our overall time budget. Returns True if the pool became
    ready, or False (after cleaning up) if it did not.
    """
    try:
        condor._setup_local_dirs()
        condor._write_config()
        condor._start_condor()
        condor._wait_for_ready(timeout=ready_timeout)
        return True
    except Exception:
        # Note: _HardTimeout is a BaseException, so it is NOT caught here -- the
        # outer deadline handler cleans up and re-raises.
        try:
            condor._cleanup()
        except Exception:
            pass
        return False


# --- Standup: metricd serving on a dedicated PROMETHEUS_HTTP_PORT ------------

@standup
def condor_dedicated_http_port(test_dir):
    metrics_dir = test_dir / "port_metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(
        metrics_dir / "00_port_test_metrics",
        """
[
  Name = "port_test_gauge";
  Value = 55;
  Desc = "Dedicated-port HTTP test gauge";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
""",
    )
    prom_file = test_dir / "port_metrics.prom"

    def make_config(shared_port, http_port):
        return {
            "DAEMON_LIST":                     "$(DAEMON_LIST) METRICD",
            "METRICD":                         "$(LIBEXEC)/condor_metricd",
            "GANGLIA_LIB":                     "NOOP",
            "GANGLIA_SEND_DATA_FOR_ALL_HOSTS": "true",
            "PROMETHEUS_METRICS_FILE":         str(prom_file),
            # metricd opens its own listening socket only when a shared port is
            # configured (SHARED_PORT_PORT > 0) AND the HTTP port differs from
            # it. The harness default SHARED_PORT_PORT=0 means "pick a dynamic
            # port", which metricd reads as 0 and so never takes this path, so we
            # must pin SHARED_PORT_PORT to a specific port. condor_shared_port
            # then actually binds it, so both this port and the HTTP port must be
            # free (the retry loop below secures and confirms them).
            "SHARED_PORT_PORT":                str(shared_port),
            "PROMETHEUS_HTTP_PORT":            str(http_port),
            "METRICD_INTERVAL":                "5",
            "METRICD_METRICS_CONFIG_DIR":      str(metrics_dir),
            "METRICD_DEBUG":                   "D_FULLDEBUG D_COMMAND",
        }

    # Stand up the pool and CONFIRM metricd bound the dedicated HTTP port. Two
    # distinct probed ports are in play -- the shared port and the HTTP port --
    # and either could be taken between our probe and the daemon binding it. A
    # lost shared port keeps the pool from coming ready (fast-failed by the
    # bounded readiness wait); a lost HTTP port merely disables HTTP (logged).
    # Either way we just retry on fresh ports. The whole loop runs under a hard
    # wall-clock deadline so it can never run away.
    condor = None
    chosen_port = None
    pending = None
    try:
        with _hard_deadline(55):
            for attempt in range(5):
                shared_port = _pick_free_tcp_port()
                http_port = _pick_free_tcp_port()
                while http_port == shared_port:
                    http_port = _pick_free_tcp_port()
                pending = Condor(
                    test_dir / ("condor_port_%d" % attempt),
                    config=make_config(shared_port, http_port),
                )
                if _bring_up_condor(pending, ready_timeout=25) and \
                        _metricd_bound_http_port(pending, http_port, timeout=10):
                    condor, chosen_port = pending, http_port
                    pending = None
                    break
                logger.warning(
                    "dedicated HTTP standup attempt %d (shared_port=%d, http_port=%d) "
                    "did not come up cleanly; retrying on fresh ports",
                    attempt, shared_port, http_port,
                )
                try:
                    pending._cleanup()
                except Exception:
                    pass
                pending = None
    except _HardTimeout:
        # Tear down whatever we managed to start before the deadline fired.
        for c in (pending, condor):
            if c is not None:
                try:
                    c._cleanup()
                except Exception:
                    pass
        raise

    if condor is None:
        raise RuntimeError(
            "metricd never bound a dedicated PROMETHEUS_HTTP_PORT within the time budget"
        )

    try:
        yield condor, chosen_port
    finally:
        condor._cleanup()


@action
def dedicated_host_port(condor_dedicated_http_port):
    # Connect on the same interface metricd binds its command socket to; the
    # dedicated HTTP socket uses that same interface, on the port we chose.
    condor, port = condor_dedicated_http_port
    host, _cmd_port = _metricd_http_port(condor)
    return host, port


@action
def dedicated_metrics_ready(test_dir, condor_dedicated_http_port):
    prom_file = test_dir / "port_metrics.prom"
    deadline = time.time() + 25
    while time.time() < deadline:
        if prom_file.exists() and "port_test_gauge" in prom_file.read_text():
            return True
        time.sleep(1)
    return False


# --- Standup: metricd with HTTP disabled (PROMETHEUS_HTTP_PORT < 0) ----------

@standup
def condor_http_disabled(test_dir):
    metrics_dir = test_dir / "noport_metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(
        metrics_dir / "00_noport_test_metrics",
        """
[
  Name = "noport_test_gauge";
  Value = 66;
  Desc = "HTTP-disabled test gauge";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
]
""",
    )
    prom_file = test_dir / "noport_metrics.prom"
    cfg = {
        "DAEMON_LIST":                     "$(DAEMON_LIST) METRICD",
        "METRICD":                         "$(LIBEXEC)/condor_metricd",
        "GANGLIA_LIB":                     "NOOP",
        "GANGLIA_SEND_DATA_FOR_ALL_HOSTS": "true",
        "PROMETHEUS_METRICS_FILE":         str(prom_file),
        # A negative port disables HTTP serving entirely.
        "PROMETHEUS_HTTP_PORT":            "-1",
        "METRICD_INTERVAL":                "5",
        "METRICD_METRICS_CONFIG_DIR":      str(metrics_dir),
        "METRICD_DEBUG":                   "D_FULLDEBUG D_COMMAND",
    }
    # Bounded by a hard deadline so a wedged standup can't run forever.
    condor = None
    try:
        with _hard_deadline(55):
            condor = Condor(test_dir / "condor_noport", config=cfg)
            condor.__enter__()
    except _HardTimeout:
        if condor is not None:
            try:
                condor.__exit__(None, None, None)
            except Exception:
                pass
        raise

    try:
        yield condor
    finally:
        condor.__exit__(None, None, None)


@action
def disabled_metrics_ready(test_dir, condor_http_disabled):
    prom_file = test_dir / "noport_metrics.prom"
    deadline = time.time() + 25
    while time.time() < deadline:
        if prom_file.exists() and "noport_test_gauge" in prom_file.read_text():
            return True
        time.sleep(1)
    return False


class TestPrometheusHTTPDedicatedPort:
    """When PROMETHEUS_HTTP_PORT is a nonzero port that differs from
    SHARED_PORT_PORT, metricd opens its own listening socket on that exact port
    and serves /metrics there directly (independent of shared port)."""

    def test_metrics_file_written(self, dedicated_metrics_ready):
        assert dedicated_metrics_ready

    def test_metricd_logged_dedicated_listen(self, condor_dedicated_http_port):
        condor, port = condor_dedicated_http_port
        log_text = (condor.log_dir / "MetricdLog").read_text(errors="replace")
        assert ("listening for HTTP requests on port %d" % port) in log_text

    def test_get_metrics_returns_200(self, dedicated_host_port, dedicated_metrics_ready):
        host, port = dedicated_host_port
        status, _ = _http_get(host, port, "/metrics")
        assert status == 200

    def test_get_metrics_body_contains_metric(self, dedicated_host_port, dedicated_metrics_ready):
        host, port = dedicated_host_port
        _, body = _http_get(host, port, "/metrics")
        assert "port_test_gauge" in body

    def test_get_metrics_body_has_help_and_type(self, dedicated_host_port, dedicated_metrics_ready):
        host, port = dedicated_host_port
        _, body = _http_get(host, port, "/metrics")
        assert "# HELP port_test_gauge" in body
        assert "# TYPE port_test_gauge gauge" in body

    def test_unknown_path_returns_404(self, dedicated_host_port, dedicated_metrics_ready):
        host, port = dedicated_host_port
        status, _ = _http_get(host, port, "/notfound")
        assert status == 404

    def test_multiple_requests_served(self, dedicated_host_port, dedicated_metrics_ready):
        host, port = dedicated_host_port
        for _ in range(3):
            status, body = _http_get(host, port, "/metrics")
            assert status == 200
            assert "port_test_gauge" in body


class TestPrometheusHTTPDisabled:
    """When PROMETHEUS_HTTP_PORT is negative, metricd must not serve HTTP at all:
    no handler is registered and no listening socket is opened. File-based
    publication of the metrics must keep working."""

    def test_metrics_file_still_written(self, disabled_metrics_ready):
        # Disabling HTTP must not affect the Prometheus metrics file.
        assert disabled_metrics_ready

    def test_no_http_handler_registered(self, condor_http_disabled, disabled_metrics_ready):
        log_text = (condor_http_disabled.log_dir / "MetricdLog").read_text(errors="replace")
        assert "registered HTTP handler for /metrics endpoint" not in log_text

    def test_no_dedicated_listen_socket(self, condor_http_disabled, disabled_metrics_ready):
        log_text = (condor_http_disabled.log_dir / "MetricdLog").read_text(errors="replace")
        assert "listening for HTTP requests on port" not in log_text

    def test_command_port_does_not_serve_metrics(self, condor_http_disabled, disabled_metrics_ready):
        # A GET to metricd's command port must NOT yield a Prometheus response,
        # since HTTP handling is off. The command socket drops or otherwise
        # fails to answer the request as HTTP; any of those outcomes is fine, as
        # long as we never get a 200 carrying our metric.
        host, port = _metricd_http_port(condor_http_disabled)
        try:
            status, body = _http_get(host, port, "/metrics", timeout=5)
        except Exception:
            # Connection refused/reset or not valid HTTP -> HTTP disabled. Good.
            return
        assert status != 200 or "noport_test_gauge" not in body

