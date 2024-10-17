import sys
import os
import os.path
import logging
import subprocess
import shlex
import shutil
import tempfile
import time
import getpass


from datetime import datetime
from pathlib import Path

import htcondor2 as htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli.dagman import DAGMan
from htcondor_cli import TMP_DIR
from htcondor_cli.utils import readable_time, readable_size, s

JSM_HTC_JOB_SUBMIT = 3

def get_job_ad(logger, job_id, options):
    # Split job id into cluster and proc
    if "." in job_id:
        cluster_id, proc_id = [int(i) for i in job_id.split(".")][:2]
    else:
        cluster_id = int(job_id)
        proc_id = 0

    # Get schedd
    schedd = htcondor.Schedd()

    # Query schedd
    constraint = f"ClusterId == {cluster_id} && ProcId == {proc_id}"
    projection = ["JobStartDate", "JobStatus", "QDate", "CompletionDate", "EnteredCurrentStatus",
                "RequestMemory", "MemoryUsage", "RequestDisk", "DiskUsage", "HoldReason",
                "ResourceType", "TargetAnnexName", "NumShadowStarts", "NumJobStarts", "NumHolds",
                "JobCurrentStartTransferOutputDate", "TotalSuspensions", "CommittedTime",
                "RemoteWallClockTime", "Iwd", "Out", "Err", "UserLog"]
    try:
        job = schedd.query(constraint=constraint, projection=projection, limit=1)
    except IndexError:
        raise RuntimeError(f"No job found for ID {job_id}.")
    except Exception as e:
        raise RuntimeError(f"Error looking up job status: {str(e)}")

    # Check the history if no jobs found
    if len(job) == 0 and not options["skip_history"]:
        try:
            logger.info(f"Job {job_id} was not found in the active queue, checking history...")
            logger.info("(This may take a while, Ctrl-C to cancel.)")
            job = list(schedd.history(constraint=constraint, projection=projection, match=1))
        except KeyboardInterrupt:
            logger.warning("History check cancelled.")
            job = []
    return job

