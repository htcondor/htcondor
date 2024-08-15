#!/usr/bin/python3

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

import os
import sys
import enum
import re

try:
    from colorama import init
    init()
    HAS_COLORAMA = True
except ImportError:
    HAS_COLORAMA = False

IS_WINDOWS = os.name == "nt"
IS_TTY = sys.stdout.isatty()

ANSI_RESET = "\033[0m"
ANSI_ESCAPE_RE = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")

class Color(str, enum.Enum):
    BLACK = "\033[30m"
    RED = "\033[31m"
    BRIGHT_RED = "\033[31;1m"
    GREEN = "\033[32m"
    BRIGHT_GREEN = "\033[32;1m"
    YELLOW = "\033[33m"
    BRIGHT_YELLOW = "\033[33;1m"
    BLUE = "\033[34m"
    BRIGHT_BLUE = "\033[34;1m"
    MAGENTA = "\033[35m"
    BRIGHT_MAGENTA = "\033[35;1m"
    CYAN = "\033[36m"
    BRIGHT_CYAN = "\033[36;1m"
    WHITE = "\033[37m"
    BRIGHT_WHITE = "\033[37;1m"

class Style(str, enum.Enum):
    BOLD = "\033[1m"
    ITALIC = "\033[3m"
    UNDERLINE = "\033[4m"
    BLINK = "\033[5m"

def is_capabale():
    return IS_TTY and (not IS_WINDOWS or HAS_COLORAMA)

def colorize(string: str, color: Color) -> str:
    return color + string + ANSI_RESET if is_capabale() else string

def underline(string: str) -> str:
    return Style.UNDERLINE + string + ANSI_RESET if is_capabale() else string

def italicize(string: str) -> str:
    return Style.ITALIC + string + ANSI_RESET if is_capabale() else string

def bold(string: str) -> str:
    return Style.BOLD + string + ANSI_RESET if is_capabale() else string

def blink(string: str) -> str:
    return Style.BLINK + string + ANSI_RESET if is_capabale() else string

def strip_ansi(string: str) -> str:
    return ANSI_ESCAPE_RE.sub("", string)
