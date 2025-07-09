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

import logging
from typing import Optional, List, Dict, Iterator

from pathlib import Path

import htcondor

from . import dag, node, edges, formatter
from .walk_order import WalkOrder

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

DEFAULT_DAG_FILE_NAME = "dagfile.dag"
CONFIG_FILE_NAME = "dagman.config"
NOOP_SUBMIT_FILE_NAME = "__JOIN__.sub"


def write_dag(
    dag: dag.DAG,
    dag_dir: Path,
    dag_file_name: Optional[str] = DEFAULT_DAG_FILE_NAME,
    node_name_formatter: Optional[formatter.NodeNameFormatter] = None,
) -> Path:
    """
    Write out the given DAG to the given directory.
    This includes the DAG description file itself, as well as any associated
    submit descriptions.

    Parameters
    ----------
    dag
        The DAG to write the description for.
    dag_dir
        The directory to write the DAG files to.
    dag_file_name
        The name of the DAG description file itself.
    node_name_formatter
        The :class:`NodeNameFormatter` to use for generating underlying node names.
        If not provided, the default is :class:`SimpleFormatter`.

    Returns
    -------
    dag_file_path : :class:`pathlib.Path`
        The path to the DAG description file;
        can be passed to :meth:`htcondor.Submit.from_dag` if you convert it to
        a string, like ``Submit.from_dag(str(write_dag(...)))``.
    """
    return DAGWriter(dag, node_name_formatter=node_name_formatter).write(
        dag_dir, dag_file_name=dag_file_name,
    )


