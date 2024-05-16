import sys
import os
import logging
import subprocess
import shlex
import tempfile
import time
import getpass
import typer


from datetime import datetime
from pathlib import Path
from typing import List, Optional

import htcondor

from htcondor_cli.dagman import DAGMan
from htcondor_cli import TMP_DIR
from htcondor_cli.utils import readable_time, readable_size, s

job_app = typer.Typer(no_args_is_help=True)

JSM_HTC_JOB_SUBMIT = 3

def get_logger(name=__name__, level=logging.INFO, fmt="%(message)s"):
    logger = logging.getLogger(name)
    if not logger.hasHandlers():
        printHandler = logging.StreamHandler()
        printHandler.setLevel(level)
        printHandler.setFormatter(logging.Formatter(fmt))
        logger.addHandler(printHandler)
    logger.setLevel(level)
    return logger

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

def complete_submit_files(incomplete: str) -> List[str]:
    """
    Autocompletion function that returns .sub files matching the incomplete input.
    """
    # List all files in the current directory (or a specific path)
    files = os.listdir('.')
    # Filter files to get those ending with .log
    submit_files = [f for f in files if f.endswith('.sub')]
    # Return files that match the incomplete input
    return [f for f in submit_files if f.startswith(incomplete)]

def convert_to_dict(job_data):
    if not job_data or not isinstance(job_data, list) or not job_data[0]:
        return {}

    # Extract the string from the list
    job_info = job_data[0]
    
    # Prepare to store the dictionary
    job_dict = {}
    
    # Split the string into key-value pairs
    attributes = job_info.split("; ")
    for attribute in attributes:
        # Split each attribute at the first occurrence of '=' to handle values that may contain '='
        key_val = attribute.split(" = ", 1)
        if len(key_val) == 2:
            key, val = key_val
            # Remove any leading and trailing whitespace or quotes
            val = val.strip('"')
            job_dict[key.strip()] = val

    return job_dict

@job_app.command(no_args_is_help=True)
def submit(
    submit_file: Path = typer.Argument(..., autocompletion=complete_submit_files,exists=True, readable=True, help="Path to the submit file"),
    resource: str = typer.Option("htcondor", "--resource", help="Resource to run this job. Supports Slurm and EC2"),
    runtime: int = typer.Option(None, "--runtime", help="Runtime for the given resource (seconds)"),
    email: str = typer.Option(None, "--email", help="Email address to receive notifications"),
    annex_name: str = typer.Option(None, "--annex-name", help="Annex name that this job must run on")
):
    """
    Submits a job when given a submit file
    """
    logger = get_logger(__name__)
    options = {"resource": resource, "runtime": runtime, "email": email, "annex_name": annex_name}
    try:
        schedd = htcondor.Schedd()

        if resource.casefold() == "htcondor":
            with submit_file.open() as f:
                submit_data = f.read()
            submit_description = htcondor.Submit(submit_data)
            if annex_name:
                if htcondor.param.get("HPC_ANNEX_ENABLED", False):
                    submit_description["MY.TargetAnnexName"] = f'"{annex_name}"'
                else:
                    raise ValueError("HPC Annex functionality has not been enabled by your HTCondor administrator.")

            result = schedd.submit(submit_description, count=1)
            cluster_id = result.cluster()
            logger.info(f"Job {cluster_id} was submitted to HTCondor.")

        elif resource.casefold() == "slurm":
            handle_slurm_submission(submit_file, runtime, email, logger)  # Function to handle Slurm specifics

        elif resource.casefold() == "ec2":
            handle_ec2_submission(submit_file, runtime, email, logger)  # Function to handle EC2 specifics

    except Exception as e:
        logger.error(f"Error submitting job: {str(e)}")
        raise typer.Exit(code=1)

def handle_slurm_submission(submit_file, runtime, email, logger):
    if runtime is None:
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



@job_app.command(no_args_is_help=True)
def status(
    job_id: str = typer.Argument(..., help="Job ID"),
    skip_history: bool = typer.Option(False, "--skip-history", help="Skip checking history for completed or removed jobs")
):
    """
    Shows current status of a job when given a job id
    """
    logger = get_logger(__name__)
    options = {"skip_history": skip_history}

    try:
        job = get_job_ad(logger, job_id, options)

        if not job:
            logger.error(f"No job found for ID {job_id}.")
            raise typer.Exit(code=1)
        # job = convert_to_dict(job)
        typer.echo(type(job))
        typer.echo(job)
        resource_type = job[0].get("ResourceType", "htcondor").casefold() or "htcondor"
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
            logger.error(f"Unsupported resource type: {resource_type}")
            raise typer.Exit(code=1)

    except Exception as e:
        logger.error(f"Error retrieving status: {str(e)}")
        raise typer.Exit(code=1)

@job_app.command(no_args_is_help=True)
def Out():
    """
    Shows the stdout  of a job when given a job id
    """
    typer.echo("Needs to be implemented")

@job_app.command(no_args_is_help=True)
def Err():
    """
    Shows the stderr of a job when given a job id
    """
    typer.echo("Needs to be implemented")

@job_app.command(no_args_is_help=True)
def resource():
    """
    Shows resources used by a job when given a job id
    """
    typer.echo("Needs to be implemented")