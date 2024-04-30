import typer
import os
import traceback
import htcondor
import json
import re

from pathlib import Path
from typing_extensions import Annotated
from typing import List, Optional
from enum import Enum
from datetime import datetime
from datetime import timedelta

eventlog_app = typer.Typer()


class attributes(str, Enum):
    GLIDEIN_ResourceName = "GLIDEIN_ResourceName"
    Memory = "Memory"


def convert_dhms_to_seconds(job_summary, key):
    """
    Converts a string in the format "0+00:00:00" to the number of seconds
    """
    # check if already in seconds
    if isinstance(job_summary.get(key, ""), int):
        return
    time = job_summary.get(key, "0+00:00:00")
    # convert time to seconds
    days, time_str = time.split("+")
    hours, minutes, seconds = map(int, time_str.split(":"))
    time_seconds = int(days)*24*60*60 + int(hours)*60 * \
        60 + int(minutes)*60 + int(seconds)
    job_summary[key] = time_seconds


def get_max_column_length(job_summaries, key):
    """
    Gets the length of the longest value for the specified key
    """
    max_length = len(key)
    for job_summary in job_summaries.values():
        length = len(str(job_summary.get(key, "")))
        if length > max_length:
            max_length = length
    return max_length + 2


def convert_seconds_to_dhms(seconds):
    """
    Converts a number of seconds to a string in the format "0+00:00:00"
    """
    days = seconds // (24*60*60)
    seconds = seconds % (24*60*60)
    hours = seconds // (60*60)
    seconds = seconds % (60*60)
    minutes = seconds // 60
    seconds = seconds % 60
    return f"{days}+{hours:02d}:{minutes:02d}:{seconds:02d}"


