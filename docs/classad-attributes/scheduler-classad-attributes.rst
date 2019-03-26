      

Scheduler ClassAd Attributes
============================

:index:`Scheduler attributes;ClassAd<single: Scheduler attributes;ClassAd>`
:index:`Autoclusters;ClassAd Scheduler attribute<single: Autoclusters;ClassAd Scheduler attribute>`

 ``Autoclusters``:
    A Statistics attribute defining the number of active autoclusters.
    :index:`CollectorHost;ClassAd Scheduler attribute<single: CollectorHost;ClassAd Scheduler attribute>`
 ``CollectorHost``:
    The name of the main *condor\_collector* which this *condor\_schedd*
    daemon reports to, as copied from ``COLLECTOR_HOST``
    :index:`COLLECTOR_HOST<single: COLLECTOR_HOST>`. If a *condor\_schedd* flocks to other
    *condor\_collector* daemons, this attribute still represents the
    "home" *condor\_collector*, so this value can be used to discover if
    a *condor\_schedd* is currently flocking.
    :index:`CondorVersion;ClassAd Scheduler attribute<single: CondorVersion;ClassAd Scheduler attribute>`
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
    :index:`DaemonCoreDutyCycle;ClassAd Scheduler attribute<single: DaemonCoreDutyCycle;ClassAd Scheduler attribute>`
 ``DaemonCoreDutyCycle``:
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time for the time period defined
    by ``StatsLifetime`` of this *condor\_schedd*. A value near 0.0
    indicates an idle daemon, while a value near 1.0 indicates a daemon
    running at or above capacity.
    :index:`DaemonStartTime;ClassAd Scheduler attribute<single: DaemonStartTime;ClassAd Scheduler attribute>`
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`DaemonLastReconfigTime;ClassAd Scheduler attribute<single: DaemonLastReconfigTime;ClassAd Scheduler attribute>`
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`DetectedCpus;ClassAd Scheduler attribute<single: DetectedCpus;ClassAd Scheduler attribute>`
 ``DetectedCpus``:
    The number of detected machine CPUs/cores.
    :index:`DetectedMemory;ClassAd Scheduler attribute<single: DetectedMemory;ClassAd Scheduler attribute>`
 ``DetectedMemory``:
    The amount of detected machine RAM in MBytes.
    :index:`JobQueueBirthdate;ClassAd Scheduler attribute<single: JobQueueBirthdate;ClassAd Scheduler attribute>`
 ``JobQueueBirthdate``:
    Description is not yet written.
    :index:`JobsAccumBadputTime;ClassAd Scheduler attribute<single: JobsAccumBadputTime;ClassAd Scheduler attribute>`
 ``JobsAccumBadputTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully have spent running over the
    lifetime of this *condor\_schedd*.
    :index:`JobsAccumExceptionalBadputTime;ClassAd Scheduler attribute<single: JobsAccumExceptionalBadputTime;ClassAd Scheduler attribute>`
 ``JobsAccumExceptionalBadputTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully due to *condor\_shadow*
    exceptions have spent running over the lifetime of this
    *condor\_schedd*.
    :index:`JobsAccumRunningTime;ClassAd Scheduler attribute<single: JobsAccumRunningTime;ClassAd Scheduler attribute>`
 ``JobsAccumRunningTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    have spent running in the time interval defined by attribute
    ``StatsLifetime``.
    :index:`JobsAccumTimeToStart;ClassAd Scheduler attribute<single: JobsAccumTimeToStart;ClassAd Scheduler attribute>`
 ``JobsAccumTimeToStart``:
    A Statistics attribute defining the sum of all the time jobs have
    spent waiting to start in the time interval defined by attribute
    ``StatsLifetime``.
    :index:`JobsBadputRuntimes;ClassAd Scheduler attribute<single: JobsBadputRuntimes;ClassAd Scheduler attribute>`
 ``JobsBadputRuntimes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, over
    the lifetime of this *condor\_schedd*. Counts within the histogram
    are separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.
    :index:`JobsBadputSizes;ClassAd Scheduler attribute<single: JobsBadputSizes;ClassAd Scheduler attribute>`
 ``JobsBadputSizes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, over the
    lifetime of this *condor\_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.
    :index:`JobsCheckpointed;ClassAd Scheduler attribute<single: JobsCheckpointed;ClassAd Scheduler attribute>`
 ``JobsCheckpointed``:
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor\_shadow* exit code of ``JOB_CKPTED`` in the
    time interval defined by attribute ``StatsLifetime``.
    :index:`JobsCompleted;ClassAd Scheduler attribute<single: JobsCompleted;ClassAd Scheduler attribute>`
 ``JobsCompleted``:
    A Statistics attribute defining the number of jobs successfully
    completed in the time interval defined by attribute
    ``StatsLifetime``.
    :index:`JobsCompletedRuntimes;ClassAd Scheduler attribute<single: JobsCompletedRuntimes;ClassAd Scheduler attribute>`
 ``JobsCompletedRuntimes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by time spent running, over the
    lifetime of this *condor\_schedd*. Counts within the histogram are
    separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.
    :index:`JobsCompletedSizes;ClassAd Scheduler attribute<single: JobsCompletedSizes;ClassAd Scheduler attribute>`
 ``JobsCompletedSizes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by image size, over the
    lifetime of this *condor\_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.
    :index:`JobsCoredumped;ClassAd Scheduler attribute<single: JobsCoredumped;ClassAd Scheduler attribute>`
 ``JobsCoredumped``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_COREDUMPED`` in
    the time interval defined by attribute ``StatsLifetime``.
    :index:`JobsDebugLogError;ClassAd Scheduler attribute<single: JobsDebugLogError;ClassAd Scheduler attribute>`
 ``JobsDebugLogError``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``DPRINTF_ERROR`` in the
    time interval defined by attribute ``StatsLifetime``.
    :index:`JobsExecFailed;ClassAd Scheduler attribute<single: JobsExecFailed;ClassAd Scheduler attribute>`
 ``JobsExecFailed``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the time interval defined by attribute ``StatsLifetime``.
    :index:`JobsExited;ClassAd Scheduler attribute<single: JobsExited;ClassAd Scheduler attribute>`
 ``JobsExited``:
    A Statistics attribute defining the number of times that jobs that
    exited (successfully or not) in the time interval defined by
    attribute ``StatsLifetime``.
    :index:`JobsExitedAndClaimClosing;ClassAd Scheduler attribute<single: JobsExitedAndClaimClosing;ClassAd Scheduler attribute>`
 ``JobsExitedAndClaimClosing``:
    A Statistics attribute defining the number of times jobs have exited
    with a *condor\_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time interval defined by
    attribute ``StatsLifetime``.
    :index:`JobsExitedNormally;ClassAd Scheduler attribute<single: JobsExitedNormally;ClassAd Scheduler attribute>`
 ``JobsExitedNormally``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time
    interval defined by attribute ``StatsLifetime``.
    :index:`JobsExitException;ClassAd Scheduler attribute<single: JobsExitException;ClassAd Scheduler attribute>`
 ``JobsExitException``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the time interval defined by attribute
    ``StatsLifetime``.
    :index:`JobsKilled;ClassAd Scheduler attribute<single: JobsKilled;ClassAd Scheduler attribute>`
 ``JobsKilled``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_KILLED`` in the
    time interval defined by attribute ``StatsLifetime``.
    :index:`JobsMissedDeferralTime;ClassAd Scheduler attribute<single: JobsMissedDeferralTime;ClassAd Scheduler attribute>`
 ``JobsMissedDeferralTime``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the time interval defined by
    attribute ``StatsLifetime``.
    :index:`JobsNotStarted;ClassAd Scheduler attribute<single: JobsNotStarted;ClassAd Scheduler attribute>`
 ``JobsNotStarted``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_NOT_STARTED`` in
    the time interval defined by attribute ``StatsLifetime``.
    :index:`JobsRestartReconnectsAttempting;ClassAd Scheduler attribute<single: JobsRestartReconnectsAttempting;ClassAd Scheduler attribute>`
 ``JobsRestartReconnectsAttempting``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* is currently attempting to reconnect
    to, in order to recover a job that was running when the
    *condor\_schedd* was restarted.
    :index:`JobsRestartReconnectsBadput;ClassAd Scheduler attribute<single: JobsRestartReconnectsBadput;ClassAd Scheduler attribute>`
 ``JobsRestartReconnectsBadput``:
    A Statistics attribute defining a histogram count of
    *condor\_startd* daemons that the *condor\_schedd* could not
    reconnect to in order to recover a job that was running when the
    *condor\_schedd* was restarted, as classified by the time the job
    spent running. Counts within the histogram are separated by a comma
    and a space, where the time interval classification is defined in
    the ClassAd attribute ``JobsRuntimesHistogramBuckets``.
    :index:`JobsRestartReconnectsFailed;ClassAd Scheduler attribute<single: JobsRestartReconnectsFailed;ClassAd Scheduler attribute>`
 ``JobsRestartReconnectsFailed``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* tried and failed to reconnect to in
    order to recover a job that was running when the *condor\_schedd*
    was restarted.
    :index:`JobsRestartReconnectsInterrupted;ClassAd Scheduler attribute<single: JobsRestartReconnectsInterrupted;ClassAd Scheduler attribute>`
 ``JobsRestartReconnectsInterrupted``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* attempted to reconnect to, in order to
    recover a job that was running when the *condor\_schedd* was
    restarted, but the attempt was interrupted, for example, because the
    job was removed.
    :index:`JobsRestartReconnectsLeaseExpired;ClassAd Scheduler attribute<single: JobsRestartReconnectsLeaseExpired;ClassAd Scheduler attribute>`
 ``JobsRestartReconnectsLeaseExpired``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* could not attempt to reconnect to, in
    order to recover a job that was running when the *condor\_schedd*
    was restarted, because the job lease had already expired.
    :index:`JobsRestartReconnectsSucceeded;ClassAd Scheduler attribute<single: JobsRestartReconnectsSucceeded;ClassAd Scheduler attribute>`
 ``JobsRestartReconnectsSucceeded``:
    A Statistics attribute defining the number of *condor\_startd*
    daemons the *condor\_schedd* has successfully reconnected to, in
    order to recover a job that was running when the *condor\_schedd*
    was restarted.
    :index:`JobsRunning;ClassAd Scheduler attribute<single: JobsRunning;ClassAd Scheduler attribute>`
 ``JobsRunning``:
    A Statistics attribute representing the number of jobs currently
    running.
    :index:`JobsRunningRuntimes;ClassAd Scheduler attribute<single: JobsRunningRuntimes;ClassAd Scheduler attribute>`
 ``JobsRunningRuntimes``:
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by elapsed runtime. Counts within the
    histogram are separated by a comma and a space, where the time
    interval classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.
    :index:`JobsRunningSizes;ClassAd Scheduler attribute<single: JobsRunningSizes;ClassAd Scheduler attribute>`
 ``JobsRunningSizes``:
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by image size. Counts within the histogram
    are separated by a comma and a space, where the size classification
    is defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.
    :index:`JobsRuntimesHistogramBuckets;ClassAd Scheduler attribute<single: JobsRuntimesHistogramBuckets;ClassAd Scheduler attribute>`
 ``JobsRuntimesHistogramBuckets``:
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify run times. Defined as

    ::

          JobsRuntimesHistogramBuckets = "30Sec, 1Min, 3Min, 10Min, 30Min, 1Hr, 3Hr, 
                  6Hr, 12Hr, 1Day, 2Day, 4Day, 8Day, 16Day"

    :index:`JobsShadowNoMemory;ClassAd Scheduler attribute<single: JobsShadowNoMemory;ClassAd Scheduler attribute>`

 ``JobsShadowNoMemory``:
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor\_shadow* in the time interval defined by attribute
    ``StatsLifetime``.
    :index:`JobsShouldHold;ClassAd Scheduler attribute<single: JobsShouldHold;ClassAd Scheduler attribute>`
 ``JobsShouldHold``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the time interval defined by attribute ``StatsLifetime``.
    :index:`JobsShouldRemove;ClassAd Scheduler attribute<single: JobsShouldRemove;ClassAd Scheduler attribute>`
 ``JobsShouldRemove``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the time interval defined by attribute ``StatsLifetime``.
    :index:`JobsShouldRequeue;ClassAd Scheduler attribute<single: JobsShouldRequeue;ClassAd Scheduler attribute>`
 ``JobsShouldRequeue``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the time interval defined by attribute ``StatsLifetime``.
    :index:`JobsSizesHistogramBuckets;ClassAd Scheduler attribute<single: JobsSizesHistogramBuckets;ClassAd Scheduler attribute>`
 ``JobsSizesHistogramBuckets``:
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify image sizes. Defined as

    ::

          JobsSizesHistogramBuckets = "64Kb, 256Kb, 1Mb, 4Mb, 16Mb, 64Mb, 256Mb, 
                  1Gb, 4Gb, 16Gb, 64Gb, 256Gb"

    Note that these values imply powers of two in numbers of bytes.
    :index:`JobsStarted;ClassAd Scheduler attribute<single: JobsStarted;ClassAd Scheduler attribute>`

 ``JobsStarted``:
    A Statistics attribute defining the number of jobs started in the
    time interval defined by attribute ``StatsLifetime``.
    :index:`JobsSubmitted;ClassAd Scheduler attribute<single: JobsSubmitted;ClassAd Scheduler attribute>`
 ``JobsSubmitted``:
    A Statistics attribute defining the number of jobs submitted in the
    time interval defined by attribute ``StatsLifetime``.
    :index:`Machine;ClassAd Scheduler attribute<single: Machine;ClassAd Scheduler attribute>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`MaxJobsRunning;ClassAd Scheduler attribute<single: MaxJobsRunning;ClassAd Scheduler attribute>`
 ``MaxJobsRunning``:
    The same integer value as set by the evaluation of the configuration
    variable ``MAX_JOBS_RUNNING`` :index:`MAX_JOBS_RUNNING<single: MAX_JOBS_RUNNING>`. See
    the definition at section \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__ on
    page \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`MonitorSelfAge;ClassAd Scheduler attribute<single: MonitorSelfAge;ClassAd Scheduler attribute>`
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
    :index:`MonitorSelfCPUUsage;ClassAd Scheduler attribute<single: MonitorSelfCPUUsage;ClassAd Scheduler attribute>`
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
    :index:`MonitorSelfImageSize;ClassAd Scheduler attribute<single: MonitorSelfImageSize;ClassAd Scheduler attribute>`
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in Kbytes.
    :index:`MonitorSelfRegisteredSocketCount;ClassAd Scheduler attribute<single: MonitorSelfRegisteredSocketCount;ClassAd Scheduler attribute>`
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
    :index:`MonitorSelfResidentSetSize;ClassAd Scheduler attribute<single: MonitorSelfResidentSetSize;ClassAd Scheduler attribute>`
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in Kbytes.
    :index:`MonitorSelfSecuritySessions;ClassAd Scheduler attribute<single: MonitorSelfSecuritySessions;ClassAd Scheduler attribute>`
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
    :index:`MonitorSelfTime;ClassAd Scheduler attribute<single: MonitorSelfTime;ClassAd Scheduler attribute>`
 ``MonitorSelfTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.
    :index:`MyAddress;ClassAd Scheduler attribute<single: MyAddress;ClassAd Scheduler attribute>`
 ``MyAddress``:
    String with the IP and port address of the *condor\_schedd* daemon
    which is publishing this ClassAd.
    :index:`MyCurrentTime;ClassAd Scheduler attribute<single: MyCurrentTime;ClassAd Scheduler attribute>`
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_schedd*
    daemon last sent a ClassAd update to the *condor\_collector*.
    :index:`Name;ClassAd Scheduler attribute<single: Name;ClassAd Scheduler attribute>`
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
    :index:`NumJobStartsDelayed;ClassAd Scheduler attribute<single: NumJobStartsDelayed;ClassAd Scheduler attribute>`
 ``NumJobStartsDelayed``:
    The number times a job requiring a *condor\_shadow* daemon could
    have been started, but was not started because of the values of
    configuration variables ``JOB_START_COUNT``
    :index:`JOB_START_COUNT<single: JOB_START_COUNT>` and ``JOB_START_DELAY``
    :index:`JOB_START_DELAY<single: JOB_START_DELAY>`.
    :index:`NumPendingClaims;ClassAd Scheduler attribute<single: NumPendingClaims;ClassAd Scheduler attribute>`
 ``NumPendingClaims``:
    The number of machines (*condor\_startd* daemons) matched to this
    *condor\_schedd* daemon, which this *condor\_schedd* knows about,
    but has not yet managed to claim.
    :index:`NumUsers;ClassAd Scheduler attribute<single: NumUsers;ClassAd Scheduler attribute>`
 ``NumUsers``:
    The integer number of distinct users with jobs in this
    *condor\_schedd*\ ’s queue.
    :index:`PublicNetworkIpAddr;ClassAd Scheduler attribute<single: PublicNetworkIpAddr;ClassAd Scheduler attribute>`
 ``PublicNetworkIpAddr``:
    Description is not yet written.
    :index:`RecentDaemonCoreDutyCycle;ClassAd Scheduler attribute<single: RecentDaemonCoreDutyCycle;ClassAd Scheduler attribute>`
 ``RecentDaemonCoreDutyCycle``:
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time in the previous time
    interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsAccumBadputTime;ClassAd Scheduler attribute<single: RecentJobsAccumBadputTime;ClassAd Scheduler attribute>`
 ``RecentJobsAccumBadputTime``:
    A Statistics attribute defining the sum of the all of the time that
    jobs which did not complete successfully have spent running in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsAccumRunningTime;ClassAd Scheduler attribute<single: RecentJobsAccumRunningTime;ClassAd Scheduler attribute>`
 ``RecentJobsAccumRunningTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` spent running.
    :index:`RecentJobsAccumTimeToStart;ClassAd Scheduler attribute<single: RecentJobsAccumTimeToStart;ClassAd Scheduler attribute>`
 ``RecentJobsAccumTimeToStart``:
    A Statistics attribute defining the sum of all the time jobs which
    have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` had spent waiting to start.
    :index:`RecentJobsBadputRuntimes;ClassAd Scheduler attribute<single: RecentJobsBadputRuntimes;ClassAd Scheduler attribute>`
 ``RecentJobsBadputRuntimes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``. Counts within the histogram are separated
    by a comma and a space, where the time interval classification is
    defined in the ClassAd attribute ``JobsRuntimesHistogramBuckets``.
    :index:`RecentJobsBadputSizes;ClassAd Scheduler attribute<single: RecentJobsBadputSizes;ClassAd Scheduler attribute>`
 ``RecentJobsBadputSizes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the size classification is defined in the ClassAd attribute
    ``JobsSizesHistogramBuckets``.
    :index:`RecentJobsCheckpointed;ClassAd Scheduler attribute<single: RecentJobsCheckpointed;ClassAd Scheduler attribute>`
 ``RecentJobsCheckpointed``:
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor\_shadow* exit code of ``JOB_CKPTED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsCompleted;ClassAd Scheduler attribute<single: RecentJobsCompleted;ClassAd Scheduler attribute>`
 ``RecentJobsCompleted``:
    A Statistics attribute defining the number of jobs successfully
    completed in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsCompletedRuntimes;ClassAd Scheduler attribute<single: RecentJobsCompletedRuntimes;ClassAd Scheduler attribute>`
 ``RecentJobsCompletedRuntimes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by time spent running, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the time interval classification is defined in the ClassAd
    attribute ``JobsRuntimesHistogramBuckets``.
    :index:`RecentJobsCompletedSizes;ClassAd Scheduler attribute<single: RecentJobsCompletedSizes;ClassAd Scheduler attribute>`
 ``RecentJobsCompletedSizes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by image size, in the previous
    time interval defined by attribute ``RecentStatsLifetime``. Counts
    within the histogram are separated by a comma and a space, where the
    size classification is defined in the ClassAd attribute
    ``JobsSizesHistogramBuckets``.
    :index:`RecentJobsCoredumped;ClassAd Scheduler attribute<single: RecentJobsCoredumped;ClassAd Scheduler attribute>`
 ``RecentJobsCoredumped``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_COREDUMPED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsDebugLogError;ClassAd Scheduler attribute<single: RecentJobsDebugLogError;ClassAd Scheduler attribute>`
 ``RecentJobsDebugLogError``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``DPRINTF_ERROR`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsExecFailed;ClassAd Scheduler attribute<single: RecentJobsExecFailed;ClassAd Scheduler attribute>`
 ``RecentJobsExecFailed``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsExited;ClassAd Scheduler attribute<single: RecentJobsExited;ClassAd Scheduler attribute>`
 ``RecentJobsExited``:
    A Statistics attribute defining the number of times that jobs have
    exited normally in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsExitedAndClaimClosing;ClassAd Scheduler attribute<single: RecentJobsExitedAndClaimClosing;ClassAd Scheduler attribute>`
 ``RecentJobsExitedAndClaimClosing``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous time interval
    defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsExitedNormally;ClassAd Scheduler attribute<single: RecentJobsExitedNormally;ClassAd Scheduler attribute>`
 ``RecentJobsExitedNormally``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous
    time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsExitException;ClassAd Scheduler attribute<single: RecentJobsExitException;ClassAd Scheduler attribute>`
 ``RecentJobsExitException``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the previous time interval defined by
    attribute ``RecentStatsLifetime``.
    :index:`RecentJobsKilled;ClassAd Scheduler attribute<single: RecentJobsKilled;ClassAd Scheduler attribute>`
 ``RecentJobsKilled``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_KILLED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsMissedDeferralTime;ClassAd Scheduler attribute<single: RecentJobsMissedDeferralTime;ClassAd Scheduler attribute>`
 ``RecentJobsMissedDeferralTime``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the previous time interval defined
    by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsNotStarted;ClassAd Scheduler attribute<single: RecentJobsNotStarted;ClassAd Scheduler attribute>`
 ``RecentJobsNotStarted``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_NOT_STARTED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsShadowNoMemory;ClassAd Scheduler attribute<single: RecentJobsShadowNoMemory;ClassAd Scheduler attribute>`
 ``RecentJobsShadowNoMemory``:
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor\_shadow* in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsShouldHold;ClassAd Scheduler attribute<single: RecentJobsShouldHold;ClassAd Scheduler attribute>`
 ``RecentJobsShouldHold``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsShouldRemove;ClassAd Scheduler attribute<single: RecentJobsShouldRemove;ClassAd Scheduler attribute>`
 ``RecentJobsShouldRemove``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsShouldRequeue;ClassAd Scheduler attribute<single: RecentJobsShouldRequeue;ClassAd Scheduler attribute>`
 ``RecentJobsShouldRequeue``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor\_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentJobsStarted;ClassAd Scheduler attribute<single: RecentJobsStarted;ClassAd Scheduler attribute>`
 ``RecentJobsStarted``:
    A Statistics attribute defining the number of jobs started in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentJobsSubmitted;ClassAd Scheduler attribute<single: RecentJobsSubmitted;ClassAd Scheduler attribute>`
 ``RecentJobsSubmitted``:
    A Statistics attribute defining the number of jobs submitted in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    :index:`RecentShadowsReconnections;ClassAd Scheduler attribute<single: RecentShadowsReconnections;ClassAd Scheduler attribute>`
 ``RecentShadowsReconnections``:
    A Statistics attribute defining the number of times that
    *condor\_shadow* daemons lost connection to their *condor\_starter*
    daemons and successfully reconnected in the previous time interval
    defined by attribute ``RecentStatsLifetime``. This statistic only
    appears in the Scheduler ClassAd if the level of verbosity set by
    the configuration variable ``STATISTICS_TO_PUBLISH`` is set to 2 or
    higher.
    :index:`RecentShadowsRecycled;ClassAd Scheduler attribute<single: RecentShadowsRecycled;ClassAd Scheduler attribute>`
 ``RecentShadowsRecycled``:
    A Statistics attribute defining the number of times *condor\_shadow*
    processes have been recycled for use with a new job in the previous
    time interval defined by attribute ``RecentStatsLifetime``. This
    statistic only appears in the Scheduler ClassAd if the level of
    verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
    :index:`RecentShadowsStarted;ClassAd Scheduler attribute<single: RecentShadowsStarted;ClassAd Scheduler attribute>`
 ``RecentShadowsStarted``:
    A Statistics attribute defining the number of *condor\_shadow*
    daemons started in the previous time interval defined by attribute
    ``RecentStatsLifetime``.
    :index:`RecentStatsLifetime;ClassAd Scheduler attribute<single: RecentStatsLifetime;ClassAd Scheduler attribute>`
 ``RecentStatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``. This value starts at 0, and it may grow to a
    value as large as the value defined for attribute
    ``RecentWindowMax``.
    :index:`RecentStatsTickTime;ClassAd Scheduler attribute<single: RecentStatsTickTime;ClassAd Scheduler attribute>`
 ``RecentStatsTickTime``:
    A Statistics attribute defining the time that attributes with names
    that begin with ``Recent`` were last updated, represented as the
    number of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1,
    1970). This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
    :index:`RecentWindowMax;ClassAd Scheduler attribute<single: RecentWindowMax;ClassAd Scheduler attribute>`
 ``RecentWindowMax``:
    A Statistics attribute defining the maximum time in seconds over
    which attributes with names that begin with ``Recent`` are
    collected. The value is set by the configuration variable
    ``STATISTICS_WINDOW_SECONDS``
    :index:`STATISTICS_WINDOW_SECONDS<single: STATISTICS_WINDOW_SECONDS>`, which defaults to 1200
    seconds (20 minutes). This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
    :index:`ScheddIpAddr;ClassAd Scheduler attribute<single: ScheddIpAddr;ClassAd Scheduler attribute>`
 ``ScheddIpAddr``:
    String with the IP and port address of the *condor\_schedd* daemon
    which is publishing this Scheduler ClassAd.
    :index:`ServerTime;ClassAd Scheduler attribute<single: ServerTime;ClassAd Scheduler attribute>`
 ``ServerTime``:
    Description is not yet written.
    :index:`ShadowsReconnections;ClassAd Scheduler attribute<single: ShadowsReconnections;ClassAd Scheduler attribute>`
 ``ShadowsReconnections``:
    A Statistics attribute defining the number of times
    *condor\_shadow*\ s lost connection to their *condor\_starter*\ s
    and successfully reconnected in the previous ``StatsLifetime``
    seconds. This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
    :index:`ShadowsRecycled;ClassAd Scheduler attribute<single: ShadowsRecycled;ClassAd Scheduler attribute>`
 ``ShadowsRecycled``:
    A Statistics attribute defining the number of times *condor\_shadow*
    processes have been recycled for use with a new job in the previous
    ``StatsLifetime`` seconds. This statistic only appears in the
    Scheduler ClassAd if the level of verbosity set by the configuration
    variable ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
    :index:`ShadowsRunning;ClassAd Scheduler attribute<single: ShadowsRunning;ClassAd Scheduler attribute>`
 ``ShadowsRunning``:
    A Statistics attribute defining the number of *condor\_shadow*
    daemons currently running that are owned by this *condor\_schedd*.
    :index:`ShadowsRunningPeak;ClassAd Scheduler attribute<single: ShadowsRunningPeak;ClassAd Scheduler attribute>`
 ``ShadowsRunningPeak``:
    A Statistics attribute defining the maximum number of
    *condor\_shadow* daemons running at one time that were owned by this
    *condor\_schedd* over the lifetime of this *condor\_schedd*.
    :index:`ShadowsStarted;ClassAd Scheduler attribute<single: ShadowsStarted;ClassAd Scheduler attribute>`
 ``ShadowsStarted``:
    A Statistics attribute defining the number of *condor\_shadow*
    daemons started in the previous time interval defined by attribute
    ``StatsLifetime``.
    :index:`StartLocalUniverse;ClassAd Scheduler attribute<single: StartLocalUniverse;ClassAd Scheduler attribute>`
 ``StartLocalUniverse``:
    The same boolean value as set in the configuration variable
    ``START_LOCAL_UNIVERSE`` :index:`START_LOCAL_UNIVERSE<single: START_LOCAL_UNIVERSE>`. See
    the definition at section \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__ on
    page \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`StartSchedulerUniverse;ClassAd Scheduler attribute<single: StartSchedulerUniverse;ClassAd Scheduler attribute>`
 ``StartSchedulerUniverse``:
    The same boolean value as set in the configuration variable
    ``START_SCHEDULER_UNIVERSE``
    :index:`START_SCHEDULER_UNIVERSE<single: START_SCHEDULER_UNIVERSE>`. See the definition at
    section \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__ on
    page \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`StatsLastUpdateTime;ClassAd Scheduler attribute<single: StatsLastUpdateTime;ClassAd Scheduler attribute>`
 ``StatsLastUpdateTime``:
    A Statistics attribute defining the time that statistics about jobs
    were last updated, represented as the number of seconds elapsed
    since the Unix epoch (00:00:00 UTC, Jan 1, 1970). This statistic
    only appears in the Scheduler ClassAd if the level of verbosity set
    by the configuration variable ``STATISTICS_TO_PUBLISH`` is set to 2
    or higher.
    :index:`StatsLifetime;ClassAd Scheduler attribute<single: StatsLifetime;ClassAd Scheduler attribute>`
 ``StatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics have been collected for attributes with names that do not
    begin with ``Recent``. This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.
    :index:`TotalFlockedJobs;ClassAd Scheduler attribute<single: TotalFlockedJobs;ClassAd Scheduler attribute>`
 ``TotalFlockedJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently flocked to other pools.
    :index:`TotalHeldJobs;ClassAd Scheduler attribute<single: TotalHeldJobs;ClassAd Scheduler attribute>`
 ``TotalHeldJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently on hold.
    :index:`TotalIdleJobs;ClassAd Scheduler attribute<single: TotalIdleJobs;ClassAd Scheduler attribute>`
 ``TotalIdleJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently idle, not including local or scheduler universe jobs.
    :index:`TotalJobAds;ClassAd Scheduler attribute<single: TotalJobAds;ClassAd Scheduler attribute>`
 ``TotalJobAds``:
    The total number of all jobs (in all states) from this
    *condor\_schedd* daemon.
    :index:`TotalLocalJobsIdle;ClassAd Scheduler attribute<single: TotalLocalJobsIdle;ClassAd Scheduler attribute>`
 ``TotalLocalJobsIdle``:
    The total number of **local**
    **universe**\ :index:`universe;submit commands<single: universe;submit commands>` jobs from
    this *condor\_schedd* daemon that are currently idle.
    :index:`TotalLocalJobsRunning;ClassAd Scheduler attribute<single: TotalLocalJobsRunning;ClassAd Scheduler attribute>`
 ``TotalLocalJobsRunning``:
    The total number of **local**
    **universe**\ :index:`universe;submit commands<single: universe;submit commands>` jobs from
    this *condor\_schedd* daemon that are currently running.
    :index:`TotalRemovedJobs;ClassAd Scheduler attribute<single: TotalRemovedJobs;ClassAd Scheduler attribute>`
 ``TotalRemovedJobs``:
    The current number of all running jobs from this *condor\_schedd*
    daemon that have remove requests.
    :index:`TotalRunningJobs;ClassAd Scheduler attribute<single: TotalRunningJobs;ClassAd Scheduler attribute>`
 ``TotalRunningJobs``:
    The total number of jobs from this *condor\_schedd* daemon that are
    currently running, not including local or scheduler universe jobs.
    :index:`TotalSchedulerJobsIdle;ClassAd Scheduler attribute<single: TotalSchedulerJobsIdle;ClassAd Scheduler attribute>`
 ``TotalSchedulerJobsIdle``:
    The total number of **scheduler**
    **universe**\ :index:`universe;submit commands<single: universe;submit commands>` jobs from
    this *condor\_schedd* daemon that are currently idle.
    :index:`TotalSchedulerJobsRunning;ClassAd Scheduler attribute<single: TotalSchedulerJobsRunning;ClassAd Scheduler attribute>`
 ``TotalSchedulerJobsRunning``:
    The total number of **scheduler**
    **universe**\ :index:`universe;submit commands<single: universe;submit commands>` jobs from
    this *condor\_schedd* daemon that are currently running.
    :index:`TransferQueueUserExpr;ClassAd Scheduler attribute<single: TransferQueueUserExpr;ClassAd Scheduler attribute>`
 ``TransferQueueUserExpr``
    A ClassAd expression that provides the name of the transfer queue
    that the *condor\_schedd* will be using for job file transfer.
    :index:`UpdateInterval;ClassAd Scheduler attribute<single: UpdateInterval;ClassAd Scheduler attribute>`
 ``UpdateInterval``:
    The interval, in seconds, between publication of this
    *condor\_schedd* ClassAd and the previous publication.
    :index:`UpdateSequenceNumber;ClassAd Scheduler attribute<single: UpdateSequenceNumber;ClassAd Scheduler attribute>`
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.
    :index:`VirtualMemory;ClassAd Scheduler attribute<single: VirtualMemory;ClassAd Scheduler attribute>`
 ``VirtualMemory``:
    Description is not yet written.
    :index:`WantResAd;ClassAd Scheduler attribute<single: WantResAd;ClassAd Scheduler attribute>`
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
configuration variable ``TRANSFER_QUEUE_USER_EXPR``
:index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`. This expression defaults to
``Owner_`` followed by the name of the job owner. The attributes that
are rates have a suffix that specifies the time span of the exponential
moving average. By default the time spans that are published are 1m, 5m,
1h, and 1d. This can be changed by configuring configuration variable
``TRANSFER_IO_REPORT_TIMESPANS``
:index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`. These attributes are only
reported once a full time span has accumulated.
:index:`FileTransferDiskThrottleExcess;ClassAd Scheduler attribute<single: FileTransferDiskThrottleExcess;ClassAd Scheduler attribute>`

 ``FileTransferDiskThrottleExcess_<timespan>``
    The exponential moving average of the disk load that exceeds the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
    :index:`FileTransferDiskThrottleHigh;ClassAd Scheduler attribute<single: FileTransferDiskThrottleHigh;ClassAd Scheduler attribute>`
 ``FileTransferDiskThrottleHigh``
    The desired upper limit for the disk load from file transfers, as
    configured by ``FILE_TRANSFER_DISK_LOAD_THROTTLE``
    :index:`FILE_TRANSFER_DISK_LOAD_THROTTLE<single: FILE_TRANSFER_DISK_LOAD_THROTTLE>`. This attribute is
    published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
    :index:`FileTransferDiskThrottleLevel;ClassAd Scheduler attribute<single: FileTransferDiskThrottleLevel;ClassAd Scheduler attribute>`
 ``FileTransferDiskThrottleLevel``
    The current concurrency limit set by the disk load throttle. The
    limit is applied to the sum of uploads and downloads. This attribute
    is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
    :index:`FileTransferDiskThrottleLow;ClassAd Scheduler attribute<single: FileTransferDiskThrottleLow;ClassAd Scheduler attribute>`
 ``FileTransferDiskThrottleLow``
    The lower limit for the disk load from file transfers, as configured
    by ``FILE_TRANSFER_DISK_LOAD_THROTTLE``
    :index:`FILE_TRANSFER_DISK_LOAD_THROTTLE<single: FILE_TRANSFER_DISK_LOAD_THROTTLE>`. This attribute is
    published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
    :index:`FileTransferDiskThrottleShortfall;ClassAd Scheduler attribute<single: FileTransferDiskThrottleShortfall;ClassAd Scheduler attribute>`
 ``FileTransferDiskThrottleShortfall_<timespan>``
    The exponential moving average of the disk load that falls below the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.
    :index:`FileTransferDownloadBytes;ClassAd Scheduler attribute<single: FileTransferDownloadBytes;ClassAd Scheduler attribute>`
 ``FileTransferDownloadBytes``
    Total number of bytes downloaded as output from jobs since this
    *condor\_schedd* was started. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferDownloadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferDownloadBytesPerSecond;ClassAd Scheduler attribute<single: FileTransferDownloadBytesPerSecond;ClassAd Scheduler attribute>`
 ``FileTransferDownloadBytesPerSecond_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which bytes have been downloaded as output from jobs. The time
    spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS``
    :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferDownloadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferFileReadLoad;ClassAd Scheduler attribute<single: FileTransferFileReadLoad;ClassAd Scheduler attribute>`
 ``FileTransferFileReadLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time reading
    from files to be transferred as input to jobs. One file transfer
    process spending nearly all of its time reading files will generate
    a load close to 1.0. The time spans that are published are
    configured by ``TRANSFER_IO_REPORT_TIMESPANS``
    :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferFileReadSeconds;ClassAd Scheduler attribute<single: FileTransferFileReadSeconds;ClassAd Scheduler attribute>`
 ``FileTransferFileReadSeconds``
    Total number of submit-side transfer process seconds spent reading
    from files to be transferred as input to jobs since this
    *condor\_schedd* was started. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferFileWriteLoad;ClassAd Scheduler attribute<single: FileTransferFileWriteLoad;ClassAd Scheduler attribute>`
 ``FileTransferFileWriteLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time writing
    to files transferred as output from jobs. One file transfer process
    spending nearly all of its time writing to files will generate a
    load close to 1.0. The time spans that are published are configured
    by ``TRANSFER_IO_REPORT_TIMESPANS``
    :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferFileWriteSeconds;ClassAd Scheduler attribute<single: FileTransferFileWriteSeconds;ClassAd Scheduler attribute>`
 ``FileTransferFileWriteSeconds``
    Total number of submit-side transfer process seconds spent writing
    to files transferred as output from jobs since this *condor\_schedd*
    was started. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileWriteSeconds``. The published
    user name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferFileNetReadLoad;ClassAd Scheduler attribute<single: FileTransferFileNetReadLoad;ClassAd Scheduler attribute>`
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
    ``TRANSFER_IO_REPORT_TIMESPANS``
    :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferNetReadSeconds;ClassAd Scheduler attribute<single: FileTransferNetReadSeconds;ClassAd Scheduler attribute>`
 ``FileTransferNetReadSeconds``
    Total number of submit-side transfer process seconds spent reading
    from the network when transferring output from jobs since this
    *condor\_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow reads from the disk on the execute
    side. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferNetWriteLoad;ClassAd Scheduler attribute<single: FileTransferNetWriteLoad;ClassAd Scheduler attribute>`
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
    ``TRANSFER_IO_REPORT_TIMESPANS``\ :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`,
    which defaults to 1m, 5m, 1h, and 1d. When less than one full time
    span has accumulated, the attribute is not published. If
    ``STATISTICS_TO_PUBLISH``\ :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>`
    contains ``TRANSFER:2``, for each active user, this attribute is
    also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferNetWriteSeconds;ClassAd Scheduler attribute<single: FileTransferNetWriteSeconds;ClassAd Scheduler attribute>`
 ``FileTransferNetWriteSeconds``
    Total number of submit-side transfer process seconds spent writing
    to the network when transferring input to jobs since this
    *condor\_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow writes to the disk on the execute side.
    The time spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS``
    :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetWriteSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferUploadBytes;ClassAd Scheduler attribute<single: FileTransferUploadBytes;ClassAd Scheduler attribute>`
 ``FileTransferUploadBytes``
    Total number of bytes uploaded as input to jobs since this
    *condor\_schedd* was started. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferUploadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`FileTransferUploadBytesPerSecond;ClassAd Scheduler attribute<single: FileTransferUploadBytesPerSecond;ClassAd Scheduler attribute>`
 ``FileTransferUploadBytesPerSecond_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which bytes have been uploaded as input to jobs. The time spans
    that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS``
    :index:`TRANSFER_IO_REPORT_TIMESPANS<single: TRANSFER_IO_REPORT_TIMESPANS>`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH``
    :index:`STATISTICS_TO_PUBLISH<single: STATISTICS_TO_PUBLISH>` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferUploadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``
    :index:`TRANSFER_QUEUE_USER_EXPR<single: TRANSFER_QUEUE_USER_EXPR>`.
    :index:`TransferQueueMBWaitingToDownload;ClassAd Scheduler attribute<single: TransferQueueMBWaitingToDownload;ClassAd Scheduler attribute>`
 ``TransferQueueMBWaitingToDownload``
    Number of megabytes of output files waiting to be downloaded.
    :index:`TransferQueueMBWaitingToUpload;ClassAd Scheduler attribute<single: TransferQueueMBWaitingToUpload;ClassAd Scheduler attribute>`
 ``TransferQueueMBWaitingToUpload``
    Number of megabytes of input files waiting to be uploaded.
    :index:`TransferQueueNumWaitingToDownload;ClassAd Scheduler attribute<single: TransferQueueNumWaitingToDownload;ClassAd Scheduler attribute>`
 ``TransferQueueNumWaitingToDownload``
    Number of jobs waiting to transfer output files.
    :index:`TransferQueueNumWaitingToUpload;ClassAd Scheduler attribute<single: TransferQueueNumWaitingToUpload;ClassAd Scheduler attribute>`
 ``TransferQueueNumWaitingToUpload``
    Number of jobs waiting to transfer input files.

      
