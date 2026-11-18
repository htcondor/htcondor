import enum
import re

from typing import (
    Callable,
    Optional,
    Union,
    List,
    Tuple,
)

from ._common_imports import classad
from .htcondor2_impl import _history_query

CMD_SCHEDD_HISTORY = 515 # QUERY_SCHEDD_HISTORY
CMD_STARTD_HISTORY = 429 # GET_HISTORY

# First version whose remote history handler understands the job/user
# and DAG/batch filters (HistoryJobSpecFilter, HistoryDagIdFilter, etc).
# Older daemons silently ignore the unknown query attributes and return
# unfiltered results, so the filters must not be sent to them; they are
# converted into an equivalent constraint instead.
MIN_VERSION_JOB_FILTERS = (26, 3, 0)

# NOTE: For internal use currently
#       Keep up to date with HistoryRecordSource
#       enum in src/condor_tools/history.cpp
class HistorySrc(enum.IntEnum):
    """
    An enumeration of history record sources.

    .. attribute:: ScheddJob

        Search final job history in :macro:`HISTORY`

    .. attribute:: StartdJob

        Search job history from Startd's :macro:`HISTORY`

    .. attribute:: JobEpoch

        Search historical records from :macro:`JOB_EPOCH_HISTORY`.
        Currently contains job epoch, transfer, and spawn records.

    .. attribute:: Daemon

        Search for historical daemon records from ``<SUBSYS>_DAEMON_HISTORY``.

        .. note::

            Requires history queries AdType to be set to the desired
            daemon subsystem.

    :meta private:
    """

    ScheddJob = 0
    StartdJob = 1
    JobEpoch = 2
    Daemon = 3


# ASCII digits only: Python's \d also matches other Unicode digits
_JOB_ID_RE = re.compile(r'[0-9]+(\.[0-9]+)?')
# Any printable ASCII character except '"' and '\\' (condor_history puts the
# name unescaped into an Owner == "name" constraint), not starting with '-'
_USER_RE = re.compile(r'[ !#-,.-\[\]-~][ !#-\[\]-~]*')
_VERSION_RE = re.compile(r'\$CondorVersion:\s*(\d+)\.(\d+)\.(\d+)')


def _is_int(value) -> bool:
    # bool is a subclass of int, but True/False are never valid ids
    return isinstance(value, int) and not isinstance(value, bool)


# Each validator returns the string form of a valid item, or raises
# TypeError (wrong type) / ValueError (right type, invalid format).
# These mirror the checks done by the remote history handler
# (src/condor_utils/history_queue.cpp) so bad input fails locally
# with a clear error instead of as a remote query failure.

def _valid_user(name: str, user) -> str:
    if not isinstance(user, str):
        raise TypeError(f"{name} must be a string or list of strings")
    # A job id (N or N.M) would be read as a job id, not a username
    if _USER_RE.fullmatch(user) is None or _JOB_ID_RE.fullmatch(user) is not None:
        raise ValueError(f"{name} contains invalid username '{user}'")
    return user


def _valid_job(name: str, job) -> str:
    # Cluster ids start at 1
    if _is_int(job):
        if job <= 0:
            raise ValueError(f"{name} contains invalid job id '{job}'")
        return str(job)
    if not isinstance(job, str):
        raise TypeError(f"{name} must be a job id or list of job ids")
    if _JOB_ID_RE.fullmatch(job) is None:
        raise ValueError(f"{name} contains invalid job id '{job}': expected N or N.M")
    cluster, _, proc = job.partition(".")
    if int(cluster) <= 0:
        raise ValueError(f"{name} contains invalid job id '{job}': cluster id must be greater than 0")
    # Normalize (i.e. "010" -> "10") as old ClassAd syntax rejects leading zeros
    return f"{int(cluster)}.{int(proc)}" if proc else str(int(cluster))


def _valid_batch_name(name: str, batch) -> str:
    if not isinstance(batch, str):
        raise TypeError(f"{name} must be a string or list of strings")
    # A batch name is any non-empty string: condor_history always takes
    # the argument after -batchname as its value
    if batch == "":
        raise ValueError(f"{name} contains invalid batch name '{batch}'")
    return batch


