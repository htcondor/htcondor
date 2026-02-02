import getpass
import htcondor2 as htcondor
import os
import sys
import stat
import tempfile
import time
import shutil
import math
import argparse

from htcondor2._utils.ansi import Color, colorize, bold, stylize, AnsiOptions
from datetime import datetime, timedelta
from pathlib import Path

from htcondor_cli.eventlog import convert_seconds_to_dhms
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import JobStatus
from htcondor_cli import TMP_DIR

JSM_HTC_DAG_SUBMIT = 4

def FormatTime(time):
    """
    Convert integer into time display string as 'n days hh:mm:ss'
    """
    out = convert_seconds_to_dhms(time)
    parts = out.split("+")
    days_display = ""
    if int(parts[0]) > 0:
        days_display = f"{parts[0]} days" if int(parts[0]) > 1 else "1 day"
    return f"{days_display} and {parts[1]}"

class Submit(Verb):
    """
    Submits a job when given a submit file
    """

    options = {
        "dag_filename": {
            "args": ("dag_filename",),
            "help": "DAG file",
        },
    }

    def __init__(self, logger, dag_filename, **options):
        # Make sure the specified DAG file exists and is readable
        dag_file = Path(dag_filename)
        if not dag_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(dag_file)}")
        if os.access(dag_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(dag_file)}")

        # Get schedd
        schedd = htcondor.Schedd()

        # Submit the DAG to the local schedd
        submit_description = htcondor.Submit.from_dag(dag_filename)
        # Set s_method to HTC_DAG_SUBMIT
        submit_description.setSubmitMethod(JSM_HTC_DAG_SUBMIT,True)

        try:
            cluster_info = schedd.submit(submit_description)
            logger.info(f"DAG {cluster_info.cluster()} was submitted.")
        except Exception as e:
            raise RuntimeError(f"Error submitting DAG:\n{str(e)}")


