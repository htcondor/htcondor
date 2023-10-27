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

from typing import Optional, Callable, List, Iterator

import logging
import abc
import time
from pathlib import Path
import collections
import weakref

import htcondor2 as htcondor
import classad2 as classad

from . import jobs, exceptions, utils

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class Handle(abc.ABC):
    """
    A connection to a set of jobs defined by a constraint.
    The handle can be used to query, act on, or edit those jobs.
    """

    def __init__(self, condor):
        self.condor = condor

    @property
    def constraint_string(self) -> str:
        raise NotImplementedError

    def __repr__(self):
        return "{}(constraint = {})".format(type(self).__name__, self.constraint_string)

    def __eq__(self, other):
        return all(
            (
                isinstance(other, self.__class__),
                self.condor == other.condor,
                self.constraint_string == other.constraint_string,
            )
        )

    def __hash__(self):
        return hash((self.__class__, self.constraint_string, self.condor))

    @property
    def get_schedd(self):
        return self.condor.get_local_schedd()

    def query(self, projection=None, options=htcondor.QueryOpts.Default, limit=-1):
        """
        Query against this set of jobs.

        Parameters
        ----------
        projection
            The :class:`classad.ClassAd` attributes to retrieve, as a list of case-insensitive strings.
            If ``None`` (the default), all attributes will be returned.
        options
        limit
            The total number of matches to return from the query.
            If ``None`` (the default), return all matches.

        Returns
        -------
        ads : Iterator[:class:`classad.ClassAd`]
            An iterator over the :class:`classad.ClassAd` that match the constraint.
        """
        return self.condor.query(
            self.constraint_string, projection=projection, opts=options, limit=limit
        )

    def _act(self, action):
        return self.condor.act(action, self.constraint_string)

    def remove(self):
        """
        Remove jobs from the queue.

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Remove)

    def hold(self):
        """
        Hold jobs.

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Hold)

    def release(self):
        """
        Release held jobs.
        They will return to the queue in the idle state.

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Release)

    def pause(self):
        """
        Pause jobs.
        Jobs will stop running, but will hold on to their claimed resources.

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Suspend)

    def resume(self):
        """
        Resume (un-pause) jobs.

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Continue)

    def vacate(self):
        """
        Vacate running jobs.
        This will force them off of their current execute resource, causing them to become idle again.

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the action.
        """
        return self._act(htcondor.JobAction.Vacate)

    def edit(self, attr, value):
        """
        Edit attributes of jobs.

        .. warning::
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

        Returns
        -------
        ad : :class:`classad.ClassAd`
            An ad describing the results of the edit.
        """
        return self.condor.edit(self.constraint_string, attr, str(value))


class ConstraintHandle(Handle):
    """
    A connection to a set of jobs defined by an :attr:`ConstraintHandle.constraint`.
    The handle can be used to query, act on, or edit those jobs.
    """

    def __init__(self, condor, constraint):
        super().__init__(condor=condor)

        if isinstance(constraint, str):
            constraint = classad.ExprTree(constraint)
        self._constraint = constraint

    @property
    def constraint(self) -> classad.ExprTree:
        """
        The constraint that defines this :class:`ConstraintHandle`,
        as an :class:`classad.ExprTree`.
        """
        return self._constraint

    @property
    def constraint_string(self) -> str:
        """
        The constraint that defines this :class:`ConstraintHandle`,
        as a string.
        """
        return str(self.constraint)

    def __repr__(self):
        return "{}(constraint = {})".format(type(self).__name__, self.constraint)


class ClusterHandle(ConstraintHandle):
    """
    A subclass of :class:`ConstraintHandle` that targets a single cluster of jobs,
    as produced by :func:`Condor.submit`.

    Because this handle targets a single cluster of jobs, it has superpowers.
    If the cluster has an event log
    (``log = <path>`` in the submit description,
    see the `docs`_),
    this handle's ``state`` attribute will be a :class:`ClusterState` that provides
    information about the current state of the jobs in the cluster.

    .. warning ::

        You shouldn't have to construct a :class:`ClusterHandle` yourself.
        Instead, use the ones returned by :func:`Condor.submit`.

    .. _docs: https://htcondor.readthedocs.io/en/latest/man-pages/condor_submit.html
    """

    def __init__(self, condor, submit_result):
        self._clusterid = submit_result.cluster()
        self._clusterad = submit_result.clusterad()
        self._first_proc = submit_result.first_proc()
        self._num_procs = submit_result.num_procs()

        super().__init__(
            condor=condor,
            constraint=classad.ExprTree("ClusterID == {}".format(self.clusterid)),
        )

        # must delay this until after init, because at this point the submit
        # transaction may not be done yet
        self._state = None
        self._event_log = None

    def __int__(self):
        return self.clusterid

    def __repr__(self):
        batch_name = self.clusterad.get("JobBatchName", None)
        batch = (
            ", JobBatchName = {}".format(batch_name) if batch_name is not None else ""
        )
        return "{}(ClusterID = {}{})".format(type(self).__name__, self.clusterid, batch)

    @property
    def clusterid(self):
        """The cluster's cluster ID."""
        return self._clusterid

    @property
    def clusterad(self):
        """The cluster's cluster ad."""
        return self._clusterad

    @property
    def first_proc(self):
        """The process ID of the first job in the cluster."""
        return self._first_proc

    @property
    def num_procs(self):
        """The number of jobs in the cluster."""
        return self._num_procs

    def __len__(self):
        return self.num_procs

    @property
    def job_ids(self) -> List[jobs.JobID]:
        """Return the list of :class:`JobID` in this :class:`ClusterHandle`."""
        return [jobs.JobID(self.clusterid, proc) for proc in range(len(self))]

    @property
    def state(self):
        """A :class:`ClusterState` that provides information about job state for this cluster."""
        if self._state is None:
            self._state = ClusterState(self)

        return self._state

    @property
    def event_log(self):
        """The :class:`EventLog` for this :class:`ClusterHandle`."""
        if self._event_log is None:
            self._event_log = EventLog(self)
        return self._event_log

    def wait(
        self,
        condition: Optional[Callable[["ClusterState"], bool]] = None,
        fail_condition: Optional[Callable[["ClusterState"], bool]] = None,
        timeout: int = 120,
        verbose: bool = False,
    ) -> bool:
        """
        Waits for the ``condition`` to become ``True``.

        Parameters
        ----------
        condition
            The function to wait to become ``True``. It will be passed the
            :class:`ClusterState` as its only argument.
            Because of how Python calls unbound class methods, you may directly
            pass :class:`ClusterState` methods as conditions
            (e.g., ``handle.wait(condition = ClusterState.any_held)``).
            The default condition is :meth:`ClusterState.all_complete`, which
            means "wait until all the jobs in this cluster are completed".
        fail_condition
            If this function becomes ``True``, ``wait`` will immediately
            return ``False``. Use this to avoid waiting for a long time when
            a test is failing.
        timeout
            After this amount of time, ``wait`` will return ``False`` and emit
            a warning in the log.
        verbose
            If ``True``, the handle's state counts will be logged during the wait.

        Returns
        -------
        success : bool
            ``True`` if the wait finished because the condition became ``True``;
            ``False`` otherwise.
        """
        if condition is None:
            condition = ClusterState.all_complete
        if fail_condition is None:
            fail_condition = lambda _: False

        start_time = time.time()
        num_events_read = self.state.read_events()
        while True:
            if verbose:
                logger.debug("Handle {} state: {}".format(self, self.state.counts()))

            if condition(self.state):
                break

            if fail_condition(self.state):
                logger.warning(
                    "Wait for handle {} triggered its failure condition".format(self)
                )
                return False

            if timeout is not None and time.time() > start_time + timeout:
                logger.warning("Wait for handle {} timed out".format(self))
                return False

            # Sleep a second here if no job log events were waiting for us to prevent
            # busy waiting.  However, if we did see an event, try to read another
            # event as often they come in bunches - and we want to consume them
            # as rapidly as possible.
            if num_events_read == 0:
                time.sleep(1)

            num_events_read = self.state.read_events()

        logger.debug("Wait for handle {} finished successfully".format(self))
        return True


