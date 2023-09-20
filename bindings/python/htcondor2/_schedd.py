# We'll be able to just use `|` when our minimum Python version becomes 3.10.
from typing import Union;

from ._common_imports import (
    classad,
    Collector,
    DaemonType,
)

from ._query_opts import QueryOpts;

from .htcondor2_impl import (
    _schedd_query,
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
        action : "JobAction", # FIXME: remove quotes
        job_spec : Union[list[str], str, classad.ExprTree],
        reason : str = None,
    ) -> classad.ClassAd:
        # FIXME
        pass


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
    # def refreshGSIProxy(self,


    def reschedule(self) -> None:
        # FIXME
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
