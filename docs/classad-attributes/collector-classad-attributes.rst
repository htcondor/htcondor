Collector ClassAd Attributes
============================


:index:`Collector attributes<single: Collector attributes; ClassAd>`
:index:`ActiveQueryWorkers<single: ActiveQueryWorkers; ClassAd Collector attribute>`

``ActiveQueryWorkers``:
    Current number of forked child processes handling queries.

:index:`ActiveQueryWorkersPeak<single: ActiveQueryWorkersPeak; ClassAd Collector attribute>`

``ActiveQueryWorkersPeak``:
    Peak number of forked child processes handling queries since
    collector startup or statistics reset.

:index:`PendingQueries<single: PendingQueries; ClassAd Collector attribute>`

``PendingQueries``:
    Number of queries pending that are waiting to fork.

:index:`PendingQueriesPeak<single: PendingQueriesPeak; ClassAd Collector attribute>`

``PendingQueriesPeak``:
    Peak number of queries pending that are waiting to fork since
    collector startup or statistics reset.

:index:`RecentDroppedQueries<single: RecentDroppedQueries; ClassAd Collector attribute>`
:index:`DroppedQueries<single: DroppedQueries; ClassAd Collector attribute>`

``DroppedQueries``:
    Total number of queries aborted since collector startup (or
    statistics reset) because ``COLLECTOR_QUERY_WORKERS_PENDING``

:index:`COLLECTOR_QUERY_WORKERS_PENDING` exceeded, or
    ``COLLECTOR_QUERY_MAX_WORKTIME``

:index:`COLLECTOR_QUERY_MAX_WORKTIME` exceeded, or client
    closed TCP socket while request was pending. This statistic is also
    available as ``RecentDroppedQueries`` which represents a count of
    recently dropped queries that occured within a recent time window
    (default of 20 minutes).

:index:`CollectorIpAddr<single: CollectorIpAddr; ClassAd Collector attribute>`

``CollectorIpAddr``:
    String with the IP and port address of the *condor_collector*
    daemon which is publishing this ClassAd.

:index:`CondorVersion<single: CondorVersion; ClassAd Collector attribute>`

``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:index:`CurrentForkWorkers<single: CurrentForkWorkers; ClassAd Collector attribute>`

``CurrentForkWorkers``:
    The current number of active forks of the Collector. The Windows
    version of the Collector does not fork and will not have this
    statistic.

:index:`CurrentJobsRunningAll<single: CurrentJobsRunningAll; ClassAd Collector attribute>`

``CurrentJobsRunningAll``:
    An integer value representing the sum of all jobs running under all
    universes.

:index:`CurrentJobsRunning<single: CurrentJobsRunning; ClassAd Collector attribute>`

``CurrentJobsRunning<universe>``:
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

:index:`DaemonStartTime<single: DaemonStartTime; ClassAd Collector attribute>`

``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:index:`DaemonLastReconfigTime<single: DaemonLastReconfigTime; ClassAd Collector attribute>`

``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:index:`HandleLocate<single: HandleLocate; ClassAd Collector attribute>`

``HandleLocate``:
    Number of locate queries the Collector has handled without forking
    since it started.

:index:`HandleLocateRuntimeAvg<single: HandleLocateRuntimeAvg; ClassAd Collector attribute>`
:index:`HandleLocateRuntimeMax<single: HandleLocateRuntimeMax; ClassAd Collector attribute>`
:index:`HandleLocateRuntimeMin<single: HandleLocateRuntimeMin; ClassAd Collector attribute>`
:index:`HandleLocateRuntimeStd<single: HandleLocateRuntimeStd; ClassAd Collector attribute>`
:index:`HandleLocateRuntime<single: HandleLocateRuntime; ClassAd Collector attribute>`

``HandleLocateRuntime``:
    Total time spent handling locate queries without forking since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively.

:index:`HandleLocateForked<single: HandleLocateForked; ClassAd Collector attribute>`

``HandleLocateForked``:
    Number of locate queries the Collector has handled by forking since
    it started. The Windows operating system does not fork and will not
    have this statistic.

:index:`HandleLocateForkedRuntimeAvg<single: HandleLocateForkedRuntimeAvg; ClassAd Collector attribute>`
:index:`HandleLocateForkedRuntimeMax<single: HandleLocateForkedRuntimeMax; ClassAd Collector attribute>`
:index:`HandleLocateForkedRuntimeMin<single: HandleLocateForkedRuntimeMin; ClassAd Collector attribute>`
:index:`HandleLocateForkedRuntimeStd<single: HandleLocateForkedRuntimeStd; ClassAd Collector attribute>`
:index:`HandleLocateForkedRuntime<single: HandleLocateForkedRuntime; ClassAd Collector attribute>`

``HandleLocateForkedRuntime``:
    Total time spent forking to handle locate queries since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively. The Windows operating system does not fork
    and will not have this statistic.

