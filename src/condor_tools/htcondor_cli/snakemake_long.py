#!/usr/bin/env python3

"""
A wrapper script that allows long-term Snakemake workflows to run on HTCondor. This works
by submitting a local universe job responsible for overseeing the terminal session that
runs the actual snakemake executable.
"""

import argparse
import os
import pathlib
import subprocess
import sys
import time

import htcondor

"""
Parse various arguments for the script. Note that this script has two "modes" of operation which
need different arguments. The "top" mode is for submitting the HTCondor wrapper, and the "long" mode
is for running the Snakemake command itself.
"""
def parse_args(isLocal=False):
    parser = argparse.ArgumentParser(description="A tool for long-running Snakemake jobs with HTCondor.")
    if isLocal:
        # We add a special command that allows this singular executable to serve two purposes. The executable
        # is first run by the user with their args to submit the local universe job. Then, the local universe
        # job runs `snakemake_long.py long <user args>` to indicate to the script that it's time to submit the
        # long-running Snakemake process instead of submitting another local universe job.
        parser.add_argument("command", help="Helper command to run", choices=["long"])
    parser.add_argument("--snakefile", help="The Snakefile to run. If omitted, the Snakefile is assumed to be in the current directory.", required=False)
    parser.add_argument("--profile", help="A path to a directory containing the desired Snakemake profile.", required=True)
    # I'd love to change this to "logdir", but using the same name as Snakemake for consistency of feeling between this script
    # and Snakemake proper.
    parser.add_argument("--htcondor-jobdir", help="The directory Snakemake will write logs to. If omitted, a 'logs` directory will be created in the current directory", required=False)
    return parser.parse_args()

"""
Given a Snakefile, profile, and HTCondor job directory, submit a local universe job that runs
Snakemake from the context of the submission directory.
"""
def submit_local(snakefile, profile, htcondor_jobdir):
    # Get the location of this script, which also serves as the executable for the condor job.
    script_location = pathlib.Path(__file__).resolve()

    submit_description = htcondor.Submit({
        "executable":              script_location,
        # We use the "long" command to indicate to the script that it should run the Snakemake command instead of submitting another job.
        # See comment in parse_args for more information.
        "arguments":               f"long --snakefile {snakefile} --profile {profile} --htcondor-jobdir {htcondor_jobdir}",
        "universe":                "local",
        "request_disk":            "512MB",
        "request_cpus":            1,
        "request_memory":          512,

        # Set up logging
        "log":                     f"{htcondor_jobdir}/snakemake.log",
        "output":                  f"{htcondor_jobdir}/snakemake.out",
        "error":                   f"{htcondor_jobdir}/snakemake.err",

        # Specify `getenv` so that our script uses the appropriate environment
        # when it runs in local universe. This allows the job to access
        # modules we've installed in the submission environment (notably spras).
        "getenv":                  "true",

        "JobBatchName":            f"spras-{time.strftime('%Y%m%d-%H%M%S')}",
    })

    schedd = htcondor.Schedd()
    submit_result = schedd.submit(submit_description)

    print("Snakemake management job was submitted with JobID %d.0. Logs can be found in %s" % (submit_result.cluster(), htcondor_jobdir))

"""
The top level function for the script that handles file creation/validation and triggers submission of the
wrapper job.
"""
def top_main():
    args = parse_args()

    # Check if the snakefile is provided. If not, assume it's in the current directory.
    if args.snakefile is None:
        cwd = os.getcwd()
        args.snakefile = pathlib.Path(cwd) / "Snakefile"
    if not os.path.exists(args.snakefile):
        raise FileNotFoundError(f"Error: The Snakefile {args.snakefile} does not exist.")

    # Make sure the profile directory exists. It's harder to check if it's a valid profile at this level
    # so that will be left to Snakemake.
    if not os.path.exists(args.profile):
        raise FileNotFoundError(f"Error: The profile directory {args.profile} does not exist.")

    # Make sure we have a value for the log directory and that the directory exists.
    if args.htcondor_jobdir is None:
        args.htcondor_jobdir = pathlib.Path(os.getcwd()) / "snakemake-long-logs"
        if not os.path.exists(args.htcondor_jobdir):
            os.makedirs(args.htcondor_jobdir)
    else:
        if not os.path.exists(args.htcondor_jobdir):
            os.makedirs(args.htcondor_jobdir)

    try:
        submit_local(args.snakefile, args.profile, args.htcondor_jobdir)
    except Exception as e:
        print(f"Error: Could not submit local universe job. {e}")
        raise

"""
Command to activate conda environment and run Snakemake. This is run by the local universe job, not the user.
"""
def long_main():
    args = parse_args(True)

    # Note that we need to unset APPTAINER_CACHEDIR in this case but not in the local terminal case because the wrapper
    # HTCondor job has a different environment and populating this value causes Snakemake to fail when it tries to write
    # to spool (a read-only filesystem from the perspective of the EP job).
    command = f"""
    source $(conda info --base)/etc/profile.d/conda.sh && \
    conda activate spras && \
    unset APPTAINER_CACHEDIR && \
    snakemake -s {args.snakefile} --profile {args.profile} --htcondor-jobdir {args.htcondor_jobdir}
    """

    try:
        subprocess.run(command, shell=True, executable='/bin/bash', check=True)
        return 0
    except subprocess.CalledProcessError as e:
        print(f"Error: Command '{e.cmd}' returned non-zero exit status {e.returncode}.")
        raise
    except Exception as e:
        print(f"Unexpected error: {e}")
        raise

def main():
    try:
        if len(sys.argv) > 1 and sys.argv[1] in ["long"]:
            return long_main()
        else:
            top_main()

    except subprocess.CalledProcessError as e:
        print(f"Error: Snakemake failed with return code {e.returncode}.")
        sys.exit(e.returncode)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()