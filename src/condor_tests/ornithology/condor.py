# Copyright 2019 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from typing import Mapping, Optional
import logging

import subprocess
from pathlib import Path
import shutil
import time
import functools
import shlex
import re
import textwrap
import os
import sys
import getpass
import socket

from .try_os_set import (
    try_os_setegid,
    try_os_seteuid,
)

import htcondor2 as htcondor

from . import job_queue, env, cmd, daemons, handles, scripts

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

unique_identifier = 0
true_exe = "/usr/bin/true" if sys.platform == "darwin" else "/bin/true"
if sys.platform == "win32" : true_exe = "$(LOCAL_DIR)/condor_tests/success.exe"

DEFAULT_PARAMS = {
    "LOCAL_CONFIG_FILE": "",
    "COLLECTOR_HOST": "$(CONDOR_HOST):0",
    "SHARED_PORT_PORT": "0",
    "MAIL": true_exe,
    "SENDMAIL": true_exe,
    "UPDATE_INTERVAL": "2",
    "POLLING_INTERVAL": "2",
    "NEGOTIATOR_INTERVAL": "2",
    "NEGOTIATOR_MIN_INTERVAL": "2",
    "SCHEDD_INTERVAL": "30",
    "STARTER_UPDATE_INTERVAL": "2",
    "SHADOW_QUEUE_UPDATE_INTERVAL": "2",
    "STARTER_INITIAL_UPDATE_INTERVAL": "2",
    "NEGOTIATOR_CYCLE_DELAY": "2",
    "MachineMaxVacateTime": "2",
    "RUNBENCHMARKS": "0",
    "MAX_JOB_QUEUE_LOG_ROTATIONS": "10",
    "PROCD_ADDRESS": "$(PROCD_ADDRESS)_$(PERSONAL_INSTANCE_ID)", # if we start multiple condors each must have a different PROCD_ADDRESS
    "FILETRANSFER_PLUGINS" : f"$(FILETRANSFER_PLUGINS) {scripts.custom_fto_plugins()}",
    "SINGULARITY": "/usr/bin/false"
}


def master_is_not_alive(self):
    return not self.master_is_alive


def condor_is_ready(self):
    return self.condor_is_ready


def condor_master_was_started(self):
    return self.condor_master is not None


def skip_if(condition):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(self, *args, **kwargs):
            if condition(self):
                logger.debug(
                    "Skipping call to {} for {} because {} was True".format(
                        func.__name__, self, condition.__name__
                    )
                )
                return

            return func(self, *args, **kwargs)

        return wrapper

    return decorator


