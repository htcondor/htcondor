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

from htcondor import dags

from .conftest import s, dagfile_lines, dagfile_text


def test_one_parent_one_child(dag, writer):
    parent = dag.layer(name="parent")
    child = parent.child_layer(name="child")

    assert "PARENT parent{s}0 CHILD child{s}0".format(s=s) in dagfile_lines(writer)


def test_two_parents_one_child(dag, writer):
    parent = dag.layer(name="parent", vars=[{}, {}])
    child = parent.child_layer(name="child")

    assert "PARENT parent{s}0 parent{s}1 CHILD child{s}0".format(s=s) in dagfile_lines(
        writer
    )


def test_one_parent_two_children(dag, writer):
    parent = dag.layer(name="parent")
    child = parent.child_layer(name="child", vars=[{}, {}])

    assert "PARENT parent{s}0 CHILD child{s}0 child{s}1".format(s=s) in dagfile_lines(
        writer
    )


def test_two_parents_two_children_creates_join_node(dag, writer):
    parent = dag.layer(name="parent", vars=[{}, {}])
    child = parent.child_layer(name="child", vars=[{}, {}])

    lines = dagfile_lines(writer)
    assert "PARENT parent{s}0 parent{s}1 CHILD __JOIN__{s}0".format(s=s) in lines
    assert "PARENT __JOIN__{s}0 CHILD child{s}0 child{s}1".format(s=s) in lines

    assert "JOB __JOIN__" in dagfile_text(writer)


def test_two_parents_two_children_one_to_one(dag, writer):
    parent = dag.layer(name="parent", vars=[{}, {}])
    child = parent.child_layer(name="child", vars=[{}, {}], edge=dags.OneToOne())

    lines = dagfile_lines(writer)
    assert "PARENT parent{s}0 CHILD child{s}0".format(s=s) in lines
    assert "PARENT parent{s}1 CHILD child{s}1".format(s=s) in lines


def test_two_parents_three_children_one_to_one_raises(dag, writer):
    parent = dag.layer(name="parent", vars=[{}, {}])
    child = parent.child_layer(name="child", vars=[{}, {}, {}], edge=dags.OneToOne())

    with pytest.raises(dags.exceptions.OneToOneEdgeNeedsSameNumberOfVars):
        dagfile_lines(writer)
