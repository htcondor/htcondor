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

from typing import Any, Iterator, List, Optional, Tuple, Union, Mapping, Iterable

import logging

import time
import collections

from . import condor, jobs

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class SetAttribute:
    def __init__(self, attribute: str, value: Optional[str]):
        self.attribute = attribute
        self.value = value

    def __eq__(self, other):
        return (
            (isinstance(other, self.__class__) or isinstance(self, other.__class__))
            and self.attribute == other.attribute
            and self.value == other.value
        )

    def __hash__(self):
        return hash((SetAttribute, self.attribute, self.value))

    def matches(self, other):
        return self == other or (
            (
                isinstance(other, self.__class__ or isinstance(self, other.__class__))
                and (
                    (
                        (self.attribute is None or other.attribute is None)
                        and self.value == other.value
                    )
                    or (
                        self.attribute == other.attribute
                        and (self.value is None or other.value is None)
                    )
                )
            )
        )

    def _fmt_value(self):
        """Some values can have special formatting depending on the attribute."""
        if self.attribute in ("JobStatus", "LastJobStatus"):
            return str(jobs.JobStatus(self.value))

        return str(self.value)

    def __repr__(self):
        return "{}(attribute = {}, value = {})".format(
            self.__class__.__name__, self.attribute, self._fmt_value()
        )

    def __str__(self):
        return "{}({} = {})".format(
            type(self).__name__, self.attribute, self._fmt_value()
        )


class SetJobStatus(SetAttribute):
    """A subclass of :class:`SetAttribute` specialized for a job status change."""

    def __init__(self, new_status: jobs.JobStatus):
        super().__init__(attribute="JobStatus", value=new_status)


EXPECTED_EVENTS = Mapping[jobs.JobID, Iterable[SetAttribute]]


class JobQueue:
    def __init__(self, condor: "condor.Condor"):
        self.condor = condor

        self.transactions = []
        self.by_jobid = collections.defaultdict(list)

        self._job_queue_log_file = None

    def __iter__(self):
        yield from self.transactions

    def events(self):
        """
        Yield a flat stream of events from the job queue log
        (ignoring transaction structure).
        """
        for transaction in self.transactions:
            yield from transaction

    def filter(self, condition):
        """
        Yield a flat stream of events on the job queue log that satisfy
        some condition
        (ignoring transaction structure).

        Parameters
        ----------
        condition
        """
        yield from ((j, e) for j, e in self.events() if condition(j, e))

    def read_transactions(self) -> Iterator[List[Tuple[jobs.JobID, SetAttribute]]]:
        """Yield transactions (i.e., lists) of (jobid, event) pairs from the job queue log."""
        if self._job_queue_log_file is None:
            self._job_queue_log_file = self.condor.job_queue_log.open(mode="r")

        transaction_accumulator = []
        for line in buffered_readlines(self._job_queue_log_file):
            jobid, event = parse_job_queue_log_line(line)

            if event is START_TRANSACTION:
                transaction_accumulator = []
            elif event is END_TRANSACTION:
                t = JobQueueTransaction(transaction_accumulator)
                self.transactions.append(t)
                yield t
            elif isinstance(jobid, jobs.JobID) and isinstance(event, SetAttribute):
                self.by_jobid[jobid].append(event)
                transaction_accumulator.append((jobid, event))

    def wait_for_events(
        self,
        expected_events: EXPECTED_EVENTS,
        unexpected_events: Optional[EXPECTED_EVENTS] = None,
        timeout: int = 120,
    ):
        """
        Wait for job queue events to occur.

        This method is primarily intended for test setup; see :func:`in_order`
        for a way to assert that job queue events occurred in a certain
        order post-facto.

        All this method cares about is what the *next* event is for a particular jobid.
        If events come out of order, it will not record the out-of-order ones!

        This method never raises an exception intentionally, even when it times out.
        It simply returns control to the test, so that the test itself can
        declare failure. Log messages are recorded with detailed information
        in either case.


        Parameters
        ----------
        expected_events
        unexpected_events
        timeout

        Returns
        -------
        all_good : bool
            ``True`` is all events occurred and no unexpected events occurred.
            ``False`` if it timed out, or if any unexpected events occurred.
        """
        all_good = True

        if unexpected_events is None:
            unexpected_events = {}

        unexpected_events = {
            jobid: set(events) for jobid, events in unexpected_events.items()
        }

        expected_events = {
            jobid: collections.deque(
                event if isinstance(event, tuple) else (event, lambda *_: None)
                for event in events
            )
            for jobid, events in expected_events.items()
        }

        jobids = set(expected_events.keys())

        start = time.time()

        while True:
            elapsed = time.time() - start
            if elapsed > timeout:
                logger.error("Job queue event wait ending due to timeout!")
                return False

            for transaction in self.read_transactions():
                for jobid, event in transaction:
                    if jobid not in jobids:
                        continue

                    if event in unexpected_events.get(jobid, ()):
                        logger.error(
                            "Saw unexpected job queue event for job {}: {} (was expecting {})".format(
                                jobid, event, expected_events[jobid][0]
                            )
                        )
                        all_good = False
                        continue

                    expected_events_for_jobid = expected_events[jobid]
                    if len(expected_events_for_jobid) == 0:
                        continue

                    next_event, callback = expected_events_for_jobid[0]

                    if not event.matches(next_event):
                        continue

                    logger.debug(
                        "Saw expected job queue event for job {}: {}".format(
                            jobid, event
                        )
                    )
                    expected_events_for_jobid.popleft()
                    callback(jobid, event)

                    if len(expected_events_for_jobid) == 0:
                        logger.debug(
                            "Have seen all expected job queue events for job {}".format(
                                jobid
                            )
                        )
                    else:
                        logger.debug(
                            "Still expecting job queue events for job {}: [{}]".format(
                                jobid,
                                ", ".join(str(e) for e, _ in expected_events_for_jobid),
                            )
                        )

                # if no more expected event, we're done!
                if all(len(events) == 0 for events in expected_events.values()):
                    logger.debug(
                        "Job queue event wait exiting with goodness: {}".format(
                            all_good
                        )
                    )
                    return all_good

            time.sleep(0.1)

    def wait_for_job_completion(self, job_ids, timeout=120) -> bool:
        """
        Wait for a set of job ids to reach the completed state.
        If any of the jobs reach a non-complete terminal state
        (held, removed, etc.)
        this function will immediately return ``False``.

        Parameters
        ----------
        job_ids
            The job ids to wait for. You may find :attr:`ClusterHandle.job_ids`
            useful.
        timeout

        Returns
        -------
        all_good : bool
            ``True`` is all events occurred and no unexpected events occurred.
            ``False`` if it timed out, or if any unexpected events occurred.
        """
        job_ids = list(job_ids)
        return self.wait_for_events(
            expected_events={
                job_id: [SetJobStatus(jobs.JobStatus.COMPLETED)] for job_id in job_ids
            },
            unexpected_events={
                job_id: {
                    SetJobStatus(jobs.JobStatus.HELD),
                    SetJobStatus(jobs.JobStatus.SUSPENDED),
                    SetJobStatus(jobs.JobStatus.REMOVED),
                }
                for job_id in job_ids
            },
            timeout=timeout,
        )


