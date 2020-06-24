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

# SET UP NULL LOG HANDLER
_logger = _logging.getLogger(__name__)
_logger.setLevel(_logging.DEBUG)
_logger.addHandler(_logging.NullHandler())

import platform
import warnings
import os.path

# check for condor_config
if "CONDOR_CONFIG" in os.environ:
    pass


# if condor_config does not exist on Linux, use /dev/null
elif platform.system() in ["Linux", "Darwin"]:
    condor_config_paths = [
        "/etc/condor/condor_config",
        "/usr/local/etc/condor_config",
        os.path.expanduser("~condor/condor_config"),
    ]

    if not (True in [os.path.isfile(path) for path in condor_config_paths]):
        os.environ["CONDOR_CONFIG"] = "/dev/null"
        message = """
Using a null condor_config.
Neither the environment variable CONDOR_CONFIG, /etc/condor/,
/usr/local/etc/, nor ~condor/ contain a condor_config source."""
        warnings.warn(message)

from .htcondor import *

# get the version using regexp ideally, and fall back to basic string parsing
import re as _re
try:
    __version__ = _re.match('^.*(\d+\.\d+\.\d+)', version()).group(1)
except (AttributeError, IndexError):
    __version__ = version().split()[1]
