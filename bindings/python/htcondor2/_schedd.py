from typing import (
    Union,
    Callable,
    Any,
    Iterator,
    List,
    Optional,
)

import re

# We'll be able to just use `|` when our minimum Python version becomes 3.10.
from typing import Union

from ._common_imports import (
    classad,
    Collector,
    DaemonType,
)

from ._job_action import JobAction
from ._transaction_flag import TransactionFlag
from ._submit import Submit
from ._submit_result import SubmitResult
# So that the typehints match version 1.
from ._query_opt import QueryOpt as QueryOpts

from .htcondor2_impl import (
    _schedd_query,
    _schedd_act_on_job_ids,
    _schedd_act_on_job_constraint,
    _schedd_edit_job_ids,
    _schedd_edit_job_constraint,
    _schedd_reschedule,
    _schedd_export_job_ids,
    _schedd_export_job_constraint,
    _schedd_import_exported_job_results,
    _schedd_unexport_job_ids,
    _schedd_unexport_job_constraint,
    _schedd_submit,
    _history_query,
    _schedd_retrieve_job_constraint,
    _schedd_retrieve_job_ids,
    _schedd_spool,
    _schedd_refresh_gsi_proxy,
)


DefaultItemData = object()


def job_spec_hack(
    addr : str,
    job_spec : Union[List[str], str, classad.ExprTree],
    f_job_ids : callable,
    f_constraint : callable,
    args : list,
):
    if isinstance(job_spec, list):
        if not all([isinstance(i, str) for i in job_spec]):
            raise TypeError("All elements of the job_spec list must be strings.")
        job_spec_string = ", ".join(job_spec)
        return f_job_ids(addr, job_spec_string, *args)
    elif isinstance(job_spec, int):
        job_spec_string = str(job_spec)
        return f_job_ids(addr, job_spec_string, *args)
    elif isinstance(job_spec, classad.ExprTree):
        job_spec_string = str(job_spec)
        return f_constraint(addr, job_spec_string, *args)
    elif isinstance(job_spec, str):
        if re.fullmatch(r'\d+(\.\d+)?', job_spec):
            return f_job_ids(addr, job_spec, *args)
        try:
            job_spec_expr = classad.ExprTree(job_spec)
            return f_constraint(addr, job_spec, *args);
        except ValueError:
            raise TypeError("The job_spec string must be a clusterID[.jobID] or the string form of an ExprTree.");
    else:
        raise TypeError("The job_spec must be list of strings, a string, an int, or an ExprTree." );