:index:`HandleLocateMissedFork<single: HandleLocateMissedFork; ClassAd Collector attribute>`

``HandleLocateMissedFork``:
    Number of locate queries the Collector recieved since the Collector
    started that could not be handled immediately because there were
    already too many forked child processes. The Windows operating
    system does not fork and will not have this statistic.

:index:`HandleLocateMissedForkRuntimeAvg<single: HandleLocateMissedForkRuntimeAvg; ClassAd Collector attribute>`
:index:`HandleLocateMissedForkRuntimeMax<single: HandleLocateMissedForkRuntimeMax; ClassAd Collector attribute>`
:index:`HandleLocateMissedForkRuntimeMin<single: HandleLocateMissedForkRuntimeMin; ClassAd Collector attribute>`
:index:`HandleLocateMissedForkRuntimeStd<single: HandleLocateMissedForkRuntimeStd; ClassAd Collector attribute>`
:index:`HandleLocateMissedForkRuntime<single: HandleLocateMissedForkRuntime; ClassAd Collector attribute>`

``HandleLocateMissedForkRuntime``:
    Total time spent queueing pending locate queries that could not be
    immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.

:index:`HandleQuery<single: HandleQuery; ClassAd Collector attribute>`

``HandleQuery``:
    Number of queries that are not locate queries the Collector has
    handled without forking since it started.

:index:`HandleQueryRuntimeAvg<single: HandleQueryRuntimeAvg; ClassAd Collector attribute>`
:index:`HandleQueryRuntimeMax<single: HandleQueryRuntimeMax; ClassAd Collector attribute>`
:index:`HandleQueryRuntimeMin<single: HandleQueryRuntimeMin; ClassAd Collector attribute>`
:index:`HandleQueryRuntimeStd<single: HandleQueryRuntimeStd; ClassAd Collector attribute>`
:index:`HandleQueryRuntime<single: HandleQueryRuntime; ClassAd Collector attribute>`

``HandleQueryRuntime``:
    Total time spent handling queries that are not locate queries
    without forking since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively.

:index:`HandleQueryForked<single: HandleQueryForked; ClassAd Collector attribute>`

``HandleQueryForked``:
    Number of queries that are not locate queries the Collector has
    handled by forking since it started. The Windows operating system
    does not fork and will not have this statistic.

:index:`HandleQueryForkedRuntimeAvg<single: HandleQueryForkedRuntimeAvg; ClassAd Collector attribute>`
:index:`HandleQueryForkedRuntimeMax<single: HandleQueryForkedRuntimeMax; ClassAd Collector attribute>`
:index:`HandleQueryForkedRuntimeMin<single: HandleQueryForkedRuntimeMin; ClassAd Collector attribute>`
:index:`HandleQueryForkedRuntimeStd<single: HandleQueryForkedRuntimeStd; ClassAd Collector attribute>`
:index:`HandleQueryForkedRuntime<single: HandleQueryForkedRuntime; ClassAd Collector attribute>`

``HandleQueryForkedRuntime``:
    Total time spent forking to handle queries that are not locate
    queries since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively. The Windows operating
    system does not fork and will not have this statistic.

:index:`HandleQueryMissedFork<single: HandleQueryMissedFork; ClassAd Collector attribute>`

``HandleQueryMissedFork``:
    Number of queries that are not locate queries the Collector recieved
    since the Collector started that could not be handled immediately
    because there were already too many forked child processes. The
    Windows operating system does not fork and will not have this
    statistic.

:index:`HandleQueryMissedForkRuntimeAvg<single: HandleQueryMissedForkRuntimeAvg; ClassAd Collector attribute>`
:index:`HandleQueryMissedForkRuntimeMax<single: HandleQueryMissedForkRuntimeMax; ClassAd Collector attribute>`
:index:`HandleQueryMissedForkRuntimeMin<single: HandleQueryMissedForkRuntimeMin; ClassAd Collector attribute>`
:index:`HandleQueryMissedForkRuntimeStd<single: HandleQueryMissedForkRuntimeStd; ClassAd Collector attribute>`
:index:`HandleQueryMissedForkRuntime<single: HandleQueryMissedForkRuntime; ClassAd Collector attribute>`

``HandleQueryMissedForkRuntime``:
    Total time spent queueing pending non-locate queries that could not
    be immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.

:index:`HostsClaimed<single: HostsClaimed; ClassAd Collector attribute>`

``HostsClaimed``:
    Description is not yet written.

:index:`HostsOwner<single: HostsOwner; ClassAd Collector attribute>`

``HostsOwner``:
    Description is not yet written.

:index:`HostsTotal<single: HostsTotal; ClassAd Collector attribute>`

``HostsTotal``:
    Description is not yet written.

:index:`HostsUnclaimed<single: HostsUnclaimed; ClassAd Collector attribute>`

