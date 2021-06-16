import htcondor
import os
import shutil
import sys

from datetime import datetime, timedelta

from .conf import *
from .dagman import DAGMan

JobStatus = [
    "NONE",
    "IDLE",
    "RUNNING",
    "REMOVED",
    "COMPLETED",
    "HELD",
    "TRANSFERRING_OUTPUT",
    "SUSPENDED",
    "JOB_STATUS_MAX"
]

schedd = htcondor.Schedd()

class Job:
    """
    A :class:`Job` holds all operations related to HTCondor jobs
    """

    @staticmethod
    def submit(file, options=None):

        # Make sure the specified submit file exists and is readable!
        if os.access(file, os.R_OK) is False:
            print(f"Error: could not read file {file}")
            sys.exit(1)

        # If no resource specified, submit job to the local schedd
        if "resource" not in options:

            submit_file = open(file)
            submit_data = submit_file.read()
            submit_file.close()
            submit_description = htcondor.Submit(submit_data)

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                print("Error: can only submit one job at a time")
                sys.exit(1)

            with schedd.transaction() as txn:
                try:
                    cluster_id = submit_description.queue(txn, 1)
                    print(f"Job {cluster_id} was submitted.")
                except Exception as error:
                    print(f"Error submitting job: {error}")
                    sys.exit(1)

        elif options["resource"] == "slurm":

            if "runtime" not in options:
                print("Error: Slurm resources must specify a --runtime argument")
                sys.exit(1)
            if "node_count" not in options:
                print("Error: Slurm resources must specify a --node_count argument")
                sys.exit(1)

            os.mkdir(TMP_DIR)
            DAGMan.write_slurm_dag(file, options["runtime"], options["node_count"], options["email"])
            os.chdir(TMP_DIR) # DAG must be submitted from TMP_DIR
            submit_description = htcondor.Submit.from_dag(str(TMP_DIR / "slurm_submit.dag"))
            submit_description["+ResourceType"] = "\"Slurm\""
            
            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                print("Error: can only submit one job at a time. See the job-set syntax for submitting multiple jobs.")
                sys.exit(1)

            with schedd.transaction() as txn:
                try:
                    cluster_id = submit_description.queue(txn, 1)
                    print(f"Job {cluster_id} was submitted.")
                except Exception as error:
                    print(f"Error submitting job: f{error}")
                    sys.exit(1)

        elif options["resource"] == "ec2":

            if "runtime" not in options:
                print("Error: EC2 resources must specify a --runtime argument")
                sys.exit(1)
            if "node_count" not in options:
                print("Error: EC2 resources must specify a --node_count argument")
                sys.exit(1)

            os.mkdir(TMP_DIR)
            DAGMan.write_ec2_dag(file, options["runtime"], options["node_count"], options["email"])
            os.chdir(TMP_DIR) # DAG must be submitted from TMP_DIR
            submit_description = htcondor.Submit.from_dag("ec2_submit.dag")
            submit_description["+ResourceType"] = "\"EC2\""

            # The Job class can only submit a single job at a time
            submit_qargs = submit_description.getQArgs()
            if submit_qargs != "" and submit_qargs != "1":
                print("Error: can only submit one job at a time. See the job-set syntax for submitting multiple jobs.")
                sys.exit(1)

            with schedd.transaction() as txn:
                try:
                    cluster_id = submit_description.queue(txn, 1)
                    print(f"Job {cluster_id} was submitted.")
                except Exception as error:
                    print(f"Error submitting job: f{error}")
                    sys.exit(1)


    @staticmethod
    def status(id, options=None):
        """
        Displays the status of a job
        """

        job = None
        job_status = "IDLE"
        resource_type = "htcondor"

        try:
            job = schedd.query(
                constraint=f"ClusterId == {id}",
                projection=["JobStartDate", "JobStatus", "LastVacateTime", "ResourceType"]
            )
        except IndexError:
            print(f"No job found for ID {id}.")
            sys.exit(0)
        except:
            print(f"Error looking up job status: {sys.exc_info()[0]}")
            sys.exit(1)

        if len(job) == 0:
            print(f"No job found for ID {id}.")
            sys.exit(0)
            
        if "ResourceType" in job[0]:
            resource_type = job[0]["ResourceType"].lower()
        
        # Now, produce job status based on the resource type
        if resource_type == "htcondor":
            if JobStatus[job[0]['JobStatus']] is "RUNNING":
                job_running_time = datetime.now() - datetime.fromtimestamp(job[0]["JobStartDate"])
                print(f"Job is {JobStatus[job[0]['JobStatus']]} since {round(job_running_time.seconds/3600)}h{round(job_running_time.seconds/60)}m{(job_running_time.seconds%60)}s")
            elif JobStatus[job[0]['JobStatus']] is "HELD":
                job_held_time = datetime.now() - datetime.fromtimestamp(job[0]["LastVacateTime"])
                print(f"Job is {JobStatus[job[0]['JobStatus']]} since {round(job_held_time.seconds/3600)}h{round(job_held_time.seconds/60)}m{(job_held_time.seconds%60)}s")
            else:
                print(f"Job is {JobStatus[job[0]['JobStatus']]}")

        # Jobs running on provisioned Slurm resources need to retrieve
        # additional information from the provisioning DAGMan log
        elif resource_type == "slurm" or resource_type == "ec2":

            # Variables specific to jobs running on Slurm clusters
            jobs_running = 0
            job_started_time = None
            provisioner_cluster_id = None
            provisioner_job_submitted_time = None
            slurm_cluster_id = None
            slurm_nodes_requested = None
            slurm_runtime = None

            dagman_dag, dagman_out, dagman_log = DAGMan.get_files(id)

            if dagman_dag is None:
                print(f"No {resource_type} job found for ID {id}.")
                sys.exit(0)

            # Parse the .dag file to retrieve some user input values
            dagman_dag_file = open(TMP_DIR / dagman_dag, "r")
            for line in dagman_dag_file.readlines():
                if "annex_node_count =" in line:
                    slurm_nodes_requested = line.split("=")[1].strip()
                if "annex_runtime =" in line:
                    slurm_runtime = int(line.split("=")[1].strip())
            
            # Parse the DAGMan event log for useful information
            dagman_events = htcondor.JobEventLog(str(TMP_DIR / dagman_log))
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

            # Now that we have all the information we want, display it
            current_time = datetime.now()
            time_diff = None
            if job_status is "PROVISIONING REQUEST PENDING":
                time_diff = current_time - provisioner_job_submitted_time
            elif job_status is "RUNNING":
                time_diff = current_time - job_started_time

            print(f"Job is {job_status}", end='')
            if time_diff is not None:
                print(f" since {round(time_diff.seconds/60)}m{(time_diff.seconds%60)}s")
            else:
                print("")

        else:
            print(f"Error: The 'job status' command does not support {resource_type} resources.")
            sys.exit(1)


    @staticmethod
    def resources(id, options=None):
        """
        Displays the resources used by a specified job
        """

        # If no resource specified, assume job is running on local pool
        if "resource" not in options:
            try:
                job = schedd.query(
                    constraint=f"ClusterId == {id}",
                    projection=["RemoteHost"]
                )
            except IndexError:
                print(f"No jobs found for ID {id}.")
                sys.exit(0)
            except:
                print(f"Unable to look up job resources")
                sys.exit(1)
                
            if len(job) == 0:
                print(f"No jobs found for ID {id}.")
                sys.exit(0)
            
            # TODO: Make this work correctly for jobs that havne't started running yet 
            job_host = job[0]["RemoteHost"]
            print(f"Job is using resource {job_host}")

        # Jobs running on provisioned Slurm resources need to retrieve
        # additional information from the provisioning DAGMan log
        elif options["resource"] == "slurm":

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
                print(f"No Slurm job found for ID {id}.")
                sys.exit(0)

            # Parse the .dag file to retrieve some user input values
            dagman_dag_file = open(dagman_dag, "r")
            for line in dagman_dag_file.readlines():
                if "annex_node_count =" in line:
                    slurm_nodes_requested = line.split("=")[1].strip()
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
            if job_status is "PROVISIONING REQUEST PENDING":
                print(f"Job is still waiting for {slurm_nodes_requested} Slurm nodes to provision")
            elif job_status is "RUNNING":
                print(f"Job is running on {jobs_running}/{slurm_nodes_requested} requested Slurm nodes")
            elif job_status is "ERROR":
                print(f"An error occurred provisioning Slurm resources")

            # Show information about time remaining
            if job_status is "RUNNING" or job_status is "COMPLETE":
                current_time = datetime.now()
                if current_time < provisioner_job_scheduled_end_time:
                    time_diff = provisioner_job_scheduled_end_time - current_time
                    print(f"Slurm resources are reserved for another {round(time_diff.seconds/60)}m{(time_diff.seconds%60)}s")
                else:
                    time_diff = current_time - provisioner_job_scheduled_end_time
                    print(f"Slurm resources were terminated since {round(time_diff.seconds/60)}m{(time_diff.seconds%60)}s")
