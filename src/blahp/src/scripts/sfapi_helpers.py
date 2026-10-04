from sfapi_client import Client, StatusValue
from sfapi_client.compute import Machine

import argparse
import shutil
import sys
from pathlib import Path

import base64
from datetime import datetime, timezone
import json
from io import BytesIO
import os

# Module-level credential globals; set by load_sflapi_client_secret() or
# injected externally (e.g. from sfapi_submit.sh inline Python).
client_id = None
client_secret = None

_DEFAULT_PRIVATE_JWK_KEY_FILE = Path.home() / ".superfacility" / "priv_key.jwk"
_DEFAULT_CLIENT_ID_FILE = Path.home() / ".superfacility" / "clientid.txt"


class SfApiHelperError(Exception):
    """Raised for errors encountered in sfapi_helpers operations."""

    pass


def submit_remote_slurm_job(job_name, job_script, input_files):
    """
    Submit a Slurm batch job to Perlmutter at NERSC via the SFAPI.

    Creates a remote working directory under the user's scratch filesystem,
    uploads all specified input files, prepends #SBATCH --output, --error,
    and --chdir directives to the job script, then submits it.

    :param job_name:    Name for the job and remote working directory.
                        Defaults to 'bl_sfapi' if None.
    :param job_script:  Content of the SBATCH batch script to submit.
    :param input_files: List of local file paths to upload to the remote
                        working directory before submission.
    :return: Tuple of (job_id, remote_dir, remote_stdout_path, remote_stderr_path).
    """
    with Client(client_id, client_secret) as client:
        # first figure out a job directory on the remote side
        # on the scratch file system
        if job_name is None:
            job_name = "bl_sfapi"
        remote_dir = create_remote_blahp_directory(name=job_name)

        # upload input files that a job requires
        for file in input_files:
            upload_file(remote_dir, file)

        remote_stdout = f"{remote_dir}/{job_name}.out"
        remote_stderr = f"{remote_dir}/{job_name}.err"

        # specify the remote_stdout and remote_stderr
        # and directory where the job should run in to the job script
        job_script = (
            f"""#!/bin/bash

#SBATCH --output={remote_stdout}
#SBATCH --error={remote_stderr}
#SBATCH --chdir={remote_dir}
"""
            + job_script
        )

        print("Submitting job \n" + job_name)

        perlmutter = client.compute(Machine.perlmutter)
        # Jobs can be submitted from
        job = perlmutter.submit_job(job_script)
        # Let's save the job id to use later
        job_id = job.jobid
        print(f"Started {job_id} with stdout: {remote_stdout} stderr: {remote_stderr}")
        return job_id, remote_dir, remote_stdout, remote_stderr


def retrieve_remote_stdout_stderr(job_id, remote_stdout, remote_stderr):
    """
    Wait for a job to complete then download its stdout and stderr files.

    Blocks until the job reaches a terminal state, then downloads the remote
    output files to <job_id>.out and <job_id>.err in the current directory.

    :param job_id:        Numeric Slurm job ID.
    :param remote_stdout: Absolute path to the job's stdout file on Perlmutter.
    :param remote_stderr: Absolute path to the job's stderr file on Perlmutter.
    """
    with Client(client_id, client_secret) as client:
        perlmutter = client.compute(Machine.perlmutter)
        job = perlmutter.job(jobid=job_id)
        job.complete()

    download_file(remote_stdout, f"{job_id}.out")
    download_file(remote_stderr, f"{job_id}.err")


def download_file(source, destination):
    """
    Download a remote file from Perlmutter to a local destination path.
    Raises SfApiHelperError if the remote path does not exist or is not a file.
    """
    with Client(client_id, client_secret) as client:
        perlmutter = client.compute(Machine.perlmutter)
        listing = perlmutter.ls(source)
        if not listing:
            raise SfApiHelperError(f"File not found on remote: {source}")
        [remote_file] = listing
        if not remote_file.is_file():
            raise SfApiHelperError(f"File not found on remote: {source}")
        buffer = remote_file.download()

        if isinstance(buffer, BytesIO):
            with open(destination, "wb") as f:
                f.write(buffer.getbuffer())
            print(f"Downloaded Binary File: {os.path.abspath(destination)}")
        else:
            # StringIO
            with open(destination, "w") as f:
                shutil.copyfileobj(buffer, f)
                print(f"Downloaded File: {os.path.abspath(destination)}")


