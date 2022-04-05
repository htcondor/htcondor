# Copyright 2020 HTCondor Team, Computer Sciences Department,
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

import atexit
import enum
import functools
import logging
import os
import shlex
import signal
import subprocess
import textwrap
import time
from pathlib import Path
from typing import List, Mapping, Optional, Set

import classad
import htcondor

__all__ = ["PersonalPool", "PersonalPoolState", "SetCondorConfig"]

logger = logging.getLogger(__name__)

IS_WINDOWS = os.name == "nt"

AUTH_METHODS = ["FS" if not IS_WINDOWS else "NTSSPI", "PASSWORD", "IDTOKENS"]

DEFAULT_CONFIG = {
    "LOCAL_CONFIG_FILE": "",
    "MASTER_ADDRESS_FILE": "$(LOG)/.master_address",
    "COLLECTOR_ADDRESS_FILE": "$(LOG)/.collector_address",
    "SCHEDD_ADDRESS_FILE": "$(LOG)/.schedd_address",
    "JOB_QUEUE_LOG": "$(SPOOL)/job_queue.log",
    # TUNING
    "UPDATE_INTERVAL": "2",
    "POLLING_INTERVAL": "2",
    "NEGOTIATOR_INTERVAL": "2",
    "STARTER_UPDATE_INTERVAL": "2",
    "STARTER_INITIAL_UPDATE_INTERVAL": "2",
    "NEGOTIATOR_CYCLE_DELAY": "2",
    # SECURITY
    # TODO: revisit security; it would be much better to not hand-roll this
    "SEC_DAEMON_AUTHENTICATION": "REQUIRED",
    "SEC_CLIENT_AUTHENTICATION": "REQUIRED",
    "SEC_DEFAULT_AUTHENTICATION_METHODS": ", ".join(AUTH_METHODS),
    "self": "$(USERNAME)@$(UID_DOMAIN) $(USERNAME)@$(IPV4_ADDRESS) $(USERNAME)@$(IPV6_ADDRESS) $(USERNAME)@$(FULL_HOSTNAME) $(USERNAME)@$(HOSTNAME)",
    "ALLOW_READ": "$(self)",
    "ALLOW_WRITE": "$(self)",
    "ALLOW_DAEMON": "$(self)",
    "ALLOW_ADMINISTRATOR": "$(self)",
    "ALLOW_ADVERTISE": "$(self)",
    "ALLOW_NEGOTIATOR": "$(self)",
    "ALLOW_READ_COLLECTOR": "$(ALLOW_READ)",
    "ALLOW_READ_STARTD": "$(ALLOW_READ)",
    "ALLOW_NEGOTIATOR_SCHEDD": "$(ALLOW_NEGOTIATOR)",
    "ALLOW_WRITE_COLLECTOR": "$(ALLOW_WRITE)",
    "ALLOW_WRITE_STARTD": "$(ALLOW_WRITE)",
    # SLOT CONFIG
    "NUM_SLOTS": "1",
    "NUM_SLOTS_TYPE_1": "1",
    "SLOT_TYPE_1": "100%",
    "SLOT_TYPE_1_PARTITIONABLE": "TRUE",
}

ROLES = ["Personal"]
FEATURES = ["GPUs"]

INHERIT = {
    "RELEASE_DIR",
    "LIB",
    "BIN",
    "SBIN",
    "INCLUDE",
    "LIBEXEC",
    "SHARE",
    "AUTH_SSL_SERVER_CAFILE",
    "AUTH_SSL_CLIENT_CAFILE",
    "AUTH_SSL_SERVER_CERTFILE",
    "AUTH_SSL_SERVER_KEYFILE",
}

INHERITED_PARAMS = {k: v for k, v in htcondor.param.items() if k in INHERIT}


def _skip_if(*states):
    """Should only be applied to PersonalPool methods that return self."""
    states = set(states)

    def decorator(func):
        @functools.wraps(func)
        def wrapper(self, *args, **kwargs):
            if self.state in states:
                logger.debug(
                    "Skipping call to {} for {} because it is {}.".format(
                        func.__name__, self, self.state
                    )
                )
                return self

            return func(self, *args, **kwargs)

        return wrapper

    return decorator


class PersonalPoolState(str, enum.Enum):
    """
    An enumeration of the possible states that a :class:`PersonalPool` can be in.
    """

    UNINITIALIZED = "UNINITIALIZED"
    INITIALIZED = "INITIALIZED"
    STARTING = "STARTING"
    READY = "READY"
    STOPPING = "STOPPING"
    STOPPED = "STOPPED"


