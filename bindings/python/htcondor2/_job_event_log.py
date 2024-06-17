import time

from .htcondor2_impl import _handle as handle_t

from ._job_event import JobEvent

from .htcondor2_impl import (
    _job_event_log_init,
    _job_event_log_next,
    _job_event_log_close,
    _job_event_log_get_offset,
    _job_event_log_set_offset,
)

class JobEventLog():
    """
    Produce an iterator over job events in a user job event log.  You may
    block or poll for new events.

    A pickled :class:`JobEventLog` resumes generating events if and only if,
    after being unpickled, the job event log file is identical except for
    appended events.
    """

    def __init__(self, filename : str):
        """
        :param filename:  A file containing a job event log.
        """
        self._deadline = 0
        self._filename = filename
        self._handle = handle_t()

        if not isinstance(filename, str):
            raise TypeError("filename must be a string")
        if len(filename) == 0:
            raise ValueError("filename must not be empty")
        _job_event_log_init(self, self._handle, filename)


    def events(self, stop_after : int = None) -> 'JobEventLog':
        """
        Returns an iterator over events in the log.

        :param stop_after:  After how many seconds should the iterator
            stop waiting for events?

            If :py:obj:`None`, the iterator waits forever (blocks).

            If ``0``, the iterator never waits (does not block;
            a pure polling iterator).

            For any other value, wait (block) for that many seconds for
            a new event, raising :class:`StopIteration` if one does not
            appear.  (This does not invalidate the iterator.)
        """
        if stop_after is None:
            self._deadline = 0
        elif isinstance(stop_after, int):
            self._deadline = int(time.time()) + stop_after
        else:
            # This was HTCondorTypeError in version 1.
            raise TypeError("deadline must be an integer")

        return self


    def close(self) -> None:
        """
        Close any open underlying file.  This object's iterators will no
        longer produce new events.
        """
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

    #
    # Pickling.
    #

    def __getstate__(self):
        offset = _job_event_log_get_offset(self, self._handle)
        state = self.__dict__.copy()
        del state['_handle']
        return (state, self._deadline, offset, self._filename)


    def __setstate__(self, t):
        self.__dict__ = t[0]
        self._deadline = t[1]
        self._filename = t[3]
        self._handle = handle_t()
        _job_event_log_init(self, self._handle, self._filename)
        _job_event_log_set_offset(self, self._handle, t[2])
