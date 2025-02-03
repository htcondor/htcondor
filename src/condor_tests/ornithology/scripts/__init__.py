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

from pathlib import Path

SCRIPTS = {
    path.stem: path.as_posix()
    for path in Path(__file__).parent.iterdir()
    if path.stem != "__init__"
}


# Return a space separated list of all custom
# fto plugins to be readily available to all tests
def custom_fto_plugins() -> str:
    return " ".join([
        SCRIPTS["null_plugin"],  # null://
        SCRIPTS["debug_plugin"],  # debug://, encode://
    ])
