from typing import (
    Union,
    List,
    Optional,
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
    """
    The startd client.  Used to access the startd history and to control
    `draining <https://htcondor.readthedocs.io/en/latest/admin-manual/cm-configuration.html#defragmenting-dynamic-slots>`_.
    """

    def __init__(self, location : classad.ClassAd = None):
        """
        :param location:  A ClassAd with a ``MyAddress`` attribute, such as
            might be returned by :meth:`htcondor2.Collector.locate`.  :py:obj:`None` means the
            the local daemon.
        """
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
      check_expr : Optional[str] = None,
      start_expr : Optional[str] = None,
      reason : Optional[str] = None,
    ) -> str:
        """
        Request that this startd begin draining.

        :param drain_type:  How much time, if any, jobs are given to finish.
        :param on_completion:  What to do when the startd finishes draining.
        :param check_expr:  An expression which must evaluate to :py:obj:`True`
            in the context of each slot ad for draining to begin.  If
            :py:obj:`None`, treated as ``True``.
        :param start_expr:  The value of the
            `START <https://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#START>`_
            expression while the startd is draining.  If
            :py:obj:`None`, use the configured default
            (`DEFAULT_DRAINING_START_EXPR <https://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#DEFAULT_DRAINING_START_EXPR>`_).
        :param reason:  A string describing the reason for draining.

        :return:  An opaque request ID only useful for cancelling this drain
            via :meth:`cancelDrainJobs`.
        """
        if check_expr is not None and not isinstance(check_expr, str):
            raise TypeError("check_expr must be a string")
        if start_expr is not None and not isinstance(start_expr, str):
            raise TypeError("start_expr must be a string")
        return _startd_drain_jobs(self._addr,
          int(drain_type), int(on_completion), check_expr, start_expr, reason
        )


    def cancelDrainJobs(self, request_id : str = None) -> None:
        """
        Cancel a draining request.

        :param request_id:  An opaque drain request ID returned by a
            previous call to :meth:`drainJobs`.  If :py:obj:`None`,
            cancel all draining requests for this startd.
        """
        _startd_cancel_drain_jobs(self._addr, request_id)


    # Totally undocumented in version 1.
    def history(self,
        constraint : Optional[Union[str, classad.ExprTree]] = None,
        projection : List[str] = [],
        match : int = -1,
        since : Union[int, str, classad.ExprTree] = None,
    ) -> List[classad.ClassAd]:
        """
        Query this startd's job history.

        :param constraint:  A query constraint.  Only jobs matching this
            constraint will be returned.  :py:obj:`None` will match all jobs.
        :param projection:  A list of job attributes.  These attributes will
            be returned for each job in the list.  (Others may be as well.)
            The default (an empty list) returns all attributes.
        :param match:  The maximum number of ads to return.  The default
            (``-1``) is to return all ads.
        :param since:  A cluster ID, job ID, or expression.  If a cluster ID
            (passed as an integer) or job ID (passed as a string in the format
            ``{clusterID}.{procID}``), only jobs recorded in the history
            file after (and not including) the matching ID will be
            returned.  If an expression (including strings that aren't
            job IDs), jobs will be returned, most-recently-recorded first,
            until the expression becomes true; the job making the expression
            become true will not be returned.  Thus, ``1038`` and
            ``"clusterID == 1038"`` return the same set of jobs.

            If :py:obj:`None`, return all (matching) jobs.
        :return:  A list of `job ClassAds <https://htcondor.readthedocs.io/en/latest/classad-attributes/job-classad-attributes.html>`_.
        """
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
            # Ad Type Filter
            "",
            # HRS_STARTD_JOB_HIST
            1,
            # GET_HISTORY
            429,
        )

