Collector ClassAd Attributes
============================

:classad-attribute-def:`ActiveQueryWorkers`
    Current number of forked child processes handling queries.

:classad-attribute-def:`ActiveQueryWorkersPeak`
    Peak number of forked child processes handling queries since
    collector startup or statistics reset.

:index:`RecentDroppedQueries (ClassAd Collector Attribute)`

:classad-attribute-def:`DroppedQueries`
    Total number of queries aborted since collector startup (or
    statistics reset) because :macro:`COLLECTOR_QUERY_WORKERS_PENDING`
    exceeded, or :macro:`COLLECTOR_QUERY_MAX_WORKTIME` exceeded, or client
    closed TCP socket while request was pending. This statistic is also
    available as ``RecentDroppedQueries`` which represents a count of
    recently dropped queries that occurred within a recent time window
    (default of 20 minutes).

:classad-attribute-def:`CollectorIpAddr`
    String with the IP and port address of the *condor_collector*
    daemon which is publishing this ClassAd.

:classad-attribute-def:`CondorVersion`
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:classad-attribute-def:`CurrentForkWorkers`
    The current number of active forks of the Collector. The Windows
    version of the Collector does not fork and will not have this
    statistic.

:classad-attribute-def:`CurrentJobsRunningAll`
    An integer value representing the sum of all jobs running under all
    universes.

:classad-attribute-def:`CurrentJobsRunning`
    An integer value representing the current number of jobs running
    under the universe which forms the attribute name. For example

    .. code-block:: condor-classad

        CurrentJobsRunningVanilla = 567

    identifies that the *condor_collector* counts 567 vanilla universe
    jobs currently running. ``<universe>`` is one of ``Unknown``,
    ``Vanilla``, ``Scheduler``, ``Java``, ``Parallel``,
    ``VM``, or ``Local``. There are other universes, but they are not
    listed here, as they represent ones that are no longer used in
    Condor.

:classad-attribute-def:`DaemonStartTime`
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`DaemonLastReconfigTime`
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`HandleLocate`
    Number of locate queries the Collector has handled without forking
    since it started.

:classad-attribute-def:`HandleLocateRuntimeAvg`
    Total time spent handling locate queries without forking since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively.

:classad-attribute-def:`HandleLocateForked`
    Number of locate queries the Collector has handled by forking since
    it started. The Windows operating system does not fork and will not
    have this statistic.

:classad-attribute-def:`HandleLocateForkedRuntimeAvg`
    Total time spent forking to handle locate queries since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively. The Windows operating system does not fork
    and will not have this statistic.

:classad-attribute-def:`HandleLocateMissedFork`
    Number of locate queries the Collector received since the Collector
    started that could not be handled immediately because there were
    already too many forked child processes. The Windows operating
    system does not fork and will not have this statistic.

:classad-attribute-def:`HandleLocateMissedForkRuntimeAvg`
    Total time spent queuing pending locate queries that could not be
    immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.

:classad-attribute-def:`HandleQuery`
    Number of queries that are not locate queries the Collector has
    handled without forking since it started.

:classad-attribute-def:`HandleQueryRuntimeAvg`
    Total time spent handling queries that are not locate queries
    without forking since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively.

:classad-attribute-def:`HandleQueryForked`
    Number of queries that are not locate queries the Collector has
    handled by forking since it started. The Windows operating system
    does not fork and will not have this statistic.

:classad-attribute-def:`HandleQueryForkedRuntimeAvg`
    Total time spent forking to handle queries that are not locate
    queries since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively. The Windows operating
    system does not fork and will not have this statistic.

:classad-attribute-def:`HandleQueryMissedFork`
    Number of queries that are not locate queries the Collector received
    since the Collector started that could not be handled immediately
    because there were already too many forked child processes. The
    Windows operating system does not fork and will not have this
    statistic.

:classad-attribute-def:`HandleQueryMissedForkRuntimeAvg`
    Total time spent queuing pending non-locate queries that could not
    be immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.

:classad-attribute-def:`HostsClaimed`
    Description is not yet written.

:classad-attribute-def:`HostsOwner`
    Description is not yet written.

:classad-attribute-def:`HostsTotal`
    Description is not yet written.

:classad-attribute-def:`HostsUnclaimed`
    Description is not yet written.

