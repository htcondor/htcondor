Scheduler ClassAd Attributes
============================

:classad-attribute-def:`Autoclusters`
    A Statistics attribute defining the number of active autoclusters.

:classad-attribute-def:`CollectorHost`
    The name of the main *condor_collector* which this *condor_schedd*
    daemon reports to, as copied from :macro:`COLLECTOR_HOST`.
    If a *condor_schedd* flocks to other
    *condor_collector* daemons, this attribute still represents the
    "home" *condor_collector*, so this value can be used to discover if
    a *condor_schedd* is currently flocking.

:classad-attribute-def:`CondorVersion`
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:classad-attribute-def:`DaemonCoreDutyCycle`
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time for the time period defined
    by :ad-attr:`StatsLifetime` of this *condor_schedd*. A value near 0.0
    indicates an idle daemon, while a value near 1.0 indicates a daemon
    running at or above capacity.

:classad-attribute-def:`DaemonStartTime`
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`DaemonLastReconfigTime`
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`DetectedCpus`
    The number of detected machine CPUs/cores.

:classad-attribute-def:`DetectedMemory`
    The amount of detected machine RAM in MBytes.

:classad-attribute-def:`EffectiveFlockList`
    A comma separated list of *condor_collector* addresses to which
    *condor_schedd* jobs are actively flocking.

:classad-attribute-def:`JobQueueBirthdate`
    This attribute contains the Unix epoch time when the job_queue.log file which
    stores the scheduler's database was first created.

:classad-attribute-def:`JobsAccumBadputTime`
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully have spent running over the
    lifetime of this *condor_schedd*.

:classad-attribute-def:`JobsAccumExceptionalBadputTime`
    A Statistics attribute defining the sum of the all of the time jobs
    which did not complete successfully due to *condor_shadow*
    exceptions have spent running over the lifetime of this
    *condor_schedd*.

:classad-attribute-def:`JobsAccumRunningTime`.
    A Statistics attribute defining the sum of the all of the time jobs
    have spent running in the time interval defined by attribute
    :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsAccumTimeToStart`.
    A Statistics attribute defining the sum of all the time jobs have
    spent waiting to start in the time interval defined by attribute
    :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsBadputRuntimes`
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, over
    the lifetime of this *condor_schedd*. Counts within the histogram
    are separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    :ad-attr:`JobsRuntimesHistogramBuckets`.

:classad-attribute-def:`JobsBadputSizes`
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, over the
    lifetime of this *condor_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute :ad-attr:`JobsSizesHistogramBuckets`.

:classad-attribute-def:`JobsCheckpointed`
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor_shadow* exit code of ``JOB_CKPTED`` in the
    time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsCompleted`
    A Statistics attribute defining the number of jobs successfully
    completed in the time interval defined by attribute
    :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsCompletedRuntimes`
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by time spent running, over the
    lifetime of this *condor_schedd*. Counts within the histogram are
    separated by a comma and a space, where the time interval
    classification is defined in the ClassAd attribute
    :ad-attr:`JobsRuntimesHistogramBuckets`.

:classad-attribute-def:`JobsCompletedSizes`
    A Statistics attribute defining a histogram count of jobs that
    completed successfully as classified by image size, over the
    lifetime of this *condor_schedd*. Counts within the histogram are
    separated by a comma and a space, where the size classification is
    defined in the ClassAd attribute :ad-attr:`JobsSizesHistogramBuckets`.

:classad-attribute-def:`JobsCoredumped`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_COREDUMPED`` in
    the time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsDebugLogError`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``DPRINTF_ERROR`` in the
    time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsExecFailed`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsExited`
    A Statistics attribute defining the number of times that jobs that
    exited (successfully or not) in the time interval defined by
    attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsExitedAndClaimClosing`
    A Statistics attribute defining the number of times jobs have exited
    with a *condor_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time interval defined by
    attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsExitedNormally`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the time
    interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsExitException`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the time interval defined by attribute
    :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsKilled`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_KILLED`` in the
    time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsMissedDeferralTime`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the time interval defined by
    attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsNotStarted`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_NOT_STARTED`` in
    the time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsRestartReconnectsAttempting`
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* is currently attempting to reconnect
    to, in order to recover a job that was running when the
    *condor_schedd* was restarted.

