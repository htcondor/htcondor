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


from typing import Optional, Dict, Iterable, Union, List, Iterator, Mapping

import itertools
import functools
from pathlib import Path
import abc

import htcondor

from . import dag, edges, utils
from .walk_order import WalkOrder


class Script:
    def __init__(
        self,
        executable: Union[str, Path],
        arguments: Optional[List[str]] = None,
        retry: bool = False,
        retry_status: int = 1,
        retry_delay: int = 0,
    ):
        """
        Parameters
        ----------
        executable
            The path to the executable to run.
        arguments
            The individual arguments to the executable. Keep in mind that these
            are evaluated as soon as the :class:`Script` is created!
        retry
            ``True`` if the script can be retried on failure.
        retry_status
            If the script exits with this status, the script run will be
            considered a failure for the purposes of retrying.
        retry_delay
            The number of seconds to wait after a script failure before
            retrying.
        """
        self.executable = executable
        if arguments is None:
            arguments = []
        self.arguments = [str(arg) for arg in arguments]

        self.retry = retry
        self.retry_status = retry_status
        self.retry_delay = retry_delay

    def __repr__(self) -> str:
        return utils.make_repr(
            self, ("executable", "arguments", "retry", "retry_status", "retry_delay")
        )


class DAGAbortCondition:
    """
    Represents the configuration of a node's DAG abort condition.

    See :ref:`abort-dag-on` for more information about DAG aborts.
    """

    def __init__(self, node_exit_value: int, dag_return_value: Optional[int] = None):
        """
        Parameters
        ----------
        node_exit_value
            If the underlying node exits with this value, the DAG will be aborted.
        dag_return_value
            If the DAG is aborted via this condition, it will exit with this code, if given.
            If not given, it will exit with the same return value that the node did.
        """
        self.node_exit_value = node_exit_value
        self.dag_return_value = dag_return_value

    def __repr__(self) -> str:
        return utils.make_repr(self, ("node_exit_value", "dag_return_value"))