class Status(Verb):
    """
    Shows current status of a DAG when given a DAG id
    """

    options = {
        "dag_id": {
            "args": ("dag_id",),
            "help": "DAG ID",
        },
    }

    def multi(self, num, past=False):
        if num > 1 and not past:
            return f"{num:3.0f} are"
        elif num > 1 and past:
            return f"{num:3.0f} have"
        elif not past:
            return f"{num:3.0f} is"
        else:
            return f"{num:3.0f} has"

    def __init__(self, logger, dag_id, **options):
        dag = None
        job_status = { 1:"idle", 2:"running", 3:"removed from the queue", 4:"is completed", 5:"held", 6:"transfering output", 7:"suspended" }
        dagman_status = { 0:"is running normally.",
                          1:"had an error occur.",
                          2:"had at least one node fail.",
                          3:"is aborting because it recieved a DAG abort value.",
                          4:"has recieved a signal to be remove.",
                          5:"has detected a cycle.",
                          6:"has been halted."}

        # Get schedd
        schedd = htcondor.Schedd()

        attributes = [
            # Projection of attributes needed: Basic Job information
            "JobStartDate", "JobStatus", "EnteredCurrentStatus", "HoldReason", "JobBatchName",
            # DAG Node information
            "DAG_NodesTotal", "DAG_NodesDone", "DAG_NodesFailed", "DAG_NodesPostrun", "DAG_NodesPrerun", "DAG_NodesQueued", "DAG_NodesReady", "DAG_NodesUnready", "DAG_NodesFutile",
            # DAG job information
            "DAG_JobsSubmitted", "DAG_JobsIdle", "DAG_JobsHeld", "DAG_JobsRunning", "DAG_JobsCompleted",
            # General DAG information
            "DAG_InRecovery", "DAG_Status", "DAG_AdUpdateTime"
        ]

        # Query schedd
        try:
            dag = schedd.query(
                constraint=f"ClusterId == {dag_id}",
                projection=attributes
            )
        except IndexError:
            raise RuntimeError(f"No DAG found for ID {dag_id}.")
        except Exception as e:
            raise RuntimeError(f"Error looking up DAG status: {str(e)}")
        if len(dag) == 0:
            raise RuntimeError(f"No DAG found for ID {dag_id}.")

        # Make sure this is a DAGMan job by verifying the DAG_NodesTotal attribute exists
        if "DAG_NodesTotal" not in dag[0]:
            raise RuntimeError(f"Job {dag_id} is not a DAG")

        ad = dag[0]
        dag_file = ad.get("JobBatchName", "UNKNOWN+1").rsplit("+",1)[0]

        # Now, produce DAG status
        if JobStatus[dag[0]['JobStatus']] == "RUNNING":
            # Check update time
            max_update_time = 600
            update_time = datetime.now() - datetime.fromtimestamp(ad.get("DAG_AdUpdateTime", datetime.now()))
            if update_time.total_seconds() > max_update_time:
                logger.info(colorize(f"Warning: DAG information for ID {dag_id} has not been updated for {FormatTime(update_time.total_seconds())}.", Color.RED))
                logger.info(colorize("         Information may be outdated. This indicates a possible issue with the DAG job.", Color.RED))
            # Check recovery
            if int(dag[0]["DAG_InRecovery"]) == 1:
                logger.info(f"DAG {dag_id} [{dag_file}] is in recovery mode and attempting to restore previous progress.")
            else:
                job_running_time = datetime.now() - datetime.fromtimestamp(dag[0]["JobStartDate"])
                dag_state = "running"
                # Check if halted
                if dag[0]["DAG_Status"] == 6:
                    dag_state = "halted"
                logger.info(f"DAG {dag_id} [{dag_file}] has been {dag_state} for {FormatTime(job_running_time.total_seconds())}")
        elif JobStatus[dag[0]['JobStatus']] == "COMPLETED":
            completion_date = datetime.fromtimestamp(dag[0]["EnteredCurrentStatus"]).strftime("%Y-%m-%d %H:%M:%S")
            logger.info(f"DAG {dag_id} [{dag_file}] completed {completion_date}")
        else:
            job_status_time = datetime.now() - datetime.fromtimestamp(dag[0]["EnteredCurrentStatus"])
            logger.info(f"DAG {dag_id} [{dag_file}] has been {job_status[dag[0]['JobStatus']]} for {FormatTime(job_status_time.total_seconds())}")
            if JobStatus[dag[0]['JobStatus']] == "HELD":
                logger.info(f"Hold Reason: {dag[0]['HoldReason']}")

        # Show some information about the jobs running under this DAG
        if ad.get('DAG_JobsSubmitted') != None:
            failed_jobs = ad.get('DAG_JobsSubmitted', 0) - ad.get('DAG_JobsIdle', 0) - ad.get('DAG_JobsHeld', 0) - ad.get('DAG_JobsRunning', 0) - ad.get('DAG_JobsCompleted', 0)
            logger.info(f"DAG has submitted {dag[0]['DAG_JobsSubmitted']} job(s), of which:")
            if ad.get('DAG_JobsIdle', 0) > 0:
                logger.info(f"\t    {self.multi(ad.get('DAG_JobsIdle'))} submitted and waiting for resources.")
            if ad.get('DAG_JobsHeld', 0) > 0:
                logger.info(f"\t    {self.multi(ad.get('DAG_JobsHeld'))} held.")
            if ad.get('DAG_JobsRunning', 0) > 0:
                logger.info(f"\t    {self.multi(ad.get('DAG_JobsRunning'))} running.")
            if ad.get('DAG_JobsCompleted', 0) > 0:
                logger.info(f"\t    {self.multi(ad.get('DAG_JobsCompleted'),True)} completed.")
            if failed_jobs > 0:
                logger.info(f"\t    {self.multi(failed_jobs,True)} failed.")

        # Show some information about the nodes running under this DAG
        if ad.get('DAG_NodesTotal') != None:
            bar_parts = { "success":{"char":"#", "color":Color.GREEN, "num":0},
                          "running":{"char":"=", "color":Color.CYAN, "num":0},
                          "ready"  :{"char":"»", "color":Color.BRIGHT_CYAN, "num":0},
                          "waiting":{"char":"-", "color":Color.BRIGHT_WHITE, "num":0},
                          "futile" :{"char":"!", "color":Color.YELLOW, "num":0},
                          "failure":{"char":"!", "color":Color.RED, "num":0}}
            logger.info(f"DAG contains {ad.get('DAG_NodesTotal')} node(s) total, of which:")
            nodes_running = ad.get('DAG_NodesPrerun', 0) + ad.get('DAG_NodesQueued', 0) + ad.get('DAG_NodesPostrun', 0)
            key = "?"
            if ad.get('DAG_NodesDone', 0) > 0:
                key = colorize(bar_parts['success']['char'], bar_parts['success']['color'])
                logger.info(f"\t[{key}] {self.multi(ad.get('DAG_NodesDone'),True)} completed.")
            if nodes_running > 0:
                running_parts = ""
                if ad.get('DAG_NodesPrerun', 0) > 0:
                    s = "s" if ad.get('DAG_NodesPrerun') > 1 else ""
                    running_parts = running_parts + f"{ad.get('DAG_NodesPrerun')} pre-script{s}"
                if ad.get('DAG_NodesQueued', 0) > 0:
                    s = "s" if ad.get('DAG_NodesQueued') > 1 else ""
                    div = ", " if len(running_parts) > 0 else ""
                    running_parts = running_parts + f"{div}{ad.get('DAG_NodesQueued')} job{s}"
                if ad.get('DAG_NodesPostrun', 0) > 0:
                    s = "s" if ad.get('DAG_NodesPostrun') > 1 else ""
                    div = ", " if len(running_parts) > 0 else ""
                    running_parts = running_parts + f"{div}{ad.get('DAG_NodesPostrun')} post-script{s}"
                key = colorize(bar_parts['running']['char'], bar_parts['running']['color'])
                logger.info(f"\t[{key}] {self.multi(nodes_running)} running: {running_parts}.")
            if ad.get('DAG_NodesReady', 0) > 0:
                key = colorize(bar_parts['ready']['char'], bar_parts['ready']['color'])
                logger.info(f"\t[{key}] {self.multi(ad.get('DAG_NodesReady'))} ready to start.")
            if ad.get('DAG_NodesUnready', 0) > 0:
                key = colorize(bar_parts['waiting']['char'], bar_parts['waiting']['color'])
                logger.info(f"\t[{key}] {self.multi(ad.get('DAG_NodesUnready'))} waiting on other nodes to finish.")
            if ad.get('DAG_NodesFutile', 0) > 0:
                key = colorize(bar_parts['futile']['char'], bar_parts['futile']['color'])
                logger.info(f"\t[{key}] {ad.get('DAG_NodesFutile'):3.0f} will never run.")
            if ad.get('DAG_NodesFailed', 0) > 0:
                key = colorize(bar_parts['failure']['char'], bar_parts['failure']['color'])
                logger.info(f"\t[{key}] {self.multi(ad.get('DAG_NodesFailed'),True)} failed.")

            bar_width = 50
            if sys.version_info.major >= 3:
                bar_width = round(shutil.get_terminal_size().columns / 2) - 2
            bar_parts["success"]["num"] = math.ceil((ad.get('DAG_NodesDone', 0) / ad.get('DAG_NodesTotal')) * bar_width)
            bar_parts["running"]["num"] = math.ceil((nodes_running / ad.get('DAG_NodesTotal')) * bar_width)
            bar_parts["ready"]["num"]   = math.ceil((ad.get('DAG_NodesReady', 0) / ad.get('DAG_NodesTotal')) * bar_width)
            bar_parts["waiting"]["num"] = math.ceil((ad.get('DAG_NodesUnready', 0) / ad.get('DAG_NodesTotal')) * bar_width)
            bar_parts["futile"]["num"]  = math.ceil((ad.get('DAG_NodesFutile', 0) / ad.get('DAG_NodesTotal')) * bar_width)
            bar_parts["failure"]["num"] = math.ceil((ad.get('DAG_NodesFailed', 0) / ad.get('DAG_NodesTotal')) * bar_width)
            largest_key = "success"
            remainder = bar_width
            for key,info in bar_parts.items():
                remainder = remainder - info["num"]
                if info["num"] > bar_parts[largest_key]["num"]:
                    largest_key = key
            if remainder > 0:
                bar_parts[largest_key]["num"] = bar_parts[largest_key]["num"] + remainder
            bar = "["
            for info in bar_parts.values():
                bar = bar + colorize((info["char"] * info["num"]), info["color"])
            bar = bar + "]"
            complete = (ad.get('DAG_NodesDone', 0) / ad.get('DAG_NodesTotal')) * 100
            possible = (1 - ((ad.get('DAG_NodesFutile', 0) + ad.get('DAG_NodesFailed', 0)) / ad.get('DAG_NodesTotal'))) * 100
            display_portion = ""
            if possible < 100.0:
                display_portion = f" Only {possible:.2f}% of the DAG can complete."
            logger.info(f"DAG {dagman_status[dag[0]['DAG_Status']]}{display_portion}")
            logger.info(f"{bar} DAG is {complete:.2f}% complete.")


