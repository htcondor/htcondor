#!/usr/bin/env pytest

#
# Regression test for HTCONDOR-3374: condor_metricd must NOT touch the
# Ganglia backend when no parsed metric publishes to Ganglia. A
# Prometheus-only deployment may not have libganglia (or gmetric)
# installed at all, so calling GangliaD::initAndReconfig() in that case
# would EXCEPT and prevent the daemon from running.
#
# Strategy:
#   1. Stand up a Condor pool WITHOUT METRICD in DAEMON_LIST, so the
#      pool comes up regardless of metricd's fate.
#   2. Launch condor_metricd directly as a subprocess, with
#      GANGLIA_LIB and GANGLIA_GMETRIC pointing at paths that do not
#      exist.  If the gate works, metricd never touches GangliaD and
#      runs fine. If the gate is broken, GangliaD::initAndReconfig()
#      EXCEPTs almost immediately and metricd exits.
#   3. Poll for any of: the Prometheus file (success), a Ganglia-init
#      log line (regression — fail fast), or the metricd subprocess
#      dying (regression — fail fast).
#
# Both success and failure complete in well under 30 seconds.
#

import logging
import os
import signal
import subprocess
import time

from ornithology import (
    standup, action,
    Condor,
    write_file,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Only Prometheus metrics — no metric should ever land in the Ganglia
# backend. METRICD_DEFAULT_EXPORT_METRIC pins the default so a metric
# without an explicit ExportMetric does not become a wildcard that
# would (correctly) activate Ganglia.
METRIC_DEFS = """
[
  Name = "prometheus_only_jobs";
  Value = 23;
  Desc = "Prometheus-only metric";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
  PrometheusLabels = strcat("machine=\\"",Machine,"\\"");
]
[
  Name = "prometheus_only_bytes";
  Value = 99;
  Desc = "Another Prometheus-only metric";
  Units = "bytes";
  TargetType = "Scheduler";
  ExportMetric = "prometheus";
  Counter = true;
]
"""

# Strings that GangliaD::initAndReconfig() emits when it runs. Their
# appearance in MetricdLog means the gate failed.
GANGLIA_INIT_SIGNALS = [
    "Searching for libganglia",
    "Loading libganglia",
    "Testing /nonexistent/gmetric",
    "Will use libganglia",
    "Will use gmetric",
    "GANGLIA_LIB=NOOP",
    "Neither gmetric nor libganglia were successfully initialized",
]


@standup
def condor_without_metricd(test_dir):
    # Deliberately do NOT add METRICD to DAEMON_LIST. We want the pool
    # to come up cleanly regardless of what metricd does, so we can
    # observe metricd directly without the master's restart loop
    # blocking the test.
    metrics_dir = test_dir / "metrics.d"
    metrics_dir.mkdir(parents=True, exist_ok=True)
    write_file(metrics_dir / "00_test_metrics", METRIC_DEFS)

    prom_file = test_dir / "metrics.prom"

    config = {
        # Paths that do not exist. If metricd ever tries to initialize
        # GangliaD, the load will fail and metricd will EXCEPT.
        "GANGLIA_LIB":                          "/nonexistent/libganglia.so.0",
        "GANGLIA_GMETRIC":                      "/nonexistent/gmetric",
        "PROMETHEUS_METRICS_FILE":              str(prom_file),
        "METRICD_DEFAULT_EXPORT_METRIC":        "prometheus",
        "METRICD_INTERVAL":                     "5",
        "METRICD_METRICS_CONFIG_DIR":           str(metrics_dir),
        "METRICD_DEBUG":                        "D_FULLDEBUG",
    }

    with Condor(test_dir / "condor", config=config) as condor:
        yield condor


@action
def metricd_outcome(test_dir, condor_without_metricd):
    """
    Launch condor_metricd directly and poll for the outcome. Returns
    a dict with keys:
      - prom_contents: text of the prom file if metric was published
      - ganglia_signal: first Ganglia-init log line that appeared
      - process_died: subprocess.Popen.returncode if metricd exited
    At most one of these is non-None on a clean pass/fail. All None
    means we hit the deadline (separate failure).
    """
    prom_file = test_dir / "metrics.prom"
    metricd_log = condor_without_metricd.log_dir / "MetricdLog"

    libexec = condor_without_metricd.run_command(
        ["condor_config_val", "LIBEXEC"], echo=False,
    ).stdout.strip()
    metricd_bin = os.path.join(libexec, "condor_metricd")

    env = os.environ.copy()
    env["CONDOR_CONFIG"] = str(condor_without_metricd.config_file)
    # -f keeps metricd in the foreground so we can observe the subprocess.
    proc = subprocess.Popen(
        [metricd_bin, "-f"],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    result = {"prom_contents": None, "ganglia_signal": None, "process_died": None}
    try:
        # Success window: METRICD_INTERVAL=5, so first publish at ~5s.
        # Failure window: GangliaD::initAndReconfig() EXCEPTs within
        # ~1s, so the process dies (or the log line lands) almost
        # immediately. 15s is a comfortable upper bound on both.
        deadline = time.time() + 15
        while time.time() < deadline:
            # Subprocess died → metricd EXCEPTed. Fast fail.
            rc = proc.poll()
            if rc is not None:
                result["process_died"] = rc
                break
            # Found the Ganglia-init signature in the log. Fast fail.
            if metricd_log.exists():
                log_text = metricd_log.read_text(errors="replace")
                hits = [s for s in GANGLIA_INIT_SIGNALS if s in log_text]
                if hits:
                    result["ganglia_signal"] = hits[0]
                    break
            # The Prometheus file has our metric. Success.
            if prom_file.exists():
                text = prom_file.read_text()
                if "prometheus_only_jobs" in text:
                    result["prom_contents"] = text
                    break
            time.sleep(0.5)
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=5)

    return result


@action
def metricd_log_contents(condor_without_metricd):
    log_file = condor_without_metricd.log_dir / "MetricdLog"
    if not log_file.exists():
        return ""
    return log_file.read_text(errors="replace")


class TestMetricdSkipsGangliaWhenNoGangliaMetrics:
    def test_ganglia_init_never_attempted(self, metricd_outcome):
        # The gate failed if any Ganglia-init log line appeared.
        assert metricd_outcome["ganglia_signal"] is None, (
            "metricd touched the Ganglia backend: log contains "
            f"{metricd_outcome['ganglia_signal']!r}"
        )

    def test_metricd_did_not_die(self, metricd_outcome):
        # The gate also failed (a different signature of the same bug)
        # if the metricd subprocess exited on its own.
        assert metricd_outcome["process_died"] is None, (
            "metricd exited on its own with return code "
            f"{metricd_outcome['process_died']} — likely EXCEPTed "
            "during Ganglia init"
        )

    def test_prometheus_file_written(self, metricd_outcome):
        assert metricd_outcome["prom_contents"] is not None, (
            "Prometheus output file was not written within deadline"
        )

    def test_prometheus_metric_present(self, metricd_outcome):
        assert "prometheus_only_jobs" in (metricd_outcome["prom_contents"] or "")

    def test_metricd_log_clean(self, metricd_log_contents):
        # Belt-and-suspenders re-scan of the on-disk log after the
        # subprocess has been stopped.
        offenders = [s for s in GANGLIA_INIT_SIGNALS if s in metricd_log_contents]
        assert offenders == [], (
            f"metricd touched the Ganglia backend: {offenders}"
        )
