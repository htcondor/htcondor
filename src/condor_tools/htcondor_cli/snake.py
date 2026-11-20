import argparse
import htcondor2 as htcondor
import shutil
import importlib.util
import sys
import signal
import os
from pathlib import Path

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import MutualExclusionArgs
import getpass


"""
HTCondor CLI for running Snakemake workflow
"""
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

JSM_HTC_SNAKE_SUBMIT = 6 

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
        """
        Initialize and submit a Snakemake management job.

        This constructor performs CLI-style parsing of the provided arguments,
        validates inputs, prepares the job directory, and submits a local
        universe HTCondor job which will run Snakemake with the HTCondor
        executor.

        Args:
            logger: Logger object used for logging messages.
            snakefile (str or pathlib.Path, optional): Path to the Snakefile to run.
            snakemake_args (list[str], optional): Remaining arguments intended
                for Snakemake (may include a leading "--" separator or jobdir).
            **options: Additional options parsed from the CLI. Supported key:
                - "jobdir": path to a directory where management logs are written.

        Returns:
            None

        Raises:
            FileNotFoundError: if the resolved Snakefile does not exist.
            RuntimeError: if Snakemake executable cannot be found or submission fails.
        """
        # Check for the presence of the executor plugin
        if importlib.util.find_spec("snakemake_executor_plugin_htcondor") is None:
            raise RuntimeError(
                "The 'snakemake-executor-plugin-htcondor' plugin is required but not yet installed.\n"
                "Install it with: pip install snakemake-executor-plugin-htcondor"
            )
            

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
                    
        # Submit a local universe job
        try:
            self._submit_local(snakefile, jobdir, snakemake_args)
        except Exception as e:
            print("Error: Could not submit local universe job.")
            print(f"Details:", str(e))
            sys.exit(1)
    
    def _extract_jobdir_from_remainder(self, snakemake_args, options):
        """
        Extract any `--jobdir` occurrences from the remainder `snakemake_args`.

        Only `--jobdir` occurrences appear before the separator `--` are considered. 
        If multiple `--jobdir` flags are present the last value wins and is written into
        `options['jobdir']`.

        Args:
            snakemake_args (list[str] or None): The argument list from the CLI
                that will be forwarded to Snakemake. May include a leading
                "--" separator and `--jobdir`.
            options (dict): Mutable dict of parsed options. Will be updated
                with a "jobdir" key if a `--jobdir` value is found.

        Returns:
            list[str] or None: A filtered list of `snakemake_args` with any
            `--jobdir` flags and their values removed. Returns the original
            `snakemake_args` if no `--jobdir` was found.
        """

        if not snakemake_args:
            return snakemake_args
        
        # Find the position of -- separator
        try:
            separator_index = snakemake_args.index('--')
        except ValueError:
            separator_index = None
        
        # Scan and extract all --jobdir occurrences from before the separator
        scan_range = separator_index if separator_index is not None else len(snakemake_args)
        
        # Collect all --jobdir values (last one will be used)
        filtered_args = []
        jobdir_value_in_remainder = None
        i = 0

        # For repeated specifications of --jobdir, the last one wins
        while i < scan_range:
            if snakemake_args[i] == '--jobdir':
                if i + 1 >= scan_range or snakemake_args[i+1].startswith("-"):
                    raise ValueError("--jobdir requires a valid directory string value")
                jobdir_value_in_remainder = snakemake_args[i+1]
                i += 2
            else:
                filtered_args.append(snakemake_args[i])
                i += 1
        
        if jobdir_value_in_remainder is None:
            return snakemake_args

        options["jobdir"] = jobdir_value_in_remainder
        filtered_args.extend(snakemake_args[scan_range:])
        return filtered_args
    
    def _validate_snakefile(self, snakefile):
        """
        Validate and normalize the given snakefile path.

        If `snakefile` is `None`, this function defaults to the string
        "Snakefile" and verifies the file exists.

        Args:
            snakefile (str or path or None): Path or name of the Snakefile.

        Returns:
            pathlib.Path: Resolved Path object pointing to the Snakefile.

        Raises:
            FileNotFoundError: If the resolved path does not exist.
        """

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
        """
        Create or resolve the job directory for management logs.

        Args:
            options (dict): Parsed CLI options. If the dict contains a
                "jobdir" key its value will be used as the job directory.

        Returns:
            pathlib.Path: Path to the created or existing job directory.
        """

        if options.get("jobdir"):
            jobdir = Path(options.get("jobdir"))
        else:
            jobdir = Path.cwd() / "logs" # default name if jobdir is not provided
        
        jobdir.mkdir(parents=True, exist_ok=True)
        return jobdir

    # ===== HTCondor SUBMISSION METHODS ===== #
    def _submit_local(self, snakefile, jobdir, snakemake_args):
        """
        Submit Snakemake as an HTCondor local-universe management job.

        This method discovers the Snakemake executable, constructs a
        Submit description for HTCondor, submits the job, writes a pointer
        file into `.snakemake/htcondor` so other commands (eg. `status`)
        can locate the workflow job directory, and prints submission info.

        Args:
            snakefile (pathlib.Path or str): Path to the Snakefile to run.
            jobdir (pathlib.Path): Directory where management logs should be
                written and where metadata will be stored.
            snakemake_args (list[str] or None): Extra arguments to forward to
                Snakemake when it runs.

        Returns:
            None

        Raises:
            RuntimeError: If the Snakemake executable cannot be found.
            Exception: Any exception raised by HTCondor submission is
                propagated to the caller.
        """

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
            f"--executor htcondor",
            f"--htcondor-jobdir {jobdir}",
        ]

        # Append any additional snakemake args passed after the -- separator
        if snakemake_args:
            args_list.extend(snakemake_args)

        arguments = " ".join(args_list)

        request_memory = htcondor.param.get("SNAKEMAKE_MANAGER_REQUEST_MEM", "512MB")

        submit_description = htcondor.Submit({
            "executable": snakemake_path,
            "arguments": arguments,
            "universe": "local",
            "request_disk": "512MB",
            "request_cpus": 1,
            "request_memory": request_memory,
            
            # Set up logging
            "log": f"{jobdir}/snakemake-mgmt-$(ClusterId).log",
            "output": f"{jobdir}/snakemake-mgmt-$(ClusterId).out",
            "error": f"{jobdir}/snakemake-mgmt-$(ClusterId).err",
            
            # Specify getenv so the job uses the submitter's environment
            "getenv": "true",

            # Management Job Name
            "JobBatchName": f"snakemake-mgmt-$(ClusterId)",

            # Setting the remove signal to be SIGINT instead of SIGTERM because Snakemake internal 
            # treat SIGTERM as graceful removal and will not trigger `cancel_jobs` method in the executor.
            "remove_kill_sig": "SIGINT",
        })
        
        # Submit to HTCondor
        schedd = htcondor.Schedd()
        # Set s_method to JSM_HTC_SNAKE_SUBMIT
        submit_description.setSubmitMethod(JSM_HTC_SNAKE_SUBMIT, True)

        submit_result = schedd.submit(submit_description)
        
        
        cluster_id = submit_result.cluster()
        print(f"Snakemake managment job submitted with JobID {cluster_id}.0")
        print(f"Logs can be found in {jobdir}")