:classad-attribute-def:`JobsRestartReconnectsBadput`
    A Statistics attribute defining a histogram count of
    *condor_startd* daemons that the *condor_schedd* could not
    reconnect to in order to recover a job that was running when the
    *condor_schedd* was restarted, as classified by the time the job
    spent running. Counts within the histogram are separated by a comma
    and a space, where the time interval classification is defined in
    the ClassAd attribute :ad-attr:`JobsRuntimesHistogramBuckets`.

:classad-attribute-def:`JobsRestartReconnectsFailed`
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* tried and failed to reconnect to in
    order to recover a job that was running when the *condor_schedd*
    was restarted.

:classad-attribute-def:`JobsRestartReconnectsInterrupted`
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* attempted to reconnect to, in order to
    recover a job that was running when the *condor_schedd* was
    restarted, but the attempt was interrupted, for example, because the
    job was removed.

:classad-attribute-def:`JobsRestartReconnectsLeaseExpired`
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* could not attempt to reconnect to, in
    order to recover a job that was running when the *condor_schedd*
    was restarted, because the job lease had already expired.

:classad-attribute-def:`JobsRestartReconnectsSucceeded`
    A Statistics attribute defining the number of *condor_startd*
    daemons the *condor_schedd* has successfully reconnected to, in
    order to recover a job that was running when the *condor_schedd*
    was restarted.

:classad-attribute-def:`JobsRunning`
    A Statistics attribute representing the number of jobs currently
    running.

:classad-attribute-def:`JobsRunningRuntimes`
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by elapsed runtime. Counts within the
    histogram are separated by a comma and a space, where the time
    interval classification is defined in the ClassAd attribute
    :ad-attr:`JobsRuntimesHistogramBuckets`.

:classad-attribute-def:`JobsRunningSizes`
    A Statistics attribute defining a histogram count of jobs currently
    running, as classified by image size. Counts within the histogram
    are separated by a comma and a space, where the size classification
    is defined in the ClassAd attribute :ad-attr:`JobsSizesHistogramBuckets`.

:classad-attribute-def:`JobsRuntimesHistogramBuckets`
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify run times. Defined as

    .. code-block:: condor-config

          JobsRuntimesHistogramBuckets = "30Sec, 1Min, 3Min, 10Min, 30Min, 1Hr, 3Hr, 6Hr, 12Hr, 1Day, 2Day, 4Day, 8Day, 16Day"


:classad-attribute-def:`JobsShadowNoMemory`
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor_shadow* in the time interval defined by attribute
    :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsShouldHold`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsShouldRemove`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsShouldRequeue`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsSizesHistogramBuckets`
    A Statistics attribute defining the predefined bucket boundaries for
    histogram statistics that classify image sizes. Defined as

    .. code-block:: condor-config

          JobsSizesHistogramBuckets = "64Kb, 256Kb, 1Mb, 4Mb, 16Mb, 64Mb, 256Mb, 1Gb, 4Gb, 16Gb, 64Gb, 256Gb"

    Note that these values imply powers of two in numbers of bytes.

:classad-attribute-def:`JobsStarted`.
    A Statistics attribute defining the number of jobs started in the
    time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsSubmitted`.
    A Statistics attribute defining the number of jobs submitted in the
    time interval defined by attribute :ad-attr:`StatsLifetime`.

:classad-attribute-def:`JobsUnmaterialized`.
    A Statistics attribute defining the number of jobs submitted as
    late materialization jobs that have not yet materialized.

:classad-attribute-def:`Machine`
    A string with the machine's fully qualified host name.

:classad-attribute-def:`MaxJobsRunning`
    The same integer value as set by the evaluation of the configuration
    variable :macro:`MAX_JOBS_RUNNING`. See the definition in the
    :ref:`admin-manual/configuration-macros:condor_schedd configuration file entries` section.

:classad-attribute-def:`MonitorSelfAge`
    The number of seconds that this daemon has been running.

:classad-attribute-def:`MonitorSelfCPUUsage`
    The fraction of recent CPU time utilized by this daemon.

:classad-attribute-def:`MonitorSelfImageSize`
    The amount of virtual memory consumed by this daemon in Kbytes.

:classad-attribute-def:`MonitorSelfRegisteredSocketCount`
    The current number of sockets registered by this daemon.

:classad-attribute-def:`MonitorSelfResidentSetSize`
    The amount of resident memory used by this daemon in Kbytes.

:classad-attribute-def:`MonitorSelfSecuritySessions`
    The number of open (cached) security sessions for this daemon.

:classad-attribute-def:`MonitorSelfTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.

