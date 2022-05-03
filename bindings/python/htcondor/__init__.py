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

import logging as _logging

import platform as _platform
import warnings as _warnings
import os as _os
from os import path as _path
import re as _re
from contextlib import contextmanager as _contextmanager

# SET UP NULL LOG HANDLER
_logger = _logging.getLogger(__name__)
_logger.setLevel(_logging.DEBUG)
_logger.addHandler(_logging.NullHandler())


def _check_for_config():
    """
    Check for CONDOR_CONFIG.

    If CONDOR_CONFIG does not exist on Linux, explicitly use /dev/null instead.
    """
    if "CONDOR_CONFIG" in _os.environ:
        return

    if _platform.system() in ["Linux", "Darwin"]:
        condor_config_paths = (
            "/etc/condor/condor_config",
            "/usr/local/etc/condor_config",
            _path.expanduser("~condor/condor_config"),
        )

        if not any(_path.isfile(path) for path in condor_config_paths):
            message = "Neither the environment variable CONDOR_CONFIG, /etc/condor/, /usr/local/etc/, nor ~condor/ contain a condor_config source. Therefore, we are using a null condor_config."
            _warnings.warn(message)
            _os.environ["CONDOR_CONFIG"] = "/dev/null"

def _add_dll_dir():
    """
    On windows for Python 3.8 or later, we have to add the bin directory to the search path for DLLs
    Because python will no longer use the PATH environment variable to find dlls.
    We assume here that this file is in $(RELEASE_DIR)\lib\python\htcondor and that the
    bin directory is relative to it at $(RELEASE_DIR)\bin
    """
    if _platform.system() in ["Windows"]:
        import sys as _sys
        if _sys.version_info >= (3,8):
            bin_path = _path.realpath(_path.join(__file__,r'..\..\..\..\bin'))
            return _os.add_dll_directory(bin_path)

    # Return a noop context manager
    @_contextmanager
    def _noop():
        yield None
    return _noop()

_check_for_config()

with _add_dll_dir():
    import classad
    from . import htcondor, _lock

# get the version using regexp ideally, and fall back to basic string parsing
try:
    __version__ = _re.match("^.*(\d+\.\d+\.\d+)", htcondor.version()).group(1)
except (AttributeError, IndexError):
    __version__ = htcondor.version().split()[1]

# add locks in-place
_lock.add_locks(htcondor, skip=_lock.DO_NOT_LOCK)

from . import _deprecation

# deprecate in-place
_deprecation.deprecate_8_9_8()

# re-import htcondor with a splat to get it into our namespace
# because of import caching, this respects the mutation we did above
from .htcondor import *
from .htcondor import _Param

from ._job_status import JobStatus