class Submit(Verb):
    """
    Submits a job when given a submit file
    """

    options = {
        "submit_file": {
            "args": ("submit_file",),
            "help": "Submit file",
        },
        "resource": {
            "args": ("--resource",),
            "help": "Resource to run this job. Supports Slurm and EC2",
        },
        "runtime": {
            "args": ("--runtime",),
            "help": "Runtime for the given resource (seconds)",
            "type": int,
        },
        "email": {
            "args": ("--email",),
            "help": "Email address to receive notifications",
        },
        "annex_name": {
            "args": ("--annex-name",),
            "help": "Annex name that this job must run on",
        },
    }

    def __init__(self, logger, submit_file, **options):
        # Make sure the specified submit file exists and is readable
        submit_file = Path(submit_file)
        if not submit_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(submit_file)}")
        if os.access(submit_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(submit_file)}")

        # Get schedd
        schedd = htcondor.Schedd()

        # If no resource specified, submit job to the local schedd
        options["resource"] = options["resource"] or "htcondor"
        if options["resource"].casefold() == "htcondor":

            with submit_file.open() as f:
                submit_data = f.read()
            submit_description = htcondor.Submit(submit_data)
            # Set s_method to HTC_JOB_SUBMIT
            submit_description.setSubmitMethod(JSM_HTC_JOB_SUBMIT, True)

            annex_name = options["annex_name"]
            if annex_name is not None:
                if htcondor.param.get("HPC_ANNEX_ENABLED", False):
                    submit_description["MY.TargetAnnexName"] = f'"{annex_name}"'
                else:
                    raise ValueError("HPC Annex functionality has not been enabled by your HTCondor administrator.")

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                raise ValueError("Can only submit one job at a time")

            try:
                result = schedd.submit(submit_description, count=1)
                cluster_id = result.cluster()
                if annex_name is None:
                    logger.info(f"Job {cluster_id} was submitted.")
                else:
                    logger.info(f"Job {cluster_id} was submitted and will only run on the annex named '{annex_name}'.")
            except Exception as e:
                raise RuntimeError(f"Error submitting job:\n{str(e)}")

        elif options["resource"].casefold() == "slurm":

            if options["runtime"] is None:
                raise TypeError("Slurm resources must specify a runtime argument")

            # Check if bosco is setup to provide Slurm access
            is_bosco_setup = False
            try:
                check_bosco_cmd = "condor_remote_cluster --status hpclogin1.chtc.wisc.edu"
                logger.debug(f"Attempting to run: {check_bosco_cmd}")
                subprocess.check_output(shlex.split(check_bosco_cmd))
                is_bosco_setup = True
            except (FileNotFoundError, subprocess.CalledProcessError):
                logger.info(f"Your CHTC Slurm account needs to be configured before you can run jobs on it.")
            except Exception as e:
                raise RuntimeError(f"Could not execute {check_bosco_cmd}:\n{str(e)}")

            if is_bosco_setup is False:
                try:
                    logger.info(f"Please enter your Slurm account password when prompted.")
                    setup_bosco_cmd = "condor_remote_cluster --add hpclogin1.chtc.wisc.edu slurm"
                    subprocess.check_output(shlex.split(setup_bosco_cmd), timeout=60)
                except Exception as e:
                    logger.info(f"Failed to configure Slurm account access.")
                    logger.info(f"If you do not already have a CHTC Slurm account, please request this at htcondor-inf@cs.wisc.edu")
                    logger.info(f"\nYou can also try to configure your account manually using the following command:")
                    logger.info(f"\ncondor_remote_cluster --add hpclogin1.chtc.wisc.edu slurm\n")
                    raise RuntimeError(f"Unable to setup Slurm execution environment")

            Path(TMP_DIR).mkdir(parents=True, exist_ok=True)
            DAGMan.write_slurm_dag(submit_file, options["runtime"], options["email"])
            os.chdir(TMP_DIR)  # DAG must be submitted from TMP_DIR
            submit_description = htcondor.Submit.from_dag(str(TMP_DIR / "slurm_submit.dag"))
            submit_description["+ResourceType"] = "\"Slurm\""

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                raise ValueError("Can only submit one job at a time. "
                                 "See the job-set syntax for submitting multiple jobs.")

            try:
                result = schedd.submit(submit_description, count=1)
                cluster_id = result.cluster()
                logger.info(f"Job {cluster_id} was submitted.")
            except Exception as e:
                raise RuntimeError(f"Error submitting job:\n{str(e)}")

        elif options["resource"] == "ec2":

            if options["runtime"] is None:
                raise TypeError("Error: EC2 resources must specify a --runtime argument")

            Path(TMP_DIR).mkdir(parents=True, exist_ok=True)
            DAGMan.write_ec2_dag(file, options["runtime"], options["email"])
            os.chdir(TMP_DIR)  # DAG must be submitted from TMP_DIR
            submit_description = htcondor.Submit.from_dag("ec2_submit.dag")
            submit_description["+ResourceType"] = "\"EC2\""

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                raise ValueError("Can only submit one job at a time. "
                                 "See the job-set syntax for submitting multiple jobs.")

            try:
                result = schedd.submit(submit_description, count=1)
                cluster_id = result.cluster()
                logger.info(f"Job {cluster_id} was submitted.")
            except Exception as e:
                raise RuntimeError(f"Error submitting job:\n{str(e)}")


class Log(Verb):
    """
    Shows the eventlog  of a job when given a job id
    """

    options = {
        "job_id": {
            "args": ("job_id",),
            "help": "Job ID",
        },
        "skip_history": {
            "args": ("--skip-history",),
            "action": "store_true",
            "default": False,
            "help": "Skip checking history for log file",
        },
    }

    def __init__(self, logger, job_id, **options):
        job = None

        job = get_job_ad(logger, job_id, options)
        log_file = job[0].get("UserLog","")
        with open(log_file, 'r') as f:
            print(f.read())