def get_job_summaries(log_file, group_by):
    """
    Gets a dictionary of job summaries from the specified log file
    """
    # dictionary to store summary of events for each job
    job_summaries = {}
    # definetly need to refactor this or test to make sure most use cases work like the three end cases should be termination, abort, and remove not evict like it is now
    job_event_log = htcondor.JobEventLog(str(log_file))
    for event in job_event_log.events(0):
        job_id = str(event.cluster) + "." + str(event.proc)
        job_summary = job_summaries.get(job_id)
        if job_summary is None:
            job_summary = {}
            job_summary["Submitted"] = False
            job_summary["Evictions"] = 0
            job_summary["Executed"] = False
            job_summary["starts"] = 0
            job_summary["Job ID"] = job_id
            job_summaries[job_id] = job_summary
        if event.type == htcondor.JobEventType.SUBMIT:
            ip_address = re.search(
                r"\d+\.\d+\.\d+\.\d+", event.get("SubmitHost")).group()
            job_summary["Submitted"] = True
            job_summary["Host"] = ip_address
        elif (event.type == htcondor.JobEventType.JOB_ABORTED or event.type == htcondor.JobEventType.JOB_EVICTED) and job_summary.get("Executed", False) is False or job_summary["Submitted"] is False:
            # remove job from dictionary if it was aborted before being executed or not submitted
            job_summaries.pop(job_id)
        elif event.type == htcondor.JobEventType.EXECUTE:
            # get hostname if possible
            if event.get("SlotName") is not None:
                hostname = event.get("SlotName")
                job_summary["Host"] = hostname
            # get grouping information, if any
            if group_by is not None and event.get("ExecuteProps") is not None:
                job_summary[group_by] = event.get(
                    "ExecuteProps").get(group_by)
            # get last execution time
            start_time = datetime.strptime(
                event.get("EventTime"), "%Y-%m-%dT%H:%M:%S").strftime("%-m/%-d %H:%M")
            job_summary["Start Time"] = start_time
            job_summary["last_exec_time"] = start_time
            # set boolean that job has been executed
            job_summary["Executed"] = True
            job_summary["starts"] = job_summary.get("starts", 0) + 1
        elif event.type == htcondor.JobEventType.JOB_EVICTED or event.type == htcondor.JobEventType.JOB_TERMINATED:
            # get good time and cpu usage
            cpu_usage_key = "RunRemoteUsage" if event.type == htcondor.JobEventType.JOB_EVICTED else "TotalRemoteUsage"
            cpu_usage = str(event.get(cpu_usage_key))
            user_time = re.search(
                r"Usr (\d+) (\d+):(\d+):(\d+),", cpu_usage)
            sys_time = re.search(r"Sys (\d+) (\d+):(\d+):(\d+)", cpu_usage)
            user_days, user_hours, user_minutes, user_seconds = user_time.group(
                1, 2, 3, 4)
            sys_days, sys_hours, sys_minutes, sys_seconds = sys_time.group(
                1, 2, 3, 4)
            total_days = int(user_days) + int(sys_days)
            total_hours = int(user_hours) + int(sys_hours)
            total_minutes = int(user_minutes) + int(sys_minutes)
            total_seconds = int(user_seconds) + int(sys_seconds)
            cpu_usage_formatted = f"{total_days}+{total_hours:02d}:{total_minutes:02d}:{total_seconds:02d}"
            job_summary["CPU Usage"] = cpu_usage_formatted
            if event.type == htcondor.JobEventType.JOB_EVICTED:
                job_summary["Evictions"] = job_summary.get(
                    "Evictions", 0) + 1
                job_summary["last_exec_time"] = datetime.strptime(
                    event.get("EventTime"), "%Y-%m-%dT%H:%M:%S").strftime("%-m/%-d %H:%M")
            job_summary["Evict Time"] = datetime.strptime(
                event.get("EventTime"), "%Y-%m-%dT%H:%M:%S").strftime("%-m/%-d %H:%M")
            delta = datetime.strptime(
                job_summary["Evict Time"], "%m/%d %H:%M") - datetime.strptime(job_summary["Start Time"], "%m/%d %H:%M")
            wall_time = f"{delta.days}+{delta.seconds // 3600:02d}:{(delta.seconds//60) % 60:02d}:{delta.seconds % 60:02d}"
            job_summary["Wall Time"] = wall_time
            # get total execution time for calculating good time
            event_time = datetime.strptime(
                event.get("EventTime"), "%Y-%m-%dT%H:%M:%S").strftime("%-m/%-d %H:%M")
            delta = datetime.strptime(
                event_time, "%m/%d %H:%M") - datetime.strptime(job_summary["last_exec_time"], "%m/%d %H:%M")
            good_time = f"{delta.days}+{delta.seconds // 3600:02d}:{(delta.seconds//60) % 60:02d}:{delta.seconds % 60:02d}"
            job_summary["Good Time"] = good_time
            # get Return Value
            job_summary["Return Value"] = event.get("ReturnValue")
    return job_summaries


