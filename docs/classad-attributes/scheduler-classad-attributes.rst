Scheduler ClassAd Attributes
============================


:index:`Scheduler attributes<single: Scheduler attributes; ClassAd>`
:index:`Autoclusters<single: Autoclusters; ClassAd Scheduler attribute>`

``Autoclusters``:
    A Statistics attribute defining the number of active autoclusters.

:index:`CollectorHost<single: CollectorHost; ClassAd Scheduler attribute>`

``CollectorHost``:
    The name of the main *condor_collector* which this *condor_schedd*
    daemon reports to, as copied from ``COLLECTOR_HOST``. :index:`COLLECTOR_HOST`
    If a *condor_schedd* flocks to other
    *condor_collector* daemons, this attribute still represents the
    "home" *condor_collector*, so this value can be used to discover if
    a *condor_schedd* is currently flocking.

:index:`CondorVersion<single: CondorVersion; ClassAd Scheduler attribute>`

``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:index:`DaemonCoreDutyCycle<single: DaemonCoreDutyCycle; ClassAd Scheduler attribute>`

``DaemonCoreDutyCycle``:
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time for the time period defined
    by ``StatsLifetime`` of this *condor_schedd*. A value near 0.0
    indicates an idle daemon, while a value near 1.0 indicates a daemon
    running at or above capacity.

:index:`DaemonStartTime<single: DaemonStartTime; ClassAd Scheduler attribute>`

``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:index:`DaemonLastReconfigTime<single: DaemonLastReconfigTime; ClassAd Scheduler attribute>`

``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:index:`DetectedCpus<single: DetectedCpus; ClassAd Scheduler attribute>`

``DetectedCpus``:
    The number of detected machine CPUs/cores.

:index:`DetectedMemory<single: DetectedMemory; ClassAd Scheduler attribute>`

``DetectedMemory``:
    The amount of detected machine RAM in MBytes.

:index:`JobQueueBirthdate<single: JobQueueBirthdate; ClassAd Scheduler attribute>`

``JobQueueBirthdate``:
    This attribute contains the Unix epoch time when the job_queue.log file which
    stores the scheduler's database was first created.

:index:`JobsAccumBadputTime<single: JobsAccumBadputTime; ClassAd Scheduler attribute>`

``JobsAccumBadputTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully have spent running over the
    lifetime of this *condor_schedd*.

:index:`JobsAccumExceptionalBadputTime<single: JobsAccumExceptionalBadputTime; ClassAd Scheduler attribute>`

``JobsAccumExceptionalBadputTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully due to *condor_shadow*
    exceptions have spent running over the lifetime of this
    *condor_schedd*.

:index:`JobsAccumRunningTime<single: JobsAccumRunningTime; ClassAd Scheduler attribute>`

``JobsAccumRunningTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    have spent running in the time interval defined by attribute
    ``StatsLifetime``.

:index:`JobsAccumTimeToStart<single: JobsAccumTimeToStart; ClassAd Scheduler attribute>`

``JobsAccumTimeToStart``:
    A Statistics attribute defining the sum of all the time jobs have
    spent waiting to start in the time interval defined by attribute
    ``StatsLifetime``.

:index:`JobsBadputRuntimes<single: JobsBadputRuntimes; ClassAd Scheduler attribute>`

``JobsBadputRuntimes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, over
    the lifetime of this *condor_schedd*. Counts within the histogram
    are separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.

:index:`JobsBadputSizes<single: JobsBadputSizes; ClassAd Scheduler attribute>`

``JobsBadputSizes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, over the
    lifetime of this *condor_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.

:index:`JobsCheckpointed<single: JobsCheckpointed; ClassAd Scheduler attribute>`

``JobsCheckpointed``:
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor_shadow* exit code of ``JOB_CKPTED`` in the
    time interval defined by attribute ``StatsLifetime``.

:index:`JobsCompleted<single: JobsCompleted; ClassAd Scheduler attribute>`

``JobsCompleted``:
    A Statistics attribute defining the number of jobs successfully
    completed in the time interval defined by attribute
    ``StatsLifetime``.

:index:`JobsCompletedRuntimes<single: JobsCompletedRuntimes; ClassAd Scheduler attribute>`

``JobsCompletedRuntimes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by time spent running, over the
    lifetime of this *condor_schedd*. Counts within the histogram are
    separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.

:index:`JobsCompletedSizes<single: JobsCompletedSizes; ClassAd Scheduler attribute>`

``JobsCompletedSizes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by image size, over the
    lifetime of this *condor_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.

:index:`JobsCoredumped<single: JobsCoredumped; ClassAd Scheduler attribute>`

``JobsCoredumped``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_COREDUMPED`` in
    the time interval defined by attribute ``StatsLifetime``.