START_TRANSACTION = object()
END_TRANSACTION = object()


def buffered_readlines(file) -> Iterator[str]:
    """
    Because of the buffering semantics of the job queue log, we need to
    do things more manually that we would like. The problem is that the
    job queue log doesn't flush in the middle of a transaction, making
    it possible to get partial lines if we read in the middle of a
    transaction. Thus, we use a simple line-oriented buffer to avoid that.
    """
    buffer = ""
    while True:
        line = file.readline()

        # If the buffer and line are both empty, this is the end of the file.
        if buffer == "" and line == "":
            break

        buffer += line

        # Oops! We read only part of a line.
        # Wait a moment, then try again to see if we can get the full line.
        if not buffer.endswith("\n"):
            time.sleep(0.0001)
            continue

        yield buffer
        buffer = ""


def parse_job_queue_log_line(
    line: str,
) -> Tuple[Optional[jobs.JobID], Optional[Union[SetAttribute, Any]]]:
    parts = line.strip().split(" ", 3)

    event_type = parts[0]
    if event_type == "103":
        return jobs.JobID(*parts[1].split(".")), SetAttribute(parts[2], parts[3])
    elif event_type == "105":
        return None, START_TRANSACTION
    elif event_type == "106":
        return None, END_TRANSACTION

    return None, None


class JobQueueTransaction:
    def __init__(self, events):
        self.events = events

    def __iter__(self):
        yield from self.events

    def __len__(self):
        return len(self.events)

    def __getitem__(self, item):
        return self.events[item]

    def __contains__(self, item):
        expected_j, expected_e = item
        return any(expected_j == j and expected_e.matches(e) for j, e in self)

    def as_dict(self):
        d = collections.defaultdict(dict)

        for j, e in self:
            d[j][e.attribute] = e.value

        return dict(d)

    def __repr__(self):
        return "{}{}".format(self.__class__.__name__, repr(self.events))
