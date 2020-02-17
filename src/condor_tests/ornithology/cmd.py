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

from typing import List, Tuple

import logging

import os
import subprocess
import re


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def run_command(
    args: List[str], stdin=None, timeout: int = 60, echo=False, suppress=False
):
    """
    Execute a command.

    Parameters
    ----------
    args
    stdin
    timeout
    echo
    suppress

    Returns
    -------

    """
    if timeout is None:
        raise TypeError("run_command timeout cannot be None")

    args = list(map(str, args))
    p = subprocess.run(
        args,
        timeout=timeout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        stdin=stdin,
        universal_newlines=True,
    )
    p.stdout = p.stdout.rstrip()
    p.stderr = p.stderr.rstrip()

    msg_lines = ["Ran command: {}".format(" ".join(p.args))]
    if not suppress:
        msg_lines += [
            "CONDOR_CONFIG = {}".format(os.environ.get("CONDOR_CONFIG", "<not set>")),
            "exit code: {}".format(p.returncode),
            "stdout:{}{}".format("\n" if "\n" in p.stdout else " ", p.stdout),
            "stderr:{}{}".format("\n" if "\n" in p.stderr else " ", p.stderr),
        ]

    msg = "\n".join(msg_lines)
    logger.debug(msg)
    if echo:
        print(msg)

    return p


RE_SUBMIT_RESULT = re.compile(r"(\d+) job\(s\) submitted to cluster (\d+)\.")


def parse_submit_result(submit_cmd: subprocess.CompletedProcess) -> Tuple[int, int]:
    """
    Get a "submit result" from a ``condor_submit`` command run via
    :func:`run_command`.

    Parameters
    ----------
    submit_cmd

    Returns
    -------

    """
    match = RE_SUBMIT_RESULT.search(submit_cmd.stdout)
    if match is not None:
        num_procs = int(match.group(1))
        clusterid = int(match.group(2))
        return clusterid, num_procs

    raise ValueError(
        'Was not able to extract submit results from command "{}", stdout:\n{}\nstderr:\n{}'.format(
            " ".join(submit_cmd.args), submit_cmd.stdout, submit_cmd.stderr
        )
    )
