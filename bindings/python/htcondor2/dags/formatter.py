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

from typing import Tuple

import abc

from . import exceptions

DEFAULT_SEPARATOR = ":"


class NodeNameFormatter(abc.ABC):
    """
    An abstract base class that represents a certain way of formatting and
    parsing underlying node names.
    """

    @abc.abstractmethod
    def generate(self, layer_name: str, node_index: int) -> str:
        """
        This method should generate a single node name,
        given the name of the layer and the index of the underlying node
        inside the layer.
        """
        raise NotImplementedError

    @abc.abstractmethod
    def parse(self, node_name: str) -> Tuple[str, int]:
        """
        This method should convert a single node name back into a layer name
        and underlying node index.
        Node names must be invertible for :func:`rescue` to work.
        """
        raise NotImplementedError


class SimpleFormatter(NodeNameFormatter):
    """
    A no-frills :class:`NodeNameFormatter`
    that produces underlying node names like ``LayerName-5``.
    """

    def __init__(
        self, separator=DEFAULT_SEPARATOR, index_format="{:d}", offset: int = 0
    ):
        self.separator = separator
        self.index_format = index_format
        self.offset = offset

    def generate(self, layer_name: str, node_index: int) -> str:
        if self.separator in layer_name:
            raise exceptions.LayerNameContainsSeparator(
                "The layer name {} cannot contain the node name separator character '{}'".format(
                    layer_name, self.separator
                )
            )
        name = "{}{}{}".format(
            layer_name,
            self.separator,
            self.index_format.format(node_index + self.offset),
        )

        if self.parse(name) != (layer_name, node_index):
            raise exceptions.CannotInvertFormat(
                "{} was not able to invert the formatted node name {}. Perhaps the index_format is incompatible?".format(
                    type(self).__name__, name
                )
            )

        return name

    def parse(self, node_name: str) -> Tuple[str, int]:
        layer, index = node_name.split(self.separator)
        try:
            index = int(index)
        except ValueError:
            raise exceptions.CannotInvertFormat(
                "{} was not able to invert the formatted node name {}. Perhaps the index_format is incompatible?".format(
                    type(self).__name__, node_name
                )
            )
        return layer, index - self.offset