def output_job_summaries(job_summaries, group_by, options):
    """
    Outputs the specified job summaries
    """
    # will probably need to refactor this to be more efficient for other group_by options
    if group_by is not None:
        if group_by == "GLIDEIN_ResourceName":
            # get total stats for each site
            site_stats = {}
            for job_id, job_summary in job_summaries.items():
                site = job_summary.get(group_by, "")
                if site not in site_stats:
                    site_stats[site] = {"Evictions": 0, "Wall Time": 0,
                                        "Good Time": 0, "CPU Usage": 0, "Successes": 0, "Failures": 0}
                site_stats[site]["Evictions"] += int(
                    job_summary.get("Evictions", 0))
                # get wall time and convert to time format for adding
                convert_dhms_to_seconds(job_summary, "Wall Time")
                site_stats[site]["Wall Time"] += job_summary.get(
                    "Wall Time", 0)
                # get good time and convert to time format for adding
                convert_dhms_to_seconds(job_summary, "Good Time")
                site_stats[site]["Good Time"] += job_summary.get(
                    "Good Time", 0)
                # get cpu usage and convert to seconds for adding
                convert_dhms_to_seconds(job_summary, "CPU Usage")
                site_stats[site]["CPU Usage"] += job_summary.get(
                    "CPU Usage", 0)
                # count number of times any job started at site
                site_stats[site]["Starts"] = site_stats[site].get(
                    "Starts", 0) + job_summary.get("starts")
                # count number of times any job ended at site with return value 0
                if job_summary.get("Return Value") == 0:
                    site_stats[site]["Successes"] = site_stats[site].get(
                        "Successes") + 1
                elif job_summary.get("Return Value") is None:
                    continue
                elif job_summary.get("Return Value") != 0:
                    site_stats[site]["Failures"] = site_stats[site].get(
                        "Failures") + 1
            # convert wall time, good time, and cpu usage to time format
            for site, stats in site_stats.items():
                site_stats[site]["Wall Time"] = convert_seconds_to_dhms(
                    stats["Wall Time"])
                site_stats[site]["Good Time"] = convert_seconds_to_dhms(
                    stats["Good Time"])
                site_stats[site]["CPU Usage"] = convert_seconds_to_dhms(
                    stats["CPU Usage"])
            # if json output, print json
            if options.get("json", True):
                print(json.dumps(site_stats, indent=4))
            elif options.get("csv", True):
                # print header
                print(
                    "Site,Evictions,Wall Time,Good Time,CPU Usage,Job Starts,Job Successes,Job Failures")
                # print stats for each site
                for site, stats in site_stats.items():
                    if site:
                        print(
                            f"{site},{stats['Evictions']},{stats['Wall Time']},{stats['Good Time']},{stats['CPU Usage']},{stats['Starts']},{stats.get('Successes', '')},{stats.get('Failures', '')}")
            else:
                # print header
                print(f"{'Site':<20} {'Evictions':<12} {'Wall Time':<12} {'Good Time':<12} {'CPU Usage':<12} {'Job Starts':<12} {'Job Successes':<14} {'Job Failures':<12}")
                # print stats for each site
                for site, stats in site_stats.items():
                    if site:
                        print(
                            f"{site:<20} {stats['Evictions']:<12} {stats['Wall Time']:<12} {stats['Good Time']:<12} {stats['CPU Usage']:<12} {stats['Starts']:<12} {stats.get('Successes', ''):<14} {stats.get('Failures', ''):<12}")
    elif options.get("json", True) or options.get("csv", True):
        # add event summaries as objects to list for JSON output
        json_output = []
        for job_id, job_summary in job_summaries.items():
            # convert cpu usage, wall time, and good time to seconds
            convert_dhms_to_seconds(job_summary, "CPU Usage")
            convert_dhms_to_seconds(job_summary, "Wall Time")
            convert_dhms_to_seconds(job_summary, "Good Time")
            json_output.append({"Job ID": job_id, **job_summary})
        if options.get("csv", True):
            print(
                "Job ID,Host,Start Time,Evict Time,Evictions,Wall Time,Good Time,CPU Usage")
            for job_id, job_summary in job_summaries.items():
                print(f"{job_id},{job_summary.get('Host', '')},{job_summary.get('Start Time', '')},{job_summary.get('Evict Time', '')},{job_summary.get('Evictions', '')},{job_summary.get('Wall Time', '')},{job_summary.get('Good Time', '')},{job_summary.get('CPU Usage', '')}")
        else:
            print(json.dumps(json_output, indent=4))
    else:
        # determine maximum length of each column
        max_job_id_len = get_max_column_length(job_summaries, "Job ID")
        max_host_len = get_max_column_length(job_summaries, "Host")
        max_start_time_len = get_max_column_length(job_summaries, "Start Time")
        max_evict_time_len = get_max_column_length(job_summaries, "Evict Time")
        max_evictions_len = get_max_column_length(job_summaries, "Evictions")
        max_time_format = 13

        # print header with dynamic column spacing
        header = f"{'Job':<{max_job_id_len}} {'Host':<{max_host_len}} {'Start Time':<{max_start_time_len}} {'Evict Time':<{max_evict_time_len}} {'Evictions':<{max_evictions_len}} {'Wall Time':<{max_time_format}} {'Good Time':<{max_time_format}} {'CPU Usage':<{max_time_format}}  "
        print(header)
        i = 1
        # print data for each job with dynamic column spacing
        for job_id, data in job_summaries.items():
            if i % 50 == 0:
                print(header)
            host = data.get("Host", "")
            start_time = data.get("Start Time", "")
            evict_time = data.get("Evict Time", "")
            evictions = data.get("Evictions", "")
            wall_time = data.get("Wall Time", "")
            good_time = data.get("Good Time", "")
            cpu_usage = data.get("CPU Usage", "")
            print(f"{job_id:<{max_job_id_len}} {host:<{max_host_len}} {start_time:<{max_start_time_len}} {evict_time:<{max_evict_time_len}} {evictions:<{max_evictions_len}} {wall_time:<{max_time_format}} {good_time:<{max_time_format}} {cpu_usage:<{max_time_format}} ")
            i += 1