``HostsUnclaimed``:
    Description is not yet written.

:index:`IdleJobs<single: IdleJobs; ClassAd Collector attribute>`

``IdleJobs``:
    Description is not yet written.

:index:`Machine<single: Machine; ClassAd Collector attribute>`

``Machine``:
    A string with the machine's fully qualified host name.

:index:`MaxJobsRunningAll<single: MaxJobsRunningAll; ClassAd Collector attribute>`

``MaxJobsRunning<universe``:
    An integer value representing the sum of all
    ``MaxJobsRunning<universe>`` values.

:index:`MaxJobsRunning<single: MaxJobsRunning; ClassAd Collector attribute>`

``MaxJobsRunning<universe>``:
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

:index:`MyAddress<single: MyAddress; ClassAd Collector attribute>`

``MyAddress``:
    String with the IP and port address of the *condor_collector*
    daemon which is publishing this ClassAd.

:index:`MyCurrentTime<single: MyCurrentTime; ClassAd Collector attribute>`

``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_schedd*
    daemon last sent a ClassAd update to the *condor_collector*.

:index:`Name<single: Name; ClassAd Collector attribute>`

``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:index:`PeakForkWorkers<single: PeakForkWorkers; ClassAd Collector attribute>`

``CondorVersion``:
    The maximum number of active forks of the Collector at any time
    since the Collector started. The Windows version of the Collector
    does not fork and will not have this statistic.

:index:`RunningJobs<single: RunningJobs; ClassAd Collector attribute>`

``RunningJobs``:
    Definition not yet written.

:index:`StartdAds<single: StartdAds; ClassAd Collector attribute>`

``StartdAds``:
    The integer number of unique *condor_startd* daemon ClassAds
    counted at the most recent time the *condor_collector* updated its
    own ClassAd.

:index:`StartdAdsPeak<single: StartdAdsPeak; ClassAd Collector attribute>`

``StartdAdsPeak``:
    The largest integer number of unique *condor_startd* daemon
    ClassAds seen at any one time, since the *condor_collector* began
    executing.

:index:`SubmitterAds<single: SubmitterAds; ClassAd Collector attribute>`

``SubmitterAds``:
    The integer number of unique submitters counted at the most recent
    time the *condor_collector* updated its own ClassAd.

:index:`SubmitterAdsPeak<single: SubmitterAdsPeak; ClassAd Collector attribute>`

``SubmitterAdsPeak``:
    The largest integer number of unique submitters seen at any one
    time, since the *condor_collector* began executing.

:index:`UpdateInterval<single: UpdateInterval; ClassAd Collector attribute>`

``UpdateInterval``:
    Description is not yet written.

:index:`UpdateSequenceNumber<single: UpdateSequenceNumber; ClassAd Collector attribute>`

``UpdateSequenceNumber``:
    An integer that begins at 0, and increments by one each time the
    same ClassAd is again advertised.

:index:`UpdatesInitial<single: UpdatesInitial; ClassAd Collector attribute>`

``UpdatesInitial``:
    A Statistics attribute representing a count of unique ClassAds seen,
    over the lifetime of this *condor_collector*. Counts per ClassAd
    are advertised in attributes named by ClassAd type as
    ``UpdatesInitial_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.

:index:`UpdatesLost<single: UpdatesLost; ClassAd Collector attribute>`

``UpdatesLost``:
    A Statistics attribute representing the count of updates lost, over
    the lifetime of this *condor_collector*. Counts per ClassAd are
    advertised in attributes named by ClassAd type as
    ``UpdatesLost_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.

:index:`UpdatesLostMax<single: UpdatesLostMax; ClassAd Collector attribute>`

``UpdatesLostMax``:
    A Statistics attribute defining the largest number of updates lost
    at any point in time, over the lifetime of this *condor_collector*.
    ClassAd sequence numbers are used to detect lost ClassAds.

:index:`UpdatesLostRatio<single: UpdatesLostRatio; ClassAd Collector attribute>`

``UpdatesLostRatio``:
    A Statistics attribute defining the floating point ratio of the
    total number of updates to the number of updates lost over the
    lifetime of this *condor_collector*. ClassAd sequence numbers are
    used to detect lost ClassAds. A value of 1 indicates that all
    ClassAds have been lost.

:index:`UpdatesTotal<single: UpdatesTotal; ClassAd Collector attribute>`

``UpdatesTotal``:
    A Statistics attribute representing the count of the number of
    ClassAd updates received over the lifetime of this
    *condor_collector*. Counts per ClassAd are advertised in attributes
    named by ClassAd type as ``UpdatesTotal_<ClassAd-Name>``.
    ``<ClassAd-Name>`` is each of ``CkptSrvr``, ``Collector``,
    ``Defrag``, ``Master``, ``Schedd``, ``Start``, ``StartdPvt``, and
    ``Submittor``.


