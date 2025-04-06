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

from .conftest import s, dagfile_lines


def test_one_parent_one_child(dag, writer):
    parent = dag.layer(name="parent")
    child = parent.child_subdag(name="child", dag_file="foobar.dag")

    lines = dagfile_lines(writer)
    assert "PARENT parent{s}0 CHILD child{s}0".format(s=s) in lines


def test_two_parents_one_child(dag, writer):
    parent = dag.layer(name="parent", vars=[{}, {}])
    child = parent.child_subdag(name="child", dag_file="foobar.dag")

    lines = dagfile_lines(writer)
    assert "PARENT parent{s}0 parent{s}1 CHILD child{s}0".format(s=s) in lines


def test_one_parent_two_children(dag, writer):
    parent = dag.subdag(name="parent", dag_file="foobar.dag")
    child = parent.child_layer(name="child", vars=[{}, {}])

    lines = dagfile_lines(writer)
    assert "PARENT parent{s}0 CHILD child{s}0 child{s}1".format(s=s) in lines