:classad-attribute-def:`MyAddress`
    String with the IP and port address of the *condor_schedd* daemon
    which is publishing this ClassAd.

:classad-attribute-def:`MyCurrentTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_schedd*
    daemon last sent a ClassAd update to the *condor_collector*.

:classad-attribute-def:`Name`
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:classad-attribute-def:`NumJobStartsDelayed`
    The number times a job requiring a *condor_shadow* daemon could
    have been started, but was not started because of the values of
    configuration variables :macro:`JOB_START_COUNT` and :macro:`JOB_START_DELAY`

:classad-attribute-def:`NumPendingClaims`
    The number of machines (*condor_startd* daemons) matched to this
    *condor_schedd* daemon, which this *condor_schedd* knows about,
    but has not yet managed to claim.

:classad-attribute-def:`NumUsers`
    The integer number of distinct users with jobs in this
    *condor_schedd* 's queue.

:classad-attribute-def:`PublicNetworkIpAddr`
    This is the public network address of this daemon.

:classad-attribute-def:`RecentDaemonCoreDutyCycle`
    A Statistics attribute defining the ratio of the time spent handling
    messages and events to the elapsed time in the previous time
    interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsAccumBadputTime`
    A Statistics attribute defining the sum of the all of the time that
    jobs which did not complete successfully have spent running in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsAccumRunningTime`
    A Statistics attribute defining the sum of the all of the time jobs
    which have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` spent running.

:classad-attribute-def:`RecentJobsAccumTimeToStart`
    A Statistics attribute defining the sum of all the time jobs which
    have exited in the previous time interval defined by attribute
    ``RecentStatsLifetime`` had spent waiting to start.

:classad-attribute-def:`RecentJobsBadputRuntimes`
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by time spent running, in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``. Counts within the histogram are separated
    by a comma and a space, where the time interval classification is
    defined in the ClassAd attribute :ad-attr:`JobsRuntimesHistogramBuckets`.

:classad-attribute-def:`RecentJobsBadputSizes`
    A Statistics attribute defining a histogram count of jobs that did
    not complete successfully, as classified by image size, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the size classification is defined in the ClassAd attribute
    :ad-attr:`JobsSizesHistogramBuckets`.

:classad-attribute-def:`RecentJobsCheckpointed`
    A Statistics attribute defining the number of times jobs that have
    exited with a *condor_shadow* exit code of ``JOB_CKPTED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsCompleted`
    A Statistics attribute defining the number of jobs successfully
    completed in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsCompletedRuntimes`
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by time spent running, in the
    previous time interval defined by attribute ``RecentStatsLifetime``.
    Counts within the histogram are separated by a comma and a space,
    where the time interval classification is defined in the ClassAd
    attribute :ad-attr:`JobsRuntimesHistogramBuckets`.

:classad-attribute-def:`RecentJobsCompletedSizes`
    A Statistics attribute defining a histogram count of jobs that
    completed successfully, as classified by image size, in the previous
    time interval defined by attribute ``RecentStatsLifetime``. Counts
    within the histogram are separated by a comma and a space, where the
    size classification is defined in the ClassAd attribute
    :ad-attr:`JobsSizesHistogramBuckets`.