:index:`JobsDebugLogError<single: JobsDebugLogError; ClassAd Scheduler attribute>`

``JobsDebugLogError``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``DPRINTF_ERROR`` in the
    time interval defined by attribute ``StatsLifetime``.

:index:`JobsExecFailed<single: JobsExecFailed; ClassAd Scheduler attribute>`

``JobsExecFailed``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the time interval defined by attribute ``StatsLifetime``.

:index:`JobsExited<single: JobsExited; ClassAd Scheduler attribute>`

``JobsExited``:
    A Statistics attribute defining the number of times that jobs that
    exited (successfully or not) in the time interval defined by
    attribute ``StatsLifetime``.

:index:`JobsExitedAndClaimClosing<single: JobsExitedAndClaimClosing; ClassAd Scheduler attribute>`

``JobsExitedAndClaimClosing``:
    A Statistics attribute defining the number of times jobs have exited
    with a *condor_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time interval defined by
    attribute ``StatsLifetime``.

:index:`JobsExitedNormally<single: JobsExitedNormally; ClassAd Scheduler attribute>`

``JobsExitedNormally``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time
    interval defined by attribute ``StatsLifetime``.

:index:`JobsExitException<single: JobsExitException; ClassAd Scheduler attribute>`

``JobsExitException``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the time interval defined by attribute
    ``StatsLifetime``.

:index:`JobsKilled<single: JobsKilled; ClassAd Scheduler attribute>`

``JobsKilled``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_KILLED`` in the
    time interval defined by attribute ``StatsLifetime``.

:index:`JobsMissedDeferralTime<single: JobsMissedDeferralTime; ClassAd Scheduler attribute>`

``JobsMissedDeferralTime``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the time interval defined by
    attribute ``StatsLifetime``.

:index:`JobsNotStarted<single: JobsNotStarted; ClassAd Scheduler attribute>`

``JobsNotStarted``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_NOT_STARTED`` in
    the time interval defined by attribute ``StatsLifetime``.

:index:`JobsRestartReconnectsAttempting<single: JobsRestartReconnectsAttempting; ClassAd Scheduler attribute>`

``JobsRestartReconnectsAttempting``:
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* is currently attempting to reconnect
    to, in order to recover a job that was running when the
    *condor_schedd* was restarted.

:index:`JobsRestartReconnectsBadput<single: JobsRestartReconnectsBadput; ClassAd Scheduler attribute>`

``JobsRestartReconnectsBadput``:
    A Statistics attribute defining a histogram count of
    *condor_startd* daemons that the *condor_schedd* could not
    reconnect to in order to recover a job that was running when the
    *condor_schedd* was restarted, as classified by the time the job
    spent running. Counts within the histogram are separated by a comma
    and a space, where the time interval classification is defined in
    the ClassAd attribute ``JobsRuntimesHistogramBuckets``.

:index:`JobsRestartReconnectsFailed<single: JobsRestartReconnectsFailed; ClassAd Scheduler attribute>`

``JobsRestartReconnectsFailed``:
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* tried and failed to reconnect to in
    order to recover a job that was running when the *condor_schedd*
    was restarted.

:index:`JobsRestartReconnectsInterrupted<single: JobsRestartReconnectsInterrupted; ClassAd Scheduler attribute>`

``JobsRestartReconnectsInterrupted``:
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* attempted to reconnect to, in order to
    recover a job that was running when the *condor_schedd* was
    restarted, but the attempt was interrupted, for example, because the
    job was removed.

:index:`JobsRestartReconnectsLeaseExpired<single: JobsRestartReconnectsLeaseExpired; ClassAd Scheduler attribute>`

``JobsRestartReconnectsLeaseExpired``:
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* could not attempt to reconnect to, in
    order to recover a job that was running when the *condor_schedd*
    was restarted, because the job lease had already expired.

:index:`JobsRestartReconnectsSucceeded<single: JobsRestartReconnectsSucceeded; ClassAd Scheduler attribute>`

``JobsRestartReconnectsSucceeded``:
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* has successfully reconnected to, in
    order to recover a job that was running when the *condor_schedd*
    was restarted.

:index:`JobsRunning<single: JobsRunning; ClassAd Scheduler attribute>`

``JobsRunning``:
    A Statistics attribute representing the number of jobs currently
    running.

:index:`JobsRunningRuntimes<single: JobsRunningRuntimes; ClassAd Scheduler attribute>`

``JobsRunningRuntimes``:
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by elapsed runtime. Counts within the
    histogram are separated by a comma and a space, where the time
    interval classification is defined in the ClassAd attribute
    ``JobsRuntimesHistogramBuckets``.

:index:`JobsRunningSizes<single: JobsRunningSizes; ClassAd Scheduler attribute>`

