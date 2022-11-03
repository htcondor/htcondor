#!/usr/bin/env python3


import os
import sys
import fcntl
import atexit
import signal
import getpass
import logging
import secrets
import textwrap
import subprocess
from pathlib import Path

import htcondor
import classad

#
# We could try to import colorama here -- see condor_watch_q -- but that's
# only necessary for Windows.
#

INITIAL_CONNECTION_TIMEOUT = int(htcondor.param.get("ANNEX_INITIAL_CONNECTION_TIMEOUT", 180))
REMOTE_CLEANUP_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_CLEANUP_TIMEOUT", 60))
REMOTE_MKDIR_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_MKDIR_TIMEOUT", 30))
REMOTE_POPULATE_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_POPULATE_TIMEOUT", 60))
TOKEN_FETCH_TIMEOUT = int(htcondor.param.get("ANNEX_TOKEN_FETCH_TIMEOUT", 20))


#
# We need a little in the way of formal structure for the system/queue table
# because Bridges 2 and Perlmutter don't behave anything like the others.
#
# Note that `queues` must be a dict for the help messages to work right,
# but the data structure can otherwise be empty.
#
class System:
    def __init__(self, *,
        pretty_name: str,
        host_name: str,
        default_queue: str,
        batch_system: str,
        script_base: str,
        allocation_reqd: bool = False,
        queues: dict,
    ):
        self.pretty_name = str(pretty_name)
        self.host_name = str(host_name)
        self.default_queue = str(default_queue)
        self.batch_system = str(batch_system)
        self.script_base = str(script_base)
        self.allocation_required = allocation_reqd
        self.queues = queues


    def __str__(self):
        rv = f"{self.__class__.__name__}("
        rv += f"pretty_name='{self.pretty_name}'"
        rv += f", host_name='{self.host_name}'"
        rv += f", default_queue='{self.default_queue}'"
        rv += f", batch_system='{self.batch_system}'"
        rv += f", script_base='{self.script_base}'"
        rv += f", allocation_required={self.allocation_required}"
        rv += ")"
        return rv


    def get_constraints(self, queue_name, gpus, gpu_type):
        return self.queues.get(queue_name)


    def validate_system_specific_constraints(self, queue_name, cpus, mem_mb):
        pass


class Bridges2System(System):
    def __init__(self, ** kwargs):
        super().__init__(** kwargs)

    def validate_system_specific_constraints(self, queue_name, cpus, mem_mb):
        if queue_name == "RM-shared":
            if mem_mb is not None:
                error_string = f"The 'RM-shared' queue assigns memory proportional to the number of CPUs.  Do not specify --mem_mb for this queue."
                raise ValueError(error_string)
        elif queue_name == "EM":
            if cpus is None or cpus % 24 != 0:
                error_string = f"The 'EM-shared' queue only allows requests of 24, 47, 72, or 96 cpus.  Use --cpus to specify."
                raise ValueError(error_string)
            if mem_mb is not None:
                error_string = f"The 'EM-shared' queue assigns memory proportional to the number of CPUs.  Do not specify --mem_mb for this queue."
                raise ValueError(error_string)
        elif queue_name == "GPU-shared":
            if cpus is not None or mem_mb is not None:
                error_string = f"The 'GPU-shared' queue assigns CPUs and memory proportional to the number of GPUs.  Do not specify --cpus or --mem_mb for this partition."
                raise ValueError(error_string)
        else:
            pass

    def get_constraints(self, queue_name, gpus, gpu_type):
        queue = self.queues.get(queue_name)
        if queue is None:
            return None

        # Don't add valid GPU information to a queue description just
        # because the user asked for it.
        if not queue_name.startswith("GPU"):
            return queue

        # Bridges 2 has three different sets of GPU machines, each of
        # of which can be specified in either the whole-node ("GPU")
        # or shared ("GPU-shared") queue.
        #
        # If the user didn't asked for a specific GPU type, tell them
        # which type to ask for.

        if gpus == 16 and gpu_type == "v100-32":
            queue = { ** queue, ** {
                "cores_per_node":       48,
                "ram_per_node":         1536 * 1024,
                "gpus_per_node":        16,
                "max_nodes_per_job":    1,
                },
            }
        elif gpu_type == "v100-32":
            queue = { ** queue, ** {
                "cores_per_node":       40,
                "ram_per_node":         512 * 1024,
                "gpus_per_node":        8,
                "max_nodes_per_job":    4,
                },
            }
        elif gpu_type == "v100-16":
            queue = { ** queue, ** {
                "cores_per_node":       40,
                "ram_per_node":         192 * 1024,
                "gpus_per_node":        8,
                "max_nodes_per_job":    4,
                },
            }
        else:
            error_string = f"The system {self.pretty_name} has the following types of GPUs: v100-16, v100-32.  Use --gpu-type to select."
            raise ValueError(error_string)

        # The GPU-shared queue only allows 4 GPUs per node.
        if queue_name == "GPU-shared":
            queue["gpus_per_node"] = 4

        return queue