def _valid_batch_id(name: str, batch) -> str:
    if _is_int(batch):
        if batch < 0:
            raise ValueError(f"{name} contains invalid batch id '{batch}'")
        return str(batch)
    if not isinstance(batch, str):
        raise TypeError(f"{name} must be a string, integer, or list of strings/integers")
    if batch == "" or batch.startswith("-"):
        raise ValueError(f"{name} contains invalid batch id '{batch}'")
    return batch


def _valid_dag_id(name: str, dag) -> str:
    if isinstance(dag, str):
        if not (dag.isascii() and dag.isdigit()):
            raise ValueError(f"{name} contains invalid DAGMan job id '{dag}'")
        dag = int(dag)
    elif not _is_int(dag):
        raise TypeError(f"{name} must be an integer, string, or list of integers/strings")
    if dag <= 0:
        raise ValueError(f"{name} contains invalid DAGMan job id '{dag}'")
    return str(dag)


def _as_list(name: str, value, valid: Callable[[str, object], str]) -> List[str]:
    """
    Normalize a scalar-or-list filter argument into a list of validated strings.
    :py:obj:`None` is an empty filter.
    """
    if value is None:
        return []
    items = value if isinstance(value, (list, tuple)) else [value]
    return [valid(name, item) for item in items]


def _to_classad_list(items: List[str]) -> str:
    """
    Format items as the string form of a ClassAd list of strings,
    i.e. ``{"a", "b"}``. An empty list returns an empty string so
    the attribute is omitted from the query ad.
    """
    if not items:
        return ""
    return "{" + ", ".join(classad.quote(item) for item in items) + "}"


def _parse_version(version: Optional[str]) -> Optional[Tuple[int, int, int]]:
    if not version:
        return None
    m = _VERSION_RE.search(version)
    if m is None:
        return None
    return (int(m[1]), int(m[2]), int(m[3]))


def _supports_job_filters(remote_version: Optional[str]) -> bool:
    """
    True if a daemon reporting ``remote_version`` (its ``CondorVersion``
    string) supports the job/user and DAG/batch history filters.
    """
    version = _parse_version(remote_version)
    return version is not None and version >= MIN_VERSION_JOB_FILTERS


def _quote_old_classad_string(value: str) -> str:
    """
    Quote a string as an old ClassAd syntax literal (as ``QuoteAdStringValue()``
    does), because the remote history constraint is parsed as old ClassAd
    syntax, where only a quote is escaped (i.e. a backslash is literal).
    """
    return classad.ClassAd({"Value": value}).printOld().strip().partition(" = ")[2]


def _filters_to_constraint(
    constraint : Optional[Union[str, classad.ExprTree]],
    users : List[str],
    jobs : List[str],
    dagids : List[str],
    batchids : List[str],
    batchnames : List[str],
) -> str:
    """
    Build the constraint (old ClassAd syntax) equivalent to the given (already
    validated) filters for daemons that do not support them. Matches what
    :tool:`condor_history` builds: the filters are OR'd together and AND'd
    with ``constraint``.
    """
    quote = _quote_old_classad_string
    terms = [f"Owner == {quote(user)}" for user in users]
    for job in jobs:
        cluster, _, proc = job.partition(".")
        term = f"ClusterId == {cluster}"
        if proc:
            term = f"({term} && ProcId == {proc})"
        terms.append(term)
    terms += [f"DAGManJobId == {dag}" for dag in dagids]
    terms += [f"JobBatchId =?= {quote(batch)}" for batch in batchids]
    terms += [f"JobBatchName =?= {quote(batch)}" for batch in batchnames]

    filter_expr = " || ".join(terms)
    if constraint is None or str(constraint) == "":
        return filter_expr
    return f"({filter_expr}) && ({constraint})"