:classad-attribute-def:`IdleJobs`
    Description is not yet written.

:classad-attribute-def:`Machine`
    A string with the machine's fully qualified host name.

:classad-attribute-def:`MaxJobsRunningAll`
    An integer value representing the sum of all
    ``MaxJobsRunning<universe>`` values.

:classad-attribute-def:`MaxJobsRunning`
    An integer value representing largest number of currently running
    jobs ever seen under the universe which forms the attribute name,
    over the life of this *condor_collector* process. For example

    .. code-block:: condor-config

          MaxJobsRunningVanilla = 401

    identifies that the *condor_collector* saw 401 vanilla universe
    jobs currently running at one point in time, and that was the
    largest number it had encountered. ``<universe>`` is one of
    ``Unknown``, ``Vanilla``, ``Scheduler``, ``Java``,
    ``Parallel``, ``VM``, or ``Local``. There are other universes, but
    they are not listed here, as they represent ones that are no longer
    used in Condor.

:classad-attribute-def:`MyAddress`
    String with the IP and port address of the *condor_collector*
    daemon which is publishing this ClassAd.

:classad-attribute-def:`MyCurrentTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_schedd*
    daemon last sent a ClassAd update to the *condor_collector*.

:classad-attribute-def:`Name`
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:classad-attribute-def:`PeakForkWorkers`
    The maximum number of active forks of the Collector at any time
    since the Collector started. The Windows version of the Collector
    does not fork and will not have this statistic.

:classad-attribute-def:`PendingQueries`
    Number of queries pending that are waiting to fork.

:classad-attribute-def:`PendingQueriesPeak`
    Peak number of queries pending that are waiting to fork since
    collector startup or statistics reset.

:classad-attribute-def:`RunningJobs`
    Definition not yet written.

:classad-attribute-def:`StartdAds`
    The integer number of unique *condor_startd* daemon ClassAds
    counted at the most recent time the *condor_collector* updated its
    own ClassAd.

:classad-attribute-def:`StartdAdsPeak`
    The largest integer number of unique *condor_startd* daemon
    ClassAds seen at any one time, since the *condor_collector* began
    executing.

:classad-attribute-def:`SubmitterAds`
    The integer number of unique submitters counted at the most recent
    time the *condor_collector* updated its own ClassAd.

:classad-attribute-def:`SubmitterAdsPeak`
    The largest integer number of unique submitters seen at any one
    time, since the *condor_collector* began executing.

:classad-attribute-def:`UpdateInterval`
    Description is not yet written.

:classad-attribute-def:`UpdateSequenceNumber`
    An integer that begins at 0, and increments by one each time the
    same ClassAd is again advertised.

:classad-attribute-def:`UpdatesInitial`
    A Statistics attribute representing a count of unique ClassAds seen,
    over the lifetime of this *condor_collector*. Counts per ClassAd
    are advertised in attributes named by ClassAd type as
    ``UpdatesInitial_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.

:classad-attribute-def:`UpdatesLost`
    A Statistics attribute representing the count of updates lost, over
    the lifetime of this *condor_collector*. Counts per ClassAd are
    advertised in attributes named by ClassAd type as
    ``UpdatesLost_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.

:classad-attribute-def:`UpdatesLostMax`
    A Statistics attribute defining the largest number of updates lost
    at any point in time, over the lifetime of this *condor_collector*.
    ClassAd sequence numbers are used to detect lost ClassAds.

:classad-attribute-def:`UpdatesLostRatio`
    A Statistics attribute defining the floating point ratio of the
    total number of updates to the number of updates lost over the
    lifetime of this *condor_collector*. ClassAd sequence numbers are
    used to detect lost ClassAds. A value of 1 indicates that all
    ClassAds have been lost.

:classad-attribute-def:`UpdatesTotal`
    A Statistics attribute representing the count of the number of
    ClassAd updates received over the lifetime of this
    *condor_collector*. Counts per ClassAd are advertised in attributes
    named by ClassAd type as ``UpdatesTotal_<ClassAd-Name>``.
    ``<ClassAd-Name>`` is each of ``CkptSrvr``, ``Collector``,
    ``Defrag``, ``Master``, ``Schedd``, ``Start``, ``StartdPvt``, and
    ``Submittor``.