#
# For queues with the whole-node allocation policy, cores and RAM -per-node
# are only informative at the moment; we could compute the necessary number
# of nodes for the user (as long as we echo it back to them), but let's not
# do that for now and just tell the user to size their requests with the
# units appropriate to the queue.
#
# * The key is currently both the value of the command-line option.
# * We omit queues (partitions) which this tool doesn't support.
# * ram_per_node is in MB.
#

SYSTEM_TABLE = {
    "stampede2": System( ** {
        "pretty_name":      "Stampede 2",
        "host_name":        "stampede2.tacc.utexas.edu",
        "default_queue":    "normal",
        "batch_system":     "SLURM",
        "script_base":      "hpc",
        "allocation_reqd":  False,

        "queues": {
            "normal": {
                "max_nodes_per_job":    256,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       68,
                "ram_per_node":         96 * 1024,

                "max_jobs_in_queue":    50,
            },
            "development": {
                "max_nodes_per_job":    16,
                "max_duration":         2 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       68,
                "ram_per_node":         96 * 1024,

                "max_jobs_in_queue":    1,
            },
            "skx-normal": {
                "max_nodes_per_job":    128,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       48,
                "ram_per_node":         192 * 1024,

                "max_jobs_in_queue":    20,
            },
        },
    },
    ),

    "expanse": System( ** {
        "pretty_name":      "Expanse",
        "host_name":        "login.expanse.sdsc.edu",
        "default_queue":    "compute",
        "batch_system":     "SLURM",
        "script_base":      "hpc",
        "allocation_reqd":  True,

        "queues": {
            "compute": {
                "max_nodes_per_job":    32,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    64,
            },
            "gpu": {
                "max_nodes_per_job":    4,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       40,
                "ram_per_node":         384 * 1024,
                "gpus_per_node":        4,

                "max_jobs_in_queue":    8,
            },
            "shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    4096,
            },
            "gpu-shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       40,
                "ram_per_node":         384 * 1024,
                "gpus_per_node":        4,

                "max_jobs_in_queue":    24,
            },
        },
    },
    ),

    "anvil": System( ** {
        "pretty_name":      "Anvil",
        "host_name":        "anvil.rcac.purdue.edu",
        "default_queue":    "wholenode",
        "batch_system":     "SLURM",
        "script_base":      "hpc",
        "allocation_reqd":  False,

        "queues": {
            "wholenode": {
                "max_nodes_per_job":    16,
                "max_duration":         96 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    128,
            },
            "wide": {
                "max_nodes_per_job":    56,
                "max_duration":         12 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    10,
            },
            "shared": {
                "max_nodes_per_job":    1,
                "max_duration":         96 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    None,
            },
            # Max of 12 GPUs per user and 32 per allocation.  Since
            # every node has 4 GPUs, and you get the whole node....
            "gpu": {
                "max_nodes_per_job":    3,
                "max_duration":         48 * 60 * 60,
                "max_jobs_in_queue":    None,

                "allocation_type":      "node",

                "cores_per_node":       128,
                "ram_per_node":         512 * 1024,

                "gpus_per_node":        4,
            },
            "gpu-debug": {
                "max_nodes_per_job":    1,
                "max_duration":         30 * 60,
                "max_jobs_in_queue":    1,

                "allocation_type":      "node",

                "cores_per_node":       128,
                "ram_per_node":         512 * 1024,

                "gpus_per_node":        2,
            },
        },
    },
    ),

    "bridges2": Bridges2System( ** {
        "pretty_name":      "Bridges-2",
        "host_name":        "bridges2.psc.edu",
        "default_queue":    "RM",
        "batch_system":     "SLURM",
        "script_base":      "hpc",
        "allocation_reqd":  False,

        "queues": {
            "RM": {
                "max_nodes_per_job":    50,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    50,
            },
            "RM-512": {
                "max_nodes_per_job":    2,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         512 * 1024,

                "max_jobs_in_queue":    50,
            },
            "RM-shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         128 * 1024,

                "max_jobs_in_queue":    50,
            },
            "EM": {
                "max_nodes_per_job":    1,
                "max_duration":         120 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       96,
                "ram_per_node":         4 * 1024 * 1024,

                "max_jobs_in_queue":    50,
            },

            # Deliberately incomplete.  See get_constraints().
            "GPU": {
                "max_nodes_per_job":    4,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "gpu_types":            [ 'v100-32', 'v100-16' ],
                "gpu_flag_type":        "job",
            },
            "GPU-shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "gpu-shared",
                "gpu_types":            [ 'v100-32', 'v100-16' ],
                "gpu_flag_type":        "job",
            },
        },
    },
    ),

    "path-facility": System( ** {
        "pretty_name":      "PATh Facilty",
        "host_name":        "ap1.facility.path-cc.io",
        "default_queue":    "cpu",
        "batch_system":     "HTCondor",
        "script_base":      "path-facility",
        "allocation_reqd":  False,

        "queues": {
            "cpu": {
                # This is actually max-jobs-per-request for this system,
                # but for now, it's easier to think about each request
                # being a single job like it is for SLURM.
                "max_nodes_per_job":    1, # FIXME
                "max_duration":         60 * 60 * 72, # FIXME
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         244 * 1024,

                "max_jobs_in_queue":    1000, # FIXME
            },
            # When we formally support GPUs, we'll need to add the GPU
            # queue here, and -- because HTCondor doesn't have multiple
            # queues -- some special code somewhere to insert request_gpus.
            # (Seems like we should allow the user to specify how many
            # GPUs per node for the PATh Facility, too, so check that
            # against the queue name (warn if cpu queue?) and also default
            # it to 1 if the queue name is 'gpu'?)
            #
            # Also, we may not want to call them "nodes" for the PATh
            # facility.
        },
    },
    ),
}