class Out(Verb):
    """
    Shows the stdout  of a job when given a job id
    """

    options = {
        "job_id": {
            "args": ("job_id",),
            "help": "Job ID",
        },
        "skip_history": {
            "args": ("--skip-history",),
            "action": "store_true",
            "default": False,
            "help": "Skip checking history for stdout",
        },
    }

    def __init__(self, logger, job_id, **options):
        job = None

        job = get_job_ad(logger, job_id, options)
        out_file = job[0].get("Out","")
        iwd      = job[0].get("Iwd","")

        if (out_file[0] != '/'):
            out_file = iwd + "/" + out_file
        with open(out_file, 'r') as f:
            print(f.read())

class Err(Verb):
    """
    Shows the stderr of a job when given a job id
    """

    options = {
        "job_id": {
            "args": ("job_id",),
            "help": "Job ID",
        },
        "skip_history": {
            "args": ("--skip-history",),
            "action": "store_true",
            "default": False,
            "help": "Skip checking history for stderr",
        },
    }

    def __init__(self, logger, job_id, **options):
        job = None

        job = get_job_ad(logger, job_id, options)
        err_file = job[0].get("Err","")
        iwd      = job[0].get("Iwd","")

        if (err_file[0] != '/'):
            err_file = iwd + "/" + err_file
        with open(err_file, 'r') as f:
            print(f.read())

