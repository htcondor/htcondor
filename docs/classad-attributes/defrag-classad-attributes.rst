      

Defrag ClassAd Attributes
=========================

:index:`ClassAd<single: ClassAd; Defrag attributes>`
:index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; AvgDrainingBadput>`

 ``AvgDrainingBadput``:
    Fraction of time CPUs in the pool have spent on jobs that were
    killed during draining of the machine. This is calculated in each
    polling interval by looking at ``TotalMachineDrainingBadput``.
    Therefore, it treats evictions of jobs that do and do not produce
    checkpoints the same. When the *condor\_startd* restarts, its
    counters start over from 0, so the average is only over the time
    since the daemons have been alive.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; AvgDrainingUnclaimedTime>`
 ``AvgDrainingUnclaimedTime``:
    Fraction of time CPUs in the pool have spent unclaimed by a user
    during draining of the machine. This is calculated in each polling
    interval by looking at ``TotalMachineDrainingUnclaimedTime``. When
    the *condor\_startd* restarts, its counters start over from 0, so
    the average is only over the time since the daemons have been alive.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; DaemonStartTime>`
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; DaemonLastReconfigTime>`
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; DrainedMachines>`
 ``DrainedMachines``:
    A count of the number of fully drained machines which have arrived
    during the run time of this *condor\_defrag* daemon.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; DrainFailures>`
 ``DrainFailures``:
    Total count of failed attempts to initiate draining during the
    lifetime of this *condor\_defrag* daemon.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; DrainSuccesses>`
 ``DrainSuccesses``:
    Total count of successful attempts to initiate draining during the
    lifetime of this *condor\_defrag* daemon.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; Machine>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MachinesDraining>`
 ``MachinesDraining``:
    Number of machines that were observed to be draining in the last
    polling interval.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MachinesDrainingPeak>`
 ``MachinesDrainingPeak``:
    Largest number of machines that were ever observed to be draining.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MeanDrainedArrived>`
 ``MeanDrainedArrived``:
    The mean time in seconds between arrivals of fully drained machines.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfAge>`
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfCPUUsage>`
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfImageSize>`
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in KiB.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfRegisteredSocketCount>`
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfResidentSetSize>`
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in KiB.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfSecuritySessions>`
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MonitorSelfTime>`
 ``MonitorSelfTime``:
    The time, represented as the number of seconds elapsed since the
    Unix epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last
    checked and set the attributes with names that begin with the string
    ``MonitorSelf``.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MyAddress>`
 ``MyAddress``:
    String with the IP and port address of the *condor\_defrag* daemon
    which is publishing this ClassAd.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; MyCurrentTime>`
 ``MyCurrentTime``:
    The time, represented as the number of seconds elapsed since the
    Unix epoch (00:00:00 UTC, Jan 1, 1970), at which the
    *condor\_defrag* daemon last sent a ClassAd update to the
    *condor\_collector*.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; Name>`
 ``Name``:
    The name of this daemon; typically the same value as the ``Machine``
    attribute, but could be customized by the site administrator via the
    configuration variable ``DEFRAG_NAME`` :index:`DEFRAG_NAME<single: DEFRAG_NAME>`.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; RecentDrainFailures>`
 ``RecentDrainFailures``:
    Count of failed attempts to initiate draining during the past
    ``RecentStatsLifetime`` seconds.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; RecentDrainSuccesses>`
 ``RecentDrainSuccesses``:
    Count of successful attempts to initiate draining during the past
    ``RecentStatsLifetime`` seconds.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; RecentStatsLifetime>`
 ``RecentStatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; UpdateSequenceNumber>`
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; WholeMachines>`
 ``WholeMachines``:
    Number of machines that were observed to be defragmented in the last
    polling interval.
    :index:`ClassAd Defrag attribute<single: ClassAd Defrag attribute; WholeMachinesPeak>`
 ``WholeMachinesPeak``:
    Largest number of machines that were ever observed to be
    simultaneously defragmented.

      
