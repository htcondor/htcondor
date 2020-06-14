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

from typing import Callable

import logging
import re
import datetime
import time
from pathlib import Path

from . import exceptions

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class DaemonLog:
    """
    Represents the log file for a particular HTCondor daemon. Can be used to
    open multiple "views" (:class:`DaemonLogStream`) of the log file.

    .. warning::

        You shouldn't need to create these yourself. Instead, see the ``<daemon>_log``
        methods on :class:`Condor`.
    """

    def __init__(self, path: Path):
        self.path = path

    def open(self):
        """
        Return a :class:`DaemonLogStream` pointing to the daemon's log file.

        Returns
        -------

        """
        return DaemonLogStream(self.path.open(mode="r", encoding="utf-8"))


class DaemonLogStream:
    def __init__(self, file):
        self.file = file
        self.messages = []
        self.line_number = 0

    @property
    def lines(self):
        """
        An iterator over the raw lines that have been read from the daemon log so far.
        """
        yield from (msg.line for msg in self.messages)

    def read(self):
        """
        Read lines from the daemon log and parse them into :class:`DaemonLogMessage`.
        Returns an iterator over the messages as they are parsed.

        .. warning::

            If you do not consume the iterator, no lines will be read!

        """
        for line in self.file:
            line = line.strip()
            if line == "":
                continue
            try:
                msg = DaemonLogMessage(line.strip(), self.file.name, self.line_number)
                self.messages.append(msg)
                yield msg
            except exceptions.DaemonLogParsingFailed as e:
                logger.warning(e)
            self.line_number += 1

    def clear(self):
        """
        Clear the internal message store; useful for isolating tests.
        """
        self.messages.clear()

    def display_raw(self):
        """
        Display the raw text of the daemon log as has been read so far.
        """
        print("\n".join(self.lines))

    def wait(
        self, condition: Callable[["DaemonLogMessage"], bool], timeout=120
    ) -> bool:
        """
        Wait for some condition to be true on a :class:`DaemonLogMessage`
        parsed from the daemon log.

        Parameters
        ----------
        condition
            A callback which will be executed on each message. If the callback
            returns ``True``, this method will return as well.
        timeout
            How long to time out after. If the method times out, it will return
            ``False`` instead of ``True``.

        Returns
        -------
        success
            ``True`` if the condition was satisfied, ``False`` otherwise.
        """
        start = time.time()
        while True:
            if time.time() - start > timeout:
                return False

            for msg in self.read():
                if condition(msg):
                    return True

            time.sleep(0.1)


RE_MESSAGE = re.compile(
    r"^(?P<timestamp>\d{2}\/\d{2}\/\d{2}\s\d{2}:\d{2}:\d{2}(?P<milliseconds>\.(\d{3}|1000))?)\s(?P<tags>(?:\([^()]+\)\s+)*)(?P<msg>.*)$"
)
RE_TAGS = re.compile(r"\(([^()]+)\)")

LOG_MESSAGE_TIME_FORMAT = r"%m/%d/%y %H:%M:%S"
MILLISECONDS_LOG_MESSAGE_TIME_FORMAT = r"%m/%d/%y %H:%M:%S.%f"


class DaemonLogMessage:
    """
    A class that represents a single message in a :class:`DaemonLog`.
    """

    def __init__(self, line: str, file_name: str, line_number: int):
        """
        Parameters
        ----------
        line
            The actual line that appeared in the daemon log file.
        file_name
            The file that the line came from.
        line_number
            The line number of the message in the log file.
        """
        self.line = line
        match = RE_MESSAGE.match(line)
        if match is None:
            raise exceptions.DaemonLogParsingFailed(
                "Failed to parse daemon log line {}:{} -> {}".format(
                    file_name, line_number, repr(line)
                )
            )

        if match.group("milliseconds"):
            self._timestamp = datetime.datetime.strptime(
                match.group("timestamp"), MILLISECONDS_LOG_MESSAGE_TIME_FORMAT
            )
            if match.group("milliseconds") == ".1000":
                self._timestamp = self._timestamp.replace(microsecond=999999)
        else:
            self._timestamp = datetime.datetime.strptime(
                match.group("timestamp"), LOG_MESSAGE_TIME_FORMAT
            )

        self._tags = RE_TAGS.findall(match.group("tags"))
        self.message = match.group("msg")

    @property
    def timestamp(self) -> datetime.datetime:
        """The time that the message occurred."""
        return self._timestamp

    @property
    def tags(self):
        """The tags (like ``D_ALL``) of the message."""
        return self._tags

    def __iter__(self):
        return self.timestamp, self.tags, self.message

    def __str__(self):
        return self.line

    def __repr__(self):
        return 'LogMessage(timestamp = {}, tags = {}, message = "{}")'.format(
            self.timestamp, self.tags, self.message
        )

    def __contains__(self, item):
        return item in self.message
