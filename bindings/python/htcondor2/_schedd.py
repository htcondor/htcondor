import re

# We'll be able to just use `|` when our minimum Python version becomes 3.10.
from typing import Union;

from ._common_imports import (
    classad,
    Collector,
    DaemonType,
)

from ._query_opts import QueryOpts;
from ._job_action import JobAction;
from ._transaction_flag import TransactionFlag;

from .htcondor2_impl import (
    _schedd_query,
    _schedd_act_on_job_ids,
    _schedd_act_on_job_constraint,
    _schedd_edit_job_ids,
    _schedd_edit_job_constraint,
    _schedd_reschedule,
)


class Schedd():

    def __init__(self, location : classad.ClassAd = None):
        if location is None:
            c = Collector()
            location = c.locate(DaemonType.Schedd)

        if not isinstance(location, classad.ClassAd):
            raise TypeError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything.
        # self._version = location['CondorVersion']


    def query(self,
        constraint : Union[str, classad.ExprTree] = 'true',
        projection : list[str] = [],
        callback : callable = None,
        limit : int = -1,
        opts : QueryOpts = QueryOpts.Default
    ) -> list[classad.ClassAd]:
        results = _schedd_query(self._addr, str(constraint), projection, int(limit), int(opts))
        if callback is None:
            return results

        # We could pass `None` as the first argument to filter() if we
        # were sure that nothing coming back from callback() was false-ish.
        #
        # The parentheses make the second argument a generator, which is
        # probably a little more efficient than a list comprehension even
        # though we immediately turn it back into a list.
        return list(
            filter(
                lambda r: r is not None,
                (callback(result) for result in results)
            )
        )


    def act(self,
        action : JobAction,
        job_spec : Union[list[str], str, classad.ExprTree],
        reason : str = None,
    ) -> classad.ClassAd:
        if not isinstance(action, JobAction):
            raise TypeError("action must be a JobAction")

        reason_code = None
        reason_string = None

        if reason is None:
            reason_string = "Python-initiated action"
        elif isinstance(reason, tuple):
            (reason_string, reason_code) = reason

        result = None
        if isinstance(job_spec, list):
            job_spec_string = ", ".join(job_spec)
            result = _schedd_act_on_job_ids(
                self._addr,
                job_spec_string, int(action), reason_string, str(reason_code)
            )
        elif re.fullmatch('\d+(\.\d+)?', job_spec):
            result = _schedd_act_on_job_ids(
                self._addr,
                job_spec, int(action), reason_string, str(reason_code)
            )
        else:
            job_constraint = str(job_spec)
            result = _schedd_act_on_job_constraint(
                self._addr,
                job_constraint, int(action), reason_string, str(reason_code)
            )


        if result is None:
            return None

        # Why did we do this in version 1?
        pyResult = classad.ClassAd()
        pyResult["TotalError"] = result["result_total_0"]
        pyResult["TotalSuccess"] = result["result_total_1"]
        pyResult["TotalNotFound"] = result["result_total_2"]
        pyResult["TotalBadStatus"] = result["result_total_3"]
        pyResult["TotalAlreadyDone"] = result["result_total_4"]
        pyResult["TotalPermissionDenied"] = result["result_total_5"]
        pyResult["TotalJobAds"] = result["TotalJobAds"]
        pyResult["TotalChangedAds"] = result["ActionResult"]

        return pyResult


    # Note that edit(ClassAd) and edit_multiple() aren't documented.
    def edit(self,
        job_spec : Union[list[str], str, classad.ExprTree],
        attr : str,
        value : Union[str, classad.ExprTree],
        flags : TransactionFlag = TransactionFlag.Default,
    ) -> classad.ClassAd:
        if not isinstance(flags, TransactionFlag):
            raise TypeError("flags must be a TransactionFlag")


        # We pass the list into C++ so that we don't have to (re)connect to
        # the schedd for each job ID.  We don't want to avoid that with a
        # handle_t stored in `self` because that would imply leaving a socket
        # open to the schedd while Python code has control, which we think
        # is a really bad idea.

        match_count = None
        if isinstance(job_spec, list):
            match_count = _schedd_edit_job_ids(
                self._addr,
                job_spec, attr, str(value), flags
            )
        elif re.fullmatch('\d+(\.\d+)?', job_spec):
            match_count = _schedd_edit_job_ids(
                self._addr,
                [job_spec], attr, str(value), flags
            )
        else:
            match_count = _schedd_edit_job_constraint(
                self._addr,
                str(job_spec), attr, str(value), flags
            )


        # FIXME: Does this really need to be an undocumented object?
        return match_count


    # In version 1, history() returned a HistoryIterator which kept a
    # socket around to read results from; this was OK, because the socket
    # usually required no, and at most minimal, attention from the schedd
    # before being handed entirely off to the history helper process.
    #
    # This can certainly be done in version 2, but I'd like to see where
    # else we'd like to keep a socket open before implementing anything.

    # `match` should be `limit` for consistency with `query()`.
    def history(self,
        constraint : Union[str, classad.ExprTree],
        projection : list[str] = [],
        match : int = -1,
        since : Union[int, str, classad.ExprTree] = None,
    ) -> "HistoryIterator": # FIXME: remove quotes
        # FIXME
        pass


    def submit(self,
        description : "Submit", # FIXME: remove quotes
        count : int = 1,
        spool : bool = False,
    ) -> "SubmitResult": # FIXME: remove quotes
        # FIXME
        pass


    def spool(self,
        ad_list : list[classad.ClassAd],
    ) -> None:
        # FIXME
        pass


    def retrieve(self,
        job_spec : Union[list[str], str, classad.ExprTree],
    ) -> None:
        # FIXME
        pass


    # Assuming this should be deprecated.
    # def refreshGSIProxy()


    def reschedule(self) -> None:
        _schedd_reschedule(self._addr)
        pass


    def export_jobs(self,
        job_spec : Union[list[str], str, classad.ExprTree],
        export_dir : str,
        new_spool_dir : str,
    ) -> classad.ClassAd:
        # FIXME
        pass


    def import_exported_job_results(self,
        import_dir : str
    ) -> classad.ClassAd:
        # FIXME
        pass


    def unexport_jobs(self,
        job_spec : Union[list[str], str, classad.ExprTree],
    ) -> classad.ClassAd:
        # FIXME
        pass