:classad-attribute-def:`RecentJobsCoredumped`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_COREDUMPED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsDebugLogError`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``DPRINTF_ERROR`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsExecFailed`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXEC_FAILED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsExited`
    A Statistics attribute defining the number of times that jobs have
    exited normally in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsExitedAndClaimClosing`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of
    ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous time interval
    defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsExitedNormally`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXITED`` or with
    an exit code of ``JOB_EXITED_AND_CLAIM_CLOSING`` in the previous
    time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsExitException`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_EXCEPTION`` or
    with an unknown status in the previous time interval defined by
    attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsKilled`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_KILLED`` in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsMissedDeferralTime`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of
    ``JOB_MISSED_DEFERRAL_TIME`` in the previous time interval defined
    by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsNotStarted`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_NOT_STARTED`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsShadowNoMemory`
    A Statistics attribute defining the number of times that jobs have
    exited because there was not enough memory to start the
    *condor_shadow* in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsShouldHold`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_HOLD`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsShouldRemove`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REMOVE`` in
    the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsShouldRequeue`
    A Statistics attribute defining the number of times that jobs have
    exited with a *condor_shadow* exit code of ``JOB_SHOULD_REQUEUE``
    in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsStarted`
    A Statistics attribute defining the number of jobs started in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentJobsSubmitted`
    A Statistics attribute defining the number of jobs submitted in the
    previous time interval defined by attribute ``RecentStatsLifetime``.

:classad-attribute-def:`RecentShadowsReconnections`
    A Statistics attribute defining the number of times that
    *condor_shadow* daemons lost connection to their *condor_starter*
    daemons and successfully reconnected in the previous time interval
    defined by attribute ``RecentStatsLifetime``. This statistic only
    appears in the Scheduler ClassAd if the level of verbosity set by
    the configuration variable :macro:`STATISTICS_TO_PUBLISH` is set to 2 or
    higher.

:classad-attribute-def:`RecentShadowsRecycled`
    A Statistics attribute defining the number of times *condor_shadow*
    processes have been recycled for use with a new job in the previous
    time interval defined by attribute ``RecentStatsLifetime``. This
    statistic only appears in the Scheduler ClassAd if the level of
    verbosity set by the configuration variable
    :macro:`STATISTICS_TO_PUBLISH` is set to 2 or higher.

:classad-attribute-def:`RecentShadowsStarted`
    A Statistics attribute defining the number of *condor_shadow*
    daemons started in the previous time interval defined by attribute
    ``RecentStatsLifetime``.

:classad-attribute-def:`RecentStatsLifetime`
    A Statistics attribute defining the time in seconds over which
    statistics values have been collected for attributes with names that
    begin with ``Recent``. This value starts at 0, and it may grow to a
    value as large as the value defined for attribute
    :ad-attr:`RecentWindowMax`.

:classad-attribute-def:`RecentStatsTickTime`
    A Statistics attribute defining the time that attributes with names
    that begin with ``Recent`` were last updated, represented as the
    number of seconds elapsed since the Unix epoch (00:00:00 UTC, Jan 1,
    1970). This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    :macro:`STATISTICS_TO_PUBLISH` is set to 2 or higher.

:classad-attribute-def:`RecentWindowMax`
    A Statistics attribute defining the maximum time in seconds over
    which attributes with names that begin with ``Recent`` are
    collected. The value is set by the configuration variable
    :macro:`STATISTICS_WINDOW_SECONDS`, which defaults to 1200
    seconds (20 minutes). This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    :macro:`STATISTICS_TO_PUBLISH` is set to 2 or higher.

:classad-attribute-def:`ScheddIpAddr`
    String with the IP and port address of the *condor_schedd* daemon
    which is publishing this Scheduler ClassAd.

:classad-attribute-def:`ShadowsReconnections`
    A Statistics attribute defining the number of times
    *condor_shadow* s lost connection to their *condor_starter* s
    and successfully reconnected in the previous :ad-attr:`StatsLifetime`
    seconds. This statistic only appears in the Scheduler ClassAd if the
    level of verbosity set by the configuration variable
    :macro:`STATISTICS_TO_PUBLISH` is set to 2 or higher.

:classad-attribute-def:`ShadowsRecycled`
    A Statistics attribute defining the number of times *condor_shadow*
    processes have been recycled for use with a new job in the previous
    :ad-attr:`StatsLifetime` seconds. This statistic only appears in the
    Scheduler ClassAd if the level of verbosity set by the configuration
    variable :macro:`STATISTICS_TO_PUBLISH` is set to 2 or higher.