class Halt(Verb):
    """
    Halt the progress of a DAG specified by a DAG id.
    """

    options = {
        "dag_id": {
            "args": ("dag_id",),
            "type": int,
            "help": "DAG ID",
        },
        "reason": {
            "args": ("-r", "--reason"),
            "type": str,
            "action": "store",
            "default": None,
            "metavar": "MESSAGE",
            "help": "Reason for halting specified DAG",
        },
        # Hidden option to completely pause a DAG
        "pause": {
            "args":("-p", "--pause"),
            "action": "store_true",
            "default": False,
            "help": argparse.SUPPRESS,
        }
    }

    def __init__(self, logger, dag_id, **options):
        dm = htcondor.DAGMan(dag_id)

        success, msg = dm.halt(options["reason"], options["pause"])

        if not success:
            raise RuntimeError(msg)

        logger.info(msg)


class Resume(Verb):
    """
    Resume progress of a halted DAG specified by a DAG id.
    """

    options = {
        "dag_id": {
            "args": ("dag_id",),
            "type": int,
            "help": "DAG ID",
        },
    }

    def __init__(self, logger, dag_id, **options):
        dm = htcondor.DAGMan(dag_id)

        success, msg = dm.resume()

        if not success:
            raise RuntimeError(msg)

        logger.info(msg)


