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

import textwrap

import htcondor

from htcondor import dags
from htcondor.dags.rescue import _rescue


@pytest.fixture(scope="session")
def rescue_dag():
    sub = htcondor.Submit(
        dict(
            executable="/bin/echo",
            arguments="hi",
            request_memory="16MB",
            request_disk="1MB",
        )
    )

    dag = dags.DAG()

    a = dag.layer(name="a", submit_description=sub)
    b = a.child_layer(name="b", submit_description=sub)
    c = b.child_layer(
        name="c",
        submit_description=sub,
        abort=dags.DAGAbortCondition(node_exit_value=0, dag_return_value=1),
    )
    d = c.child_layer(name="d", submit_description=sub)

    return dag


@pytest.fixture(scope="session")
def rescue_file_text():
    return textwrap.dedent(
        """
    # Rescue DAG file, created after running
    #  the dagfile.dag DAG file
    # Created 11/8/2019 04:08:46 UTC
    # Rescue DAG version: 2.0.1 (partial)

    # Total number of Nodes: 4
    # Nodes premarked DONE: 2
    # Nodes that failed: 0
    #   <ENDLIST>

    DONE a:0
    DONE b:0
    """
    )


def test_rescue(rescue_dag, rescue_file_text):
    _rescue(rescue_dag, rescue_file_text, formatter=dags.SimpleFormatter())

    assert rescue_dag._nodes["a"].done == {0: True}
    assert rescue_dag._nodes["b"].done == {0: True}
    assert rescue_dag._nodes["c"].done == {}
    assert rescue_dag._nodes["d"].done == {}


@pytest.mark.parametrize("num_rescues", [1, 5, 15, 150])
def test_find_rescue_file_with_existing_rescue_file(dag_dir, num_rescues):
    base = "dagfile.dag"
    for n in range(num_rescues):
        (dag_dir / "{}.rescue{:03d}".format(base, n + 1)).touch()

    assert dags.find_rescue_file(dag_dir, base) == (
        dag_dir / "{}.rescue{:03d}".format(base, num_rescues)
    )


def test_find_rescue_file_raises_if_no_rescue_found(dag_dir):
    with pytest.raises(htcondor.dags.exceptions.NoRescueFileFound):
        dags.find_rescue_file(dag_dir, "dagfile.dag")
