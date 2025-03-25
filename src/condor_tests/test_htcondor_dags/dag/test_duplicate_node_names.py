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


def test_two_node_layers_with_same_name_raises(dag):
    dag.layer(name="alice")

    with pytest.raises(dags.exceptions.DuplicateNodeName):
        dag.layer(name="alice")


def test_layer_then_final_raises(dag):
    dag.layer(name="alice")

    with pytest.raises(dags.exceptions.DuplicateNodeName):
        dag.final(name="alice")


def test_final_then_layer_raises(dag):
    dag.final(name="alice")

    with pytest.raises(dags.exceptions.DuplicateNodeName):
        dag.layer(name="alice")