class Histogram(Verb):
    """
    Displays a stacked histogram of job states over time from a DAG event log
    """

    options = {
        "log_file": {
            "args": ("log_file",),
            "help": "DAG event log file",
        },
        "cumulative": {
            "args": ("-c", "--cumulative"),
            "action": "store_true",
            "default": False,
            "help": "Show cumulative job states over time (default mode)",
        },
        "instant": {
            "args": ("-i", "--instant"),
            "action": "store_true",
            "default": False,
            "help": "Show state transitions per time bucket",
        },
    }

    def __init__(self, logger, log_file, **options):
        log_file = Path(log_file)
        if not log_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(log_file)}")
        if os.access(log_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(log_file)}")

        instant_mode = options.get("instant", False)
        cumulative_mode = options.get("cumulative", False)
        if instant_mode and cumulative_mode:
            raise RuntimeError("Cannot specify both --cumulative and --instant")
        # Default to cumulative if neither specified
        if not instant_mode:
            cumulative_mode = True

        # States and their display properties
        all_states = ["idle", "running", "xfer_in", "xfer_out", "held", "done"]
        state_labels = {
            "done":     "Done",
            "running":  "Running",
            "xfer_in":  "Xfer In",
            "xfer_out": "Xfer Out",
            "idle":     "Idle",
            "held":     "Held",
        }
        # Stack order bottom to top
        stack_order = ["done", "running", "xfer_in", "xfer_out", "idle", "held"]
        # 256-color palette IDs for half-block rendering (fg/bg via AnsiOptions)
        state_color_ids = {
            "done":     2,  # green
            "running":  6,  # cyan
            "xfer_in":  4,  # blue
            "xfer_out": 5,  # magenta
            "idle":     3,  # yellow
            "held":     1,  # red
        }

        # Parse events from the log file
        transitions = []
        with htcondor.JobEventLog(str(log_file)) as event_log:
            for event in event_log.events(0):
                job_id = f"{event.cluster}.{event.proc}"
                timestamp = datetime.strptime(
                    event.get("EventTime"), "%Y-%m-%dT%H:%M:%S"
                )

                new_state = None
                if event.type == htcondor.JobEventType.SUBMIT:
                    new_state = "idle"
                elif event.type == htcondor.JobEventType.EXECUTE:
                    new_state = "running"
                elif event.type == htcondor.JobEventType.JOB_HELD:
                    new_state = "held"
                elif event.type == htcondor.JobEventType.JOB_RELEASED:
                    new_state = "idle"
                elif event.type == htcondor.JobEventType.JOB_EVICTED:
                    new_state = "idle"
                elif event.type == htcondor.JobEventType.JOB_TERMINATED:
                    new_state = "done"
                elif event.type == htcondor.JobEventType.JOB_ABORTED:
                    new_state = "done"
                elif event.type == htcondor.JobEventType.JOB_STAGE_IN:
                    new_state = "xfer_in"
                elif event.type == htcondor.JobEventType.JOB_STAGE_OUT:
                    new_state = "xfer_out"
                elif event.type == htcondor.JobEventType.FILE_TRANSFER:
                    try:
                        ft_type = event.get("Type")
                        if ft_type in (1, 2):       # IN_QUEUED, IN_STARTED
                            new_state = "xfer_in"
                        elif ft_type == 3:           # IN_FINISHED
                            new_state = "running"
                        elif ft_type in (4, 5):      # OUT_QUEUED, OUT_STARTED
                            new_state = "xfer_out"
                        elif ft_type == 6:           # OUT_FINISHED
                            new_state = "running"
                    except Exception:
                        pass

                if new_state is not None:
                    transitions.append((timestamp, job_id, new_state))

        if not transitions:
            print("No state-change events found in log file.")
            return

        transitions.sort(key=lambda t: t[0])

        t_start = transitions[0][0]
        t_end = transitions[-1][0]
        total_seconds = (t_end - t_start).total_seconds()

        if total_seconds == 0:
            print("All events have the same timestamp; cannot build histogram.")
            return

        # Terminal dimensions
        term_size = shutil.get_terminal_size()
        term_width = term_size.columns
        term_height = term_size.lines

        y_label_width = 7   # "NNNNNN "
        chart_width = term_width - y_label_width - 1  # 1 for │
        chart_height = term_height - 6  # title, blank, x-axis, x-labels, blank, legend

        if chart_width < 10 or chart_height < 3:
            print("Terminal too small for histogram.")
            return

        # Build time bins
        bin_seconds = total_seconds / chart_width
        bins = []
        event_idx = 0

        if cumulative_mode:
            # Cumulative: track running state of every job, count jobs per state
            job_states = {}
            for col in range(chart_width):
                bin_end = t_start + timedelta(seconds=(col + 1) * bin_seconds)
                while event_idx < len(transitions) and transitions[event_idx][0] <= bin_end:
                    _, jid, st = transitions[event_idx]
                    job_states[jid] = st
                    event_idx += 1

                counts = {s: 0 for s in all_states}
                for st in job_states.values():
                    counts[st] += 1
                bins.append(counts)
        else:
            # Instant: count state transitions per time bucket
            for col in range(chart_width):
                counts = {s: 0 for s in all_states}
                bin_end = t_start + timedelta(seconds=(col + 1) * bin_seconds)
                while event_idx < len(transitions) and transitions[event_idx][0] <= bin_end:
                    _, _, st = transitions[event_idx]
                    counts[st] += 1
                    event_idx += 1
                bins.append(counts)

        max_total = max(sum(b.values()) for b in bins)
        if max_total == 0:
            print("No jobs found in log file.")
            return

        # Render title
        mode_label = "Cumulative" if cumulative_mode else "Instant"
        print(bold(f"DAG Event Log Histogram ({mode_label}): {log_file.name}"))
        print()

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
        # Always label bottom (0)
        row_labels[1] = 0
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
            for state in stack_order:
                cum += b[state]
                if cum > y_val:
                    return state
            return None

        # Render chart rows top to bottom using half-block characters
        for row in range(chart_height, 0, -1):
            # Two sub-rows per character cell
            top_threshold = (row - 0.25) * max_total / chart_height
            bot_threshold = (row - 0.75) * max_total / chart_height

            # Y-axis label
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
                    line += stylize("█", AnsiOptions(color_id=state_color_ids[top_state]))
                elif top_state is None and bot_state is not None:
                    # Only bottom half filled
                    line += stylize("▄", AnsiOptions(color_id=state_color_ids[bot_state]))
                elif top_state is not None and bot_state is None:
                    # Only top half filled
                    line += stylize("▀", AnsiOptions(color_id=state_color_ids[top_state]))
                else:
                    # Different states: ▄ with fg=bottom, bg=top
                    line += stylize("▄", AnsiOptions(
                        color_id=state_color_ids[bot_state],
                        highlight=state_color_ids[top_state],
                    ))

            print(line)

        # X-axis line
        print("       └" + "─" * chart_width)

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

        print("        " + "".join(label_chars))

        # Legend with double-width swatches and wider spacing
        legend_parts = []
        for state in stack_order:
            swatch = stylize("██", AnsiOptions(color_id=state_color_ids[state]))
            legend_parts.append(swatch + " " + state_labels[state])
        print()
        print("  " + "   ".join(legend_parts))


