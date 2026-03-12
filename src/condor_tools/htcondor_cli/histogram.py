import os
import sys
import stat
import tempfile
import time
import shutil
import math
import enum

import htcondor2

from datetime import datetime, timedelta
from pathlib import Path
from htcondor2._utils.ansi import Color, colorize, bold, stylize, AnsiOptions

from typing import Union, List, Optional

class HistogramMode(enum.Enum):
    CUMULATIVE = enum.auto()
    INSTANT = enum.auto()

def produce_job_event_histogram(log_file: Union[Path, str], mode: HistogramMode) -> List[str]:
    log_file = Path(log_file)

    if not log_file.exists():
        raise FileNotFoundError(f"Could not find file: {str(log_file)}")

    if os.access(log_file, os.R_OK) is False:
        raise PermissionError(f"Could not access file: {str(log_file)}")

    # State -> (Label, Color)
    # NOTE: In stack order (done->held : bottom->top)
    # 256-color palette IDs for half-block rendering (fg/bg via AnsiOptions)
    KEY_LABEL = 0
    KEY_COLOR = 1
    STATES = {
        "done": ("Done", 2),            # green
        "running": ("Running", 6),      # cyan
        "xfer_in": ("Xfer In", 4),      # blue
        "xfer_out": ("Xfer Out", 5),    # magenta
        "idle": ("Idle", 3),            # yellow
        "held": ("Held", 1),            # red
    }

    def resolve_transfer_event(event) -> Optional[str]:
        """Determine state from transfer event"""
        XFER = {
            1: "xfer_in",      # IN_QUEUED
            2: "xfer_in",      # IN_STARTED
            3: "running",      # IN_FINISHED
            4: "xfer_out",     # OUT_QUEUED
            5: "xfer_out",     # OUT_STARTED
            6: "running",      # OUT_FINISHED
        }
        return XFER.get(event.get("Type"))

    # Map of job events to desired states
    EVENT_TO_STATE = {
        htcondor2.JobEventType.SUBMIT: "idle",
        htcondor2.JobEventType.EXECUTE: "running",
        htcondor2.JobEventType.JOB_HELD: "held",
        htcondor2.JobEventType.JOB_RELEASED: "idle",
        htcondor2.JobEventType.JOB_EVICTED: "idle",
        htcondor2.JobEventType.JOB_TERMINATED: "done",
        htcondor2.JobEventType.JOB_ABORTED: "done",
        htcondor2.JobEventType.JOB_STAGE_IN: "xfer_in",
        htcondor2.JobEventType.JOB_STAGE_OUT: "xfer_out",
        htcondor2.JobEventType.FILE_TRANSFER: resolve_transfer_event,
    }

    # List of lines to return (built from top -> bottom of terminal)
    LINES = list()

    # Parse events from the log file
    transitions = []
    with htcondor2.JobEventLog(str(log_file)) as event_log:
        for event in event_log.events(0):
            job_id = f"{event.cluster}.{event.proc}"
            timestamp = datetime.strptime(
                event.get("EventTime"), "%Y-%m-%dT%H:%M:%S"
            )

            res = EVENT_TO_STATE.get(event.type)
            new_state = res(event) if callable(res) else res

            if new_state is not None:
                transitions.append((timestamp, job_id, new_state))

    if not transitions:
        LINES.append("No state-change events found in log file.")
        return LINES

    transitions.sort(key=lambda t: t[0])

    t_start = transitions[0][0]
    t_end = transitions[-1][0]
    total_seconds = (t_end - t_start).total_seconds()

    if total_seconds == 0:
        LINES.append("All events have the same timestamp; cannot build histogram.")
        return LINES

    # Terminal dimensions
    term_size = shutil.get_terminal_size()
    term_width = term_size.columns
    term_height = term_size.lines

    y_label_width = 7   # "NNNNNN "
    chart_width = term_width - y_label_width - 1  # 1 for │
    chart_height = term_height - 7  # title, blank, x-axis, x-labels, blank, legend, new terminal line

    if chart_width < 10 or chart_height < 3:
        LINES.append("Terminal too small for histogram.")
        return LINES

    # Build time bins
    bin_seconds = total_seconds / chart_width
    bins = []
    event_idx = 0

    job_states = {}
    for col in range(chart_width):
        counts = {s: 0 for s in STATES}
        bin_end = t_start + timedelta(seconds=(col + 1) * bin_seconds)
        while event_idx < len(transitions) and transitions[event_idx][0] <= bin_end:
            _, jid, st = transitions[event_idx]
            if mode == HistogramMode.CUMULATIVE:
                job_states[jid] = st
            else:
                counts[st] += 1
            event_idx += 1
        if mode == HistogramMode.CUMULATIVE:
            for st in job_states.values():
                counts[st] += 1

        bins.append(counts)

    max_total = max(sum(b.values()) for b in bins)
    if max_total == 0:
        LINES.append("No jobs found in log file.")
        return LINES

    # Render title
    mode_label = "Cumulative" if mode == HistogramMode.CUMULATIVE else "Instant"
    LINES += [
        bold(f"DAG Event Log Histogram ({mode_label}): {log_file.name}"),
        ""
    ]

    # Compute nice Y-axis tick marks (~5 labels)
    def nice_num(x, do_round=True):
        """Round x to a 'nice' number (1, 2, 5 × 10^n)."""
        if x <= 0:
            return 1
        exp = math.floor(math.log10(x))
        frac = x / (10 ** exp)
        if do_round:
            if frac < 1.5:
                nice = 1
            elif frac < 3:
                nice = 2
            elif frac < 7:
                nice = 5
            else:
                nice = 10
        else:
            if frac <= 1:
                nice = 1
            elif frac <= 2:
                nice = 2
            elif frac <= 5:
                nice = 5
            else:
                nice = 10
        return nice * (10 ** exp)

    tick_step = nice_num(max_total / 5)
    if tick_step < 1:
        tick_step = 1
    tick_step = int(tick_step)

    # Build set of Y values that get labels
    row_labels = {}
    # Generate tick values
    tick_val = tick_step
    while tick_val <= max_total:
        # Map value to row: row = value * chart_height / max_total, rounded
        row = round(tick_val * chart_height / max_total)
        if 1 <= row <= chart_height:
            row_labels[row] = tick_val
        tick_val += tick_step
    # Always label top
    row_labels[chart_height] = max_total

    # Helper: find which state a given Y value falls in for a bin
    def state_at_y(b, y_val):
        """Return the state name at y_val in stacked bar b, or None if empty."""
        cum = 0
        for state in STATES:
            cum += b[state]
            if cum > y_val:
                return state
        return None

    # Render chart rows top to bottom using half-block characters
    for row in range(chart_height, 0, -1):
        # Two sub-rows per character cell
        top_threshold = (row - 0.25) * max_total / chart_height
        bot_threshold = (row - 0.75) * max_total / chart_height

        # Y-axis label (manually print 0 row flush with x-axis)
        if row in row_labels:
            label = f"{row_labels[row]:>6} "
        else:
            label = "       "

        line = label + "│"

        for col in range(chart_width):
            b = bins[col]
            top_state = state_at_y(b, top_threshold)
            bot_state = state_at_y(b, bot_threshold)

            if top_state is None and bot_state is None:
                # Both empty
                line += " "
            elif top_state == bot_state:
                # Both same state - full block
                line += stylize("█", AnsiOptions(color_id=STATES[top_state][KEY_COLOR]))
            elif top_state is None and bot_state is not None:
                # Only bottom half filled
                line += stylize("▄", AnsiOptions(color_id=STATES[bot_state][KEY_COLOR]))
            elif top_state is not None and bot_state is None:
                # Only top half filled
                line += stylize("▀", AnsiOptions(color_id=STATES[top_state][KEY_COLOR]))
            else:
                # Different states: ▄ with fg=bottom, bg=top
                line += stylize("▄", AnsiOptions(
                    color_id=STATES[bot_state][KEY_COLOR],
                    highlight=STATES[top_state][KEY_COLOR],
                ))

        LINES.append(line)

    # X-axis line
    LINES.append("     0 └" + "─" * chart_width)

    # Time labels
    if total_seconds < 86400:
        fmt = "%H:%M:%S"
    else:
        fmt = "%m/%d %H:%M"

    start_str = t_start.strftime(fmt)
    end_str = t_end.strftime(fmt)
    mid_time = t_start + timedelta(seconds=total_seconds / 2)
    mid_str = mid_time.strftime(fmt)

    label_chars = [" "] * chart_width

    # Place start label
    for i, c in enumerate(start_str):
        if i < chart_width:
            label_chars[i] = c

    # Place end label
    end_pos = chart_width - len(end_str)
    for i, c in enumerate(end_str):
        idx = end_pos + i
        if 0 <= idx < chart_width:
            label_chars[idx] = c

    # Place mid label only if it fits without overlapping
    mid_pos = chart_width // 2 - len(mid_str) // 2
    if mid_pos > len(start_str) + 1 and mid_pos + len(mid_str) < end_pos - 1:
        for i, c in enumerate(mid_str):
            idx = mid_pos + i
            if 0 <= idx < chart_width:
                label_chars[idx] = c

    LINES.append("        " + "".join(label_chars))

    # Legend with double-width swatches and wider spacing
    legend_parts = []
    for state in STATES:
        swatch = stylize("██", AnsiOptions(color_id=STATES[state][KEY_COLOR]))
        legend_parts.append(swatch + " " + STATES[state][KEY_LABEL])
    LINES += [
        "",
        "  " + "   ".join(legend_parts),
    ]

    return LINES


def print_job_event_histogram(log_file: Union[Path, str], mode: HistogramMode) -> None:
    for line in produce_job_event_histogram(log_file, mode):
        print(line)