:classad-attribute-def:`ShadowsRunning`
    A Statistics attribute defining the number of *condor_shadow*
    daemons currently running that are owned by this *condor_schedd*.

:classad-attribute-def:`ShadowsRunningPeak`
    A Statistics attribute defining the maximum number of
    *condor_shadow* daemons running at one time that were owned by this
    *condor_schedd* over the lifetime of this *condor_schedd*.

:classad-attribute-def:`ShadowsStarted`
    A Statistics attribute defining the number of *condor_shadow*
    daemons started in the previous time interval defined by attribute
    :ad-attr:`StatsLifetime`.

:classad-attribute-def:`StartLocalUniverse`
    The same boolean value as set in the configuration variable
    :macro:`START_LOCAL_UNIVERSE`. See the definition in the
    :ref:`admin-manual/configuration-macros:condor_schedd configuration file entries` section.

:classad-attribute-def:`StartSchedulerUniverse`
    The same boolean value as set in the configuration variable
    :macro:`START_SCHEDULER_UNIVERSE`. See the definition in the
    :ref:`admin-manual/configuration-macros:condor_schedd
    configuration file entries` section.

:classad-attribute-def:`StatsLastUpdateTime`
    A Statistics attribute defining the time that statistics about jobs
    were last updated, represented as the number of seconds elapsed
    since the Unix epoch (00:00:00 UTC, Jan 1, 1970). This statistic
    only appears in the Scheduler ClassAd if the level of verbosity set
    by the configuration variable :macro:`STATISTICS_TO_PUBLISH` is set to 2
    or higher.

:classad-attribute-def:`StatsLifetime`
    A Statistics attribute defining the time in seconds over which
    statistics have been collected for attributes with names that do not
    begin with ``Recent``. This statistic only appears in the Scheduler
    ClassAd if the level of verbosity set by the configuration variable
    :macro:`STATISTICS_TO_PUBLISH` is set to 2 or higher.

:classad-attribute-def:`TotalFlockedJobs`
    The total number of jobs from this *condor_schedd* daemon that are
    currently flocked to other pools.

:classad-attribute-def:`TotalHeldJobs`
    The total number of jobs from this *condor_schedd* daemon that are
    currently on hold.

:classad-attribute-def:`TotalIdleJobs`
    The total number of jobs from this *condor_schedd* daemon that are
    currently idle, not including local or scheduler universe jobs.

:classad-attribute-def:`TotalJobAds`
    The total number of all jobs (in all states) from this
    *condor_schedd* daemon.

:classad-attribute-def:`TotalLocalJobsIdle`
    The total number of **local**
    :subcom:`universe[and attribute TotalLocalJobsIdle]` jobs from
    this *condor_schedd* daemon that are currently idle.

:classad-attribute-def:`TotalLocalJobsRunning`
    The total number of **local**
    :subcom:`universe[and attribute TotalLocalJobsRunning]` jobs from
    this *condor_schedd* daemon that are currently running.

:classad-attribute-def:`TotalRemovedJobs`
    The current number of all running jobs from this *condor_schedd*
    daemon that have remove requests.

:classad-attribute-def:`TotalRunningJobs`
    The total number of jobs from this *condor_schedd* daemon that are
    currently running, not including local or scheduler universe jobs.

:classad-attribute-def:`TotalSchedulerJobsIdle`
    The total number of **scheduler**
    :subcom:`universe[and attribute TotalSchedulerJobsIdle]` jobs from
    this *condor_schedd* daemon that are currently idle.

:classad-attribute-def:`TotalSchedulerJobsRunning`
    The total number of **scheduler**
    :subcom:`universe[and attribute TotalSchedulerJobsRunning]` jobs from
    this *condor_schedd* daemon that are currently running.

:classad-attribute-def:`TransferQueueUserExpr`
    A ClassAd expression that provides the name of the transfer queue
    that the *condor_schedd* will be using for job file transfer.

:classad-attribute-def:`UpdateInterval`
    The interval, in seconds, between publication of this
    *condor_schedd* ClassAd and the previous publication.

