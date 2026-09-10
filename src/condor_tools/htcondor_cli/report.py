import csv
import statistics
import sys

from collections import Counter
from datetime import datetime, timedelta
from difflib import SequenceMatcher
from pathlib import Path
from time import time as now
from types import SimpleNamespace
from typing import Dict, List, Optional, Tuple

import htcondor2
from htcondor2._utils.ansi import AnsiOptions, Color, bold, colorize, id_colorize, stylize

# 256-color id for the light-orange used to highlight suggested follow-up
# commands (e.g. "htcondor cluster analytics 1020") -- distinct enough to
# stand out from the report's red/yellow/green/cyan status colors without
# reading as another alert color.
_TOOL_HINT_COLOR_ID = 215


def _tool_hint(text: str) -> str:
    return id_colorize(text, _TOOL_HINT_COLOR_ID)

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from .utils import *

# All parameters needed by the analytics suite
REQUIRED_ATTRS = [
    # Job identifiers
    "ClusterId",
    "ProcId",
    "JobStatus",

    # Resource requests
    "RequestMemory",
    "RequestDisk",
    "RequestCpus",
    "RequestGpus",

    # Resource usage (RAW values in KiB)
    "ResidentSetSize_RAW",
    "DiskUsage_RAW",

    # CPU usage (in seconds)
    "RemoteUserCpu",
    "RemoteSysCpu",
    "RemoteWallClockTime",

    # Provisioned resources
    "CpusProvisioned",

    # Hold information
    "HoldReason",
    "HoldReasonCode",
    "HoldReasonSubCode",

    # Timing information
    "QDate",
    "CompletionDate",
    "JobStartDate",
    "EnteredCurrentStatus",
]

# HTCondor JobStatus code -> human-readable name, in canonical display order.
JOB_STATUS_NAMES = {
    1: "Idle",
    2: "Running",
    3: "Removing",
    4: "Completed",
    5: "Held",
    6: "Transferring Output",
    7: "Suspended",
}


