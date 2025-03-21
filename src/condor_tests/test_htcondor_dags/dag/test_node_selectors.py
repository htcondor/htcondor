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


def test_select_nodes(dag):
    a = dag.layer(name="a")
    bb = dag.layer(name="bb")
    ccc = dag.layer(name="ccc")
    dd = dag.layer(name="dd")

    assert dag.select(lambda n: len(n.name) == 2) == dags.Nodes(bb, dd)


def test_glob_nodes(dag):
    aa = dag.layer(name="aa")
    bb = dag.layer(name="bb")
    ac = dag.layer(name="ac")
    dd = dag.layer(name="dd")

    assert dag.glob("a*") == dags.Nodes(aa, ac)


def test_roots(dag):
    root = dag.layer(name="root")
    middle = root.child_layer(name="middle")
    leaf = middle.child_layer(name="leaf")

    assert dag.roots == dags.Nodes(root)


def test_leaves(dag):
    root = dag.layer(name="root")
    middle = root.child_layer(name="middle")
    leaf = middle.child_layer(name="leaf")

    assert dag.leaves == dags.Nodes(leaf)
