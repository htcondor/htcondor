      

Collector ClassAd Attributes
============================

:index:`ClassAd<single: ClassAd; Collector attributes>`
:index:`ClassAd Collector attribute<single: ClassAd Collector attribute; ActiveQueryWorkers>`

 ``ActiveQueryWorkers``:
    Current number of forked child processes handling queries.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; ActiveQueryWorkersPeak>`
 ``ActiveQueryWorkersPeak``:
    Peak number of forked child processes handling queries since
    collector startup or statistics reset.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; PendingQueries>`
 ``PendingQueries``:
    Number of queries pending that are waiting to fork.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; PendingQueriesPeak>`
 ``PendingQueriesPeak``:
    Peak number of queries pending that are waiting to fork since
    collector startup or statistics reset.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; RecentDroppedQueries>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; DroppedQueries>`
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
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; CollectorIpAddr>`
 ``CollectorIpAddr``:
    String with the IP and port address of the *condor\_collector*
    daemon which is publishing this ClassAd.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; CondorVersion>`
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; CurrentForkWorkers>`
 ``CondorVersion``:
    The current number of active forks of the Collector. The Windows
    version of the Collector does not fork and will not have this
    statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; CurrentJobsRunningAll>`
 ``CurrentJobsRunningAll``:
    An integer value representing the sum of all jobs running under all
    universes.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; CurrentJobsRunning<universe>>`
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
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; DaemonStartTime>`

 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; DaemonLastReconfigTime>`
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocate>`
 ``HandleLocate``:
    Number of locate queries the Collector has handled without forking
    since it started.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateRuntimeAvg>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateRuntimeMax>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateRuntimeMin>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateRuntimeStd>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateRuntime>`
 ``HandleLocateRuntime``:
    Total time spent handling locate queries without forking since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateForked>`
 ``HandleLocateForked``:
    Number of locate queries the Collector has handled by forking since
    it started. The Windows operating system does not fork and will not
    have this statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateForkedRuntimeAvg>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateForkedRuntimeMax>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateForkedRuntimeMin>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateForkedRuntimeStd>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateForkedRuntime>`
 ``HandleLocateForkedRuntime``:
    Total time spent forking to handle locate queries since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively. The Windows operating system does not fork
    and will not have this statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateMissedFork>`
 ``HandleLocateMissedFork``:
    Number of locate queries the Collector recieved since the Collector
    started that could not be handled immediately because there were
    already too many forked child processes. The Windows operating
    system does not fork and will not have this statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateMissedForkRuntimeAvg>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateMissedForkRuntimeMax>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateMissedForkRuntimeMin>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateMissedForkRuntimeStd>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleLocateMissedForkRuntime>`
 ``HandleLocateMissedForkRuntime``:
    Total time spent queueing pending locate queries that could not be
    immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQuery>`
 ``HandleQuery``:
    Number of queries that are not locate queries the Collector has
    handled without forking since it started.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryRuntimeAvg>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryRuntimeMax>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryRuntimeMin>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryRuntimeStd>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryRuntime>`
 ``HandleQueryRuntime``:
    Total time spent handling queries that are not locate queries
    without forking since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryForked>`
 ``HandleQueryForked``:
    Number of queries that are not locate queries the Collector has
    handled by forking since it started. The Windows operating system
    does not fork and will not have this statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryForkedRuntimeAvg>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryForkedRuntimeMax>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryForkedRuntimeMin>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryForkedRuntimeStd>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryForkedRuntime>`
 ``HandleQueryForkedRuntime``:
    Total time spent forking to handle queries that are not locate
    queries since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively. The Windows operating
    system does not fork and will not have this statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryMissedFork>`
 ``HandleQueryMissedFork``:
    Number of queries that are not locate queries the Collector recieved
    since the Collector started that could not be handled immediately
    because there were already too many forked child processes. The
    Windows operating system does not fork and will not have this
    statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryMissedForkRuntimeAvg>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryMissedForkRuntimeMax>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryMissedForkRuntimeMin>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryMissedForkRuntimeStd>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HandleQueryMissedForkRuntime>`
 ``HandleQueryMissedForkRuntime``:
    Total time spent queueing pending non-locate queries that could not
    be immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HostsClaimed>`
 ``HostsClaimed``:
    Description is not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HostsOwner>`
 ``HostsOwner``:
    Description is not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HostsTotal>`
 ``HostsTotal``:
    Description is not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; HostsUnclaimed>`
 ``HostsUnclaimed``:
    Description is not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; IdleJobs>`
 ``IdleJobs``:
    Description is not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; Machine>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; MaxJobsRunningAll>`
 ``MaxJobsRunning<universe``:
    An integer value representing the sum of all
    ``MaxJobsRunning<universe>`` values.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; MaxJobsRunning<universe>>`
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
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; MyAddress>`

 ``MyAddress``:
    String with the IP and port address of the *condor\_collector*
    daemon which is publishing this ClassAd.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; MyCurrentTime>`
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_schedd*
    daemon last sent a ClassAd update to the *condor\_collector*.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; Name>`
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; PeakForkWorkers>`
 ``CondorVersion``:
    The maximum number of active forks of the Collector at any time
    since the Collector started. The Windows version of the Collector
    does not fork and will not have this statistic.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; RunningJobs>`
 ``RunningJobs``:
    Definition not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; StartdAds>`
 ``StartdAds``:
    The integer number of unique *condor\_startd* daemon ClassAds
    counted at the most recent time the *condor\_collector* updated its
    own ClassAd.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; StartdAdsPeak>`
 ``StartdAdsPeak``:
    The largest integer number of unique *condor\_startd* daemon
    ClassAds seen at any one time, since the *condor\_collector* began
    executing.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; SubmitterAds>`
 ``SubmitterAds``:
    The integer number of unique submitters counted at the most recent
    time the *condor\_collector* updated its own ClassAd.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; SubmitterAdsPeak>`
 ``SubmitterAdsPeak``:
    The largest integer number of unique submitters seen at any one
    time, since the *condor\_collector* began executing.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdateInterval>`
 ``UpdateInterval``:
    Description is not yet written.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdateSequenceNumber>`
 ``UpdateSequenceNumber``:
    An integer that begins at 0, and increments by one each time the
    same ClassAd is again advertised.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesInitial>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesInitial_<ClassAd-Name>>`
 ``UpdatesInitial``:
    A Statistics attribute representing a count of unique ClassAds seen,
    over the lifetime of this *condor\_collector*. Counts per ClassAd
    are advertised in attributes named by ClassAd type as
    ``UpdatesInitial_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesLost>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesLost_<ClassAd-Name>>`
 ``UpdatesLost``:
    A Statistics attribute representing the count of updates lost, over
    the lifetime of this *condor\_collector*. Counts per ClassAd are
    advertised in attributes named by ClassAd type as
    ``UpdatesLost_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesLostMax>`
 ``UpdatesLostMax``:
    A Statistics attribute defining the largest number of updates lost
    at any point in time, over the lifetime of this *condor\_collector*.
    ClassAd sequence numbers are used to detect lost ClassAds.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesLostRatio>`
 ``UpdatesLostRatio``:
    A Statistics attribute defining the floating point ratio of the
    total number of updates to the number of updates lost over the
    lifetime of this *condor\_collector*. ClassAd sequence numbers are
    used to detect lost ClassAds. A value of 1 indicates that all
    ClassAds have been lost.
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesTotal>`
    :index:`ClassAd Collector attribute<single: ClassAd Collector attribute; UpdatesTotal_<ClassAd-Name>>`
 ``UpdatesTotal``:
    A Statistics attribute representing the count of the number of
    ClassAd updates received over the lifetime of this
    *condor\_collector*. Counts per ClassAd are advertised in attributes
    named by ClassAd type as ``UpdatesTotal_<ClassAd-Name>``.
    ``<ClassAd-Name>`` is each of ``CkptSrvr``, ``Collector``,
    ``Defrag``, ``Master``, ``Schedd``, ``Start``, ``StartdPvt``, and
    ``Submittor``.

      