class _MockSubmitResult:
    """
    This class is used purely to transform unpacked submit results back into
    "submit results" to accommodate the :class:`ClusterHandle` constructor.
    **Should not be used in user code.**
    """

    def __init__(self, clusterid, clusterad, first_proc, num_procs):
        self._clusterid = clusterid
        self._clusterad = clusterad
        self._first_proc = first_proc
        self._num_procs = num_procs

    def cluster(self):
        return self._clusterid

    def clusterad(self):
        return self._clusterad

    def first_proc(self):
        return self._first_proc

    def num_procs(self):
        return self._num_procs


JOB_EVENT_STATUS_TRANSITIONS = {
    htcondor.JobEventType.SUBMIT: jobs.JobStatus.IDLE,
    htcondor.JobEventType.JOB_EVICTED: jobs.JobStatus.IDLE,
    htcondor.JobEventType.JOB_UNSUSPENDED: jobs.JobStatus.IDLE,
    htcondor.JobEventType.JOB_RELEASED: jobs.JobStatus.IDLE,
    htcondor.JobEventType.SHADOW_EXCEPTION: jobs.JobStatus.IDLE,
    htcondor.JobEventType.JOB_RECONNECT_FAILED: jobs.JobStatus.IDLE,
    htcondor.JobEventType.JOB_TERMINATED: jobs.JobStatus.COMPLETED,
    htcondor.JobEventType.EXECUTE: jobs.JobStatus.RUNNING,
    htcondor.JobEventType.JOB_HELD: jobs.JobStatus.HELD,
    htcondor.JobEventType.JOB_SUSPENDED: jobs.JobStatus.SUSPENDED,
    htcondor.JobEventType.JOB_ABORTED: jobs.JobStatus.REMOVED,
}

