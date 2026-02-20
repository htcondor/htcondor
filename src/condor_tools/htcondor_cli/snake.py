import htcondor2 as htcondor
import time
from htcondor2._utils.ansi import Color, colorize, bold, stylize, AnsiOptions
from datetime import datetime, timedelta
from pathlib import Path

from htcondor_cli.eventlog import convert_seconds_to_dhms
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import JobStatus
from htcondor_cli import TMP_DIR
from htcondor_cli import MutualExclusionArgs
from htcondor_cli.snakemake_long import submit_local
import traceback

"""
HTCondor CLI for running Snakemake workflow

- need `-help` flag to guide users about the commands
"""
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

class Submit(Verb):
    """
    Milestone One: htcondor snake submit
    when running htcondor snake submit:
    - snakemake_long.py is invoked for the long running job for the workflow
    --> this launches a local universe job
    - Snakemake jobs are submitted: 2 ways
    --> from a profile configuration
    --> from regular snakemake commands


    #additional behavior
    - after running condor_q -nobatch or watch_q: local universe job should appear sth like
    `snakemake_$(clusterid)` as a job management id
    """
    # command-line argument configurations
    options = {
        "snakefile": {
            "args": ("--snakefile",),
            "help": "Path to Snakefile. If omitted, Snakefile is assumed to be at the current directory.",
            "required": False,
        },
        "profile": {
            "args": ("--profile",),
            "help": "Snakemake profile directory.",
            "required": False,
        },
        "htcondor_jobdir": {
            "args": ("--htcondor-jobdir",),
            "help": "Directory for logs. If omitted, a `snakemake-long-logs` directory will be created as the current directory.",
            "required": False,
        },
       

        
        ##### for now assume that we are in the right environment ####
        # "conda_env": {
        #     "args": ("--conda-env"),
        #     "help": "Conda environment name for Snakemake",
        #     "required": False,
        # },
        # "use_mamba": {
        #     "args": ("--use-mamba"),
        #     "action": "store-true",
        #     "help": "Use mamba instead of conda",
        # },
        # Allow pass-through of arbitrary Snakemake args
    
        #### we cannot have snake-args for now ####
        # "snake-args": {
        #     "args": ("--snake-args",),
        #     "help": "Additional Snakemake arguments (comma-seperated)",
        #     "required": False,
        # },
    }

    def __init__(self, logger, **options):
        # Basic validations of CLI
        snakefile = self._validate_snakefile(options)
        profile = self._validate_profile(options) # can be None depends on what users' choices
        jobdir = self._setup_jobdir(options)

        #debuggin print statement 
        print(f"snakefile: {snakefile}")
        print(f"profile: {profile}")
        print(f"jobdir: {jobdir}")


        # submit a local universe job
        try:
            submit_local(snakefile, profile, jobdir)
        except Exception as e:
            print("Error: Could not submit local universe job.")
            print(f"Exception details:", str(e))
            traceback.print_exc()
            raise

    def _validate_snakefile(self, options):
        """Minimal validation: file exists"""
        if options.get("snakefile"):
            snakefile = Path(options.get("snakefile"))
        else:
            snakefile = Path.cwd()/"Snakefile"
        if not snakefile.exists():
            raise FileNotFoundError(
                f"Could not find Snakefile: {snakefile}\n"
                f"Specify with --snakefile or create ./Snakefile"
            )
        return snakefile

    def _validate_profile(self, options):
        """Minimal validation: if provided, profile directory should exists"""
        if options.get("profile"):
            profile = Path(options.get("profile")) 
        else:
            profile = None
        if profile and not profile.exists():
            raise FileNotFoundError(
                f"Could not file Profile directory: {profile}\n"
                f"Ensure the profile directory exists or omit -profile and specify --snake-args instead to use CLI args"
            )
        return profile
    def _setup_jobdir(self, options):
        """Create log directory if needed"""
        if options.get("htcondor-jobdir"):
            jobdir = Path(options.get("htcondor-jobdir"))
        else:
            jobdir = Path.cwd()/ "snakemake-long-logs"
        jobdir.mkdir(parents=True, exist_ok = True)
        return jobdir

#class Status(Verb):
#    pass


#class Halt(Verb):
#    pass


class Snake(Noun):
    """
    Run operations on Snakemake workflows via HTCondor
    """

    class submit(Submit):
        pass

    @classmethod
    def verbs(cls):
        return [cls.submit] 





