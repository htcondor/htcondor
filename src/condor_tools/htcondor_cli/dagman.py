import getpass
import htcondor
import os
import sys
import stat
import tempfile
import time

from datetime import datetime
from pathlib import Path

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import JobStatus
from htcondor_cli import TMP_DIR

JSM_HTC_DAG_SUBMIT = 4

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

        with schedd.transaction() as txn:
            try:
                cluster_id = submit_description.queue(txn, 1)
                logger.info(f"DAG {cluster_id} was submitted.")
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


    def __init__(self, logger, dag_id, **options):
        dag = None
        dag_status = "IDLE"

        # Get schedd
        schedd = htcondor.Schedd()

        # Query schedd
        try:
            dag = schedd.query(
                constraint=f"ClusterId == {dag_id}",
                projection=["JobStartDate", "JobStatus", "EnteredCurrentStatus", "HoldReason", "ResourceType", "DAG_NodesTotal"]
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

        # Now, produce DAG status
        if JobStatus[dag[0]['JobStatus']] == "RUNNING":
            job_running_time = datetime.now() - datetime.fromtimestamp(dag[0]["JobStartDate"])
            logger.info(f"DAG is running since {round(job_running_time.seconds/3600)}h{round(job_running_time.seconds/60)}m{(job_running_time.seconds%60)}s")
        elif JobStatus[dag[0]['JobStatus']] == "HELD":
            job_held_time = datetime.now() - datetime.fromtimestamp(dag[0]["EnteredCurrentStatus"])
            logger.info(f"DAG is held since {round(job_held_time.seconds/3600)}h{round(job_held_time.seconds/60)}m{(job_held_time.seconds%60)}s\nHold Reason: {job[0]['HoldReason']}")
        elif JobStatus[dag[0]['JobStatus']] == "COMPLETED":
            logger.info("DAG has completed")
        else:
            logger.info(f"DAG is {JobStatus[dag[0]['JobStatus']]}")

        # Show some information about the jobs running under this DAG
        if dag[0]['DAG_NodesTotal']:
            logger.info(f"Of {dag[0]['DAG_NodesTotal']} total jobs:")

            # Information about active jobs should come directly from the schedd
            active_jobs = schedd.query(
                constraint=f"DAGManJobId == {dag_id}"
            )
            running_jobs = [job for job in active_jobs if job['JobStatus'] == JobStatus.index("RUNNING")]
            idle_jobs = [job for job in active_jobs if job['JobStatus'] == JobStatus.index("IDLE")]
            held_jobs = [job for job in active_jobs if job['JobStatus'] == JobStatus.index("HELD")]
            logger.info(f"{len(running_jobs)} are currently running")
            logger.info(f"{len(idle_jobs)} are idle")
            logger.info(f"{len(held_jobs)} are held")

            # Information about completed jobs
            completed_jobs = schedd.history(
                constraint=f"DAGManJobId == {dag_id}",
                projection=["ClusterId", "JobStatus"]
            )
            successful_jobs = completed_jobs = [job for job in completed_jobs if job['JobStatus'] == JobStatus.index("COMPLETED")]
            logger.info(f"{len(successful_jobs)} completed successfully")

class DAG(Noun):
    """
    Run operations on HTCondor DAGs
    """

    class submit(Submit):
        pass

    class status(Status):
        pass

    """
    class resources(Resources):
        pass
    """

    @classmethod
    def verbs(cls):
        return [cls.submit, cls.status]


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
