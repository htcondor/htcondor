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

from .conftest import s, dagfile_lines


def test_grouper_edge_produces_correct_dagfile_lines(dag, writer):
    parent = dag.layer(name="parent", vars=[{}] * 6)
    child = parent.child_layer(name="child", vars=[{}] * 4, edge=dags.Grouper(3, 2))

    lines = dagfile_lines(writer)

    assert (
        "PARENT parent{s}0 parent{s}1 parent{s}2 CHILD __JOIN__{s}0".format(s=s)
        in lines
    )
    assert (
        "PARENT parent{s}3 parent{s}4 parent{s}5 CHILD __JOIN__{s}1".format(s=s)
        in lines
    )
    assert "PARENT __JOIN__{s}0 CHILD child{s}0 child{s}1".format(s=s) in lines
    assert "PARENT __JOIN__{s}1 CHILD child{s}2 child{s}3".format(s=s) in lines


@pytest.mark.parametrize(
    "num_parent_vars, num_child_vars, parent_group_size, child_group_size",
    [
        (6, 4, 3, 2),
        (9, 3, 3, 1),
        (9, 6, 3, 2),
        (1, 1, 1, 1),
        (2, 1, 2, 1),
        (1, 2, 1, 2),
        (2, 2, 1, 1),
    ],
)
def test_compatible_grouper_edges(
    num_parent_vars, num_child_vars, parent_group_size, child_group_size, dag, writer
):
    parent = dag.layer(name="parent", vars=[{}] * num_parent_vars)
    child = parent.child_layer(
        name="child",
        vars=[{}] * num_child_vars,
        edge=dags.Grouper(parent_group_size, child_group_size),
    )

    dagfile_lines(writer)


@pytest.mark.parametrize(
    "num_parent_vars, num_child_vars, parent_group_size, child_group_size",
    [(2, 1, 1, 1), (1, 2, 1, 1), (9, 6, 3, 3), (5, 1, 3, 1), (1, 5, 1, 3)],
)
def test_incompatible_grouper_edges(
    num_parent_vars, num_child_vars, parent_group_size, child_group_size, dag, writer
):
    parent = dag.layer(name="parent", vars=[{}] * num_parent_vars)
    child = parent.child_layer(
        name="child",
        vars=[{}] * num_child_vars,
        edge=dags.Grouper(parent_group_size, child_group_size),
    )

    with pytest.raises(dags.exceptions.IncompatibleGrouper):
        dagfile_lines(writer)