``JobsRunningSizes``:
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by image size. Counts within the histogram
    are separated by a comma and a space, where the size classification
    is defined in the ClassAd attribute ``JobsSizesHistogramBuckets``.

:index:`JobsRuntimesHistogramBuckets<single: JobsRuntimesHistogramBuckets; ClassAd Scheduler attribute>`

``JobsRuntimesHistogramBuckets``:
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify run times. Defined as

    .. code-block:: condor-config

          JobsRuntimesHistogramBuckets = "30Sec, 1Min, 3Min, 10Min, 30Min, 1Hr, 3Hr,
                  6Hr, 12Hr, 1Day, 2Day, 4Day, 8Day, 16Day"


:index:`JobsShadowNoMemory<single: JobsShadowNoMemory; ClassAd Scheduler attribute>`

``JobsShadowNoMemory``:
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor_shadow* in the time interval defined by attribute
    ``StatsLifetime``.

:index:`JobsShouldHold<single: JobsShouldHold; ClassAd Scheduler attribute>`

``JobsShouldHold``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the time interval defined by attribute ``StatsLifetime``.

:index:`JobsShouldRemove<single: JobsShouldRemove; ClassAd Scheduler attribute>`

``JobsShouldRemove``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the time interval defined by attribute ``StatsLifetime``.

:index:`JobsShouldRequeue<single: JobsShouldRequeue; ClassAd Scheduler attribute>`

``JobsShouldRequeue``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the time interval defined by attribute ``StatsLifetime``.

:index:`JobsSizesHistogramBuckets<single: JobsSizesHistogramBuckets; ClassAd Scheduler attribute>`

``JobsSizesHistogramBuckets``:
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify image sizes. Defined as

    .. code-block:: condor-config

          JobsSizesHistogramBuckets = "64Kb, 256Kb, 1Mb, 4Mb, 16Mb, 64Mb, 256Mb,
                  1Gb, 4Gb, 16Gb, 64Gb, 256Gb"

    Note that these values imply powers of two in numbers of bytes.

:index:`JobsStarted<single: JobsStarted; ClassAd Scheduler attribute>`

``JobsStarted``:
    A Statistics attribute defining the number of jobs started in the
    time interval defined by attribute ``StatsLifetime``.

:index:`JobsSubmitted<single: JobsSubmitted; ClassAd Scheduler attribute>`

``JobsSubmitted``:
    A Statistics attribute defining the number of jobs submitted in the
    time interval defined by attribute ``StatsLifetime``.

:index:`Machine<single: Machine; ClassAd Scheduler attribute>`

``Machine``:
    A string with the machine's fully qualified host name.

:index:`MaxJobsRunning<single: MaxJobsRunning; ClassAd Scheduler attribute>`

``MaxJobsRunning``:
    The same integer value as set by the evaluation of the configuration
    variable ``MAX_JOBS_RUNNING`` :index:`MAX_JOBS_RUNNING`. See
    the definition in the :ref:`admin-manual/configuration-macros:condor_schedd
    configuration file entries` section.

:index:`MonitorSelfAge<single: MonitorSelfAge; ClassAd Scheduler attribute>`

``MonitorSelfAge``:
    The number of seconds that this daemon has been running.

:index:`MonitorSelfCPUUsage<single: MonitorSelfCPUUsage; ClassAd Scheduler attribute>`

``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.

:index:`MonitorSelfImageSize<single: MonitorSelfImageSize; ClassAd Scheduler attribute>`

``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in Kbytes.

:index:`MonitorSelfRegisteredSocketCount<single: MonitorSelfRegisteredSocketCount; ClassAd Scheduler attribute>`

``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.

:index:`MonitorSelfResidentSetSize<single: MonitorSelfResidentSetSize; ClassAd Scheduler attribute>`

``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in Kbytes.

:index:`MonitorSelfSecuritySessions<single: MonitorSelfSecuritySessions; ClassAd Scheduler attribute>`

``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.

:index:`MonitorSelfTime<single: MonitorSelfTime; ClassAd Scheduler attribute>`

``MonitorSelfTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.

:index:`MyAddress<single: MyAddress; ClassAd Scheduler attribute>`

``MyAddress``:
    String with the IP and port address of the *condor_schedd* daemon
    which is publishing this ClassAd.

:index:`MyCurrentTime<single: MyCurrentTime; ClassAd Scheduler attribute>`

``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_schedd*
    daemon last sent a ClassAd update to the *condor_collector*.

:index:`Name<single: Name; ClassAd Scheduler attribute>`

``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:index:`NumJobStartsDelayed<single: NumJobStartsDelayed; ClassAd Scheduler attribute>`

