import argparse
import htcondor2 as htcondor
import shutil
import subprocess
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
import traceback


"""
HTCondor CLI for running Snakemake workflow
"""
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

class Submit(Verb):
    """
    Submit a local universe job and Snakemake jobs when run.
    """
    # command-line argument configurations
    options = {
        "jobdir": {
            "args": ("--jobdir",),
            "help": "Optional directory for HTCondor management job log files. If omitted, a `logs` directory will be created at the current directory.",
            "required": False,
        },
        "snakemake_args": {
            "args": ("snakemake_args",),
            "nargs": argparse.REMAINDER,
            "help": "[snakefile] [-- [snakemake args...]]  Optionally specify a Snakefile as the first positional argument. Use -- to pass remaining arguments directly to snakemake.",
        },
    }

    def __init__(self, logger, snakefile=None, snakemake_args=None, **options):
        snakemake_args = list(snakemake_args or [])

        # snakefile is only provided when called directly (e.g. from tests).
        # When invoked via the CLI, everything lands in snakemake_args via REMAINDER.
        # We split that list here: the first non-flag arg (before any --) is the
        # snakefile; the -- separator and everything after it go to snakemake.
        if snakefile is None:
            if snakemake_args and snakemake_args[0] == '--':
                # No snakefile given; strip the separator
                snakemake_args = snakemake_args[1:]
            else:
                if snakemake_args and not snakemake_args[0].startswith('-'):
                    snakefile = snakemake_args.pop(0)
                # Strip the -- separator that may follow the snakefile
                if snakemake_args and snakemake_args[0] == '--':
                    snakemake_args = snakemake_args[1:]

        # Basic validations of CLI
        snakefile = self._validate_snakefile(snakefile)
        jobdir = self._setup_jobdir(options)

        # submit a local universe job
        try:
            self._submit_local(snakefile, jobdir, snakemake_args or [])
        except Exception as e:
            print("Error: Could not submit local universe job.")
            print(f"Exception details:", str(e))
            traceback.print_exc()
            raise

    def _validate_snakefile(self, snakefile):
        """Minimal validation: file exists"""
        # if snakefile is not provided
        if snakefile is None:
            snakefile = "Snakefile"
        snakefile = Path(snakefile)
        if not snakefile.exists():
            raise FileNotFoundError(
                f"Could not find Snakefile: {snakefile}\n"
                f"Make sure to provide the path to the Snakefile or place it at the submit directory or "
            )
        return snakefile

    def _setup_jobdir(self, options):
        """Create log directory if needed"""
        if options.get("jobdir"):
            jobdir = Path(options.get("jobdir"))
        else:
            jobdir = Path.cwd() / "logs"
        
        jobdir.mkdir(parents=True, exist_ok=True)
        return jobdir

    # ===== HTCondor SUBMISSION METHODS ===== #
    def _submit_local(self, snakefile, jobdir, snakemake_args):
        """Submit snakemake as an HTCondor local universe job."""
        # Resolve snakemake executable from user's environment
        snakemake_path = shutil.which("snakemake")
        if snakemake_path is None:
            raise RuntimeError(
                "Could not find 'snakemake' executable on PATH.\n"
                "Make sure your Snakemake environment is activated."
            )

        # Build arguments for snakemake
        args_list = [
            f"-s {snakefile}",
            f"--htcondor-jobdir {jobdir}",
        ]

        # Append any additional snakemake args passed after the -- separator
        if snakemake_args:
            args_list.extend(snakemake_args)

        arguments = " ".join(args_list)
        
        submit_description = htcondor.Submit({
            "executable": snakemake_path,
            "arguments": arguments,
            "universe": "local",
            "request_disk": "512MB",
            "request_cpus": 1,
            "request_memory": 512,
            
            # Set up logging
            "log": f"{jobdir}/snakemake-mgmt-$(ClusterId).log",
            "output": f"{jobdir}/snakemake-mgmt-$(ClusterId).out",
            "error": f"{jobdir}/snakemake-mgmt-$(ClusterId).err",
            
            # Specify getenv so the job uses the submitter's environment
            "getenv": "true",
            
            # Job naming - updated with cluster ID after submission
            "JobBatchName": f"snakemake-mgmt-{time.strftime('%Y%m%d-%H%M%S')}",
        })
        
        # Submit to HTCondor
        schedd = htcondor.Schedd()
        submit_result = schedd.submit(submit_description)
        
        cluster_id = submit_result.cluster()
        # Immediately update JobBatchName with cluster ID
        schedd.edit([f"{cluster_id}.0"], "JobBatchName", f"snakemake-mgmt-{cluster_id}")

        print(f"Snakemake managment job submitted with JobID {cluster_id}.0")
        print(f"Logs can be found in {jobdir}")


class Snake(Noun):
    """
    Run operations on Snakemake workflows via HTCondor
    """

    class submit(Submit):
        pass

    @classmethod
    def verbs(cls):
        return [cls.submit] 





