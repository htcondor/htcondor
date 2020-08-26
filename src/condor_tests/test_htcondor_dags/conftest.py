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

from pathlib import Path

from htcondor import dags


@pytest.fixture(scope="function")
def dag(request):
    dag = dags.DAG()

    yield dag

    if request.session.testsfailed:
        print(dag.describe())


@pytest.fixture(scope="function")
def dag_dir(tmp_path):
    d = Path(str(tmp_path / "dag-dir"))
    d.mkdir()

    return d
