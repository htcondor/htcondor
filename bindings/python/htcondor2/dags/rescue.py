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

from typing import Optional, Mapping, Set

import collections
from pathlib import Path

from . import dag
from .formatter import NodeNameFormatter, SimpleFormatter
from .writer import DEFAULT_DAG_FILE_NAME
from . import exceptions


def rescue(
    dag: dag.DAG, rescue_file: Path, formatter: Optional[NodeNameFormatter] = None
) -> None:
    """
    Applies state recorded in a DAGMan rescue file to the ``dag``.
    The ``dag`` will be modified in-place.

    .. warning::
        Running this function on a :class:`DAG` **replaces** any existing
        ``DONE`` information on **all** of its nodes.
        Every node will have a dictionary for its ``done`` attribute.
        If you want to edit this information manually, always run this function
        **first**, then make the desired changes on top.

    .. warning::
        This function cannot detect changes in node names. If node names are
        different in the rescue file compared to the :class:`DAG`, this function
        will not behave as expected.

    Parameters
    ----------
    dag
        The DAG to apply the rescue state to.
    rescue_file
        The file to get rescue state from.
        Use the :func:`find_rescue_file` helper function to find the right rescue
        file.
    formatter
        The node name formatter that was used to write out the original DAG.
    """
    if formatter is None:
        formatter = SimpleFormatter()
    _rescue(dag, Path(rescue_file).read_text(), formatter)


def _rescue(dag: dag.DAG, rescue_file_text: str, formatter: NodeNameFormatter) -> None:
    finished_nodes = parse_rescue_file_text(rescue_file_text, formatter)

    apply_rescue(dag, finished_nodes)


def parse_rescue_file_text(
    rescue_file_text: str, formatter: NodeNameFormatter
) -> Mapping[str, Set[int]]:
    finished_nodes = collections.defaultdict(set)
    for line in rescue_file_text.splitlines():
        if line.startswith("#"):
            continue
        if line == "":
            continue

        node_name = line.lstrip("DONE ")
        layer, index = formatter.parse(node_name)
        finished_nodes[layer].add(index)

    return finished_nodes


def apply_rescue(dag: dag.DAG, finished_nodes: Mapping[str, Set[int]]) -> None:
    for node in dag.nodes:
        node.done = {}
        for index in finished_nodes[node.name]:
            node.done[index] = True


def find_rescue_file(
    dag_dir: Path, dag_file_name: str = DEFAULT_DAG_FILE_NAME
) -> Optional[Path]:
    """
    Finds the latest rescue file in a DAG directory
    (just like DAGMan itself would).

    Parameters
    ----------
    dag_dir
        The directory to search in.
    dag_file_name
        The base name of the DAG description file;
        the same name you would pass to :func:`write_dag`.

    Returns
    -------
    rescue_file : :class:`pathlib.Path`
        The path to the latest rescue file found in the ``dag_dir``.
    """
    dag_dir = Path(dag_dir)
    rescue_files = sorted(dag_dir.glob("{}.rescue*".format(dag_file_name)))

    if len(rescue_files) == 0:
        raise exceptions.NoRescueFileFound(
            "No rescue file for dag {} found in {}".format(dag_file_name, dag_dir)
        )

    return rescue_files[-1]