:classad-attribute-def:`UpdateSequenceNumber`
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor_collector*. The *condor_collector* uses
    this value to sequence the updates it receives.

:classad-attribute-def:`VirtualMemory`
    Description is not yet written.

:classad-attribute-def:`WantResAd` causes the *condor_negotiator*
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
name is actually the file transfer queue name, as defined by configuration
variable :macro:`TRANSFER_QUEUE_USER_EXPR`. This expression defaults to
``Owner_`` followed by the name of the job owner. The attributes that
are rates have a suffix that specifies the time span of the exponential
moving average. By default the time spans that are published are 1m, 5m,
1h, and 1d. This can be changed by configuring configuration variable
:macro:`TRANSFER_IO_REPORT_TIMESPANS`. These attributes are only reported
once a full time span has accumulated.

:classad-attribute-def:`FileTransferDiskThrottleExcess_<timespan>`
    The exponential moving average of the disk load that exceeds the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE` is defined.

:classad-attribute-def:`FileTransferDiskThrottleHigh`
    The desired upper limit for the disk load from file transfers, as
    configured by :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE`
    This attribute is published only if configuration variable
    :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE` is defined.

:classad-attribute-def:`FileTransferDiskThrottleLevel`
    The current concurrency limit set by the disk load throttle. The
    limit is applied to the sum of uploads and downloads. This attribute
    is published only if configuration variable
    :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE` is defined.

:classad-attribute-def:`FileTransferDiskThrottleLow`
    The lower limit for the disk load from file transfers, as configured
    by :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE` This attribute is published
    only if configuration variable :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE`
    is defined.

