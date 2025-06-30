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

from typing import Optional, Dict, Union, Any, Iterator, Callable, Tuple, Set, Mapping
import logging

import collections
import functools
from pathlib import Path
import collections.abc
import fnmatch

from . import node, edges, utils, exceptions
from .walk_order import WalkOrder

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class DotConfig:
    """
    A :class:`DotConfig` holds the configuration options for whether and how
    DAGMan will produce a DOT file representing its execution graph.

    See :ref:`visualizing-dags-with-dot` for more information.
    """

    def __init__(
        self,
        path: Path,
        update: bool = False,
        overwrite: bool = True,
        include_file: Optional[Path] = None,
    ):
        """
        Parameters
        ----------
        path
            The path to write the DOT file to.
        update
            If ``True``, the DOT file will be updated as the DAG executes.
            If ``False``, it will be written once at startup.
        overwrite
            If ``True``, the DOT file will be updated in-place.
            If ``False``, new DOT files will be created next to the original.
        include_file
            Include the contents of the file at this path in the DOT file.
        """
        self.path = Path(path)
        self.update = update
        self.overwrite = overwrite
        self.include_file = include_file if include_file is None else Path(include_file)

    def __repr__(self) -> str:
        return utils.make_repr(self, ("path", "update", "overwrite", "include_file"))


class NodeStatusFile:
    """
    A :class:`NodeStatusFile` holds the configuration options for whether and how
    DAGMan will write a file containing node status information.

    See :ref:`node-status-file` for more information.
    """

    def __init__(
        self,
        path: Path,
        update_time: Optional[int] = None,
        always_update: Optional[bool] = False,
    ):
        """

        Parameters
        ----------
        path
            The path to write the node status file to.
        update_time
            The minimum interval to write new information to the node status file.
        always_update
            Always update the node status file after the ``update_time``, even
            if there are no changes from the previous update.
        """
        self.path = Path(path)
        self.update_time = update_time
        self.always_update = always_update

    def __repr__(self) -> str:
        return utils.make_repr(self, ("path", "update_time", "always_update"))


def _check_node_name_uniqueness(func):
    @functools.wraps(func)
    def wrapper(dag: "DAG", **kwargs):
        name = kwargs["name"]
        if name in dag._nodes:
            raise exceptions.DuplicateNodeName(
                "the DAG already has a node named {}: {}".format(name, dag._nodes[name])
            )
        if dag._final_node is not None and dag._final_node.name == name:
            raise exceptions.DuplicateNodeName(
                "the DAG already has a node named {}: {}".format(name, dag._final_node)
            )

        return func(dag, **kwargs)

    return wrapper