def download_job_outputs(blahp_job_id):
    """
    Given a blahp job ID of the form sfapi/<date>/<jobid>, locate the
    corresponding jobstate file at ~/.blah/sfapi_jobs/<date>_<jobid>,
    parse each entry, and download every remote output file to its
    local destination path.

    The jobstate file is written by sfapi_submit.sh and has one entry
    per line in the format:
        <type>::<local_path>:<remote_path>
    e.g.:
        # type::<local file on submit host>:<remote file to retrieve via sfapi>
        remote_dir::/pscratch/sd/v/vahi/.blah/bl_dGdwlN
        stdout::/pegasus/hello-world/run0032//00/00/hello_ID0000001.out:/pscratch/sd/v/vahi/.blah/bl_dGdwlN/bl_dGdwlN.out
        stderr::/pegasus/hello-world/run0032//00/00/hello_ID0000001.err:/pscratch/sd/v/vahi/.blah/bl_dGdwlN/bl_dGdwlN.err
        output::/pegasus/wf-scratch/LOCAL/scitech/pegasus/hello-world/run0032/f.inter:/pscratch/sd/v/vahi/.blah/bl_dGdwlN/f.inter

    """
    parts = blahp_job_id.strip().split("/")
    if len(parts) != 3 or parts[0] != "sfapi":
        raise ValueError(
            f"Invalid blahp job ID {blahp_job_id!r}. Expected sfapi/<date>/<jobid>"
        )
    date_str, job_id = parts[1], parts[2]

    jobstate_file = Path.home() / ".blah" / "sfapi_jobs" / f"{date_str}_{job_id}"
    if not jobstate_file.exists():
        raise FileNotFoundError(f"Job state file not found: {jobstate_file}")

    for line in jobstate_file.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "::" not in line:
            continue
        # Split into type tag and the path pair
        type, paths = line.split("::", 1)
        if type == "remote_dir":
            continue

        # Both paths are absolute so splitting on the first ':' is unambiguous
        local_path, remote_path = paths.split(":", 1)
        print(f"Downloading {remote_path} -> {local_path}")
        download_file(remote_path, local_path)


def delete_remote_job_directory(blahp_job_id):
    """
    Given a blahp job ID of the form sfapi/<date>/<jobid>, locate the
    corresponding jobstate file at ~/.blah/sfapi_jobs/<date>_<jobid>,
    read the remote_dir entry, and delete that directory on Perlmutter
    via the NERSC DTNs.

    :param blahp_job_id: Blahp job ID in the form sfapi/<date>/<jobid>.
    :raises ValueError:       If the blahp job ID format is invalid.
    :raises FileNotFoundError: If the jobstate file does not exist.
    :raises SfApiHelperError: If no remote_dir entry is found in the jobstate file.
    """
    parts = blahp_job_id.strip().split("/")
    if len(parts) != 3 or parts[0] != "sfapi":
        raise ValueError(
            f"Invalid blahp job ID {blahp_job_id!r}. Expected sfapi/<date>/<jobid>"
        )
    date_str, job_id = parts[1], parts[2]

    jobstate_file = Path.home() / ".blah" / "sfapi_jobs" / f"{date_str}_{job_id}"
    if not jobstate_file.exists():
        raise FileNotFoundError(f"Job state file not found: {jobstate_file}")

    remote_dir = None
    for line in jobstate_file.read_text().splitlines():
        line = line.strip()
        if line.startswith("remote_dir::"):
            _, remote_dir = line.split("::", 1)
            remote_dir = remote_dir.strip()
            break

    if not remote_dir:
        raise SfApiHelperError(
            f"No remote_dir entry found in jobstate file: {jobstate_file}"
        )

    with Client(client_id, client_secret) as client:
        dtns = client.compute(Machine.dtns)
        dtns.run(f"rm -rf {remote_dir}")
    print(f"Deleted remote directory {remote_dir}")


