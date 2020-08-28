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

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)
logger.addHandler(logging.NullHandler())

from .cmd import run_command, parse_submit_result
from .condor import Condor, get_port_host_from_sinful
from .daemons import DaemonLog, DaemonLogStream, DaemonLogMessage
from .env import SetEnv, SetCondorConfig, ChangeDir
from .helpers import in_order, track_quantity
from .io import write_file
from .job_queue import SetAttribute, SetJobStatus, JobQueue
from .jobs import JobID, JobStatus
from .handles import Handle, ConstraintHandle, ClusterHandle, ClusterState, EventLog
from .meta import get_current_func_name
from .utils import chain_get, format_script
from .fixtures import config, standup, action, CONFIG_IDS
from .scripts import SCRIPTS
