# Create the base logger
import logging as _logging
_logger = _logging.getLogger(__name__)
_logger.setLevel(_logging.DEBUG)

# Set the TMPDIR
from pathlib import Path as _Path
from os import environ as _environ
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
    "quiet": {
        "args": ("-q", "--quiet"),
        "action": "count",
        "default": 0,
        "help": "Decrease verbosity of output (can be specified multiple times)",
    },
}


# Import all of the noun classes (do this section last) and then
# create an OrderedDict of nouns, mapping the name to be used on the
# command line to the name of the class containing the noun's verbs.
from collections import OrderedDict as _OrderedDict
from htcondor_cli.job import Job
from htcondor_cli.job_set import JobSet
NOUNS = _OrderedDict()
NOUNS["job"] = Job
NOUNS["jobset"] = JobSet