``NumJobStartsDelayed``:
    The number times a job requiring a *condor_shadow* daemon could
    have been started, but was not started because of the values of
    configuration variables ``JOB_START_COUNT`` :index:`JOB_START_COUNT`
    and ``JOB_START_DELAY`` :index:`JOB_START_DELAY`

:index:`NumPendingClaims<single: NumPendingClaims; ClassAd Scheduler attribute>`

``NumPendingClaims``:
    The number of machines (*condor_startd* daemons) matched to this
    *condor_schedd* daemon, which this *condor_schedd* knows about,
    but has not yet managed to claim.

:index:`NumUsers<single: NumUsers; ClassAd Scheduler attribute>`

``NumUsers``:
    The integer number of distinct users with jobs in this
    *condor_schedd* 's queue.

:index:`PublicNetworkIpAddr<single: PublicNetworkIpAddr; ClassAd Scheduler attribute>`

``PublicNetworkIpAddr``:
    This is the public network address of this daemon.

:index:`RecentDaemonCoreDutyCycle<single: RecentDaemonCoreDutyCycle; ClassAd Scheduler attribute>`

``RecentDaemonCoreDutyCycle``:
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time in the previous time
    interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsAccumBadputTime<single: RecentJobsAccumBadputTime; ClassAd Scheduler attribute>`

``RecentJobsAccumBadputTime``:
    A Statistics attribute defining the sum of the all of the time that
    jobs which did not complete successfully have spent running in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsAccumRunningTime<single: RecentJobsAccumRunningTime; ClassAd Scheduler attribute>`

``RecentJobsAccumRunningTime``:
    A Statistics attribute defining the sum of the all of the time jobs
    which have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` spent running.

:index:`RecentJobsAccumTimeToStart<single: RecentJobsAccumTimeToStart; ClassAd Scheduler attribute>`

``RecentJobsAccumTimeToStart``:
    A Statistics attribute defining the sum of all the time jobs which
    have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` had spent waiting to start.

:index:`RecentJobsBadputRuntimes<single: RecentJobsBadputRuntimes; ClassAd Scheduler attribute>`

``RecentJobsBadputRuntimes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``. Counts within the histogram are separated
    by a comma and a space, where the time interval classification is
    defined in the ClassAd attribute ``JobsRuntimesHistogramBuckets``.

:index:`RecentJobsBadputSizes<single: RecentJobsBadputSizes; ClassAd Scheduler attribute>`

``RecentJobsBadputSizes``:
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the size classification is defined in the ClassAd attribute
    ``JobsSizesHistogramBuckets``.

:index:`RecentJobsCheckpointed<single: RecentJobsCheckpointed; ClassAd Scheduler attribute>`

``RecentJobsCheckpointed``:
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor_shadow* exit code of ``JOB_CKPTED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsCompleted<single: RecentJobsCompleted; ClassAd Scheduler attribute>`

``RecentJobsCompleted``:
    A Statistics attribute defining the number of jobs successfully
    completed in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsCompletedRuntimes<single: RecentJobsCompletedRuntimes; ClassAd Scheduler attribute>`

``RecentJobsCompletedRuntimes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by time spent running, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the time interval classification is defined in the ClassAd
    attribute ``JobsRuntimesHistogramBuckets``.

:index:`RecentJobsCompletedSizes<single: RecentJobsCompletedSizes; ClassAd Scheduler attribute>`

``RecentJobsCompletedSizes``:
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by image size, in the previous
    time interval defined by attribute ``RecentStatsLifetime``. Counts
    within the histogram are separated by a comma and a space, where the
    size classification is defined in the ClassAd attribute
    ``JobsSizesHistogramBuckets``.

:index:`RecentJobsCoredumped<single: RecentJobsCoredumped; ClassAd Scheduler attribute>`

``RecentJobsCoredumped``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_COREDUMPED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsDebugLogError<single: RecentJobsDebugLogError; ClassAd Scheduler attribute>`

``RecentJobsDebugLogError``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``DPRINTF_ERROR`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsExecFailed<single: RecentJobsExecFailed; ClassAd Scheduler attribute>`

``RecentJobsExecFailed``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsExited<single: RecentJobsExited; ClassAd Scheduler attribute>`

``RecentJobsExited``:
    A Statistics attribute defining the number of times that jobs have
    exited normally in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsExitedAndClaimClosing<single: RecentJobsExitedAndClaimClosing; ClassAd Scheduler attribute>`

``RecentJobsExitedAndClaimClosing``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous time interval
    defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsExitedNormally<single: RecentJobsExitedNormally; ClassAd Scheduler attribute>`