class DAG:
    """
    This object represents the entire DAGMan workflow, including both the
    execution graph and miscellaneous configuration options.

    It contains the individual :class:`NodeLayer` and :class:`SubDAG` that are
    the "logical" nodes in the graph, created by the :meth:`layer` and
    :meth:`subdag` methods respectively.
    """

    def __init__(
        self,
        dagman_config: Optional[Mapping[str, Any]] = None,
        dagman_job_attributes: Optional[Mapping[str, Any]] = None,
        max_jobs_by_category: Optional[Mapping[str, int]] = None,
        dot_config: Optional[DotConfig] = None,
        jobstate_log: Optional[Path] = None,
        node_status_file: Optional[NodeStatusFile] = None,
    ):
        """
        Parameters
        ----------
        dagman_config
            A mapping of DAGMan configuration options.
        dagman_job_attributes
            A mapping that describes additional HTCondor JobAd attributes for
            the DAGMan job itself.
        max_jobs_by_category
            A mapping that describes the maximum number of jobs (values) that
            should be run simultaneously from each category (keys).
        dot_config
            Configuration options for writing a DOT file,
            as a :class:`DotConfig`.
        jobstate_log
            The path to the jobstate log. If not given, the jobstate log will
            not be written.
        node_status_file
            Configuration options for the node status file,
            as a :class:`NodeStatusFile`.
        """
        self._nodes = NodeStore()
        self._edges = EdgeStore()
        self._final_node = None

        self.jobstate_log = jobstate_log if jobstate_log is None else Path(jobstate_log)
        self.max_jobs_per_category = max_jobs_by_category or {}
        self.dagman_config = dagman_config or {}
        self.dagman_job_attrs = dagman_job_attributes or {}
        self.dot_config = dot_config
        self.node_status_file = node_status_file

    def walk(self, order: WalkOrder = WalkOrder.DEPTH_FIRST) -> Iterator[node.BaseNode]:
        """
        Iterate over all of the nodes in the DAG, starting from the roots
        (i.e., the nodes with no parents),
        in either depth-first or breadth-first order.

        Sibling order is not specified,
        and may be different in different calls to this method.

        Parameters
        ----------
        order
            Walk depth-first (children before siblings)
            or breadth-first (siblings before children).
        """
        yield from self._walk(
            initial_stack=self.roots, add_to_stack=lambda n: n.children, order=order
        )

    def walk_ancestors(
        self, node: node.BaseNode, order: WalkOrder = WalkOrder.DEPTH_FIRST
    ) -> Iterator[node.BaseNode]:
        """
        Iterate over all of the ancestors
        (i.e., parents, parents of parents, etc.)
        of some node,
        in either depth-first or breadth-first order.

        Sibling order is not specified,
        and may be different in different calls to this method.

        Parameters
        ----------
        node
            The node to begin walking from.
            It will not be included in the results.
        order
            Walk depth-first (parents before siblings)
            or breadth-first (siblings before parents).
        """
        yield from self._walk(
            initial_stack=self.node_to_parents[node],
            add_to_stack=lambda n: n.parents,
            order=order,
        )

    def walk_descendants(
        self, node: node.BaseNode, order: WalkOrder = WalkOrder.DEPTH_FIRST
    ) -> Iterator[node.BaseNode]:
        """
        Iterate over all of the descendants
        (i.e., children, children of children, etc.)
        of some node,
        in either depth-first or breadth-first order.

        Sibling order is not specified,
        and may be different in different calls to this method.

        Parameters
        ----------
        node
            The node to begin walking from.
            It will not be included in the results.
        order
            Walk depth-first (children before siblings)
            or breadth-first (siblings before children).
        """
        yield from self._walk(
            initial_stack=self.node_to_children[node],
            add_to_stack=lambda n: n.children,
            order=order,
        )

    def _walk(self, initial_stack, add_to_stack, order):
        """Private helper method for public walk* methods."""
        seen = set()
        stack = collections.deque(initial_stack)

        while len(stack) != 0:
            if order is WalkOrder.DEPTH_FIRST:
                node = stack.pop()
            elif order is WalkOrder.BREADTH_FIRST:
                node = stack.popleft()
            else:
                raise exceptions.UnrecognizedWalkOrder(
                    "Unrecognized {}: {}".format(WalkOrder.__name__, order)
                )

            if node in seen:
                continue
            seen.add(node)

            stack.extend(add_to_stack(node))
            yield node

    @property
    def edges(
        self,
    ) -> Iterator[Tuple[Tuple[node.BaseNode, node.BaseNode], edges.BaseEdge]]:
        """
        Iterate over ``((parent, child), edge)`` tuples,
        for every edge in the graph.
        """
        yield from self._edges

    def __contains__(self, node) -> bool:
        return node in self._nodes

    @_check_node_name_uniqueness
    def layer(self, **kwargs) -> node.NodeLayer:
        """
        Create a new :class:`NodeLayer` in the graph with no parents or children.
        Keyword arguments are forwarded to :class:`NodeLayer`.
        """
        n = node.NodeLayer(dag=self, **kwargs)
        self._nodes.add(n)
        return n

    @_check_node_name_uniqueness
    def subdag(self, **kwargs) -> node.SubDAG:
        """
        Create a new :class:`SubDAG` in the graph with no parents or children.
        Keyword arguments are forwarded to :class:`SubDAG`.
        """
        n = node.SubDAG(dag=self, **kwargs)
        self._nodes.add(n)
        return n

    @_check_node_name_uniqueness
    def final(self, **kwargs) -> node.FinalNode:
        """
        Create the ``FINAL`` node of the DAG.
        A DAG can only have one ``FINAL`` node; if you call this method multiple
        times, it will override any previous calls.
        To customize the ``FINAL`` node after creation,
        modify the :class:`FinalNode` instance that it returns.
        """
        n = node.FinalNode(dag=self, **kwargs)
        self._final_node = n
        return n

    def select(self, selector: Callable[[node.BaseNode], bool]) -> node.Nodes:
        """
        Return a :class:`Nodes` of the nodes in the DAG that satisfy ``selector``.
        ``selector`` should be a function which takes a single :class:`BaseNode`
        and returns ``True`` (will be included) or ``False`` (will not be included).
        """
        return node.Nodes(
            *(node for name, node in self._nodes.items() if selector(node))
        )

    def glob(self, pattern: str) -> node.Nodes:
        """
        Return a :class:`Nodes` of the nodes in the DAG
        whose names match the glob ``pattern``.
        """
        return self.select(lambda node: fnmatch.fnmatchcase(node.name, pattern))

    @property
    def node_to_children(self) -> Dict[node.BaseNode, node.Nodes]:
        """
        Return a dictionary that maps each node to a :class:`Nodes`
        containing its children.
        The :class:`Nodes` will be empty if the node has no children.
        """
        d = {n: set() for n in self.nodes}
        for parent, child in self.edges:
            d[parent].add(child)

        return {k: node.Nodes(v) for k, v in d.items()}

    @property
    def node_to_parents(self) -> Dict[node.BaseNode, node.Nodes]:
        """
        Return a dictionary that maps each node to a :class:`Nodes`
        containing its parents.
        The :class:`Nodes` will be empty if the node has no parents.
        """
        d = {n: set() for n in self.nodes}
        for parent, child in self.edges:
            d[child].add(parent)

        return {k: node.Nodes(v) for k, v in d.items()}

    @property
    def nodes(self) -> node.Nodes:
        """Iterate over all of the nodes in the DAG, in no particular order."""
        return node.Nodes(self._nodes)

    @property
    def roots(self) -> node.Nodes:
        """A :class:`Nodes` of the nodes in the DAG that have no parents."""
        return node.Nodes(
            child
            for child, parents in self.node_to_parents.items()
            if len(parents) == 0
        )

    @property
    def leaves(self) -> node.Nodes:
        """A :class:`Nodes` of the nodes in the DAG that have no children."""
        return node.Nodes(
            parent
            for parent, children in self.node_to_children.items()
            if len(children) == 0
        )

    def describe(self) -> str:  # pragma: no cover
        """Return a tabular description of the DAG's structure."""
        rows = []

        for n in self.walk(WalkOrder.BREADTH_FIRST):
            if isinstance(n, node.NodeLayer):
                type, name = "Layer", n.name
                vars = len(n.vars)
            elif isinstance(n, node.SubDAG):
                type, name = "SubDAG", n.name
                vars = None
            else:
                raise Exception("Unrecognized node type: {}".format(n))

            children = len(n.children)

            if len(n.parents) > 0:
                parents = ", ".join(
                    "{}[{}]".format(p.name, self._edges.get(p, n)) for p in n.parents
                )
            else:
                parents = None

            rows.append((type, name, vars, children, parents))

        return utils.table(
            headers=["Type", "Name", "# Nodes", "# Children", "Parents"],
            rows=rows,
            alignment={"Type": "ljust", "Parents": "ljust"},
        )


