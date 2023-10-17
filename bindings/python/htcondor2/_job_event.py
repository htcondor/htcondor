from collections.abc import Mapping

from ._common_imports import (
    classad,
)

from ._job_event_type import JobEventType

class JobEvent(Mapping):

    def __init__(self, data : classad, event_text : str):
        self._data = data
        self._event_text = event_text


    @property
    def type(self) -> JobEventType:
        return JobEventType(self._data["type"])


    @property
    def cluster(self) -> int:
        return self._data["cluster"]


    @property
    def proc(self) -> int:
        return self._data["proc"]


    @property
    def timestamp(self) -> str:
        return self._data["timestamp"]


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