``RecentJobsExitedNormally``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous
    time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsExitException<single: RecentJobsExitException; ClassAd Scheduler attribute>`

``RecentJobsExitException``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the previous time interval defined by
    attribute ``RecentStatsLifetime``.

:index:`RecentJobsKilled<single: RecentJobsKilled; ClassAd Scheduler attribute>`

``RecentJobsKilled``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_KILLED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsMissedDeferralTime<single: RecentJobsMissedDeferralTime; ClassAd Scheduler attribute>`

``RecentJobsMissedDeferralTime``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the previous time interval defined
    by attribute ``RecentStatsLifetime``.

:index:`RecentJobsNotStarted<single: RecentJobsNotStarted; ClassAd Scheduler attribute>`

``RecentJobsNotStarted``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_NOT_STARTED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsShadowNoMemory<single: RecentJobsShadowNoMemory; ClassAd Scheduler attribute>`

``RecentJobsShadowNoMemory``:
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor_shadow* in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsShouldHold<single: RecentJobsShouldHold; ClassAd Scheduler attribute>`

``RecentJobsShouldHold``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsShouldRemove<single: RecentJobsShouldRemove; ClassAd Scheduler attribute>`

``RecentJobsShouldRemove``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsShouldRequeue<single: RecentJobsShouldRequeue; ClassAd Scheduler attribute>`

``RecentJobsShouldRequeue``:
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentJobsStarted<single: RecentJobsStarted; ClassAd Scheduler attribute>`

``RecentJobsStarted``:
    A Statistics attribute defining the number of jobs started in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentJobsSubmitted<single: RecentJobsSubmitted; ClassAd Scheduler attribute>`

``RecentJobsSubmitted``:
    A Statistics attribute defining the number of jobs submitted in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:index:`RecentShadowsReconnections<single: RecentShadowsReconnections; ClassAd Scheduler attribute>`

``RecentShadowsReconnections``:
    A Statistics attribute defining the number of times that
    *condor_shadow* daemons lost connection to their *condor_starter*
    daemons and successfully reconnected in the previous time interval
    defined by attribute ``RecentStatsLifetime``. This statistic only
    appears in the Scheduler ClassAd if the level of verbosity set by
    the configuration variable ``STATISTICS_TO_PUBLISH`` is set to 2 or
    higher.

:index:`RecentShadowsRecycled<single: RecentShadowsRecycled; ClassAd Scheduler attribute>`

``RecentShadowsRecycled``:
    A Statistics attribute defining the number of times *condor_shadow*
    processes have been recycled for use with a new job in the previous
    time interval defined by attribute ``RecentStatsLifetime``. This
    statistic only appears in the Scheduler ClassAd if the level of
    verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.

:index:`RecentShadowsStarted<single: RecentShadowsStarted; ClassAd Scheduler attribute>`

``RecentShadowsStarted``:
    A Statistics attribute defining the number of *condor_shadow*
    daemons started in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:index:`RecentStatsLifetime<single: RecentStatsLifetime; ClassAd Scheduler attribute>`

``RecentStatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``. This value starts at 0, and it may grow to a
    value as large as the value defined for attribute
    ``RecentWindowMax``.

:index:`RecentStatsTickTime<single: RecentStatsTickTime; ClassAd Scheduler attribute>`

``RecentStatsTickTime``:
    A Statistics attribute defining the time that attributes with names
    that begin with ``Recent`` were last updated, represented as the
    number of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1,
    1970). This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.

:index:`RecentWindowMax<single: RecentWindowMax; ClassAd Scheduler attribute>`

``RecentWindowMax``:
    A Statistics attribute defining the maximum time in seconds over
    which attributes with names that begin with ``Recent`` are
    collected. The value is set by the configuration variable
    ``STATISTICS_WINDOW_SECONDS`` :index:`STATISTICS_WINDOW_SECONDS`
    , which defaults to 1200
    seconds (20 minutes). This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.

:index:`ScheddIpAddr<single: ScheddIpAddr; ClassAd Scheduler attribute>`

``ScheddIpAddr``:
    String with the IP and port address of the *condor_schedd* daemon
    which is publishing this Scheduler ClassAd.

:index:`ServerTime<single: ServerTime; ClassAd Scheduler attribute>`

``ServerTime``:
    This is the current time, in Unix epoch seconds when the reply to
    this query started.

:index:`ShadowsReconnections<single: ShadowsReconnections; ClassAd Scheduler attribute>`

``ShadowsReconnections``:
    A Statistics attribute defining the number of times
    *condor_shadow* s lost connection to their *condor_starter* s
    and successfully reconnected in the previous ``StatsLifetime``
    seconds. This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.

:index:`ShadowsRecycled<single: ShadowsRecycled; ClassAd Scheduler attribute>`

