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


def test_walk_depth_first(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = a.child_layer(name="c")
    d = dag.layer(name="d")
    d.add_parents(b, c)

    nodes = list(dag.walk(dags.WalkOrder.DEPTH_FIRST))
    # sibling order is not specified
    assert nodes in ([a, b, d, c], [a, c, d, b])


def test_walk_breadth_first(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = a.child_layer(name="c")
    d = dag.layer(name="d")
    d.add_parents(b, c)

    nodes = list(dag.walk(dags.WalkOrder.BREADTH_FIRST))
    # sibling order is not specified
    assert nodes in ([a, b, c, d], [a, c, b, d])


def test_walk_bad_order_raises(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = a.child_layer(name="c")
    d = dag.layer(name="d")
    d.add_parents(b, c)

    with pytest.raises(dags.exceptions.UnrecognizedWalkOrder):
        list(dag.walk(order="foobar"))


def test_ancestors_has_all_ancestors_linear(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = b.child_layer(name="c")

    assert list(c.walk_ancestors()) == [b, a]


def test_ancestors_has_all_ancestors_branching_depth_first(dag):
    a = dag.layer(name="a")
    b1 = a.child_layer(name="b1")
    b2 = dag.layer(name="b2")
    c = dags.Nodes(b1, b2).child_layer(name="c")

    assert list(c.walk_ancestors(dags.WalkOrder.DEPTH_FIRST)) in (
        [b1, a, b2],
        [b2, b1, a],
    )


def test_ancestors_has_all_ancestors_branching_breadth_first(dag):
    a = dag.layer(name="a")
    b1 = a.child_layer(name="b1")
    b2 = dag.layer(name="b2")
    c = dags.Nodes(b1, b2).child_layer(name="c")

    assert list(c.walk_ancestors(dags.WalkOrder.BREADTH_FIRST)) in (
        [b1, b2, a],
        [b2, b1, a],
    )


def test_ancestors_doesnt_include_disconnected_piece(dag):
    a = dag.layer(name="a")
    b1 = a.child_layer(name="b1")
    b2 = dag.layer(name="b2")
    c = dags.Nodes(b1, b2).child_layer(name="c")

    d = dag.layer(name="d")

    assert d not in set(c.walk_ancestors())


def test_ancestors_of_nodes_joins_ancestors(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = dag.layer(name="c")
    d = c.child_layer(name="d")

    assert set(dags.Nodes(b, d).walk_ancestors()) == {a, c}


def test_descendants_has_all_descendants_linear(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = b.child_layer(name="c")

    assert list(a.walk_descendants()) == [b, c]


def test_descendants_has_all_descendants_branching_depth_first(dag):
    a = dag.layer(name="a")
    b1 = a.child_layer(name="b1")
    c = b1.child_layer(name="c")
    b2 = a.child_layer(name="b2")

    assert list(a.walk_descendants(dags.WalkOrder.DEPTH_FIRST)) in (
        [b1, c, b2],
        [b2, b1, c],
    )


def test_descendants_has_all_descendants_branching_breadth_first(dag):
    a = dag.layer(name="a")
    b1 = a.child_layer(name="b1")
    c = b1.child_layer(name="c")
    b2 = a.child_layer(name="b2")

    assert list(a.walk_descendants(dags.WalkOrder.BREADTH_FIRST)) in (
        [b1, b2, c],
        [b2, b1, c],
    )


def test_descendants_of_nodes_joins_descendants(dag):
    a = dag.layer(name="a")
    b = a.child_layer(name="b")
    c = dag.layer(name="c")
    d = c.child_layer(name="d")

    assert set(dags.Nodes(a, c).walk_descendants()) == {b, d}