class EdgeStore:
    """
    An EdgeStore stores edges for a DAG.

    This object is for internal use only.
    """

    def __init__(self):
        self.edges = {}

    def __iter__(self) -> Iterator[edges.BaseEdge]:
        yield from self.edges

    def __contains__(self, item) -> bool:
        return item in self.edges

    def items(
        self,
    ) -> Iterator[Tuple[Tuple[node.BaseNode, node.BaseNode], edges.BaseEdge]]:
        yield from self.edges.items()

    def get(
        self, parent: node.BaseNode, child: node.BaseNode
    ) -> Optional[edges.BaseEdge]:
        try:
            return self.edges[(parent, child)]
        except KeyError:
            return None

    def add(
        self,
        parent: node.BaseNode,
        child: node.BaseNode,
        edge: Optional[edges.BaseEdge] = None,
    ) -> None:
        if edge is None:
            edge = edges.ManyToMany()
        self.edges[(parent, child)] = edge

    def pop(
        self, parent: node.BaseNode, child: node.BaseNode
    ) -> Optional[edges.BaseEdge]:
        return self.edges.pop((parent, child), None)


class NodeStore:
    """
    A NodeStore behaves roughly like a dictionary mapping node names to nodes.
    However, it does not support setting items. Instead, you can ``add`` or
    ``remove`` nodes from the store. nodes.Nodes can be specified by name, or by the
    actual node instance, for flexibility.

    This object is for internal use only.
    """

    def __init__(self):
        self.nodes = {}

    def add(self, *nodes: node.BaseNode) -> None:
        for n in nodes:
            if isinstance(n, node.BaseNode):
                self.nodes[n.name] = n
            elif isinstance(n, node.Nodes):
                self.add(self, n)

    def remove(self, *nodes: node.BaseNode) -> None:
        for n in nodes:
            if isinstance(n, str):
                self.nodes.pop(n, None)
            elif isinstance(n, node.BaseNode):
                self.nodes.pop(n.name, None)
            elif isinstance(n, node.Nodes):
                self.remove(n)

    def __getitem__(self, n: Union[node.BaseNode, str]) -> node.BaseNode:
        if isinstance(n, str):
            return self.nodes[n]
        elif isinstance(n, node.BaseNode):
            return self.nodes[n.name]
        else:
            raise TypeError("Nodes can be retrieved by name or value")

    def __iter__(self) -> Iterator[node.BaseNode]:
        yield from self.nodes.values()

    def __contains__(self, n: Union[node.BaseNode, str]) -> bool:
        if isinstance(n, node.BaseNode):
            return n in self.nodes.values()
        elif isinstance(n, str):
            return n in self.nodes.keys()
        return False

    def items(self) -> Iterator[Tuple[str, node.BaseNode]]:
        yield from self.nodes.items()

    def __repr__(self) -> str:
        return repr(set(self.nodes.values()))

    def __str__(self) -> str:
        return str(set(self.nodes.values()))

    def __len__(self) -> int:
        return len(self.nodes)

    def __eq__(self, other):
        return self.nodes == other.nodes
