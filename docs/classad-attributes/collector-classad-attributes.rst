Collector ClassAd Attributes
============================

:index:`Collector attributes<single: Collector attributes; ClassAd>`

:classad-attribute:`ActiveQueryWorkers`
    Current number of forked child processes handling queries.

:classad-attribute:`ActiveQueryWorkersPeak`
    Peak number of forked child processes handling queries since
    collector startup or statistics reset.

:index:`RecentDroppedQueries (ClassAd Collector Attribute)`

:classad-attribute:`DroppedQueries`
    Total number of queries aborted since collector startup (or
    statistics reset) because ``COLLECTOR_QUERY_WORKERS_PENDING``
    exceeded, or :macro:`COLLECTOR_QUERY_MAX_WORKTIME` exceeded, or client
    closed TCP socket while request was pending. This statistic is also
    available as ``RecentDroppedQueries`` which represents a count of
    recently dropped queries that occured within a recent time window
    (default of 20 minutes).

:classad-attribute:`CollectorIpAddr`
    String with the IP and port address of the *condor_collector*
    daemon which is publishing this ClassAd.

:classad-attribute:`CondorVersion`
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:classad-attribute:`CurrentForkWorkers`
    The current number of active forks of the Collector. The Windows
    version of the Collector does not fork and will not have this
    statistic.

:classad-attribute:`CurrentJobsRunningAll`
    An integer value representing the sum of all jobs running under all
    universes.

:classad-attribute:`CurrentJobsRunning`
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

:classad-attribute:`DaemonStartTime`
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`DaemonLastReconfigTime`
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`HandleLocate`
    Number of locate queries the Collector has handled without forking
    since it started.

:classad-attribute:`HandleLocateRuntimeAvg`
    Total time spent handling locate queries without forking since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively.

:classad-attribute:`HandleLocateForked`
    Number of locate queries the Collector has handled by forking since
    it started. The Windows operating system does not fork and will not
    have this statistic.

:classad-attribute:`HandleLocateForkedRuntimeAvg`
    Total time spent forking to handle locate queries since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively. The Windows operating system does not fork
    and will not have this statistic.

:classad-attribute:`HandleLocateMissedFork`
    Number of locate queries the Collector recieved since the Collector
    started that could not be handled immediately because there were
    already too many forked child processes. The Windows operating
    system does not fork and will not have this statistic.

:classad-attribute:`HandleLocateMissedForkRuntimeAvg`
    Total time spent queueing pending locate queries that could not be
    immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.

:classad-attribute:`HandleQuery`
    Number of queries that are not locate queries the Collector has
    handled without forking since it started.

:classad-attribute:`HandleQueryRuntimeAvg`
    Total time spent handling queries that are not locate queries
    without forking since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively.

:classad-attribute:`HandleQueryForked`
    Number of queries that are not locate queries the Collector has
    handled by forking since it started. The Windows operating system
    does not fork and will not have this statistic.

:classad-attribute:`HandleQueryForkedRuntimeAvg`
    Total time spent forking to handle queries that are not locate
    queries since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively. The Windows operating
    system does not fork and will not have this statistic.

:classad-attribute:`HandleQueryMissedFork`
    Number of queries that are not locate queries the Collector recieved
    since the Collector started that could not be handled immediately
    because there were already too many forked child processes. The
    Windows operating system does not fork and will not have this
    statistic.

:classad-attribute:`HandleQueryMissedForkRuntimeAvg`
    Total time spent queueing pending non-locate queries that could not
    be immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.

:classad-attribute:`HostsClaimed`
    Description is not yet written.

:classad-attribute:`HostsOwner`
    Description is not yet written.

:classad-attribute:`HostsTotal`
    Description is not yet written.

:classad-attribute:`HostsUnclaimed`
    Description is not yet written.

:classad-attribute:`IdleJobs`
    Description is not yet written.

:classad-attribute:`Machine`
    A string with the machine's fully qualified host name.

:classad-attribute:`MaxJobsRunningAll`
    An integer value representing the sum of all
    ``MaxJobsRunning<universe>`` values.

:classad-attribute:`MaxJobsRunning`
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

:classad-attribute:`MyAddress`
    String with the IP and port address of the *condor_collector*
    daemon which is publishing this ClassAd.

:classad-attribute:`MyCurrentTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_schedd*
    daemon last sent a ClassAd update to the *condor_collector*.

:classad-attribute:`Name`
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:classad-attribute:`PeakForkWorkers`
    The maximum number of active forks of the Collector at any time
    since the Collector started. The Windows version of the Collector
    does not fork and will not have this statistic.

:classad-attribute:`PendingQueries`
    Number of queries pending that are waiting to fork.

:classad-attribute:`PendingQueriesPeak`
    Peak number of queries pending that are waiting to fork since
    collector startup or statistics reset.

:classad-attribute:`RunningJobs`
    Definition not yet written.

:classad-attribute:`StartdAds`
    The integer number of unique *condor_startd* daemon ClassAds
    counted at the most recent time the *condor_collector* updated its
    own ClassAd.

:classad-attribute:`StartdAdsPeak`
    The largest integer number of unique *condor_startd* daemon
    ClassAds seen at any one time, since the *condor_collector* began
    executing.

:classad-attribute:`SubmitterAds`
    The integer number of unique submitters counted at the most recent
    time the *condor_collector* updated its own ClassAd.

:classad-attribute:`SubmitterAdsPeak`
    The largest integer number of unique submitters seen at any one
    time, since the *condor_collector* began executing.

:classad-attribute:`UpdateInterval`
    Description is not yet written.

:classad-attribute:`UpdateSequenceNumber`
    An integer that begins at 0, and increments by one each time the
    same ClassAd is again advertised.

:classad-attribute:`UpdatesInitial`
    A Statistics attribute representing a count of unique ClassAds seen,
    over the lifetime of this *condor_collector*. Counts per ClassAd
    are advertised in attributes named by ClassAd type as
    ``UpdatesInitial_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.

:classad-attribute:`UpdatesLost`
    A Statistics attribute representing the count of updates lost, over
    the lifetime of this *condor_collector*. Counts per ClassAd are
    advertised in attributes named by ClassAd type as
    ``UpdatesLost_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.

:classad-attribute:`UpdatesLostMax`
    A Statistics attribute defining the largest number of updates lost
    at any point in time, over the lifetime of this *condor_collector*.
    ClassAd sequence numbers are used to detect lost ClassAds.

:classad-attribute:`UpdatesLostRatio`
    A Statistics attribute defining the floating point ratio of the
    total number of updates to the number of updates lost over the
    lifetime of this *condor_collector*. ClassAd sequence numbers are
    used to detect lost ClassAds. A value of 1 indicates that all
    ClassAds have been lost.

:classad-attribute:`UpdatesTotal`
    A Statistics attribute representing the count of the number of
    ClassAd updates received over the lifetime of this
    *condor_collector*. Counts per ClassAd are advertised in attributes
    named by ClassAd type as ``UpdatesTotal_<ClassAd-Name>``.
    ``<ClassAd-Name>`` is each of ``CkptSrvr``, ``Collector``,
    ``Defrag``, ``Master``, ``Schedd``, ``Start``, ``StartdPvt``, and
    ``Submittor``.