class DAGWriter:
    """Not re-entrant!"""

    def __init__(
        self,
        dag: "dag.DAG",
        node_name_formatter: Optional[formatter.NodeNameFormatter] = None,
    ):
        self.dag = dag

        if node_name_formatter is None:
            node_name_formatter = formatter.SimpleFormatter()
        self.node_name_formatter = node_name_formatter

        self.join_factory = edges.JoinFactory()

    def write(
        self, dag_dir: Path, dag_file_name: Optional[str] = DEFAULT_DAG_FILE_NAME
    ) -> Path:
        dag_dir = Path(dag_dir).absolute()
        dag_file_name = dag_file_name or DEFAULT_DAG_FILE_NAME

        dag_dir.mkdir(parents=True, exist_ok=True)

        dag_file_path = dag_dir / dag_file_name

        self.write_dag_file(dag_file_path)
        self.write_submit_files_for_layers(dag_dir)
        if len(self.join_factory.joins) > 0:
            self.write_noop_submit_file(dag_dir)
        if len(self.dag.dagman_config) > 0:
            self.write_dagman_config_file(dag_dir)

        return dag_file_path

    def write_dag_file(self, dag_file_path):
        with dag_file_path.open(mode="w") as f:
            for line in self.yield_dag_file_lines():
                f.write(line + "\n")

    def write_submit_files_for_layers(self, path):
        for layer in (
            n
            for n in self.dag.nodes
            if isinstance(n, node.NodeLayer)
            and isinstance(n.submit_description, htcondor.Submit)
        ):
            text = str(layer.submit_description) + "\nqueue"
            (path / "{}.sub".format(layer.name)).write_text(text)

    def write_noop_submit_file(self, dag_dir):
        """
        Write out the shared submit file for the NOOP join nodes.
        This is not done by default; it is only done if we actually need a
        join node.
        """
        (dag_dir / NOOP_SUBMIT_FILE_NAME).touch(exist_ok=True)

    def yield_dag_file_lines(self) -> Iterator[str]:
        yield "# BEGIN META"
        for line in self.yield_dag_meta_lines():
            yield line
        yield "# END META"

        yield "# BEGIN NODES AND EDGES"
        for node in self.dag.walk(order=WalkOrder.BREADTH_FIRST):
            yield from self.yield_node_lines(node)
            yield from self.yield_edge_lines(node)
        yield from self.yield_join_node_lines()
        yield "# END NODES AND EDGES"

        if self.dag._final_node is not None:
            yield "# FINAL NODE"
            yield from self.yield_node_lines(self.dag._final_node)
            yield "# END FINAL NODE"

    def yield_join_node_lines(self):
        for join in self.join_factory.joins:
            yield "JOB {} {} NOOP".format(
                self.join_node_name(join), NOOP_SUBMIT_FILE_NAME
            )

    def yield_dag_meta_lines(self):
        if len(self.dag.dagman_config) > 0:
            yield "CONFIG {}".format(CONFIG_FILE_NAME)

        if self.dag.jobstate_log is not None:
            yield "JOBSTATE_LOG {}".format(self.dag.jobstate_log.as_posix())

        if self.dag.node_status_file is not None:
            nsf = self.dag.node_status_file
            parts = ["NODE_STATUS_FILE", nsf.path.as_posix()]
            if nsf.update_time is not None:
                parts.append(str(nsf.update_time))
            if nsf.always_update:
                parts.append("ALWAYS-UPDATE")
            yield " ".join(parts)

        if self.dag.dot_config is not None:
            c = self.dag.dot_config
            parts = [
                "DOT",
                c.path.as_posix(),
                "UPDATE" if c.update else "DONT-UPDATE",
                "OVERWRITE" if c.overwrite else "DONT-OVERWRITE",
            ]
            if c.include_file is not None:
                parts.extend(("INCLUDE", c.include_file.as_posix()))
            yield " ".join(parts)

        for k, v in self.dag.dagman_job_attrs.items():
            yield "SET_JOB_ATTR {} = {}".format(k, v)

        for category, value in self.dag.max_jobs_per_category.items():
            yield "CATEGORY {} {}".format(category, value)

    def write_dagman_config_file(self, dag_dir: Path):
        contents = "\n".join(
            "{} = {}".format(k, v) for k, v in self.dag.dagman_config.items()
        )
        (dag_dir / CONFIG_FILE_NAME).write_text(contents)

    def yield_node_lines(self, node_: node.BaseNode) -> Iterator[str]:
        if isinstance(node_, node.NodeLayer):
            yield from self.yield_layer_lines(node_)
        elif isinstance(node_, node.SubDAG):
            yield from self.yield_subdag_lines(node_)
        elif isinstance(node_, node.FinalNode):
            yield from self.yield_final_node_lines(node_)
        else:
            raise TypeError(
                "unrecognized node type ({}) for node {}".format(node_.__class__, node_)
            )

    def yield_layer_lines(self, layer: node.NodeLayer) -> Iterator[str]:
        # write out each low-level dagman node in the layer
        for idx, vars in enumerate(layer.vars):
            name = self.get_node_name(layer, idx)
            sub_file = (
                "{}.sub".format(layer.name)
                if isinstance(layer.submit_description, htcondor.Submit)
                else layer.submit_description.absolute().as_posix()
            )
            parts = ["JOB {} {}".format(name, sub_file)] + self.get_node_meta_parts(
                layer, idx
            )
            yield " ".join(parts)

            if len(vars) > 0:
                parts = ["VARS {}".format(name)]
                for key, value in vars.items():
                    value_text = str(value).replace("\\", "\\\\").replace('"', r"\"")
                    parts.append('{}="{}"'.format(key, value_text))
                yield " ".join(parts)

            yield from self.yield_node_meta_lines(layer, name)

    def yield_subdag_lines(self, subdag: node.SubDAG) -> Iterator[str]:
        name = self.get_node_name(subdag, 0)
        parts = ["SUBDAG EXTERNAL {} {}".format(name, subdag.dag_file)]
        parts += self.get_node_meta_parts(subdag, 0)
        yield " ".join(parts)

        yield from self.yield_node_meta_lines(subdag, name)

    def yield_final_node_lines(self, n: node.FinalNode) -> Iterator[str]:
        yield "FINAL {name} {name}.sub".format(name=n.name)
        yield from self.yield_node_meta_lines(n, n.name)

    def get_node_meta_parts(self, n: node.BaseNode, idx: int) -> List[str]:
        parts = []

        if n.dir is not None:
            parts.extend(("DIR", str(n.dir)))

        if n.noop.get(idx, False):
            parts.append("NOOP")

        if n.done.get(idx, False):
            parts.append("DONE")

        return parts

    def yield_node_meta_lines(self, node: node.BaseNode, name: str) -> Iterator[str]:
        if node.retries is not None:
            parts = ["RETRY {} {}".format(name, node.retries)]
            if node.retry_unless_exit is not None:
                parts.append("UNLESS-EXIT {}".format(node.retry_unless_exit))
            yield " ".join(parts)

        if node.pre is not None:
            yield from self.yield_script_line(name, node.pre, "PRE")
        if node.post is not None:
            yield from self.yield_script_line(name, node.post, "POST")

        if node.pre_skip_exit_code is not None:
            yield "PRE_SKIP {} {}".format(name, node.pre_skip_exit_code)

        if node.priority != 0:
            yield "PRIORITY {} {}".format(name, node.priority)

        if node.category is not None:
            yield "CATEGORY {} {}".format(name, node.category)

        if node.abort is not None:
            parts = ["ABORT-DAG-ON {} {}".format(name, node.abort.node_exit_value)]
            if node.abort.dag_return_value is not None:
                parts.append("RETURN {}".format(node.abort.dag_return_value))
            yield " ".join(parts)

    def yield_script_line(
        self, name: str, script: node.Script, which: str
    ) -> Iterator[str]:
        parts = ["SCRIPT"]

        if script.retry:
            parts.extend(["DEFER", script.retry_status, script.retry_delay])

        parts.extend((which.upper(), name, script.executable, *script.arguments))

        yield " ".join(str(p) for p in parts)

    def get_node_name(self, n: node.BaseNode, idx: int) -> str:
        return self.node_name_formatter.generate(n.name, idx)

    def get_indexes_to_node_names(self, n: node.BaseNode) -> Dict[int, str]:
        if isinstance(n, node.SubDAG):
            return {0: self.get_node_name(n, 0)}
        elif isinstance(n, node.NodeLayer):
            return {idx: self.get_node_name(n, idx) for idx in range(len(n.vars))}
        else:
            raise TypeError(
                "Was not able to generate node names for node {} because it was not a recognized node type".format(
                    n
                )
            )

    def join_node_name(self, join: edges.JoinNode) -> str:
        return self.node_name_formatter.generate("__JOIN__", join.id)

    def yield_edge_lines(self, parent_layer: node.BaseNode) -> Iterator[str]:
        parent_layer_nodes = self.get_indexes_to_node_names(parent_layer)
        for child_layer in parent_layer.children:
            child_layer_nodes = self.get_indexes_to_node_names(child_layer)

            edge = self.dag._edges.get(parent_layer, child_layer)

            for p, c in edge.get_edges(parent_layer, child_layer, self.join_factory):
                parent_node_names = (
                    (parent_layer_nodes[_] for _ in p)
                    if not isinstance(p, edges.JoinNode)
                    else (self.join_node_name(p),)
                )
                child_node_names = (
                    (child_layer_nodes[_] for _ in c)
                    if not isinstance(c, edges.JoinNode)
                    else (self.join_node_name(c),)
                )
                yield "PARENT {} CHILD {}".format(
                    " ".join(parent_node_names), " ".join(child_node_names)
                )
