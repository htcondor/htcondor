# We'll be able to just use `|` when our minimum Python version becomes 3.10.
from typing import Union;

from ._common_imports import (
    classad,
)

from ._query_opts import QueryOpts;

# from .htcondor2_impl import ()


class Schedd():

    def __init__(self, location : classad.ClassAd = None):
        # FIXME
        pass


    def query(self,
        constraint : bool = True,
        projection : list[str] = [],
        callback : callable = None,
        limit : int = -1,
        opts : QueryOpts = QueryOpts.Default
    ) -> list[classad.ClassAd]:
        # FIXME
        pass


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