def _cmd_delete(args):
    """Handler for the 'delete' subcommand."""
    load_sflapi_client_secret()
    delete_remote_job_directory(args.blahp_job_id)


def upload_file(directory, file):
    """
    Upload a local file to a remote directory on Perlmutter via the NERSC DTNs.

    :param directory: Absolute remote path of the target directory on Perlmutter.
    :param file:      Local path of the file to upload.
    :return:          The remote path of the uploaded file as returned by the SFAPI.
    """
    with Client(client_id, client_secret) as client:
        dtns = client.compute(Machine.dtns)

        [remote_dir] = dtns.ls(directory, directory=True)
        # Check that the directory is there
        if remote_dir.is_dir():
            with open(file, "rb") as fh:
                buf = BytesIO(fh.read())
                # Give our BytesIO file a filename to upload to
                buf.filename = os.path.basename(file)
                uploaded_file = remote_dir.upload(buf)
                print(f"Uploaded file to {uploaded_file}")
                # print(f"Now there's {len(dtns.ls(remote_dir))} files in the directory")

    return uploaded_file


def create_remote_blahp_directory(name):
    """
    Create a job working directory under the user's Perlmutter scratch filesystem.

    The directory is created at $PSCRATCH/.blah/<name> using the NERSC DTNs.

    :param name: Directory name (typically the blahp job name / temp file basename).
    :return:     Absolute remote path of the created directory.
    """
    with Client(client_id, client_secret) as client:
        # retrieve the remote user at perlmutter to which
        # the key maps to
        user = client.user()

        #remote_home = f"/global/homes/{user.name[0]}/{user.name}"
        remote_scratch = f"/pscratch/sd/{user.name[0]}/{user.name}"

        # we create it on remote_scratch

        dtns = client.compute(Machine.dtns)
        remote_dir = f"{remote_scratch}/.blah/{name}"
        # This will run a command on dtns, here we use `mkdir` to make our output directory
        dtns.run(f"mkdir -p {remote_dir}")
        return remote_dir


def check_nersc_status(resource_name):
    """
    Check whether a NERSC resource is active via the SFAPI.

    Prints a confirmation message if the resource is active.
    Raises RuntimeError if the resource is in any non-active state.

    :param resource_name: NERSC resource name to query (e.g. 'perlmutter').
    :raises RuntimeError: If the resource is not active.
    """
    with Client() as client:
        resource_status = client.resources.status(resource_name)
    if resource_status.status == StatusValue.active:
        print(f"Resource {resource_name} is active")
    else:
        raise RuntimeError(
            f"Resource {resource_name} is not active: {resource_status.description}"
        )


def check_job_status(jobid):
    """
    Query and print the current Slurm state of a job on Perlmutter.

    Loads SFAPI credentials, queries Perlmutter for the job, and prints
    a line of the form "Job <jobid> state: <state>".

    :param jobid: Numeric Slurm job ID to query.
    """
    load_sflapi_client_secret()
    with Client(client_id, client_secret) as client:
        perlmutter = client.compute(Machine.perlmutter)
        job = perlmutter.job(jobid=jobid)
    print(f"Job {jobid} state: {job.state}")


