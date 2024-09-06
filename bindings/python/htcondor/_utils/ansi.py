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
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    DARK_GREY = "\033[90m"
    BRIGHT_RED = "\033[91m"
    BRIGHT_GREEN = "\033[92m"
    BRIGHT_YELLOW = "\033[93m"
    BRIGHT_BLUE = "\033[94m"
    BRIGHT_MAGENTA = "\033[95m"
    BRIGHT_CYAN = "\033[96m"
    BRIGHT_WHITE = "\033[97m"

class Style(str, enum.Enum):
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"
    BLINK = "\033[5m"
    # ITALIC = "\033[3m" Note: Not widely supported

class AnsiOptions():
    def __init__(self, **kwargs):
        self.bold = kwargs.get("bold", False)
        self.blink = kwargs.get("blink", False)
        self.underline = kwargs.get("underline", False)
        self.style = kwargs.get("style")
        self.color = kwargs.get("color")
        self.color_id = kwargs.get("color_id")

        if self.style is not None:
            self.bold = True if self.style == Style.BOLD else self.bold
            self.blink = True if self.style == Style.BLINK else self.blink
            self.underline = True if self.style == Style.UNDERLINE else self.underline

        if self.color is not None:
            self.color = self.color[2:-1]

        if self.color_id is not None:
            self.color_id = sorted([0, self.color_id, 255])[1]


    def __str__(self):
        style = "\033["
        style += "1;" if self.bold else ""
        style += "4;" if self.underline else ""
        style += "5;" if self.blink else ""
        style += f"38;5;{self.color_id};" if self.color_id is not None else (f"{self.color};" if self.color is not None else "")
        return f"{style[:-1]}m" if ";" in style else ""

def is_capable():
    return IS_TTY and (not IS_WINDOWS or HAS_COLORAMA)

def colorize(string: str, color: Color) -> str:
    return color + string + ANSI_RESET if is_capable() else string

# See https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797#256-colors for values
def id_colorize(string: str, id: int) -> str:
    id = sorted([0, id, 255])[1]
    return f"\033[38;5;{id}m" + string + ANSI_RESET if is_capable() else string

def underline(string: str) -> str:
    return Style.UNDERLINE + string + ANSI_RESET if is_capable() else string

#def italicize(string: str) -> str:
#    return Style.ITALIC + string + ANSI_RESET if is_capable() else string

def bold(string: str) -> str:
    return Style.BOLD + string + ANSI_RESET if is_capable() else string

def blink(string: str) -> str:
    return Style.BLINK + string + ANSI_RESET if is_capable() else string

def strip_ansi(string: str) -> str:
    return ANSI_ESCAPE_RE.sub("", string)

def stylize(string: str, options: AnsiOptions) -> str:
    return str(options) + string + ANSI_RESET if is_capable() else string