class PersonalPool:
    """
    A :class:`PersonalPool` is responsible for managing the lifecycle of a
    personal HTCondor pool. It can be used to start and stop a personal pool,
    and can also "attach" to an existing personal pool that is already running.
    """

    def __init__(
        self,
        local_dir: Optional[Path] = None,
        config: Mapping[str, str] = None,
        raw_config: Optional[str] = None,
        detach: bool = False,
        use_config: bool = True,
    ):
        """
        Parameters
        ----------
        local_dir
            The local directory for the personal HTCondor pool.
            All configuration and state for the personal pool
            will be stored in this directory.
        config
            HTCondor configuration parameters to inject,
            as a mapping of key-value pairs.
        raw_config
            Raw HTCondor configuration language to inject,
            as a string.
        detach
            If ``True``, the personal HTCondor pool will not be shut down
            when this object is destroyed (e.g., by stopping Python).
            Defaults to ``False``.
        use_config
            If ``True``, the environment variable ``CONDOR_CONFIG`` will be
            set during initialization, such that this personal pool appears to
            be the local HTCondor pool for all operations in this Python session,
            even ones that don't go through the :class:`PersonalPool` object.
            The personal pool will also be initialized.
            Defaults to ``True``.
        """
        self._state = PersonalPoolState.UNINITIALIZED
        atexit.register(self._atexit)

        if local_dir is None:
            local_dir = Path.home() / ".condor" / "personal"
        self.local_dir = Path(local_dir).absolute()

        self._detach = detach

        self.execute_dir = self.local_dir / "execute"
        self.lock_dir = self.local_dir / "lock"
        self.log_dir = self.local_dir / "log"
        self.run_dir = self.local_dir / "run"
        self.spool_dir = self.local_dir / "spool"
        self.passwords_dir = self.local_dir / "passwords.d"
        self.tokens_dir = self.local_dir / "tokens.d"
        self.system_tokens_dir = self.local_dir / "system_tokens.d"

        self.config_file = self.local_dir / "condor_config"

        if config is None:
            config = {}
        self._config = {k: v if v is not None else "" for k, v in config.items()}
        self._raw_config = raw_config or ""

        self.condor_master = None

        if use_config:
            self.initialize()
            logger.debug("Setting CONDOR_CONFIG globally for {}".format(self))
            self.use_config().set()

    @property
    def state(self):
        """The current :class:`PersonalPoolState` of the personal pool."""
        return self._state

    @state.setter
    def state(self, state):
        old_state = self._state
        self._state = state
        logger.debug("State of {} changed from {} to {}".format(self, old_state, state))

    def __repr__(self):
        shortest_path = min(
            (
                str(self.local_dir),
                "~/" + str(_try_relative_to(self.local_dir, Path.home())),
                "./" + str(_try_relative_to(self.local_dir, Path.cwd())),
            ),
            key=len,
        )

        return "{}(local_dir={}, state={})".format(
            type(self).__name__, shortest_path, self.state
        )

    def use_config(self):
        """
        Returns a :class:`SetCondorConfig` context manager that sets
        ``CONDOR_CONFIG`` to point to the configuration file for this personal pool.
        """
        return SetCondorConfig(self.config_file)

    @property
    def collector(self):
        """
        The :class:`htcondor.Collector` for the personal pool's collector.
        """
        with self.use_config():
            # This odd construction ensure that the Collector we return
            # doesn't just point to "the local collector" - that could be
            # overridden by changing CONDOR_CONFIG after the Collector
            # was initialized. Locating first keeps it stable.
            return htcondor.Collector(
                htcondor.Collector().locate(htcondor.DaemonTypes.Collector)
            )

    @property
    def schedd(self):
        """
        The :class:`htcondor.Schedd` for the personal pool's schedd.
        """
        with self.use_config():
            return htcondor.Schedd()

    def __enter__(self):
        self.use_config().set()
        return self.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        logger.debug("Stop triggered for {} by context exit.".format(self))
        self.stop()
        self.use_config().unset()

    def __del__(self):
        logger.debug("Stop triggered for {} by object deletion.".format(self))
        self.stop()

    def _atexit(self):
        logger.debug("Stop triggered for {} by interpreter shutdown.".format(self))
        self.stop()

    @_skip_if(PersonalPoolState.READY)
    def start(self) -> "PersonalPool":
        """
        Start the personal condor (bringing it to the ``READY`` state from
        either ``UNINITIALIZED`` or ``INITIALIZED``).

        Returns
        -------
        self : PersonalPool
            This method returns ``self``.
        """
        logger.info("Starting {}".format(self))

        try:
            self.initialize()
            self._start_condor()
            self._wait_for_ready()
        except BaseException:
            logger.exception(
                "Encountered error during setup of {}, cleaning up!".format(self)
            )
            self.stop()
            raise

        logger.info("Started {}".format(self))

        return self

    @_skip_if(
        PersonalPoolState.INITIALIZED,
        PersonalPoolState.STARTING,
        PersonalPoolState.READY,
    )
    def initialize(self, overwrite_config=True) -> "PersonalPool":
        """
        Initialize the personal pool by creating its local directory and writing
        out configuration files.

        The contents of the local directory
        (except for the configuration file if ``overwrite_config=True``)
        will not be overridden.

        Parameters
        ----------
        overwrite_config
            If ``True``, the existing configuration file will be overwritten
            with the configuration set up in the constructor.
            If ``False`` and there is an existing configuration file, an
            exception will be raised.
            Defaults to ``True``.

        Returns
        -------
        self : PersonalPool
            This method returns ``self``.
        """
        self._setup_local_dirs()
        self._write_config(overwrite=overwrite_config)

        self.state = PersonalPoolState.INITIALIZED

        return self

    def _setup_local_dirs(self):
        for dir in (
            self.local_dir,
            self.execute_dir,
            self.lock_dir,
            self.log_dir,
            self.run_dir,
            self.spool_dir,
            self.passwords_dir,
            self.tokens_dir,
            self.system_tokens_dir,
        ):
            dir.mkdir(parents=True, exist_ok=True)

        self.passwords_dir.chmod(0o700)
        self.tokens_dir.chmod(0o700)
        self.system_tokens_dir.chmod(0o700)

    def _write_config(self, overwrite: bool = True) -> None:
        if not overwrite and self.config_file.exists():
            raise FileExistsError(
                "Found existing config file; refusing to write config because overwrite={}.".format(
                    overwrite
                )
            )

        self.config_file.parent.mkdir(parents=True, exist_ok=True)

        param_lines = []

        param_lines += ["# INHERITED"]
        param_lines += ["{} = {}".format(k, v) for k, v in INHERITED_PARAMS.items()]

        param_lines += ["# ROLES"]
        param_lines += ["use ROLE: {}".format(role) for role in ROLES]

        param_lines += ["# FEATURES"]
        param_lines += ["use FEATURE: {}".format(feature) for feature in FEATURES]

        base_config = {
            "LOCAL_DIR": self.local_dir.as_posix(),
            "EXECUTE": self.execute_dir.as_posix(),
            "LOCK": self.lock_dir.as_posix(),
            "LOG": self.log_dir.as_posix(),
            "RUN": self.run_dir.as_posix(),
            "SPOOL": self.spool_dir.as_posix(),
            "SEC_PASSWORD_DIRECTORY": self.passwords_dir.as_posix(),
            "SEC_TOKEN_DIRECTORY": self.tokens_dir.as_posix(),
            "SEC_TOKEN_SYSTEM_DIRECTORY": self.system_tokens_dir.as_posix(),
            "MAIL": "/bin/true",
            "SENDMAIL": "/bin/true",
        }

        param_lines += ["# BASE PARAMS"]
        param_lines += ["{} = {}".format(k, v) for k, v in base_config.items()]

        param_lines += ["# DEFAULT PARAMS"]
        param_lines += ["{} = {}".format(k, v) for k, v in DEFAULT_CONFIG.items()]

        param_lines += ["# CUSTOM PARAMS"]
        param_lines += ["{} = {}".format(k, v) for k, v in self._config.items()]

        param_lines += ["# RAW PARAMS"]
        param_lines += textwrap.dedent(self._raw_config).splitlines()

        params = "\n".join(param_lines + [""])
        self.config_file.write_text(params)

    @_skip_if(PersonalPoolState.STARTING, PersonalPoolState.READY)
    def _start_condor(self):
        if self._is_ready():
            raise Exception(
                "Cannot start a {t} in the same local_dir as an already-running {t}.".format(
                    t=type(self).__name__
                )
            )

        with SetCondorConfig(self.config_file):
            self.condor_master = subprocess.Popen(
                ["condor_master", "-f"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
            )
            logger.debug(
                "Started condor_master (pid {}) for {}".format(
                    self.condor_master.pid, self
                )
            )

        self.state = PersonalPoolState.STARTING

        return self

    def _daemons(self) -> Set[str]:
        return set(self.get_config_val("DAEMON_LIST").split(" "))

    @_skip_if(PersonalPoolState.READY)
    def _wait_for_ready(self, timeout=120):
        daemons = self._daemons()
        master_log_path = self._master_log

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
                    "MASTER_LOG at {} does not yet exist for {}, retrying in 1 seconds (giving up in {} seconds).".format(
                        master_log_path, self, time_to_give_up
                    )
                )
                time.sleep(1)
                continue

            who = self.run_command(
                shlex.split(
                    "condor_who -wait:10 'IsReady && STARTD_State =?= \"Ready\"'"
                ),
            )
            if who.stdout.strip() == "":
                logger.debug(
                    "condor_who stdout was unexpectedly blank for {}, retrying in 1 second (giving up in {} seconds). condor_who stderr:\n{}".format(
                        self, time_to_give_up, who.stderr
                    )
                )
                time.sleep(1)
                continue

            who_ad = classad.parseOne(who.stdout)

            if (
                who_ad.get("IsReady")
                and who_ad.get("STARTD_State") == "Ready"
                and all(who_ad.get(d) == "Alive" for d in daemons)
            ):
                self.state = PersonalPoolState.READY
                return self

            logger.debug(
                "{} is waiting for daemons to be ready (giving up in {} seconds)".format(
                    self, time_to_give_up
                )
            )

        raise TimeoutError("Standup for {} failed".format(self))

    def who(self) -> classad.ClassAd:
        """
        Return the result of ``condor_who -quick``,
        as a :class:`classad.ClassAd`.
        If ``condor_who -quick`` fails, or the output can't be parsed into
        a sensible who ad, this method returns an empty ad.
        """
        who = self.run_command(["condor_who", "-quick"])

        try:
            parsed = classad.parseOne(who.stdout)

            # If there's no MASTER key in the parsed ad, it indicates
            # that we actually got the special post-shutdown message
            # from condor_who and should act like there's nothing there.
            if "MASTER" not in parsed:
                return classad.ClassAd()

            return parsed
        except Exception:
            return classad.ClassAd()

    def _condor_master_is_alive(self) -> bool:
        if self.condor_master is not None:
            return self.condor_master.poll() is None
        else:
            return bool(self.who())

    def _master_pid(self) -> int:
        if self.condor_master is not None:
            return self.condor_master.pid
        else:
            return int(self.who()["MASTER_PID"])

    def _daemon_pids(self) -> List[int]:
        return [int(v) for k, v in self.who() if k.endswith("_PID")]

    def _is_ready(self) -> bool:
        return self.who().get("IsReady", False)

    @_skip_if(
        PersonalPoolState.UNINITIALIZED,
        PersonalPoolState.INITIALIZED,
        PersonalPoolState.STOPPING,
        PersonalPoolState.STOPPED,
    )
    def stop(self):
        """
        Stop the personal condor, bringing it from the ``READY`` state to
        ``STOPPED``.

        Returns
        -------
        self : PersonalPool
            This method returns ``self``.
        """
        if self._detach:
            logger.debug("Will not stop {} because it is detached".format(self))
            return self

        logger.info("Stopping {}".format(self))

        self.state = PersonalPoolState.STOPPING

        self._condor_off()
        self._wait_for_master_to_terminate()

        self.state = PersonalPoolState.STOPPED

        logger.info("Stopped {}".format(self))

        return self

    def _condor_off(self):
        if not self._condor_master_is_alive():
            return

        off = self.run_command(["condor_off", "-daemon", "master"], timeout=30)

        if not off.returncode == 0:
            logger.error(
                "condor_off failed for {}, exit code: {}, stderr: {}".format(
                    self, off.returncode, off.stderr
                )
            )
            self._terminate_condor_master()
            return

        logger.debug("condor_off succeeded for {}: {}".format(self, off.stdout))

    def _wait_for_master_to_terminate(self, kill_after: int = 60, timeout: int = 120):
        logger.debug(
            "Waiting for condor_master (pid {}) for {} to terminate".format(
                self._master_pid(), self
            )
        )

        start = time.time()
        killed = False
        while True:
            if not self._condor_master_is_alive():
                break

            elapsed = time.time() - start

            if not killed:
                logger.debug(
                    "condor_master for {} has not terminated yet, will kill in {} seconds.".format(
                        self, int(kill_after - elapsed)
                    )
                )

            if elapsed > kill_after and not killed:
                self._kill_condor_system()
                killed = True

            if elapsed > timeout:
                raise TimeoutError(
                    "Timed out while waiting for condor_master to terminate."
                )

            time.sleep(1)

        logger.debug("condor_master for {} has terminated.".format(self))

    def _terminate_condor_master(self):
        if not self._condor_master_is_alive():
            return

        pid = self._master_pid()

        if self.condor_master is not None:
            self.condor_master.terminate()
        else:
            os.kill(pid, signal.SIGTERM)

        logger.debug(
            "Sent terminate signal to condor_master (pid {}) for {}".format(pid, self)
        )

    def _kill_condor_system(self):
        if self._condor_master_is_alive():
            return

        pids = self._daemon_pids()
        for pid in pids:
            os.kill(pid, signal.SIGKILL)

        logger.debug(
            "Sent kill signals to condor daemons (pids {}) for {}".format(
                ", ".join(map(str, pids)), self
            )
        )

    def run_command(
        self,
        args: List[str],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines: bool = True,
        **kwargs
    ) -> subprocess.CompletedProcess:
        """
        Execute a command in a subprocess against this personal pool,
        using :func:`subprocess.run` with good defaults for executing
        HTCondor commands.
        All of the keyword arguments of this function are passed directly to
        :func:`subprocess.run`.

        Parameters
        ----------
        args
            The command to run, and its arguments, as a list of strings.
        kwargs
            All keyword arguments
            (including ``stdout``, ``stderr``, and ``universal_newlines``)
            are passed to :func:`subprocess.run`.

        Returns
        -------
        completed_process : subprocess.CompletedProcess
        """
        with self.use_config():
            p = subprocess.run(
                list(map(str, args)),
                stdout=stdout,
                stderr=stderr,
                universal_newlines=universal_newlines,
                **kwargs,
            )

            p.stdout = p.stdout.rstrip()
            p.stderr = p.stderr.rstrip()

            return p

    @property
    def _master_log(self) -> Path:
        return Path(self.get_config_val("MASTER_LOG"))

    def get_config_val(self, macro: str, default: Optional[str] = None) -> str:
        """
        Get the value of a configuration macro.
        The value will be "evaluated", meaning that other configuration macros
        or functions inside it will be expanded.

        Parameters
        ----------
        macro
            The configuration macro to look up the value for.
        default
            If not ``None``, and the config macro has no value, return this instead.
            If ``None``, a :class:`KeyError` will be raised instead.

        Returns
        -------
        value : str
            The evaluated value of the configuration macro.
        """
        with self.use_config():
            try:
                return htcondor.param[macro]
            except KeyError:
                if default is not None:
                    return default
                raise

    @classmethod
    def attach(cls, local_dir: Optional[Path] = None) -> "PersonalPool":
        """
        Make a new :class:`PersonalPool` attached to an existing personal pool
        that is already running in ``local_dir``.

        Parameters
        ----------
        local_dir
            The local directory for the existing personal pool.

        Returns
        -------
        self : PersonalPool
            This method returns ``self``.
        """
        pool = cls(local_dir=local_dir)

        if not pool._is_ready():
            raise Exception(
                "There is not already a running HTCondor instance for {}.".format(pool)
            )

        pool.state = PersonalPoolState.READY

        return pool

    def detach(self) -> "PersonalPool":
        """
        Detach the personal pool
        (as in the constructor argument),
        and return ``self``.
        """
        self._detach = True
        return self


