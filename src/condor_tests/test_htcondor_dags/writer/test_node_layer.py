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

from pathlib import Path

from htcondor import dags

from .conftest import s, dagfile_lines, dagfile_text


def test_layer_name_appears(dag, writer):
    dag.layer(name="foobar")

    assert "foobar{s}0".format(s=s) in dagfile_text(writer)


def test_job_line_for_no_vars(dag, writer):
    dag.layer(name="foobar")

    assert "JOB foobar{s}0 foobar.sub".format(s=s) in dagfile_lines(writer)


def test_job_line_for_one_vars(dag, writer):
    dag.layer(name="foobar", vars=[{"bing": "bang"}])

    assert "JOB foobar{s}0 foobar.sub".format(s=s) in dagfile_lines(writer)


def test_job_lines_for_two_vars(dag, writer):
    dag.layer(name="foobar", vars=[{"bing": "bang"}, {"bing": "bong"}])

    lines = dagfile_lines(writer)
    assert "JOB foobar{s}0 foobar.sub".format(s=s) in lines
    assert 'VARS foobar{s}0 bing="bang"'.format(s=s) in lines
    assert "JOB foobar{s}1 foobar.sub".format(s=s) in lines
    assert 'VARS foobar{s}1 bing="bong"'.format(s=s) in lines


def test_node_inline_meta(dag, writer):
    dag.layer(name="foobar", dir="dir", noop=True, done=True)

    assert "JOB foobar{s}0 foobar.sub DIR dir NOOP DONE".format(s=s) in dagfile_lines(
        writer
    )


def test_layer_retry(dag, writer):
    dag.layer(name="foobar", retries=5)

    assert "RETRY foobar{s}0 5".format(s=s) in dagfile_lines(writer)


def test_layer_retry_with_unless_exit(dag, writer):
    dag.layer(name="foobar", retries=5, retry_unless_exit=2)

    assert "RETRY foobar{s}0 5 UNLESS-EXIT 2".format(s=s) in dagfile_lines(writer)


def test_layer_category(dag, writer):
    dag.layer(name="foobar", category="cat")

    assert "CATEGORY foobar{s}0 cat".format(s=s) in dagfile_lines(writer)


def test_layer_priority(dag, writer):
    dag.layer(name="foobar", priority=3)

    assert "PRIORITY foobar{s}0 3".format(s=s) in dagfile_lines(writer)


def test_layer_pre_skip(dag, writer):
    dag.layer(name="foobar", pre_skip_exit_code=1)

    assert "PRE_SKIP foobar{s}0 1".format(s=s) in dagfile_lines(writer)


def test_layer_script_meta(dag, writer):
    dag.layer(
        name="foobar",
        pre=dags.Script(
            executable="/bin/sleep",
            arguments=["5m"],
            retry=True,
            retry_status=2,
            retry_delay=3,
        ),
    )

    assert "SCRIPT DEFER 2 3 PRE foobar{s}0 /bin/sleep 5m".format(s=s) in dagfile_lines(
        writer
    )


def test_layer_abort(dag, writer):
    dag.layer(name="foobar", abort=dags.DAGAbortCondition(node_exit_value=3))

    assert "ABORT-DAG-ON foobar{s}0 3".format(s=s) in dagfile_lines(writer)


def test_layer_abort_with_meta(dag, writer):
    dag.layer(
        name="foobar",
        abort=dags.DAGAbortCondition(node_exit_value=3, dag_return_value=10),
    )

    assert "ABORT-DAG-ON foobar{s}0 3 RETURN 10".format(s=s) in dagfile_lines(writer)


def test_submit_description_from_file(dag, writer):
    p = Path("here.sub")
    dag.layer(name="foobar", submit_description=p)

    assert "JOB foobar{s}0 {p}".format(s=s, p=p.absolute().as_posix()) in dagfile_text(
        writer
    )