``ShadowsRecycled``:
    A Statistics attribute defining the number of times *condor_shadow*
    processes have been recycled for use with a new job in the previous
    ``StatsLifetime`` seconds. This statistic only appears in the
    Scheduler ClassAd if the level of verbosity set by the configuration
    variable ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.

:index:`ShadowsRunning<single: ShadowsRunning; ClassAd Scheduler attribute>`

``ShadowsRunning``:
    A Statistics attribute defining the number of *condor_shadow*
    daemons currently running that are owned by this *condor_schedd*.

:index:`ShadowsRunningPeak<single: ShadowsRunningPeak; ClassAd Scheduler attribute>`

``ShadowsRunningPeak``:
    A Statistics attribute defining the maximum number of
    *condor_shadow* daemons running at one time that were owned by this
    *condor_schedd* over the lifetime of this *condor_schedd*.

:index:`ShadowsStarted<single: ShadowsStarted; ClassAd Scheduler attribute>`

``ShadowsStarted``:
    A Statistics attribute defining the number of *condor_shadow*
    daemons started in the previous time interval defined by attribute
    ``StatsLifetime``.

:index:`StartLocalUniverse<single: StartLocalUniverse; ClassAd Scheduler attribute>`

``StartLocalUniverse``:
    The same boolean value as set in the configuration variable
    ``START_LOCAL_UNIVERSE`` :index:`START_LOCAL_UNIVERSE`. See
    the definition in the :ref:`admin-manual/configuration-macros:condor_schedd
    configuration file entries` section.

:index:`StartSchedulerUniverse<single: StartSchedulerUniverse; ClassAd Scheduler attribute>`

``StartSchedulerUniverse``:
    The same boolean value as set in the configuration variable
    ``START_SCHEDULER_UNIVERSE``. :index:`START_SCHEDULER_UNIVERSE`
    See the definition in
    the :ref:`admin-manual/configuration-macros:condor_schedd
    configuration file entries` section.

:index:`StatsLastUpdateTime<single: StatsLastUpdateTime; ClassAd Scheduler attribute>`

``StatsLastUpdateTime``:
    A Statistics attribute defining the time that statistics about jobs
    were last updated, represented as the number of seconds elapsed
    since the Unix epoch (00:00:00 UTC, Jan 1, 1970). This statistic
    only appears in the Scheduler ClassAd if the level of verbosity set
    by the configuration variable ``STATISTICS_TO_PUBLISH`` is set to 2
    or higher.

:index:`StatsLifetime<single: StatsLifetime; ClassAd Scheduler attribute>`

``StatsLifetime``:
    A Statistics attribute defining the time in seconds over which
    statistics have been collected for attributes with names that do not
    begin with ``Recent``. This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    ``STATISTICS_TO_PUBLISH`` is set to 2 or higher.

:index:`TotalFlockedJobs<single: TotalFlockedJobs; ClassAd Scheduler attribute>`

``TotalFlockedJobs``:
    The total number of jobs from this *condor_schedd* daemon that are
    currently flocked to other pools.

:index:`TotalHeldJobs<single: TotalHeldJobs; ClassAd Scheduler attribute>`

``TotalHeldJobs``:
    The total number of jobs from this *condor_schedd* daemon that are
    currently on hold.

:index:`TotalIdleJobs<single: TotalIdleJobs; ClassAd Scheduler attribute>`

``TotalIdleJobs``:
    The total number of jobs from this *condor_schedd* daemon that are
    currently idle, not including local or scheduler universe jobs.

:index:`TotalJobAds<single: TotalJobAds; ClassAd Scheduler attribute>`

``TotalJobAds``:
    The total number of all jobs (in all states) from this
    *condor_schedd* daemon.

:index:`TotalLocalJobsIdle<single: TotalLocalJobsIdle; ClassAd Scheduler attribute>`

``TotalLocalJobsIdle``:
    The total number of **local**
    **universe** :index:`universe<single: universe; submit commands>` jobs from
    this *condor_schedd* daemon that are currently idle.

:index:`TotalLocalJobsRunning<single: TotalLocalJobsRunning; ClassAd Scheduler attribute>`

``TotalLocalJobsRunning``:
    The total number of **local**
    **universe** :index:`universe<single: universe; submit commands>` jobs from
    this *condor_schedd* daemon that are currently running.

:index:`TotalRemovedJobs<single: TotalRemovedJobs; ClassAd Scheduler attribute>`

``TotalRemovedJobs``:
    The current number of all running jobs from this *condor_schedd*
    daemon that have remove requests.

:index:`TotalRunningJobs<single: TotalRunningJobs; ClassAd Scheduler attribute>`

``TotalRunningJobs``:
    The total number of jobs from this *condor_schedd* daemon that are
    currently running, not including local or scheduler universe jobs.