class Status(Verb):
    """
    Shows current status of a job when given a job id
    """

    options = {
        "job_id": {
            "args": ("job_id",),
            "help": "Job ID",
        },
        "skip_history": {
            "args": ("--skip-history",),
            "action": "store_true",
            "default": False,
            "help": "Skip checking history for completed or removed jobs",
        },
    }

    def __init__(self, logger, job_id, **options):
        job = None
        job_status = "IDLE"
        resource_type = "htcondor"

        job = get_job_ad(logger, job_id, options)

        if len(job) == 0:
            raise RuntimeError(f"No job found for ID {job_id}.")

        resource_type = job[0].get("ResourceType", "htcondor") or "htcondor"
        resource_type = resource_type.casefold()

        # Now, produce job status based on the resource type
        if resource_type == "htcondor":

            job_ad = job[0]

            try:
                job_status = job_ad["JobStatus"]
            except IndexError as err:
                logger.error(f"Job {job_id} has unknown status")
                raise err

            # Get annex info
            annex_info = ""
            target_annex_name = job_ad.get('TargetAnnexName')
            if target_annex_name is not None:
                annex_info = f" on the annex named '{target_annex_name}'"

            # Get some common numbers
            job_queue_time = datetime.now() - datetime.fromtimestamp(job_ad["QDate"])
            job_atts = job_ad.get("NumShadowStarts", 0)
            job_execs = job_ad.get("NumJobStarts", 0)
            job_holds = job_ad.get("NumHolds", 0)
            job_suspends = job_ad.get("TotalSuspensions", 0)
            # Calculate job goodput
            job_committed_time = job_ad.get("CommittedTime", 0)
            job_wall_clock_time = job_ad.get("RemoteWallClockTime", 0)
            goodput = (job_committed_time / job_wall_clock_time) if job_wall_clock_time > 0 else 0.0

            # Compute memory and disk usage if available
            memory_usage, disk_usage = None, None
            if "MemoryUsage" in job_ad:
                memory_usage = (
                    f"{readable_size(job_ad.eval('MemoryUsage')*10**6)} out of "
                    f"{readable_size(job_ad.eval('RequestMemory')*10**6)} requested"
                )
            if "DiskUsage" in job_ad:
                disk_usage = (
                    f"{readable_size(job_ad.eval('DiskUsage')*10**3)} out of "
                    f"{readable_size(job_ad.eval('RequestDisk')*10**3)} requested"
                )

            # Print information relevant to each job status
            if job_status == htcondor.JobStatus.IDLE:
                logger.info(f"Job {job_id} is currently idle.")
                logger.info(f"It was submitted {readable_time(job_queue_time.seconds)} ago.")
                logger.info(f"It requested {readable_size(job_ad.eval('RequestMemory')*10**6)} of memory.")
                logger.info(f"It requested {readable_size(job_ad.eval('RequestDisk')*10**3)} of disk space.")
                if job_holds > 0:
                    logger.info(f"It has been held {job_holds} time{s(job_holds)}.")
                if job_atts > 0:
                    logger.info(f"HTCondor has attempted to start the job {job_atts} time{s(job_atts)}.")
                    logger.info(f"The job has started {job_execs} time{s(job_execs)}.")

            elif job_status == htcondor.JobStatus.RUNNING:
                job_running_time = datetime.now() - datetime.fromtimestamp(job_ad["JobStartDate"])
                logger.info(f"Job {job_id} is currently running{annex_info}.")
                logger.info(f"It started running {readable_time(job_running_time.seconds)} ago.")
                logger.info(f"It was submitted {readable_time(job_queue_time.seconds)} ago.")
                if memory_usage:
                    logger.info(f"Its current memory usage is {memory_usage}.")
                if disk_usage:
                    logger.info(f"Its current disk usage is {disk_usage}.")
                if job_suspends > 0:
                    logger.info(f"It has been suspended {job_suspends} time{s(job_suspends)}.")
                if job_holds > 0:
                    logger.info(f"It has been held {job_holds} time{s(job_holds)}.")
                if job_atts > 1:
                    logger.info(f"HTCondor has attempted to start the job {job_atts} time{s(job_atts)}.")
                    logger.info(f"The job has started {job_execs} time{s(job_execs)}.")

            elif job_status == htcondor.JobStatus.SUSPENDED:
                job_suspended_time = datetime.now() - datetime.fromtimestamp(job_ad["EnteredCurrentStatus"])
                logger.info(f"Job {job_id} is currently suspended{annex_info}.")
                logger.info(f"It has been suspended for {readable_time(job_suspended_time.seconds)}.")
                logger.info(f"It has been suspended {job_suspends} time{s(job_suspends)}.")
                logger.info(f"It was submitted {readable_time(job_queue_time.seconds)} ago.")
                if memory_usage:
                    logger.info(f"Its last memory usage was {memory_usage}.")
                if disk_usage:
                    logger.info(f"Its last disk usage was {disk_usage}.")

            elif job_status == htcondor.JobStatus.TRANSFERRING_OUTPUT:
                job_transfer_time = datetime.now() - datetime.fromtimestamp(job_ad["JobCurrentStartTransferOutputDate"])
                logger.info(f"Job {job_id} is currently transferring output{annex_info}.")
                logger.info(f"It started transferring output {readable_time(job_transfer_time.seconds)} ago.")
                if memory_usage:
                    logger.info(f"Its last memory usage was {memory_usage}.")
                if disk_usage:
                    logger.info(f"Its last disk usage was {disk_usage}.")

            elif job_status == htcondor.JobStatus.HELD:
                job_held_time = datetime.now() - datetime.fromtimestamp(job_ad["EnteredCurrentStatus"])
                logger.info(f"Job {job_id} is currently held.")
                logger.info(f"It has been held for {readable_time(job_held_time.seconds)}.")
                logger.info(f"""It was held because "{job_ad['HoldReason']}".""")
                logger.info(f"It has been held {job_holds} time{s(job_holds)}.")
                logger.info(f"It was submitted {readable_time(job_queue_time.seconds)} ago.")
                if job_execs >= 1:
                    if memory_usage:
                        logger.info(f"Its last memory usage was {memory_usage}.")
                    if disk_usage:
                        logger.info(f"Its last disk usage was {disk_usage}.")
                if job_atts >= 1:
                    logger.info(f"HTCondor has attempted to start the job {job_atts} time{s(job_atts)}.")
                    logger.info(f"The job has started {job_execs} time{s(job_execs)}.")

            elif job_status == htcondor.JobStatus.REMOVED:
                job_removed_time = datetime.now() - datetime.fromtimestamp(job_ad["EnteredCurrentStatus"])
                logger.info(f"Job {job_id} was removed.")
                logger.info(f"It was removed {readable_time(job_removed_time.seconds)} ago.")
                logger.info(f"It was submitted {readable_time(job_queue_time.seconds)} ago.")
                if memory_usage:
                    logger.info(f"Its last memory usage was {memory_usage}.")
                if disk_usage:
                    logger.info(f"Its last disk usage was {disk_usage}.")

            elif job_status == htcondor.JobStatus.COMPLETED:
                job_completed_time = datetime.now() - datetime.fromtimestamp(job_ad["CompletionDate"])
                logger.info(f"Job {job_id} has completed.")
                logger.info(f"It completed {readable_time(job_completed_time.seconds)} ago.")
                logger.info(f"It was submitted {readable_time(job_queue_time.seconds)} ago.")
                if memory_usage:
                    logger.info(f"Its last memory usage was {memory_usage}.")
                if disk_usage:
                    logger.info(f"Its last disk usage was {disk_usage}.")

            else:
                goodput = None
                logger.info(f"Job {job_id} is in an unknown state (JobStatus = {job_status}).")

            # Display Good put for all job states
            if goodput is not None:
                goodput = goodput * 100
                tense = "had" if job_status == htcondor.JobStatus.COMPLETED else "has"
                logger.info(
                    f"Job {tense} {goodput:.1f}% goodput for {readable_time(job_wall_clock_time)} of wall clock time."
                )

        # Jobs running on provisioned Slurm or EC2 resources need to retrieve
        # additional information from the provisioning DAGMan log
        elif resource_type in ["slurm", "ec2"]:

            # Variables specific to jobs running on Slurm clusters
            jobs_running = 0
            job_started_time = None
            provisioner_cluster_id = None
            provisioner_job_submitted_time = None
            slurm_cluster_id = None
            slurm_nodes_requested = None
            slurm_runtime = None

            dagman_dag, dagman_out, dagman_log = DAGMan.get_files(job_id)

            if dagman_dag is None:
                raise RuntimeError(f"No {resource_type} job found for ID {job_id}.")

            # Parse the .dag file to retrieve some user input values
            with open(dagman_dag, "r") as dagman_dag_file:
                for line in dagman_dag_file.readlines():
                    if "annex_runtime =" in line:
                        slurm_runtime = int(line.split("=")[1].strip())

            # Parse the DAGMan event log for useful information
            dagman_events = htcondor.JobEventLog(dagman_log)
            for event in dagman_events.events(0):
                if "LogNotes" in event.keys() and event["LogNotes"] == "DAG Node: B":
                    provisioner_cluster_id = event.cluster
                    provisioner_job_submitted_time = datetime.fromtimestamp(event.timestamp)
                    job_status = "PROVISIONING REQUEST PENDING"
                elif "LogNotes" in event.keys() and event["LogNotes"] == "DAG Node: C":
                    slurm_cluster_id = event.cluster
                elif event.cluster == slurm_cluster_id and event.type == htcondor.JobEventType.EXECUTE:
                    job_status = "RUNNING"
                    jobs_running += 1
                    if job_started_time is None:
                        job_started_time = datetime.fromtimestamp(event.timestamp)
                elif event.cluster == slurm_cluster_id and event.type == htcondor.JobEventType.JOB_TERMINATED:
                    jobs_running -= 1
                    if jobs_running == 0:
                        job_status = "COMPLETE"
                elif event.type == htcondor.JobEventType.JOB_HELD or event.type == htcondor.JobEventType.EXECUTABLE_ERROR:
                    job_status = "ERROR"

            # Calculate how long job has been in its current state
            current_time = datetime.now()
            time_diff = None
            if job_status == "PROVISIONING REQUEST PENDING":
                time_diff = current_time - provisioner_job_submitted_time
            elif job_status == "RUNNING":
                time_diff = current_time - job_started_time

            # Now that we have all the information we want, display it
            if job_status == "COMPLETED":
                logger.info("Job has completed")
            else:
                if job_status == "PROVISIONING REQUEST PENDING":
                    logger.info(f"Job is waiting for {resource_type.upper()} to provision pending request", end='')
                else:
                    info_str = f"Job is {job_status}"
                    if time_diff is not None:
                        info_str = f"{info_str} since {round(time_diff.seconds/60)}m{(time_diff.seconds%60)}s"
                    logger.info(info_str)

        else:
            raise ValueError(f"Error: The 'job status' command does not support {resource_type} resources.")


