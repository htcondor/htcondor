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

from typing import Tuple, Iterable, Union

import abc
import itertools

from . import node, utils, exceptions


class JoinNode:
    def __init__(self, id: int):
        self.id = id


class JoinFactory:
    def __init__(self):
        self.id_generator = itertools.count(0)
        self.joins = []

    def get_join_node(self) -> JoinNode:
        j = JoinNode(next(self.id_generator))
        self.joins.append(j)
        return j


class BaseEdge(abc.ABC):
    """
    An abstract class that represents the edge between two logical nodes
    in the DAG.
    """

    @abc.abstractmethod
    def get_edges(
        self, parent: "node.BaseNode", child: "node.BaseNode", join_factory: JoinFactory
    ) -> Iterable[
        Union[
            Tuple[Tuple[int], Tuple[int]],
            Tuple[Tuple[int], JoinNode],
            Tuple[JoinNode, Tuple[int]],
        ]
    ]:
        """
        This abstract method is used by the writer to figure out which nodes
        in the parent and child should be connected by an actual DAGMan
        edge. It should yield (or simply return an iterable of)
        individual edge specifications.

        Each edge specification is a tuple containing two elements: the first is
        a group of parent node indices, the second is a group of child node indices.
        Either (but not both) may be replaced by a special :class:`JoinNode` object
        provided by :meth:`JoinFactory.get_join_node`. An instance of this class
        is passed into this function by the writer; you should not create one
        yourself.

        You may yield any number of edge specifications, but the more compact
        you can make the representation
        (i.e., fewer edge specifications, each with fewer elements), the better.
        This is where join nodes are helpful: they can turn "many-to-many"
        relationships into a significantly smaller number of actual edges
        (:math:`2N` instead of :math:`N^2`).

        A :class:`SubDAG` or a zero-vars :class:`NodeLayer` both implicitly
        have a single node index, ``0``. See the source code of :class:`ManyToMany`
        for a simple pattern for dealing with this.

        Parameters
        ----------
        parent
            The parent, a concrete subclass of :class:`BaseNode`.
        child
            The child, a concrete subclass of :class:`BaseNode`.
        join_factory
            An instance of :class:`JoinFactory` that will be provided by the
            writer.

        Returns
        -------

        """
        raise NotImplementedError

    def __repr__(self) -> str:
        return self.__class__.__name__


class ManyToMany(BaseEdge):
    """
    This edge connects two layers "densely": every node in the child layer
    is a child of every node in the parent layer.
    """

    def get_edges(
        self, parent: "node.BaseNode", child: "node.BaseNode", join_factory: JoinFactory
    ) -> Iterable[
        Union[
            Tuple[Tuple[int], Tuple[int]],
            Tuple[Tuple[int], JoinNode],
            Tuple[JoinNode, Tuple[int]],
        ]
    ]:
        num_parent_vars = len(parent)
        num_child_vars = len(child)

        if num_parent_vars == 1 or num_child_vars == 1:
            # the weird pattern here is just to symmetrize the result, so that
            # we don't care which number of vars was 1
            yield tuple(range(num_parent_vars)), tuple(range(num_child_vars))
        else:
            join = join_factory.get_join_node()
            yield tuple(range(num_parent_vars)), join
            yield join, tuple(range(num_child_vars))


class OneToOne(BaseEdge):
    """
    This edge connects two layers "linearly": each underlying node in the child
    layer is a child of the corresponding underlying node with the same index
    in the parent layer.
    The parent and child layers must have the same number of underlying nodes.
    """

    def get_edges(
        self, parent: "node.BaseNode", child: "node.BaseNode", join_factory: JoinFactory
    ) -> Iterable[
        Union[
            Tuple[Tuple[int], Tuple[int]],
            Tuple[Tuple[int], JoinNode],
            Tuple[JoinNode, Tuple[int]],
        ]
    ]:
        num_parent_vars = len(parent)
        num_child_vars = len(child)

        if num_parent_vars != num_child_vars:
            raise exceptions.OneToOneEdgeNeedsSameNumberOfVars(
                "Parent layer {} has {} nodes, but child layer {} has {} nodes".format(
                    parent, num_parent_vars, child, num_child_vars
                )
            )

        yield from (((i,), (i,)) for i in range(num_parent_vars))