NO_EVENT_LOG = object()


class ClusterState:
    """
    A class that manages the state of the cluster tracked by a :class:`ClusterHandle`.
    It reads from the cluster's event log internally and provides a variety of views
    of the individual job states.

    .. warning::

        :class:`ClusterState` objects should not be instantiated manually.
        :class:`ClusterHandle` will create them automatically when needed.
    """

    def __init__(self, handle):
        self._handle = weakref.proxy(handle)
        self._clusterid = handle.clusterid
        self._offset = handle.first_proc

        self._data = self._make_initial_data(handle)
        self._counts = collections.Counter(jobs.JobStatus(js) for js in self._data)

        self._last_event_read = -1

    def _make_initial_data(self, handle):
        return [jobs.JobStatus.UNMATERIALIZED for _ in range(len(handle))]

    def read_events(self):
        # TODO: this reacharound through the handle is bad
        # trigger a read...
        list(self._handle.event_log.read_events())

        # ... but actually look through everything we haven't read yet
        # in case someone else has read elsewhere
        num_events_read = 0
        for event in self._handle.event_log.events[self._last_event_read + 1 :]:
            self._last_event_read += 1
            num_events_read += 1

            new_status = JOB_EVENT_STATUS_TRANSITIONS.get(event.type, None)
            if new_status is not None:
                key = event.proc - self._offset

                # update counts
                old_status = self._data[key]
                self._counts[old_status] -= 1
                self._counts[new_status] += 1

                # set new status on individual job
                self._data[key] = new_status

                # break here to avoid race conditions where a test may be waiting
                # for a status that is very temporary and thus the wait condition never fires.
                break
        return num_events_read

    def __getitem__(self, proc):
        if isinstance(proc, int):
            return self._data[proc - self._offset]
        elif isinstance(proc, slice):
            start, stop, stride = proc.indices(len(self))
            return self._data[start - self._offset : stop - self._offset : stride]

    def counts(self):
        """
        Return the number of jobs in each :class:`JobStatus`, as a :class:`collections.Counter`.
        """
        return self._counts.copy()

    @property
    def by_name(self):
        states = collections.defaultdict(list)
        for p, s in enumerate(self._data):
            states[s].append(jobs.JobID(self._clusterid, p + self._offset))
        return states

    def __iter__(self):
        yield from self._data

    def __str__(self):
        return str(self._data)

    def __repr__(self):
        return repr(self._data)

    def __len__(self):
        return len(self._data)

    def __eq__(self, other):
        return isinstance(other, self.__class__) and self._handle == other._handle

    def all_complete(self) -> bool:
        """
        Return ``True`` if **all** of the jobs in the cluster are complete.
        Note that this definition does include jobs that have left the queue,
        not just ones that are in the "Completed" state in the queue.
        """
        return self.all_status(jobs.JobStatus.COMPLETED)

    def any_complete(self) -> bool:
        """
        Return ``True`` if **any** of the jobs in the cluster are complete.
        Note that this definition does include jobs that have left the queue,
        not just ones that are in the "Completed" state in the queue.
        """
        return self.any_status(jobs.JobStatus.COMPLETED)

    def any_idle(self) -> bool:
        """Return ``True`` if **any** of the jobs in the cluster are idle."""
        return self.any_status(jobs.JobStatus.IDLE)

    def none_idle(self) -> bool:
        """Return ``True`` if **none** of the jobs in the cluster are idle."""
        return self.none_status(jobs.JobStatus.IDLE)

    @staticmethod
    def running_exactly(count) -> bool:
        """Returns ``True`` if **count** of the jobs in the cluster are running."""
        return lambda self: self.status_exactly(count, jobs.JobStatus.RUNNING)

    def any_running(self) -> bool:
        """Return ``True`` if **any** of the jobs in the cluster are running."""
        return self.any_status(jobs.JobStatus.RUNNING)

    def all_running(self) -> bool:
        """Return ``True`` if **all** of the jobs in the cluster are running."""
        return self.all_status(jobs.JobStatus.RUNNING)

    def all_held(self) -> bool:
        """Return ``True`` if **all** of the jobs in the cluster are held."""
        return self.all_status(jobs.JobStatus.HELD)

    def all_idle(self) -> bool:
        """Return ``True`` if **all** of the jobs in the cluster are held."""
        return self.all_status(jobs.JobStatus.IDLE)

    def any_held(self) -> bool:
        """Return ``True`` if **any** of the jobs in the cluster are held."""
        return self.any_status(jobs.JobStatus.HELD)

    def none_held(self) -> bool:
        """Return ``True`` if **none** of the jobs in the cluster are held."""
        return self.none_status(jobs.JobStatus.HELD)

    def all_terminal(self) -> bool:
        """Return ``True`` if **all** of the jobs in the cluster are completed, held, or removed."""
        return self.all_status(
            jobs.JobStatus.COMPLETED, jobs.JobStatus.HELD, jobs.JobStatus.REMOVED
        )

    def any_terminal(self) -> bool:
        """Return ``True`` if **any** of the jobs in the cluster are completed, held, or removed."""
        return self.any_status(
            jobs.JobStatus.COMPLETED, jobs.JobStatus.HELD, jobs.JobStatus.REMOVED
        )

    def status_exactly(self, count, *statuses: jobs.JobStatus) -> bool:
        """
        Return ``True`` if **exactly** ``count`` of the jobs in the cluster are
        in one of the ``statuses``.  Prefer one of the explicitly-named helper
        methods when possible, and don't be afraid to make a new helper method!
        """
        return self.count_status(*statuses) == count

    def all_status(self, *statuses: jobs.JobStatus) -> bool:
        """
        Return ``True`` if **all** of the jobs in the cluster are in one of the ``statuses``.
        Prefer one of the explicitly-named helper methods when possible,
        and don't be afraid to make a new helper method!
        """
        return self.count_status(*statuses) == len(self)

    def any_status(self, *statuses: jobs.JobStatus) -> bool:
        """
        Return ``True`` if **any** of the jobs in the cluster are in one of the ``statuses``.
        Prefer one of the explicitly-named helper methods when possible,
        and don't be afraid to make a new helper method!
        """
        return self.count_status(*statuses) > 0

    def none_status(self, *statuses: jobs.JobStatus) -> bool:
        """
        Return ``True`` if **none** of the jobs in the cluster are in one of the ``statuses``.
        Prefer one of the explicitly-named helper methods when possible,
        and don't be afraid to make a new helper method!
        """
        return self.count_status(*statuses) == 0

    def count_status(self, *statuses: jobs.JobStatus) -> int:
        """Return the total number of jobs in the cluster in any of the given statuses."""
        counts = self.counts()
        return sum(counts[status] for status in statuses)


