      

Collector ClassAd Attributes
============================

:index:`Collector attributes;ClassAd<single: Collector attributes;ClassAd>`
:index:`ActiveQueryWorkers;ClassAd Collector attribute<single: ActiveQueryWorkers;ClassAd Collector attribute>`

 ``ActiveQueryWorkers``:
    Current number of forked child processes handling queries.
    :index:`ActiveQueryWorkersPeak;ClassAd Collector attribute<single: ActiveQueryWorkersPeak;ClassAd Collector attribute>`
 ``ActiveQueryWorkersPeak``:
    Peak number of forked child processes handling queries since
    collector startup or statistics reset.
    :index:`PendingQueries;ClassAd Collector attribute<single: PendingQueries;ClassAd Collector attribute>`
 ``PendingQueries``:
    Number of queries pending that are waiting to fork.
    :index:`PendingQueriesPeak;ClassAd Collector attribute<single: PendingQueriesPeak;ClassAd Collector attribute>`
 ``PendingQueriesPeak``:
    Peak number of queries pending that are waiting to fork since
    collector startup or statistics reset.
    :index:`RecentDroppedQueries;ClassAd Collector attribute<single: RecentDroppedQueries;ClassAd Collector attribute>`
    :index:`DroppedQueries;ClassAd Collector attribute<single: DroppedQueries;ClassAd Collector attribute>`
 ``DroppedQueries``:
    Total number of queries aborted since collector startup (or
    statistics reset) because ``COLLECTOR_QUERY_WORKERS_PENDING``
    :index:`COLLECTOR_QUERY_WORKERS_PENDING<single: COLLECTOR_QUERY_WORKERS_PENDING>` exceeded, or
    ``COLLECTOR_QUERY_MAX_WORKTIME``
    :index:`COLLECTOR_QUERY_MAX_WORKTIME<single: COLLECTOR_QUERY_MAX_WORKTIME>` exceeded, or client
    closed TCP socket while request was pending. This statistic is also
    available as ``RecentDroppedQueries`` which represents a count of
    recently dropped queries that occured within a recent time window
    (default of 20 minutes).
    :index:`CollectorIpAddr;ClassAd Collector attribute<single: CollectorIpAddr;ClassAd Collector attribute>`
 ``CollectorIpAddr``:
    String with the IP and port address of the *condor\_collector*
    daemon which is publishing this ClassAd.
    :index:`CondorVersion;ClassAd Collector attribute<single: CondorVersion;ClassAd Collector attribute>`
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
    :index:`CurrentForkWorkers;ClassAd Collector attribute<single: CurrentForkWorkers;ClassAd Collector attribute>`
 ``CondorVersion``:
    The current number of active forks of the Collector. The Windows
    version of the Collector does not fork and will not have this
    statistic.
    :index:`CurrentJobsRunningAll;ClassAd Collector attribute<single: CurrentJobsRunningAll;ClassAd Collector attribute>`
 ``CurrentJobsRunningAll``:
    An integer value representing the sum of all jobs running under all
    universes.
    :index:`CurrentJobsRunning<universe>;ClassAd Collector attribute<single: CurrentJobsRunning<universe>;ClassAd Collector attribute>`
 ``CurrentJobsRunning<universe>``:
    An integer value representing the current number of jobs running
    under the universe which forms the attribute name. For example

    ::

          CurrentJobsRunningVanilla = 567

    identifies that the *condor\_collector* counts 567 vanilla universe
    jobs currently running. ``<universe>`` is one of ``Unknown``,
    ``Standard``, ``Vanilla``, ``Scheduler``, ``Java``, ``Parallel``,
    ``VM``, or ``Local``. There are other universes, but they are not
    listed here, as they represent ones that are no longer used in
    Condor.
    :index:`DaemonStartTime;ClassAd Collector attribute<single: DaemonStartTime;ClassAd Collector attribute>`

 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`DaemonLastReconfigTime;ClassAd Collector attribute<single: DaemonLastReconfigTime;ClassAd Collector attribute>`
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`HandleLocate;ClassAd Collector attribute<single: HandleLocate;ClassAd Collector attribute>`
 ``HandleLocate``:
    Number of locate queries the Collector has handled without forking
    since it started.
    :index:`HandleLocateRuntimeAvg;ClassAd Collector attribute<single: HandleLocateRuntimeAvg;ClassAd Collector attribute>`
    :index:`HandleLocateRuntimeMax;ClassAd Collector attribute<single: HandleLocateRuntimeMax;ClassAd Collector attribute>`
    :index:`HandleLocateRuntimeMin;ClassAd Collector attribute<single: HandleLocateRuntimeMin;ClassAd Collector attribute>`
    :index:`HandleLocateRuntimeStd;ClassAd Collector attribute<single: HandleLocateRuntimeStd;ClassAd Collector attribute>`
    :index:`HandleLocateRuntime;ClassAd Collector attribute<single: HandleLocateRuntime;ClassAd Collector attribute>`
 ``HandleLocateRuntime``:
    Total time spent handling locate queries without forking since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively.
    :index:`HandleLocateForked;ClassAd Collector attribute<single: HandleLocateForked;ClassAd Collector attribute>`
 ``HandleLocateForked``:
    Number of locate queries the Collector has handled by forking since
    it started. The Windows operating system does not fork and will not
    have this statistic.
    :index:`HandleLocateForkedRuntimeAvg;ClassAd Collector attribute<single: HandleLocateForkedRuntimeAvg;ClassAd Collector attribute>`
    :index:`HandleLocateForkedRuntimeMax;ClassAd Collector attribute<single: HandleLocateForkedRuntimeMax;ClassAd Collector attribute>`
    :index:`HandleLocateForkedRuntimeMin;ClassAd Collector attribute<single: HandleLocateForkedRuntimeMin;ClassAd Collector attribute>`
    :index:`HandleLocateForkedRuntimeStd;ClassAd Collector attribute<single: HandleLocateForkedRuntimeStd;ClassAd Collector attribute>`
    :index:`HandleLocateForkedRuntime;ClassAd Collector attribute<single: HandleLocateForkedRuntime;ClassAd Collector attribute>`
 ``HandleLocateForkedRuntime``:
    Total time spent forking to handle locate queries since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively. The Windows operating system does not fork
    and will not have this statistic.
    :index:`HandleLocateMissedFork;ClassAd Collector attribute<single: HandleLocateMissedFork;ClassAd Collector attribute>`
 ``HandleLocateMissedFork``:
    Number of locate queries the Collector recieved since the Collector
    started that could not be handled immediately because there were
    already too many forked child processes. The Windows operating
    system does not fork and will not have this statistic.
    :index:`HandleLocateMissedForkRuntimeAvg;ClassAd Collector attribute<single: HandleLocateMissedForkRuntimeAvg;ClassAd Collector attribute>`
    :index:`HandleLocateMissedForkRuntimeMax;ClassAd Collector attribute<single: HandleLocateMissedForkRuntimeMax;ClassAd Collector attribute>`
    :index:`HandleLocateMissedForkRuntimeMin;ClassAd Collector attribute<single: HandleLocateMissedForkRuntimeMin;ClassAd Collector attribute>`
    :index:`HandleLocateMissedForkRuntimeStd;ClassAd Collector attribute<single: HandleLocateMissedForkRuntimeStd;ClassAd Collector attribute>`
    :index:`HandleLocateMissedForkRuntime;ClassAd Collector attribute<single: HandleLocateMissedForkRuntime;ClassAd Collector attribute>`
 ``HandleLocateMissedForkRuntime``:
    Total time spent queueing pending locate queries that could not be
    immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.
    :index:`HandleQuery;ClassAd Collector attribute<single: HandleQuery;ClassAd Collector attribute>`
 ``HandleQuery``:
    Number of queries that are not locate queries the Collector has
    handled without forking since it started.
    :index:`HandleQueryRuntimeAvg;ClassAd Collector attribute<single: HandleQueryRuntimeAvg;ClassAd Collector attribute>`
    :index:`HandleQueryRuntimeMax;ClassAd Collector attribute<single: HandleQueryRuntimeMax;ClassAd Collector attribute>`
    :index:`HandleQueryRuntimeMin;ClassAd Collector attribute<single: HandleQueryRuntimeMin;ClassAd Collector attribute>`
    :index:`HandleQueryRuntimeStd;ClassAd Collector attribute<single: HandleQueryRuntimeStd;ClassAd Collector attribute>`
    :index:`HandleQueryRuntime;ClassAd Collector attribute<single: HandleQueryRuntime;ClassAd Collector attribute>`
 ``HandleQueryRuntime``:
    Total time spent handling queries that are not locate queries
    without forking since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively.
    :index:`HandleQueryForked;ClassAd Collector attribute<single: HandleQueryForked;ClassAd Collector attribute>`
 ``HandleQueryForked``:
    Number of queries that are not locate queries the Collector has
    handled by forking since it started. The Windows operating system
    does not fork and will not have this statistic.
    :index:`HandleQueryForkedRuntimeAvg;ClassAd Collector attribute<single: HandleQueryForkedRuntimeAvg;ClassAd Collector attribute>`
    :index:`HandleQueryForkedRuntimeMax;ClassAd Collector attribute<single: HandleQueryForkedRuntimeMax;ClassAd Collector attribute>`
    :index:`HandleQueryForkedRuntimeMin;ClassAd Collector attribute<single: HandleQueryForkedRuntimeMin;ClassAd Collector attribute>`
    :index:`HandleQueryForkedRuntimeStd;ClassAd Collector attribute<single: HandleQueryForkedRuntimeStd;ClassAd Collector attribute>`
    :index:`HandleQueryForkedRuntime;ClassAd Collector attribute<single: HandleQueryForkedRuntime;ClassAd Collector attribute>`
 ``HandleQueryForkedRuntime``:
    Total time spent forking to handle queries that are not locate
    queries since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively. The Windows operating
    system does not fork and will not have this statistic.
    :index:`HandleQueryMissedFork;ClassAd Collector attribute<single: HandleQueryMissedFork;ClassAd Collector attribute>`
 ``HandleQueryMissedFork``:
    Number of queries that are not locate queries the Collector recieved
    since the Collector started that could not be handled immediately
    because there were already too many forked child processes. The
    Windows operating system does not fork and will not have this
    statistic.
    :index:`HandleQueryMissedForkRuntimeAvg;ClassAd Collector attribute<single: HandleQueryMissedForkRuntimeAvg;ClassAd Collector attribute>`
    :index:`HandleQueryMissedForkRuntimeMax;ClassAd Collector attribute<single: HandleQueryMissedForkRuntimeMax;ClassAd Collector attribute>`
    :index:`HandleQueryMissedForkRuntimeMin;ClassAd Collector attribute<single: HandleQueryMissedForkRuntimeMin;ClassAd Collector attribute>`
    :index:`HandleQueryMissedForkRuntimeStd;ClassAd Collector attribute<single: HandleQueryMissedForkRuntimeStd;ClassAd Collector attribute>`
    :index:`HandleQueryMissedForkRuntime;ClassAd Collector attribute<single: HandleQueryMissedForkRuntime;ClassAd Collector attribute>`
 ``HandleQueryMissedForkRuntime``:
    Total time spent queueing pending non-locate queries that could not
    be immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.
    :index:`HostsClaimed;ClassAd Collector attribute<single: HostsClaimed;ClassAd Collector attribute>`
 ``HostsClaimed``:
    Description is not yet written.
    :index:`HostsOwner;ClassAd Collector attribute<single: HostsOwner;ClassAd Collector attribute>`
 ``HostsOwner``:
    Description is not yet written.
    :index:`HostsTotal;ClassAd Collector attribute<single: HostsTotal;ClassAd Collector attribute>`
 ``HostsTotal``:
    Description is not yet written.
    :index:`HostsUnclaimed;ClassAd Collector attribute<single: HostsUnclaimed;ClassAd Collector attribute>`
 ``HostsUnclaimed``:
    Description is not yet written.
    :index:`IdleJobs;ClassAd Collector attribute<single: IdleJobs;ClassAd Collector attribute>`
 ``IdleJobs``:
    Description is not yet written.
    :index:`Machine;ClassAd Collector attribute<single: Machine;ClassAd Collector attribute>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`MaxJobsRunningAll;ClassAd Collector attribute<single: MaxJobsRunningAll;ClassAd Collector attribute>`
 ``MaxJobsRunning<universe``:
    An integer value representing the sum of all
    ``MaxJobsRunning<universe>`` values.
    :index:`MaxJobsRunning<universe>;ClassAd Collector attribute<single: MaxJobsRunning<universe>;ClassAd Collector attribute>`
 ``MaxJobsRunning<universe>``:
    An integer value representing largest number of currently running
    jobs ever seen under the universe which forms the attribute name,
    over the life of this *condor\_collector* process. For example

    ::

          MaxJobsRunningVanilla = 401

    identifies that the *condor\_collector* saw 401 vanilla universe
    jobs currently running at one point in time, and that was the
    largest number it had encountered. ``<universe>`` is one of
    ``Unknown``, ``Standard``, ``Vanilla``, ``Scheduler``, ``Java``,
    ``Parallel``, ``VM``, or ``Local``. There are other universes, but
    they are not listed here, as they represent ones that are no longer
    used in Condor.
    :index:`MyAddress;ClassAd Collector attribute<single: MyAddress;ClassAd Collector attribute>`

 ``MyAddress``:
    String with the IP and port address of the *condor\_collector*
    daemon which is publishing this ClassAd.
    :index:`MyCurrentTime;ClassAd Collector attribute<single: MyCurrentTime;ClassAd Collector attribute>`
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_schedd*
    daemon last sent a ClassAd update to the *condor\_collector*.
    :index:`Name;ClassAd Collector attribute<single: Name;ClassAd Collector attribute>`
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
    :index:`PeakForkWorkers;ClassAd Collector attribute<single: PeakForkWorkers;ClassAd Collector attribute>`
 ``CondorVersion``:
    The maximum number of active forks of the Collector at any time
    since the Collector started. The Windows version of the Collector
    does not fork and will not have this statistic.
    :index:`RunningJobs;ClassAd Collector attribute<single: RunningJobs;ClassAd Collector attribute>`
 ``RunningJobs``:
    Definition not yet written.
    :index:`StartdAds;ClassAd Collector attribute<single: StartdAds;ClassAd Collector attribute>`
 ``StartdAds``:
    The integer number of unique *condor\_startd* daemon ClassAds
    counted at the most recent time the *condor\_collector* updated its
    own ClassAd.
    :index:`StartdAdsPeak;ClassAd Collector attribute<single: StartdAdsPeak;ClassAd Collector attribute>`
 ``StartdAdsPeak``:
    The largest integer number of unique *condor\_startd* daemon
    ClassAds seen at any one time, since the *condor\_collector* began
    executing.
    :index:`SubmitterAds;ClassAd Collector attribute<single: SubmitterAds;ClassAd Collector attribute>`
 ``SubmitterAds``:
    The integer number of unique submitters counted at the most recent
    time the *condor\_collector* updated its own ClassAd.
    :index:`SubmitterAdsPeak;ClassAd Collector attribute<single: SubmitterAdsPeak;ClassAd Collector attribute>`
 ``SubmitterAdsPeak``:
    The largest integer number of unique submitters seen at any one
    time, since the *condor\_collector* began executing.
    :index:`UpdateInterval;ClassAd Collector attribute<single: UpdateInterval;ClassAd Collector attribute>`
 ``UpdateInterval``:
    Description is not yet written.
    :index:`UpdateSequenceNumber;ClassAd Collector attribute<single: UpdateSequenceNumber;ClassAd Collector attribute>`
 ``UpdateSequenceNumber``:
    An integer that begins at 0, and increments by one each time the
    same ClassAd is again advertised.
    :index:`UpdatesInitial;ClassAd Collector attribute<single: UpdatesInitial;ClassAd Collector attribute>`
    ` <index://UpdatesInitial_<ClassAd-Name>;ClassAd Collector attribute>`__
 ``UpdatesInitial``:
    A Statistics attribute representing a count of unique ClassAds seen,
    over the lifetime of this *condor\_collector*. Counts per ClassAd
    are advertised in attributes named by ClassAd type as
    ``UpdatesInitial_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.
    :index:`UpdatesLost;ClassAd Collector attribute<single: UpdatesLost;ClassAd Collector attribute>`
    ` <index://UpdatesLost_<ClassAd-Name>;ClassAd Collector attribute>`__
 ``UpdatesLost``:
    A Statistics attribute representing the count of updates lost, over
    the lifetime of this *condor\_collector*. Counts per ClassAd are
    advertised in attributes named by ClassAd type as
    ``UpdatesLost_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.
    :index:`UpdatesLostMax;ClassAd Collector attribute<single: UpdatesLostMax;ClassAd Collector attribute>`
 ``UpdatesLostMax``:
    A Statistics attribute defining the largest number of updates lost
    at any point in time, over the lifetime of this *condor\_collector*.
    ClassAd sequence numbers are used to detect lost ClassAds.
    :index:`UpdatesLostRatio;ClassAd Collector attribute<single: UpdatesLostRatio;ClassAd Collector attribute>`
 ``UpdatesLostRatio``:
    A Statistics attribute defining the floating point ratio of the
    total number of updates to the number of updates lost over the
    lifetime of this *condor\_collector*. ClassAd sequence numbers are
    used to detect lost ClassAds. A value of 1 indicates that all
    ClassAds have been lost.
    :index:`UpdatesTotal;ClassAd Collector attribute<single: UpdatesTotal;ClassAd Collector attribute>`
    ` <index://UpdatesTotal_<ClassAd-Name>;ClassAd Collector attribute>`__
 ``UpdatesTotal``:
    A Statistics attribute representing the count of the number of
    ClassAd updates received over the lifetime of this
    *condor\_collector*. Counts per ClassAd are advertised in attributes
    named by ClassAd type as ``UpdatesTotal_<ClassAd-Name>``.
    ``<ClassAd-Name>`` is each of ``CkptSrvr``, ``Collector``,
    ``Defrag``, ``Master``, ``Schedd``, ``Start``, ``StartdPvt``, and
    ``Submittor``.

      