class Condor:
    """
    A :class:`Condor` is responsible for managing the lifecycle of an HTCondor pool.
    """

    def __init__(
        self,
        local_dir: Path,
        config: Optional[Mapping[str, str]] = None,
        raw_config: Optional[str] = None,
        clean_local_dir_before: bool = True,
        submit_user : str = None,
        condor_user : str = None,
        use_sudo: Optional[bool] = None,
    ):
        """
        Parameters
        ----------
        local_dir
            The local directory for the HTCondor pool. All HTCondor state will
            be stored in this directory.
        config
            HTCondor configuration parameters to inject, as a mapping of key-value pairs.
        raw_config
            Raw HTCondor configuration language to inject, as a string.
        clean_local_dir_before
            If ``True``, any existing directory at ``local_dir`` will be removed
            before trying to stand up the new instance.
        submit_user
            If set, the user to switch to when submitting a job.
        condor_user
            If set, the user that HTCondor will run as.
        use_sudo
            If ``True``, start condor_master with sudo. If ``None`` (default),
            checks HTCONDOR_TEST_USE_SUDO environment variable. If ``False``,
            never use sudo even if environment variable is set.
        """
        self.submit_user = submit_user
        
        # Auto-detect sudo mode from environment if not explicitly specified
        if use_sudo is None:
            use_sudo = os.environ.get("HTCONDOR_TEST_USE_SUDO") == "1"
        
        self.use_sudo = use_sudo
        
        # If using sudo but no condor_user specified, default to "condor"
        if self.use_sudo and condor_user is None:
            condor_user = "condor"
        
        self.condor_user = condor_user
        self.local_dir = local_dir

        # TODO: don't assume paths, get these from config
        self.execute_dir = self.local_dir / "execute"
        self.lock_dir = self.local_dir / "lock"
        self.log_dir = self.local_dir / "log"
        self.run_dir = self.local_dir / "run"
        self.spool_dir = self.local_dir / "spool"
        self.passwords_dir = self.local_dir / "passwords.d"
        self.tokens_dir = self.local_dir / "tokens.d"

        self.config_file = self.local_dir / "condor_config"

        if config is None:
            config = {}
        self.config = {k: v if v is not None else "" for k, v in config.items()}
        self.raw_config = raw_config or ""

        self.clean_local_dir_before = clean_local_dir_before

        self.condor_master = None
        self.condor_is_ready = False

        self.job_queue = job_queue.JobQueue(self)

    def use_config(self):
        """
        Returns a context manager that sets ``CONDOR_CONFIG`` to point to the
        config file for this HTCondor pool.
        """
        return env.SetCondorConfig(self.config_file)

    def __repr__(self):
        return "{}(local_dir = {})".format(self.__class__.__name__, self.local_dir)

    @property
    def master_is_alive(self):
        return self.condor_master is not None and self.condor_master.poll() is None

    def __enter__(self):
        self._start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._cleanup()

    def _start(self):
        logger.info("Starting {}".format(self))

        try:
            self._setup_local_dirs()
            self._write_config()
            self._start_condor()
            self._wait_for_ready()
            self._make_job_queue_log_public_if_root()
        except BaseException:
            logger.exception(
                "Encountered error during setup of {}, cleaning up!".format(self)
            )
            self._cleanup()
            raise

        logger.info("Started {}".format(self))

    def _setup_local_dirs(self):
        if self.clean_local_dir_before and self.local_dir.exists():
            shutil.rmtree(self.local_dir)
            logger.debug("Removed existing local dir for {}".format(self))

        condor_dirs_to_make = [
            self.local_dir,
            self.execute_dir,
            self.lock_dir,
            self.log_dir,
            self.run_dir,
            self.spool_dir,
            self.passwords_dir,
            self.tokens_dir,
        ]

        # First make the dirs as non-privileged user
        for dir in condor_dirs_to_make:
            dir.mkdir(parents=True, exist_ok=not self.clean_local_dir_before)

        # Unprivileged users will write the condor_config file here, cheat by making world writable
        if self.use_sudo:
            # chmod 0777 condor dir so we can write config file there as non-root
            cmd.run_command(
                ["sudo", "chmod", "0777", f"{self.condor_user}:{self.condor_user}", self.local_dir.as_posix()],
                echo=False,
                suppress=True,
            )
        # Now chown them if needed
        for dir in condor_dirs_to_make:
            if self.use_sudo:
                cmd.run_command(
                    ["sudo", "chown", "-R", f"{self.condor_user}:{self.condor_user}", dir.as_posix()],
                    echo=False,
                    suppress=True,
                )
            else:
                if self.condor_user:
                    try:
                        shutil.chown(
                            dir, user=self.condor_user, group=self.condor_user
                        )
                    except PermissionError:
                        pass

    def _write_config(self):
        # TODO: how to ensure that this always hits the right config?
        # TODO: switch to -summary instead of -write:up
        write = cmd.run_command(
            ["condor_config_val", "-write:up", self.config_file.as_posix()],
            echo=False,
            suppress=True,
        )
        if write.returncode != 0:
            raise Exception("Failed to copy base OS config: {}".format(write.stderr))

        param_lines = []

        param_lines += ["#", "# ROLES", "#"]
        param_lines += [
            "DAEMON_LIST = MASTER COLLECTOR NEGOTIATOR STARTD SCHEDD",
        ]

        base_config = {
            "LOCAL_DIR": self.local_dir.as_posix(),
            #"EXECUTE": "$(LOCAL_DIR)/execute",
            "LOCK": "$(LOCAL_DIR)/lock",
            #"LOG": "$(LOCAL_DIR)/log",
            "RUN": "$(LOCAL_DIR)/run",
            #"SPOOL": "$(LOCAL_DIR)/spool",
            "SEC_PASSWORD_DIRECTORY": "$(LOCAL_DIR)/passwords.d",
            "SEC_TOKEN_SYSTEM_DIRECTORY": "$(LOCAL_DIR)/tokens.d",
            # "STARTD_DEBUG": "D_FULLDEBUG D_COMMAND:1",
        }

        if self.condor_user or self.use_sudo:
            try:
                from pwd import getpwnam
                from grp import getgrnam

                uid = getpwnam(self.condor_user).pw_uid
                gid = getgrnam(self.condor_user).gr_gid
                base_config["CONDOR_IDS"] = f"{uid}.{gid}"
            except ModuleNotFoundError:
                # This should arguably be a setup error if this process
                # has user-switching privileges.
                pass

        if self.use_sudo:
            username = getpass.getuser()
            fqdn     = socket.getfqdn()
            base_config["ALLOW_WRITE"] = f"{username}@{fqdn}"

        # 
        global unique_identifier
        unique_identifier += 1
        base_config["PERSONAL_INSTANCE_ID"] = "{}".format(unique_identifier)

        # win32 default for PROCD_ADDRESS can end up being too long when running tests
        # so we want to use a different value than the default
        # $Fddub() extracts the last 2 components of LOCAL_DIR as posix with no trailing /
        if sys.platform == "win32":
            base_config["PROCD_ADDRESS"] = r"\\.\pipe\$Fddub(LOCAL_DIR)/{}_$(PERSONAL_INSTANCE_ID)".format(os.getpid())

        param_lines += ["#", "# BASE PARAMS", "#"]
        param_lines += ["{} = {}".format(k, v) for k, v in base_config.items()]

        param_lines += ["#", "# DEFAULT PARAMS", "#"]
        param_lines += ["{} = {}".format(k, v) for k, v in DEFAULT_PARAMS.items()]

        param_lines += ["#", "# CUSTOM PARAMS", "#"]
        param_lines += ["{} = {}".format(k, v) for k, v in self.config.items()]

        param_lines += ["#", "# RAW PARAMS", "#"]
        param_lines += textwrap.dedent(self.raw_config).splitlines()

        with self.config_file.open(mode="a") as f:
            f.write("\n".join(param_lines))
        logger.debug("Wrote config file for {} to {}".format(self, self.config_file))

    @skip_if(condor_master_was_started)
    def _start_condor(self):
        with env.SetCondorConfig(self.config_file):
            # If we invoke the condor_master via sudo, sudo won't use the path for
            # security reasons, so we have to find the binary ourselves.
            master_bin = shutil.which("condor_master")
            cmd = [master_bin, "-f"]
            
            if self.use_sudo:
                cmd = ["sudo", "-E"] + cmd
                logger.info(
                    f"Starting condor_master with sudo (will run as {self.condor_user or 'root'})"
                )
            
            self.condor_master = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE
            )

            logger.debug(
                "Started condor_master (pid {})".format(self.condor_master.pid)
            )

    @skip_if(condor_is_ready)
    def _wait_for_ready(self, timeout: int = 120, dump_logs_if_fail: bool = False):
        daemons = set(
            self.run_command(["condor_config_val", "DAEMON_LIST"], echo=False)
            .stdout.replace(",", " ")
            .split(" ")
        )
        master_log_path = self.master_log.path

        logger.debug(
            "Starting up daemons for {}, waiting for: {}".format(
                self, " ".join(sorted(daemons))
            )
        )

        start = time.time()
        while time.time() - start < timeout:
            time_to_give_up = int(timeout - (time.time() - start))

            # if the master log does not exist yet, we can't use condor_who
            if not master_log_path.exists():
                logger.debug(
                    "MASTER_LOG at {} does not yet exist for {}, retrying in 1 seconds (giving up in {} seconds)".format(
                        self.master_log, self, time_to_give_up
                    )
                )
                time.sleep(1)
                continue

            if "STARTD" in daemons:
                who_command = "condor_who -wait:10 'IsReady && STARTD_State =?= \"Ready\"'"
            else:
                who_command = "condor_who -wait:10 'IsReady'"
            who = self.run_command(
                shlex.split(who_command),
                echo=False,
                suppress=True,
            )
            if who.stdout.strip() == "":
                logger.debug(
                    "condor_who stdout was unexpectedly blank for {}, retrying in 1 second (giving up in {} seconds)".format(
                        self, time_to_give_up
                    )
                )
                time.sleep(1)
                continue

            who_ad = dict(kv.split(" = ") for kv in who.stdout.splitlines())

            if (
                who_ad.get("IsReady") == "true"
                and ("STARTD" not in daemons or who_ad.get("STARTD_State") == '"Ready"')
                and all(who_ad.get(d) == '"Alive"' for d in daemons)
            ):
                self.condor_is_ready = True
                return

            logger.debug(
                "{} is waiting for daemons to be ready (giving up in {} seconds)".format(
                    self, time_to_give_up
                )
            )

        self.run_command(["condor_who", "-quick"])
        if dump_logs_if_fail:
            for logfile in self.log_dir.iterdir():
                logger.error("Contents of {}:\n{}".format(logfile, logfile.read_text()))

        raise TimeoutError("Standup for {} failed".format(self))

    def _cleanup(self):
        logger.info("Cleaning up {}".format(self))

        self._condor_off()
        self._wait_for_master_to_terminate()
        # TODO: look for core dumps
        # self._remove_local_dir()

        logger.info("Cleaned up {}".format(self))

    @skip_if(master_is_not_alive)
    def _condor_off(self):
        off = self.run_command(
            ["condor_off", "-fast", "-daemon", "master"], timeout=30, echo=False
        )

        if not off.returncode == 0:
            logger.error(
                "condor_off failed, exit code: {}, stderr: {}".format(
                    off.returncode, off.stderr
                )
            )
            self._terminate_condor_master()
            return

        logger.debug("condor_off succeeded: {}".format(off.stdout))

    @skip_if(master_is_not_alive)
    def _wait_for_master_to_terminate(self, kill_after: int = 60, timeout: int = 120):
        logger.debug(
            "Waiting for condor_master (pid {}) to terminate".format(
                self.condor_master.pid
            )
        )

        start = time.time()
        killed = False
        while True:
            try:
                self.condor_master.communicate(timeout=5)
                break
            except subprocess.TimeoutExpired:
                pass

            elapsed = time.time() - start

            if not killed:
                logger.debug(
                    "condor_master has not terminated yet, will kill in {} seconds".format(
                        int(kill_after - elapsed)
                    )
                )

            if elapsed > kill_after and not killed:
                # TODO: in this path, we should also kill the other daemons
                # TODO: we can find their pids by reading the master log
                self._kill_condor_master()
                killed = True

            if elapsed > timeout:
                raise TimeoutError(
                    "Timed out while waiting for condor_master to terminate"
                )

        logger.debug(
            "condor_master (pid {}) has terminated with exit code {}".format(
                self.condor_master.pid, self.condor_master.returncode
            )
        )

    @skip_if(master_is_not_alive)
    def _terminate_condor_master(self):
        if not self.master_is_alive:
            return

        self.condor_master.terminate()
        logger.debug(
            "Sent terminate signal to condor_master (pid {})".format(
                self.condor_master.pid
            )
        )

    @skip_if(master_is_not_alive)
    def _kill_condor_master(self):
        self.condor_master.kill()
        logger.debug(
            "Sent kill signal to condor_master (pid {})".format(self.condor_master.pid)
        )

    def read_config(self) -> str:
        return self.config_file.read_text()

    def run_command(self, *args, **kwargs) -> subprocess.CompletedProcess:
        """
        Execute a command with ``CONDOR_CONFIG`` set to point to this HTCondor pool.
        Arguments and keyword arguments are passed through to :func:`~run_command`.
        """
        with self.use_config():
            return cmd.run_command(*args, **kwargs)

    @property
    def master_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the pool's master."""
        return self._get_daemon_log("MASTER")

    @property
    def collector_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the pool's collector."""
        return self._get_daemon_log("COLLECTOR")

    @property
    def negotiator_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the pool's negotiator."""
        return self._get_daemon_log("NEGOTIATOR")

    @property
    def schedd_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the pool's schedd."""
        return self._get_daemon_log("SCHEDD")

    @property
    def startd_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the pool's startd."""
        return self._get_daemon_log("STARTD")

    @property
    def shadow_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the pool's shadows."""
        return self._get_daemon_log("SHADOW")

    @property
    def credmon_oauth_log(self) -> daemons.DaemonLog:
        """A :class:`DaemonLog` for the OAuth CredMon."""
        return self._get_daemon_log("CREDMON_OAUTH")

    @property
    def job_queue_log(self) -> Path:
        """The :class:`pathlib.Path` to the pool's job queue log."""
        return self._get_log_path("JOB_QUEUE")

    @property
    def startd_address(self):
        """The address of the pool's startd."""
        return self._get_address_file("STARTD").read_text().splitlines()[0]

    def _get_log_path(self, subsystem):
        return self._get_path_from_condor_config_val("{}_LOG".format(subsystem))

    def _get_address_file(self, subsystem):
        return self._get_path_from_condor_config_val(
            "{}_ADDRESS_FILE".format(subsystem)
        )

    def _get_path_from_condor_config_val(self, attr):
        return Path(
            self.run_command(
                ["condor_config_val", attr], echo=False, suppress=True
            ).stdout
        )

    def _get_daemon_log(self, daemon_name):
        return daemons.DaemonLog(self._get_log_path(daemon_name))

    def get_local_schedd(self):
        """Return the :class:`htcondor.Schedd` for this pool's schedd."""
        with self.use_config():
            #htcondor.enable_debug("D_HOSTNAME D_CAT")
            sched = htcondor.Schedd()
            #htcondor.disable_debug()
            return sched

    def get_local_collector(self):
        """Return the :class:`htcondor.Collector` for this pool's collector."""
        with self.use_config():
            return htcondor.Collector()

    def status(self, ad_type=htcondor.AdTypes.Any, constraint="true", projection=None):
        """
        Perform a status query against the pool's collector.

        Parameters
        ----------
        ad_type
        constraint
        projection

        Returns
        -------

        """
        projection = projection or []

        with self.use_config():
            result = self.get_local_collector().query(
                ad_type=ad_type, constraint=constraint, projection=projection
            )

        logger.debug(
            'Ads returned by status query for {} ads with constraint "{}":\n'.format(
                ad_type, constraint
            )
            + "\n".join(str(ad) for ad in result)
        )

        return result

    def direct_status(self, daemon_type, ad_type, constraint="true", projection=None):
        """
        Perform a direct status query against a daemon.

        Parameters
        ----------
        daemon_type
        ad_type
        constraint
        projection

        Returns
        -------

        """
        projection = projection or []

        # TODO: we would like this to use Collector.directQuery, but it can't because of https://htcondor-wiki.cs.wisc.edu/index.cgi/tktview?tn=7420

        with self.use_config():
            daemon_location = self.get_local_collector().locate(daemon_type)

            # pretend the target daemon is a collector so we can query it
            daemon = htcondor.Collector(daemon_location["MyAddress"])
            result = daemon.query(
                ad_type=ad_type, constraint=constraint, projection=projection
            )

        logger.debug(
            'Ads returned by direct status query against {} for {} ads with constraint "{}":\n'.format(
                daemon_type, ad_type, constraint
            )
            + "\n".join(str(ad) for ad in result)
        )

        return result

    def query(
        self,
        constraint="true",
        projection=None,
        limit=-1,
        opts=htcondor.QueryOpts.Default,
    ):
        """
        Perform a job information query against the pool's schedd.

        Parameters
        ----------
        constraint
        projection
        limit
        opts

        Returns
        -------

        """
        if projection is None:
            projection = []

        with self.use_config():
            result = self.get_local_schedd().query(
                constraint=constraint, projection=projection, limit=limit, opts=opts
            )

        logger.debug(
            'Ads returned by queue query with constraint "{}":\n'.format(constraint)
            + "\n".join(str(ad) for ad in result)
        )

        return result

    def act(self, action, constraint="true"):
        """
        Perform a job action against the pool's schedd.

        Parameters
        ----------
        action
        constraint

        Returns
        -------

        """
        with self.use_config():
            logger.debug(
                'Executing action: {} with constraint "{}"'.format(action, constraint)
            )
            return self.get_local_schedd().act(action, constraint)

    def edit(self, attr, value, constraint="true"):
        """
        Perform a job attribute edit action against the pool's schedd.

        Parameters
        ----------
        attr
        value
        constraint

        Returns
        -------

        """
        with self.use_config():
            logger.debug(
                'Executing edit: setting {} to {} with constraint "{}"'.format(
                    attr, value, constraint
                )
            )
            return self.get_local_schedd().edit(constraint, attr, value)

    def submit(
        self, description: Mapping[str, str], count=1, itemdata=None
    ) -> handles.ClusterHandle:
        """
        Submit jobs to the pool.

        Internally, this method uses the Python bindings to submit jobs and
        returns a rich handle object (a :class:`ClusterHandle`)
        that can be used to inspect and manage the jobs.

        .. warning::

            If your intent is to test job submission itself, **DO NOT** use this method.
            Instead, submit jobs by calling `condor_q` with :meth:`Condor.run_command`,
            using the Python bindings directly, or whatever other method you want
            to test.

        Parameters
        ----------
        description
        count
        itemdata

        Returns
        -------

        """
        sub = htcondor.Submit(dict(description))
        logger.debug(
            "Submitting jobs with description:\n{}\nCount: {}\nItemdata: {}".format(
                sub, count, itemdata
            )
        )

        if self.submit_user:
            euid = os.geteuid()
            egid = os.getegid()
            try_os_setegid( name=self.submit_user )
            try_os_seteuid( name=self.submit_user )
        with self.use_config():
            schedd = self.get_local_schedd()
            result = schedd.submit(sub, count=count, itemdata=itemdata)
            logger.debug("Got submit result:\n{}".format(result))
        if self.submit_user:
            try_os_setegid( number=egid )
            try_os_seteuid( number=euid )

        return handles.ClusterHandle(self, result)

    def submit_dag(self, dagfile: Path) -> handles.ClusterHandle:
        """
        Submit a DAG.

        Parameters
        ----------
        dagfile

        Returns
        -------

        """
        return self.submit(htcondor.Submit.from_dag(str(dagfile)))

    def _make_job_queue_log_public_if_root(self):
        if self.use_sudo:
            # If we're running condor_master as root, then the job queue log will be owned by root and not readable by the unprivileged user running this code, so chmod it to be world-readable
            cmd.run_command(
                ["sudo", "chmod", "0666", self.job_queue_log.as_posix()],
                echo=False,
                suppress=True,
            )

RE_PORT_HOST = re.compile(r"\d+\.\d+\.\d+\.\d+:\d+")


def get_port_host_from_sinful(sinful):
    return RE_PORT_HOST.search(sinful)[0]