def check_token_validity(key_path):
    """
    Check whether an SFAPI private key can successfully authenticate with NERSC.

    Loads the JWK private key from ``key_path`` and the client ID from
    file named ``clientid.txt`` from the directory where key exists,
    then calls ``Client.token`` to fetch a bearer token from the NERSC
    OIDC endpoint.  A successful fetch confirms that the key is well-formed,
    the client ID matches, and the credentials are accepted by NERSC — without
    duplicating any REST calls directly.

    :param key_path: Path to the JWK private key file
                       (e.g. ~/.superfacility/priv_key.jwk).
    :raises SfApiHelperError: If the key file is missing or unparsable, the
                              client ID file is missing, or authentication fails.
    """

    if not key_path.exists():
        raise SfApiHelperError(f"Token key file not found: {key_path}")

    # Load the matching client ID from the same location as where key exists
    client_id_file = Path(key_path).parent / "clientid.txt"
    cid, key = load_sflapi_client_secret(key_path, client_id_file)

    # Attempt to fetch a bearer token — this is the sfapi_client's own
    # mechanism for verifying credentials without calling REST directly.
    try:
        with Client(cid, key) as client:
            bearer = client.token
    except Exception as e:
        raise SfApiHelperError(
            f"Token authentication failed for client_id {cid} using key {key_path}: {e}"
        )

    print("Token is valid.")

    # Decode the JWT payload (middle segment) to read the exp claim.
    # No signature verification is needed here — we just fetched this token
    # successfully from NERSC, so we know it's genuine.
    try:
        payload_b64 = bearer.split(".")[1]
        # Restore base64url padding before decoding.
        payload_b64 += "=" * (4 - len(payload_b64) % 4)
        payload = json.loads(base64.urlsafe_b64decode(payload_b64))

        exp_ts = payload.get("exp")
        if exp_ts is not None:
            exp_dt = datetime.fromtimestamp(exp_ts, tz=timezone.utc)
            now = datetime.now(tz=timezone.utc)
            delta = exp_dt - now
            total_seconds = int(delta.total_seconds())

            if total_seconds <= 0:
                print(
                    f"Token EXPIRED {-total_seconds // 60}m {-total_seconds % 60}s ago "
                    f"(at {exp_dt.strftime('%Y-%m-%d %H:%M:%S %Z')})"
                )
            else:
                hours, remainder = divmod(total_seconds, 3600)
                minutes, seconds = divmod(remainder, 60)
                if hours > 0:
                    human = f"{hours}h {minutes}m {seconds}s"
                else:
                    human = f"{minutes}m {seconds}s"
                print(
                    f"Token expires in {human} "
                    f"(at {exp_dt.strftime('%Y-%m-%d %H:%M:%S %Z')})"
                )
        else:
            print("Token has no expiry claim (exp not present in JWT payload)")
    except Exception:
        # Non-fatal: we already confirmed the token is valid above.
        print("Token is valid (could not decode expiry from JWT payload)")


def print_nersc_status():
    """
    Print the current status of all NERSC resources to stdout.

    Uses public SFAPI access (no credentials required) and prints one line
    per resource in the format: name | description | status.
    """
    with Client() as client:
        nersc_status = client.resources.status()

    # For each of the resources print the status
    for name, status in nersc_status.items():
        print(f"{name: <22}| {status.description: <25}| {status.status}")