class Remove(Verb):
    """
    Remove associated jobs given the management job ID
    
    There are two modes:
    - fast mode: hard kill of the Snakemake process (all associated jobs will be removed and clean up process triggered)
    - peaceful mode: graceful termination of the Snakemake process (allow running jobs to complete)
    """
    # Positional argument for the management ID
    options = {
        "mgmt_id": {
            "args": ("mgmt_id",), # positional argument
            "type": int,
            "help": "Positional argument for a management JobID that oversees the entire workflow. Must be specified.",
        },
        "remove_mode": MutualExclusionArgs({
            "fast": {
                "args": ("--fast", "-f"),
                "action": "store_true",
                "help": "Immediate termination of the current workflow: stop all jobs associated with the provided the management id.",
            },
            "peaceful": {
                "args": ("--peaceful", "-p"),
                "action": "store_true",
                "help": "Graceful termination of the current workflow: allow running jobs to complete, stop submitting new ones.",
            },
        }),
    }

    def __init__(self, logger, mgmt_id=None, **options):
        """
        When `htcondor snake remove <mgmt_id> --peaceful is run, allow running jobs to complete and stop submitting new ones.

        When `htcondor snake remove <mgmt_id> --fast` is run, remove all jobs associated with the management job immediately.

        When `--fast` or `--peaceful` is not specified, we'll default to `--fast`.
        
        This also involves making sure that the id provided is the management job id and the jobs to be removed are under it.

        Args:
            logger: Logger object used for logging messages.
            mgmt_id (str or int): Management job ClusterId for the workflow.
            **options: for method specifications. Suported flags:
                - "--fast": hard kill (SIGINT)
                - "--peaceful": graceful kill (SIGTERM)

        Returns:
            None

        Raises:
            RuntimeError: if the schedd cannot be reached or the removal request fails.
        """
        self.logger = logger
        
        if mgmt_id is None:
            print("Error: management ID is required")
            sys.exit(1)

        try:
            mgmt_id = int(mgmt_id)
        except ValueError:
            print("Management Job ID must be an integer.")
            sys.exit(1)

        try:
            schedd = htcondor.Schedd()
            if options.get("fast") or (not options.get("peaceful")):
                # Verify that the management id given belongs to Snakemake process by checking job submit method
                
                # Send the signal to the schedd to remove the management job -> pass to Snakemake process
                # Instead of SIGTERM that is the default, we use SIGINT set as a classad in submit description
                # because Snakemake internal does graceful removal with SIGTERM and will not trigger `cancel_jobs()`.
                # This also removes the child jobs as well.
                # Note: running condor_rm for this workflow = sending SIGINT
                res = schedd.act(
                    htcondor.JobAction.Remove,
                    f"(ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT}) || SnakeManagerJobId == {mgmt_id}",
                    reason=f"via htcondor snake remove (by user {getpass.getuser()})",
                )
                
                # Check that the job was actually found and removed = 1 here
                total_success = res.get("TotalSuccess", 0)
                total_error = res.get("TotalError", 0)

                if total_success > 0:
                    self.logger.info(f"Removing the management job {mgmt_id}. "
                                     "All associated jobs will be removed shortly after that.")
                elif total_error > 0:
                    self.logger.warning(f"Failed to remove job {mgmt_id}: schedd reported {total_error} error(s).")
                else:
                    self.logger.info(
                        f"Job {mgmt_id} was not found as a `htcondor snake submit` "
                        "management job."
                    )
            else:
                # Query the SnakeMgmtPID 
                jobs = schedd.query(
                    constraint=f"ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT}", 
                    projection=["JobStatus", "SnakeMgmtPID"])

                if not jobs:
                    self.logger.error(f"No management job found with JobID {mgmt_id}.")
                    sys.exit(1)

                # Only one management job is expected
                if len(jobs) > 1:
                    self.logger.error(f"Expecting exactly one management job with ClusterId {mgmt_id}, but multiple were found.")
                    sys.exit(1)

                job = jobs[0]
                status = job.get("JobStatus")

                # Handling job statuses
                if status == 1 or status == 5:
                    # Idle or Held status: no Snakemake process, so just remove it
                    self._remove_mgmt_job(schedd, mgmt_id)
                    return
                elif status == 3 or status == 4:
                    # Job status = 3 and 4 signal removed or completed
                    self.logger.info(f"Management job {mgmt_id} has already been removed or completed.")
                    return

                # Get the pid for the running management job
                target_pid = job.get("SnakeMgmtPID")
                if target_pid is None:
                    self.logger.warning(f"Management job {mgmt_id} is starting up. Please try again in a few seconds.")
                    sys.exit(1)

                # Sending SIGTERM to management job.
                # Snakemake catches SIGTERM and shuts down gracefully: it lets
                # already-running jobs finish and stops submitting new ones.
                try:
                    os.kill(target_pid, signal.SIGTERM)
                except ProcessLookupError:
                    # Due to race condition, PID may be out of date
                    # Re-query the SnakeMgmtPID 
                    jobs = schedd.query(
                        constraint=f"ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT}", 
                        projection=["JobStatus", "SnakeMgmtPID"])
    
                    if not jobs or jobs[0].get("JobStatus") in (3,4):
                        self.logger.info(f"Management job {mgmt_id} has already exited.")
                        return

                    new_pid = jobs[0].get("SnakeMgmtPID")
                    if jobs[0].get("JobStatus") == 2 and new_pid is not None and new_pid != target_pid:
                        self.logger.warning(
                            f"Management job {mgmt_id} restarted with a new process (PID {new_pid}). "
                            f"Run `htcondor snake remove {mgmt_id} --peaceful` again to stop it."
                        )
                        sys.exit(1)

                    if jobs[0].get("JobStatus") == 2 and new_pid is None:
                        self.logger.warning(f"Management job {mgmt_id} is starting up. Please try again in a few seconds.")
                        sys.exit(1)

                    # Idle or Held, or Running with the same (stale) PID, so there is no live process
                    self._remove_mgmt_job(schedd, mgmt_id)
                    return
                
                self.logger.info(f"Removing the management job {mgmt_id}. Its running jobs will be completed before exiting.")
        except Exception as e:
            self.logger.error(f"Could not remove the management job: {e}")
            sys.exit(1)

    def _remove_mgmt_job(self, schedd, mgmt_id):
        """Remove only the management job through the schedd."""
        res = schedd.act(
            htcondor.JobAction.Remove, 
            f"ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT}",
            reason=f"via htcondor snake remove --peaceful (by user {getpass.getuser()})"
        )

        if res.get("TotalSuccess", 0) > 0:
            self.logger.info(
                f"Removed management job {mgmt_id} from the queue."
            )
        else:
            self.logger.warning(
                f"Could not remove management job {mgmt_id}: "
                f"schedd reported {res.get('TotalError', 0)} error(s)."
            )
            sys.exit(1)

class Snake(Noun):
    """
    Run operations on Snakemake workflows via HTCondor
    """

    class submit(Submit):
        pass

    class remove(Remove):
        pass
    @classmethod
    def verbs(cls):
        return [cls.submit, cls.remove] 