:index:`TotalSchedulerJobsIdle<single: TotalSchedulerJobsIdle; ClassAd Scheduler attribute>`

``TotalSchedulerJobsIdle``:
    The total number of **scheduler**
    **universe** :index:`universe<single: universe; submit commands>` jobs from
    this *condor_schedd* daemon that are currently idle.

:index:`TotalSchedulerJobsRunning<single: TotalSchedulerJobsRunning; ClassAd Scheduler attribute>`

``TotalSchedulerJobsRunning``:
    The total number of **scheduler**
    **universe** :index:`universe<single: universe; submit commands>` jobs from
    this *condor_schedd* daemon that are currently running.

:index:`TransferQueueUserExpr<single: TransferQueueUserExpr; ClassAd Scheduler attribute>`

``TransferQueueUserExpr``
    A ClassAd expression that provides the name of the transfer queue
    that the *condor_schedd* will be using for job file transfer.

:index:`UpdateInterval<single: UpdateInterval; ClassAd Scheduler attribute>`

``UpdateInterval``:
    The interval, in seconds, between publication of this
    *condor_schedd* ClassAd and the previous publication.

:index:`UpdateSequenceNumber<single: UpdateSequenceNumber; ClassAd Scheduler attribute>`

``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor_collector*. The *condor_collector* uses
    this value to sequence the updates it receives.

:index:`VirtualMemory<single: VirtualMemory; ClassAd Scheduler attribute>`

``VirtualMemory``:
    Description is not yet written.

:index:`WantResAd<single: WantResAd; ClassAd Scheduler attribute>`

``WantResAd``:
    A boolean value that when ``True`` causes the *condor_negotiator*
    daemon to send to this *condor_schedd* daemon a full machine
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
configuration variable ``TRANSFER_QUEUE_USER_EXPR``. :index:`TRANSFER_QUEUE_USER_EXPR`
This expression defaults to
``Owner_`` followed by the name of the job owner. The attributes that
are rates have a suffix that specifies the time span of the exponential
moving average. By default the time spans that are published are 1m, 5m,
1h, and 1d. This can be changed by configuring configuration variable
``TRANSFER_IO_REPORT_TIMESPANS``.  :index:`TRANSFER_IO_REPORT_TIMESPANS`
These attributes are only reported once a full time span has accumulated.

:index:`FileTransferDiskThrottleExcess<single: FileTransferDiskThrottleExcess; ClassAd Scheduler attribute>`

``FileTransferDiskThrottleExcess_<timespan>``
    The exponential moving average of the disk load that exceeds the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.

:index:`FileTransferDiskThrottleHigh<single: FileTransferDiskThrottleHigh; ClassAd Scheduler attribute>`

``FileTransferDiskThrottleHigh``
    The desired upper limit for the disk load from file transfers, as
    configured by ``FILE_TRANSFER_DISK_LOAD_THROTTLE``. :index:`FILE_TRANSFER_DISK_LOAD_THROTTLE`
    This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.

:index:`FileTransferDiskThrottleLevel<single: FileTransferDiskThrottleLevel; ClassAd Scheduler attribute>`

``FileTransferDiskThrottleLevel``
    The current concurrency limit set by the disk load throttle. The
    limit is applied to the sum of uploads and downloads. This attribute
    is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.

:index:`FileTransferDiskThrottleLow<single: FileTransferDiskThrottleLow; ClassAd Scheduler attribute>`

``FileTransferDiskThrottleLow``
    The lower limit for the disk load from file transfers, as configured
    by ``FILE_TRANSFER_DISK_LOAD_THROTTLE``. :index:`FILE_TRANSFER_DISK_LOAD_THROTTLE`
    This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.

:index:`FileTransferDiskThrottleShortfall<single: FileTransferDiskThrottleShortfall; ClassAd Scheduler attribute>`

``FileTransferDiskThrottleShortfall_<timespan>``
    The exponential moving average of the disk load that falls below the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    ``FILE_TRANSFER_DISK_LOAD_THROTTLE`` is defined.

:index:`FileTransferDownloadBytes<single: FileTransferDownloadBytes; ClassAd Scheduler attribute>`

``FileTransferDownloadBytes``
    Total number of bytes downloaded as output from jobs since this
    *condor_schedd* was started. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH` 
    contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferDownloadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferDownloadBytesPerSecond<single: FileTransferDownloadBytesPerSecond; ClassAd Scheduler attribute>`

``FileTransferDownloadBytesPerSecond_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which bytes have been downloaded as output from jobs. The time
    spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS`` :index:`TRANSFER_IO_REPORT_TIMESPANS`
    , which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH` 
    contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferDownloadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferFileReadLoad<single: FileTransferFileReadLoad; ClassAd Scheduler attribute>`

``FileTransferFileReadLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time reading
    from files to be transferred as input to jobs. One file transfer
    process spending nearly all of its time reading files will generate
    a load close to 1.0. The time spans that are published are
    configured by ``TRANSFER_IO_REPORT_TIMESPANS`` :index:`TRANSFER_IO_REPORT_TIMESPANS`
    , which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferFileReadSeconds<single: FileTransferFileReadSeconds; ClassAd Scheduler attribute>`

``FileTransferFileReadSeconds``
    Total number of submit-side transfer process seconds spent reading
    from files to be transferred as input to jobs since this
    *condor_schedd* was started. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferFileWriteLoad<single: FileTransferFileWriteLoad; ClassAd Scheduler attribute>`

``FileTransferFileWriteLoad_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time writing
    to files transferred as output from jobs. One file transfer process
    spending nearly all of its time writing to files will generate a
    load close to 1.0. The time spans that are published are configured
    by ``TRANSFER_IO_REPORT_TIMESPANS`` :index:`TRANSFER_IO_REPORT_TIMESPANS`
    , which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferFileWriteSeconds<single: FileTransferFileWriteSeconds; ClassAd Scheduler attribute>`

``FileTransferFileWriteSeconds``
    Total number of submit-side transfer process seconds spent writing
    to files transferred as output from jobs since this *condor_schedd*
    was started. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileWriteSeconds``. The published
    user name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferFileNetReadLoad<single: FileTransferFileNetReadLoad; ClassAd Scheduler attribute>`

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
    ``TRANSFER_IO_REPORT_TIMESPANS`` :index:`TRANSFER_IO_REPORT_TIMESPANS`
    , which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferNetReadSeconds<single: FileTransferNetReadSeconds; ClassAd Scheduler attribute>`

``FileTransferNetReadSeconds``
    Total number of submit-side transfer process seconds spent reading
    from the network when transferring output from jobs since this
    *condor_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow reads from the disk on the execute
    side. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferNetWriteLoad<single: FileTransferNetWriteLoad; ClassAd Scheduler attribute>`

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
    ``TRANSFER_IO_REPORT_TIMESPANS``\ :index:`TRANSFER_IO_REPORT_TIMESPANS`,
    which defaults to 1m, 5m, 1h, and 1d. When less than one full time
    span has accumulated, the attribute is not published. If
    ``STATISTICS_TO_PUBLISH``\ :index:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is
    also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferNetWriteSeconds<single: FileTransferNetWriteSeconds; ClassAd Scheduler attribute>`

``FileTransferNetWriteSeconds``
    Total number of submit-side transfer process seconds spent writing
    to the network when transferring input to jobs since this
    *condor_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow writes to the disk on the execute side.
    The time spans that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS``, :index:`TRANSFER_IO_REPORT_TIMESPANS`  which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetWriteSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferUploadBytes<single: FileTransferUploadBytes; ClassAd Scheduler attribute>`

``FileTransferUploadBytes``
    Total number of bytes uploaded as input to jobs since this
    *condor_schedd* was started. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferUploadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`FileTransferUploadBytesPerSecond<single: FileTransferUploadBytesPerSecond; ClassAd Scheduler attribute>`

``FileTransferUploadBytesPerSecond_<timespan>``
    Exponential moving average over the specified time span of the rate
    at which bytes have been uploaded as input to jobs. The time spans
    that are published are configured by
    ``TRANSFER_IO_REPORT_TIMESPANS`` :index:`TRANSFER_IO_REPORT_TIMESPANS`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If ``STATISTICS_TO_PUBLISH`` :index:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferUploadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable ``TRANSFER_QUEUE_USER_EXPR``

:index:`TRANSFER_QUEUE_USER_EXPR`
:index:`TransferQueueMBWaitingToDownload<single: TransferQueueMBWaitingToDownload; ClassAd Scheduler attribute>`

``TransferQueueMBWaitingToDownload``
    Number of megabytes of output files waiting to be downloaded.

:index:`TransferQueueMBWaitingToUpload<single: TransferQueueMBWaitingToUpload; ClassAd Scheduler attribute>`

``TransferQueueMBWaitingToUpload``
    Number of megabytes of input files waiting to be uploaded.

:index:`TransferQueueNumWaitingToDownload<single: TransferQueueNumWaitingToDownload; ClassAd Scheduler attribute>`

``TransferQueueNumWaitingToDownload``
    Number of jobs waiting to transfer output files.

:index:`TransferQueueNumWaitingToUpload<single: TransferQueueNumWaitingToUpload; ClassAd Scheduler attribute>`

``TransferQueueNumWaitingToUpload``
    Number of jobs waiting to transfer input files.