class Grouper(BaseEdge):
    """
    This edge connects two layers in "chunks". The nodes in each layer are
    divided into chunks based on their respective chunk sizes (given in the
    constructor). Chunks are then connected like a :class:`OneToOne` edge.

    The number of chunks in each layer must be the same, and each layer must be
    evenly-divided into chunks (no leftover underlying nodes).

    When both chunk sizes are ``1`` this is identical to a :class:`OneToOne`
    edge, and you should use that edge instead because it produces a more
    compact representation.
    """

    def __init__(self, parent_chunk_size: int = 1, child_chunk_size: int = 1):
        """
        Parameters
        ----------
        parent_chunk_size
            The number of nodes in each chunk in the parent layer.
        child_chunk_size
            The number of nodes in each chunk in the child layer.
        """
        self.parent_chunk_size = parent_chunk_size
        self.child_chunk_size = child_chunk_size

    def get_edges(
        self, parent: "node.BaseNode", child: "node.BaseNode", join_factory: JoinFactory
    ) -> Iterable[
        Union[
            Tuple[Tuple[int], Tuple[int]],
            Tuple[Tuple[int], JoinNode],
            Tuple[JoinNode, Tuple[int]],
        ]
    ]:
        num_parent_vars = len(parent)
        num_child_vars = len(child)

        if num_parent_vars % self.parent_chunk_size != 0:
            raise exceptions.IncompatibleGrouper(
                "Cannot apply edge {} to parent layer {} because number of real parent nodes ({}) is not evenly divisible by the parent chunk size ({})".format(
                    self, parent, len(parent), self.parent_chunk_size
                )
            )
        if num_child_vars % self.child_chunk_size != 0:
            raise exceptions.IncompatibleGrouper(
                "Cannot apply edge {} to child layer {} because number of real child nodes ({}) is not evenly divisible by the child chunk size ({})".format(
                    self, child, len(child), self.child_chunk_size
                )
            )
        if (num_parent_vars // self.parent_chunk_size) != (
            num_child_vars // self.child_chunk_size
        ):
            raise exceptions.IncompatibleGrouper(
                "Cannot apply edge {} to layers {} and {} because they do not produce the same number of chunks (parent chunk: {} / {} = {}, child chunk: {} / {} = {})".format(
                    self,
                    parent,
                    child,
                    len(parent),
                    self.parent_chunk_size,
                    len(parent) // self.parent_chunk_size,
                    len(child),
                    self.child_chunk_size,
                    len(child) // self.child_chunk_size,
                )
            )

        for parent_group, child_group in zip(
            utils.grouper(range(num_parent_vars), self.parent_chunk_size),
            utils.grouper(range(num_child_vars), self.child_chunk_size),
        ):
            join = join_factory.get_join_node()
            yield parent_group, join
            yield join, child_group

    def __repr__(self) -> str:
        return utils.make_repr(self, ("parent_chunk_size", "child_chunk_size"))


class Slicer(BaseEdge):
    """
    This edge connects individual nodes in the layers, selected by slices.
    Each node from the parent layer that is in the parent slice is joined,
    one-to-one, with the matching node from the child layer that is in the child
    slice.
    """

    def __init__(
        self, parent_slice: slice = slice(None), child_slice: slice = slice(None)
    ):
        """
        Parameters
        ----------
        parent_slice
            The slice to use for the parent layer.
        child_slice
            The slice to use for the child layer.
        """
        self.parent_slice = parent_slice
        self.child_slice = child_slice

    def get_edges(
        self, parent: "node.BaseNode", child: "node.BaseNode", join_factory: JoinFactory
    ) -> Iterable[
        Union[
            Tuple[Tuple[int], Tuple[int]],
            Tuple[Tuple[int], JoinNode],
            Tuple[JoinNode, Tuple[int]],
        ]
    ]:
        num_parent_vars = len(parent)
        num_child_vars = len(child)

        for parent_index, child_index in zip(
            itertools.islice(
                range(num_parent_vars),
                self.parent_slice.start,
                self.parent_slice.stop,
                self.parent_slice.step,
            ),
            itertools.islice(
                range(num_child_vars),
                self.child_slice.start,
                self.child_slice.stop,
                self.child_slice.step,
            ),
        ):
            yield (parent_index,), (child_index,)

    def __repr__(self) -> str:
        return utils.make_repr(self, ("parent_slice", "child_slice"))
