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


def test_slicer_edge_produces_correct_dagfile_lines(dag, writer):
    # note that the slice stops when the child layer runs out of vars!
    parent = dag.layer(name="parent", vars=[{}] * 6)
    child = parent.child_layer(
        name="child",
        vars=[{}] * 4,
        edge=dags.Slicer(
            parent_slice=slice(None, None, 2), child_slice=slice(1, None, 2)
        ),
    )

    lines = dagfile_lines(writer)

    assert "PARENT parent{s}0 CHILD child{s}1".format(s=s) in lines
    assert "PARENT parent{s}2 CHILD child{s}3".format(s=s) in lines