def make_initial_ssh_connection(
    ssh_connection_sharing,
    ssh_user_name,
    ssh_host_name,
):

    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"

    proc = subprocess.Popen(
        [
            "ssh",
            "-f",
            *ssh_connection_sharing,
            ssh_target,
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
    except KeyboardInterrupt:
        sys.exit(1)


def remove_remote_temporary_directory(
    logger, ssh_connection_sharing, ssh_user_name, ssh_host_name, remote_script_dir
):
    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"

    if remote_script_dir is not None:
        logger.debug("Cleaning up remote temporary directory...")
        proc = subprocess.Popen(
            [
                "ssh",
                *ssh_connection_sharing,
                ssh_target,
                "rm",
                "-fr",
                remote_script_dir,
            ],
        )

        try:
            return proc.wait(timeout=REMOTE_CLEANUP_TIMEOUT)
        except subprocess.TimeoutExpired:
            logger.error(
                f"Did not clean up remote temporary directory after {REMOTE_CLEANUP_TIMEOUT} seconds, '{remote_script_dir}' may need to be deleted manually."
            )

    return 0


def make_remote_temporary_directory(
    logger,
    ssh_connection_sharing,
    ssh_user_name,
    ssh_host_name,
):
    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"

    remote_command = r'mkdir -p \${HOME}/.hpc-annex/scratch && ' \
        r'mktemp --tmpdir=\${HOME}/.hpc-annex/scratch --directory remote_script.XXXXXXXX'
    proc = subprocess.Popen(
        [
            "ssh",
            *ssh_connection_sharing,
            ssh_target,
            "sh",
            "-c",
            f"\"{remote_command}\"",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        # This implies text mode.
        errors="replace",
    )

    try:
        out, err = proc.communicate(timeout=REMOTE_MKDIR_TIMEOUT)

        if proc.returncode == 0:
            return out.strip()
        else:
            logger.error("Failed to make remote temporary directory, got output:")
            logger.error(f"{out.strip()}")
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
    ssh_user_name,
    ssh_host_name,
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
    files = f"--transform='s|^|sif/|' {files}"

    transfer_files(
        logger,
        ssh_connection_sharing,
        ssh_user_name,
        ssh_host_name,
        remote_script_dir,
        files,
        "transfer .sif files",
    )


def populate_remote_temporary_directory(
    logger,
    ssh_connection_sharing,
    ssh_user_name,
    ssh_host_name,
    system,
    local_script_dir,
    remote_script_dir,
    token_file,
    password_file,
):
    script_base = SYSTEM_TABLE[system].script_base

    files = [
        f"-C {local_script_dir} {script_base}.sh",
        f"-C {local_script_dir} {system}.fragment",
        f"-C {local_script_dir} {script_base}.pilot",
        f"-C {local_script_dir} {script_base}.multi-pilot",
        f"-C {str(token_file.parent)} {token_file.name}",
        f"-C {str(password_file.parent)} {password_file.name}",
    ]
    files = " ".join(files)

    transfer_files(
        logger,
        ssh_connection_sharing,
        ssh_user_name,
        ssh_host_name,
        remote_script_dir,
        files,
        "populate remote temporary directory",
    )


def transfer_files(
    logger,
    ssh_connection_sharing,
    ssh_user_name,
    ssh_host_name,
    remote_script_dir,
    files,
    task,
):
    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"

    # FIXME: Pass an actual list to Popen
    full_command = f'tar -c -f- {files} | ssh {" ".join(ssh_connection_sharing)} {ssh_target} tar -C {remote_script_dir} -x -f-'
    proc = subprocess.Popen(
        [
            full_command
        ],
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        # This implies text mode.
        errors="replace",
    )

    try:
        out, err = proc.communicate(timeout=REMOTE_POPULATE_TIMEOUT)

        if proc.returncode == 0:
            return 0
        else:
            logger.error(f"Failed to {task}, aborting.")
            logger.debug(f"Command '{full_command}' got output:")
            logger.warning(f"{out.strip()}")
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

    lines = [line.decode("utf-8", errors="replace")
             for line in buffer[:last_newline].split(b"\n")]
    return buffer[last_newline + 1 :], lines


previous_line_was_update = False
def process_line(line, update_function):
    global previous_line_was_update

    control = "=-.-= "
    if line.startswith(control):
        command = line[len(control) :]
        attribute, value = command.split(" ")
        update_function(attribute, value)
        previous_line_was_update = False
    elif line.startswith('\r'):
        print("\r   ", line[1:], end='')
        previous_line_was_update = True
    else:
        if previous_line_was_update:
            print('')
        print("   ", line)
        previous_line_was_update = False


def invoke_pilot_script(
    ssh_connection_sharing,
    ssh_user_name,
    ssh_host_name,
    remote_script_dir,
    system,
    startd_noclaim_shutdown,
    annex_name,
    queue_name,
    collector,
    token_file,
    lifetime,
    owners,
    nodes,
    allocation,
    update_function,
    request_id,
    password_file,
    cpus,
    mem_mb,
    gpus,
    gpu_type,
):
    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"

    script_base = SYSTEM_TABLE[system].script_base

    gpu_argument = str(gpus)
    if gpu_type is not None:
        gpu_argument = f"{gpu_type}:{gpus}"
    args = [
        "ssh",
        *ssh_connection_sharing,
        ssh_target,
        str(remote_script_dir / f"{script_base}.sh"),
        system,
        str(startd_noclaim_shutdown),
        annex_name,
        queue_name,
        collector,
        str(remote_script_dir / token_file.name),
        str(lifetime),
        str(remote_script_dir / f"{script_base}.pilot"),
        owners,
        str(nodes),
        str(remote_script_dir / f"{script_base}.multi-pilot"),
        str(allocation),
        request_id,
        str(remote_script_dir / password_file.name),
        str(cpus),
        str(mem_mb),
        gpu_argument
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
        print("   ", out_buffer.decode("utf-8", errors="replace"))
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


def create_annex_token(logger, type):
    token_lifetime = int(htcondor.param.get("ANNEX_TOKEN_LIFETIME", 60 * 60 * 24 * 90))
    annex_token_key_name = htcondor.param.get("ANNEX_TOKEN_KEY_NAME", "hpcannex-key")
    annex_token_domain = htcondor.param.get("ANNEX_TOKEN_DOMAIN", "annex.osgdev.chtc.io")
    token_name = f"{type}.{getpass.getuser()}@{annex_token_domain}"

    args = [
        'condor_token_fetch',
        '-lifetime', str(token_lifetime),
        '-token', token_name,
        '-key', annex_token_key_name,
        '-authz', 'READ',
        '-authz', 'ADVERTISE_STARTD',
        '-authz', 'ADVERTISE_MASTER',
        '-authz', 'ADVERTISE_SCHEDD',
    ]

    proc = subprocess.Popen(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        # This implies text mode.
        errors="replace",
    )

    try:
        out, err = proc.communicate(timeout=TOKEN_FETCH_TIMEOUT)

        if proc.returncode == 0:
            sec_token_directory = htcondor.param.get("SEC_TOKEN_DIRECTORY")
            if sec_token_directory == "":
                sec_token_directory = "~/.condor/tokens.d"
            return os.path.expanduser(f"{sec_token_directory}/{token_name}")
        else:
            logger.error(f"Failed to create annex token, aborting.")
            logger.warning(f"{out.strip()}")
            raise RuntimeError(f"Failed to create annex token")

    except subprocess.TimeoutExpired:
        logger.error(f"Failed to create annex token in {TOKEN_FETCH_TIMEOUT} seconds, aborting.")
        proc.kill()
        out, err = proc.communicate()
        raise RuntimeError(f"Failed to create annex token in {TOKEN_FETCH_TIMEOUT} seconds")


def validate_system_specific_constraints(
    system,
    queue_name,
    nodes,
    lifetime_in_seconds,
    allocation,
    cpus,
    mem_mb,
    idletime_in_seconds,
    gpus,
    gpu_type,
):
    system = SYSTEM_TABLE[system]

    # (A) Is the queue a valid, known, and supported?
    queue = system.get_constraints(queue_name, gpus, gpu_type)
    if queue is None:
        pretty_name = system.pretty_name
        queue_list = "\n    ".join(system.queues)
        default_queue = system.default_queue

        error_string = f"'{queue_name}' is not a supported queue on the system named '{pretty_name}'."
        error_string = f"{error_string}  Supported queues are:\n    {queue_list}\nUse '{default_queue}' if you're not sure."
        raise ValueError(error_string)

    # (B) If the user did not specify CPUs or memory, then the queue's
    # allocation type must be 'node'.  (We supply a default number of nodes.)
    if cpus is None and mem_mb is None:
        if queue['allocation_type'] == 'cores_or_ram':
            error_string = f"The '{queue_name}' queue is not a whole-node queue.  You must specify either CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

    # (C) If the queue's allocation type is 'node', then specifying CPUs or
    # memory is pointless.
    if queue['allocation_type'] == 'node':
        if cpus is not None or mem_mb is not None:
            error_string = f"The '{queue_name}' queue is a whole-node queue.  You can't specify CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

        # Don't allow the user to request more than max nodes.
        mnpj = queue['max_nodes_per_job']
        if nodes > mnpj:
            error_string = f"The '{queue_name}' queue is limited to {mnpj} nodes per job.  Use --nodes to set."
            raise ValueError(error_string)

    # (D) If the queue's allocation type is 'cores_or_ram', you must specify
    # CPUs or memory.  (This might not actually be true, technically.)
    # The first check is presently redundant with check (A).
    #
    # When you specify CPUs or memory, you must specify less than the max.
    if queue['allocation_type'] == 'cores_or_ram':
        if cpus is None and mem_mb is None:
            error_string = f"The '{queue_name}' queue is a shared-node queue.  You must specify CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

        rpn = queue['ram_per_node']
        if mem_mb is not None and mem_mb > rpn:
            error_string = f"The '{queue_name}' queue is limited to {rpn} MB per node.  Use --mem_mb to set."
            raise ValueError(error_string)

        if cpus is not None and cpus > queue['cores_per_node']:
            error_string = f"The '{queue_name}' queue is limited to {queue['cores_per_node']} cores per node."
            raise ValueError(error_string)

        # Don't allow the user to request more than max nodes.
        mnpj = queue['max_nodes_per_job']
        if nodes > mnpj:
            error_string = f"The '{queue_name}' queue is limited to {mnpj} nodes per job.  Use --nodes to set."
            raise ValueError(error_string)

    # (J) If the queue's allocation type is 'gpu-shared', then specifying
    #     CPUs or memory is pointless.
    if queue['allocation_type'] == 'gpu-shared':
        if cpus is not None or mem_mb is not None:
            error_string = f"The '{queue_name}' queue always assigns CPUs and RAM in proportiona to the ratio of requested GPUs to total GPUs.  You can't specify CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

        # Don't allow the user to request more than max nodes.
        mnpj = queue['max_nodes_per_job']
        if nodes > mnpj:
            error_string = f"The '{queue_name}' queue is limited to {mnpj} nodes per job.  Use --nodes to set."
            raise ValueError(error_string)

    # (E) You must not specify a lifetime longer than the queue's duration.
    if queue['max_duration'] < lifetime_in_seconds:
        error_string = f"The '{queue_name}' queue has a maximum duration of {queue['max_duration']} seconds, which is less than the requested lifetime ({lifetime_in_seconds} seconds).  Use --lifetime to set."
        raise ValueError(error_string)

    # (F) You must not specify an idletime longer than the queue's duration.
    #     There's an argument that doing so is the natural way to disable
    #     the idleness check entirely, so let's skip this check for now.
    # if queue['max_duration'] < idletime_in_seconds:
    #    error_string = f"The '{queue_name}' queue has a maximum duration of {queue['max_duration']} seconds, which is less than the requeqested lifetime ({idletime_in_seconds} seconds).  Use --idle-time to set."
    #    raise ValueError(error_string)

    # (G) You must not specify an idletime longer than the lifetime.
    if lifetime_in_seconds < idletime_in_seconds:
        error_string = f"You may not specify an idle time (--idle-time) longer than the lifetime (--lifetime)."
        raise ValueError(error_string)

    # (H) Some systems require an allocation be specified, even if it's
    #     your only one.
    if allocation is None and system.allocation_required:
        error_string = f"The system named '{system.pretty_name}' requires you to specify a project (--project), even if you only have one."
        raise ValueError(error_string)

    # (I) You must request an appropriate number of GPUs if the queue
    #     offer GPUs.
    gpus_per_node = queue.get('gpus_per_node')
    if gpus_per_node is not None:
        if gpus is None or gpus <= 0:
            error_string = f"The '{queue_name}' queue is a GPU queue.  You must specify a number of GPUs (--gpus)."
            raise ValueError(error_string)

        if gpus > gpus_per_node:
            error_string = f"The '{queue_name}' queue is limited to {gpus_per_node} GPUs per node.  Use --gpus to set."
            raise ValueError(error_string)

        # If the allocation type is node, you get (and are charged for) all
        # of the GPUs whether you asked for them or not.
        if queue['allocation_type'] == 'node':
            if gpus < gpus_per_node:
                error_string = f"The '{queue_name}' queue always assigns {gpus_per_node} GPUs per node.  Set --gpus to match."
                raise ValueError(error_string)

    # (L) You must not request GPUs from queues which don't offer them.
    if gpus_per_node is None:
        if gpus is None or gpus <= 0:
            pass
        else:
            error_string = f"The '{queue_name}' does not offer GPUs.  Do not set --gpus for this queue."
            raise ValueError(error_string)

    # (M) You must not specify a GPU type for a queue which doesn't offer them.
    #     You must specify a valid GPU type for queues which do.
    if gpu_type is not None:
        gpu_types = queue.get('gpu_types')
        if queue.get('gpu_types') is None:
            error_string = f"The '{queue_name}' queue only has one type of GPU.  Do not set --gpu-type for this queue."
            raise ValueError(error_string)
        elif not gpu_type in gpu_types:
            gpu_type_list = ", ".join(gpu_types)
            error_string = f"The system {system.pretty_name} has the following types of GPUs: {gpu_type_list}.  Use --gpu-type to select."
            raise ValueError(error_string)

    # (K) Run the system-specific constraint checker.
    system.validate_system_specific_constraints(queue_name, cpus, mem_mb)

    if gpus_per_node is not None:
        if queue.get('gpu_flag_type') is 'job':
            if gpus is not None and gpus > 0:
                assert nodes is not None and nodes >= 1, f"Internal error during validation: node count ({nodes}) should have been >= 1 by now."
                gpus = gpus * nodes

    return gpus

def annex_inner_func(
    logger,
    annex_name,
    nodes,
    lifetime,
    allocation,
    queue_at_system,
    owners,
    collector,
    token_file,
    password_file,
    control_path,
    cpus,
    mem_mb,
    login_name,
    login_host,
    startd_noclaim_shutdown,
    gpus,
    gpu_type,
    test,
):
    if '@' in queue_at_system:
        (queue_name, system) = queue_at_system.split('@', 1)
    else:
        error_string = "Argument must have the form queue@system."

        system = queue_at_system.casefold()
        if system not in SYSTEM_TABLE:
            error_string = f"{error_string}  Also, '{queue_at_system}' is not the name of a supported system."
            system_list = "\n    ".join(SYSTEM_TABLE.keys())
            error_string = f"{error_string}  Supported systems are:\n    {system_list}"
        else:
            default_queue = SYSTEM_TABLE[system].default_queue
            queue_list = "\n    ".join([q for q in SYSTEM_TABLE[system].queues])
            error_string = f"{error_string}  Supported queues are:\n    {queue_list}\nUse '{default_queue}' if you're not sure."

        raise ValueError(error_string)

    # We use this same method to determine the user name in `htcondor job`,
    # so even if it's wrong, it will at least consistently so.
    username = getpass.getuser()

    system = system.casefold()
    if system not in SYSTEM_TABLE:
        error_string = f"'{system}' is not the name of a supported system."
        system_list = "\n    ".join(SYSTEM_TABLE.keys())
        error_string = f"{error_string}  Supported systems are:\n    {system_list}"
        raise ValueError(error_string)


    #
    # Handle special case for Bridges 2.
    #
    if gpus is not None:
        if ":" in gpus:
            (gpu_type, gpus) = gpus.split(":")
        try:
            gpus = int(gpus)
        except ValueError:
            error_string = f"The --gpus argument must be either an integer or an integer followed by ':' and a GPU type."
            raise ValueError(error_string)


    #
    # We'll need to validate the requested nodes (or CPUs and memory)
    # against the requested queue[-system pair].  We also need to
    # check the lifetime (and idle time, once we support it).
    #

    # As reminders for when we fix lifetime being specified in seconds.
    lifetime_in_seconds = lifetime
    idletime_in_seconds = startd_noclaim_shutdown

    gpus = validate_system_specific_constraints(system, queue_name, nodes, lifetime_in_seconds, allocation, cpus, mem_mb, idletime_in_seconds, gpus, gpu_type)

    if test:
        return

    # Location of the local universe script files
    local_script_dir = (
        Path(htcondor.param.get("LIBEXEC", "/usr/libexec/condor")) / "annex"
    )

    if not local_script_dir.is_dir():
        raise RuntimeError(f"Annex script dir {local_script_dir} not found or not a directory.")

    # If the user didn't specify a token file, create one on the fly.
    if token_file is None:
        logger.debug("Creating annex token...")
        token_file = create_annex_token(logger, annex_name)
        atexit.register(lambda: os.unlink(token_file))
        logger.debug("..done.")

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
            old_umask = os.umask(0o077)
            with password_file.open("wb") as f:
                password = secrets.token_bytes(16)
                f.write(password)
            password_file.chmod(0o0400)
            try:
                os.umask(old_umask)
            except OSError:
                pass
        except OSError as ose:
            raise RuntimeError(
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

    ssh_user_name = htcondor.param.get(f"HPC_ANNEX_{system}_USER_NAME")
    if login_name is not None:
        ssh_user_name = login_name

    ssh_host_name = htcondor.param.get(
        f"HPC_ANNEX_{system}_HOST_NAME", SYSTEM_TABLE[system].host_name,
    )
    if login_host is not None:
        ssh_host_name = login_host

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
    if sif_files:
        logger.debug(f"Got sif files: {sif_files}")
    else:
        logger.debug("No sif files found, continuing...")
    # The .sif files will be transferred to the target system later.

    #
    # Print out what's going to be done, along with how to change the
    # values that came from defaults rather than the command-line.
    #
    # FIXME: add --idle, once that's implemented.
    #
    resources = ""
    if cpus is not None:
        resources = f"{cpus} CPUs "
        if mem_mb is not None:
            resources = f"{resources}and "
    if mem_mb is not None:
        resources = f"{resources}{mem_mb}MB of RAM "
    if resources is "":
        resources = f"{nodes} nodes "
    resources = f"{resources}for {lifetime/(60*60):.2f} hours"

    as_project = " (as the default project) "
    if allocation is not None:
        as_project = f" (as the project '{allocation}') "

    logger.info(
        f"This will{as_project}request {resources} for an annex named '{annex_name}'"
        f" from the queue named '{queue_name}' on the system named '{SYSTEM_TABLE[system].pretty_name}'."
        f"  To change the project, use --project."
        f"  To change the resources requested, use either --nodes or one or more of --cpus and --mem_mb."
        f"  To change how long the resources are reqested for, use --lifetime (in seconds)."
        f"\n"
    )


    # Distinguish our text from SSH's text.
    ANSI_BRIGHT = "\033[1m"
    ANSI_RESET_ALL = "\033[0m"

    ##
    ## The user will do the 2FA/SSO dance here.
    ##
    logger.info(
        f"{ANSI_BRIGHT}This command will access the system named "
        f"'{SYSTEM_TABLE[system].pretty_name}' via SSH.  To proceed, "
        f"follow the prompts from that system "
        f"below; to cancel, hit CTRL-C.{ANSI_RESET_ALL}"
    )
    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"
    logger.debug(
        f"  (You can run 'ssh {' '.join(ssh_connection_sharing)} {ssh_target}' to use the shared connection.)"
    )
    rc = make_initial_ssh_connection(
        ssh_connection_sharing,
        ssh_user_name,
        ssh_host_name,
    )
    if rc != 0:
        raise RuntimeError(
            f"Failed to make initial connection to the system named '{SYSTEM_TABLE[system].pretty_name}', aborting ({rc})."
        )
    logger.info(f"{ANSI_BRIGHT}Thank you.{ANSI_RESET_ALL}\n")

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
            ssh_user_name,
            ssh_host_name,
            remote_script_dir,
        )
    )

    logger.debug("Making remote temporary directory...")
    remote_script_dir = Path(
        make_remote_temporary_directory(
            logger,
            ssh_connection_sharing,
            ssh_user_name,
            ssh_host_name,
        )
    )
    logger.debug(f"... made remote temporary directory {remote_script_dir} ...")

    #
    # This operation can fail or take a while, but (HTCONDOR-1058) should
    # not need to display two lines.
    #
    logging.StreamHandler.terminator = " ";
    logger.info(f"Populating annex temporary directory...")
    populate_remote_temporary_directory(
        logger,
        ssh_connection_sharing,
        ssh_user_name,
        ssh_host_name,
        system,
        local_script_dir,
        remote_script_dir,
        token_file,
        password_file,
    )
    if sif_files:
        logger.debug("(transferring container images)")
        transfer_sif_files(
            logger,
            ssh_connection_sharing,
            ssh_user_name,
            ssh_host_name,
            remote_script_dir,
            sif_files,
        )
    logging.StreamHandler.terminator = "\n";
    logger.info("done.")

    # Submit local universe job.
    logger.debug("Submitting state-tracking job...")
    local_job_executable = local_script_dir / "annex-local-universe.py"
    if not local_job_executable.exists():
        raise RuntimeError(
            f"Could not find local universe executable, expected {local_job_executable}"
        )
    #
    # The magic in this job description is thus:
    #   * hpc_annex_start_time is undefined until the job runs and finds
    #     a startd ad with a matching hpc_annex_request_id.
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
            # a startd ad with a matching request ID.  At that point, we can
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
            "+arguments": f'strcat( "$(CLUSTER).0 hpc_annex_request_id ", GlobalJobID, " {collector}")',
            "jobbatchname": f'{annex_name} [HPC Annex]',
            "+hpc_annex_request_id": 'GlobalJobID',
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
            "+hpc_annex_nodes": f'"{nodes}"'
                if nodes is not None else "undefined",
            "+hpc_annex_cpus": f'"{cpus}"'
                if cpus is not None else "undefined",
            "+hpc_annex_mem_mb": f'"{mem_mb}"'
                if mem_mb is not None else "undefined",
            "+hpc_annex_allocation": f'"{allocation}"'
                if allocation is not None else "undefined",
            # Hard state required for clean up.  We'll be adding
            # hpc_annex_PID, hpc_annex_PILOT_DIR, and hpc_annex_JOB_ID
            # as they're reported by the back-end script.
            "+hpc_annex_remote_script_dir": f'"{remote_script_dir}"',
        }
    )

    try:
        logger.debug(f"")
        logger.debug(textwrap.indent(str(submit_description), "  "))
        submit_result = schedd.submit(submit_description)
    except Exception:
        raise RuntimeError(f"Failed to submit state-tracking job, aborting.")

    cluster_id = submit_result.cluster()
    logger.debug(f"... done.")
    logger.debug(f"with cluster ID {cluster_id}.")

    results = schedd.query(
        f'ClusterID == {cluster_id} && ProcID == 0',
        opts=htcondor.QueryOpts.DefaultMyJobsOnly,
        projection=["GlobalJobID"],
        )
    request_id = results[0]["GlobalJobID"]

    ##
    ## We changed the job(s) at submit time to prevent them from running
    ## anywhere other than the annex, so it's OK to change them again to
    ## make it impossible to run them anywhere else before the annex job
    ## is successfully submitted.  Doing so after allows for a race
    ## condition, so let's not, since we don't hvae to.
    ##
    ## Change the jobs so that they don't transfer the .sif files we just
    ## pre-staged.
    ##
    ## The startd can't rewrite the job ad the shadow uses to decide
    ## if it should transfer the .sif file, but it can change ContainerImage
    ## to point the pre-staged image, if it's just the basename.  (Otherwise,
    ## it gets impossibly difficult to make the relative paths work.)
    ##
    ## We could do this at job-submission time, but then we'd have to record
    ## the full path to the .sif file in some other job attribute, so it's
    ## not worth changing at this point.
    ##
    for job_ad in annex_jobs:
        job_id = f'{job_ad["ClusterID"]}.{job_ad["ProcID"]}'
        sif_file = extract_sif_file(job_ad)
        if sif_file is not None:
            remote_sif_file = Path(sif_file).name
            logger.debug(
                f"Setting ContainerImage = {remote_sif_file} in annex job {job_id}"
            )
            schedd.edit(job_id, "ContainerImage", f'"{remote_sif_file}"')

            transfer_input = job_ad.get("TransferInput", "")
            if isinstance(transfer_input, classad.Value):  # UNDEFINED or ERROR
                transfer_input = ""
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

    remotes = {}
    logger.info(f"Requesting annex named '{annex_name}' from queue '{queue_name}' on the system named '{SYSTEM_TABLE[system].pretty_name}'...\n")
    rc = invoke_pilot_script(
        ssh_connection_sharing,
        ssh_user_name,
        ssh_host_name,
        remote_script_dir,
        system,
        startd_noclaim_shutdown,
        annex_name,
        queue_name,
        collector,
        token_file,
        lifetime,
        owners,
        nodes,
        allocation,
        lambda attribute, value: updateJobAd(cluster_id, attribute, value, remotes),
        request_id,
        password_file,
        cpus,
        mem_mb,
        gpus,
        gpu_type,
    )

    if rc == 0:
        logger.info(f"... requested.")
        logger.info(f"\nIt may take some time for the system named '{SYSTEM_TABLE[system].pretty_name}' to establish the requested annex.")
        logger.info(f"To check on the status of the annex, run 'htcondor annex status {annex_name}'.")
    else:
        error = f"Failed to start annex, {SYSTEM_TABLE[system].batch_system} returned code {rc}"
        try:
            schedd.act(htcondor.JobAction.Remove, f'ClusterID == {cluster_id}', error)
        except Exception:
            logger.warn(f"Could not remove cluster ID {cluster_id}.")
        raise RuntimeError(error)


def annex_name_exists(annex_name):
    schedd = htcondor.Schedd()
    constraint = f'hpc_annex_name == "{annex_name}"'
    annex_jobs = schedd.query(
        constraint,
        opts=htcondor.QueryOpts.DefaultMyJobsOnly,
        projection=['ClusterID', 'ProcID'],
    )
    return len(annex_jobs) > 0


def annex_create(logger, annex_name, **others):
    if others.get("test") is not True:
        if annex_name_exists(annex_name):
            raise ValueError(f"You've already created an annex named '{annex_name}'.  To request more resources, use 'htcondor annex add'.")
    return annex_inner_func(logger, annex_name, **others)


def annex_add(logger, annex_name, **others):
    if others.get("test") is not True:
        if not annex_name_exists(annex_name):
            raise ValueError(f"You need to create an an annex named '{annex_name}' first.  To do so, use 'htcondor annex create'.")
    return annex_inner_func(logger, annex_name, **others)
