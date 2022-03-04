#!/usr/bin/env python3


import os
import sys
import fcntl
import atexit
import signal
import getpass
import logging
import secrets
import subprocess
from pathlib import Path

import htcondor


INITIAL_CONNECTION_TIMEOUT = int(
    htcondor.param.get("ANNEX_INITIAL_CONNECTION_TIMEOUT", 180)
)
REMOTE_CLEANUP_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_CLEANUP_TIMEOUT", 60))
REMOTE_MKDIR_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_MKDIR_TIMEOUT", 5))
REMOTE_POPULATE_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_POPULATE_TIMEOUT", 60))

# FIXME: Make this come from config? At least some place not hardcoded.
MACHINE_QUEUE_MAP = {"stampede2": "normal"}


def make_initial_ssh_connection(
    ssh_connection_sharing,
    ssh_target,
    ssh_indirect_command,
):
    proc = subprocess.Popen(
        [
            "ssh",
            "-f",
            *ssh_connection_sharing,
            ssh_target,
            *ssh_indirect_command,
            "exit",
            "0",
        ],
    )

    try:
        return proc.wait(timeout=INITIAL_CONNECTION_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise RuntimeError(
            f"Did not make initial connection after {INITIAL_CONNECTION_TIMEOUT} seconds, aborting."
        )


def remove_remote_temporary_directory(
    logger, ssh_connection_sharing, ssh_target, ssh_indirect_command, remote_script_dir
):
    if remote_script_dir is not None:
        logger.debug("Cleaning up remote temporary directory...")
        proc = subprocess.Popen(
            [
                "ssh",
                *ssh_connection_sharing,
                ssh_target,
                *ssh_indirect_command,
                "rm",
                "-fr",
                remote_script_dir,
            ],
        )

        try:
            return proc.wait(timeout=REMOTE_CLEANUP_TIMEOUT)
        except subprocess.TimeoutExpired:
            logger.error(
                f"Did not clean up remote temporary directory after {INITIAL_CONNECTION_TIMEOUT} seconds, '{remote_script_dir}' may need to be deleted manually."
            )

    return 0


def make_remote_temporary_directory(
    logger,
    ssh_connection_sharing,
    ssh_target,
    ssh_indirect_command,
):
    proc = subprocess.Popen(
        [
            "ssh",
            *ssh_connection_sharing,
            ssh_target,
            *ssh_indirect_command,
            "mktemp",
            "--tmpdir=\\${SCRATCH}",
            "--directory",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        out, err = proc.communicate(timeout=REMOTE_MKDIR_TIMEOUT)

        if proc.returncode == 0:
            return out.strip()
        else:
            logger.error("Failed to make remote temporary directory, got output:")
            logger.error(f"stdout: {out.strip()}")
            logger.error(f"stderr: {err.strip()}")
            raise IOError("Failed to make remote temporary directory.")

    except subprocess.TimeoutExpired:
        logger.error(
            f"Failed to make remote temporary directory after {REMOTE_MKDIR_TIMEOUT} seconds, aborting..."
        )
        logger.info("... cleaning up...")
        proc.kill()
        logger.debug("...")
        out, err = proc.communicate()
        logger.info("... clean up complete.")
        raise IOError(
            f"Failed to make remote temporary directory after {REMOTE_MKDIR_TIMEOUT} seconds"
        )


def transfer_sif_files(
    logger,
    ssh_connection_sharing,
    ssh_target,
    ssh_indirect_command,
    remote_script_dir,
    sif_files,
):
    # We'd prefer to avoid the possibility of .sif name collisions,
    # but for now that's too hard.  FIXME: detect it?
    # Instead, we'll rewrite ContainerImage to be the basename in the
    # job ad, assuming that TransferInput already has the correct path.
    file_list = [f"-C {str(sif_file.parent)} {sif_file.name}" for sif_file in sif_files]
    files = " ".join(file_list)

    # -h meaning "follow symlinks"
    files = f"-h {files}"

    # Meaning, "stuff these files into the sif/ directory."
    files = f"--transform='s/^/sif\//' ${files}"

    transfer_files(
        logger,
        ssh_connection_sharing,
        ssh_target,
        ssh_indirect_command,
        remote_script_dir,
        files,
        "transfer .sif files",
    )


def populate_remote_temporary_directory(
    logger,
    ssh_connection_sharing,
    ssh_target,
    ssh_indirect_command,
    target,
    local_script_dir,
    remote_script_dir,
    token_file,
    password_file,
):
    files = [
        f"-C {local_script_dir} {target}.sh",
        f"-C {local_script_dir} {target}.pilot",
        f"-C {local_script_dir} {target}.multi-pilot",
        f"-C {str(token_file.parent)} {token_file.name}",
        f"-C {str(password_file.parent)} {password_file.name}",
    ]
    files = " ".join(files)

    transfer_files(
        logger,
        ssh_connection_sharing,
        ssh_target,
        ssh_indirect_command,
        remote_script_dir,
        files,
        "populate remote temporary directory",
    )


def transfer_files(
    logger,
    ssh_connection_sharing,
    ssh_target,
    ssh_indirect_command,
    remote_script_dir,
    files,
    task,
):
    # FIXME: Pass an actual list to Popen
    proc = subprocess.Popen(
        [
            f'tar -c -f- {files} | ssh {" ".join(ssh_connection_sharing)} {ssh_target} {" ".join(ssh_indirect_command)} tar -C {remote_script_dir} -x -f-'
        ],
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        out, err = proc.communicate(timeout=REMOTE_POPULATE_TIMEOUT)

        if proc.returncode == 0:
            return 0
        else:
            logger.error(f"Failed to {task}, aborting.")
            logger.warning(f"stdout: {out.strip()}")
            logger.warning(f"stderr: {err.strip()}")
            raise RuntimeError(f"Failed to {task}")

    except subprocess.TimeoutExpired:
        logger.error(
            f"Failed to {task} in {REMOTE_POPULATE_TIMEOUT} seconds, aborting."
        )
        proc.kill()
        out, err = proc.communicate()
        raise RuntimeError(f"Failed to {task} in {REMOTE_POPULATE_TIMEOUT} seconds")


def extract_full_lines(buffer):
    last_newline = buffer.rfind(b"\n")
    if last_newline == -1:
        return buffer, []

    lines = [line.decode("utf-8") for line in buffer[:last_newline].split(b"\n")]
    return buffer[last_newline + 1 :], lines


def process_line(line, update_function):
    control = "=-.-= "
    if line.startswith(control):
        command = line[len(control) :]
        attribute, value = command.split(" ")
        update_function(attribute, value)
    else:
        print("   ", line)


def invoke_pilot_script(
    ssh_connection_sharing,
    ssh_target,
    ssh_indirect_command,
    remote_script_dir,
    target,
    annex_name,
    queue_name,
    collector,
    token_file,
    lifetime,
    owners,
    nodes,
    allocation,
    update_function,
    cluster_id,
    password_file,
):
    args = [
        "ssh",
        *ssh_connection_sharing,
        ssh_target,
        *ssh_indirect_command,
        str(remote_script_dir / f"{target}.sh"),
        annex_name,
        queue_name,
        collector,
        str(remote_script_dir / token_file.name),
        str(lifetime),
        str(remote_script_dir / f"{target}.pilot"),
        owners,
        str(nodes),
        str(remote_script_dir / f"{target}.multi-pilot"),
        str(allocation),
        str(cluster_id),
        str(remote_script_dir / password_file.name),
    ]
    proc = subprocess.Popen(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )

    #
    # An alternative to nonblocking I/O and select.poll() would be to
    # spawn a thread to do blocking read()s and feed a queue.Queue;
    # the main thread would use queue.Queue.get_nowait().
    #
    # This should be pretty safe and easy to implement, but let's not
    # bother unless we need to (because we lose data while processing).
    #

    # Make the output stream nonblocking.
    out_fd = proc.stdout.fileno()
    flags = fcntl.fcntl(out_fd, fcntl.F_GETFL)
    fcntl.fcntl(out_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    # Check to see if the child process is still alive.  Regardless, empty
    # the output stream, then process it.  If the child process was not alive,
    # we've read all of its output and are done.  Otherwise, repeat.

    out_bytes = bytes()
    out_buffer = bytes()

    while True:
        rc = proc.poll()

        while True:
            try:
                out_bytes = os.read(proc.stdout.fileno(), 1024)
                if len(out_bytes) == 0:
                    break
                out_buffer += out_bytes

                # Process the output a line at a time, since the control
                # sequences we care about are newline-terminated.

                out_buffer, lines = extract_full_lines(out_buffer)
                for line in lines:
                    process_line(line, update_function)

            # Python throws a BlockingIOError if an asynch FD is unready.
            except BlockingIOError:
                pass

        if rc != None:
            break

    # Print the remainder, if any.
    if len(out_buffer) != 0:
        print("   ", out_buffer.decode("utf-8"))
    print()

    # Set by proc.poll().
    return proc.returncode


# FIXME: catch HTCondorIOError and retry.
def updateJobAd(cluster_id, attribute, value, remotes):
    schedd = htcondor.Schedd()
    schedd.edit(cluster_id, f"hpc_annex_{attribute}", f'"{value}"')
    remotes[attribute] = value


def extract_sif_file(job_ad):
    try:
        container_image = Path(job_ad.get("ContainerImage"))
    except TypeError:
        return None

    if not container_image.name.endswith(".sif"):
        return None

    if container_image.is_absolute():
        return container_image

    try:
        iwd = Path(job_ad["iwd"])
    except KeyError:
        raise KeyError("Could not find iwd in job ad.")

    return iwd / container_image


def annex_create(
    logger,
    annex_name,
    nodes,
    lifetime,
    allocation,
    target,
    queue_name,
    owners,
    collector,
    token_file,
    password_file,
    ssh_target,
    control_path,
):

    # We use this same method to determine the user name in `htcondor job`,
    # so even if it's wrong, it will at least consistently so.
    username = getpass.getuser()

    if target not in MACHINE_QUEUE_MAP:
        raise ValueError(f"{target} is not a known machine.")

    # Location of the local universe script files
    local_script_dir = (
        Path(htcondor.param.get("LIBEXEC", "/usr/libexec/condor")) / "annex"
    )

    if queue_name is None:
        queue_name = MACHINE_QUEUE_MAP[target]
        logger.debug(f"No queue name given, defaulting to {queue_name}")

    token_file = Path(token_file).expanduser()
    if not token_file.exists():
        raise RuntimeError(f"Token file {token_file} doesn't exist.")

    control_path = Path(control_path).expanduser()
    if control_path.is_dir():
        if not control_path.exists():
            logger.debug(f"{control_path} not found, attempt to create it")
            control_path.mkdir(parents=True, exist_ok=True)
    else:
        raise RuntimeError(f"{control_path} must be a directory")

    password_file = Path(password_file).expanduser()
    if not password_file.exists():
        try:
            with password_file.open("wb") as f:
                password = secrets.token_bytes(16)
                f.write(password)
            password_file.chmod(0o0400)
        except OSError as ose:
            raise OSError(
                f"Password file {password_file} does not exist and could not be created: {ose}."
            )

    # Derived constants.
    ssh_connection_sharing = [
        "-o",
        'ControlPersist="5m"',
        "-o",
        'ControlMaster="auto"',
        "-o",
        f'ControlPath="{control_path}/master-%C"',
    ]
    ssh_indirect_command = ["gsissh", target]

    ##
    ## While we're requiring that jobs are submitted before creating the
    ## annex (for .sif pre-staging purposes), refuse to make the annex
    ## if no such jobs exist.
    ##
    schedd = htcondor.Schedd()
    annex_jobs = schedd.query(f'TargetAnnexName == "{annex_name}"')
    if len(annex_jobs) == 0:
        raise RuntimeError(
            f"No jobs for '{annex_name}' are in the queue. Use 'htcondor job submit --annex-name' to add them first."
        )
    logger.debug(
        f"""Found {len(annex_jobs)} annex jobs matching 'TargetAnnexName == "{annex_name}"."""
    )

    # Extract the .sif file from each job.
    sif_files = set()
    for job_ad in annex_jobs:
        sif_file = extract_sif_file(job_ad)
        if sif_file is not None:
            sif_file = Path(sif_file)
            if sif_file.exists():
                sif_files.add(sif_file)
            else:
                raise RuntimeError(
                    f"""Job {job_ad["ClusterID"]}.{job_ad["ProcID"]} specified container image '{sif_file}', which doesn't exist."""
                )
    if len(sif_files) > 0:
        logger.debug(f"Got sif files: {sif_files}")
    else:
        logger.debug("No sif files found, continuing...")
    # The .sif files will be transferred to the target machine later.

    ##
    ## The user will do the 2FA/SSO dance here.
    ##
    logger.info("Making initial SSH connection...")
    logger.info(
        f"  (You can run 'ssh {' '.join(ssh_connection_sharing)} {ssh_target}' to use the shared connection.)"
    )
    rc = make_initial_ssh_connection(
        ssh_connection_sharing,
        ssh_target,
        ssh_indirect_command,
    )
    if rc != 0:
        raise RuntimeError(
            f"Failed to make initial connection to {target}, aborting ({rc})."
        )

    ##
    ## Register the clean-up function before creating the mess to clean-up.
    ##
    remote_script_dir = None

    # Allow atexit functions to run on SIGTERM.
    signal.signal(signal.SIGTERM, lambda signal, frame: sys.exit(128 + 15))
    # Hide the traceback on CTRL-C.
    signal.signal(signal.SIGINT, lambda signal, frame: sys.exit(128 + 2))
    # Remove the temporary directories on exit.
    atexit.register(
        lambda: remove_remote_temporary_directory(
            logger,
            ssh_connection_sharing,
            ssh_target,
            ssh_indirect_command,
            remote_script_dir,
        )
    )

    logger.info("Making remote temporary directory...")
    remote_script_dir = Path(
        make_remote_temporary_directory(
            logger,
            ssh_connection_sharing,
            ssh_target,
            ssh_indirect_command,
        )
    )

    logger.info(f"Populating {remote_script_dir} ...")
    populate_remote_temporary_directory(
        logger,
        ssh_connection_sharing,
        ssh_target,
        ssh_indirect_command,
        target,
        local_script_dir,
        remote_script_dir,
        token_file,
        password_file,
    )
    logger.debug("... transferring sif files ...")
    transfer_sif_files(
        logger,
        ssh_connection_sharing,
        ssh_target,
        ssh_indirect_command,
        remote_script_dir,
        sif_files,
    )
    logger.info("... remote directory populated.")

    # Submit local universe job.
    logger.info("Submitting state-tracking job...")
    local_job_executable = local_script_dir / "annex-local-universe.py"
    if not local_job_executable.exists():
        raise RuntimeError(
            f"Could not find local universe executable, expected {local_job_executable}"
        )
    #
    # The magic in this job description is thus:
    #   * hpc_annex_start_time is undefined until the job runs and finds
    #     a machine ad with a matching hpc_annex_request_id.
    #   * The job will go idle (because it can't start) at that poing,
    #     based on its Requirements.
    #   * Before then, the job's on_exit_remove must be false -- not
    #     undefined -- to make sure it keeps polling.
    #   * The job runs every five minutes because of cron_minute.
    #
    submit_description = htcondor.Submit(
        {
            "universe": "local",
            # hpc_annex_start time is set by the job script when it finds
            # a machine with a matching request ID.  At that point, we can
            # stop runnig this script, but we don't remove it to simplify
            # the UI/UX code; instead, we wait until an hour past the end
            # of the request's lifetime to trigger a peridic remove.
            "requirements": "hpc_annex_start_time =?= undefined",
            "executable": str(local_job_executable),
            # Sadly, even if you set on_exit_remove to ! requirements,
            # the job lingers in X state for a good long time.
            "cron_minute": "*/5",
            "on_exit_remove": "PeriodicRemove =?= true",
            "periodic_remove": f"hpc_annex_start_time + {lifetime} + 3600 < time()",
            # Consider adding a log, an output, and an error file to assist
            # in debugging later.  Problem: where should it go?  How does it
            # cleaned up?
            "environment": f'PYTHONPATH={os.environ.get("PYTHONPATH", "")}',
            "arguments": f"$(CLUSTER).0 hpc_annex_request_id $(CLUSTER) {collector}",
            "+hpc_annex_request_id": '"$(CLUSTER)"',
            # Properties of the annex request.  We should think about
            # representing these as a nested ClassAd.  Ideally, the back-end
            # would, instead of being passed a billion command-line arguments,
            # just pull this ad from the collector (after this local universe
            # job has forwarded it there).
            "+hpc_annex_name": f'"{annex_name}"',
            "+hpc_annex_queue_name": f'"{queue_name}"',
            "+hpc_annex_collector": f'"{collector}"',
            "+hpc_annex_lifetime": f'"{lifetime}"',
            "+hpc_annex_owners": f'"{owners}"',
            "+hpc_annex_nodes": f'"{nodes}"',
            "+hpc_annex_allocation": f'"{allocation}"'
            if allocation is not None
            else "undefined",
            # Hard state required for clean up.  We'll be adding
            # hpc_annex_PID, hpc_annex_PILOT_DIR, and hpc_annex_JOB_ID
            # as they're reported by the back-end script.
            "+hpc_annex_remote_script_dir": f'"{remote_script_dir}"',
        }
    )

    try:
        logger.debug(f"... submitting {submit_description}")
        submit_result = schedd.submit(submit_description)
    except:
        raise RuntimeError(f"Failed to submit state-tracking job, aborting.")

    cluster_id = submit_result.cluster()
    logger.info(f"... done, with cluster ID {cluster_id}.")

    remotes = {}
    logger.info(f"Submitting SLURM job on {target}:\n")
    rc = invoke_pilot_script(
        ssh_connection_sharing,
        ssh_target,
        ssh_indirect_command,
        remote_script_dir,
        target,
        annex_name,
        queue_name,
        collector,
        token_file,
        lifetime,
        owners,
        nodes,
        allocation,
        lambda attribute, value: updateJobAd(cluster_id, attribute, value, remotes),
        cluster_id,
        password_file,
    )

    if rc == 0:
        logger.info(f"... remote SLURM job submitted.")
    else:
        raise RuntimeError(f"Failed to start annex, SLURM returned code {rc}")

    ##
    ## Now that we've started the annex, rewrite the jobs targeting it
    ## so that they don't transfer the .sif files we just pre-staged.
    ##
    ## The startd can't rewrite the job ad the shadow uses to decide
    ## if it should transfer the .sif file, but it can change ContainerImage
    ## to point the pre-staged image, if it's just the basename.  (Otherwise,
    ## it gets impossibly difficult to make the relative paths work.)
    ##

    sif_dir = Path(remotes["PILOT_DIR"]) / "sif"
    for job_ad in annex_jobs:
        job_id = f'{job_ad["ClusterID"]}.{job_ad["ProcID"]}'
        sif_file = extract_sif_file(job_ad)
        if sif_file is not None:
            remote_sif_file = Path(sif_file).name
            logger.debug(
                f"Setting ContainerImage = {remote_sif_file} in annex job {job_id}"
            )
            schedd.edit(job_id, "ContainerImage", f'"{remote_sif_file}"')

            transfer_input = job_ad["TransferInput"]
            input_files = transfer_input.split(",")
            if job_ad["ContainerImage"] in input_files:
                logger.debug(
                    f"Removing {job_ad['ContainerImage']} from input files in annex job {job_id}"
                )
                input_files.remove(job_ad["ContainerImage"])
                if len(input_files) != 0:
                    schedd.edit(job_id, "TransferInput", f'"{",".join(input_files)}"')
                else:
                    schedd.edit(job_id, "TransferInput", "undefined")


# FIXME: If we really care about this functionality, use argparse and duplicate
# the args from the htcondor cli tool
def __main():
    """
    Main entry point, only to be used when run as a standalone executable
    """
    username = getpass.getuser()
    target = "stampede2"
    nodes = 2
    lifetime = 7200

    annex_name = "hpc-annex"
    queue_name = "development"
    owners = username
    allocation = None

    collector = htcondor.param.get(
        "ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io"
    )
    token_file = f"~/.condor/tokens.d/{username}@annex.osgdev.chtc.io"
    control_path = "~/.hpc-annex"
    password_file = htcondor.param.get(
        "ANNEX_PASSWORD_FILE", "~/.condor/annex_password_file"
    )

    ssh_target = f"{username}@login.xsede.org"

    logging.basicConfig(format="%(message)s", level=logging.INFO)
    logger = logging.getLogger()

    annex_create(
        logger,
        annex_name,
        nodes,
        lifetime,
        allocation,
        target,
        queue_name,
        owners,
        collector,
        token_file,
        password_file,
        ssh_target,
        control_path,
    )


if __name__ == "__main__":
    __main()