def _try_relative_to(path: Path, to: Path) -> Path:
    try:
        return path.relative_to(to)
    except ValueError:
        return path


class SetCondorConfig:
    """
    A context manager. Inside the block, the Condor config file is the one given
    to the constructor. After the block, it is reset to whatever it was before
    the block was entered.
    """

    def __init__(self, config_file: Path):
        """
        Parameters
        ----------
        config_file
            The path to an HTCondor configuration file.
        """
        self.config_file = Path(config_file)
        self.previous_value = None

    def set(self):
        """Set ``CONDOR_CONFIG`` and tell HTCondor to reconfigure."""
        self.previous_value = os.environ.get("CONDOR_CONFIG", None)
        _set_env_var("CONDOR_CONFIG", str(self.config_file))

        htcondor.reload_config()

    def unset(self):
        """Un-set ``CONDOR_CONFIG`` and tell HTCondor to reconfigure."""
        if self.previous_value is not None:
            _set_env_var("CONDOR_CONFIG", self.previous_value)
            htcondor.reload_config()
        else:
            _unset_env_var("CONDOR_CONFIG")

    def __enter__(self):
        self.set()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.unset()


def _set_env_var(key: str, value: str):
    os.environ[key] = value


def _unset_env_var(key: str):
    value = os.environ.get(key, None)

    if value is not None:
        del os.environ[key]
