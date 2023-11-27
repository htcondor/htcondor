from collections.abc import Mapping

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
    def timestamp(self) -> str:
        iso8601 = self._data["EventTime"]
        return datetime.datetime.fromisoformat(iso8601)


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