class Resources(Verb):
    """
    Shows resources used by a job when given a job id
    """

    options = {
        "job_id": {
            "args": ("job_id",),
            "help": "Job ID",
        },
        "resource": {
            "args": ("--resource",),
            "help": "Resource used by job. Required for Slurm jobs.",
        },
    }

    def __init__(self, logger, job_id, **options):
        # Get schedd
        schedd = htcondor.Schedd()

        # If no resource specified, assume job is running on local pool
        resource = options.get("resource", "htcondor") or "htcondor"
        resource = resource.casefold()
        if resource == "htcondor":
            try:
                job = schedd.query(
                    constraint=f"ClusterId == {job_id}",
                    projection=["RemoteHost", "TargetAnnexName", "MachineAttrAnnexName0"]
                )
            except IndexError:
                raise RuntimeError(f"No jobs found for ID {job_id}.")
            except:
                raise RuntimeError(f"Unable to look up job resources")
            if len(job) == 0:
                raise RuntimeError(f"No jobs found for ID {job_id}.")

            # TODO: Make this work correctly for jobs that haven't started running yet
            try:
                job_host = job[0]["RemoteHost"]
            except KeyError:
                raise RuntimeError(f"Job {job_id} is not running yet.")

            target_annex = job[0].get("TargetAnnexName", None)
            machine_annex = job[0].get("MachineAttrAnnexName0", None)
            if target_annex is not None and machine_annex is not None and target_annex == machine_annex:
                pretty_job_host = job_host.split('@')[1]
                logger.info(f"Job is using annex '{target_annex}', node {pretty_job_host}.")
            else:
                logger.info(f"Job is using resource {job_host}")

        # Jobs running on provisioned Slurm resources need to retrieve
        # additional information from the provisioning DAGMan log
        elif resource == "slurm":

            # Internal variables
            dagman_cluster_id = None
            provisioner_cluster_id = None
            slurm_cluster_id = None

            # User-facing variables (all values set below are default/initial state)
            provisioner_job_submitted_time = None
            provisioner_job_scheduled_end_time = None
            job_status = "NOT SUBMITTED"
            job_started_time = None
            jobs_running = 0
            slurm_nodes_requested = None
            slurm_runtime = None

            dagman_dag, dagman_out, dagman_log = DAGMan.get_files(id)

            if dagman_dag is None:
                raise RuntimeError(f"No Slurm job found for ID {job_id}.")

            # Parse the .dag file to retrieve some user input values
            with open(dagman_dag, "r") as dagman_dag_file:
                for line in dagman_dag_file.readlines():
                    if "annex_runtime =" in line:
                        slurm_runtime = int(line.split("=")[1].strip())

            # Parse the DAGMan event log for useful information
            dagman_events = htcondor.JobEventLog(dagman_log)
            for event in dagman_events.events(0):
                if "LogNotes" in event.keys() and event["LogNotes"] == "DAG Node: B":
                    provisioner_cluster_id = event.cluster
                    provisioner_job_submitted_time = datetime.fromtimestamp(event.timestamp)
                    provisioner_job_scheduled_end_time = datetime.fromtimestamp(event.timestamp + slurm_runtime)
                    job_status = "PROVISIONING REQUEST PENDING"
                if event.cluster == provisioner_cluster_id and event.type == htcondor.JobEventType.EXECUTE:
                    provisioner_job_started_time = datetime.fromtimestamp(event.timestamp)
                    provisioner_job_scheduled_end_time = datetime.fromtimestamp(event.timestamp + slurm_runtime)
                if "LogNotes" in event.keys() and event["LogNotes"] == "DAG Node: C":
                    slurm_cluster_id = event.cluster
                    job_started_time = datetime.fromtimestamp(event.timestamp)
                if event.cluster == slurm_cluster_id and event.type == htcondor.JobEventType.EXECUTE:
                    job_status = "RUNNING"
                    jobs_running += 1
                if event.cluster == slurm_cluster_id and (event.type == htcondor.JobEventType.JOB_TERMINATED or event.type == htcondor.JobEventType.JOB_EVICTED):
                    jobs_running -= 1
                    if jobs_running == 0:
                        job_status = "COMPLETE"
                if event.type == htcondor.JobEventType.JOB_HELD or event.type == htcondor.JobEventType.EXECUTABLE_ERROR:
                    job_status = "ERROR"

            # Now that we have all the information we want, display it
            if job_status == "PROVISIONING REQUEST PENDING":
                logger.info(f"Job is still waiting for {slurm_nodes_requested} Slurm nodes to provision")
            elif job_status == "RUNNING":
                logger.info(f"Job is running on {jobs_running}/{slurm_nodes_requested} requested Slurm nodes")
            elif job_status == "ERROR":
                logger.info(f"An error occurred provisioning Slurm resources")

            # Show information about time remaining
            if job_status == "RUNNING" or job_status == "COMPLETE":
                current_time = datetime.now()
                if current_time < provisioner_job_scheduled_end_time:
                    time_diff = provisioner_job_scheduled_end_time - current_time
                    logger.info(f"Slurm resources are reserved for another {round(time_diff.seconds/60)}m{(time_diff.seconds%60)}s")
                else:
                    time_diff = current_time - provisioner_job_scheduled_end_time
                    logger.info(f"Slurm resources were terminated since {round(time_diff.seconds/60)}m{(time_diff.seconds%60)}s")

