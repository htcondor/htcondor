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

import os as _os
import platform as _platform
from contextlib import contextmanager as _contextmanager

# SET UP NULL LOG HANDLER
_logger = _logging.getLogger(__name__)
_logger.setLevel(_logging.DEBUG)
_logger.addHandler(_logging.NullHandler())

def _add_dll_dir():
    """
    On windows for Python 3.8 or later, we have to add the bin directory to the search path
    for DLLs because python will no longer use the PATH environment variable to find DLLs.
    We assume here that this file is in $(RELEASE_DIR)\lib\python\classd and that the
    bin directory is relative to it at $(RELEASE_DIR)\bin
    """
    if _platform.system() in ["Windows"]:
        import sys as _sys
        if _sys.version_info >= (3,8):
            bin_path = _os.path.realpath(_os.path.join(__file__,r'..\..\..\..\bin'))
            return _os.add_dll_directory(bin_path)

    # Return a noop context manager
    @_contextmanager
    def _noop():
        yield None
    return _noop()

with _add_dll_dir():
    from .classad import *

__version__ = version()
