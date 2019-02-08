      

Scheduler ClassAd Attributes
============================

 ``Autoclusters``:
    A Statistics attribute defining the number of active autoclusters.
 ``CollectorHost``:
    The name of the main *condor\_collector* which this *condor\_schedd*
    daemon reports to, as copied from ``COLLECTOR_HOST`` . If a
    *condor\_schedd* flocks to other *condor\_collector* daemons, this
    attribute still represents the "home" *condor\_collector*, so this
    value can be used to discover if a *condor\_schedd* is currently
    flocking.
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
 ``DaemonCoreDutyCycle``:
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time for the time period defined
    by ``StatsLifetime`` of this *condor\_schedd*. A value near 0.0
    indicates an idle daemon, while a value near 1.0 indicates a daemon
    running at or above capacity.
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``DetectedCpus``:
    The number of detected machine CPUs/cores.
 ``DetectedMemory``:
    The amount of detected machine RAM in MBytes.
 ``JobQueueBirthdate``:
    Description is not yet written.
 ``JobsAccumBadputTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully have spent running over the
    lifetime of this *condor\_schedd*.
 ``JobsAccumExceptionalBadputTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully due to *condor\_shadow*
    exceptions have spent running over the lifetime of this
    *condor\_schedd*.
 ``JobsAccumRunningTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    have spent running in the time interval defined by attribute
    ``StatsLifetime``.
 ``JobsAccumTimeToStart``:
    A Statistics attribute defining the sum of all the time jobs have
    spent waiting to start in the time interval defined by attribute
    ``StatsLifetime``.
 ``JobsBadputRuntimes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, over
    the lifetime of this *condor\_schedd*. Counts within the histogram
    are separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.
 ``JobsBadputSizes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, over the
    lifetime of this *condor\_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.
 ``JobsCheckpointed``:
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor\_shadow* exit code of ``JOB_CKPTED`` in the
    time interval defined by attribute ``StatsLifetime``.
 ``JobsCompleted``:
    A Statistics attribute defining the number of jobs successfully
    completed in the time interval defined by attribute
    ``StatsLifetime``.
 ``JobsCompletedRuntimes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by time spent running, over the
    lifetime of this *condor\_schedd*. Counts within the histogram are
    separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.
 ``JobsCompletedSizes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by image size, over the
    lifetime of this *condor\_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.
 ``JobsCoredumped``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_COREDUMPED`` in
    the time interval defined by attribute ``StatsLifetime``.
 ``JobsDebugLogError``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``DPRINTF_ERROR`` in the
    time interval defined by attribute ``StatsLifetime``.
 ``JobsExecFailed``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the time interval defined by attribute ``StatsLifetime``.
 ``JobsExited``:
    A Statistics attribute defining the number of times that jobs that
    exited (successfully or not) in the time interval defined by
    attribute ``StatsLifetime``.
 ``JobsExitedAndClaimClosing``:
    A Statistics attribute defining the number of times jobs have exited
    with a *condor\_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time interval defined by
    attribute ``StatsLifetime``.
 ``JobsExitedNormally``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time
    interval defined by attribute ``StatsLifetime``.
 ``JobsExitException``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the time interval defined by attribute
    ``StatsLifetime``.
 ``JobsKilled``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_KILLED`` in the
    time interval defined by attribute ``StatsLifetime``.
 ``JobsMissedDeferralTime``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the time interval defined by
    attribute ``StatsLifetime``.
 ``JobsNotStarted``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_NOT_STARTED`` in
    the time interval defined by attribute ``StatsLifetime``.
 ``JobsRestartReconnectsAttempting``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* is currently attempting to reconnect
    to, in order to recover a job that was running when the
    *condor\_schedd* was restarted.
 ``JobsRestartReconnectsBadput``:
    A Statistics attribute defining a histogram count of
    *condor\_startd* daemons that the *condor\_schedd* could not
    reconnect to in order to recover a job that was running when the
    *condor\_schedd* was restarted, as classified by the time the job
    spent running. Counts within the histogram are separated by a comma
    and a space, where the time interval classification is defined in
    the ClassAd attribute ``JobsRuntimesHistogramBuckets``.
 ``JobsRestartReconnectsFailed``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* tried and failed to reconnect to in
    order to recover a job that was running when the *condor\_schedd*
    was restarted.
 ``JobsRestartReconnectsInterrupted``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* attempted to reconnect to, in order to
    recover a job that was running when the *condor\_schedd* was
    restarted, but the attempt was interrupted, for example, because the
    job was removed.
 ``JobsRestartReconnectsLeaseExpired``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* could not attempt to reconnect to, in
    order to recover a job that was running when the *condor\_schedd*
    was restarted, because the job lease had already expired.
 ``JobsRestartReconnectsSucceeded``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* has successfully reconnected to, in
    order to recover a job that was running when the *condor\_schedd*
    was restarted.
 ``JobsRunning``:
    A Statistics attribute representing the number of jobs currently
    running.
 ``JobsRunningRuntimes``:
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by elapsed runtime. Counts within the
    histogram are separated by a comma and a space, where the time
    interval classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.
 ``JobsRunningSizes``:
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by image size. Counts within the histogram
    are separated by a comma and a space, where the size classification
    is defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.
 ``JobsRuntimesHistogramBuckets``:
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify run times. Defined as

    ::

          JobsRuntimesHistogramBuckets = "30Sec, 1Min, 3Min, 10Min, 30Min, 1Hr, 3Hr, 
                  6Hr, 12Hr, 1Day, 2Day, 4Day, 8Day, 16Day"

 ``JobsShadowNoMemory``:
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor\_shadow* in the time interval defined by attribute
    ``StatsLifetime``.
 ``JobsShouldHold``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the time interval defined by attribute ``StatsLifetime``.
 ``JobsShouldRemove``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the time interval defined by attribute ``StatsLifetime``.
 ``JobsShouldRequeue``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the time interval defined by attribute ``StatsLifetime``.
 ``JobsSizesHistogramBuckets``:
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify image sizes. Defined as

    ::

          JobsSizesHistogramBuckets = "64Kb, 256Kb, 1Mb, 4Mb, 16Mb, 64Mb, 256Mb, 
                  1Gb, 4Gb, 16Gb, 64Gb, 256Gb"

    Note that these values imply powers of two in numbers of bytes.

 ``JobsStarted``:
    A Statistics attribute defining the number of jobs started in the
    time interval defined by attribute ``StatsLifetime``.
 ``JobsSubmitted``:
    A Statistics attribute defining the number of jobs submitted in the
    time interval defined by attribute ``StatsLifetime``.
 ``Machine``:
    A string with the machine’s fully qualified host name.
 ``MaxJobsRunning``:
    The same integer value as set by the evaluation of the configuration
    variable ``MAX_JOBS_RUNNING`` . See the definition at
    section \ `3.5.9 <ConfigurationMacros.html#x33-1960003.5.9>`__ on
    page \ `687 <ConfigurationMacros.html#x33-1960003.5.9>`__.
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in Kbytes.
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in Kbytes.
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
 ``MonitorSelfTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.
 ``MyAddress``:
    String with the IP and port address of the *condor\_schedd* daemon
    which is publishing this ClassAd.
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
 ``NumJobStartsDelayed``:
    The number times a job requiring a *condor\_shadow* daemon could
    have been started, but was not started because of the values of
    configuration variables ``JOB_START_COUNT`` and ``JOB_START_DELAY``
    .
 ``NumPendingClaims``:
    The number of machines (*condor\_startd* daemons) matched to this
    *condor\_schedd* daemon, which this *condor\_schedd* knows about,
    but has not yet managed to claim.
 ``NumUsers``:
    The integer number of distinct users with jobs in this
    *condor\_schedd*\ ’s queue.
 ``PublicNetworkIpAddr``:
    Description is not yet written.
 ``RecentDaemonCoreDutyCycle``:
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time in the previous time
    interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsAccumBadputTime``:
    A Statistics attribute defining the sum of the all of the time that
    jobs which did not complete successfully have spent running in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsAccumRunningTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` spent running.
 ``RecentJobsAccumTimeToStart``:
    A Statistics attribute defining the sum of all the time jobs which
    have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` had spent waiting to start.
 ``RecentJobsBadputRuntimes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``. Counts within the histogram are separated
    by a comma and a space, where the time interval classification is
    defined in the ClassAd attribute ``JobsRuntimesHistogramBuckets``.
 ``RecentJobsBadputSizes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the size classification is defined in the ClassAd attribute
    ``JobsSizesHistogramBuckets``.
 ``RecentJobsCheckpointed``:
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor\_shadow* exit code of ``JOB_CKPTED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsCompleted``:
    A Statistics attribute defining the number of jobs successfully
    completed in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsCompletedRuntimes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by time spent running, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the time interval classification is defined in the ClassAd
    attribute ``JobsRuntimesHistogramBuckets``.
 ``RecentJobsCompletedSizes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by image size, in the previous
    time interval defined by attribute ``RecentStatsLifetime``. Counts
    within the histogram are separated by a comma and a space, where the
    size classification is defined in the ClassAd attribute
    ``JobsSizesHistogramBuckets``.
 ``RecentJobsCoredumped``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_COREDUMPED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsDebugLogError``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``DPRINTF_ERROR`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsExecFailed``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsExited``:
    A Statistics attribute defining the number of times that jobs have
    exited normally in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsExitedAndClaimClosing``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous time interval
    defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsExitedNormally``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous
    time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsExitException``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the previous time interval defined by
    attribute ``RecentStatsLifetime``.
 ``RecentJobsKilled``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_KILLED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsMissedDeferralTime``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the previous time interval defined
    by attribute ``RecentStatsLifetime``.
 ``RecentJobsNotStarted``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_NOT_STARTED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsShadowNoMemory``:
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor\_shadow* in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsShouldHold``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsShouldRemove``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsShouldRequeue``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentJobsStarted``:
    A Statistics attribute defining the number of jobs started in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentJobsSubmitted``:
    A Statistics attribute defining the number of jobs submitted in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
 ``RecentShadowsReconnections``:
    A Statistics attribute defining the number of times that
    *condor\_shadow* daemons lost connection to their *condor\_starter*
    daemons and successfully reconnected in the previous time interval
    defined by attribute ``RecentStatsLifetime``. This statistic only
    appears in the Scheduler ClassAd if the level of verbosity set by
    the configuration variable ``STATISTICS_TO_PUBLISH`` is set to 2 or
    higher.
 ``RecentShadowsRecycled``:
    A Statistics attribute defining the number of times *condor\_shadow*
    processes have been recycled for use with a new job in the previous
    time interval defined by attribute ``RecentStatsLifetime``. This
    statistic only appears in the Scheduler ClassAd if the level of
    verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
 ``RecentShadowsStarted``:
    A Statistics attribute defining the number of *condor\_shadow*
    daemons started in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
 ``RecentStatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``. This value starts at 0, and it may grow to a
    value as large as the value defined for attribute
    ``RecentWindowMax``.
 ``RecentStatsTickTime``:
    A Statistics attribute defining the time that attributes with names
    that begin with ``Recent`` were last updated, represented as the
    number of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1,
    1970). This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
 ``RecentWindowMax``:
    A Statistics attribute defining the maximum time in seconds over
    which attributes with names that begin with ``Recent`` are
    collected. The value is set by the configuration variable
    ``STATISTICS_WINDOW_SECONDS`` , which defaults to 1200 seconds (20
    minutes). This statistic only appears in the Scheduler ClassAd if
    the level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
 ``ScheddIpAddr``:
    String with the IP and port address of the *condor\_schedd* daemon
    which is publishing this Scheduler ClassAd.
 ``ServerTime``:
    Description is not yet written.
 ``ShadowsReconnections``:
    A Statistics attribute defining the number of times
    *condor\_shadow*\ s lost connection to their *condor\_starter*\ s
    and successfully reconnected in the previous ``StatsLifetime``
    seconds. This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
 ``ShadowsRecycled``:
    A Statistics attribute defining the number of times *condor\_shadow*
    processes have been recycled for use with a new job in the previous
    ``StatsLifetime`` seconds. This statistic only appears in the
    Scheduler ClassAd if the level of verbosity set by the configuration
    variable ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
 ``ShadowsRunning``:
    A Statistics attribute defining the number of *condor\_shadow*
    daemons currently running that are owned by this *condor\_schedd*.
 ``ShadowsRunningPeak``:
    A Statistics attribute defining the maximum number of
    *condor\_shadow* daemons running at one time that were owned by this
    *condor\_schedd* over the lifetime of this *condor\_schedd*.
 ``ShadowsStarted``:
    A Statistics attribute defining the number of *condor\_shadow*
    daemons started in the previous time interval defined by attribute
    ``StatsLifetime``.
 ``StartLocalUniverse``:
    The same boolean value as set in the configuration variable
    ``START_LOCAL_UNIVERSE`` . See the definition at
    section \ `3.5.9 <ConfigurationMacros.html#x33-1960003.5.9>`__ on
    page \ `684 <ConfigurationMacros.html#x33-1960003.5.9>`__.
 ``StartSchedulerUniverse``:
    The same boolean value as set in the configuration variable
    ``START_SCHEDULER_UNIVERSE`` . See the definition at
    section \ `3.5.9 <ConfigurationMacros.html#x33-1960003.5.9>`__ on
    page \ `686 <ConfigurationMacros.html#x33-1960003.5.9>`__.
 ``StatsLastUpdateTime``:
    A Statistics attribute defining the time that statistics about jobs
    were last updated, represented as the number of seconds elapsed
    since the Unix epoch (00:00:00 UTC, Jan 1, 1970). This statistic
    only appears in the Scheduler ClassAd if the level of verbosity set
    by the configuration variable ``STATISTICS_TO_PUBLISH`` is set to 2
    or higher.
 ``StatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics have been collected for attributes with names that do not
    begin with ``Recent``. This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
 ``TotalFlockedJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently flocked to other pools.
 ``TotalHeldJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently on hold.
 ``TotalIdleJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently idle, not including local or scheduler universe jobs.
 ``TotalJobAds``:
    The total number of all jobs (in all states) from this
    *condor\_schedd* daemon.
 ``TotalLocalJobsIdle``:
    The total number of **local** **universe** jobs from this
    *condor\_schedd* daemon that are currently idle.
 ``TotalLocalJobsRunning``:
    The total number of **local** **universe** jobs from this
    *condor\_schedd* daemon that are currently running.
 ``TotalRemovedJobs``:
    The current number of all running jobs from this *condor\_schedd*
    daemon that have remove requests.
 ``TotalRunningJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently running, not including local or scheduler universe jobs.
 ``TotalSchedulerJobsIdle``:
    The total number of **scheduler** **universe** jobs from this
    *condor\_schedd* daemon that are currently idle.
 ``TotalSchedulerJobsRunning``:
    The total number of **scheduler** **universe** jobs from this
    *condor\_schedd* daemon that are currently running.
 ``TransferQueueUserExpr``
    A ClassAd expression that provides the name of the transfer queue
    that the *condor\_schedd* will be using for job file transfer.
 ``UpdateInterval``:
    The interval, in seconds, between publication of this
    *condor\_schedd* ClassAd and the previous publication.
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.
 ``VirtualMemory``:
    Description is not yet written.
 ``WantResAd``:
    A boolean value that when ``True`` causes the *condor\_negotiator*
    daemon to send to this *condor\_schedd* daemon a full machine
    ClassAd corresponding to a matched job.