def load_sflapi_client_secret(key_path=None, clientid_path=None):
    """
    Loads the SFAPI client_id and client_secret from ~/.superfacility/ and
    sets them as module globals so all helper functions can use them.

    :param key_path: Path to the JWK private key file
                       (defaults to ~/.superfacility/priv_key.jwk).

    :param clientid_path: Path to the JWK private key file
                       (defaults to ~/.superfacility/clientid.txt).

    :return: client_id, client_secret
    """
    global client_id, client_secret

    if key_path is None:
        key_path = _DEFAULT_PRIVATE_JWK_KEY_FILE

    if clientid_path is None:
        clientid_path = _DEFAULT_CLIENT_ID_FILE

    if not key_path.exists():
        raise SfApiHelperError(f"JSON Web Token Key file not found: {key_path}")

    if not clientid_path.exists():
        raise SfApiHelperError(f"Clientid file not found: {clientid_path}")

        # Parse the JWK to catch format errors before hitting the network.
    try:
        with open(key_path, "r") as f:
            client_secret = json.load(f)
    except (json.JSONDecodeError, ValueError, KeyError) as e:
        raise SfApiHelperError(f"Failed to parse token key file {key_path}: {e}")

    client_id = clientid_path.read_text().strip()
    # print(f"User client id for superfacility is {client_id}")

    with open(key_path, "r") as f:
        client_secret = json.load(f)

    return client_id, client_secret


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main():
    """
    Demo entry point: print NERSC status, list project allocations, and
    submit a small test job that generates random numbers on Perlmutter.
    """
    print_nersc_status()
    client_id, client_secret = load_sflapi_client_secret()

    # all the projects user has
    N = 10000
    with Client(client_id, client_secret) as client:
        # Get the user info, "Who does the api think I am?"
        user = client.user()
        print(user)
        projects = user.projects()

    print("Project name |        Hours Given | Hours Remaining")
    for project in projects:
        print("=" * 51)
        print(
            f"{project.repo_name: <13}| {project.hours_given:>12.2f} Hours | {project.hours_given - project.hours_used:>8.2f} Hours"
        )

    # create an input file
    input_file = "hello.in"
    with open(input_file, "w") as file:
        file.write("Hello, world!\n")
        file.write("This is a second line.")

    account = project.name
    qos = "debug"

    """
    #SBATCH --chdir={scratch}/sfapi-demo
    """

    job_script = f"""
#SBATCH -q {qos}
#SBATCH -A {account}
#SBATCH -N 1
#SBATCH -n 1
#SBATCH -C cpu
#SBATCH -t 00:02:00
#SBATCH -J sfapi-demo

module load python
# Prints N random numbers to form a normal disrobution
python -c "import numpy as np; numbers = np.random.normal(size={N}); [print(n) for n in numbers]"
    """
    job_id, remote_stdout, remote_stderr = submit_remote_slurm_job(
        None, job_script, [input_file]
    )
    retrieve_remote_stdout_stderr(job_id, remote_stdout, remote_stderr)


def _cmd_submit(args):
    """Handler for the 'submit' subcommand."""
    global client_id, client_secret
    client_id, client_secret = load_sflapi_client_secret()

    # Read the job script from a file or stdin
    if args.script:
        with open(args.script) as fh:
            job_script = fh.read()
    else:
        job_script = sys.stdin.read()

    # Parse comma-separated input files, stripping whitespace around each entry
    input_files = []
    if args.input_files:
        input_files = [f.strip() for f in args.input_files.split(",") if f.strip()]

    job_id, remote_dir, remote_stdout, remote_stderr = submit_remote_slurm_job(
        args.job_name, job_script, input_files
    )

    # Print in the format expected by sfapi_submit.sh
    print(f"SFAPI_RESULT:{job_id}:{remote_dir}:{remote_stdout}:{remote_stderr}")


def cancel_job(job_id):
    """
    Cancel a job on Perlmutter via the SFAPI.

    Accepts either the bare numeric Slurm job ID or a blahp job ID of the
    form sfapi/<date>/<jobid>; the blahp prefix is stripped automatically.
    """
    # Accept both "sfapi/20240506/12345" and plain "12345"
    numeric_id = job_id.strip().split("/")[-1]

    with Client(client_id, client_secret) as client:
        perlmutter = client.compute(Machine.perlmutter)
        job = perlmutter.job(jobid=numeric_id)
        job.cancel()
    print(f"Cancelled job {numeric_id}")


def _cmd_cancel(args):
    """Handler for the 'cancel' subcommand."""
    load_sflapi_client_secret()
    cancel_job(args.job_id)


def _cmd_status(args):
    """Handler for the 'status' subcommand."""
    if args.type == "resource":
        if not args.value:
            raise SystemExit("error: --value is required when --type=resource")
        check_nersc_status(args.value)
    elif args.type == "job":
        if not args.value:
            raise SystemExit("error: --value is required when --type=job")
        check_job_status(args.value)
    elif args.type == "key":
        token_path = args.value if args.value else _DEFAULT_PRIVATE_JWK_KEY_FILE
        check_token_validity(token_path)