def complete_log_files(incomplete: str) -> List[str]:
    """
    Autocompletion function that returns .log files matching the incomplete input.
    """
    # List all files in the current directory (or a specific path)
    files = os.listdir('.')
    # Filter files to get those ending with .log
    log_files = [f for f in files if f.endswith('.log')]
    # Return files that match the incomplete input
    return [f for f in log_files if f.startswith(incomplete)]

@eventlog_app.command()
def read(
    log_files: Annotated[List[str], typer.Argument(..., autocompletion=complete_log_files, help="Log file or space separated list of log files")],
    json: Annotated[bool, typer.Option(help="Output in JSON format")] = False,
    csv: Annotated[bool, typer.Option(help="Output in CSV format")] = False,
    group: Annotated[Optional[attributes], typer.Option(
        help="Group by this attribute")] = None
):
    """
    Reads an eventlog
    """
    job_summaries_across_files = {}  # Stores summary of events for all jobs across files
    for file_path in log_files:
        # Ensure each specified log file exists and is readable
        log_file = Path(file_path)
        if not log_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(log_file)}")
        if os.access(log_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(log_file)}")
        try:
            current_file_job_summaries = get_job_summaries(log_file, group)
            # Merge current file's job summaries into the collective summary
            for job_id, job_summary in current_file_job_summaries.items():
                if job_id not in job_summaries_across_files:
                    job_summaries_across_files[job_id] = job_summary
                else:
                    # Update existing job summary with new information from current file
                    job_summaries_across_files[job_id].update(job_summary)
        except Exception as error:
            traceback.print_exc()
            return
    # Output aggregated job summaries
    options = {"json": json, "csv": csv}
    output_job_summaries(job_summaries_across_files, group, options)


@eventlog_app.command()
def follow(
    log_file: Annotated[str, typer.Argument(..., autocompletion=complete_log_files, help="Log file")],
    json: Annotated[bool, typer.Option(help="Output in JSON format")] = False,
    csv: Annotated[bool, typer.Option(help="Output in CSV format")] = False,
    group: Annotated[Optional[attributes], typer.Option(
        help="Group by this attribute")] = None
):
    """
    Follows an eventlog
    """
    # make sure the specified log file exists and is readable
    log_file = Path(log_file)
    if not log_file.exists():
        raise FileNotFoundError(f"Could not find file: {str(log_file)}")
    if os.access(log_file, os.R_OK) is False:
        raise PermissionError(f"Could not access file: {str(log_file)}")
    try:
        # output job summary when log file is updated
        last_size = log_file.stat().st_size
        options = {"json": json, "csv": csv}
        output_job_summaries(get_job_summaries(
            log_file, group), group, options)
        while True:
            # check if the log file has changed
            current_size = log_file.stat().st_size
            if current_size != last_size:
                # get event summaries
                job_summaries = get_job_summaries(log_file, group)
                # output event summaries
                output_job_summaries(job_summaries, group, options)
                # update last size
                last_size = current_size
    except KeyboardInterrupt:
        # exit gracefully if user presses Ctrl+C
        print(" Exiting...")
        pass


if __name__ == "__main__":
    eventlog_app()