class EventLog:
    """
    This class represents the job event log for a :class:`ClusterHandle`.

    .. warning ::

        You shouldn't have to construct this yourself.
        Instead, use :attr:`ClusterHandle.event_log`.
    """

    def __init__(self, handle: ClusterHandle):
        self._handle = handle
        self._clusterid = handle.clusterid
        raw_event_log_path = utils.chain_get(
            handle.clusterad, ("UserLog", "DAGManNodesLog"), default=NO_EVENT_LOG
        )
        if raw_event_log_path is NO_EVENT_LOG:
            raise exceptions.NoJobEventLog(
                "Cluster for handle {} does not have a job event log, so it cannot track job state".format(
                    self._handle
                )
            )
        self._event_log_path = Path(raw_event_log_path).absolute()

        self._event_reader = None
        self.events = []

    def read_events(self) -> Iterator[htcondor.JobEvent]:
        """Yield all un-read events in the event log."""
        if self._event_reader is None:
            self._event_reader = htcondor.JobEventLog(
                self._event_log_path.as_posix()
            ).events(0)

        for event in self._event_reader:
            if event.cluster != self._clusterid:
                continue

            self.events.append(event)
            yield event

    def filter(
        self, condition: Callable[[htcondor.JobEvent], bool]
    ) -> List[htcondor.JobEvent]:
        """
        Return a list containing the job events that the condition is ``True`` for.
        """
        return [e for e in self.events if condition(e)]
