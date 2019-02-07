      

Defrag ClassAd Attributes
=========================

 ``AvgDrainingBadput``:
    Fraction of time CPUs in the pool have spent on jobs that were
    killed during draining of the machine. This is calculated in each
    polling interval by looking at ``TotalMachineDrainingBadput``.
    Therefore, it treats evictions of jobs that do and do not produce
    checkpoints the same. When the *condor\_startd* restarts, its
    counters start over from 0, so the average is only over the time
    since the daemons have been alive.
 ``AvgDrainingUnclaimedTime``:
    Fraction of time CPUs in the pool have spent unclaimed by a user
    during draining of the machine. This is calculated in each polling
    interval by looking at ``TotalMachineDrainingUnclaimedTime``. When
    the *condor\_startd* restarts, its counters start over from 0, so
    the average is only over the time since the daemons have been alive.
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``DrainedMachines``:
    A count of the number of fully drained machines which have arrived
    during the run time of this *condor\_defrag* daemon.
 ``DrainFailures``:
    Total count of failed attempts to initiate draining during the
    lifetime of this *condor\_defrag* daemon.
 ``DrainSuccesses``:
    Total count of successful attempts to initiate draining during the
    lifetime of this *condor\_defrag* daemon.
 ``Machine``:
    A string with the machine’s fully qualified host name.
 ``MachinesDraining``:
    Number of machines that were observed to be draining in the last
    polling interval.
 ``MachinesDrainingPeak``:
    Largest number of machines that were ever observed to be draining.
 ``MeanDrainedArrived``:
    The mean time in seconds between arrivals of fully drained machines.
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in KiB.
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in KiB.
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
 ``MonitorSelfTime``:
    The time, represented as the number of seconds elapsed since the
    Unix epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last
    checked and set the attributes with names that begin with the string
    ``MonitorSelf``.
 ``MyAddress``:
    String with the IP and port address of the *condor\_defrag* daemon
    which is publishing this ClassAd.
 ``MyCurrentTime``:
    The time, represented as the number of seconds elapsed since the
    Unix epoch (00:00:00 UTC, Jan 1, 1970), at which the
    *condor\_defrag* daemon last sent a ClassAd update to the
    *condor\_collector*.
 ``Name``:
    The name of this daemon; typically the same value as the ``Machine``
    attribute, but could be customized by the site administrator via the
    configuration variable ``DEFRAG_NAME`` .
 ``RecentDrainFailures``:
    Count of failed attempts to initiate draining during the past
    ``RecentStatsLifetime`` seconds.
 ``RecentDrainSuccesses``:
    Count of successful attempts to initiate draining during the past
    ``RecentStatsLifetime`` seconds.
 ``RecentStatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``.
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.
 ``WholeMachines``:
    Number of machines that were observed to be defragmented in the last
    polling interval.
 ``WholeMachinesPeak``:
    Largest number of machines that were ever observed to be
    simultaneously defragmented.

      
