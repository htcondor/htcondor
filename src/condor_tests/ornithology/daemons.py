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

import re
import datetime
import time

from . import exceptions

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class DaemonLog:
    def __init__(self, path):
        self.path = path

    def open(self):
        return DaemonLogStream(self.path.open(mode="r", encoding="utf-8"))


class DaemonLogStream:
    def __init__(self, file):
        self.file = file
        self.messages = []
        self.line_number = 0

    @property
    def lines(self):
        yield from (msg.line for msg in self.messages)

    def read(self):
        for line in self.file:
            line = line.strip()
            if line == "":
                continue
            try:
                msg = LogMessage(line.strip(), self.file.name, self.line_number)
            except exceptions.DaemonLogParsingFailed as e:
                logger.warning(e)
            self.line_number += 1
            self.messages.append(msg)
            yield msg

    def clear(self):
        """Clear the internal message store; useful for isolating tests."""
        self.messages.clear()

    def display_raw(self):
        print("\n".join(self.lines))

    def wait(self, condition, timeout=60):
        start = time.time()
        while True:
            if time.time() - start > timeout:
                return False

            for msg in self.read():
                if condition(msg):
                    return True

            time.sleep(0.1)


RE_MESSAGE = re.compile(
    r"^(?P<timestamp>\d{2}\/\d{2}\/\d{2}\s\d{2}:\d{2}:\d{2})\s(?P<tags>(?:\([^()]+\)\s+)*)(?P<msg>.*)$"
)
RE_TAGS = re.compile(r"\(([^()]+)\)")

LOG_MESSAGE_TIME_FORMAT = r"%m/%d/%y %H:%M:%S"


class LogMessage:
    def __init__(self, line, file_name, line_number):
        self.line = line
        match = RE_MESSAGE.match(line)
        if match is None:
            raise exceptions.DaemonLogParsingFailed(
                "Failed to parse daemon log line {}:{} -> {}".format(
                    file_name, line_number, repr(line)
                )
            )

        self.timestamp = datetime.datetime.strptime(
            match.group("timestamp"), LOG_MESSAGE_TIME_FORMAT
        )

        self.tags = RE_TAGS.findall(match.group("tags"))
        self.message = match.group("msg")

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
