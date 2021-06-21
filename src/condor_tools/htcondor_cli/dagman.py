import getpass
import htcondor
import os
import sys
import stat

from .conf import *

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
    
        dag, log, out = None, None, None

        env = schedd.query(
            constraint=f"ClusterId == {dagman_id}",
            projection=["Env"],
        )

        if env:
            env = dict(item.split("=", 1) for item in env[0]["Env"].split(";"))
            out = env["_CONDOR_DAGMAN_LOG"]
            log = out.replace(".dagman.out", ".nodes.log")
            dag = out.replace(".dagman.out", "")

        return dag, out, log

    @staticmethod
    def write_slurm_dag(jobfile, runtime, node_count, email):

        sendmail_sh = "#!/bin/sh\n"
        if email is not None:
            sendmail_sh += f"\necho -e '$2' | mail -v -s '$1' {email}\n"

        sendmail_sh_file = open(TMP_DIR / "sendmail.sh", "w")
        sendmail_sh_file.write(sendmail_sh)
        sendmail_sh_file.close()
        st = os.stat(TMP_DIR / "sendmail.sh")
        os.chmod(TMP_DIR / "sendmail.sh", st.st_mode | stat.S_IEXEC)

        slurm_config = "DAGMAN_USE_CONDOR_SUBMIT = False\nDAGMAN_USE_STRICT = 0\n"
        slurm_config_file = open(TMP_DIR / "slurm_submit.config", "w")
        slurm_config_file.write(slurm_config)
        slurm_config_file.close()

        slurm_dag = f"""JOB A {{
    executable = sendmail.sh
    arguments = \\\"Job submitted to run on Slurm\\\" \\\"Your job ({jobfile}) has been submitted to run on a Slurm resource\\\"
    universe = local
    output = job-A-email.$(cluster).$(process).out
    request_disk = 10M
}}
JOB B {{
    executable = /home/jfrey/hobblein/hobblein_remote.sh
    universe = grid
    grid_resource = batch slurm hpclogin1.chtc.wisc.edu
    transfer_executable = false
    output = job-B.$(Cluster).$(Process).out
    error = job-B.$(Cluster).$(Process).err
    log = job-B.$(Cluster).$(Process).log
    annex_runtime = {runtime}
    annex_node_count = {node_count}
    annex_name = {getpass.getuser()}-test
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

        dag_file = open(TMP_DIR / "slurm_submit.dag", "w")
        dag_file.write(slurm_dag)
        dag_file.close()

    @staticmethod
    def write_ec2_dag(jobfile, runtime, node_count, email):

        sendmail_sh = "#!/bin/sh\n"
        if email is not None:
            sendmail_sh += f"\necho -e '$2' | mail -v -s '$1' {email}\n"

        sendmail_sh_file = open(TMP_DIR / "sendmail.sh", "w")
        sendmail_sh_file.write(sendmail_sh)
        sendmail_sh_file.close()
        st = os.stat(TMP_DIR / "sendmail.sh")
        os.chmod(TMP_DIR / "sendmail.sh", st.st_mode | stat.S_IEXEC)

        ec2_annex_sh = f"""#!/bin/sh

yes | /usr/bin/condor_annex -count $1 -duration $2 -annex-name EC2Annex-{int(time.time())}
"""
        ec2_annex_sh_file = open(TMP_DIR / "ec2_annex.sh", "w")
        ec2_annex_sh_file.write(ec2_annex_sh)
        ec2_annex_sh_file.close()
        st = os.stat(TMP_DIR / "ec2_annex.sh")
        os.chmod(TMP_DIR / "ec2_annex.sh", st.st_mode | stat.S_IEXEC)

        ec2_config = "DAGMAN_USE_CONDOR_SUBMIT = False\nDAGMAN_USE_STRICT = 0\n"
        ec2_config_file = open(TMP_DIR / "ec2_submit.config", "w")
        ec2_config_file.write(ec2_config)
        ec2_config_file.close()

        ec2_dag = f"""JOB A {{
    executable = sendmail.sh
    arguments = \\\"Job submitted to run on EC2\\\" \\\"Your job ({jobfile}) has been submitted to run on a EC2 resource\\\"
    universe = local
    output = job-A-email.$(cluster).$(process).out
    request_disk = 10M
}}
JOB B {{
    executable = ec2_annex.sh
    arguments = {node_count} {runtime}
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

        dag_file = open(TMP_DIR / "ec2_submit.dag", "w")
        dag_file.write(ec2_dag)
        dag_file.close()