def _fetch_cluster_jobs(cluster_id: int, filepath: Path) -> int:
    """
    Fetch all jobs from HTCondor history for a given cluster and save to CSV.
    """
    schedd = htcondor2.Schedd()

    filepath.parent.mkdir(parents=True, exist_ok=True)

    print(f"Fetching jobs for cluster {cluster_id}...")
    print(f"This may take a moment for large clusters...\n")

    data = dict()

    def _get_attr(ad, attr):
        """Best-effort read of one ClassAd attribute, falling back to eval()."""
        try:
            value = ad.get(attr)
        except Exception:
            value = None
        if value is None:
            try:
                value = ad.eval(attr)
            except Exception:
                value = None
        return value

    def _get_job_info(query_func) -> None:
        nonlocal data

        for ad in query_func(constraint=f"ClusterId=={cluster_id}", projection=REQUIRED_ATTRS):
            jid = str(ad["ClusterId"]) + "." + str(ad["ProcId"])
            data[jid] = {attr: _get_attr(ad, attr) for attr in REQUIRED_ATTRS}

    print("Querying current queue...", file=sys.stderr)
    queue_job_count = 0
    try:
        _get_job_info(schedd.query)
        queue_job_count = len(data)
        print(f"  Queue complete: {queue_job_count} jobs", file=sys.stderr)
    except Exception as e:
        print(f"Warning: Error querying queue: {e}", file=sys.stderr)

    print("Querying job history...", file=sys.stderr)
    try:
        _get_job_info(schedd.history)
        history_job_count = len(data) - queue_job_count
        print(f"  History complete: {history_job_count} jobs", file=sys.stderr)
    except Exception as e:
        print(f"Warning: Error querying history: {e}", file=sys.stderr)

    job_count = len(data)
    if job_count == 0:
        print(f"\nError: No jobs found for cluster {cluster_id}")
        print("Please verify the cluster ID is correct.")
        sys.exit(1)

    print(f"\nWriting {job_count} jobs to CSV...", file=sys.stderr)
    try:
        with open(filepath, "w", newline="", encoding="utf-8") as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=REQUIRED_ATTRS)
            writer.writeheader()
            writer.writerows(data.values())
    except Exception as e:
        print(f"Error writing CSV: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"✓ Successfully saved data to: {filepath}")
    print(f"✓ Total jobs fetched: {job_count}")

    status_counts = {}
    for job in data.values():
        status = job.get("JobStatus")
        if status:
            try:
                status = int(status)
            except Exception:
                status = -2
        else:
            status = -1
        name = JOB_STATUS_NAMES.get(status, f"Unknown({status})")
        status_counts[name] = status_counts.get(name, 0) + 1

    print("\nJob Status Breakdown:")
    for status, count in sorted(status_counts.items()):
        print(f"  {status:<15}: {count:>6} jobs")

    return job_count


def _validate_cluster_exists(cluster_id: int) -> bool:
    """
    Quick check to see if cluster exists before full fetch.
    """
    schedd = htcondor2.Schedd()

    try:
        if len(schedd.query(f"ClusterId=={cluster_id}", ["ClusterId"], match=1)) == 1:
            return True
    except Exception:
        pass

    try:
        if len(schedd.history(f"ClusterId=={cluster_id}", ["ClusterId"], match=1)) == 1:
            return True
    except Exception:
        pass

    return False


def _normalize_hold_reason(raw: str) -> str:
    """
    Normalize a HoldReason string for fuzzy-bucketing: keep only the first
    sentence, and strip a leading "Error from <slot>: " prefix if present.
    """
    reason = (raw or "").split(". ")[0]
    if "Error from" in reason and ": " in reason:
        parts = reason.split(": ", 1)
        if len(parts) == 2:
            reason = parts[1]
    return reason


# Default cache location: the current working directory.
_DEFAULT_CACHE_DIR = Path(".")


def _cached_cluster_file(cluster_id: int, cache_dir: Path) -> Path:
    """
    Get the expected file path for locally cached cluster data
    """
    return cache_dir / "cluster_data" / f"cluster_{cluster_id}_jobs.csv"


def _load_csv_for_cluster(cluster_id: int, exit_on_missing: bool = True):
    """
    Load the cached CSV for a cluster (see _cached_cluster_file()) and
    return a list of job dicts.

    Parameters:
        cluster_id (str or int): The cluster ID to load.
        exit_on_missing (bool): If True, print an error and sys.exit(1) when
            the file is not found. If False, return None instead.

    Returns:
        list[dict] or None
    """
    filepath = _cached_cluster_file(cluster_id, _DEFAULT_CACHE_DIR)

    if not filepath.exists():
        if exit_on_missing:
            print(
                f"Cluster data not found for cluster {cluster_id}. "
                "Please make sure you have the correct CSV file and cluster ID."
            )
            sys.exit(1)
        return None

    with open(filepath, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def _ensure_cluster_data(cluster_id: int, cache_dir: Path = _DEFAULT_CACHE_DIR, fresh: bool = False) -> None:
    """
    Ensure cluster_data/cluster_<id>_jobs.csv exists; fetch via HTCondor if not.
    """
    filepath = _cached_cluster_file(cluster_id, cache_dir)

    if not fresh and filepath.exists():
        return
    elif filepath.exists():
        print(f"Fetching fresh data for cluster {cluster_id}...")
    else:
        print(f"No cached data found for cluster {cluster_id}. Fetching data...")

    if not _validate_cluster_exists(cluster_id):
        raise RuntimeError(
            f"No jobs found for cluster {cluster_id}. Verify the cluster ID, "
            f"your permissions, and that it exists in HTCondor history or queue."
        )

    _fetch_cluster_jobs(cluster_id, filepath)

# ── Verbs ─────────────────────────────────────────────────────────────────────

class Dashboard(Verb):
    """
    Displays an ASCII job-status bar chart for a cluster
    """

    options = {
        "cluster_id": {
            "args": ("cluster_id",),
            "type": int,
            "help": "HTCondor cluster ID to display",
        },
    }

    # get data from the schedd
    def fetch_counts(cluster_id):
        schedd = htcondor2.Schedd()
        counts = {name: 0 for name in JOB_STATUS_NAMES.values()}

        print("Fetching job history (this may take a moment)...", file=sys.stderr)

        total_found = 0

        for ad in schedd.history(
            constraint=f"ClusterId == {cluster_id}",
            projection=["JobStatus"],
            match=-1,
        ):
            total_found += 1
            counts[JOB_STATUS_NAMES[ad.eval("JobStatus")]] += 1

            if total_found % 1000 == 0:
                print(f"  Found {total_found} matching jobs...", file=sys.stderr)

        print(f"  Completed: {total_found} matching jobs found", file=sys.stderr)

        print("Fetching current queue...", file=sys.stderr)
        for ad in schedd.query(
            constraint=f"ClusterId == {cluster_id}",
            projection=["JobStatus"],
            limit=-1,
        ):
            counts[JOB_STATUS_NAMES[ad.eval("JobStatus")]] += 1
        print("Done fetching data\n", file=sys.stderr)

        return counts

    # print the dashboard
    def draw_bars(counts, bar_width=50):
        job_states = list(JOB_STATUS_NAMES.values())
        max_label_len = max(len(s) for s in job_states)
        count_width = max(len(str(max(counts.values()) or 1)), len("Count"))
        per_width = len("100.0%")
        total_count = sum(counts.values())

        if total_count == 0:
            print("No jobs in the cluster found, please recheck cluster_id")
            sys.exit(1)

        header = (
            f"{'Status'.rjust(max_label_len)} | "
            f"{'Bar'.ljust(bar_width)} | "
            f"{'Count'.rjust(count_width)} | "
            f"{'%'.rjust(per_width)}"
        )
        print(header)
        print("-" * len(header))

        for state in job_states:
            cnt = counts[state]
            bar = render_bar(cnt, total_count, bar_width, ceil=True)
            per = cnt * 100 / total_count

            state_str = state.rjust(max_label_len)
            bar_str = bar.ljust(bar_width)
            cnt_str = str(cnt).rjust(count_width)
            per_str = f"{per:5.1f}%".rjust(per_width)

            print(f"{state_str} | {bar_str} | {cnt_str} | {per_str}")

    def get_dashboard_data(cluster_id):
        """
        Return job status counts as a dictionary for use by cluster_health.py.
        Does not print anything, just returns computed metrics.
        """
        try:
            counts = Dashboard.fetch_counts(cluster_id)
            total = sum(counts.values())

            if total == 0:
                return None

            return {
                "total_jobs": total,
                "status_counts": counts,
                "completed": counts.get("Completed", 0),
                "held": counts.get("Held", 0),
                "running": counts.get("Running", 0),
                "idle": counts.get("Idle", 0),
                "held_pct": (counts.get("Held", 0) / total) * 100 if total > 0 else 0,
                "completed_pct": (counts.get("Completed", 0) / total) * 100 if total > 0 else 0,
            }
        except Exception:
            return None

    def __init__(self, logger, cluster_id, **options):
        counts = Dashboard.fetch_counts(cluster_id)
        print(f"\nCluster {cluster_id} Status Dashboard\n")
        Dashboard.draw_bars(counts)

class Histogram(Verb):
    """
    Plots runtime distribution (CDF and/or percentile histogram) for a cluster
    """

    options = {
        "cluster_id": {
            "args": ("cluster_id",),
            "type": int,
            "help": "HTCondor cluster ID to analyse",
        },
        "show": {
            "args": ("--show",),
            "choices": ["cdf", "histogram", "both"],
            "default": "both",
            "help": "Which graph to display (default: both)",
        },
        "print_list": {
            "args": ("--print-list",),
            "action": "store_true",
            "default": False,
            "help": "Print job IDs with runtime < 10 minutes",
        },
        "percentiles": {
            "args": ("--percentiles",),
            "type": int,
            "default": 10,
            "help": "Number of percentile bins for the histogram (default: 10)",
        },
    }

    # ── Data loading ───────────────────────────────────────────────────────────────

    def load_data_for_cluster(cluster_id):
        """Load cluster CSV and return a cleaned DataFrame."""
        jobs = _load_csv_for_cluster(cluster_id)
        return make_dataframe(jobs, numeric_cols=["RemoteWallClockTime", "QDate", "CompletionDate"])

    def get_positive_runtimes(df, quiet=False):
        """Return positive runtime Series, or None if missing/empty."""
        if "RemoteWallClockTime" not in df.columns:
            if not quiet:
                print("[WARN] Missing RemoteWallClockTime column.")
            return None

        rt = df["RemoteWallClockTime"].dropna()
        rt = rt[rt > 0]

        if rt.empty:
            if not quiet:
                print("[WARN] No valid runtime data found.")
            return None

        return rt

    def cumulative_distribution(rt, height=20, width=60):
        """
        Print an ASCII CDF line plot of job runtimes.

        X-axis: runtime over full observed range.
        Y-axis: cumulative % of jobs.
        Reads as: "X% of jobs completed within Y time."
        """
        if rt is None:
            return

        runtimes = sorted(rt.values)
        n = len(runtimes)

        marker_pcts = [25, 50, 75, 90, 95]
        marker_times = {p: float(np_percentile(runtimes, p)) for p in marker_pcts}

        x_max = float(runtimes[-1])
        plot = [[" " for _ in range(width)] for _ in range(height)]

        prev_y = height - 1
        for x in range(width):
            rt_at_x = (x / (width - 1)) * x_max if width > 1 else x_max
            cum_pct = float(np_searchsorted(runtimes, rt_at_x)) / n
            y = int((1.0 - cum_pct) * (height - 1))
            y = max(0, min(height - 1, y))

            y_lo, y_hi = min(y, prev_y), max(y, prev_y)
            for fy in range(y_lo, y_hi + 1):
                plot[fy][x] = "·"
            plot[y][x] = "•"
            prev_y = y

        # Percentile guideline rows.
        # Multiple percentiles may map to the same row, so store a list.
        marker_rows = {}
        for p, t in marker_times.items():
            row = int((1.0 - p / 100.0) * (height - 1))
            row = max(0, min(height - 1, row))
            marker_rows.setdefault(row, []).append((p, t))

            for x in range(width):
                if plot[row][x] == " ":
                    plot[row][x] = "╌"

        label_w = 5

        print(f"  {'Cumulative Distribution':^{width}}")
        print(f"{'':>{label_w}}  {'-' * width}")

        for i, row in enumerate(plot):
            pct_val = round(100 * (1.0 - i / (height - 1))) if height > 1 else 100
            label = f"{pct_val}%" if pct_val in (0, 25, 50, 75, 100) else ""

            annotation = ""
            if i in marker_rows:
                parts = [
                    f"{format_seconds_human(t)} (P{p})"
                    for p, t in sorted(marker_rows[i], key=lambda x: x[0])
                ]
                annotation = "  " + " | ".join(parts)

            coloured = []
            for cell in row:
                if cell in ("•", "·"):
                    coloured.append(colorize(cell, Color.BRIGHT_CYAN))
                elif cell == "╌":
                    coloured.append(colorize(cell, Color.BRIGHT_YELLOW))
                else:
                    coloured.append(cell)

            print(f"{label:>{label_w}} |{''.join(coloured)}{annotation}")

        print(f"{'':>{label_w}} +{'-' * width}")

        col = max(1, width // 4)
        ticks = (
            f"{'0s':<{col}}"
            f"{format_seconds_human(x_max * 0.25):<{col}}"
            f"{format_seconds_human(x_max * 0.50):<{col}}"
            f"{format_seconds_human(x_max * 0.75):<{max(1, col - 1)}}"
            f"{format_seconds_human(x_max):>{col}}"
        )
        print(f"{'':>{label_w + 1}} {ticks}")
        print(f"{'':>{label_w + 1}} {'Runtime':^{width}}")

        print(
            f"\n  Key:  {colorize('•', Color.BRIGHT_CYAN)} curve point   "
            f"{colorize('·', Color.BRIGHT_CYAN)} connector   "
            f"{colorize('╌', Color.BRIGHT_YELLOW)} percentile guideline"
        )
        print()


    # ── Histogram ──────────────────────────────────────────────────────────────────

    def histogram(df, rt, percentiles=10, max_width=20, show_fast_jobs=False):
        """
        Print histogram where bins are percentile ranges.
        Bars are red if the bin median runtime is < 10 minutes.
        Optionally print exact job IDs whose runtime is < 10 minutes.
        """
        if rt is None:
            return

        runtimes = rt.values
        percentiles_list = np_linspace(0, 100, percentiles + 1)
        raw_edges = np_percentile(runtimes, percentiles_list)
        bin_edges = np_unique(raw_edges)

        if len(bin_edges) < 2:
            print("[WARN] Not enough unique runtime values to build histogram.")
            return

        counts, _ = np_histogram(runtimes, bins=bin_edges)
        max_count = max(counts) if len(counts) > 0 else 0

        print(f"\n  {'Histogram by Percentile':^80}")
        print(f"  {'-' * 80}")
        print()

        pct_width = 15
        label_width = 30
        count_width = 7

        header = (
            f"{'Percentile':<{pct_width}}"
            f"{'Time Range':<{label_width}}"
            f"| {'Histogram':<{max_width}}"
            f" {'# Jobs':>{count_width}}"
        )
        print(header)
        print("-" * len(header))

        jobs_in_red_bins = 0

        for i in range(len(counts)):
            left = bin_edges[i]
            right = bin_edges[i + 1]

            mask = (
                (rt >= left) & (rt <= right)
                if i == len(counts) - 1
                else (rt >= left) & (rt < right)
            )

            in_bin = rt[mask]
            median_time = in_bin.median() if not in_bin.empty else 0
            is_red = median_time < 600

            if is_red:
                jobs_in_red_bins += len(in_bin)

            time_range = (
                f"{format_seconds_human(left):>10} - {format_seconds_human(right):>10}"
            ).rjust(label_width)

            # Because duplicate percentile edges may have been collapsed by np.unique,
            # compute an approximate percentile label from the actual edge positions.
            left_pct = int(round(100 * i / len(counts)))
            right_pct = int(round(100 * (i + 1) / len(counts)))
            pct_range = f"{left_pct:02}–{right_pct:02}%".ljust(pct_width)

            bar = f"{render_bar(counts[i], max_count, max_width):<{max_width}}"
            if is_red:
                bar = colorize(bar, Color.BRIGHT_RED)

            print(f"{pct_range}{time_range} | {bar} {counts[i]:>{count_width}}")

        print(f"\n{colorize('Note:', Color.BRIGHT_RED)} Bars in red represent bins with median runtime < 10 minutes.")
        print(f"{colorize('Info:', Color.BRIGHT_RED)} Total number of jobs in such bins: {jobs_in_red_bins}")

        if show_fast_jobs:
            if "ClusterId" in df.columns and "ProcId" in df.columns:
                fast_mask = df["RemoteWallClockTime"].fillna(-1) < 600
                fast_jobs = df.loc(fast_mask, ["ClusterId", "ProcId"]).dropna()

                fast_job_ids = [
                    f"{int(r)}.{int(p)}" if notna(r) and notna(p) else f"{r}.{p}"
                    for r, p in zip(fast_jobs["ClusterId"], fast_jobs["ProcId"])
                ]

                if fast_job_ids:
                    print(f"\nJob IDs with runtime < 10 minutes:")
                    print(", ".join(fast_job_ids))
                else:
                    print(f"\nNo jobs with runtime < 10 minutes.")
            else:
                print("\n[WARN] Cannot print fast job IDs: missing ClusterId or ProcId column.")


    # ── Data-only interface for summarize.py ──────────────────────────────────────

    def get_histogram_data(cluster_id):
        """
        Return runtime analysis metrics as a dict for use by summarize.py.
        Does not print anything.
        """
        jobs = _load_csv_for_cluster(cluster_id, exit_on_missing=False)
        if not jobs:
            return None

        df = make_dataframe(jobs, numeric_cols=["RemoteWallClockTime", "QDate", "CompletionDate"])

        rt = Histogram.get_positive_runtimes(df, quiet=True)
        if rt is None:
            return None

        runtimes = rt.values
        p95 = np_percentile(runtimes, 95)

        qdate_series = df["QDate"].dropna() if "QDate" in df.columns else Series([])
        completion_series = (
            df["CompletionDate"].dropna()
            if "CompletionDate" in df.columns else Series([])
        )

        # Correlation between submission time and runtime: positive means
        # later-submitted jobs ran longer, negative means they ran shorter.
        # Computed from the raw job list (not qdate_series/rt above) since
        # those are independently dropna()'d and may no longer line up
        # job-for-job; correlation needs QDate/RemoteWallClockTime pairs
        # from the *same* job.
        qdate_paired, runtime_paired = [], []
        for job in jobs:
            qd = safe(float, job.get("QDate"))
            wt = safe(float, job.get("RemoteWallClockTime"))
            if qd is not None and wt is not None and wt > 0:
                qdate_paired.append(qd)
                runtime_paired.append(wt)
        correlation = np_corrcoef(qdate_paired, runtime_paired)

        return {
            "total_runtime_jobs": len(runtimes),
            "mean_runtime": rt.mean(),
            "median_runtime": rt.median(),
            "std_runtime": rt.std(),
            "cv": rt.std() / rt.mean() if rt.mean() > 0 else 0,
            "correlation": correlation,
            "fast_jobs": sum(1 for v in runtimes if v < 600),
            "fast_jobs_pct": float(sum(1 for v in runtimes if v < 600) / len(runtimes) * 100),
            "long_jobs": sum(1 for v in runtimes if v > p95),
            "p95_runtime": p95,
            "min_runtime": rt.min(),
            "max_runtime": rt.max(),
            "first_submitted": qdate_series.min() if not qdate_series.empty else None,
            "last_completed": completion_series.max() if not completion_series.empty else None,
        }


    # ── Entry point ────────────────────────────────────────────────────────────────

    def __init__(self, logger, cluster_id, **options):
        _ensure_cluster_data(cluster_id)

        df = Histogram.load_data_for_cluster(cluster_id)

        rt = Histogram.get_positive_runtimes(df)
        n = len(rt) if rt is not None else 0

        submit_times = df["QDate"].dropna() if "QDate" in df.columns else Series([])
        completion_times = (
            df["CompletionDate"].dropna()
            if "CompletionDate" in df.columns else Series([])
        )

        first_sub = (
            format_epoch_human_relative(submit_times.min())
            if not submit_times.empty else "N/A"
        )
        last_comp = (
            format_epoch_human_relative(completion_times.max())
            if not completion_times.empty else "N/A"
        )

        print(f"\n{bold('Runtime Analysis'.center(80))}")
        print("=" * 80)
        print(
            f"  Cluster : {cluster_id}   |   Jobs: {n}   |   "
            f"Submitted: {first_sub}   |   Completed: {last_comp}"
        )
        print("=" * 80)

        show = options.get("show", "both")
        if show in ("cdf", "both"):
            Histogram.cumulative_distribution(rt, height=15, width=60)
        if show in ("histogram", "both"):
            Histogram.histogram(
                df, rt,
                percentiles=options.get("percentiles", 10),
                max_width=20,
                show_fast_jobs=options.get("print_list", False),
            )

class Analytics(Verb):
    """
    Produces a resource utilisation report (CPU / memory / disk)
    """

    options = {
        "cluster_id": {
            "args": ("cluster_id",),
            "type": int,
            "help": "HTCondor cluster ID to analyse",
        },
    }

    # to print the bar visualizations
    def bar(pct, width=50):
        filled = render_bar(pct, 100, width)
        return "[" + filled.ljust(width) + f"] {pct:.1f}%"

    # to calculate efficiency
    def efficiency(used, expected):
        if not expected:
            return 0.0
        return (used / expected) * 100

    # to print the usage report
    def compute_usage_summary(data, label, percentage=False, unit=None):
        if not data or len(data) < 2:
            return f"{label:<25}: Not enough data"

        data_sorted = sorted(data)
        min_val = data_sorted[0]
        q1 = statistics.quantiles(data_sorted, n=4)[0]
        median = statistics.median(data_sorted)
        q3 = statistics.quantiles(data_sorted, n=4)[2]
        max_val = data_sorted[-1]
        std_dev = statistics.stdev(data_sorted)

        fmt = "{:.1f}%" if percentage else "{:.1f}"
        return (
            f"{label:<25}: "
            f"{fmt.format(min_val):>6}  {fmt.format(q1):>6}  {fmt.format(median):>7}  "
            f"{fmt.format(q3):>6}  {fmt.format(max_val):>6}   {fmt.format(std_dev):>6}"
        )

    # prints the resource request table
    def print_resource_table(name, values, unit=""):
        if not values:
            print(f"{name:<15}: No data")
            return

        counts = Counter(values)
        print(f"{name:<15}:")
        for val, count in sorted(counts.items()):
            print(f"{'':<15}  {val:<10} {unit:<5}  {count} job(s)")
        print()

    # prints distribution of jobs by actual resource usage as a histogram
    def print_usage_distribution(name, used_list, unit="GiB"):
        if not used_list:
            return

        max_val = max(used_list)

        # Define bins based on the data range
        if max_val <= 10:
            bins = [0, 2, 5, 10, float("inf")]
            labels = ["0-2", "2-5", "5-10", "10+"]
        elif max_val <= 50:
            bins = [0, 5, 10, 20, 50, float("inf")]
            labels = ["0-5", "5-10", "10-20", "20-50", "50+"]
        else:
            bins = [0, 10, 25, 50, 100, float("inf")]
            labels = ["0-10", "10-25", "25-50", "50-100", "100+"]

        # Count jobs in each bin
        bin_counts = [0] * len(labels)
        for val in used_list:
            for i in range(len(bins) - 1):
                if bins[i] <= val < bins[i+1]:
                    bin_counts[i] += 1
                    break

        total_jobs = len(used_list)

        print(f"\n{name} Distribution:")

        # Find max count for scaling
        max_count = max(bin_counts) if bin_counts else 1
        bar_width = 50

        for label, count in zip(labels, bin_counts):
            pct = (count / total_jobs) * 100 if count > 0 else 0.0
            bar_visual = render_bar(count, max_count, bar_width)
            print(f"  {label:>10} {unit}: {bar_visual:<{bar_width}} {count:>4} ({pct:>5.1f}%)")

    # compute recommended requests + potential savings from p95 usage plus a
    # buffer; shared by summarize() (prints it) and get_analytics_data()
    # (returns it as-is)
    def compute_savings(mem_requested, mem_used, disk_requested, disk_used, cpu_requests, cpu_eff_list, avg_runtime_hours):
        savings = {}

        if mem_requested and mem_used:
            p95_mem = np_percentile(mem_used, 95)
            recommended_mem = p95_mem * 1.1  # 10% buffer
            median_mem_req = statistics.median(mem_requested)

            if recommended_mem < median_mem_req * 0.8:  # can save > 20%
                savings["memory"] = {
                    "current": median_mem_req,
                    "recommended": recommended_mem,
                    "savings_gib_hours": (median_mem_req - recommended_mem) * len(mem_used) * avg_runtime_hours,
                    "reduction_pct": ((median_mem_req - recommended_mem) / median_mem_req) * 100,
                    "jobs_affected": len(mem_used),
                }

        if disk_requested and disk_used:
            p95_disk = np_percentile(disk_used, 95)
            recommended_disk = p95_disk * 1.2  # 20% buffer
            median_disk_req = statistics.median(disk_requested)

            if recommended_disk < median_disk_req * 0.8:
                savings["disk"] = {
                    "current": median_disk_req,
                    "recommended": recommended_disk,
                    "savings_gib_hours": (median_disk_req - recommended_disk) * len(disk_used) * avg_runtime_hours,
                    "reduction_pct": ((median_disk_req - recommended_disk) / median_disk_req) * 100,
                    "jobs_affected": len(disk_used),
                }

        if cpu_requests and cpu_eff_list:
            median_cpu_pct = statistics.median(cpu_eff_list)
            median_cpu_req = statistics.median(cpu_requests)

            if median_cpu_pct < 50:
                savings["cpu"] = {
                    "current": median_cpu_req,
                    "recommended": max(1, int(median_cpu_req * (median_cpu_pct / 100) * 1.2)),  # 20% buffer
                    "current_efficiency": median_cpu_pct,
                    "jobs_affected": len(cpu_eff_list),
                }

        return savings

    # print recommendations
    def print_recommendations(savings, avg_runtime_hours):
        print(f"\n{'Resource Optimization Recommendations':^80}")
        print("=" * 80)

        if "memory" in savings:
            s = savings["memory"]
            waste_per_job = s["current"] - s["recommended"]
            print(f"\n📊 Memory:")
            print(f"  Current Request     : {s['current']:.1f} GiB")
            print(f"  Recommended         : {s['recommended']:.1f} GiB (P95 + 10% buffer)")
            print(f"  Potential Savings   : {s['savings_gib_hours']:.1f} GiB-hours")
            print(f"                        (≈ {waste_per_job:.1f} GiB/job × {s['jobs_affected']} jobs × {avg_runtime_hours:.1f} hr avg runtime)")
            print(f"  Jobs Affected       : {s['jobs_affected']}")

        if "disk" in savings:
            s = savings["disk"]
            waste_per_job = s["current"] - s["recommended"]
            print(f"\n💾 Disk:")
            print(f"  Current Request     : {s['current']:.1f} GiB")
            print(f"  Recommended         : {s['recommended']:.1f} GiB (P95 + 20% buffer)")
            print(f"  Potential Savings   : {s['savings_gib_hours']:.1f} GiB-hours")
            print(f"                        (≈ {waste_per_job:.1f} GiB/job × {s['jobs_affected']} jobs × {avg_runtime_hours:.1f} hr avg runtime)")
            print(f"  Jobs Affected       : {s['jobs_affected']}")

        if "cpu" in savings:
            s = savings["cpu"]
            print(f"\n⚙️  CPU:")
            print(f"  Current Request     : {s['current']:.1f} CPUs")
            print(f"  Current Efficiency  : {s['current_efficiency']:.1f}%")
            print(f"  Recommended         : {s['recommended']} CPUs")
            print(f"  Jobs Affected       : {s['jobs_affected']}")

    # loads a cluster's cached job CSV and computes all resource-usage metrics;
    # shared by summarize() (prints them) and get_analytics_data() (returns them)
    def collect_metrics(cluster_id, exit_on_missing=True):
        jobs = _load_csv_for_cluster(cluster_id, exit_on_missing=exit_on_missing)
        if jobs is None:
            return None

        mem_requested, mem_used = [], []
        disk_requested, disk_used = [], []
        run_time, cpu_used_time = [], []
        runtimes = []
        cpu_requests = []
        gpu_requests = []

        for job in jobs:
            mem_req = safe(float, job.get("RequestMemory"))
            mem_use = safe(float, job.get("ResidentSetSize_RAW"))
            if mem_req:
                mem_requested.append(round(mem_req / 1024, 2))  # Convert MiB to GiB
            if mem_use:
                mem_used.append(mem_use / 1024 / 1024)  # Convert KiB to GiB

            disk_req = safe(float, job.get("RequestDisk"))
            disk_use = safe(float, job.get("DiskUsage_RAW"))
            if disk_req:
                disk_requested.append(round(disk_req / (1024 * 1024), 2))  # Convert KiB to GiB
            if disk_use:
                disk_used.append(disk_use / (1024 * 1024))  # Convert KiB to GiB

            cpus = safe(float, job.get("RequestCpus"))
            if cpus:
                cpu_requests.append(int(cpus))

            gpus = safe(float, job.get("RequestGpus"))
            if gpus:
                gpu_requests.append(int(gpus))

            user_cpu = safe(float, job.get("RemoteUserCpu")) or 0
            sys_cpu = safe(float, job.get("RemoteSysCpu")) or 0
            wall_time = safe(float, job.get("RemoteWallClockTime"))

            if wall_time and cpus and (user_cpu or sys_cpu):
                cpu_used_time.append(sys_cpu / cpus)
                run_time.append(wall_time)

            if wall_time:
                runtimes.append(wall_time)

        # Compute per-job efficiency lists
        per_job_cpu_eff = [Analytics.efficiency(u, r) for u, r in zip(cpu_used_time, run_time) if r]
        per_job_mem_eff = [Analytics.efficiency(u, r) for u, r in zip(mem_used, mem_requested) if r]
        per_job_disk_eff = [Analytics.efficiency(u, r) for u, r in zip(disk_used, disk_requested) if r]

        # Take medians
        avg_cpu_eff = statistics.median(per_job_cpu_eff) if per_job_cpu_eff else 0
        avg_mem_eff = statistics.median(per_job_mem_eff) if per_job_mem_eff else 0
        avg_disk_eff = statistics.median(per_job_disk_eff) if per_job_disk_eff else 0

        avg_runtime = statistics.mean(runtimes) if runtimes else 0
        avg_runtime_hours = avg_runtime / 3600 if avg_runtime else 1.0

        savings = Analytics.compute_savings(
            mem_requested, mem_used, disk_requested, disk_used,
            cpu_requests, per_job_cpu_eff, avg_runtime_hours,
        )

        return {
            "total_jobs": len(jobs),
            "avg_runtime": avg_runtime,
            "avg_runtime_str": str(timedelta(seconds=int(avg_runtime))) if avg_runtime else "N/A",
            "avg_runtime_hours": avg_runtime_hours,
            "memory_efficiency": avg_mem_eff,
            "disk_efficiency": avg_disk_eff,
            "cpu_efficiency": avg_cpu_eff,
            "memory_jobs": len(per_job_mem_eff),
            "disk_jobs": len(per_job_disk_eff),
            "cpu_jobs": len(per_job_cpu_eff),
            "mem_requested": mem_requested,
            "mem_used": mem_used,
            "disk_requested": disk_requested,
            "disk_used": disk_used,
            "cpu_requests": cpu_requests,
            "gpu_requests": gpu_requests,
            "cpu_efficiency_list": per_job_cpu_eff,
            "savings": savings,
        }

    # prints the total report
    def summarize(cluster_id):
        m = Analytics.collect_metrics(cluster_id)

        print("=" * 80)
        print(f"{'HTCondor Cluster Resource Summary':^80}")
        print("=" * 80)
        print(f"{'Cluster ID':>20}: {cluster_id}")
        print(f"{'Job Count':>20}: {m['total_jobs']}")
        print(f"{'Avg Runtime':>20}: {m['avg_runtime_str']}")
        print()

        print(f"{'Requested Resources':^80}")
        print("=" * 80)
        Analytics.print_resource_table("Memory (GiB)", m["mem_requested"], "GiB")
        Analytics.print_resource_table("Disk (GiB)", m["disk_requested"], "GiB")
        Analytics.print_resource_table("CPUs", m["cpu_requests"], "")
        Analytics.print_resource_table("GPUs", m["gpu_requests"], "")

        print(f"{'Number Summary Table':^80}")
        print("=" * 80)
        print(f"{'Resource (units)':<25}: {'Min':>6}  {'Q1':>6}  {'Median':>7}  {'Q3':>6}  {'Max':>6}   {'StdDev':>6}")
        print("-" * 80)

        print(Analytics.compute_usage_summary(m["mem_used"], "Memory Used (GiB)"))
        print(Analytics.compute_usage_summary(m["disk_used"], "Disk Used (GiB)"))
        print(Analytics.compute_usage_summary(m["cpu_efficiency_list"], "CPU Usage (%)", percentage=True))

        print()

        print(f"{'Overall Utilization':^80}")
        print("=" * 80)
        print(f"  Memory usage      {Analytics.bar(m['memory_efficiency'])}")
        print(f"  Disk usage        {Analytics.bar(m['disk_efficiency'])}")
        print(f"  CPU usage         {Analytics.bar(m['cpu_efficiency'])}")
        print()

        # Usage distribution
        print(f"{'Resource Usage Distribution':^80}")
        print("=" * 80)
        Analytics.print_usage_distribution("Memory", m["mem_used"], "GiB")
        Analytics.print_usage_distribution("Disk", m["disk_used"], "GiB")

        # Recommendations
        Analytics.print_recommendations(m["savings"], m["avg_runtime_hours"])

        # Gives human readable notes on the efficiency and also warnings
        print()
        print(f"{'Efficiency Summary':^80}")
        print("=" * 80)

        def warn(resource, efficiency):
            if efficiency < 15:
                print(f"  ⚠️  {resource} usage is {efficiency:.1f}% - significant over-provisioning")
            elif efficiency < 50:
                print(f"  ⚠️  {resource} usage is {efficiency:.1f}% - consider reducing requests")
            elif efficiency > 80:
                print(f"  ✅ {resource} usage is {efficiency:.1f}% - well optimized")
            else:
                print(f"  ✅ {resource} usage is {efficiency:.1f}%")

        warn("Memory", m["memory_efficiency"])
        warn("Disk", m["disk_efficiency"])
        warn("CPU", m["cpu_efficiency"])

        print()
        print(f"{'End of Summary':^80}")
        print("=" * 80)

    def get_analytics_data(cluster_id):
        """
        Return analytics data as a dictionary for use by cluster_health.py
        Does not print anything, just returns computed metrics.

        Returns:
            dict: Dictionary containing all analytics metrics
        """
        return Analytics.collect_metrics(cluster_id, exit_on_missing=False)

    def __init__(self, logger, cluster_id, **options):
        _ensure_cluster_data(cluster_id)
        Analytics.summarize(cluster_id)

class Hold(Verb):
    """
    Classifies and buckets held jobs by hold reason
    """

    options = {
        "cluster_id": {
            "args": ("cluster_id",),
            "type": int,
            "help": "HTCondor cluster ID to analyse",
        },
        "min_count": {
            "args": ("--min-count",),
            "type": int,
            "default": 1,
            "help": "Only show buckets with at least N jobs (default: 1)",
        },
        "top": {
            "args": ("--top",),
            "type": int,
            "default": None,
            "help": "Show only the top N most common buckets",
        },
        "code": {
            "args": ("--code",),
            "type": int,
            "default": None,
            "help": "Filter to jobs with a specific HoldReasonCode",
        },
        "sort_by": {
            "args": ("--sort-by",),
            "choices": ["count", "code", "percent", "time"],
            "default": "count",
            "help": "Sort results by count, code, percent, or time (default: count)",
        },
        "threshold": {
            "args": ("--threshold",),
            "type": float,
            "default": 0.7,
            "help": "Similarity threshold (0.0-1.0) for grouping error messages (default: 0.7)",
        },
        "show_job_ids": {
            "args": ("--show-job-ids",),
            "action": "store_true",
            "default": False,
            "help": "Display ProcIds in the output table",
        },
        "export_jobs": {
            "args": ("--export-jobs",),
            "default": None,
            "help": "Export held job IDs to a CSV file for bulk operations",
        },
    }

    # Mapping of HoldReasonCodes to their explanations
    HOLD_REASON_CODES = {
        1:  {"label": "UserRequest",                   "reason": "The user put the job on hold with condor_hold."},
        3:  {"label": "JobPolicy",                     "reason": "The PERIODIC_HOLD expression evaluated to True. Or, ON_EXIT_HOLD was true."},
        4:  {"label": "CorruptedCredential",           "reason": "The credentials for the job are invalid."},
        5:  {"label": "JobPolicyUndefined",            "reason": "A job policy expression evaluated to Undefined."},
        6:  {"label": "FailedToCreateProcess",         "reason": "The condor_starter failed to start the executable."},
        7:  {"label": "UnableToOpenOutput",            "reason": "The standard output file for the job could not be opened."},
        8:  {"label": "UnableToOpenInput",             "reason": "The standard input file for the job could not be opened."},
        9:  {"label": "UnableToOpenOutputStream",      "reason": "The standard output stream for the job could not be opened."},
        10: {"label": "UnableToOpenInputStream",       "reason": "The standard input stream for the job could not be opened."},
        11: {"label": "InvalidTransferAck",            "reason": "An internal HTCondor protocol error was encountered when transferring files."},
        12: {"label": "TransferOutputError",           "reason": "An error occurred while transferring job output files or self-checkpoint files."},
        13: {"label": "TransferInputError",            "reason": "An error occurred while transferring job input files."},
        14: {"label": "IwdError",                      "reason": "The initial working directory of the job cannot be accessed."},
        15: {"label": "SubmittedOnHold",               "reason": "The user requested the job be submitted on hold."},
        16: {"label": "SpoolingInput",                 "reason": "Input files are being spooled."},
        17: {"label": "JobShadowMismatch",             "reason": "A standard universe job is not compatible with the condor_shadow version available on the submitting machine."},
        18: {"label": "InvalidTransferGoAhead",        "reason": "An internal HTCondor protocol error was encountered when transferring files."},
        19: {"label": "HookPrepareJobFailure",         "reason": "<Keyword>_HOOK_PREPARE_JOB was defined but could not be executed or returned failure."},
        20: {"label": "MissedDeferredExecutionTime",   "reason": "The job missed its deferred execution time and therefore failed to run."},
        21: {"label": "StartdHeldJob",                 "reason": "The job was put on hold because WANT_HOLD in the machine policy was true."},
        22: {"label": "UnableToInitUserLog",           "reason": "Unable to initialize job event log."},
        23: {"label": "FailedToAccessUserAccount",     "reason": "Failed to access user account."},
        24: {"label": "NoCompatibleShadow",            "reason": "No compatible shadow."},
        25: {"label": "InvalidCronSettings",           "reason": "Invalid cron settings."},
        26: {"label": "SystemPolicy",                  "reason": "SYSTEM_PERIODIC_HOLD evaluated to true."},
        27: {"label": "SystemPolicyUndefined",         "reason": "The system periodic job policy evaluated to undefined."},
        32: {"label": "MaxTransferInputSizeExceeded",  "reason": "The maximum total input file transfer size was exceeded."},
        33: {"label": "MaxTransferOutputSizeExceeded", "reason": "The maximum total output file transfer size was exceeded."},
        34: {"label": "JobOutOfResources",             "reason": "Memory usage exceeds a memory limit."},
        35: {"label": "InvalidDockerImage",            "reason": "Specified Docker image was invalid."},
        36: {"label": "FailedToCheckpoint",            "reason": "Job failed when sent the checkpoint signal it requested."},
        43: {"label": "PreScriptFailed",               "reason": "Pre script failed."},
        44: {"label": "PostScriptFailed",              "reason": "Post script failed."},
        45: {"label": "SingularityTestFailed",         "reason": "Test of singularity runtime failed before launching a job"},
        46: {"label": "JobDurationExceeded",           "reason": "The job's allowed duration was exceeded."},
        47: {"label": "JobExecuteExceeded",            "reason": "The job's allowed execution time was exceeded."},
        48: {"label": "HookShadowPrepareJobFailure",   "reason": "Prepare job shadow hook failed when it was executed; status code indicated job should be held."}
    }

    class HoldReason:
        def __init__(self, msg: str, code: int, pid: int, time: int) -> None:
            self.reason = msg
            self.subcode = code
            self.proc = pid
            self.entered = time

    def bucket_reasons_with_data(reason_data: List[HoldReason], threshold: float = 0.7) -> List[List[HoldReason]]:
        """Groups similar hold reason messages using fuzzy string matching (difflib.SequenceMatcher)."""
        buckets = []

        for hold in reason_data:
            placed = False

            for bucket in buckets:
                ratio = SequenceMatcher(None, hold.reason, bucket[0].reason).ratio()

                if ratio >= threshold:
                    bucket.append(hold)
                    placed = True
                    break

            if not placed:
                buckets.append([hold])

        return buckets

    def calculate_avg_hold_time(bucket: List[HoldReason]) -> Tuple[Optional[float], str]:
        """Calculate average time jobs have been held in a bucket"""
        current_time = now()
        hold_durations = []

        for reason in bucket:
            if reason.entered > 0:
                duration = current_time - reason.entered
                hold_durations.append(duration)

        if not hold_durations:
            return None, "N/A"

        avg_seconds = sum(hold_durations) / len(hold_durations)
        return avg_seconds, format_seconds_human(avg_seconds)

    def group_by_code(cluster_id: int) -> Dict[int, List[HoldReason]]:
        """
        Queries the HTCondor schedd for held jobs in the specified cluster and groups them by their HoldReasonCode.
        Now also collects ProcId and EnteredCurrentStatus (hold time).
        """
        schedd = htcondor2.Schedd()
        reasons_by_code = {}

        print("Fetching held jobs from cluster...", file=sys.stderr)

        for ad in schedd.query(
            constraint=f"ClusterId=={cluster_id} && JobStatus==5",
            projection=["ProcId", "HoldReasonCode", "HoldReason", "HoldReasonSubCode", "EnteredCurrentStatus"],
            limit=-1
        ):

            code = ad.get("HoldReasonCode")
            subcode = ad.get("HoldReasonSubCode")
            proc_id = ad.get("ProcId")
            hold_time = ad.get("EnteredCurrentStatus", 0)

            # Displaying only the first line of HoldReason, to bucket more efficiently
            reason = _normalize_hold_reason(ad.get("HoldReason"))

            reasons_by_code.setdefault(code, []).append(Hold.HoldReason(reason, subcode, proc_id, hold_time))

        print(f"Found {sum(len(v) for v in reasons_by_code.values())} held jobs\n", file=sys.stderr)

        return reasons_by_code

    def print_time_analysis(reasons_by_code: Dict[int, List[HoldReason]]):
        """Analyzes and prints time-based statistics for held jobs."""
        all_times = []
        for reasons in reasons_by_code.values():
            all_times.extend([reason.entered for reason in reasons if reason.entered > 0])

        if not all_times:
            print("⏱️  Time Analysis: No timestamp data available\n")
            return

        earliest = min(all_times)
        latest = max(all_times)
        current_time = now()

        print("⏱️  Time Analysis:")
        print(f"  First held: {datetime.fromtimestamp(earliest).strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"  Last held:  {datetime.fromtimestamp(latest).strftime('%Y-%m-%d %H:%M:%S')}")
        duration_hours = (latest - earliest) / 3600
        print(f"  Duration:   {duration_hours:.1f} hours")

        # Calculate overall average hold time
        avg_hold_duration = (current_time - sum(all_times) / len(all_times))
        print(f"  Avg hold:   {format_seconds_human(avg_hold_duration)}")
        print()

    def export_job_ids(all_buckets: List[List[HoldReason]], reasons_by_code: Dict[int, List[HoldReason]], cluster_id: int, filename: Path) -> None:
        """Export job IDs with hold reason codes to a CSV file for bulk operations."""
        # Build a mapping of proc_id to hold reason code
        proc_to_code = {}
        for code, reasons in reasons_by_code.items():
            for reason in reasons:
                proc_to_code[reason.proc] = code

        # Collect job IDs with their codes
        job_data = []
        seen_jobs = set()
        for bucket in all_buckets:
            for reason in bucket:
                proc_id = reason.proc
                job_id = f"{cluster_id}.{proc_id}"
                if job_id not in seen_jobs:
                    seen_jobs.add(job_id)
                    hold_code = proc_to_code.get(proc_id, "Unknown")
                    hold_label = Hold.HOLD_REASON_CODES.get(hold_code, {}).get("label", f"Code {hold_code}")
                    job_data.append((job_id, hold_code, hold_label))

        # Sort by job ID for consistency
        job_data.sort(key=lambda x: (int(x[0].split(".")[0]), int(x[0].split(".")[1])))

        # Write to CSV
        with open(filename, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["JobID", "HoldReasonCode", "HoldReasonLabel"])
            writer.writerows(job_data)

        print(f"✓ Exported {len(job_data)} unique job IDs to {filename}\n")

    def bucket_and_print_table(reasons_by_code: Dict[int, List[HoldReason]], args) -> None:
        """Processes grouped hold reasons and prints a detailed table with filtering and sorting options."""
        print(f"Cluster ID: {args.cluster_id}")

        held_jobs = sum(len(reasons) for reasons in reasons_by_code.values())
        print(f"Held Jobs in Cluster: {held_jobs}\n")

        # Time analysis
        Hold.print_time_analysis(reasons_by_code)

        example_rows = []
        all_buckets = []
        seen_codes = set()

        # Filter by specific code if requested
        if args.code:
            if args.code not in reasons_by_code:
                print(f"No held jobs found with HoldReasonCode {args.code}")
                return
            reasons_by_code = {args.code: reasons_by_code[args.code]}

        for code, reasons in reasons_by_code.items():
            label = Hold.HOLD_REASON_CODES.get(code, {}).get("label", f"Code {code}")
            seen_codes.add(code)

            for bucket in Hold.bucket_reasons_with_data(reasons, threshold=args.threshold):
                # Apply min-count filter
                if len(bucket) < args.min_count:
                    continue

                all_buckets.append(bucket)
                first = bucket[0]
                example_reason, subcode, proc_id, hold_time = first.reason, first.subcode, first.proc, first.entered
                percent = (len(bucket) / held_jobs) * 100 if held_jobs > 0 else 0

                # Calculate average hold time for this bucket
                avg_hold_seconds, avg_hold_str = Hold.calculate_avg_hold_time(bucket)

                # Prepare job IDs string if requested
                job_ids_str = ""
                if args.show_job_ids:
                    if len(bucket) <= 5:
                        ids = [str(h.proc) for h in bucket]
                        job_ids_str = ", ".join(ids)
                    else:
                        ids = [str(h.proc) for h in bucket[:3]]
                        job_ids_str = f"{', '.join(ids)}... (+{len(bucket)-3} more)"

                row = [
                    label,
                    subcode,
                    f"{percent:.1f}% ({len(bucket)})",
                    avg_hold_str,
                    example_reason
                ]
                if args.show_job_ids:
                    row.append(job_ids_str)

                # Store avg_hold_seconds for sorting
                row.append(avg_hold_seconds if avg_hold_seconds else 0)

                example_rows.append(row)

        # Sort results
        if args.sort_by == "count":
            example_rows.sort(key=lambda x: int(x[2].split("(")[1].split(")")[0]), reverse=True)
        elif args.sort_by == "code":
            example_rows.sort(key=lambda x: x[0])
        elif args.sort_by == "percent":
            example_rows.sort(key=lambda x: float(x[2].split("%")[0]), reverse=True)
        elif args.sort_by == "time":
            example_rows.sort(key=lambda x: x[-1], reverse=True)  # Sort by avg_hold_seconds

        # Remove the avg_hold_seconds column (used only for sorting)
        example_rows = [row[:-1] for row in example_rows]

        # Apply top N filter
        if args.top:
            example_rows = example_rows[:args.top]

        headers = ["Hold Reason Label", "SubCode", "% of Held Jobs (Count)", "Avg Hold Time", "Example Reason"]
        if args.show_job_ids:
            headers.append("Job IDs (ProcId)")

        print(tabulate(example_rows, headers=headers, tablefmt="grid"))

        print("\nLegend:")
        legend = []
        for code in sorted(seen_codes):
            entry = Hold.HOLD_REASON_CODES.get(code, {})
            legend.append([code, entry.get("label", "Unknown"), entry.get("reason", "No description available.")])
        print(tabulate(legend, headers=["Code", "Label", "Reason"], tablefmt="fancy_grid"))


        # Export job IDs if requested
        if args.export_jobs:
            Hold.export_job_ids(all_buckets, reasons_by_code, args.cluster_id, args.export_jobs)

    def get_hold_bucket_data(cluster_id, threshold=0.7):
        """
        Return held jobs analysis data as a dictionary for use by cluster_health.py
        Does not print anything, just returns computed metrics.

        Returns:
            dict: Dictionary containing held jobs analysis
        """
        try:
            reasons_by_code = Hold.group_by_code(cluster_id)

            if not reasons_by_code:
                return {
                    "held_count": 0,
                    "held_codes": {},
                    "held_reasons": {},
                    "unique_reasons": 0,
                    "buckets": [],
                }

            held_count = sum(len(pairs) for pairs in reasons_by_code.values())
            held_codes = {}
            all_buckets = []

            for code, pairs in reasons_by_code.items():
                held_codes[code] = len(pairs)
                buckets = Hold.bucket_reasons_with_data(pairs, threshold=threshold)
                all_buckets.extend(buckets)

            # Get top reasons
            held_reasons = {}
            for bucket in all_buckets:
                reason = bucket[0].reason  # Get example reason from first job
                held_reasons[reason] = len(bucket)

            # Time analysis
            all_times = []
            for pairs in reasons_by_code.values():
                all_times.extend([r.entered for r in pairs if r.entered > 0])

            time_stats = {}
            if all_times:
                current_time = now()
                earliest = min(all_times)
                latest = max(all_times)
                avg_hold_duration = (current_time - sum(all_times) / len(all_times))

                time_stats = {
                    "first_held": earliest,
                    "last_held": latest,
                    "duration_hours": (latest - earliest) / 3600,
                    "avg_hold_duration": avg_hold_duration,
                }

            return {
                "held_count": held_count,
                "held_codes": held_codes,
                "held_reasons": held_reasons,
                "unique_reasons": len(held_reasons),
                "buckets": all_buckets,
                "time_stats": time_stats,
            }
        except Exception as e:
            return {
                "held_count": 0,
                "error": str(e)
            }

    def __init__(self, logger, cluster_id, **options):
        args = SimpleNamespace(
            cluster_id=cluster_id,
            min_count=options.get("min_count", 1),
            top=options.get("top"),
            code=options.get("code"),
            sort_by=options.get("sort_by", "count"),
            threshold=options.get("threshold", 0.7),
            show_job_ids=options.get("show_job_ids", False),
            export_jobs=options.get("export_jobs"),
        )

        reasons_by_code = Hold.group_by_code(cluster_id)

        if not reasons_by_code:
            logger.info(f"No held jobs found in cluster {cluster_id}")
            return

        Hold.bucket_and_print_table(reasons_by_code, args)

class Summarize(Verb):
    """
    This program provides a concise cluster health summary by aggregating
    data from all analysis tools and providing tool recommendations.
    """

    options = {
        "cluster_id": {
            "args": ("cluster_id",),
            "type": int,
            "help": "HTCondor cluster ID to summarize",
        },
    }

    THRESHOLDS = {
        "memory_efficiency": {"critical": 15, "warning": 50, "good": 80},
        "disk_efficiency": {"critical": 15, "warning": 50, "good": 80},
        "cpu_efficiency": {"critical": 15, "warning": 50, "good": 80},
        "held_jobs_pct": {"critical": 20, "warning": 10},
        "fast_jobs_pct": {"critical": 30, "warning": 15},
        "runtime_variance": {"critical": 3.0, "warning": 2.0},
    }

    def get_health_status(value, threshold_dict, higher_is_better=True):
        if higher_is_better:
            if value >= threshold_dict["good"]:
                return "HEALTHY"
            elif value >= threshold_dict["warning"]:
                return "WARNING"
            else:
                return "CRITICAL"
        else:
            if value >= threshold_dict["critical"]:
                return "CRITICAL"
            elif value >= threshold_dict["warning"]:
                return "WARNING"
            else:
                return "HEALTHY"

    def get_status_symbol(status):
        if status == "CRITICAL":
            return colorize("🔴 CRITICAL", Color.BRIGHT_RED)
        elif status == "WARNING":
            return colorize("🟡 WARNING", Color.BRIGHT_YELLOW)
        else:
            return colorize("🟢 HEALTHY", Color.BRIGHT_GREEN)

    def generate_health_report(cluster_id, efficiency_data, status_data, runtime_data, held_data):
        findings = []

        for aspect, eff_key, jobs_key in [
            ("Memory Efficiency", "memory_efficiency", "memory_jobs"),
            ("Disk Efficiency", "disk_efficiency", "disk_jobs"),
            ("CPU Efficiency", "cpu_efficiency", "cpu_jobs"),
        ]:
            eff = efficiency_data.get(eff_key, 0)
            status = Summarize.get_health_status(eff, Summarize.THRESHOLDS[eff_key])
            jobs = efficiency_data.get(jobs_key, 0)
            if status == "CRITICAL":
                reason = f"Severe over-provisioning ({eff:.1f}% efficiency)"
            elif status == "WARNING":
                reason = f"Low efficiency ({eff:.1f}%)"
            else:
                reason = f"Well optimized ({eff:.1f}% efficiency)"
            findings.append({
                "aspect": aspect, "status": status,
                "value": f"{eff:.1f}%", "jobs": jobs, "reason": reason,
                "tool": _tool_hint(f"htcondor cluster analytics {cluster_id}"),
            })

        held_count = held_data.get("held_count", 0)
        held_pct = status_data.get("held_pct", 0)
        if held_count > 0:
            held_status = Summarize.get_health_status(held_pct, Summarize.THRESHOLDS["held_jobs_pct"], higher_is_better=False)
            unique_reasons = held_data.get("unique_reasons", 0)
            reason = (
                f"{held_count} jobs held ({held_pct:.1f}%), {unique_reasons} unique reasons"
                if held_status in ("CRITICAL", "WARNING")
                else f"{held_count} jobs held ({held_pct:.1f}%)"
            )
            findings.append({
                "aspect": "Held Jobs", "status": held_status,
                "value": str(held_count), "jobs": held_count, "reason": reason,
                "tool": _tool_hint(f"htcondor cluster hold {cluster_id}"),
            })
        else:
            findings.append({
                "aspect": "Held Jobs", "status": "HEALTHY",
                "value": "0", "jobs": 0, "reason": "No held jobs", "tool": "N/A",
            })

        fast_jobs = runtime_data.get("fast_jobs", 0)
        fast_pct = runtime_data.get("fast_jobs_pct", 0)
        total_runtime_jobs = runtime_data.get("total_runtime_jobs", 0)
        if fast_jobs > 0:
            fast_status = Summarize.get_health_status(fast_pct, Summarize.THRESHOLDS["fast_jobs_pct"], higher_is_better=False)
            if fast_status == "CRITICAL":
                reason = f"{fast_jobs} jobs < 10min ({fast_pct:.1f}%), consider job bundling"
            elif fast_status == "WARNING":
                reason = f"{fast_jobs} jobs < 10min ({fast_pct:.1f}%), review job granularity"
            else:
                reason = f"{fast_jobs} jobs < 10min ({fast_pct:.1f}%)"
            findings.append({
                "aspect": "Fast Jobs", "status": fast_status,
                "value": f"{fast_pct:.1f}%", "jobs": fast_jobs, "reason": reason,
                "tool": _tool_hint(f"htcondor cluster histogram {cluster_id}"),
            })
        else:
            findings.append({
                "aspect": "Fast Jobs", "status": "HEALTHY",
                "value": "0%", "jobs": 0, "reason": "No fast jobs detected", "tool": "N/A",
            })

        cv = runtime_data.get("cv", 0)
        correlation = runtime_data.get("correlation", 0)
        long_jobs = runtime_data.get("long_jobs", 0)
        p95_runtime = runtime_data.get("p95_runtime", 0)
        if cv > 0:
            cv_status = Summarize.get_health_status(cv, Summarize.THRESHOLDS["runtime_variance"], higher_is_better=False)
            if cv_status == "CRITICAL":
                reason = f"High variance (CV={cv:.2f}), inconsistent runtimes"
            elif cv_status == "WARNING":
                reason = f"Moderate variance (CV={cv:.2f})"
            else:
                reason = f"Consistent runtimes (CV={cv:.2f})"
            if correlation > 0.4:
                reason += ", later jobs slower"
            elif correlation < -0.4:
                reason += ", later jobs faster"
            if long_jobs > 0:
                reason += f", {long_jobs} job{s(long_jobs)} exceeded P95 ({format_seconds_human(p95_runtime)})"
            findings.append({
                "aspect": "Runtime Consistency", "status": cv_status,
                "value": f"CV={cv:.2f}", "jobs": total_runtime_jobs, "reason": reason,
                "tool": _tool_hint(f"htcondor cluster histogram {cluster_id}"),
            })
        else:
            findings.append({
                "aspect": "Runtime Consistency", "status": "HEALTHY",
                "value": "N/A", "jobs": 0, "reason": "No runtime data available", "tool": "N/A",
            })

        total_jobs = status_data.get("total_jobs", 0)
        completed = status_data.get("completed", 0)
        running = status_data.get("running", 0)
        idle = status_data.get("idle", 0)
        if total_jobs > 0:
            findings.append({
                "aspect": "Job Status", "status": "INFO",
                "value": str(total_jobs), "jobs": total_jobs,
                "reason": f"{completed} completed, {running} running, {idle} idle",
                "tool": _tool_hint(f"htcondor cluster dashboard {cluster_id}"),
            })

        return findings

    def print_health_summary(cluster_id, findings):
        print("\n" + "=" * 120)
        title = stylize("HTCondor Cluster Health Report", AnsiOptions(bold=True, color=Color.BRIGHT_CYAN))
        print(f"{title:^130}")
        print("=" * 120)
        print(f"Cluster ID: {cluster_id}")
        print(f"Report Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 120)

        critical_count = sum(1 for f in findings if f["status"] == "CRITICAL")
        warning_count = sum(1 for f in findings if f["status"] == "WARNING")

        if critical_count > 0:
            overall = colorize("🔴 CRITICAL", Color.BRIGHT_RED)
        elif warning_count > 0:
            overall = colorize("🟡 WARNING", Color.BRIGHT_YELLOW)
        else:
            overall = colorize("🟢 HEALTHY", Color.BRIGHT_GREEN)

        print(f"\nOverall Status: {overall}")
        print(f"Critical Issues: {critical_count} | Warnings: {warning_count}")
        print()

        table_data = []
        for finding in findings:
            if finding["status"] == "INFO":
                status_display = colorize("ℹ️  INFO", Color.BRIGHT_CYAN)
            else:
                status_display = Summarize.get_status_symbol(finding["status"])
            table_data.append([
                finding["aspect"], status_display, finding["value"],
                finding["reason"], finding["tool"],
            ])

        headers = ["Aspect", "Status", "Value", "Reason / Details", "Tool for Details"]
        print(tabulate(table_data, headers=headers, tablefmt="grid", maxcolwidths=[20, 15, 12, 50, 35]))

        print("\n" + "=" * 120)
        print(bold("Recommended Next Steps"))
        print("=" * 120)

        if critical_count > 0:
            print(f"\n{colorize('🔴 HIGH PRIORITY:', Color.BRIGHT_RED)}")
            for finding in findings:
                if finding["status"] == "CRITICAL" and finding["tool"] != "N/A":
                    print(f"  • {finding['aspect']}: {finding['tool']}")

        if warning_count > 0:
            print(f"\n{colorize('🟡 REVIEW:', Color.BRIGHT_YELLOW)}")
            for finding in findings:
                if finding["status"] == "WARNING" and finding["tool"] != "N/A":
                    print(f"  • {finding['aspect']}: {finding['tool']}")

        if critical_count == 0 and warning_count == 0:
            print(f"\n{colorize('✓ Cluster is healthy - no immediate action required', Color.BRIGHT_GREEN)}")
            print("  • Continue regular monitoring")
            print("  • Run weekly health checks")

        print("\n" + "=" * 120)
        print()

    def __init__(self, logger, cluster_id, **options):
        _ensure_cluster_data(cluster_id)

        efficiency_data = Analytics.get_analytics_data(cluster_id)
        if not efficiency_data:
            print(f"ERROR: Could not load analytics data.")
            print(f"Please run: {_tool_hint(f'htcondor cluster fetch {cluster_id}')}")
            sys.exit(1)

        status_data = Dashboard.get_dashboard_data(cluster_id)
        if not status_data:
            status_data = {
                "total_jobs": efficiency_data.get("total_jobs", 0),
                "status_counts": {}, "held": 0, "held_pct": 0,
                "completed": 0, "running": 0, "idle": 0,
            }

        runtime_data = Histogram.get_histogram_data(cluster_id)
        if not runtime_data:
            runtime_data = {
                "cv": 0, "fast_jobs": 0, "fast_jobs_pct": 0,
                "total_runtime_jobs": 0, "correlation": 0,
            }

        held_data = Hold.get_hold_bucket_data(cluster_id)
        if not held_data:
            held_data = {"held_count": 0, "unique_reasons": 0}

        findings = Summarize.generate_health_report(
            cluster_id, efficiency_data, status_data, runtime_data, held_data
        )
        Summarize.print_health_summary(cluster_id, findings)

class Fetch(Verb):
    """
    Fetches raw job data for a cluster from HTCondor and caches it as CSV
    """

    options = {
        "cluster_id": {
            "args": ("cluster_id",),
            "type": int,
            "help": "HTCondor cluster ID to fetch",
        },
        "cache_dir": {
            "args": ("--cache-dir",),
            "default": _DEFAULT_CACHE_DIR,
            "type": Path,
            "help": "Directory to save the CSV file",
        },
    }

    def __init__(self, logger, cluster_id, **options):
        filepath = _cached_cluster_file(cluster_id, options.get("cache_dir"))
        job_count = _fetch_cluster_jobs(cluster_id, filepath)

        logger.info(f"Fetched {job_count} jobs for cluster {cluster_id} -> {filepath}")

# ── Noun ──────────────────────────────────────────────────────────────────────

class Cluster(Noun):
    """
    Run profiling and health analyses on an HTCondor job cluster
    """

    class dashboard(Dashboard):
        pass

    class histogram(Histogram):
        pass

    class analytics(Analytics):
        pass

    class hold(Hold):
        pass

    class summarize(Summarize):
        pass

    class fetch(Fetch):
        pass

    @classmethod
    def verbs(cls):
        return [
            cls.summarize,
            cls.dashboard,
            cls.histogram,
            cls.analytics,
            cls.hold,
            cls.fetch,
        ]