@functools.total_ordering
class BaseNode(abc.ABC):
    """
    This is the superclass for all node-like objects
    (things that can be the logical nodes in a :class:`DAG`,
    like :class:`NodeLayer` and :class:`SubDAG`).

    Generally, you do not need to construct nodes yourself; instead, they are
    created by calling methods like :meth:`DAG.layer`, :meth:`DAG.subdag`,
    :meth:`BaseNode.child_layer`, and so forth. These methods automatically
    attach the new node to the same :class:`DAG` as the node you called the
    method on.
    """

    def __init__(
        self,
        dag: "dag.DAG",
        *,
        name: str,
        dir: Optional[Path] = None,
        noop: Union[bool, Mapping[int, bool]] = False,
        done: Union[bool, Mapping[int, bool]] = False,
        retries: Optional[int] = None,
        retry_unless_exit: Optional[int] = None,
        pre: Optional[Script] = None,
        post: Optional[Script] = None,
        pre_skip_exit_code: Optional[int] = None,
        priority: int = 0,
        category: Optional[str] = None,
        abort: Optional[DAGAbortCondition] = None
    ):
        """
        Parameters
        ----------
        dag
            Which :class:`DAG` to attach this node to.
        name
            The human-readable name of this node.
        dir
            The directory to submit from. If ``None``, it will be the directory
            the DAG itself was submitted from.
        noop
            If this is ``True``, this node will be skipped and marked as completed,
            no matter what it says it does.
            For a :class:`NodeLayer`, this can be dictionary mapping individual
            underlying node indices to their desired value.
        done
            If this is ``True``, this node will be considered already completed.
            For a :class:`NodeLayer`, this can be dictionary mapping individual
            underlying node indices to their desired value.
        retries
            The number of times to retry the node if it fails
            (defined by ``retry_unless_exit``).
        retry_unless_exit
            If the node exits with this code, it will not be retried.
        pre
            A :class:`Script` to run before the node itself.
        post
            A :class:`Script` to run after the node itself.
        pre_skip_exit_code
            If the pre-script exits with this code, the node will be skipped.
        priority
            The internal priority for DAGMan to run this node.
        category
            Which ``CATEGORY`` this node belongs to.
        abort
            A :class:`DAGAbortCondition` which may cause the entire DAG to stop
            if this node exits in a certain way.
        """
        self._dag = dag
        self.name = name

        self.dir = Path(dir) if dir is not None else None
        if isinstance(noop, bool):
            noop = {0: noop}
        self.noop = noop
        if isinstance(done, bool):
            done = {0: done}
        self.done = done

        self.retries = retries
        self.retry_unless_exit = retry_unless_exit
        self.priority = priority
        self.category = category
        self.abort = abort

        self.pre = pre
        self.pre_skip_exit_code = pre_skip_exit_code
        self.post = post

    def __len__(self):
        return 1

    def __repr__(self) -> str:
        return utils.make_repr(self, ("name",))

    def __iter__(self) -> "BaseNode":
        yield self

    def __hash__(self):
        return hash((self.__class__, self.name))

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return NotImplemented
        return self._dag == other._dag and self.name == other.name

    def __lt__(self, other):
        if not isinstance(other, NodeLayer):
            return NotImplemented
        return self.name < other.name

    def child_layer(
        self, edge: Optional[edges.BaseEdge] = None, **kwargs
    ) -> "NodeLayer":
        """
        Create a new :class:`NodeLayer` which is a child of this node.

        Parameters
        ----------
        edge
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`NodeLayer`
            constructor.

        Returns
        -------
        node_layer : :class:`NodeLayer`
            The newly-created node layer.
        """
        node = self._dag.layer(**kwargs)

        self._dag._edges.add(self, node, edge=edge)

        return node

    def parent_layer(
        self, edge: Optional[edges.BaseEdge] = None, **kwargs
    ) -> "NodeLayer":
        """
        Create a new :class:`NodeLayer` which is a parent of this node.

        Parameters
        ----------
        edge
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`NodeLayer`
            constructor.

        Returns
        -------
        node_layer : :class:`NodeLayer`
            The newly-created node layer.
        """
        node = self._dag.layer(**kwargs)

        self._dag._edges.add(node, self, edge=edge)

        return node

    def child_subdag(self, edge: Optional[edges.BaseEdge] = None, **kwargs) -> "SubDAG":
        """
        Create a new :class:`SubDAG` which is a child of this node.

        Parameters
        ----------
        edge
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`SubDAG`
            constructor.

        Returns
        -------
        subdag : :class:`SubDAG`
            The newly-created sub-DAG.
        """
        node = self._dag.subdag(**kwargs)

        self._dag._edges.add(self, node, edge=edge)

        return node

    def parent_subdag(
        self, edge: Optional[edges.BaseEdge] = None, **kwargs
    ) -> "SubDAG":
        """
        Create a new :class:`SubDAG` which is a parent of this node.

        Parameters
        ----------
        edge
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`SubDAG`
            constructor.

        Returns
        -------
        subdag : :class:`SubDAG`
            The newly-created sub-DAG.
        """
        node = self._dag.subdag(**kwargs)

        self._dag._edges.add(node, self, edge=edge)

        return node

    def add_children(self, *nodes, edge: Optional[edges.BaseEdge] = None) -> "BaseNode":
        """
        Makes all of the ``nodes`` children of this node.

        Parameters
        ----------
        nodes
            The nodes to make children of this node.
        edge
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.

        Returns
        -------
        self : :class:`BaseNode`
            This method returns ``self``.
        """
        nodes = utils.flatten(nodes)
        for node in nodes:
            self._dag._edges.add(self, node, edge=edge)

        return self

    def remove_children(self, *nodes) -> "BaseNode":
        """
        Makes sure that the ``nodes`` are **not** children of this node.

        Parameters
        ----------
        nodes
            The nodes to remove edges from.

        Returns
        -------
        self : :class:`BaseNode`
            This method returns ``self``.
        """
        nodes = utils.flatten(nodes)
        for node in nodes:
            self._dag._edges.remove(self, node)

        return self

    def add_parents(self, *nodes, edge: Optional[edges.BaseEdge] = None) -> "BaseNode":
        """
        Makes all of the ``nodes`` parents of this node.

        Parameters
        ----------
        nodes
            The nodes to make parents of this node.
        edge
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.

        Returns
        -------
        self : :class:`BaseNode`
            This method returns ``self``.
        """
        nodes = utils.flatten(nodes)
        for node in nodes:
            self._dag._edges.add(node, self, edge=edge)

        return self

    def remove_parents(self, *nodes) -> "BaseNode":
        """
        Makes sure that the ``nodes`` are **not** parents of this node.

        Parameters
        ----------
        nodes
            The nodes to remove edges from.

        Returns
        -------
        self : :class:`BaseNode`
            This method returns ``self``.
        """
        nodes = utils.flatten(nodes)
        for node in nodes:
            self._dag._edges.remove(node, self)

        return self

    @property
    def children(self) -> "Nodes":
        """Return a :class:`Nodes` containing all of the children of this node."""
        return self._dag.node_to_children[self]

    @property
    def parents(self) -> "Nodes":
        """Return a :class:`Nodes` containing all of the parents of this node."""
        return self._dag.node_to_parents[self]

    def walk_ancestors(
        self, order: WalkOrder = WalkOrder.DEPTH_FIRST
    ) -> Iterator["BaseNode"]:
        """Walk over all of the ancestors of this node, in the given order."""
        return self._dag.walk_ancestors(node=self, order=order)

    def walk_descendants(
        self, order: WalkOrder = WalkOrder.DEPTH_FIRST
    ) -> Iterator["BaseNode"]:
        """Walk over all of the descendants of this node, in the given order."""
        return self._dag.walk_descendants(node=self, order=order)


