import htcondor2 as htcondor
import shutil
import subprocess
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

from htcondor_cli.eventlog import convert_seconds_to_dhms
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import JobStatus
from htcondor_cli import TMP_DIR
from htcondor_cli import MutualExclusionArgs
import traceback
import yaml
import textwrap
"""
HTCondor CLI for running Snakemake workflow

- need `-help` flag to guide users about the commands
"""
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

class Submit(Verb):
    """
    Submit a local universe job and Snakemake jobs when run.
    """
    # command-line argument configurations
    options = {
        "snakefile": {
            "args": ("snakefile",), # positional argument
            "help": "Path to Snakefile. If omitted, Snakefile is assumed to be at the current directory.",
            "nargs": "?",
        },
        "profile": {
            "args": ("--profile",),
            "help": "Snakemake profile directory.",
            "required": False,
        }, 
        "jobdir": {
            "args": ("--jobdir",),
            "help": "Directory for HTCondor log files. If omitted, a `logs` directory will be created as the current directory.",
            "required": False,
        },
        "executor": {
            "args": ("--executor",),
            "help": "Required Snakemake-HTCondor executor plugin to be sent to the execution point (EP). Please install the executor beforehand.",
            "required": True,
        }
    }

    def __init__(self, logger, snakefile, snakemake_args=None, **options):
        # Basic validations of CLI
        snakefile = self._validate_snakefile(snakefile)
        profile = self._validate_profile(options) # can be None depends on what users' choices
        jobdir = self._setup_jobdir(options)
        executor = options.get("executor")

        # submit a local universe job
        try:
            self._submit_local(snakefile, profile, jobdir, executor, snakemake_args or [])
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

    def _validate_profile(self, options):
        """Return profile path if it is explicitly provided by user"""
        profile_specified = options.get("profile")
        if profile_specified:
            profile = Path(profile_specified) 
            if not profile.exists():
                raise FileNotFoundError(f"Profile directory not found: {profile}")
            return profile
        return None

    def _setup_jobdir(self, options):
        """Create log directory if needed"""
        if options.get("jobdir"):
            jobdir = Path(options.get("jobdir"))
        else:
            jobdir = Path.cwd() / "logs"
        
        jobdir.mkdir(parents=True, exist_ok=True)
        return jobdir

    # ===== HTCondor SUBMISSION METHODS =====
    def _submit_local(self, snakefile, profile, jobdir, executor, snakemake_args):
        """Submit snakemake as an HTCondor local universe job."""
        # Resolve snakemake executable from user's environment
        snakemake_path = shutil.which("snakemake")
        if snakemake_path is None:
            raise RuntimeError(
                "Could not find 'snakemake' executable on PATH.\n"
                "Make sure your Snakemake environment is activated."
            )
        
        # Check if user already specified --htcondor-jobdir in snakemake_args
        has_htcondor_jobdir = any(snakemake_args[i] == "--htcondor-jobdir" for i in range(len(snakemake_args)))
        # Build arguments for snakemake
        args_list = [
            f"-s {snakefile}",
            f"--executor {executor}",
        ]
        
        # Only add --htcondor-jobdir if user did not specify it 
        if not has_htcondor_jobdir:
            args_list.append(f"--htcondor-jobdir {jobdir}")

        # Add profile if specified
        if profile:
            args_list.append(f"--profile {profile}")
        
        # Add any additional snakemake args
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





