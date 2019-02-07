      

Collector ClassAd Attributes
============================

 ``ActiveQueryWorkers``:
    Current number of forked child processes handling queries.
 ``ActiveQueryWorkersPeak``:
    Peak number of forked child processes handling queries since
    collector startup or statistics reset.
 ``PendingQueries``:
    Number of queries pending that are waiting to fork.
 ``PendingQueriesPeak``:
    Peak number of queries pending that are waiting to fork since
    collector startup or statistics reset.
 ``DroppedQueries``:
    Total number of queries aborted since collector startup (or
    statistics reset) because ``COLLECTOR_QUERY_WORKERS_PENDING``
    exceeded, or ``COLLECTOR_QUERY_MAX_WORKTIME`` exceeded, or client
    closed TCP socket while request was pending. This statistic is also
    available as ``RecentDroppedQueries`` which represents a count of
    recently dropped queries that occured within a recent time window
    (default of 20 minutes).
 ``CollectorIpAddr``:
    String with the IP and port address of the *condor\_collector*
    daemon which is publishing this ClassAd.
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
 ``CondorVersion``:
    The current number of active forks of the Collector. The Windows
    version of the Collector does not fork and will not have this
    statistic.
 ``CurrentJobsRunningAll``:
    An integer value representing the sum of all jobs running under all
    universes.
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

 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``HandleLocate``:
    Number of locate queries the Collector has handled without forking
    since it started.
 ``HandleLocateRuntime``:
    Total time spent handling locate queries without forking since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively.
 ``HandleLocateForked``:
    Number of locate queries the Collector has handled by forking since
    it started. The Windows operating system does not fork and will not
    have this statistic.
 ``HandleLocateForkedRuntime``:
    Total time spent forking to handle locate queries since the
    Collector started. This attribute also has minimum, maximum, average
    and standard deviation statistics with Min, Max, Avg and Std
    suffixes respectively. The Windows operating system does not fork
    and will not have this statistic.
 ``HandleLocateMissedFork``:
    Number of locate queries the Collector recieved since the Collector
    started that could not be handled immediately because there were
    already too many forked child processes. The Windows operating
    system does not fork and will not have this statistic.
 ``HandleLocateMissedForkRuntime``:
    Total time spent queueing pending locate queries that could not be
    immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.
 ``HandleQuery``:
    Number of queries that are not locate queries the Collector has
    handled without forking since it started.
 ``HandleQueryRuntime``:
    Total time spent handling queries that are not locate queries
    without forking since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively.
 ``HandleQueryForked``:
    Number of queries that are not locate queries the Collector has
    handled by forking since it started. The Windows operating system
    does not fork and will not have this statistic.
 ``HandleQueryForkedRuntime``:
    Total time spent forking to handle queries that are not locate
    queries since the Collector started. This attribute also has
    minimum, maximum, average and standard deviation statistics with
    Min, Max, Avg and Std suffixes respectively. The Windows operating
    system does not fork and will not have this statistic.
 ``HandleQueryMissedFork``:
    Number of queries that are not locate queries the Collector recieved
    since the Collector started that could not be handled immediately
    because there were already too many forked child processes. The
    Windows operating system does not fork and will not have this
    statistic.
 ``HandleQueryMissedForkRuntime``:
    Total time spent queueing pending non-locate queries that could not
    be immediately handled by forking since the Collector started. This
    attribute also has minimum, maximum, average and standard deviation
    statistics with Min, Max, Avg and Std suffixes respectively. The
    Windows operating system does not fork and will not have this
    statistic.
 ``HostsClaimed``:
    Description is not yet written.
 ``HostsOwner``:
    Description is not yet written.
 ``HostsTotal``:
    Description is not yet written.
 ``HostsUnclaimed``:
    Description is not yet written.
 ``IdleJobs``:
    Description is not yet written.
 ``Machine``:
    A string with the machine’s fully qualified host name.
 ``MaxJobsRunning<universe``:
    An integer value representing the sum of all
    ``MaxJobsRunning<universe>`` values.
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

 ``MyAddress``:
    String with the IP and port address of the *condor\_collector*
    daemon which is publishing this ClassAd.
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_schedd*
    daemon last sent a ClassAd update to the *condor\_collector*.
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
 ``CondorVersion``:
    The maximum number of active forks of the Collector at any time
    since the Collector started. The Windows version of the Collector
    does not fork and will not have this statistic.
 ``RunningJobs``:
    Definition not yet written.
 ``StartdAds``:
    The integer number of unique *condor\_startd* daemon ClassAds
    counted at the most recent time the *condor\_collector* updated its
    own ClassAd.
 ``StartdAdsPeak``:
    The largest integer number of unique *condor\_startd* daemon
    ClassAds seen at any one time, since the *condor\_collector* began
    executing.
 ``SubmitterAds``:
    The integer number of unique submitters counted at the most recent
    time the *condor\_collector* updated its own ClassAd.
 ``SubmitterAdsPeak``:
    The largest integer number of unique submitters seen at any one
    time, since the *condor\_collector* began executing.
 ``UpdateInterval``:
    Description is not yet written.
 ``UpdateSequenceNumber``:
    An integer that begins at 0, and increments by one each time the
    same ClassAd is again advertised.
 ``UpdatesInitial``:
    A Statistics attribute representing a count of unique ClassAds seen,
    over the lifetime of this *condor\_collector*. Counts per ClassAd
    are advertised in attributes named by ClassAd type as
    ``UpdatesInitial_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.
 ``UpdatesLost``:
    A Statistics attribute representing the count of updates lost, over
    the lifetime of this *condor\_collector*. Counts per ClassAd are
    advertised in attributes named by ClassAd type as
    ``UpdatesLost_<ClassAd-Name>``. ``<ClassAd-Name>`` is each of
    ``CkptSrvr``, ``Collector``, ``Defrag``, ``Master``, ``Schedd``,
    ``Start``, ``StartdPvt``, and ``Submittor``.
 ``UpdatesLostMax``:
    A Statistics attribute defining the largest number of updates lost
    at any point in time, over the lifetime of this *condor\_collector*.
    ClassAd sequence numbers are used to detect lost ClassAds.
 ``UpdatesLostRatio``:
    A Statistics attribute defining the floating point ratio of the
    total number of updates to the number of updates lost over the
    lifetime of this *condor\_collector*. ClassAd sequence numbers are
    used to detect lost ClassAds. A value of 1 indicates that all
    ClassAds have been lost.
 ``UpdatesTotal``:
    A Statistics attribute representing the count of the number of
    ClassAd updates received over the lifetime of this
    *condor\_collector*. Counts per ClassAd are advertised in attributes
    named by ClassAd type as ``UpdatesTotal_<ClassAd-Name>``.
    ``<ClassAd-Name>`` is each of ``CkptSrvr``, ``Collector``,
    ``Defrag``, ``Master``, ``Schedd``, ``Start``, ``StartdPvt``, and
    ``Submittor``.

      
