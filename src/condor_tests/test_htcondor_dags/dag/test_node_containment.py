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

from htcondor2 import dags


def test_dag_contains_child(dag):
    a_child = dag.layer(name="a_child")

    assert a_child in dag


def test_other_does_not_contain_child():
    a = dags.DAG()
    b = dags.DAG()

    a_child = a.layer(name="a_child")

    assert a_child not in b


def test_other_does_not_contain_child_even_if_same_name():
    a = dags.DAG()
    b = dags.DAG()

    a_child = a.layer(name="child")
    b_child = b.layer(name="child")

    assert a_child not in b
    assert b_child not in a
