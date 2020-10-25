Defrag ClassAd Attributes
=========================


:index:`Defrag attributes<single: Defrag attributes; ClassAd>`
:index:`AvgDrainingBadput<single: AvgDrainingBadput; ClassAd Defrag attribute>`

``AvgDrainingBadput``:
    Fraction of time CPUs in the pool have spent on jobs that were
    killed during draining of the machine. This is calculated in each
    polling interval by looking at ``TotalMachineDrainingBadput``.
    Therefore, it treats evictions of jobs that do and do not produce
    checkpoints the same. When the *condor_startd* restarts, its
    counters start over from 0, so the average is only over the time
    since the daemons have been alive.

:index:`AvgDrainingUnclaimedTime<single: AvgDrainingUnclaimedTime; ClassAd Defrag attribute>`

``AvgDrainingUnclaimedTime``:
    Fraction of time CPUs in the pool have spent unclaimed by a user
    during draining of the machine. This is calculated in each polling
    interval by looking at ``TotalMachineDrainingUnclaimedTime``. When
    the *condor_startd* restarts, its counters start over from 0, so
    the average is only over the time since the daemons have been alive.

:index:`DaemonStartTime<single: DaemonStartTime; ClassAd Defrag attribute>`

``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:index:`DaemonLastReconfigTime<single: DaemonLastReconfigTime; ClassAd Defrag attribute>`

``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:index:`DrainedMachines<single: DrainedMachines; ClassAd Defrag attribute>`

``DrainedMachines``:
    A count of the number of fully drained machines which have arrived
    during the run time of this *condor_defrag* daemon.

:index:`DrainFailures<single: DrainFailures; ClassAd Defrag attribute>`

``DrainFailures``:
    Total count of failed attempts to initiate draining during the
    lifetime of this *condor_defrag* daemon.

:index:`DrainSuccesses<single: DrainSuccesses; ClassAd Defrag attribute>`

``DrainSuccesses``:
    Total count of successful attempts to initiate draining during the
    lifetime of this *condor_defrag* daemon.

:index:`Machine<single: Machine; ClassAd Defrag attribute>`

``Machine``:
    A string with the machine's fully qualified host name.

:index:`MachinesDraining<single: MachinesDraining; ClassAd Defrag attribute>`

``MachinesDraining``:
    Number of machines that were observed to be draining in the last
    polling interval.

:index:`MachinesDrainingPeak<single: MachinesDrainingPeak; ClassAd Defrag attribute>`

``MachinesDrainingPeak``:
    Largest number of machines that were ever observed to be draining.

:index:`MeanDrainedArrived<single: MeanDrainedArrived; ClassAd Defrag attribute>`

``MeanDrainedArrived``:
    The mean time in seconds between arrivals of fully drained machines.

:index:`MonitorSelfAge<single: MonitorSelfAge; ClassAd Defrag attribute>`

``MonitorSelfAge``:
    The number of seconds that this daemon has been running.

:index:`MonitorSelfCPUUsage<single: MonitorSelfCPUUsage; ClassAd Defrag attribute>`

``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.

:index:`MonitorSelfImageSize<single: MonitorSelfImageSize; ClassAd Defrag attribute>`

``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in KiB.

:index:`MonitorSelfRegisteredSocketCount<single: MonitorSelfRegisteredSocketCount; ClassAd Defrag attribute>`

``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.

:index:`MonitorSelfResidentSetSize<single: MonitorSelfResidentSetSize; ClassAd Defrag attribute>`

``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in KiB.

:index:`MonitorSelfSecuritySessions<single: MonitorSelfSecuritySessions; ClassAd Defrag attribute>`

``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.

:index:`MonitorSelfTime<single: MonitorSelfTime; ClassAd Defrag attribute>`

``MonitorSelfTime``:
    The time, represented as the number of seconds elapsed since the
    Unix epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last
    checked and set the attributes with names that begin with the string
    ``MonitorSelf``.

:index:`MyAddress<single: MyAddress; ClassAd Defrag attribute>`

``MyAddress``:
    String with the IP and port address of the *condor_defrag* daemon
    which is publishing this ClassAd.

:index:`MyCurrentTime<single: MyCurrentTime; ClassAd Defrag attribute>`

``MyCurrentTime``:
    The time, represented as the number of seconds elapsed since the
    Unix epoch (00:00:00 UTC, Jan 1, 1970), at which the
    *condor_defrag* daemon last sent a ClassAd update to the
    *condor_collector*.

:index:`Name<single: Name; ClassAd Defrag attribute>`

``Name``:
    The name of this daemon; typically the same value as the ``Machine``
    attribute, but could be customized by the site administrator via the
    configuration variable ``DEFRAG_NAME`` :index:`DEFRAG_NAME`.

:index:`RecentCancelsList<single: RecentCancelsList; ClassAd Defrag attribute>`

``RecentCancelsList``:
    A ClassAd list of ClassAds describing the last ten cancel commands sent
    by this daemon.  Attributes include ``when``, as the number of seconds
    since the Unix epoch; and ``who``, the ``Name`` of the slot being drained.

:index:`RecentDrainFailures<single: RecentDrainFailures; ClassAd Defrag attribute>`

``RecentDrainFailures``:
    Count of failed attempts to initiate draining during the past
    ``RecentStatsLifetime`` seconds.

:index:`RecentDrainSuccesses<single: RecentDrainSuccesses; ClassAd Defrag attribute>`

``RecentDrainSuccesses``:
    Count of successful attempts to initiate draining during the past
    ``RecentStatsLifetime`` seconds.

:index:`RecentDrainsList<single: RecentDrainsList; ClassAd Defrag attribute>`

``RecentDrainsList``:
    A ClassAd list of ClassAds describing the last ten drain commands sent
    by this daemon.  Attributes include ``when``, as the number of seconds
    since the Unix epoch; ``who``, the ``Name`` of the slot being drained;
    and ``what``, one of the three strings ``graceful``, ``quick``, or
    ``fast``.

:index:`RecentStatsLifetime<single: RecentStatsLifetime; ClassAd Defrag attribute>`

``RecentStatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``.

:index:`UpdateSequenceNumber<single: UpdateSequenceNumber; ClassAd Defrag attribute>`

``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor_collector*. The *condor_collector* uses
    this value to sequence the updates it receives.

:index:`WholeMachines<single: WholeMachines; ClassAd Defrag attribute>`

``WholeMachines``:
    Number of machines that were observed to be defragmented in the last
    polling interval.

:index:`WholeMachinesPeak<single: WholeMachinesPeak; ClassAd Defrag attribute>`

``WholeMachinesPeak``:
    Largest number of machines that were ever observed to be
    simultaneously defragmented.
