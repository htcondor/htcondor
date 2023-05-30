import os
import re

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
        }
    }

    def __init__(self, logger, log_file, **options):
        # make sure the specified log file exists and is readable
        log_file = Path(log_file)
        if not log_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(log_file)}")
        if os.access(log_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(log_file)}")
        # dictionary to store summary of events for each job
        event_summaries = {}
        # print events
        with htcondor.JobEventLog(str(log_file)) as event_log:
            for event in event_log.events(0):
                job_id = str(event.cluster) + "." + str(event.proc)
                if job_id not in event_summaries:
                    # probably want to check if event is a submit event
                    ip_address = re.search(r"\d+\.\d+\.\d+\.\d+", event.get("SubmitHost")).group()
                    start_time = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    event_summary = {"Host": ip_address, "Start Time": start_time}
                    event_summaries[job_id] = event_summary
                    event_summary["Evictions"] = 0
                else:
                    event_summary = event_summaries[job_id]
                    if event.type == htcondor.JobEventType.JOB_EVICTED:
                        event_summary["Evict Time"] = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                        event_summary["Evictions"] = event_summary.get("Evictions", 0) + 1
                        delta = datetime.strptime(event_summary["Evict Time"], "%m/%d %H:%M") - datetime.strptime(event_summary["Start Time"], "%m/%d %H:%M")
                        event_summary["Wall Time"] = f"{delta.days}+{delta.seconds//3600}:{(delta.seconds//60)%60}:{delta.seconds%60}"
                        # get good time and cpu usage
                        cpu_usage = re.search(r"(\d+):(\d+):(\d+)", str(event.get("RunRemoteUsage"))).group()
                        event_summary["CPU Usage"] = cpu_usage
                    elif event.type == htcondor.JobEventType.JOB_TERMINATED:
                        event_summary["Evict Time"] = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                        delta = datetime.strptime(event_summary["Evict Time"], "%m/%d %H:%M") - datetime.strptime(event_summary["Start Time"], "%m/%d %H:%M")
                        event_summary["Wall Time"] = f"{delta.days}+{delta.seconds//3600}:{(delta.seconds//60)%60}:{delta.seconds%60}"
                        # get good time and cpu usage
                        cpu_usage = re.search(r"(\d+):(\d+):(\d+)", str(event.get("TotalRemoteUsage"))).group()
                        event_summary["CPU Usage"] = cpu_usage
                    elif event.type == htcondor.JobEventType.EXECUTE:
                        # get hostname if possible
                        if event.get("SlotName") is not None:
                            hostname = event.get("SlotName")
                            event_summary["Host"] = hostname

        # determine maximum length of each column
        max_job_id_len = max(len(str(job_id)) for job_id in event_summaries.keys())+2
        max_host_len = max(len(data.get("Host", "")) for data in event_summaries.values())+2
        max_start_time_len = max(len(data.get("Start Time", "")) for data in event_summaries.values())+2
        max_evict_time_len = max(len(data.get("Evict Time", "")) for data in event_summaries.values())+2
        max_evictions_len = len(str("Evictions"))+2
        max_wall_time_len = len(str("0+00:00:00"))+2
        max_cpu_usage_len = len(str("0+00:00:00"))+2

        # print header with dynamic column spacing
        print(f"{'Job':<{max_job_id_len}} {'Host':<{max_host_len}} {'Start Time':<{max_start_time_len}} {'Evictions':<{max_evictions_len}} {'Evict Time':<{max_evict_time_len}} {'Wall Time':<{max_wall_time_len}} {'CPU Usage':<{max_cpu_usage_len}}  ")

        # print data for each job with dynamic column spacing
        for job_id, data in event_summaries.items():
            host = data.get("Host", "")
            start_time = data.get("Start Time", "")
            evict_time = data.get("Evict Time", "")
            evictions = data.get("Evictions", "")
            wall_time = data.get("Wall Time", "")
            cpu_usage = data.get("CPU Usage", "")
            if data.get("Wall Time"):
                days, wall_time = wall_time.split("+")
                wall_time_hours, wall_time_minutes, wall_time_seconds = wall_time.split(":")
                wall_time_formatted = f"{int(days)}+{int(wall_time_hours):02d}:{int(wall_time_minutes):02d}:{int(wall_time_seconds):02d}"
            print(f"{job_id:<{max_job_id_len}} {host:<{max_host_len}} {start_time:<{max_start_time_len}} {evictions:<{max_evictions_len}} {evict_time:<{max_evict_time_len}} {wall_time_formatted:<{max_wall_time_len}} {cpu_usage:<{max_cpu_usage_len}} ")
            

class EventLog(Noun):
    """
    Interacts with eventlogs
    """
    
    class read(Read):
        pass

    @classmethod
    def verbs(cls):
        return [cls.read]