class NodeLayer(BaseNode):
    """
    Represents a "layer" of actual ``JOB`` nodes that share a submit description
    and edge relationships.
    Each underlying actual node's attributes may be customized using ``vars``.
    """

    def __init__(
        self,
        dag: "dag.DAG",
        *,
        submit_description: Union[Optional[htcondor.Submit], Path] = None,
        vars: Optional[Iterable[Dict[str, str]]] = None,
        **kwargs
    ):
        """
        Parameters
        ----------
        dag
            The DAG to connect this node to.
        submit_description
            The HTCondor submit description for this node. Can be either an
            :class:`htcondor.Submit` object or a :class:`~pathlib.Path` to an
            existing submit file on disk.
        vars
            The ``VARS`` for this logical node; one actual node will be created
            for each dictionary in the ``vars``.
        kwargs
            Additional keyword arguments are passed to the :class:`BaseNode`
            constructor.
        """
        super().__init__(dag, **kwargs)

        self.submit_description = submit_description or htcondor.Submit({})

        # todo: this is bad, should be an empty list
        if vars is None:
            vars = [{}]
        self.vars = list(vars)

    def __len__(self):
        """The number of actual nodes in the layer."""
        return len(self.vars)


class SubDAG(BaseNode):
    """
    Represents a ``SUBDAG`` in the graph.

    See :ref:`subdag-external` for more information on sub-DAGs.
    """

    def __init__(self, dag: "dag.DAG", *, dag_file: Path, **kwargs):
        """
        Parameters
        ----------
        dag
            The DAG to connect this node to.
        dag_file
            The :class:`pathlib.Path` to where the sub-DAG's DAG description file
            is (or will be).
        kwargs
            Additional keyword arguments are passed to the :class:`BaseNode`
            constructor.
        """
        super().__init__(dag, **kwargs)

        self.dag_file = dag_file


class FinalNode(BaseNode):
    """
    Represents the ``FINAL`` node in a DAG.

    See :ref:`final-node` for more information on the ``FINAL`` node.
    """

    def __init__(
        self,
        dag: "dag.DAG",
        submit_description: Union[Optional[htcondor.Submit], Path] = None,
        **kwargs
    ):
        """
        Parameters
        ----------
        dag
            The DAG to connect this node to.
        submit_description
            The HTCondor submit description for this node. Can be either an
            :class:`htcondor.Submit` object or a :class:`~pathlib.Path` to an
            existing submit file on disk.
        kwargs
            Additional keyword arguments are passed to the :class:`BaseNode`
            constructor.
        """
        super().__init__(dag, **kwargs)

        self.submit_description = submit_description or htcondor.Submit({})