"""
if __name__ == '__main__':
    main()
"""

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="NERSC SFAPI helpers for blahp remote job submission"
    )
    subparsers = parser.add_subparsers(dest="command", metavar="<command>")
    subparsers.required = True

    # --- submit subcommand ---
    sp = subparsers.add_parser(
        "submit",
        help="Submit a job script to Perlmutter via SFAPI",
        description=(
            "Upload input files to a remote scratch directory on Perlmutter "
            "and submit the given SBATCH job script via the NERSC SFAPI."
        ),
    )
    sp.add_argument(
        "-n",
        "--job-name",
        metavar="NAME",
        default=None,
        help="Job name used for the remote working directory and output files "
        "(default: auto-generated)",
    )
    sp.add_argument(
        "-i",
        "--input-files",
        metavar="FILE1,FILE2,...",
        default=None,
        help="Comma-separated list of local files to upload to the remote job "
        "directory before submission",
    )
    sp.add_argument(
        "-s",
        "--script",
        metavar="SCRIPT",
        default=None,
        help="Path to the SBATCH job script to submit (reads from stdin when omitted)",
    )

    # --- status subcommand ---
    st = subparsers.add_parser(
        "status",
        help="Check the status of a NERSC resource or a submitted job",
        description=(
            "Query the NERSC SFAPI for the status of a system resource "
            "(e.g. perlmutter) or a specific job by ID."
        ),
    )
    st.add_argument(
        "-t",
        "--type",
        metavar="TYPE",
        choices=["resource", "job", "key"],
        required=True,
        help="What to query: 'resource' to check a NERSC system status, "
        "'job' to check a submitted job state, "
        "'key' to verify that an SFAPI private key can authenticate",
    )
    st.add_argument(
        "-v",
        "--value",
        metavar="VALUE",
        default=None,
        help="Resource name (e.g. 'perlmutter') when --type=resource; "
        "job ID when --type=job; "
        "path to the JWK private key file when --type=key; "
        f"(default: {_DEFAULT_PRIVATE_JWK_KEY_FILE})",
    )

    # --- download subcommand ---
    dl = subparsers.add_parser(
        "download",
        help="Download output files for a completed job",
        description=(
            "Read the jobstate file written by sfapi_submit.sh for the given "
            "blahp job ID and download all remote output files to their local "
            "destination paths."
        ),
    )
    dl.add_argument(
        "blahp_job_id",
        metavar="BLAHP_JOB_ID",
        help="Blahp job ID in the form sfapi/<date>/<jobid>",
    )

    # --- cancel subcommand ---
    ca = subparsers.add_parser(
        "cancel",
        help="Cancel a running or queued job on Perlmutter",
        description=(
            "Cancel a job by its blahp job ID (sfapi/<date>/<jobid>) "
            "or bare numeric Slurm job ID."
        ),
    )
    ca.add_argument(
        "job_id",
        metavar="JOB_ID",
        help="Blahp job ID (sfapi/<date>/<jobid>) or bare numeric Slurm job ID",
    )

    # --- delete subcommand ---
    de = subparsers.add_parser(
        "delete",
        help="Delete the remote working directory for a completed job",
        description=(
            "Read the jobstate file for the given blahp job ID, extract the "
            "remote_dir entry written at submit time, and remove that directory "
            "from Perlmutter scratch via the NERSC DTNs."
        ),
    )
    de.add_argument(
        "blahp_job_id",
        metavar="BLAHP_JOB_ID",
        help="Blahp job ID in the form sfapi/<date>/<jobid>",
    )

    args = parser.parse_args()
    if args.command == "submit":
        _cmd_submit(args)
    elif args.command == "status":
        _cmd_status(args)
    elif args.command == "download":
        load_sflapi_client_secret()
        download_job_outputs(args.blahp_job_id)
    elif args.command == "cancel":
        _cmd_cancel(args)
    elif args.command == "delete":
        _cmd_delete(args)
