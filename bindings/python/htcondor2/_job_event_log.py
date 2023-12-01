import time

from .htcondor2_impl import _handle as handle_t

from ._job_event import JobEvent

from .htcondor2_impl import (
    _job_event_log_init,
    _job_event_log_next,
    _job_event_log_close,
)

class JobEventLog():

    def __init__(self, filename : str):
        self._deadline = 0
        self._filename = filename
        self._handle = handle_t()

        if not isinstance(filename, str):
            raise TypeError("filename must be a string")
        if len(filename) == 0:
            raise ValueError("filename must not be empty")
        _job_event_log_init(self, self._handle, filename)


    def events(self, stop_after : int = None) -> 'JobEventLog':
        if stop_after is None:
            self._deadline = 0
        elif isinstance(stop_after, int):
            self._deadline = int(time.time()) + stop_after
        else:
            # This was HTCondorTypeError in version 1.
            raise TypeError("deadline must be an integer")

        return self


    def close(self) -> None:
        return _job_event_log_close(self, self._handle)


    def __iter__(self) -> 'JobEventLog':
        return self


    def __next__(self) -> JobEvent:
        event_text, data =  _job_event_log_next(self, self._handle, self._deadline)
        return JobEvent(data, event_text)


    def __enter__(self):
        self._deadline = 0
        return self


    def __exit__(self, exceptionType, exceptionValue, traceback):
        self.close()
        return False


    # Pickling.
    def __getnewargs__(self):
        # We could go into C and ask the wful for its filename,
        # but it seems like knowing the filename might be useful
        # for other purposes.
        return (self._filename,)


    def __getstate__(self):
        offset = _job_event_log_get_offset(self, self._handle)
        return (self.__dict__, self._deadline, offset_)


    def __setstate__(self, t):
        self.__dict__ = t[0]
        self._deadlines = t[1]
        _job_event_log_set_offset(self, self._handle, t[2])
