import os
import re
import json

from pathlib import Path

import htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from datetime import datetime
from datetime import timedelta

class Read(Verb):
    """
    Reads an eventlog
    """

    options = {
        "log_file": {
            "args": ("log_file",),
            "help": "Log file",
       },
        "json": {
            "args": ("-json",),
            "action": "store_true",
            "help": "Output logs in JSON format",
        },
        "csv": {
            "args": ("-csv",),
            "action": "store_true",
            "help": "Output logs in CSV format",
        },
        "groupby": {
            "args": ("-groupby",),
            "help": "Group output by specified attribute",
            "type": str,
            "choices": ["GLIDEIN_ResourceName"],
        },
    }

    def __init__(self, logger, log_file, groupby=None, **options):
        # make sure the specified log file exists and is readable
        log_file = Path(log_file)
        if not log_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(log_file)}")
        if os.access(log_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(log_file)}")
        
        # dictionary to store summary of events for each job
        event_summaries = {}

        # definetly need to refactor this or test to make sure most use cases work like the three end cases should be termination, abort, and remove not evict like it is now
        with htcondor.JobEventLog(str(log_file)) as event_log:
            for event in event_log.events(0):
                job_id = str(event.cluster) + "." + str(event.proc)
                event_summary = event_summaries.get(job_id, {})
                if event.type == htcondor.JobEventType.SUBMIT:
                    ip_address = re.search(r"\d+\.\d+\.\d+\.\d+", event.get("SubmitHost")).group()
                    event_summaries[job_id] = {"Host": ip_address, "Evictions": 0, "Executed": False, "starts": 0}
                elif event.type == htcondor.JobEventType.JOB_ABORTED and event_summary.get("Executed", False) is False:
                    # remove job from dictionary if it was aborted before being executed
                    event_summaries.pop(job_id)
                elif event.type == htcondor.JobEventType.EXECUTE:
                    # get hostname if possible
                    if event.get("SlotName") is not None:
                        hostname = event.get("SlotName")
                        event_summary["Host"] = hostname
                    # get site name
                    if groupby == "GLIDEIN_ResourceName":
                        site = event.get("ExecuteProps").get(groupby)
                        event_summary["Site"] = site
                    # get last execution time
                    start_time = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    event_summary["Start Time"] = start_time
                    event_summary["last_exec_time"] = start_time
                    # set boolean that job has been executed
                    event_summary["Executed"] = True
                    event_summary["starts"] = event_summary.get("starts", 0) + 1
                elif event.type == htcondor.JobEventType.JOB_EVICTED or event.type == htcondor.JobEventType.JOB_TERMINATED:
                    # get good time and cpu usage
                    cpu_usage_key = "RunRemoteUsage" if event.type == htcondor.JobEventType.JOB_EVICTED else "TotalRemoteUsage"
                    cpu_usage = str(event.get(cpu_usage_key))
                    user_time = re.search(r"Usr \d+ (\d+):(\d+):(\d+),", cpu_usage)
                    sys_time = re.search(r"Sys \d+ (\d+):(\d+):(\d+)", cpu_usage)
                    user_minutes, user_seconds = user_time.group(1, 2)
                    sys_minutes, sys_seconds = sys_time.group(1, 2)
                    total_minutes = int(user_minutes) + int(sys_minutes)
                    total_seconds = int(user_seconds) + int(sys_seconds)
                    cpu_usage_formatted = f"0+{total_minutes:02d}:{total_seconds:02d}"
                    event_summary["CPU Usage"] = cpu_usage_formatted
                    if event.type == htcondor.JobEventType.JOB_EVICTED:
                        event_summary["Evictions"] = event_summary.get("Evictions", 0) + 1
                        event_summary["last_exec_time"] = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    event_summary["Evict Time"] = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    delta = datetime.strptime(event_summary["Evict Time"], "%m/%d %H:%M") - datetime.strptime(event_summary["Start Time"], "%m/%d %H:%M")
                    wall_time = f"{delta.days}+{delta.seconds//3600:02d}:{(delta.seconds//60)%60:02d}:{delta.seconds%60:02d}"
                    event_summary["Wall Time"] = wall_time
                    # get total execution time for calculating good time
                    event_time = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    delta = datetime.strptime(event_time, "%m/%d %H:%M") - datetime.strptime(event_summary["last_exec_time"], "%m/%d %H:%M")
                    good_time = f"{delta.days}+{delta.seconds//3600:02d}:{(delta.seconds//60)%60:02d}"
                    event_summary["Good Time"] = good_time
                    # get Return Value
                    event_summary["Return Value"] = event.get("ReturnValue")
        # will probably need to refactor this to be more efficient for other groupby options
        if groupby:
            if groupby == "GLIDEIN_ResourceName":
                # get total stats for each site
                site_stats = {}
                for job_id, event_summary in event_summaries.items():
                    site = event_summary.get("Site", "")
                    if site not in site_stats:
                        site_stats[site] = {"Evictions": 0, "Wall Time": 0, "Good Time": 0, "CPU Usage": 0, "Successes": 0, "Failures": 0}
                    site_stats[site]["Evictions"] += int(event_summary.get("Evictions", 0))
                    # get wall time and convert to time format for adding
                    wall_time = event_summary.get("Wall Time", "0+00:00:00")
                    wall_time_seconds = int(wall_time.split("+")[1].replace(":", ""))
                    site_stats[site]["Wall Time"] += wall_time_seconds
                    # get good time and convert to time format for adding
                    good_time = event_summary.get("Good Time", "0+00:00:00")
                    good_time_seconds = int(good_time.split("+")[1].replace(":", ""))
                    site_stats[site]["Good Time"] += good_time_seconds
                    # get cpu usage and convert to seconds for adding
                    cpu_usage = event_summary.get("CPU Usage", "0+00:00:00")
                    cpu_usage_seconds = int(cpu_usage.split("+")[1].replace(":", ""))
                    site_stats[site]["CPU Usage"] += cpu_usage_seconds
                    # count number of times any job started at site
                    site_stats[site]["Starts"] = site_stats[site].get("Starts", 0) + event_summary.get("starts")
                    # count number of times any job ended at site with return value 0
                    if event_summary.get("Return Value") == 0:
                        site_stats[site]["Successes"] = site_stats[site].get("Successes") + 1
                    elif event_summary.get("Return Value") is None:
                        continue
                    elif event_summary.get("Return Value") != 0:
                        site_stats[site]["Failures"] = site_stats[site].get("Failures") + 1
                # convert wall time, good time, and cpu usage to time format
                for site, stats in site_stats.items():
                    wall_time = f"0+{stats['Wall Time']//3600:02d}:{(stats['Wall Time']//60)%60:02d}:{stats['Wall Time']%60:02d}"
                    site_stats[site]["Wall Time"] = wall_time
                    good_time = f"0+{stats['Good Time']//3600:02d}:{(stats['Good Time']//60)%60:02d}:{stats['Good Time']%60:02d}"
                    site_stats[site]["Good Time"] = good_time
                    cpu_usage = f"0+{stats['CPU Usage']//3600:02d}:{(stats['CPU Usage']//60)%60:02d}:{stats['CPU Usage']%60:02d}"
                    site_stats[site]["CPU Usage"] = cpu_usage
                # if json output, print json
                if options.get("json", True):
                    print(json.dumps(site_stats, indent=4))
                elif options.get("csv", True):
                    # print header
                    print("Site,Evictions,Wall Time,Good Time,CPU Usage,Job Starts,Job Successes,Job Failures")
                    # print stats for each site
                    for site, stats in site_stats.items():
                        if site:
                            print(f"{site},{stats['Evictions']},{stats['Wall Time']},{stats['Good Time']},{stats['CPU Usage']},{stats['Starts']},{stats.get('Successes', '')},{stats.get('Failures', '')}")
                else:
                # print header
                    print(f"{'Site':<20} {'Evictions':<12} {'Wall Time':<12} {'Good Time':<12} {'CPU Usage':<12} {'Job Starts':<12} {'Job Successes':<14} {'Job Failures':<12}")
                    # print stats for each site
                    for site, stats in site_stats.items():
                        if site:
                            print(f"{site:<20} {stats['Evictions']:<12} {stats['Wall Time']:<12} {stats['Good Time']:<12} {stats['CPU Usage']:<12} {stats['Starts']:<12} {stats.get('Successes', ''):<14} {stats.get('Failures', ''):<12}")
        elif options.get("json", True) or options.get("csv", True):
            # add event summaries as objects to list for JSON output
            json_output = []
            for job_id, event_summary in event_summaries.items():
                # convert cpu usage, wall time, and good time to seconds
                cpu_usage = event_summary.get("CPU Usage", "0+00:00:00")
                cpu_usage_seconds = int(cpu_usage.split("+")[1].replace(":", ""))
                wall_time = event_summary.get("Wall Time", "0+00:00:00")
                wall_time_seconds = int(wall_time.split("+")[1].replace(":", ""))
                good_time = event_summary.get("Good Time", "0+00:00:00")
                good_time_seconds = int(good_time.split("+")[1].replace(":", ""))
                # replace cpu usage, wall time, and good time with seconds
                event_summary["CPU Usage"] = cpu_usage_seconds
                event_summary["Wall Time"] = wall_time_seconds
                event_summary["Good Time"] = good_time_seconds
                json_output.append({"Job ID": job_id, **event_summary})
            if options.get("csv", True):
                print("Job ID,Host,Start Time,Evict Time,Evictions,Wall Time,Good Time,CPU Usage")
                for job_id, event_summary in event_summaries.items():
                    print(f"{job_id},{event_summary.get('Host', '')},{event_summary.get('Start Time', '')},{event_summary.get('Evict Time', '')},{event_summary.get('Evictions', '')},{event_summary.get('Wall Time', '')},{event_summary.get('Good Time', '')},{event_summary.get('CPU Usage', '')}")
            else:
                print(json.dumps(json_output, indent=4))
        else:
            # determine maximum length of each column
            max_job_id_len = max(len(str(job_id)) for job_id in event_summaries.keys())+2
            max_host_len = max(len(data.get("Host", "")) for data in event_summaries.values())+2
            max_start_time_len = max(len(data.get("Start Time", "")) for data in event_summaries.values())+2
            max_evict_time_len = max(len(data.get("Evict Time", "")) for data in event_summaries.values())+2
            max_evictions_len = len(str("Evictions"))+2
            max_time_format = len(str("0+00:00:00"))+2

            # print header with dynamic column spacing
            print(f"{'Job':<{max_job_id_len}} {'Host':<{max_host_len}} {'Start Time':<{max_start_time_len}} {'Evict Time':<{max_evict_time_len}} {'Evictions':<{max_evictions_len}} {'Wall Time':<{max_time_format}} {'Good Time':<{max_time_format}} {'CPU Usage':<{max_time_format}}  ")

            # print data for each job with dynamic column spacing
            for job_id, data in event_summaries.items():
                host = data.get("Host", "")
                start_time = data.get("Start Time", "")
                # should anything be output if there is no evict time?
                evict_time = data.get("Evict Time", "")
                evictions = data.get("Evictions", "")
                wall_time = data.get("Wall Time", "")
                good_time = data.get("Good Time", "")
                cpu_usage = data.get("CPU Usage", "")
                print(f"{job_id:<{max_job_id_len}} {host:<{max_host_len}} {start_time:<{max_start_time_len}} {evict_time:<{max_evict_time_len}} {evictions:<{max_evictions_len}} {wall_time:<{max_time_format}} {good_time:<{max_time_format}} {cpu_usage:<{max_time_format}} ")
            

class EventLog(Noun):
    """
    Interacts with eventlogs
    """
    
    class read(Read):
        pass

    @classmethod
    def verbs(cls):
        return [cls.read]