def load_templates() -> list:
    """
    Load templates from a directory.
    """
    try:
        template_dir = htcondor.param.get("JOB_TEMPLATE_DIR", "")
        templates    = [f for f in os.listdir(template_dir) if os.path.isfile(os.path.join(template_dir, f))]
        return templates
    except FileNotFoundError:
        return []

class Template(Verb):
    """
    Displays and copies template submit files
    """

    options = {
        "template_file": {
            "args": ("template_file",),
            "nargs": "*",
            "help": "Name of template to use",
            "default": "",
        },
    }

    def __init__(self, logger, template_file, **options):
        templates = load_templates()
        if template_file:
            if (not template_file[0] in templates):
                raise RuntimeError(f"No template found named {template_file}.")
            logger.info(f"Creating submit file from template: {template_file[0]}")
            shutil.copyfile(htcondor.param.get("JOB_TEMPLATE_DIR", "") + "/" + template_file[0], "./" + template_file[0])
        else:
            if templates:
                logger.info("Available templates:")
                for template in templates:
                   logger.info(f"Name: {template}")
            else:
               logger.info("No templates available.")
        return

class Job(Noun):
    """
    Run operations on HTCondor jobs
    """

    class submit(Submit):
        pass

    class status(Status):
        pass

    class out(Out):
        pass

    class err(Err):
        pass

    class log(Log):
        pass

    class resources(Resources):
        pass

    class template(Template):
        pass

    @classmethod
    def verbs(cls):
        return [cls.submit, cls.status, cls.out, cls.err, cls.log, cls.resources, cls.template]
