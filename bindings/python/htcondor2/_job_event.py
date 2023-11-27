from collections.abc import Mapping

import datetime

from ._common_imports import (
    classad,
)

from ._job_event_type import JobEventType

class JobEvent(Mapping):

    def __init__(self, data : classad.ClassAd, event_text : str):
        self._data = data
        self._event_text = event_text


    @property
    def type(self) -> JobEventType:
        return JobEventType(self._data["EventTypeNumber"])


    @property
    def cluster(self) -> int:
        return self._data["cluster"]


    @property
    def proc(self) -> int:
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
        dt = datetime.datetime.fromisoformat(iso)
        # We return an int here for backwards-compatibility with the
        # actual implementation (the documentation says `str`, but the
        # code never did that).  If this causes somebody grief, we can
        # easily just return the dt itself; note that the `EventTime`
        # may be in UTC and/or include sub-second timing, if those
        # format options were turned on in config when the log was written.
        # (A datetime.datetime object return from fromisoformat() should
        # be "naive" -- see the documentation -- if the string did not
        # include a timezone, and otherwise set the timezone correctly,
        # which is exactly the behavior Python programmers will be expecting.)
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