def _history_internal(
    addr: str,
    cmd: int,
    src: int,
    constraint : Optional[Union[str, classad.ExprTree]] = None,
    projection : List[str] = [],
    match : int = -1,
    since : Union[int, str, classad.ExprTree] = None,
    ad_type : Union[str, List[str]] = "",
    remote_version : Optional[str] = None,
    # High level filter options that gain benefit from the librarian
    # database. They are handled by the remote side and do not modify
    # the user provided constraint here.
    users : Optional[Union[str, List[str]]] = None,
    jobs : Optional[Union[str, int, List[Union[str, int]]]] = None,
    batchnames : Optional[Union[str, List[str]]] = None,
    batchids : Optional[Union[str, int, List[Union[str, int]]]] = None,
    dagids : Optional[Union[int, str, List[Union[int, str]]]] = None,
) -> List[classad.ClassAd]:
    """
    Internal handler for all history queries to Schedd/Startd.

    :param addr: Address of Schedd/Startd to query history from.
    :param cmd: The specific command int (tied to daemon type) to
        execute for remote history query.
    :param src: Then enumeration value of which historical record
        source to read from. See history.cpp
    :param constraint: Constraint expression to return only the
        matching ClassAds.
    :param projection: List of specific ClassAd attributes to return
        from each matched ClassAd. If not specified the full ClassAd
        is returned.
    :param match: Limit of the maximum number of matching ClassAds to
        return. -1 represents no limit.
    :param since: Condition to stop scanning the history file. In the
        integer and string case, stop once job id is located. Otherwise,
        stop if the ClassAd expression evaluates to true.
    :param ad_type: Record type(s) to filter on.
    :param remote_version: ``CondorVersion`` string of the remote daemon,
        used to check if it supports the job/user and DAG/batch filters.
        If not, the filters are sent as an equivalent constraint.
    :param users: A username or list of usernames to filter on.
    :param jobs: A cluster/job id or list of cluster/job ids to filter on.
    :param batchnames: A batch name or list of batch names to filter on.
    :param batchids: A batch id or list of batch ids to filter on.
    :param dagids: A DAGMan job id or list of DAGMan job ids to filter on.
    :return: List of ClassAds.

    :meta private:
    """

    user_filter = _as_list("users", users, _valid_user)
    job_filter = _as_list("jobs", jobs, _valid_job)
    batchname_filter = _as_list("batchnames", batchnames, _valid_batch_name)
    batchid_filter = _as_list("batchids", batchids, _valid_batch_id)
    dagid_filter = _as_list("dagids", dagids, _valid_dag_id)

    if user_filter or job_filter or batchname_filter or batchid_filter or dagid_filter:
        if not _supports_job_filters(remote_version):
            # Older daemons ignore the filters, so send the equivalent
            # constraint instead (which cannot use the librarian index)
            constraint = _filters_to_constraint(
                constraint, user_filter, job_filter,
                dagid_filter, batchid_filter, batchname_filter,
            )
            user_filter, job_filter = [], []
            batchname_filter, batchid_filter, dagid_filter = [], [], []

    jobspecs = user_filter + job_filter

    projection_string = ",".join(projection)

    if _is_int(since):
        since = f"ClusterID == {since}"
    elif isinstance(since, str):
        if _JOB_ID_RE.fullmatch(since) is not None:
            # Use int() to drop leading zeros, which old ClassAd syntax
            # (used by the remote side to parse since) rejects
            cluster, _, proc = since.partition(".")
            since = f"ClusterID == {int(cluster)}"
            if proc:
                since += f" && ProcID == {int(proc)}"
        else:
            try:
                classad.ExprTree(since)
            except classad.ClassAdException:
                raise ValueError("since string must be a clusterID[.procID] or the string form of an ExprTree.")
    elif isinstance(since, classad.ExprTree):
        since = str(since)
    elif since is None:
        since = ""
    else:
        raise TypeError("since must be an int, string, or ExprTree")

    if ad_type is None:
        ad_type = ""
    elif isinstance(ad_type, list):
        ad_type = ",".join(ad_type)
    elif not isinstance(ad_type, str):
        raise TypeError("ad_type must be a list of strings or a string")

    if constraint is None:
        constraint = ""

    return _history_query(
        addr,
        str(constraint),
        projection_string,
        int(match),
        since,
        # Specific record type filter
        ad_type,
        # Specific history source file(s)
        src,
        # Command int to send
        cmd,
        # Job/user & DAG/batch filters as ClassAd lists of strings
        _to_classad_list(jobspecs),
        _to_classad_list(dagid_filter),
        _to_classad_list(batchid_filter),
        _to_classad_list(batchname_filter),
    )
