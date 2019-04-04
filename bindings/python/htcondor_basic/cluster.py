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

import htcondor

from . import exceptions

logger = logging.getLogger(__name__)


class _QueryActEdit(object):
    """
    A mixin class that provides Query/Act/Edit methods that target a certain
    set of constraints.
    The constraints are provided by the hook method ``_constraint``, which should
    be overridden in the mixed class to provide an HTCondor constraint string
    that targets only the desired jobs.
    """

    def _constraint(self, constraint=None):
        """
        Produce a constraint which targets this set of jobs,
        in addition to whatever other constraints are given.
        """
        raise NotImplementedError

    def query(self, constraint=None, projection=None, limit=-1):
        """
        Query against this set of jobs.

        Parameters
        ----------
        constraint: str
            An HTCondor constraint, which will be combined with the constraint
            for this set of jobs to limit which jobs are affected.
        projection: list[str]
            The :class:`classad.ClassAd` attributes to retrieve, as a list of case-insensitive strings.
            If ``None``, all attributes will be returned.
        limit: int
            The total number of matches to return from the query.
            If ``-1`` (the default), return all matches.

        Returns
        -------
        ads :
            An iterator over the :class:`classad.ClassAd` that match the constraint.
        """
        if projection is None:
            projection = []
        constraint = self._constraint(constraint)

        return htcondor.Schedd().xquery(constraint, projection=projection, limit=limit)

    def _act(self, action, constraint=None):
        req = self._constraint(constraint)
        act_result = htcondor.Schedd().act(action, req)

        return act_result

    def remove(self, constraint=None):
        """
        Remove jobs from the queue.

        Parameters
        ----------
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Remove, constraint=constraint)

    def hold(self, constraint=None):
        """
        Hold jobs.

        Parameters
        ----------
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Hold, constraint=constraint)

    def release(self, constraint=None):
        """
        Release held jobs.
        They will return to the queue in the idle state.

        Parameters
        ----------
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Release, constraint=constraint)

    def pause(self, constraint=None):
        """
        Pause jobs.
        Jobs will stop running, but will hold on to their claimed resources.

        Parameters
        ----------
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Suspend, constraint=constraint)

    def resume(self, constraint=None):
        """
        Resume (un-pause) jobs.

        Parameters
        ----------
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Continue, constraint=constraint)

    def vacate(self, constraint=None):
        """
        Vacate running jobs.
        This will force them off of their current execute resource, causing them to become idle again.

        Parameters
        ----------
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Vacate, constraint=constraint)

    def edit(self, attr, value, constraint=None):
        """
        Edit attributes of jobs.

        .. warning ::

            Many attribute edits will not affect jobs that have already matched.
            For example, changing ``RequestMemory`` will not affect the memory allocation
            of a job that is already executing.
            In that case, you would need to vacate (or release the job if it was held)
            before the edit had the desired effect.

        Parameters
        ----------
        attr
            The attribute to edit. Case-insensitive.
        value
            The new value for the attribute.
        constraint
            An HTCondor constraint, which determines which subset of the jobs
            connected to this handle are affected.
            If ``None``, all jobs connected to this handle will be affected.

        Returns
        -------
        ad : classad.ClassAd
            An ad describing the results of the edit.
        """
        return htcondor.Schedd().edit(self._constraint(constraint), attr, value)


class Cluster(_QueryActEdit):
    """
    A handle to a cluster of jobs (a group of jobs which share a ``ClusterId``).
    The ``Cluster`` object lets you perform queries on, change the state of, and edit the attributes of
    the jobs in the cluster.

    Attributes
    ----------
    id : int
        The ``ClusterId`` for this cluster of jobs.
    ad : classad.ClassAd
        The ClusterAd for this cluster of jobs, if available.
        It will not be available if all of the cluster's jobs have left the queue and the pool's history.
    """

    def __init__(self, id, cluster_ad=None):
        """
        Parameters
        ----------
        id : int
            The ``ClusterId`` for this handle.
        cluster_ad : classad.ClassAd
            The ClusterAd for the cluster.
            If ``None``, we will attempt to find the cluster's ad on the schedd.
            If that fails, we attempt to find a JobAd from this cluster in the
            schedd's history and and assume that it is roughly similar to the ClusterAd.
            if that fails, we give up, and certain methods of this handle will not work.
        """
        self.id = id

        if cluster_ad is None:
            # try to get the clusterad from the schedd
            schedd = htcondor.Schedd()
            try:
                cluster_ad = schedd.query(
                    self._constraint(), opts=htcondor.QueryOpts(0x10), limit=1
                )[0]
            except IndexError:
                # try to get a jobad from the schedd's history
                try:
                    cluster_ad = tuple(schedd.history(self._constraint(), [], 1))[0]
                except IndexError:
                    cluster_ad = None
        self._ad = cluster_ad

    def __hash__(self):
        return hash((self.__class__, self.id))

    def __eq__(self, other):
        """:class:`Cluster` are equal if they have the same ``id``."""
        return all((isinstance(self, other), self.id == other.id))

    def __int__(self):
        return self.id

    def _constraint(self, constraint=None):
        """Produce a constraint which targets this cluster, any maybe even less."""
        base = "ClusterId=={cid}".format(cid=self.id)
        extra = (
            " && ({constraint})".format(constraint=constraint)
            if constraint is not None
            else ""
        )

        return base + extra

    @property
    def ad(self):
        """The ClusterAd for this cluster of jobs."""
        if self._ad is not None:
            return self._ad

        raise ValueError("{self} does not have a clusterad".format(self=self))

    def __str__(self):
        return "<{cn}(id={id})>".format(cn=type(self).__name__, id=self.id)

    def __format__(self, format_spec):
        if format_spec == "d":
            return str(self.id)

        return super(Cluster, self).__format__(format_spec)

    def events(self, timeout=0):
        """
        Returns an iterator over the event log for this cluster of jobs.
        For this to behave as expected, the entire cluster must share an event log.
        That is, the value of the ``log`` key in :class:`Submit` must be set to a constant.

        The iterator is re-entrant: you can read all of the events out of it,
        do something else, then iterate over the iterator again and read any new
        events that have been added since the last time it was read.

        Parameters
        ----------
        timeout
            The amount of time (in seconds) to wait for new events before giving up.
            If zero, returns immediately when it reaches the end of the event log.
            If negative, wait forever.

        Returns
        -------
        events :
            An iterator of :class:`JobEvent` for this cluster of jobs.
        """
        try:
            log_path = self.ad["UserLog"]
        except KeyError:
            raise exceptions.NoEventLog('This cluster of jobs does not have an event log; specify "log = <path>" in Submit to get an event log')

        return htcondor.JobEventLog(log_path).events(timeout)
