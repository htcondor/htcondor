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


def test_can_mark_part_of_layer_noop(dag, writer):
    layer = dag.layer(name="layer", vars=[{}] * 2, noop={0: True})

    lines = dagfile_lines(writer)
    assert "JOB layer{s}0 layer.sub NOOP".format(s=s) in lines
    assert "JOB layer{s}1 layer.sub".format(s=s) in lines


def test_can_mark_part_of_layer_done(dag, writer):
    layer = dag.layer(name="layer", vars=[{}] * 2, done={0: True})

    lines = dagfile_lines(writer)
    assert "JOB layer{s}0 layer.sub DONE".format(s=s) in lines
    assert "JOB layer{s}1 layer.sub".format(s=s) in lines
