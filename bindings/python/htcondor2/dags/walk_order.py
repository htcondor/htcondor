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


import enum


class WalkOrder(str, enum.Enum):
    """
    An enumeration for keeping track of which order to walk through a graph.
    Depth-first means that parents/children will be visited before siblings.
    Breadth-first means that siblings will be visited before parents/children.
    """

    DEPTH_FIRST = "DEPTH"
    BREADTH_FIRST = "BREADTH"

    def __repr__(self) -> str:
        return "{}.{}".format(type(self).__name__, str(self))
