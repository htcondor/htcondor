import sys
import os
import logging
import subprocess
import shlex
import tempfile
import time
import getpass


from datetime import datetime
from pathlib import Path

import htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli.dagman import DAGMan
from htcondor_cli import JobStatus
from htcondor_cli import TMP_DIR

JSM_HTC_JOB_SUBMIT = 3

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
	    #Set s_method to HTC_JOB_SUBMIT
            submit_description.setSubmitMethod(JSM_HTC_JOB_SUBMIT,True)

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

            with schedd.transaction() as txn:
                try:
                    cluster_id = submit_description.queue(txn, 1)
                    if annex_name is None:
                        logger.info(f"Job {cluster_id} was submitted.")
                    else:
                        logger.info(f"Job {cluster_id} was submitted and will only run on the annex '{annex_name}'.")
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
            os.chdir(TMP_DIR) # DAG must be submitted from TMP_DIR
            submit_description = htcondor.Submit.from_dag(str(TMP_DIR / "slurm_submit.dag"))
            submit_description["+ResourceType"] = "\"Slurm\""

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                raise ValueError("Can only submit one job at a time. "
                    "See the job-set syntax for submitting multiple jobs.")

            with schedd.transaction() as txn:
                try:
                    cluster_id = submit_description.queue(txn, 1)
                    logger.info(f"Job {cluster_id} was submitted.")
                except Exception as e:
                    raise RuntimeError(f"Error submitting job:\n{str(e)}")

        elif options["resource"] == "ec2":

            if options["runtime"] is None:
                raise TypeError("Error: EC2 resources must specify a --runtime argument")

            Path(TMP_DIR).mkdir(parents=True, exist_ok=True)
            DAGMan.write_ec2_dag(file, options["runtime"], options["email"])
            os.chdir(TMP_DIR) # DAG must be submitted from TMP_DIR
            submit_description = htcondor.Submit.from_dag("ec2_submit.dag")
            submit_description["+ResourceType"] = "\"EC2\""

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                raise ValueError("Can only submit one job at a time. "
                    "See the job-set syntax for submitting multiple jobs.")

            with schedd.transaction() as txn:
                try:
                    cluster_id = submit_description.queue(txn, 1)
                    logger.info(f"Job {cluster_id} was submitted.")
                except Exception as e:
                    raise RuntimeError(f"Error submitting job:\n{str(e)}")

class Status(Verb):
    """
    Shows current status of a job when given a job id
    """

    options = {
        "job_id": {
            "args": ("job_id",),
            "help": "Job ID",
        },
    }


    def __init__(self, logger, job_id, **options):
        job = None
        job_status = "IDLE"
        resource_type = "htcondor"

        # Get schedd
        schedd = htcondor.Schedd()

        # Query schedd
        try:
            job = schedd.query(
                constraint=f"ClusterId == {job_id}",
                projection=["JobStartDate", "JobStatus", "EnteredCurrentStatus", "HoldReason", "ResourceType", "TargetAnnexName"]
            )
        except IndexError:
            raise RuntimeError(f"No job found for ID {job_id}.")
        except Exception as e:
            raise RuntimeError(f"Error looking up job status: {str(e)}")
        if len(job) == 0:
            raise RuntimeError(f"No job found for ID {job_id}.")

        resource_type = job[0].get("ResourceType", "htcondor") or "htcondor"
        resource_type = resource_type.casefold()

        # Now, produce job status based on the resource type
        if resource_type == "htcondor":
            annex_info = ""
            target_annex_name = job[0].get('TargetAnnexName')
            if target_annex_name is not None:
                annex_info = " on the annex named '{target_annex_name}' "

            if JobStatus[job[0]['JobStatus']] == "RUNNING":
                job_running_time = datetime.now() - datetime.fromtimestamp(job[0]["JobStartDate"])
                logger.info(f"Job has been running {annex_info}for {round(job_running_time.seconds/3600)} hour(s), {round(job_running_time.seconds/60)} minute(s), and {(job_running_time.seconds%60)} second(s).")
            elif JobStatus[job[0]['JobStatus']] == "HELD":
                job_held_time = datetime.now() - datetime.fromtimestamp(job[0]["EnteredCurrentStatus"])
                logger.info(f"Job has been held for {round(job_held_time.seconds/3600)} hour(s), {round(job_held_time.seconds/60)} minute(s), and {(job_held_time.seconds%60)} second(s).\nHold Reason: {job[0]['HoldReason']}")
            elif JobStatus[job[0]['JobStatus']] == "COMPLETED":
                logger.info("Job completed.")
            else:
                logger.info(f"Job is {JobStatus[job[0]['JobStatus']]}.")

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
                    projection=["RemoteHost","TargetAnnexName","MachineAttrAnnexName0"]
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



class Job(Noun):
    """
    Run operations on HTCondor jobs
    """

    class submit(Submit):
        pass

    class status(Status):
        pass

    class resources(Resources):
        pass

    @classmethod
    def verbs(cls):
        return [cls.submit, cls.status, cls.resources]

