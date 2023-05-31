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

        with htcondor.JobEventLog(str(log_file)) as event_log:
            for event in event_log.events(0):
                job_id = str(event.cluster) + "." + str(event.proc)
                event_summary = event_summaries.get(job_id, {})
                if event.type == htcondor.JobEventType.SUBMIT:
                    ip_address = re.search(r"\d+\.\d+\.\d+\.\d+", event.get("SubmitHost")).group()
                    event_summaries[job_id] = {"Host": ip_address, "Evictions": 0,}
                elif event.type == htcondor.JobEventType.EXECUTE:
                    # get hostname if possible
                    if event.get("SlotName") is not None:
                        hostname = event.get("SlotName")
                        event_summary["Host"] = hostname
                    # get last execution time
                    start_time = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    event_summary["Start Time"] = start_time
                    event_summary["last_exec_time"] = start_time
                elif event.type == htcondor.JobEventType.JOB_EVICTED or event.type == htcondor.JobEventType.JOB_TERMINATED:
                    # get good time and cpu usage
                    if event.type == htcondor.JobEventType.JOB_EVICTED:
                        event_summary["Evictions"] = event_summary.get("Evictions", 0) + 1
                        cpu_usage = re.search(r"(\d+):(\d+):(\d+)", str(event.get("RunRemoteUsage"))).group()
                        event_summary["CPU Usage"] = cpu_usage
                        event_summary["last_exec_time"] = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    else:
                        cpu_usage = re.search(r"(\d+):(\d+):(\d+)", str(event.get("TotalRemoteUsage"))).group()
                        event_summary["CPU Usage"] = cpu_usage
                    event_summary["Evict Time"] = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    delta = datetime.strptime(event_summary["Evict Time"], "%m/%d %H:%M") - datetime.strptime(event_summary["Start Time"], "%m/%d %H:%M")
                    wall_time = f"{delta.days}+{delta.seconds//3600:02d}:{(delta.seconds//60)%60:02d}:{delta.seconds%60:02d}"
                    event_summary["Wall Time"] = wall_time
                    # get total execution time for calculating good time
                    event_time = datetime.fromisoformat(event.get("EventTime")).strftime("%-m/%-d %H:%M")
                    delta = datetime.strptime(event_time, "%m/%d %H:%M") - datetime.strptime(event_summary["last_exec_time"], "%m/%d %H:%M")
                    good_time = f"{delta.days}+{delta.seconds//3600:02d}:{(delta.seconds//60)%60:02d}"
                    event_summary["Good Time"] = good_time
        
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