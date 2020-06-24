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

from pathlib import Path

from htcondor import dags
from htcondor.dags.writer import DAGWriter

from .conftest import s, dagfile_lines


def test_empty_dag_writes_empty_dagfile(dag, writer):
    lines = dagfile_lines(writer)

    # if there are any lines in the file, they must be comments
    assert all(line.startswith("#") for line in lines)


def test_jobstate_log():
    logfile = Path("i_am_the_jobstate.log").absolute()

    dag = dags.DAG(jobstate_log=logfile)
    writer = DAGWriter(dag)

    lines = dagfile_lines(writer)
    assert "JOBSTATE_LOG {}".format(logfile.as_posix()) in lines


def test_config_command_in_dagfile_if_config_given():
    dag = dags.DAG(dagman_config={"DAGMAN_MAX_JOBS_IDLE": 10})
    writer = DAGWriter(dag)

    lines = dagfile_lines(writer)
    assert "CONFIG {}".format(dags.CONFIG_FILE_NAME) in lines


def test_config_file_gets_written_if_config_given(dag_dir):
    dag = dags.DAG(dagman_config={"DAGMAN_MAX_JOBS_IDLE": 10})
    dags.write_dag(dag, dag_dir)

    assert (dag_dir / dags.CONFIG_FILE_NAME).exists()


def test_config_file_has_right_contents(dag_dir):
    dag = dags.DAG(dagman_config={"DAGMAN_MAX_JOBS_IDLE": 10})
    dags.write_dag(dag, dag_dir)

    assert (
        "DAGMAN_MAX_JOBS_IDLE = 10"
        in (dag_dir / dags.CONFIG_FILE_NAME).read_text().splitlines()
    )


def test_dagman_job_attributes_with_one_attr():
    dag = dags.DAG(dagman_job_attributes={"foo": "bar"})

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert "SET_JOB_ATTR foo = bar" in lines


def test_dagman_job_attributes_with_two_attrs():
    dag = dags.DAG(dagman_job_attributes={"foo": "bar", "wizard": 17})

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert all(("SET_JOB_ATTR foo = bar" in lines, "SET_JOB_ATTR wizard = 17" in lines))


def test_max_jobs_per_category_with_one_category():
    dag = dags.DAG(max_jobs_by_category={"foo": 5})

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert "CATEGORY foo 5" in lines


def test_max_jobs_per_category_with_two_categories():
    dag = dags.DAG(max_jobs_by_category={"foo": 5, "bar": 10})

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert all(("CATEGORY foo 5" in lines, "CATEGORY bar 10" in lines))


def test_dot_config_default():
    dag = dags.DAG(dot_config=dags.DotConfig(Path("dag.dot")))

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert "DOT dag.dot DONT-UPDATE OVERWRITE" in lines


def test_dot_config_not_default():
    dag = dags.DAG(
        dot_config=dags.DotConfig(
            Path("dag.dot"),
            update=True,
            overwrite=False,
            include_file=Path("include-me.dot"),
        )
    )

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert "DOT dag.dot UPDATE DONT-OVERWRITE INCLUDE include-me.dot" in lines


def test_node_status_file_default():
    dag = dags.DAG(node_status_file=dags.NodeStatusFile(Path("node_status_file")))

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert "NODE_STATUS_FILE node_status_file" in lines


def test_node_status_file_not_default():
    dag = dags.DAG(
        node_status_file=dags.NodeStatusFile(
            Path("node_status_file"), update_time=60, always_update=True
        )
    )

    writer = DAGWriter(dag)
    lines = dagfile_lines(writer)
    assert "NODE_STATUS_FILE node_status_file 60 ALWAYS-UPDATE" in lines