When using file transfer concurrency limits, the following additional
I/O usage statistics are published. These includes the sum and rate of
bytes transferred as well as time spent reading and writing to files and
to the network. These statistics are reported for the sum of all users
and may also be reported individually for recently active users by
increasing the verbosity level ``STATISTICS_TO_PUBLISH = TRANSFER:2``.
Each of the per-user statistics is prefixed by a user name in the form
``Owner_<username>_FileTransferUploadBytes``. In this case, the
attribute represents activity by the specified user. The published user
name is actually the file transfer queue name, as defined by
configuration variable ``TRANSFER_QUEUE_USER_EXPR`` . This expression
defaults to ``Owner_`` followed by the name of the job owner. The
attributes that are rates have a suffix that specifies the time span of
the exponential moving average. By default the time spans that are
published are 1m, 5m, 1h, and 1d. This can be changed by configuring
configuration variable ``TRANSFER_IO_REPORT_TIMESPANS`` . These
attributes are only reported once a full time span has accumulated.

 ``FileTransferDiskThrottleExcess_<timespan>``
    The exponential moving average of the disk load that exceeds the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
 ``FileTransferDiskThrottleHigh``
    The desired upper limit for the disk load from file transfers, as
    configured by ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` . This attribute
    is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
 ``FileTransferDiskThrottleLevel``
    The current concurrency limit set by the disk load throttle. The
    limit is applied to the sum of uploads and downloads. This attribute
    is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
 ``FileTransferDiskThrottleLow``
    The lower limit for the disk load from file transfers, as configured
    by ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` . This attribute is
    published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
 ``FileTransferDiskThrottleShortfall_<timespan>``
    The exponential moving average of the disk load that falls below the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
 ``FileTransferDownloadBytes``
    Total number of bytes downloaded as output from jobs since this
    *condor\_schedd* was started. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferDownloadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferDownloadBytesPerSecond_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which bytes have been downloaded as output from jobs. The time
    spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS`` , which defaults to 1m, 5m, 1h, and
    1d. When less than one full time span has accumulated, the attribute
    is not published. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferDownloadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferFileReadLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time reading
    from files to be transferred as input to jobs. One file transfer
    process spending nearly all of its time reading files will generate
    a load close to 1.0. The time spans that are published are
    configured by ``TRANSFER_IO_REPORT_TIMESPANS`` , which defaults to
    1m, 5m, 1h, and 1d. When less than one full time span has
    accumulated, the attribute is not published. If
    ``STATISTICS_TO_PUBLISH`` contains ``TRANSFER:2``, for each active
    user, this attribute is also published prefixed by the user name,
    with the name
    ``Owner_<username>_FileTransferFileReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferFileReadSeconds``
    Total number of submit-side transfer process seconds spent reading
    from files to be transferred as input to jobs since this
    *condor\_schedd* was started. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferFileReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferFileWriteLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time writing
    to files transferred as output from jobs. One file transfer process
    spending nearly all of its time writing to files will generate a
    load close to 1.0. The time spans that are published are configured
    by ``TRANSFER_IO_REPORT_TIMESPANS`` , which defaults to 1m, 5m, 1h,
    and 1d. When less than one full time span has accumulated, the
    attribute is not published. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferFileWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferFileWriteSeconds``
    Total number of submit-side transfer process seconds spent writing
    to files transferred as output from jobs since this *condor\_schedd*
    was started. If ``STATISTICS_TO_PUBLISH`` contains ``TRANSFER:2``,
    for each active user, this attribute is also published prefixed by
    the user name, with the name
    ``Owner_<username>_FileTransferFileWriteSeconds``. The published
    user name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferNetReadLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time reading
    from the network when transferring output from jobs. One file
    transfer process spending nearly all of its time reading from the
    network will generate a load close to 1.0. The reason a file
    transfer process may spend a long time writing to the network could
    be a network bottleneck on the path between the submit and execute
    machine. It could also be caused by slow reads from the disk on the
    execute side. The time spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS`` , which defaults to 1m, 5m, 1h, and
    1d. When less than one full time span has accumulated, the attribute
    is not published. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferNetReadSeconds``
    Total number of submit-side transfer process seconds spent reading
    from the network when transferring output from jobs since this
    *condor\_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow reads from the disk on the execute
    side. If ``STATISTICS_TO_PUBLISH`` contains ``TRANSFER:2``, for each
    active user, this attribute is also published prefixed by the user
    name, with the name ``Owner_<username>_FileTransferNetReadSeconds``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferNetWriteLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time writing
    to the network when transferring input to jobs. One file transfer
    process spending nearly all of its time writing to the network will
    generate a load close to 1.0. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow writes to the disk on the execute side.
    The time spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS``, which defaults to 1m, 5m, 1h, and
    1d. When less than one full time span has accumulated, the attribute
    is not published. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferNetWriteSeconds``
    Total number of submit-side transfer process seconds spent writing
    to the network when transferring input to jobs since this
    *condor\_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow writes to the disk on the execute side.
    The time spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS`` , which defaults to 1m, 5m, 1h, and
    1d. When less than one full time span has accumulated, the attribute
    is not published. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetWriteSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferUploadBytes``
    Total number of bytes uploaded as input to jobs since this
    *condor\_schedd* was started. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferUploadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``FileTransferUploadBytesPerSecond_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which bytes have been uploaded as input to jobs. The time spans
    that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS`` , which defaults to 1m, 5m, 1h, and
    1d. When less than one full time span has accumulated, the attribute
    is not published. If ``STATISTICS_TO_PUBLISH`` contains
    ``TRANSFER:2``, for each active user, this attribute is also
    published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferUploadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR`` .
 ``TransferQueueMBWaitingToDownload``
    Number of megabytes of output files waiting to be downloaded.
 ``TransferQueueMBWaitingToUpload``
    Number of megabytes of input files waiting to be uploaded.
 ``TransferQueueNumWaitingToDownload``
    Number of jobs waiting to transfer output files.
 ``TransferQueueNumWaitingToUpload``
    Number of jobs waiting to transfer input files.

      
