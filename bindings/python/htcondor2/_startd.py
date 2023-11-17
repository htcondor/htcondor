from typing import (
    Union,
    List,
)

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
    _history_query,
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


    # Totally undocumented in version 1.
    def history(self,
        constraint : Union[str, classad.ExprTree],
        projection : List[str] = [],
        match : int = -1,
        since : Union[int, str, classad.ExprTree] = None,
    ) -> List[classad.ClassAd]:
        projection_string = ",".join(projection)

        if isinstance(since, int):
            since = f"ClusterID == {since}"
        elif isinstance(since, str):
            pattern = re.compile(r'(\d+).(\d+)')
            matches = pattern.match(since)
            if matches is None:
                raise ValueError("since string must be in the form {clusterID}.{procID}")
            since = f"ClusterID == {matches[0]} && ProcID == {matches[1]}"
        elif isinstance(since, classad.ExprTree):
            since = str(since)
        elif since is None:
            since = ""
        else:
            raise TypeError("since must be an int, string, or ExprTree")

        if constraint is None:
            constraint = ""

        return _history_query(self._addr,
            str(constraint), projection_string, int(match), since,
            # HRS_STARTD_JOB_HIST
            1,
            # GET_HISTORY
            429,
        )

