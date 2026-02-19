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

import sys
import os
from pathlib import Path

SCRIPT_EXT = ".cmd" if sys.platform == "win32" else ".py"

SCRIPTS = {
    path.stem: path.with_suffix(SCRIPT_EXT).as_posix()
    for path in Path(__file__).parent.iterdir()
    if path.stem != "__init__"
}

WIN32_PY_SCRIPT_PROLOG = [
    "0<0# : ^", "'''", "@echo off",
    ("py.exe" if not sys.executable else '"{}"'.format(sys.executable)) + ' "%~f0" %*',
    "@goto :EOF", "'''"
]

def prepare_script(path):
    if os.path.isfile(path) : return

    source = Path(path).with_suffix(".py")
    if os.path.isfile(source):
        try:
            with open(source, 'rb') as f:
                body = f.read()
            if body:
                with open(path,'wb') as f:
                    f.write("\n".join(WIN32_PY_SCRIPT_PROLOG).encode('utf8') + body)
                os.remove(source)
        except IOError:
            return # do nothing
    return


# Return a space separated list of all custom
# fto plugins to be readily available to all tests
def custom_fto_plugins() -> str:
    if SCRIPT_EXT == ".cmd":
       for name,path in SCRIPTS.items():
          prepare_script(path)

    return " ".join([
        SCRIPTS["null_plugin"],  # null://
        SCRIPTS["debug_plugin"],  # debug://, encode://
    ])