class DAG(Noun):
    """
    Run operations on HTCondor DAGs
    """

    class submit(Submit):
        pass

    class status(Status):
        pass

    class halt(Halt):
        pass

    class resume(Resume):
        pass

    class histogram(Histogram):
        pass

    """
    class resources(Resources):
        pass
    """

    @classmethod
    def verbs(cls):
        return [cls.submit, cls.status, cls.halt, cls.resume, cls.histogram]


class DAGMan:
    """
    A :class:`DAGMan` holds internal operations related to DAGMan jobs
    """


    @staticmethod
    def get_files(dagman_id):
        """
        Retrieve the filenames of a DAGs output and event logs based on
        DAGMan cluster id
        """

        dag, iwd, log, out = None, None, None, None

        # Get schedd
        schedd = htcondor.Schedd()

        env = schedd.query(
            constraint=f"ClusterId == {dagman_id}",
            projection=["Env", "Iwd"],
        )

        if env:
            iwd = env[0]["Iwd"]
            env = dict(item.split("=", 1) for item in env[0]["Env"].split(";"))
            out = Path(iwd) / Path(os.path.split(env["_CONDOR_DAGMAN_LOG"])[1])
            log = Path(str(out).replace(".dagman.out", ".nodes.log"))
            dag = Path(str(out).replace(".dagman.out", ""))

        return str(dag), str(out), str(log)


    @staticmethod
    def write_slurm_dag(jobfile, runtime, email):

        sendmail_sh = "#!/bin/sh\n"
        if email is not None:
            sendmail_sh += f"\necho -e '$2' | mail -v -s '$1' {email}\n"

        with open(TMP_DIR / "sendmail.sh", "w") as sendmail_sh_file:
            sendmail_sh_file.write(sendmail_sh)
        st = os.stat(TMP_DIR / "sendmail.sh")
        os.chmod(TMP_DIR / "sendmail.sh", st.st_mode | stat.S_IEXEC)

        slurm_config = "DAGMAN_USE_DIRECT_SUBMIT = True\nDAGMAN_USE_STRICT = 0\n"
        with open(TMP_DIR / "slurm_submit.config", "w") as slurm_config_file:
            slurm_config_file.write(slurm_config)

        slurm_dag = f"""JOB A {{
    executable = sendmail.sh
    arguments = \\\"Job submitted to run on Slurm\\\" \\\"Your job ({jobfile}) has been submitted to run on a Slurm resource\\\"
    universe = local
    output = job-A-email.$(cluster).$(process).out
    request_disk = 10M
}}
JOB B {{
    executable = /home/chtcshare/hobblein/hobblein_remote.sh
    universe = grid
    grid_resource = batch slurm hpclogin1.chtc.wisc.edu
    transfer_executable = false
    output = job-B.$(Cluster).$(Process).out
    error = job-B.$(Cluster).$(Process).err
    log = job-B.$(Cluster).$(Process).log
    annex_runtime = {runtime}
    annex_node_count = 1
    annex_name = {getpass.getuser()}-annex
    annex_user = {getpass.getuser()}
    # args: <node count> <run time> <annex name> <user>
    arguments = $(annex_node_count) $(annex_runtime) $(annex_name) $(annex_user)
    +NodeNumber = $(annex_node_count)
    +BatchRuntime = $(annex_runtime)
    request_disk = 30
    notification = NEVER
}}
JOB C {jobfile} DIR {os.getcwd()}
JOB D {{
    executable = sendmail.sh
    arguments = \\\"Job completed run on Slurm\\\" \\\"Your job ({jobfile}) has completed running on a Slurm resource\\\"
    universe = local
    output = job-D-email.$(cluster).$(process).out
    request_disk = 10M
}}

PARENT A CHILD B C
PARENT B C CHILD D

CONFIG slurm_submit.config

VARS C Requirements="(Facility == \\\"CHTC_Slurm\\\")"
VARS C +MayUseSlurm="True"
VARS C +WantFlocking="True"
"""

        with open(TMP_DIR / "slurm_submit.dag", "w") as dag_file:
            dag_file.write(slurm_dag)


    @staticmethod
    def write_ec2_dag(jobfile, runtime, email):

        sendmail_sh = "#!/bin/sh\n"
        if email is not None:
            sendmail_sh += f"\necho -e '$2' | mail -v -s '$1' {email}\n"

        with open(TMP_DIR / "sendmail.sh", "w") as sendmail_sh_file:
            sendmail_sh_file.write(sendmail_sh)
        st = os.stat(TMP_DIR / "sendmail.sh")
        os.chmod(TMP_DIR / "sendmail.sh", st.st_mode | stat.S_IEXEC)

        ec2_annex_sh = f"""#!/bin/sh

yes | /usr/bin/condor_annex -count 1 -duration $1 -annex-name EC2Annex-{int(time.time())}
"""
        with open(TMP_DIR / "ec2_annex.sh", "w") as ec2_annex_sh_file:
            ec2_annex_sh_file.write(ec2_annex_sh)
        st = os.stat(TMP_DIR / "ec2_annex.sh")
        os.chmod(TMP_DIR / "ec2_annex.sh", st.st_mode | stat.S_IEXEC)

        ec2_config = "DAGMAN_USE_DIRECT_SUBMIT = True\nDAGMAN_USE_STRICT = 0\n"
        with open(TMP_DIR / "ec2_submit.config", "w") as ec2_config_file:
            ec2_config_file.write(ec2_config)

        ec2_dag = f"""JOB A {{
    executable = sendmail.sh
    arguments = \\\"Job submitted to run on EC2\\\" \\\"Your job ({jobfile}) has been submitted to run on a EC2 resource\\\"
    universe = local
    output = job-A-email.$(cluster).$(process).out
    request_disk = 10M
}}
JOB B {{
    executable = ec2_annex.sh
    arguments = {runtime}
    output = job-B-ec2_annex.$(Cluster).$(Process).out
    error = job-B-ec2_annex.$(Cluster).$(Process).err
    log = ec2_annex.log
    universe = local
    request_disk = 10M
}}
JOB C {jobfile} DIR {os.getcwd()}
JOB D {{
    executable = sendmail.sh
    arguments = \\\"Job completed run on EC2\\\" \\\"Your job ({jobfile}) has completed running on a EC2 resource\\\"
    universe = local
    output = job-D-email.$(cluster).$(process).out
    request_disk = 10M
}}

PARENT A CHILD B C
PARENT B C CHILD D

CONFIG ec2_submit.config

VARS C Requirements="(EC2InstanceID =!= undefined) && (TRUE || TARGET.OpSysMajorVer)"
VARS C +MayUseAWS="True"
VARS C +WantFlocking="True"
"""

        with open(TMP_DIR / "ec2_submit.dag", "w") as dag_file:
            dag_file.write(ec2_dag)