class Nodes:
    """
    This class represents an arbitrary collection of :class:`BaseNode`.
    In many cases, especially when manipulating the structure of the graph,
    it can be used as a replacement for directly iterating over
    collections of nodes.
    """

    def __init__(self, *nodes: Union[BaseNode, Iterable[BaseNode]]):
        """
        Parameters
        ----------
        nodes
            The logical nodes that will be in this :class:`Nodes`.
        """
        self.nodes = dag.NodeStore()
        nodes = utils.flatten(nodes)
        for node in nodes:
            self.nodes.add(node)

    def __eq__(self, other):
        return self.nodes == other.nodes

    def __len__(self) -> int:
        return len(self.nodes)

    def __iter__(self) -> Iterator[BaseNode]:
        yield from self.nodes

    def __contains__(self, node) -> bool:
        return node in self.nodes

    def __repr__(self) -> str:
        return "Nodes({})".format(
            ", ".join(repr(n) for n in sorted(self.nodes, key=lambda n: n.name))
        )

    def __str__(self) -> str:
        return "Nodes({})".format(
            ", ".join(str(n) for n in sorted(self.nodes, key=lambda n: n.name))
        )

    def _some_element(self) -> BaseNode:
        return next(iter(self.nodes))

    def child_layer(self, type: Optional[edges.BaseEdge] = None, **kwargs) -> NodeLayer:
        """
        Create a new :class:`NodeLayer` which is a child of all of the nodes in
        this :class:`Nodes`.

        Parameters
        ----------
        type
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`NodeLayer`
            constructor.

        Returns
        -------
        node_layer : :class:`NodeLayer`
            The newly-created node layer.
        """
        node = self._some_element().child_layer(**kwargs)

        node.add_parents(self, edge=type)

        return node

    def parent_layer(
        self, type: Optional[edges.BaseEdge] = None, **kwargs
    ) -> NodeLayer:
        """
        Create a new :class:`NodeLayer` which is a parent of all of the nodes in
        this :class:`Nodes`.

        Parameters
        ----------
        type
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`NodeLayer`
            constructor.

        Returns
        -------
        node_layer : :class:`NodeLayer`
            The newly-created node layer.
        """
        node = self._some_element().parent_layer(**kwargs)

        node.add_children(self, edge=type)

        return node

    def child_subdag(self, type: Optional[edges.BaseEdge] = None, **kwargs) -> SubDAG:
        """
        Create a new :class:`SubDAG` which is a child of all of the nodes in
        this :class:`Nodes`.

        Parameters
        ----------
        type
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`SubDAG`
            constructor.

        Returns
        -------
        subdag : :class:`SubDAG`
            The newly-created sub-DAG.
        """
        node = self._some_element().child_subdag(**kwargs)

        node.add_parents(self, edge=type)

        return node

    def parent_subdag(self, type: Optional[edges.BaseEdge] = None, **kwargs) -> SubDAG:
        """
        Create a new :class:`SubDAG` which is a parent of all of the nodes in
        this :class:`Nodes`.

        Parameters
        ----------
        type
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.
        kwargs
            Additional keyword arguments are passed to the :class:`SubDAG`
            constructor.

        Returns
        -------
        subdag : :class:`SubDAG`
            The newly-created sub-DAG.
        """
        node = self._some_element().parent_subdag(**kwargs)

        node.add_children(self, edge=type)

        return node

    def add_children(self, *nodes, type: Optional[edges.BaseEdge] = None) -> "Nodes":
        """
        Makes all of the ``nodes`` children of all of the nodes
        in this :class:`Nodes`.

        Parameters
        ----------
        nodes
            The nodes to make children of this :class:`Nodes`.
        type
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.

        Returns
        -------
        self : :class:`Nodes`
            This method returns ``self``.
        """
        for s in self:
            s.add_children(nodes, edge=type)

        return self

    def remove_children(self, *nodes) -> "Nodes":
        """
        Makes sure that the ``nodes`` are **not** children of all of the nodes
        in this :class:`Nodes`.

        Parameters
        ----------
        nodes
            The nodes to remove edges from.

        Returns
        -------
        self : :class:`Nodes`
            This method returns ``self``.
        """
        for s in self:
            s.remove_children(nodes)

        return self

    def add_parents(self, *nodes, type: Optional[edges.BaseEdge] = None) -> "Nodes":
        """
        Makes all of the ``nodes`` parents of all of the nodes in this
        :class:`Nodes`.

        Parameters
        ----------
        nodes
            The nodes to make parents of this :class:`Nodes`.
        type
            The type of edge to use; an instance of a concrete subclass of
            :class:`BaseEdge`. If ``None``, a :class:`ManyToMany` edge will be
            used.

        Returns
        -------
        self : :class:`Nodes`
            This method returns ``self``.
        """
        for s in self:
            s.add_parents(nodes, edge=type)

        return self

    def remove_parents(self, *nodes) -> "Nodes":
        """
        Makes sure that the ``nodes`` are **not** parents of any of the nodes
        in this :class:`Nodes`.

        Parameters
        ----------
        nodes
            The nodes to remove edges from.

        Returns
        -------
        self : :class:`Nodes`
            This method returns ``self``.
        """
        for s in self:
            s.remove_parents(nodes)

        return self

    def walk_ancestors(self, order: WalkOrder = WalkOrder.DEPTH_FIRST):
        """Walk over all of the ancestors of all of the nodes in this :class:`Nodes`, in the given order."""
        return itertools.chain.from_iterable(
            n.walk_ancestors(order=order) for n in self
        )

    def walk_descendants(self, order: WalkOrder = WalkOrder.DEPTH_FIRST):
        """Walk over all of the descendants of all of the nodes in this :class:`Nodes`, in the given order."""
        return itertools.chain.from_iterable(
            n.walk_descendants(order=order) for n in self
        )
