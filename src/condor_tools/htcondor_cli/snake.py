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
    # Command-line argument configurations
    options = {
        "jobdir": {
            "args": ("--jobdir",),
            "help": "Optional directory for HTCondor management job log files. If omitted, a `logs` directory will be created at the current directory. Must be specified before the -- separator.",
            "required": False,
        },
        "snakemake_args": {
            "args": ("snakemake_args",),
            "nargs": argparse.REMAINDER,
            "help": "Snakefile followed by optional snakemake arguments. Usage: [--jobdir DIR] [Snakefile] [-- snakemake_args]. Snakefile and --jobdir must come before the -- separator.",
        },
    }

    def __init__(self, logger, snakefile=None, snakemake_args=None, **options):
        
        # snakefile is only provided when called directly (e.g. from tests).
        # When invoked via the CLI, everything lands in snakemake_args via REMAINDER.
        # 
        # Valid usage patterns (-- is a boundary, everything after it goes to snakemake):
        #   (1) htcondor snake submit --jobdir logs Snakefile -- --args
        #   (2) htcondor snake submit Snakefile --jobdir logs -- --args
        #   (3) htcondor snake submit -- --profile htcondor_profile (uses default Snakefile)

        if snakefile is None:
            # Extract --jobdir from snakemake_args if present
            snakemake_args = self._extract_jobdir_from_remainder(snakemake_args, options)
            
            if snakemake_args and snakemake_args[0] == '--':
                # When no snakefile given, we can just strip the separator
                snakemake_args = snakemake_args[1:]
            else:
                if snakemake_args and not snakemake_args[0].startswith('-'):
                    snakefile = snakemake_args.pop(0)
                # Strip the -- separator that may follow the snakefile
                if snakemake_args and snakemake_args[0] == '--':
                    snakemake_args = snakemake_args[1:]

        jobdir = self._setup_jobdir(options)

        # Basic validations for Snakefile
        snakefile = self._validate_snakefile(snakefile)
                    
        # submit a local universe job
        try:
            self._submit_local(snakefile, jobdir, snakemake_args)
        except Exception as e:
            print("Error: Could not submit local universe job.")
            print(f"Details:", str(e))
            sys.exit(1)
    
    def _extract_jobdir_from_remainder(self, snakemake_args, options):
        """
        Scan snakemake_args for all --jobdir occurrences and extract them, but only before the -- separator.
        The -- separator acts as a boundary so everything after is sent to Snakemake.
        If multiple --jobdir flags appear, the last one wins
        Returns filtered snakemake_args without any --jobdir flags and their values.
        Updates options["jobdir"] if --jobdir was found in REMAINDER.
        """
        if not snakemake_args:
            return snakemake_args
        
        # Find the position of -- separator
        separator_index = None
        try:
            separator_index = snakemake_args.index('--')
        except ValueError:
            pass  # No separator found
        
        # Scan and extract all --jobdir occurrences from before the separator
        scan_range = separator_index if separator_index is not None else len(snakemake_args)
        
        # Collect all --jobdir values (last one will be used)
        jobdir_value_in_remainder = None
        indices_to_skip = set()
        
        i = 0
        while i < len(snakemake_args):
            if i < scan_range and snakemake_args[i] == '--jobdir' and i + 1 < scan_range:
                # Found --jobdir before the separator
                # Unintended behavior if value of --jobdir is not specified
                jobdir_value_in_remainder = snakemake_args[i + 1]  # Update to latest value
                indices_to_skip.add(i)      # Mark --jobdir for removal
                indices_to_skip.add(i + 1)  # Mark its value for removal
                i += 2
            else:
                i += 1
        
        # If we found any --jobdir in REMAINDER, rebuild filtered args
        if indices_to_skip:
            options["jobdir"] = jobdir_value_in_remainder
            filtered_args = []
            for i in range(len(snakemake_args)):
                if i not in indices_to_skip:
                    filtered_args.append(snakemake_args[i])
            return filtered_args
        
        return snakemake_args
    
    def _validate_snakefile(self, snakefile):
        """Minimal validation: file exists"""
        # If snakefile is not provided, use default
        if snakefile is None:
            snakefile = "Snakefile"
        
        snakefile_path = Path(snakefile)
        if not snakefile_path.exists():
            raise FileNotFoundError(
                f"Could not find Snakefile: {snakefile}\n"
                f"Make sure to provide the correct path to the Snakefile or place 'Snakefile' in the current directory."
            )
        return snakefile_path

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
            f"--executor htcondor", # hardcoded as we assume that users have it
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
            
            # Management Job Name
            "JobBatchName": f"snakemake-mgmt-$(ClusterId)",
        })
        
        # Submit to HTCondor
        schedd = htcondor.Schedd()
        submit_result = schedd.submit(submit_description)
        
        cluster_id = submit_result.cluster()
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





