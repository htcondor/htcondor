#!/usr/bin/env pytest

#
# Regression test for HTCONDOR-3374: condor_metricd should publish a
# Prometheus text-format file containing only the metrics configured for
# the Prometheus backend.
#

import logging
import time

from ornithology import (
    config, standup, action,
    Condor,
    write_file,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

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
        "METRICD_INTERVAL":                     "10",
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