:classad-attribute-def:`FileTransferDiskThrottleShortfall_<timespan>`
    The exponential moving average of the disk load that falls below the
    upper limit set for the disk load throttle. Periods of time in which
    there is no excess and no waiting transfers do not contribute to the
    average. This attribute is published only if configuration variable
    :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE` is defined.

:index:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferDownloadBytes`
    Total number of bytes downloaded as output from jobs since this
    *condor_schedd* was started. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute
    is also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferDownloadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferDownloadBytesPerSecond_<timespan>`
    Exponential moving average over the specified time span of the rate
    at which bytes have been downloaded as output from jobs. The time
    spans that are published are configured by :macro:`TRANSFER_IO_REPORT_TIMESPANS`
    , which defaults to 1m, 5m, 1h, and 1d. When less than one full
    time span has accumulated, the attribute is not published. If
    :macro:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferDownloadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferFileReadLoad_<timespan>`
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time reading
    from files to be transferred as input to jobs. One file transfer
    process spending nearly all of its time reading files will generate
    a load close to 1.0. The time spans that are published are configured
    by :macro:`TRANSFER_IO_REPORT_TIMESPANS`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is
    also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferFileReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferFileReadSeconds`
    Total number of submit-side transfer process seconds spent reading
    from files to be transferred as input to jobs since this
    *condor_schedd* was started. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferFileReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferFileWriteLoad_<timespan>`
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time writing
    to files transferred as output from jobs. One file transfer process
    spending nearly all of its time writing to files will generate a
    load close to 1.0. The time spans that are published are configured
    by :macro:`TRANSFER_IO_REPORT_TIMESPANS`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is
    also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferFileWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferFileWriteSeconds`
    Total number of submit-side transfer process seconds spent writing
    to files transferred as output from jobs since this *condor_schedd*
    was started. If :macro:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``,
    for each active user, this attribute is also published prefixed by
    the user name, with the name
    ``Owner_<username>_FileTransferFileWriteSeconds``. The published
    user name is actually the file transfer queue name, as defined by
    configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferNetReadLoad_<timespan>`
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time reading
    from the network when transferring output from jobs. One file
    transfer process spending nearly all of its time reading from the
    network will generate a load close to 1.0. The reason a file
    transfer process may spend a long time writing to the network could
    be a network bottleneck on the path between the submit and execute
    machine. It could also be caused by slow reads from the disk on the
    execute side. The time spans that are published are configured by
    :macro:`TRANSFER_IO_REPORT_TIMESPANS`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is
    also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetReadLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferNetReadSeconds`
    Total number of submit-side transfer process seconds spent reading
    from the network when transferring output from jobs since this
    *condor_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow reads from the disk on the execute
    side. If :macro:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for
    each active user, this attribute is also published prefixed by the
    user name, with the name
    ``Owner_<username>_FileTransferNetReadSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferNetWriteLoad_<timespan>`
    Exponential moving average over the specified time span of the rate
    at which submit-side file transfer processes have spent time writing
    to the network when transferring input to jobs. One file transfer
    process spending nearly all of its time writing to the network will
    generate a load close to 1.0. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow writes to the disk on the execute side.
    The time spans that are published are configured by
    :macro:`TRANSFER_IO_REPORT_TIMESPANS`, which defaults to 1m, 5m, 1h,
    and 1d. When less than one full time span has accumulated, the attribute
    is not published. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute is
    also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetWriteLoad_<timespan>``. The
    published user name is actually the file transfer queue name, as
    defined by configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferNetWriteSeconds`
    Total number of submit-side transfer process seconds spent writing
    to the network when transferring input to jobs since this
    *condor_schedd* was started. The reason a file transfer process may
    spend a long time writing to the network could be a network
    bottleneck on the path between the submit and execute machine. It
    could also be caused by slow writes to the disk on the execute side.
    The time spans that are published are configured by
    :macro:`TRANSFER_IO_REPORT_TIMESPANS`, which defaults to 1m,
    5m, 1h, and 1d. When less than one full time span has accumulated,
    the attribute is not published. If :macro:`STATISTICS_TO_PUBLISH` contains
    ``TRANSFER:2``, for each active user, this attribute is also published
    prefixed by the user name, with the name
    ``Owner_<username>_FileTransferNetWriteSeconds``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferUploadBytes`
    Total number of bytes uploaded as input to jobs since this
    *condor_schedd* was started. If :macro:`STATISTICS_TO_PUBLISH`
    contains ``TRANSFER:2``, for each active user, this attribute
    is also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferUploadBytes``. The published user
    name is actually the file transfer queue name, as defined by
    configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`FileTransferUploadBytesPerSecond_<timespan>`
    Exponential moving average over the specified time span of the rate
    at which bytes have been uploaded as input to jobs. The time spans
    that are published are configured by :macro:`TRANSFER_IO_REPORT_TIMESPANS`,
    which defaults to 1m, 5m, 1h, and 1d. When less than one full time
    span has accumulated, the attribute is not published. If
    :macro:`STATISTICS_TO_PUBLISH` contains ``TRANSFER:2``, for each active
    user, this attribute is also published prefixed by the user name, with the name
    ``Owner_<username>_FileTransferUploadBytesPerSecond_<timespan>``.
    The published user name is actually the file transfer queue name, as
    defined by configuration variable :macro:`TRANSFER_QUEUE_USER_EXPR`

:classad-attribute-def:`TransferQueueMBWaitingToDownload`
    Number of megabytes of output files waiting to be downloaded.

:classad-attribute-def:`TransferQueueMBWaitingToUpload`
    Number of megabytes of input files waiting to be uploaded.

:classad-attribute-def:`TransferQueueDownloadWaitTime`
    The time waiting in the transfer queue for the job that has been
    waiting to transfer output files the longest.

:classad-attribute-def:`TransferQueueUploadWaitTime`
    The time waiting in the transfer queue for the job that has been
    waiting to transfer input files the longest.

:classad-attribute-def:`TransferQueueNumWaitingToDownload`
    Number of jobs waiting to transfer output files.

:classad-attribute-def:`TransferQueueNumWaitingToUpload`
    Number of jobs waiting to transfer input files.

:classad-attribute-def:`TransferQueueNumDownloading`
    Number of jobs transfering output files.

:classad-attribute-def:`TransferQueueNumUploading`
    Number of jobs transfering input files.

:classad-attribute-def:`TransferQueueMaxDownloading`
    Maximum number of jobs transfering output files concurrently.

:classad-attribute-def:`TransferQueueMaxUploading`
    Maximum number of jobs transfering input files concurrently.
