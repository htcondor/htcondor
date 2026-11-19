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

import htcondor2 as htcondor


def queue_lines(text):
    return [
        line
        for line in text.splitlines()
        if line.strip().lower().startswith("queue")
    ]


def test_submit_file_has_exactly_one_queue_statement(dag, writer, dag_dir):
    dag.layer(
        name="foobar",
        submit_description=htcondor.Submit(
            {"executable": "/bin/sleep", "arguments": "1"}
        ),
    )

    writer.write(dag_dir)

    text = (dag_dir / "foobar.sub").read_text()
    assert queue_lines(text) == ["queue"]


def test_submit_file_preserves_queue_statement(dag, writer, dag_dir):
    dag.layer(
        name="foobar",
        submit_description=htcondor.Submit("executable = /bin/sleep\nqueue 5"),
    )

    writer.write(dag_dir)

    text = (dag_dir / "foobar.sub").read_text()
    assert queue_lines(text) == ["queue 5"]
