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

import pytest

from htcondor2 import dags
from htcondor2.dags.writer import DAGWriter

# just shorthand
s = dags.DEFAULT_SEPARATOR


@pytest.fixture(scope="function")
def writer(dag):
    return DAGWriter(dag)


def dagfile_lines(writer):
    lines = list(writer.yield_dag_file_lines())
    print("\n" + " DAGFILE LINES ".center(40, "-"))
    for line in lines:
        print(line)
    print("-" * 40)
    return lines


def dagfile_text(writer):
    return "\n".join(dagfile_lines(writer))
