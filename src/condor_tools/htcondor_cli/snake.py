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
            "args": ("--htcondor_jobdir",),
            "help": "Directory for htcondor logs. If omitted, a `snakemake-long-logs` directory will be created as the current directory.",
            "required": False,
        },
        "snake-args": {
            "args": ("--snake_args",),
            "help": "Additional Snakemake arguments (comma-seperated) and must be wrapped around quotation marks and is comma-seperated. E.g: --snake-args \"core: 4, dry-run\"",
            "required": False,
        },
    }

    def __init__(self, logger, **options):
        # Basic validations of CLI
        snakefile = self._validate_snakefile(options)
        profile = self._validate_profile(options) # can be None depends on what users' choices
        jobdir = self._setup_jobdir(options)
        snake_args = self._snake_args(options, profile)


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
        """Minimal validation and create profile if it does not exist:"""
        profile_specified = options.get("profile")
        if profile_specified:
            profile = Path(profile_specified) 
        else:
            # use default profile directory in cwd
            profile = Path.cwd() / "profile"
        
        if not profile.exists():
            # create profile directory by default
            profile.mkdir(parents=True, exist_ok=True)
            if not profile_specified: 
                print(f"Created default profile directory at: {profile}")
                print(f"You can specify snakemake and htcondor arguments in the config.yaml or use --snake-args")

        # create config file if it does not exist
        config_file = profile / "config.yaml"
        if not config_file.exists():
            config_content = textwrap.dedent("""\
            # Snakemake HTCondor Profile Configuration
            # Uncomment and modify settings as needed
            # Your snake-args will be appended here

            # Number of jobs to run in parallels
            jobs: 10

            # Executor type (required for HTCondor)
            executor : htcondor

            # File system usage
            shared-fs-usage: none

            # Number of retires for failed jobs
            retries: 3

            # Default resources for HTCondor jobs
            default-resources:
                # if you have wrapper script
                job_wrapper: "wrapper.sh"
                # example environment package needed
                htcondor_transfer_input_files: "/workspaces/htcondor/example/snakemake-env.tar.gz"
                reserve_relative_paths: true
                universe: "vanilla"
                request_disk: "4GB"
                request_memory: "1GB"
                threads: 1
            """)
            config_file.write_text(config_content)
        else:  
            with open(config_file, "r") as f:
                config = yaml.safe_load(f) or {}

            # check if the executor is present
            if "executor" not in  config or config["executor"] is None:
                # add executor
                config["executor"] = "htcondor"

            with open(config_file, "w") as f:
                yaml.dump(config, f, default_flow_style=False, sort_keys=False)

        return profile

    def _setup_jobdir(self, options):
        """Create log directory if needed"""
        if options.get("htcondor_jobdir"):
            jobdir = Path(options.get("htcondor_jobdir"))
        else:
            jobdir = Path.cwd()/ "snakemake-long-logs"
        jobdir.mkdir(parents=True, exist_ok = True)
        return jobdir

    def _snake_args(self, options, profile):
        """Append additional snake-args into the profile"""
        snake_args = options.get("snake_args")
        # no snake-args specified 
        if not snake_args:
            return
    
        # we have snake-args to process. Profile already exists
        config_file = profile/"config.yaml"

        with open(config_file, "r") as f:
            config = yaml.safe_load(f) or {}

        for arg in snake_args.split(","):
            arg = arg.strip()
            if not arg:
                continue
            # replace the corresponding argument's value if it is already existed before
            parsed = yaml.safe_load(arg)
            if isinstance(parsed, dict):
                # update value for the resources if needed
                config.update(parsed)
            else:
                # set one argument resources to True if specified. i.e: dry-run: True
                config[arg] = True
            
        with open(config_file, "w") as f:
            yaml.dump(config, f, default_flow_style=False, sort_keys=False)
            

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





