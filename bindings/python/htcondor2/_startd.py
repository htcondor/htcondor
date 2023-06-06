from ._common_imports import (
    classad,
    Collector,
    DaemonType,
)

from ._drain_type import DrainType
from ._completion_type import CompletionType

from .htcondor2_impl import (
    _startd_drain_jobs,
    _startd_cancel_drain_jobs,
)


class Startd():

    def __init__(self, location : classad.ClassAd = None):
        if location is None:
            c = Collector()
            location = c.locate(DaemonType.Startd)

        if not isinstance(location, classad.ClassAd):
            raise TypeError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything.
        # self._version = location['CondorVersion']


    # In version 1, check_expr and start_expr could also be `ExprTree`s.
    def drainJobs(self,
      drain_type : DrainType = DrainType.Graceful,
      on_completion : CompletionType = CompletionType.Nothing,
      check_expr : str = "True",
      start_expr : str = "False",
      reason : str = None,
    ) -> str:
        if check_expr is None:
            raise TypeError("check_expr must be a string")
        if start_expr is None:
            raise TypeError("start_expr must be a string")
        return _startd_drain_jobs(self._addr,
          int(drain_type), int(on_completion), check_expr, start_expr, reason
        )


    def cancelDrainJobs(self, request_id : str = None) -> None:
        _startd_cancel_drain_jobs(self._addr, request_id)
