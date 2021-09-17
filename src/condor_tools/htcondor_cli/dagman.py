import getpass
import htcondor
import os
import sys
import stat
import tempfile
import time

from pathlib import Path

from htcondor_cli import TMP_DIR


schedd = htcondor.Schedd()


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

        slurm_config = "DAGMAN_USE_CONDOR_SUBMIT = False\nDAGMAN_USE_STRICT = 0\n"
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

        ec2_config = "DAGMAN_USE_CONDOR_SUBMIT = False\nDAGMAN_USE_STRICT = 0\n"
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
