from collections.abc import Mapping

import datetime

from ._common_imports import (
    classad,
)

from ._job_event_type import JobEventType

class JobEvent(Mapping):
    """
    A single event from a job event log.

    A :class:`JobEvent` is a :class:`Mapping` of event properties
    to event values; the type of the value depends on the property.
    The names of the properties and their types are currently
    undocumented.

    Because all events have the ``type``, ``cluster``, ``proc``,
    and ``timestamp`` properties, these are available as attributes;
    see below.
    """

    # It would be nice if this signature didn't appear in the docs,
    # since instantiating these objects shouldn't be in the API.
    # ... there's probably a standard Pythonic way of indicating that.
    def __init__(self, data : classad.ClassAd, event_text : str):
        self._data = data
        self._event_text = event_text


    @property
    def type(self) -> JobEventType:
        """
        The type of the event.
        """
        return JobEventType(self._data["EventTypeNumber"])


    @property
    def cluster(self) -> int:
        """
        The cluster ID of the job to which the event happened.
        """
        return self._data.get("cluster", -1)


    @property
    def proc(self) -> int:
        """
        The process ID of the job to which the event happened.  Returns
        ``-1`` if no process ID was recorded for the event.
        """
        # The event ClassAd doesn't contain a Proc attribute if the original
        # event's ProcID was less than 0.  ULogEvent's default
        # ("invalid") procID is -1, so we'll just assume that's good for now.
        #
        # If we ever need/want to expose other "invalid" ProcIDs, we can
        # adjust _job_event_log_next() to insert `proc` into the ClassAd
        # as well.
        return self._data.get("proc", -1)


    @property
    def timestamp(self) -> int:
        '''
        The Unix timestamp of the event.
        '''

        iso = self._data["EventTime"]

        # Python 3.6 doesn't have fromisoformat().  In additon,
        # its implementation of %z doesn't recognize 'Z' as UTC.
        format = "%Y-%m-%dT%H:%M:%S"

        if '.' in iso:
            format += ".%f"

        if iso.endswith("Z"):
            format += "Z"

        dt = datetime.datetime.strptime(iso ,format)

        if iso.endswith("Z"):
            dt.replace(tzinfo=datetime.timezone.utc)

        # We return an int here for backwards-compatibility with the
        # actual implementation (the documentation says `str`, but the
        # code never did that).  If this causes somebody grief, we can
        # easily just return the dt itself.
        return int(dt.timestamp())


    def __getitem__(self, key):
        return self._data[key]


    def __iter__(self):
        return iter(self._data)


    def __len__(self):
        return len(self._data)


    def __repr__(self):
        return f"JobEvent(type={self.type}, cluster={self.cluster}, proc={self.proc}, timestamp={self.timestamp})"


    def __str__(self):
        return self._event_text


