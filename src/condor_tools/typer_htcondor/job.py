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

@job_app.command()
def submit(
    submit_file: Path = typer.Argument(..., exists=True, readable=True, help="Path to the submit file"),
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

@job_app.command()
def status():
    typer.echo("Checking job status...")

