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

import htcondor2 as htcondor
import classad2 as classad

from htcondor_cli.annex_validate import SYSTEM_TABLE, validate_constraints

#
# We could try to import colorama here -- see condor_watch_q -- but that's
# only necessary for Windows.
#

INITIAL_CONNECTION_TIMEOUT = int(htcondor.param.get("ANNEX_INITIAL_CONNECTION_TIMEOUT", 180))
REMOTE_CLEANUP_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_CLEANUP_TIMEOUT", 60))
REMOTE_MKDIR_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_MKDIR_TIMEOUT", 30))
REMOTE_POPULATE_TIMEOUT = int(htcondor.param.get("ANNEX_REMOTE_POPULATE_TIMEOUT", 60))
TOKEN_FETCH_TIMEOUT = int(htcondor.param.get("ANNEX_TOKEN_FETCH_TIMEOUT", 20))


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
    files = [
        f"-C {local_script_dir} {SYSTEM_TABLE[system].executable}",
        f"-C {local_script_dir} {system}.fragment",
        f"-C {str(token_file.parent)} {token_file.name}",
        f"-C {str(password_file.parent)} {password_file.name}",
        * [
            f"-C {local_script_dir} {other_script}" for other_script in SYSTEM_TABLE[system].other_scripts
        ]
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
    schedd_name,
):
    ssh_target = ssh_host_name
    if ssh_user_name is not None:
        ssh_target = f"{ssh_user_name}@{ssh_host_name}"

    gpu_argument = str(gpus)
    if gpu_type is not None:
        gpu_argument = f"{gpu_type}:{gpus}"
    args = [
        "ssh",
        *ssh_connection_sharing,
        ssh_target,
        str(remote_script_dir / SYSTEM_TABLE[system].executable),
        system,
        str(startd_noclaim_shutdown),
        annex_name,
        queue_name,
        collector,
        str(remote_script_dir / token_file.name),
        str(lifetime),
        str(remote_script_dir / SYSTEM_TABLE[system].other_scripts[0]),
        owners,
        str(nodes),
        str(remote_script_dir / SYSTEM_TABLE[system].other_scripts[1]),
        str(allocation),
        request_id,
        str(remote_script_dir / password_file.name),
        schedd_name,
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
    schedd.edit(str(cluster_id), f"hpc_annex_{attribute}", f'"{value}"')
    remotes[attribute] = value


def extract_sif_file(job_ad):
    transfer_container = job_ad.get("TransferContainer")
    if not transfer_container:
        return None

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
        # For direct attach.
        '-authz', 'WRITE',
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

    gpus, real_queue_name, mem_mb = validate_constraints(
        system=system,
        queue_name=queue_name,
        nodes=nodes,
        lifetime_in_seconds=lifetime_in_seconds,
        allocation=allocation,
        cpus=cpus,
        mem_mb=mem_mb,
        idletime_in_seconds=idletime_in_seconds,
        gpus=gpus,
        gpu_type=gpu_type,
    )

    if test is not None and test == 1:
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

    #
    # The pilot needs to know the name of the schedd so it can look the schedd
    # up in the annex collector.  Since it's hard to extract the name of the
    # schedd from the schedd itself (?!), we might as well ask the annex collector
    # ourselves and verify that the pilot will get an answer.
    #
    if test is None:
        c = htcondor.Collector(collector)
        r = c.query(ad_type=htcondor.AdTypes.Schedd)
        if len(r) != 1:
            raise RuntimeError("Found more than one scheduler in the AP collector; this configuration is not supported.  Contact your administrator.")
        schedd_name = r[0].get('name')
        if schedd_name is None:
            raise RuntimeError("Scheduler ad in AP collector did not contain the scheduler's name.")


    ##
    ## While we're requiring that jobs are submitted before creating the
    ## annex (for .sif pre-staging purposes), refuse to make the annex
    ## if no such jobs exist.
    ##
    schedd = htcondor.Schedd()
    annex_jobs = schedd.query(f'TargetAnnexName == "{annex_name}"')

    enable_job_check = htcondor.param.get('HPC_ANNEX_REQUIRE_JOB')
    if enable_job_check is None or enable_job_check.casefold() != 'FALSE'.casefold():
        if not annex_jobs:
            raise RuntimeError(
                f"No jobs for '{annex_name}' are in the queue. Use 'htcondor job submit --annex-name' to add them first."
            )
        logger.debug(
            f"""Found {len(annex_jobs)} annex jobs matching 'TargetAnnexName == "{annex_name}"."""
        )

    if test == 2:
        return

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
    if resources == "":
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
        real_queue_name,
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
        schedd_name,
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
    if others.get("test") is None:
        if annex_name_exists(annex_name):
            raise ValueError(f"You've already created an annex named '{annex_name}'.  To request more resources, use 'htcondor annex add'.")
    return annex_inner_func(logger, annex_name, **others)


def annex_add(logger, annex_name, **others):
    if others.get("test") is None:
        if not annex_name_exists(annex_name):
            raise ValueError(f"You need to create an an annex named '{annex_name}' first.  To do so, use 'htcondor annex create'.")
    return annex_inner_func(logger, annex_name, **others)
