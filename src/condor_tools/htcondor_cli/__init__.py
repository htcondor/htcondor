# Create the base logger
import logging as _logging
_logger = _logging.getLogger(__name__)
_logger.setLevel(_logging.DEBUG)

import htcondor2

# Set the TMPDIR
from pathlib import Path as _Path
from os import environ as _environ
from os import name as _os_name
from tempfile import gettempdir as _gettempdir
from time import time as _time
TMP_DIR = _Path(_environ.get("_CONDOR_TMPDIR", _gettempdir())) / "htcondor_cli" / str(_time())

# Dict of common command-line options, flags go in the "args" tuple,
# otherwise, use kwargs from:
# https://docs.python.org/3/library/argparse.html#the-add-argument-method
GLOBAL_OPTIONS = {
    "verbose": {
        "args": ("-v", "--verbose"),
        "action": "count",
        "default": 0,
        "help": "Increase verbosity of output (can be specified multiple times)",
    },
    "version": {
        "args": ("-V", "--version"),
        "action": "version",
        "version": htcondor2.version(),
        "help": "Print the HTCondor version and exit"
    },
    "quiet": {
        "args": ("-q", "--quiet"),
        "action": "count",
        "default": 0,
        "help": "Decrease verbosity of output (can be specified multiple times)",
    },
}

# Global objects

# Must be consistent with job status definitions in
# src/condor_includes/proc.h
JobStatus = [
    "NONE",
    "IDLE",
    "RUNNING",
    "REMOVED",
    "COMPLETED",
    "HELD",
    "TRANSFERRING_OUTPUT",
    "SUSPENDED",
    "JOB_STATUS_MAX"
]

# Must be consistent with dag status definitions in
# src/condor_utils/dagman_utils.h
DagStatus = [
    "OK",
    "ERROR",
    "FAILED-NODE",
    "ABORT-ON-SIGNAL",
    "REMOVED",
    "CYCLE",
    "HALTED"
]

# Import all of the noun classes (do this section last) and then
# create an OrderedDict of nouns, mapping the name to be used on the
# command line to the name of the class containing the noun's verbs.
from collections import OrderedDict as _OrderedDict
from htcondor_cli.dagman import DAG
from htcondor_cli.job import Job
from htcondor_cli.job_set import JobSet
from htcondor_cli.eventlog import EventLog
from htcondor_cli.credential import Credential
from htcondor_cli.server import Server
from htcondor_cli.access_point import AccessPoint
from htcondor_cli.central_manager import CentralManager
from htcondor_cli.ocu import OCU
NOUNS = _OrderedDict()
NOUNS["dag"] = DAG
NOUNS["job"] = Job
NOUNS["jobset"] = JobSet
NOUNS["eventlog"] = EventLog
NOUNS["credential"] = Credential
NOUNS["server"] = Server
NOUNS["ap"] = AccessPoint
NOUNS["cm"] = CentralManager
NOUNS["ocu"] = OCU

# annex needs fcntl which does not exist on windows
if _os_name != 'nt':
    from htcondor_cli.annex import Annex
    NOUNS["annex"] = Annex