class Schedd():
    '''
    Client object for a *condor_schedd*.
    '''

    def __init__(self, location : classad.ClassAd = None):
        '''
        :param location:  A :class:`classad2.ClassAd` specifying a remote
            *condor_schedd* daemon, as returned by :meth:`Collector.locate`.
            If `None`, the client will connect to the local *condor_schedd*.
        '''
        if location is None:
            c = Collector()
            location = c.locate(DaemonType.Schedd)

        if not isinstance(location, classad.ClassAd):
            raise TypeError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything, but we'll carry it
        # around in case somebody else does.
        self._version = location['CondorVersion']


    def query(self,
        constraint : Union[str, classad.ExprTree] = 'True',
        projection : List[str] = [],
        callback : Callable[[classad.ClassAd], Any] = None,
        limit : int = -1,
        opts : QueryOpts = QueryOpts.Default
    ) -> List[classad.ClassAd]:
        '''
        Query the *condor_schedd* daemon for job ads.

        :param constraint:  A query constraint.  Only jobs matching this
            constraint will be returned.  The default will return all jobs.
        :param projection:  A list of job attributes.  These attributes will
            be returned for each job in the list.  (Others may be as well.)
            The default (an empty list) returns all attributes.
        :param callback:  A filtering function.  It will be invoked for
            each job ad which matches the constraint.  The value returned
            by *callback* will replace the corresponding job ad in the
            list returned by this method unless that value is `None`, which
            will instead be omitted.
        :param limit:  The maximum number of ads to return.  The default
            (``-1``) is to return all ads.
        :param opts:  Special query options; see the enumeration for details.
        '''
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
        job_spec : Union[List[str], str, classad.ExprTree, int],
        reason : str = None,
    ) -> classad.ClassAd:
        """
        Change the status of job(s) in the *condor_schedd* daemon.

        :param action:  The action to perform.
        :param job_spec: Which job(s) to act on.  A :class:`str`
             of the form ``clusterID[.procID]``, a :class:`list` of such
             strings, a :class:`classad2.ExprTree` constraint, the
             the string form of such a constraint, or the :class:`int` cluster ID.
        :param reason:  A free-form justification.  Defaults to
            "Python-initiated action".
        :return:  A ClassAd describing the number of jobs changed.  This
                  ClassAd is currently undocumented.
        """
        if not isinstance(action, JobAction):
            raise TypeError("action must be a JobAction")

        reason_code = None
        reason_string = None

        if reason is None:
            reason_string = "Python-initiated action"
        elif isinstance(reason, tuple):
            (reason_string, reason_code) = reason

        result = job_spec_hack(self._addr, job_spec,
            _schedd_act_on_job_ids, _schedd_act_on_job_constraint,
            (int(action), reason_string, str(reason_code))
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


    # In version 1, edit(ClassAd) and edit_multiple() weren't documented,
    # so they're not implemented in version 2.
    def edit(self,
        job_spec : Union[List[str], str, classad.ExprTree, int],
        attr : str,
        value : Union[str, classad.ExprTree],
        flags : TransactionFlag = TransactionFlag.Default,
    ) -> int:
        """
        Change the value of an attribute in zero or more jobs.

        :param job_spec: Which job(s) to act on.  A :class:`str`
             of the form ``clusterID[.procID]``, a :class:`list` of such
             strings, a :class:`classad2.ExprTree` constraint, the
             the string form of such a constraint, or the :class:`int` cluster ID.
        :param attr:  Which attribute to change.
        :param value:  The new value for the attribute.
        :param flags:  Optional flags specifying alternate transaction behavior.
        :return:  The number of jobs that were edited.
        """
        if not isinstance(flags, TransactionFlag):
            raise TypeError("flags must be a TransactionFlag")

        if isinstance(value, classad.ExprTree):
            str_value = repr(value)
        else:
            str_value = str(value)

        # We pass the list into C++ so that we don't have to (re)connect to
        # the schedd for each job ID.  We don't want to avoid that with a
        # handle_t stored in `self` because that would imply leaving a socket
        # open to the schedd while Python code has control, which we think
        # is a really bad idea.

        match_count = job_spec_hack(self._addr, job_spec,
            _schedd_edit_job_ids, _schedd_edit_job_constraint,
            (attr, str_value, flags),
        )

        # In version 1, this was an undocumented and mostly pointless object.
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
        constraint : Optional[Union[str, classad.ExprTree]] = None,
        projection : List[str] = [],
        match : int = -1,
        since : Union[int, str, classad.ExprTree] = None,
    ) -> List[classad.ClassAd]:
        """
        Query this schedd's job history.

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
            # HRS_JOB_HISTORY
            0,
            # QUERY_SCHEDD_HISTORY
            515
        )


    def jobEpochHistory(self,
        constraint : Optional[Union[str, classad.ExprTree]] = None,
        projection : List[str] = [],
        match : int = -1,
        since : Union[int, str, classad.ExprTree] = None,
        **kwargs
    ) -> List[classad.ClassAd]:
        """
        Query this schedd's
        `job epoch <https://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#JOB_EPOCH_HISTORY>`_
        history.

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

        ad_type = kwargs.get("ad_type", None)
        if isinstance(ad_type, list):
            ad_type = ",".join(ad_type)
        elif ad_type is None:
            ad_type = ""
        elif not isinstance(ad_type, str):
            raise TypeError("ad_type must be a list of strings or a string")

        if constraint is None:
            constraint = ""

        return _history_query(self._addr,
            str(constraint), projection_string, int(match), since, ad_type,
            # HRS_JOB_EPOCH
            2,
            # QUERY_SCHEDD_HISTORY
            515
        )


    def submit(self,
        description : Submit,
        #
        # The version 1 documentation claims this is "the number of jobs
        # to submit to the cluster."  That is true only if we don't do
        # submit with itemdata, which we want to do from here if the
        # submit object has any.  If we do submit with itemdata, `count`
        # is the number of jobs per itemdatum, e.g. `QUEUE 5 FROM *.dat`
        # submit five jobs per `.dat` file.
        #
        # If `itemdata` is `None` and `count` is 0, the count should
        # come from the QUEUE statement.  In version 1, a `count` of 0
        # always functions as if `count` were 1, so we can safely change
        # the default to 0.  That allows
        #
        # In any case, `None` should not raise an exception.
        #
        count : int = 0,
        spool : bool = False,
        # This was undocumented in version 1, but it became necessary when
        # we removed Submit.queue_with_itemdata().
        itemdata : Optional[ Union[ Iterator[str], Iterator[dict] ] ] = DefaultItemData,
    ) -> SubmitResult:
        '''
        Submit one or more jobs.

        .. note::
            This function presently uses :mod:`warnings` to pass along
            the warnings generated about the submit.  Python by default
            suppresses the second and subsequent reports of a warning
            for the same line of code.

        :param description:  The job(s) to submit.
        :param count:  Every valid queue statement in the submit language
            has an associated count, which is implicitly 1, but may be
            set explicitly, e.g., ``queue 3 dat_file matching *.dat`` has
            a count of 3.  If specified, this parameter overrides the count
            in ``description``.
        :param spool:  If :py:obj:`True`, submit the job(s) on hold and in
            such a way that its input files can later be uploaded to this
            schedd's `SPOOL <https://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#SPOOL>`_ directory, using :meth:`spool`.
        :param itemdata:  If your submit description includes a queue statement
            which requires item data, you may supply (or override) that item
            data with this parameter.  If you provide an iterator over strings,
            each string will be parsed as its own item data line.  If you
            provide an iterator over dictionaries, the dictionary's key-value
            pairs will become submit variable name-value pairs; only the first
            dictionary's keys will be used.  In either case, leading and
            trailing whitespace will be trimmed for each individual item.
            Lines (and items) may not contain newlines (``\\n``) or the ASCII
            unit separator character (``\\x1F``).  Keys, if specified, must be
            valid submit-language variable names.
        '''
        if not isinstance(description, Submit):
            raise TypeError( "description must be an htcondor2.Submit object")

        submit_file = ''
        for key, value in description.items():
            submit_file = submit_file + f"{key} = {value}\n"
        submit_file = submit_file + "\n"

        if itemdata is DefaultItemData:
            submit_file = submit_file + "queue " + description.getQArgs() + "\n"

            original_item_data = description.itemdata()
            if original_item_data is not None:
                for item in original_item_data:
                    submit_file = _add_line_from_itemdata(submit_file, item)
                submit_file = submit_file + ")\n"

        elif itemdata is None:
            submit_file = submit_file + "queue\n"

        else:
            first = next(itemdata)
            if isinstance(first, str):
                submit_file = submit_file + "QUEUE item FROM "
            elif isinstance(first, dict):
                if any(not isinstance(x, str) for x in first.keys()):
                    raise TypeError("itemdata dictionaries must have string keys")
                keys_list = ",".join(first.keys())
                submit_file = submit_file + f"QUEUE {keys_list} FROM "
            else:
                raise TypeError("itemdata must be a list of strings or dictionaries")

            submit_file = submit_file + "(\n"
            submit_file = _add_line_from_itemdata(submit_file, first)
            for item in itemdata:
                submit_file = _add_line_from_itemdata(submit_file, item)
            submit_file = submit_file + ")\n"

        real = Submit(submit_file)
        real.setSubmitMethod(
            description.getSubmitMethod(),
            allow_reserved_values=True,
        )
        return _schedd_submit(self._addr, real._handle, count, spool)


    # Let's not support submitMany() unless we have to, since it
    # requires a ClassAd like the one we're deprecating out of submit().


    def spool(self,
        result : SubmitResult
    ) -> None:
        """
        Upload the input files corresponding to a given :meth:`submit`
        for which the ``spool`` flag was set.
        """
        if result._spooledProcAds is None:
            raise ValueError("result must have come from submit() with spool set")

        _schedd_spool(self._addr, result._clusterad._handle, result._spooledProcAds._handle)


    def retrieve(self,
        job_spec : Union[List[str], str, classad.ExprTree, int],
    ) -> None:
        #
        # In version 1, this function was documented as accepting either
        # a constraint expression as a string or a list of ClassAds, but
        # the latter was not implemented.
        #
        # This was presumably intended to be the output from Submit.jobs(),
        # but since that may never be implemented in version 2, let's not
        # worry about it.
        #
        """
        Retrieve the output files from the job(s) in a given :meth:`submit`.

        :param job_spec: Which job(s) to act on.  A :class:`str`
             of the form ``clusterID[.procID]``, a :class:`list` of such
             strings, a :class:`classad2.ExprTree` constraint, the
             the string form of such a constraint, or the :class:`int` cluster ID.
        """
        result = job_spec_hack(self._addr, job_spec,
            _schedd_retrieve_job_ids, _schedd_retrieve_job_constraint,
            []
        )


    def refreshGSIProxy(cluster : int, proc : int, proxy_filename : str, lifetime : int = -1) -> int:
        """
        Refresh a (running) job's GSI proxy.

        :param cluster:  The job's cluster ID.
        :param proc:  The job's proc ID.
        :param proxy_filename:  The name of the file containing the refreshed proxy.
        :param lifetime:  The desired lifetime (in seconds) of the refreshed proxy.
            Specify ``0`` to avoid changing the proxy's lifetime.  Specify
            ``-1`` to use the value specific by :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`.
        :return:  The remaining lifetime.
        """
        return _schedd_refresh_gsi_proxy(self._addr, int(cluster), int(proxy), str(proxy_filename), int(lifetime))


    def reschedule(self) -> None:
        """
        Sends the reschedule command to this schedd, which asks the schedd
        to ask the negotiator to consider starting the next negotiation
        cycle.  Frequently has no effect, because the negotiator is either
        already busy or waiting out the minimum cycle time.
        """
        _schedd_reschedule(self._addr)


    def export_jobs(self,
        job_spec : Union[List[str], str, classad.ExprTree, int],
        export_dir : str,
        new_spool_dir : str,
    ) -> classad.ClassAd:
        """
        Export one or more job clusters from the queue to put those jobs
        into the externally managed state.

        :param job_spec: Which job(s) to act on.  A :class:`str`
             of the form ``clusterID[.procID]``, a :class:`list` of such
             strings, a :class:`classad2.ExprTree` constraint, the
             the string form of such a constraint, or the :class:`int` cluster ID.
        :param export_dir:  Write the exported job(s) into this directory.
        :param new_spool_dir:  The IWD of the export job(s).
        :return:  A ClassAd containing information about the export operation.
            This type of ClassAd is currently undocumented.
        """
        return job_spec_hack(self._addr, job_spec,
            _schedd_export_job_ids, _schedd_export_job_constraint,
            (str(export_dir), str(new_spool_dir)),
        )


    def import_exported_job_results(self,
        import_dir : str
    ) -> classad.ClassAd:
        """
        Import results from previously exported jobs, and take those jobs
        back out of the externally managed state.

        :param import_dir:  Read the imported jobs from this directory.
        :return:  A ClassAd containing information about the import operation.
            This type of ClassAd is currently undocumented.
        """
        return _schedd_import_exported_job_results(self._addr, import_dir)


    def unexport_jobs(self,
        job_spec : Union[List[str], str, classad.ExprTree, int],
    ) -> classad.ClassAd:
        """
        Unexport one or more job clusters that were previously exported
        from the queue.

        :param job_spec: Which job(s) to act on.  A :class:`str`
             of the form ``clusterID[.procID]``, a :class:`list` of such
             strings, a :class:`classad2.ExprTree` constraint, the
             the string form of such a constraint, or the :class:`int` cluster ID.
        :return:  A ClassAd containing information about the unexport operation.
            This type of ClassAd is currently undocumented.
        """
        return job_spec_hack(self._addr, job_spec,
            _schedd_unexport_job_ids, _schedd_unexport_job_constraint,
            (),
        )


def _add_line_from_itemdata(submit_file, item):
    if isinstance(item, str):
        if "\n" in item:
            raise ValueError("itemdata strings must not contain newlines")
        submit_file = submit_file + item + "\n"
    elif isinstance(item, dict):
        if any(["\n" in x for x in item.keys()]):
            raise ValueError("itemdata keys must not contain newlines")
        if any(["\n" in x for x in item.values()]):
            raise ValueError("itemdata values must not contain newlines")
        submit_file = submit_file + "\x1F".join(item.values()) + "\n"
    return submit_file


# Sphinx whines about forward references in the type hints if this isn't here.
from ._submit_result import SubmitResult
