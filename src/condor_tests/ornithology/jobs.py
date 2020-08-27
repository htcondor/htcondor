# Copyright 2019 HTCondor Team, Computer Sciences Department,
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
import enum
import functools

import htcondor
import classad

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class JobID:
    """
    A class that encapsulates a (cluster ID, proc ID) pair.
    """

    def __init__(self, cluster, proc):
        """
        Parameters
        ----------
        cluster
            The cluster ID for the job.
        proc
            The process ID for the job.
        """
        self.cluster = int(cluster)
        self.proc = int(proc)

    @classmethod
    def from_job_event(cls, job_event: htcondor.JobEvent) -> "JobID":
        """Get the :class:`JobID` of a :class:`htcondor.JobEvent`."""
        return cls(job_event.cluster, job_event.proc)

    @classmethod
    def from_job_ad(cls, job_ad: classad.ClassAd) -> "JobID":
        """Get the :class:`JobID` of a job ad."""
        return cls(job_ad["ClusterID"], job_ad["ProcID"])

    def id_for_cluster_ad(self) -> "JobID":
        """Return the :class:`JobID` for the cluster ad of the cluster this :class:`JobID` belongs to."""
        return type(self)(cluster=self.cluster, proc=-1)

    def __eq__(self, other):
        return (
            (isinstance(other, self.__class__) or isinstance(self, other.__class__))
            and self.cluster == other.cluster
            and self.proc == other.proc
        )

    def __hash__(self):
        return hash((self.__class__, self.cluster, self.proc))

    def __repr__(self):
        return "{}(cluster = {}, proc = {})".format(
            self.__class__.__name__, self.cluster, self.proc
        )

    def __str__(self):
        return "{}.{}".format(self.cluster, self.proc)

    def __ge__(self, other):
        return (self.cluster, self.proc) >= (other.cluster, other.proc)

    def __gt__(self, other):
        return (self.cluster, self.proc) > (other.cluster, other.proc)

    def __le__(self, other):
        return (self.cluster, self.proc) <= (other.cluster, other.proc)

    def __lt__(self, other):
        return (self.cluster, self.proc) < (other.cluster, other.proc)


class JobStatus(str, enum.Enum):
    """
    An enumeration of the HTCondor job states.

    .. warning ::

        ``UNMATERIALIZED`` is not a real job state! It is used as the initial
        state for jobs in some places, but will never show up in (for example)
        the job queue log.
    """

    IDLE = "1"
    RUNNING = "2"
    REMOVED = "3"
    COMPLETED = "4"
    HELD = "5"
    TRANSFERRING_OUTPUT = "6"
    SUSPENDED = "7"
    UNMATERIALIZED = "100"

    def __repr__(self):
        return "{}.{}".format(self.__class__.__name__, self.name)

    __str__ = __repr__
