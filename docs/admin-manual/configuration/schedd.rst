:index:`Schedd Daemon Options<single: Configuration; Schedd Daemon Options>`

.. _schedd_config_options:

Schedd Daemon Configuration Options
===================================

These macros control the *condor_schedd*.

:macro-def:`SHADOW`
    This macro determines the full path of the *condor_shadow* binary
    that the *condor_schedd* spawns. It is normally defined in terms of
    ``$(SBIN)``.

:macro-def:`START_LOCAL_UNIVERSE`
    A boolean value that defaults to ``TotalLocalJobsRunning < 200``.
    The *condor_schedd* uses this macro to determine whether to start a
    **local** universe job. At intervals determined by
    :macro:`SCHEDD_INTERVAL`, the *condor_schedd* daemon evaluates this
    macro for each idle **local** universe job that it has. For each
    job, if the :macro:`START_LOCAL_UNIVERSE` macro is ``True``, then the
    job's ``Requirements`` :index:`Requirements` expression is
    evaluated. If both conditions are met, then the job is allowed to
    begin execution.

    The following example only allows 10 **local** universe jobs to
    execute concurrently. The attribute :ad-attr:`TotalLocalJobsRunning` is
    supplied by *condor_schedd* 's ClassAd:

    .. code-block:: condor-config

            START_LOCAL_UNIVERSE = TotalLocalJobsRunning < 10


:macro-def:`STARTER_LOCAL`
    The complete path and executable name of the *condor_starter* to
    run for **local** universe jobs. This variable's value is defined in
    the initial configuration provided with HTCondor as

    .. code-block:: condor-config

          STARTER_LOCAL = $(SBIN)/condor_starter


    This variable would only be modified or hand added into the
    configuration for a pool to be upgraded from one running a version
    of HTCondor that existed before the **local** universe to one that
    includes the **local** universe, but without utilizing the newer,
    provided configuration files.

:macro-def:`LOCAL_UNIV_EXECUTE`
    A string value specifying the execute location for local universe
    jobs. Each running local universe job will receive a uniquely named
    subdirectory within this directory. If not specified, it defaults to
    ``$(SPOOL)/local_univ_execute``.

:macro-def:`USE_CGROUPS_FOR_LOCAL_UNIVERSE`
    A boolean value that defaults to true.  When true, local universe
    jobs on Linux are put into their own cgroup, for monitoring and
    cleanup.

:macro-def:`LOCAL_UNIVERSE_CGROUP_ENFORCEMENT`
    When the above is true, if this boolean value which defaults to false
    is true, then local universe jobs need to have a :subcom:`request_memory`
    and if the local universe job exceeds that, it will be put on hold.

:macro-def:`START_SCHEDULER_UNIVERSE`
    A boolean value that defaults to
    ``TotalSchedulerJobsRunning < 500``. The *condor_schedd* uses this
    macro to determine whether to start a **scheduler** universe job. At
    intervals determined by :macro:`SCHEDD_INTERVAL`, the *condor_schedd*
    daemon evaluates this macro for each idle **scheduler** universe job
    that it has. For each job, if the :macro:`START_SCHEDULER_UNIVERSE` macro
    is ``True``, then the job's ``Requirements``
    :index:`Requirements` expression is evaluated. If both
    conditions are met, then the job is allowed to begin execution.

    The following example only allows 10 **scheduler** universe jobs to
    execute concurrently. The attribute :ad-attr:`TotalSchedulerJobsRunning` is
    supplied by *condor_schedd* 's ClassAd:

    .. code-block:: condor-config

            START_SCHEDULER_UNIVERSE = TotalSchedulerJobsRunning < 10

:macro-def:`START_VANILLA_UNIVERSE`
    A boolean expression that defaults to nothing.
    When this macro is defined the *condor_schedd* uses it
    to determine whether to start a **vanilla** universe job.
    The *condor_schedd* uses the expression
    when matching a job with a slot in addition to the ``Requirements``
    expression of the job and the slot ClassAds.  The expression can
    refer to job attributes by using the prefix ``JOB``, slot attributes
    by using the prefix ``SLOT``, job owner attributes by using the
    prefix ``OWNER``, and attributes from the schedd ad by using the
    prefix ``SCHEDD``.

    The following example prevents jobs owned by a user from starting when
    that user has more than 25 held jobs

    .. code-block:: condor-config

            START_VANILLA_UNIVERSE = OWNER.JobsHeld <= 25


:macro-def:`SCHEDD_USES_STARTD_FOR_LOCAL_UNIVERSE`
    A boolean value that defaults to false. When true, the
    *condor_schedd* will spawn a special startd process to run local
    universe jobs. This allows local universe jobs to run with both a
    condor_shadow and a condor_starter, which means that file transfer
    will work with local universe jobs.

:macro-def:`MAX_JOBS_RUNNING`
    An integer representing a limit on the number of *condor_shadow*
    processes spawned by a given *condor_schedd* daemon, for all job
    universes except grid, scheduler, and local universe. Limiting the
    number of running scheduler and local universe jobs can be done
    using :macro:`START_LOCAL_UNIVERSE` and :macro:`START_SCHEDULER_UNIVERSE`. The
    actual number of allowed *condor_shadow* daemons may be reduced, if
    the amount of memory defined by :macro:`RESERVED_SWAP` limits the number
    of *condor_shadow* daemons. A value for :macro:`MAX_JOBS_RUNNING` that
    is less than or equal to 0 prevents any new job from starting.
    Changing this setting to be below the current number of jobs that
    are running will cause running jobs to be aborted until the number
    running is within the limit.

    Like all integer configuration variables, :macro:`MAX_JOBS_RUNNING` may
    be a ClassAd expression that evaluates to an integer, and which
    refers to constants either directly or via macro substitution. The
    default value is an expression that depends on the total amount of
    memory and the operating system. The default expression requires
    1MByte of RAM per running job on the access point. In some
    environments and configurations, this is overly generous and can be
    cut by as much as 50%. On Windows platforms, the number of running
    jobs is capped at 2000. A 64-bit version of Windows is recommended
    in order to raise the value above the default. Under Unix, the
    maximum default is now 10,000. To scale higher, we recommend that
    the system ephemeral port range is extended such that there are at
    least 2.1 ports per running job.

    Here are example configurations:

    .. code-block:: condor-config

        ## Example 1:
        MAX_JOBS_RUNNING = 10000

        ## Example 2:
        ## This is more complicated, but it produces the same limit as the default.
        ## First define some expressions to use in our calculation.
        ## Assume we can use up to 80% of memory and estimate shadow private data
        ## size of 800k.
        MAX_SHADOWS_MEM = ceiling($(DETECTED_MEMORY)*0.8*1024/800)
        ## Assume we can use ~21,000 ephemeral ports (avg ~2.1 per shadow).
        ## Under Linux, the range is set in /proc/sys/net/ipv4/ip_local_port_range.
        MAX_SHADOWS_PORTS = 10000
        ## Under windows, things are much less scalable, currently.
        ## Note that this can probably be safely increased a bit under 64-bit windows.
        MAX_SHADOWS_OPSYS = ifThenElse(regexp("WIN.*","$(OPSYS)"),2000,100000)
        ## Now build up the expression for MAX_JOBS_RUNNING.  This is complicated
        ## due to lack of a min() function.
        MAX_JOBS_RUNNING = $(MAX_SHADOWS_MEM)
        MAX_JOBS_RUNNING = \
          ifThenElse( $(MAX_SHADOWS_PORTS) < $(MAX_JOBS_RUNNING), \
                      $(MAX_SHADOWS_PORTS), \
                      $(MAX_JOBS_RUNNING) )
        MAX_JOBS_RUNNING = \
          ifThenElse( $(MAX_SHADOWS_OPSYS) < $(MAX_JOBS_RUNNING), \
                      $(MAX_SHADOWS_OPSYS), \
                      $(MAX_JOBS_RUNNING) )

:macro-def:`MAX_JOBS_SUBMITTED`
    This integer value limits the number of jobs permitted in a
    *condor_schedd* daemon's queue. Submission of a new cluster of jobs
    fails, if the total number of jobs would exceed this limit. The
    default value for this variable is the largest positive integer
    value.

:macro-def:`MAX_JOBS_PER_OWNER`
    This integer value limits the number of jobs any given owner (user)
    is permitted to have within a *condor_schedd* daemon's queue. A job
    submission fails if it would cause this limit on the number of jobs
    to be exceeded. The default value is 100000.

    This configuration variable may be most useful in conjunction with
    :macro:`MAX_JOBS_SUBMITTED`, to ensure that no one user can dominate the
    queue.

:macro-def:`ALLOW_SUBMIT_FROM_KNOWN_USERS_ONLY`
    This boolean value determines if a User record will be created automatically
    when an unknown user submits a job.
    When true, only daemons or users that have a User record in the *condor_schedd*
    can submit jobs. When false, a User record will be added during job submission
    for users that have never submitted a job before. The default value is false
    which is consistent with the behavior of the *condor_schedd* before User records
    were added.

:macro-def:`MAX_RUNNING_SCHEDULER_JOBS_PER_OWNER`
    This integer value limits the number of scheduler universe jobs that
    any given owner (user) can have running at one time. This limit will
    affect the number of running Dagman jobs, but not the number of
    nodes within a DAG. The default value is 200

:macro-def:`MAX_JOBS_PER_SUBMISSION`
    This integer value limits the number of jobs any single submission
    is permitted to add to a *condor_schedd* daemon's queue. The whole
    submission fails if the number of jobs would exceed this limit. The
    default value is 20000.

    This configuration variable may be useful for catching user error,
    and for protecting a busy *condor_schedd* daemon from the
    excessively lengthy interruption required to accept a very large
    number of jobs at one time.

:macro-def:`MAX_SHADOW_EXCEPTIONS`
    This macro controls the maximum number of times that
    *condor_shadow* processes can have a fatal error (exception) before
    the *condor_schedd* will relinquish the match associated with the
    dying shadow. Defaults to 2.

:macro-def:`MAX_PENDING_STARTD_CONTACTS`
    An integer value that limits the number of simultaneous connection
    attempts by the *condor_schedd* when it is requesting claims from
    one or more *condor_startd* daemons. The intention is to protect
    the *condor_schedd* from being overloaded by authentication
    operations. The default value is 0. The special value 0 indicates no
    limit.

:macro-def:`CURB_MATCHMAKING`
    A ClassAd expression evaluated by the *condor_schedd* in the
    context of the *condor_schedd* daemon's own ClassAd. While this
    expression evaluates to ``True``, the *condor_schedd* will refrain
    from requesting more resources from a *condor_negotiator*. Defaults
    to ``RecentDaemonCoreDutyCycle > 0.98``.

:macro-def:`MAX_CONCURRENT_DOWNLOADS`
    This specifies the maximum number of simultaneous transfers of
    output files from execute machines to the access point. The limit
    applies to all jobs submitted from the same *condor_schedd*. The
    default is 100. A setting of 0 means unlimited transfers. This limit
    currently does not apply to grid universe jobs,
    and it also does not apply to streaming output files. When the
    limit is reached, additional transfers will queue up and wait before
    proceeding.

:macro-def:`MAX_CONCURRENT_UPLOADS`
    This specifies the maximum number of simultaneous transfers of input
    files from the access point to execute machines. The limit applies
    to all jobs submitted from the same *condor_schedd*. The default is
    100. A setting of 0 means unlimited transfers. This limit currently
    does not apply to grid universe jobs. When
    the limit is reached, additional transfers will queue up and wait
    before proceeding.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE`
    This configures throttling of file transfers based on the disk load
    generated by file transfers. The maximum number of concurrent file
    transfers is specified by :macro:`MAX_CONCURRENT_UPLOADS` and
    :macro:`MAX_CONCURRENT_DOWNLOADS`. Throttling will dynamically
    reduce the level of concurrency further to attempt to prevent disk
    load from exceeding the specified level. Disk load is computed as
    the average number of file transfer processes conducting read/write
    operations at the same time. The throttle may be specified as a
    single floating point number or as a range. Syntax for the range is
    the smaller number followed by 1 or more spaces or tabs, the string
    ``"to"``, 1 or more spaces or tabs, and then the larger number.
    Example:

    .. code-block:: condor-config

          FILE_TRANSFER_DISK_LOAD_THROTTLE = 5 to 6.5

    If only a single number is provided, this serves as the upper limit,
    and the lower limit is set to 90% of the upper limit. When the disk
    load is above the upper limit, no new transfers will be started.
    When between the lower and upper limits, new transfers will only be
    started to replace ones that finish. The default value is 2.0.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE_WAIT_BETWEEN_INCREMENTS`
    This rarely configured variable sets the waiting period between
    increments to the concurrency level set by
    :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE`. The default is 1 minute. A
    value that is too short risks starting too many transfers before
    their effect on the disk load becomes apparent.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE_SHORT_HORIZON`
    This rarely configured variable specifies the string name of the
    short monitoring time span to use for throttling. The named time
    span must exist in :macro:`TRANSFER_IO_REPORT_TIMESPANS`. The
    default is ``1m``, which is 1 minute.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE_LONG_HORIZON`
    This rarely configured variable specifies the string name of the
    long monitoring time span to use for throttling. The named time span
    must exist in :macro:`TRANSFER_IO_REPORT_TIMESPANS`. The default is
    ``5m``, which is 5 minutes.

:macro-def:`TRANSFER_QUEUE_USER_EXPR`
    This rarely configured expression specifies the user name to be used
    for scheduling purposes in the file transfer queue. The scheduler
    attempts to give equal weight to each user when there are multiple
    jobs waiting to transfer files within the limits set by
    :macro:`MAX_CONCURRENT_UPLOADS` and/or :macro:`MAX_CONCURRENT_DOWNLOADS`.
    When choosing a new job to allow to transfer, the first job belonging
    to the transfer queue user who has least number of active transfers
    will be selected. In case of a tie, the user who has least recently
    been given an opportunity to start a transfer will be selected. By
    default, a transfer queue user is identified as the job owner. A
    different user name may be specified by configuring :macro:`TRANSFER_QUEUE_USER_EXPR`
    to a string expression that is evaluated in the context of the job ad.
    For example, if this expression were set to a name that is the same
    for all jobs, file transfers would be scheduled in
    first-in-first-out order rather than equal share order. Note that
    the string produced by this expression is used as a prefix in the
    ClassAd attributes for per-user file transfer I/O statistics that
    are published in the *condor_schedd* ClassAd.

:macro-def:`MAX_TRANSFER_INPUT_MB`
    This integer expression specifies the maximum allowed total size in
    MiB of the input files that are transferred for a job. This
    expression does not apply to grid universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value ``-1`` indicates
    no limit. The default value is -1. The job may override the system
    setting by specifying its own limit using the :ad-attr:`MaxTransferInputMB`
    attribute. If the observed size of all input files at submit time is
    larger than the limit, the job will be immediately placed on hold
    with a :ad-attr:`HoldReasonCode` value of 32. If the job passes this
    initial test, but the size of the input files increases or the limit
    decreases so that the limit is violated, the job will be placed on
    hold at the time when the file transfer is attempted.

:macro-def:`MAX_TRANSFER_OUTPUT_MB`
    This integer expression specifies the maximum allowed total size in
    MiB of the output files that are transferred for a job. This
    expression does not apply to grid universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value ``-1`` indicates
    no limit. The default value is -1. The job may override the system
    setting by specifying its own limit using the
    :ad-attr:`MaxTransferOutputMB` attribute. If the total size of the job's
    output files to be transferred is larger than the limit, the job
    will be placed on hold with a :ad-attr:`HoldReasonCode` value of 33. The
    output will be transferred up to the point when the limit is hit, so
    some files may be fully transferred, some partially, and some not at
    all.

:macro-def:`MAX_TRANSFER_QUEUE_AGE`
    The number of seconds after which an aged and queued transfer may be
    dequeued from the transfer queue, as it is presumably hung. Defaults
    to 7200 seconds, which is 120 minutes.

:macro-def:`TRANSFER_IO_REPORT_INTERVAL`
    The sampling interval in seconds for collecting I/O statistics for
    file transfer. The default is 10 seconds. To provide sufficient
    resolution, the sampling interval should be small compared to the
    smallest time span that is configured in
    :macro:`TRANSFER_IO_REPORT_TIMESPANS`. The shorter the sampling interval,
    the more overhead of data collection, which may slow down the
    *condor_schedd*. See :doc:`/classad-attributes/scheduler-classad-attributes`
    for a description of the published attributes.

:macro-def:`TRANSFER_IO_REPORT_TIMESPANS`
    A string that specifies a list of time spans over which I/O
    statistics are reported, using exponential moving averages (like the
    1m, 5m, and 15m load averages in Unix). Each entry in the list
    consists of a label followed by a colon followed by the number of
    seconds over which the named time span should extend. The default is
    1m:60 5m:300 1h:3600 1d:86400. To provide sufficient resolution, the
    smallest reported time span should be large compared to the sampling
    interval, which is configured by :macro:`TRANSFER_IO_REPORT_INTERVAL`.
    See :doc:`/classad-attributes/scheduler-classad-attributes` for a
    description of the published attributes.

:macro-def:`SCHEDD_QUERY_WORKERS`
    This specifies the maximum number of concurrent sub-processes that
    the *condor_schedd* will spawn to handle queries. The setting is
    ignored in Windows. In Unix, the default is 8. If the limit is
    reached, the next query will be handled in the *condor_schedd* 's
    main process.

:macro-def:`CONDOR_Q_USE_V3_PROTOCOL`
    A boolean value that, when ``True``, causes the *condor_schedd* to
    use an algorithm that responds to :tool:`condor_q` requests by not
    forking itself to handle each request. It instead handles the
    requests in a non-blocking way. The default value is ``True``.

:macro-def:`CONDOR_Q_DASH_BATCH_IS_DEFAULT`
    A boolean value that, when ``True``, causes :tool:`condor_q` to print the
    **-batch** output unless the **-nobatch** option is used or the
    other arguments to :tool:`condor_q` are incompatible with batch mode. For
    instance **-long** is incompatible with **-batch**. The default
    value is ``True``.

:macro-def:`CONDOR_Q_ONLY_MY_JOBS`
    A boolean value that, when ``True``, causes :tool:`condor_q` to request
    that only the current user's jobs be queried unless the current user
    is a queue superuser. It also causes the *condor_schedd* to honor
    that request. The default value is ``True``. A value of ``False`` in
    either :tool:`condor_q` or the *condor_schedd* will result in the old
    behavior of querying all jobs.

:macro-def:`CONDOR_Q_SHOW_OLD_SUMMARY`
    A boolean value that, when ``True``, causes :tool:`condor_q` to show the
    old single line summary totals. When ``False`` :tool:`condor_q` will show
    the new multi-line summary totals.

:macro-def:`SCHEDD_MIN_INTERVAL`
    This macro determines the minimum interval for both how often the
    *condor_schedd* sends a ClassAd update to the *condor_collector*
    and how often the *condor_schedd* daemon evaluates jobs. It is
    defined in terms of seconds and defaults to 5 seconds.

:macro-def:`SCHEDD_INTERVAL`
    This macro determines the maximum interval for both how often the
    *condor_schedd* sends a ClassAd update to the *condor_collector*
    and how often the *condor_schedd* daemon evaluates jobs. It is
    defined in terms of seconds and defaults to 300 (every 5 minutes).

:macro-def:`SCHEDD_HISTORY_RECORD_INTERVAL`
    An integer value representing the maximum interval between writing
    the Schedd's ClassAd to the :macro:`SCHEDD_DAEMON_HISTORY` file. This is
    defined in terms of seconds and defaults to 900 (every 15 minutes).

:macro-def:`ABSENT_SUBMITTER_LIFETIME`
    This macro determines the maximum time that the *condor_schedd*
    will remember a submitter after the last job for that submitter
    leaves the queue. It is defined in terms of seconds and defaults to
    1 week.

:macro-def:`ABSENT_SUBMITTER_UPDATE_RATE`
    This macro can be used to set the maximum rate at which the
    *condor_schedd* sends updates to the *condor_collector* for
    submitters that have no jobs in the queue. It is defined in terms of
    seconds and defaults to 300 (every 5 minutes).

:macro-def:`WINDOWED_STAT_WIDTH`
    The number of seconds that forms a time window within which
    performance statistics of the *condor_schedd* daemon are
    calculated. Defaults to 300 seconds.

:macro-def:`SCHEDD_INTERVAL_TIMESLICE`
    The bookkeeping done by the *condor_schedd* takes more time when
    there are large numbers of jobs in the job queue. However, when it
    is not too expensive to do this bookkeeping, it is best to keep the
    collector up to date with the latest state of the job queue.
    Therefore, this macro is used to adjust the bookkeeping interval so
    that it is done more frequently when the cost of doing so is
    relatively small, and less frequently when the cost is high. The
    default is 0.05, which means the schedd will adapt its bookkeeping
    interval to consume no more than 5% of the total time available to
    the schedd. The lower bound is configured by :macro:`SCHEDD_MIN_INTERVAL`
    (default 5 seconds), and the upper bound is configured by
    :macro:`SCHEDD_INTERVAL` (default 300 seconds).

:macro-def:`JOB_START_COUNT`
    This macro works together with the :macro:`JOB_START_DELAY` macro
    to throttle job starts. The default and minimum values for this
    integer configuration variable are both 1.

:macro-def:`JOB_START_DELAY`
    This integer-valued macro works together with the :macro:`JOB_START_COUNT`
    macro to throttle job starts. The *condor_schedd* daemon starts
    ``$(JOB_START_COUNT)`` jobs at a time, then delays for
    ``$(JOB_START_DELAY)`` seconds before starting the next set of jobs.
    This delay prevents a sudden, large load on resources required by
    the jobs during their start up phase. The resulting job start rate
    averages as fast as (``$(JOB_START_COUNT)``/``$(JOB_START_DELAY)``)
    jobs/second. This setting is defined in terms of seconds and
    defaults to 0, which means jobs will be started as fast as possible.
    If you wish to throttle the rate of specific types of jobs, you can
    use the job attribute :ad-attr:`NextJobStartDelay`.

:macro-def:`MAX_NEXT_JOB_START_DELAY`
    An integer number of seconds representing the maximum allowed value
    of the job ClassAd attribute :ad-attr:`NextJobStartDelay`. It defaults to
    600, which is 10 minutes.

:macro-def:`JOB_STOP_COUNT`
    An integer value representing the number of jobs operated on at one
    time by the *condor_schedd* daemon, when throttling the rate at
    which jobs are stopped via :tool:`condor_rm`, :tool:`condor_hold`, or
    :tool:`condor_vacate_job`. The default and minimum values are both 1.
    This variable is ignored for grid and scheduler universe jobs.

:macro-def:`JOB_STOP_DELAY`
    An integer value representing the number of seconds delay utilized
    by the *condor_schedd* daemon, when throttling the rate at which
    jobs are stopped via :tool:`condor_rm`, :tool:`condor_hold`, or
    :tool:`condor_vacate_job`. The *condor_schedd* daemon stops
    ``$(JOB_STOP_COUNT)`` jobs at a time, then delays for
    ``$(JOB_STOP_DELAY)`` seconds before stopping the next set of jobs.
    This delay prevents a sudden, large load on resources required by
    the jobs when they are terminating. The resulting job stop rate
    averages as fast as ``JOB_STOP_COUNT/JOB_STOP_DELAY`` jobs per
    second. This configuration variable is also used during the graceful
    shutdown of the *condor_schedd* daemon. During graceful shutdown,
    this macro determines the wait time in between requesting each
    *condor_shadow* daemon to gracefully shut down. The default value
    is 0, which means jobs will be stopped as fast as possible. This
    variable is ignored for grid and scheduler universe jobs.

:macro-def:`JOB_IS_FINISHED_COUNT`
    An integer value representing the number of jobs that the
    *condor_schedd* will let permanently leave the job queue each time
    that it examines the jobs that are ready to do so. The default value
    is 1.

:macro-def:`JOB_IS_FINISHED_INTERVAL`
    The *condor_schedd* maintains a list of jobs that are ready to
    permanently leave the job queue, for example, when they have
    completed or been removed. This integer-valued macro specifies a
    delay in seconds between instances of taking jobs permanently out of
    the queue. The default value is 0, which tells the *condor_schedd*
    to not impose any delay.

:macro-def:`ALIVE_INTERVAL`
    An initial value for an integer number of seconds defining how often
    the *condor_schedd* sends a UDP keep alive message to any
    *condor_startd* it has claimed. When the *condor_schedd* claims a
    *condor_startd*, the *condor_schedd* tells the *condor_startd*
    how often it is going to send these messages. The utilized interval
    for sending keep alive messages is the smallest of the two values
    :macro:`ALIVE_INTERVAL` and the expression ``JobLeaseDuration/3``, formed
    with the job ClassAd attribute :ad-attr:`JobLeaseDuration`. The value of
    the interval is further constrained by the floor value of 10
    seconds. If the *condor_startd* does not receive any of these keep
    alive messages during a certain period of time (defined via
    :macro:`MAX_CLAIM_ALIVES_MISSED`) the  *condor_startd* releases the claim,
    and the *condor_schedd* no longer pays for the resource (in terms of
    user priority in the system). The macro is defined in terms of seconds
    and defaults to 300, which is 5 minutes.

:macro-def:`REQUEST_CLAIM_TIMEOUT`
    This macro sets the time (in seconds) that the *condor_schedd* will
    wait for a claim to be granted by the *condor_startd*. The default
    is 30 minutes. This is only likely to matter if
    :macro:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION` is ``True``, and
    the *condor_startd* has an existing claim, and it takes a long time
    for the existing claim to be preempted due to
    ``MaxJobRetirementTime``. Once a request times out, the
    *condor_schedd* will simply begin the process of finding a machine
    for the job all over again.

    Normally, it is not a good idea to set this to be very small, where
    a small value is a few minutes. Doing so can lead to failure to
    preempt, because the preempting job will spend a significant
    fraction of its time waiting to be re-matched. During that time, it
    would miss out on any opportunity to run if the job it is trying to
    preempt gets out of the way.

:macro-def:`SHADOW_SIZE_ESTIMATE`
    The estimated private virtual memory size of each *condor_shadow*
    process in KiB. This value is only used if :macro:`RESERVED_SWAP` is
    non-zero. The default value is 800.

:macro-def:`SHADOW_RENICE_INCREMENT`
    When the *condor_schedd* spawns a new *condor_shadow*, it can do
    so with a nice-level. A nice-level is a Unix mechanism that allows
    users to assign their own processes a lower priority so that the
    processes run with less priority than other tasks on the machine.
    The value can be any integer between 0 and 19, with a value of 19
    being the lowest priority. It defaults to 0.

:macro-def:`SCHED_UNIV_RENICE_INCREMENT`
    Analogous to :macro:`JOB_RENICE_INCREMENT` and
    :macro:`SHADOW_RENICE_INCREMENT`, scheduler universe jobs can be given a
    nice-level. The value can be any integer between 0 and 19, with a
    value of 19 being the lowest priority. It defaults to 0.

:macro-def:`QUEUE_CLEAN_INTERVAL`
    The *condor_schedd* maintains the job queue on a given machine. It
    does so in a persistent way such that if the *condor_schedd*
    crashes, it can recover a valid state of the job queue. The
    mechanism it uses is a transaction-based log file (the
    ``job_queue.log`` file, not the ``SchedLog`` file). This file
    contains an initial state of the job queue, and a series of
    transactions that were performed on the queue (such as new jobs
    submitted or jobs completing). Periodically, the
    *condor_schedd* will go through this log, truncate all the
    transactions and create a new file with containing only the new
    initial state of the log. This is a somewhat expensive operation,
    but it speeds up when the *condor_schedd* restarts since there are
    fewer transactions it has to play to figure out what state the job
    queue is really in. This macro determines how often the
    *condor_schedd* should rework this queue to cleaning it up. It is
    defined in terms of seconds and defaults to 86400 (once a day).

:macro-def:`WALL_CLOCK_CKPT_INTERVAL`
    The job queue contains a counter for each job's "wall clock" run
    time, i.e., how long each job has executed so far. This counter is
    displayed by :tool:`condor_q`. The counter is updated when the job is
    evicted or when the job completes. When the *condor_schedd*
    crashes, the run time for jobs that are currently running will not
    be added to the counter (and so, the run time counter may become
    smaller than the CPU time counter). The *condor_schedd* saves run
    time "checkpoints" periodically for running jobs so if the
    *condor_schedd* crashes, only run time since the last checkpoint is
    lost. This macro controls how often the *condor_schedd* saves run
    time checkpoints. It is defined in terms of seconds and defaults to
    3600 (one hour). A value of 0 will disable wall clock checkpoints.

:macro-def:`QUEUE_ALL_USERS_TRUSTED`
    Defaults to False. If set to True, then unauthenticated users are
    allowed to write to the queue, and also we always trust whatever the
    :ad-attr:`Owner` value is set to be by the client in the job ad. This was
    added so users can continue to use the SOAP web-services interface
    over HTTP (w/o authenticating) to submit jobs in a secure,
    controlled environment - for instance, in a portal setting.

:macro-def:`QUEUE_SUPER_USERS`
    A comma and/or space separated list of user names on a given machine
    that are given super-user access to the job queue, meaning that they
    can modify or delete the job ClassAds of other users. These should be
    of form ``USER@DOMAIN``; if the domain is not present in the username,
    HTCondor will assume the default :macro:`UID_DOMAIN`. When not on
    this list, users can only modify or delete their own ClassAds from
    the job queue. Whatever user name corresponds with the UID that
    HTCondor is running as - usually user condor - will automatically be
    included in this list, because that is needed for HTCondor's proper
    functioning. See
    :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    on UIDs in HTCondor for more details on this. By default, the Unix user root
    and the Windows user administrator are given the ability to remove
    other user's jobs, in addition to user condor. In addition to a
    single user, Unix user groups may be specified by using a special
    syntax defined for this configuration variable; the syntax is the
    percent character (``%``) followed by the user group name. All
    members of the user group are given super-user access.

:macro-def:`QUEUE_SUPER_USER_MAY_IMPERSONATE`
    A regular expression that matches the operating system user names
    (that is, job owners in the form ``USER``) that the queue super user
    may impersonate when managing jobs.  This allows the admin to limit
    the operating system users a super user can launch jobs as.
    When not set, the default behavior is to allow impersonation of any
    user who has had a job in the queue during the life of the
    *condor_schedd*. For proper functioning of the *condor_shadow*,
    the *condor_gridmanager*, and the *condor_job_router*, this
    expression, if set, must match the owner names of all jobs that
    these daemons will manage. Note that a regular expression that
    matches only part of the user name is still considered a match. If
    acceptance of partial matches is not desired, the regular expression
    should begin with ^ and end with $.

:macro-def:`SYSTEM_JOB_MACHINE_ATTRS`
    This macro specifies a space and/or comma separated list of machine
    attributes that should be recorded in the job ClassAd. The default
    attributes are :ad-attr:`Cpus` and :ad-attr:`SlotWeight`. When there are multiple
    run attempts, history of machine attributes from previous run
    attempts may be kept. The number of run attempts to store is
    specified by the configuration variable
    :macro:`SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH`. A machine
    attribute named ``X`` will be inserted into the job ClassAd as an
    attribute named ``MachineAttrX0``. The previous value of this
    attribute will be named ``MachineAttrX1``, the previous to that will
    be named ``MachineAttrX2``, and so on, up to the specified history
    length. A history of length 1 means that only ``MachineAttrX0`` will
    be recorded. Additional attributes to record may be specified on a
    per-job basis by using the
    :subcom:`job_machine_attrs[and SYSTEM_JOB_MACHINE_ATTRS]`
    submit file command. The value recorded in the job ClassAd is the
    evaluation of the machine attribute in the context of the job
    ClassAd when the *condor_schedd* daemon initiates the start up of
    the job. If the evaluation results in an ``Undefined`` or ``Error``
    result, the value recorded in the job ClassAd will be ``Undefined``
    or ``Error`` respectively.

:macro-def:`SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH`
    The integer number of run attempts to store in the job ClassAd when
    recording the values of machine attributes listed in
    :macro:`SYSTEM_JOB_MACHINE_ATTRS`. The default is 1. The
    history length may also be extended on a per-job basis by using the
    submit file command
    :subcom:`job_machine_attrs_history_length[and SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH]`
    The larger of the system and per-job history lengths will be used. A
    history length of 0 disables recording of machine attributes.

:macro-def:`SCHEDD_LOCK`
    This macro specifies what lock file should be used for access to the
    ``SchedLog`` file. It must be a separate file from the ``SchedLog``,
    since the ``SchedLog`` may be rotated and synchronization across log
    file rotations is desired. This macro is defined relative to the
    ``$(LOCK)`` macro.

:macro-def:`SCHEDD_NAME`
    Used to give an alternative value to the ``Name`` attribute in the
    *condor_schedd* 's ClassAd.

    See the description of :macro:`MASTER_NAME`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`SCHEDD_ATTRS`
    This macro is described in :macro:`<SUBSYS>_ATTRS`.

:macro-def:`SCHEDD_DEBUG`
    This macro (and other settings related to debug logging in the
    *condor_schedd*) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`SCHEDD_ADDRESS_FILE`
    This macro is described in :macro:`<SUBSYS>_ADDRESS_FILE`.

:macro-def:`SCHEDD_EXECUTE`
    A directory to use as a temporary sandbox for local universe jobs.
    Defaults to ``$(SPOOL)``/execute.

:macro-def:`FLOCK_TO`
    This defines a comma separate list of central manager
    machines this schedd should flock to.  The default value
    is empty.  For flocking to work, each of these central
    managers should also define :macro:`FLOCK_FROM` with the
    name of this schedd in that list.  This parameter
    explicitly sets :macro:`FLOCK_NEGOTIATOR_HOSTS` and 
    :macro:`FLOCK_COLLECTOR_HOSTS` so that you usually
    just need to set :macro:`FLOCK_TO` and no others to make
    flocking work.

:macro-def:`FLOCK_NEGOTIATOR_HOSTS`
    Defines a comma and/or space separated list of *condor_negotiator*
    host names for pools in which the *condor_schedd* should attempt to
    run jobs. If not set, the *condor_schedd* will query the
    *condor_collector* daemons for the addresses of the
    *condor_negotiator* daemons. If set, then the *condor_negotiator*
    daemons must be specified in order, corresponding to the list set by
    :macro:`FLOCK_COLLECTOR_HOSTS`. In the typical case, where each pool has
    the *condor_collector* and *condor_negotiator* running on the same
    machine, ``$(FLOCK_NEGOTIATOR_HOSTS)`` should have the same
    definition as ``$(FLOCK_COLLECTOR_HOSTS)``. This configuration value
    is also typically used as a macro for adding the
    *condor_negotiator* to the relevant authorization lists.

:macro-def:`FLOCK_COLLECTOR_HOSTS`
    This macro defines a list of collector host names (not including the
    local ``$(COLLECTOR_HOST)`` machine) for pools in which the
    *condor_schedd* should attempt to run jobs. Hosts in the list
    should be in order of preference. The *condor_schedd* will only
    send a request to a central manager in the list if the local pool
    and pools earlier in the list are not satisfying all the job
    requests. :macro:`ALLOW_NEGOTIATOR_SCHEDD`
    must also be configured to allow negotiators from all of the pools to
    contact the *condor_schedd* at the ``NEGOTIATOR`` authorization level.
    Similarly, the central managers of the remote pools must be
    configured to allow this *condor_schedd* to join the pool (this
    requires ``ADVERTISE_SCHEDD`` authorization level, which defaults to
    ``WRITE``).

:macro-def:`FLOCK_INCREMENT`
    This integer value controls how quickly flocking to various pools
    will occur. It defaults to 1, meaning that pools will be considered
    for flocking slowly. The first *condor_collector* daemon listed in
    :macro:`FLOCK_COLLECTOR_HOSTS` will be considered for flocking, and
    then the second, and so on. A larger value increases the number of
    *condor_collector* daemons to be considered for flocking. For example,
    a value of 2 will partition the :macro:`FLOCK_COLLECTOR_HOSTS`
    into sets of 2 *condor_collector* daemons, and each set will be
    considered for flocking.

:macro-def:`MIN_FLOCK_LEVEL`
    This integer value specifies a number of remote pools that the
    *condor_schedd* should always flock to.
    It defaults to 0, meaning that none of the pools listed in
    :macro:`FLOCK_COLLECTOR_HOSTS` will be considered for flocking when
    there are no idle jobs in need of match-making.
    Setting a larger value N means the *condor_schedd* will always
    flock to (i.e. look for matches in) the first N pools listed in
    :macro:`FLOCK_COLLECTOR_HOSTS`.

:macro-def:`NEGOTIATE_ALL_JOBS_IN_CLUSTER`
    If this macro is set to False (the default), when the
    *condor_schedd* fails to start an idle job, it will not try to
    start any other idle jobs in the same cluster during that
    negotiation cycle. This makes negotiation much more efficient for
    large job clusters. However, in some cases other jobs in the cluster
    can be started even though an earlier job can't. For example, the
    jobs' requirements may differ, because of different disk space,
    memory, or operating system requirements. Or, machines may be
    willing to run only some jobs in the cluster, because their
    requirements reference the jobs' virtual memory size or other
    attribute. Setting this macro to True will force the
    *condor_schedd* to try to start all idle jobs in each negotiation
    cycle. This will make negotiation cycles last longer, but it will
    ensure that all jobs that can be started will be started.

:macro-def:`PERIODIC_EXPR_INTERVAL`
    This macro determines the minimum period, in seconds, between
    evaluation of periodic job control expressions, such as
    periodic_hold, periodic_release, and periodic_remove, given by
    the user in an HTCondor submit file. By default, this value is 60
    seconds. A value of 0 prevents the *condor_schedd* from performing
    the periodic evaluations.

:macro-def:`MAX_PERIODIC_EXPR_INTERVAL`
    This macro determines the maximum period, in seconds, between
    evaluation of periodic job control expressions, such as
    periodic_hold, periodic_release, and periodic_remove, given by
    the user in an HTCondor submit file. By default, this value is 1200
    seconds. If HTCondor is behind on processing events, the actual
    period between evaluations may be higher than specified.

:macro-def:`PERIODIC_EXPR_TIMESLICE`
    This macro is used to adapt the frequency with which the
    *condor_schedd* evaluates periodic job control expressions. When
    the job queue is very large, the cost of evaluating all of the
    ClassAds is high, so in order for the *condor_schedd* to continue
    to perform well, it makes sense to evaluate these expressions less
    frequently. The default time slice is 0.01, so the *condor_schedd*
    will set the interval between evaluations so that it spends only 1%
    of its time in this activity. The lower bound for the interval is
    configured by :macro:`PERIODIC_EXPR_INTERVAL` (default 60 seconds)
    and the upper bound is configured with :macro:`MAX_PERIODIC_EXPR_INTERVAL`
    (default 1200 seconds).

:macro-def:`SYSTEM_PERIODIC_HOLD_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    job that is not in the ``HELD``, ``COMPLETED``, or ``REMOVED``
    state. Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_HOLD_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    not case-sensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_HOLD` expression will be evaluated. If any
    of these expression evaluates to ``True`` the job will be held.  See also
    :macro:`SYSTEM_PERIODIC_HOLD`
    There is no default value.

:macro-def:`SYSTEM_PERIODIC_HOLD[SCHED]`
    .. faux-definition::

:macro-def:`SYSTEM_PERIODIC_HOLD_<Name>`
    This expression behaves identically to the job expression
    ``periodic_hold``, but it is evaluated for every job in the queue.
    It defaults to ``False``. When ``True``, it causes the job to stop
    running and go on hold. Here is an example that puts jobs on hold if
    they have been restarted too many times, have an unreasonably large
    virtual memory :ad-attr:`ImageSize`, or have unreasonably large disk usage
    for an invented environment. 

    .. code-block:: condor-config

        if version > 9.5
           # use hold names if the version supports it
           SYSTEM_PERIODIC_HOLD_NAMES = Mem Disk
           SYSTEM_PERIODIC_HOLD_Mem = ImageSize > 3000000
           SYSTEM_PERIODIC_HOLD_Disk = JobStatus == 2 && DiskUsage > 10000000
           SYSTEM_PERIODIC_HOLD = JobStatus == 1 && JobRunCount > 10
        else
           SYSTEM_PERIODIC_HOLD = \
          (JobStatus == 1 || JobStatus == 2) && \
          (JobRunCount > 10 || ImageSize > 3000000 || DiskUsage > 10000000)
        endif

:macro-def:`SYSTEM_PERIODIC_HOLD_REASON`
    .. faux-definition::

:macro-def:`SYSTEM_PERIODIC_HOLD_<Name>_REASON`
    This string expression is evaluated when the job is placed on hold
    due to :macro:`SYSTEM_PERIODIC_HOLD` or :macro:`SYSTEM_PERIODIC_HOLD_<Name>` evaluating to ``True``. If it
    evaluates to a non-empty string, this value is used to set the job
    attribute :ad-attr:`HoldReason`. Otherwise, a default description is used.

:macro-def:`SYSTEM_PERIODIC_HOLD_SUBCODE`
    .. faux-definition::

:macro-def:`SYSTEM_PERIODIC_HOLD_<Name>_SUBCODE`
    This integer expression is evaluated when the job is placed on hold
    due to :macro:`SYSTEM_PERIODIC_HOLD` or :macro:`SYSTEM_PERIODIC_HOLD_<Name>` evaluating to ``True``. If it
    evaluates to a valid integer, this value is used to set the job
    attribute :ad-attr:`HoldReasonSubCode`. Otherwise, a default of 0 is used.
    The attribute :ad-attr:`HoldReasonCode` is set to 26, which indicates that
    the job went on hold due to a system job policy expression.

:macro-def:`SYSTEM_PERIODIC_RELEASE_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    job that is in the ``HELD`` state (jobs with a :ad-attr:`HoldReasonCode`
    value of ``1`` are ignored). Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_RELEASE_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    not case-sensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_RELEASE` expression will be evaluated. If any
    of these expressions evaluates to ``True`` the job will be released.  See also
    :macro:`SYSTEM_PERIODIC_RELEASE`
    There is no default value.

:macro-def:`SYSTEM_PERIODIC_RELEASE`
    .. faux-definition::

:macro-def:`SYSTEM_PERIODIC_RELEASE_<Name>`
    This expression behaves identically to a job's definition of a
    :subcom:`periodic_release[and SYSTEM_PERIODIC_RELEASE]`
    expression in a submit description file, but it is evaluated for
    every job in the queue. It defaults to ``False``. When ``True``, it
    causes a Held job to return to the Idle state. Here is an example
    that releases jobs from hold if they have tried to run less than 20
    times, have most recently been on hold for over 20 minutes, and have
    gone on hold due to ``Connection timed out`` when trying to execute
    the job, because the file system containing the job's executable is
    temporarily unavailable.

    .. code-block:: condor-config

        SYSTEM_PERIODIC_RELEASE = \
          (JobRunCount < 20 && (time() - EnteredCurrentStatus) > 1200 ) &&  \
            (HoldReasonCode == 6 && HoldReasonSubCode == 110)

:macro-def:`SYSTEM_PERIODIC_REMOVE_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    job in the queue. Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_REMOVE_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    not case-sensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_REMOVE` expression will be evaluated. If any
    of these expressions evaluates to ``True`` the job will be removed from the queue.  See also
    :macro:`SYSTEM_PERIODIC_REMOVE`
    There is no default value.

:macro-def:`SYSTEM_PERIODIC_REMOVE`
    .. faux-definition::

:macro-def:`SYSTEM_PERIODIC_REMOVE_<Name>`
    This expression behaves identically to the job expression
    ``periodic_remove``, but it is evaluated for every job in the queue.
    As it is in the configuration file, it is easy for an administrator
    to set a remove policy that applies to all jobs. It defaults to
    ``False``. When ``True``, it causes the job to be removed from the
    queue. Here is an example that removes jobs which have been on hold
    for 30 days:

    .. code-block:: condor-config

        SYSTEM_PERIODIC_REMOVE = \
          (JobStatus == 5 && time() - EnteredCurrentStatus > 3600*24*30)

:macro-def:`SYSTEM_PERIODIC_VACATE_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    running job in the queue. Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_VACATE_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    case-insensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_VACATE` expression will be evaluated. If any
    of these expressions evaluates to ``True`` the job will be evicted
    from the machine it is running on, and returned to the queue as Idle.  See also
    :macro:`SYSTEM_PERIODIC_VACATE` There is no default value.

:macro-def:`SYSTEM_PERIODIC_VACATE`
    .. faux-definition::

:macro-def:`SYSTEM_PERIODIC_VACATE_<Name>`
    This expression behaves identically to the job expression
    ``periodic_vacate``, but it is evaluated for every running job in the queue.
    As it is in the configuration file, it is easy for an administrator
    to set a vacate policy that applies to all jobs. It defaults to
    ``False``. When ``True``, it causes the job to be evicted from the
    machine it is running on.

:macro-def:`SYSTEM_ON_VACATE_COOL_DOWN`
    This expression is evaluated whenever an execution attempt for a
    job is interrupted (i.e. the job does not exit of its own accord).
    If it evaluates to a positive integer, then the job is put into a
    cool-down state for that number of seconds. During this time, the
    job will not be run again.

:macro-def:`SCHEDD_ASSUME_NEGOTIATOR_GONE`
    This macro determines the period, in seconds, that the
    *condor_schedd* will wait for the *condor_negotiator* to initiate
    a negotiation cycle before the schedd will simply try to claim any
    local *condor_startd*. This allows for a machine that is acting as
    both a submit and execute node to run jobs locally if it cannot
    communicate with the central manager. The default value, if not
    specified, is 2,000,000 seconds (effectively never).  If this
    feature is desired, we recommend setting it to some small multiple
    of the negotiation cycle, say, 1200 seconds, or 20 minutes.

:macro-def:`SYSTEM_MAX_RELEASES`
    An integer which defaults to -1 (unlimited). When set to a positive
    integer, no job will be allowed to be released more than this
    number of times from the held state.  Does not apply to
    :macro:`QUEUE_SUPER_USERS`.

.. _GRACEFULLY_REMOVE_JOBS:

:macro-def:`GRACEFULLY_REMOVE_JOBS`
    A boolean value defaulting to ``True``.  If ``True``, jobs will be
    given a chance to shut down cleanly when removed.  In the vanilla
    universe, this means that the job will be sent the signal set in
    its ``SoftKillSig`` attribute, or ``SIGTERM`` if undefined; if the
    job hasn't exited after its max vacate time, it will be hard-killed
    (sent ``SIGKILL``).  Signals are different on Windows, and other
    details differ between universes.

    The submit command :subcom:`want_graceful_removal[and GRACEFULLY_REMOVE_JOBS]`
    overrides this configuration variable.

    See :macro:`MachineMaxVacateTime` for details on
    how HTCondor computes the job's max vacate time.

:macro-def:`SCHEDD_ROUND_ATTR_<xxxx>`
    This is used to round off attributes in the job ClassAd so that
    similar jobs may be grouped together for negotiation purposes. There
    are two cases. One is that a percentage such as 25% is specified. In
    this case, the value of the attribute named <xxxx>\\ in the job
    ClassAd will be rounded up to the next multiple of the specified
    percentage of the values order of magnitude. For example, a setting
    of 25% will cause a value near 100 to be rounded up to the next
    multiple of 25 and a value near 1000 will be rounded up to the next
    multiple of 250. The other case is that an integer, such as 4, is
    specified instead of a percentage. In this case, the job attribute
    is rounded up to the specified number of decimal places. Replace
    <xxxx> with the name of the attribute to round, and set this macro
    equal to the number of decimal places to round up. For example, to
    round the value of job ClassAd attribute ``foo`` up to the nearest
    100, set

    .. code-block:: condor-config

                SCHEDD_ROUND_ATTR_foo = 2

    When the schedd rounds up an attribute value, it will save the raw
    (un-rounded) actual value in an attribute with the same name
    appended with "_RAW". So in the above example, the raw value will
    be stored in attribute ``foo_RAW`` in the job ClassAd. The following
    are set by default:

    .. code-block:: condor-config

                SCHEDD_ROUND_ATTR_ResidentSetSize = 25%
                SCHEDD_ROUND_ATTR_ProportionalSetSizeKb = 25%
                SCHEDD_ROUND_ATTR_ImageSize = 25%
                SCHEDD_ROUND_ATTR_ExecutableSize = 25%
                SCHEDD_ROUND_ATTR_DiskUsage = 25%
                SCHEDD_ROUND_ATTR_NumCkpts = 4

    Thus, an ImageSize near 100MB will be rounded up to the next
    multiple of 25MB. If your batch slots have less memory or disk than
    the rounded values, it may be necessary to reduce the amount of
    rounding, because the job requirements will not be met.

:macro-def:`SCHEDD_BACKUP_SPOOL`
    A boolean value that, when ``True``, causes the *condor_schedd* to
    make a backup of the job queue as it starts. When ``True``, the
    *condor_schedd* creates a host-specific backup of the current spool
    file to the spool directory. This backup file will be overwritten
    each time the *condor_schedd* starts. Defaults to ``False``.

:macro-def:`SCHEDD_PREEMPTION_REQUIREMENTS`
    This boolean expression is utilized only for machines allocated by a
    dedicated scheduler. When ``True``, a machine becomes a candidate
    for job preemption. This configuration variable has no default; when
    not defined, preemption will never be considered.

:macro-def:`SCHEDD_PREEMPTION_RANK`
    This floating point value is utilized only for machines allocated by
    a dedicated scheduler. It is evaluated in context of a job ClassAd,
    and it represents a machine's preference for running a job. This
    configuration variable has no default; when not defined, preemption
    will never be considered.

:macro-def:`ParallelSchedulingGroup`
    For parallel jobs which must be assigned within a group of machines
    (and not cross group boundaries), this configuration variable is a
    string which identifies a group of which this machine is a member.
    Each machine within a group sets this configuration variable with a
    string that identifies the group.

:macro-def:`PER_JOB_HISTORY_DIR`
    If set to a directory writable by the HTCondor user, when a job
    leaves the *condor_schedd* 's queue, a copy of the job's ClassAd
    will be written in that directory. The files are named ``history``,
    with the job's cluster and process number appended. For example, job
    35.2 will result in a file named ``history.35.2``. HTCondor does not
    rotate or delete the files, so without an external entity to clean
    the directory, it can grow very large. This option defaults to being
    unset. When not set, no files are written.

:macro-def:`DEDICATED_SCHEDULER_USE_FIFO`
    When this parameter is set to true (the default), parallel universe
    jobs will be scheduled in a first-in, first-out manner. When set to
    false, parallel jobs are scheduled using a best-fit algorithm. Using
    the best-fit algorithm is not recommended, as it can cause
    starvation.

:macro-def:`DEDICATED_SCHEDULER_WAIT_FOR_SPOOLER`
    A boolean value that when ``True``, causes the dedicated scheduler
    to schedule parallel universe jobs in a very strict first-in,
    first-out manner. When the default value of ``False``, parallel jobs
    that are being remotely submitted to a scheduler and are on hold,
    waiting for spooled input files to arrive at the scheduler, will not
    block jobs that arrived later, but whose input files have finished
    spooling. When ``True``, jobs with larger cluster IDs, but that are
    in the Idle state will not be scheduled to run until all earlier
    jobs have finished spooling in their input files and have been
    scheduled.

:macro-def:`SCHEDD_SEND_VACATE_VIA_TCP`
    A boolean value that defaults to ``True``. When ``True``, the
    *condor_schedd* daemon sends vacate signals via TCP, instead of the
    default UDP.

:macro-def:`SCHEDD_CLUSTER_INITIAL_VALUE`
    An integer that specifies the initial cluster number value to use
    within a job id when a job is first submitted. If the job cluster
    number reaches the value set by :macro:`SCHEDD_CLUSTER_MAXIMUM_VALUE` and
    wraps, it will be re-set to the value given by this variable. The
    default value is 1.

:macro-def:`SCHEDD_CLUSTER_INCREMENT_VALUE`
    A positive integer that defaults to 1, representing a stride used
    for the assignment of cluster numbers within a job id. When a job is
    submitted, the job will be assigned a job id. The cluster number of
    the job id will be equal to the previous cluster number used plus
    the value of this variable.

:macro-def:`SCHEDD_CLUSTER_MAXIMUM_VALUE`
    An integer that specifies an upper bound on assigned job cluster id
    values. For value M, the maximum job cluster id assigned to any job
    will be M - 1. When the maximum id is reached, cluster ids will
    continue assignment using :macro:`SCHEDD_CLUSTER_INITIAL_VALUE`. The
    default value of this variable is zero, which represents the
    behavior of having no maximum cluster id value.

    Note that HTCondor does not check for nor take responsibility for
    duplicate cluster ids for queued jobs. If
    :macro:`SCHEDD_CLUSTER_MAXIMUM_VALUE` is set to a non-zero value, the
    system administrator is responsible for ensuring that older jobs do
    not stay in the queue long enough for cluster ids of new jobs to
    wrap around and reuse the same id. With a low enough value, it is
    possible for jobs to be erroneously assigned duplicate cluster ids,
    which will result in a corrupt job queue.

:macro-def:`SCHEDD_JOB_QUEUE_LOG_FLUSH_DELAY`
    An integer which specifies an upper bound in seconds on how long it
    takes for changes to the job ClassAd to be visible to the HTCondor
    Job Router. The default is 5 seconds.

:macro-def:`ROTATE_HISTORY_DAILY`
    A boolean value that defaults to ``False``. When ``True``, the
    history file will be rotated daily, in addition to the rotations
    that occur due to the definition of :macro:`MAX_HISTORY_LOG` that rotate
    due to size.

:macro-def:`ROTATE_HISTORY_MONTHLY`
    A boolean value that defaults to ``False``. When ``True``, the
    history file will be rotated monthly, in addition to the rotations
    that occur due to the definition of :macro:`MAX_HISTORY_LOG` that rotate
    due to size.

:macro-def:`SCHEDD_COLLECT_STATS_FOR_<Name>`
    A boolean expression that when ``True`` creates a set of
    *condor_schedd* ClassAd attributes of statistics collected for a
    particular set. These attributes are named using the prefix of
    ``<Name>``. The set includes each entity for which this expression
    is ``True``. As an example, assume that *condor_schedd* statistics
    attributes are to be created for only user Einstein's jobs. Defining

    .. code-block:: condor-config

          SCHEDD_COLLECT_STATS_FOR_Einstein = (Owner=="einstein")

    causes the creation of the set of statistics attributes with names
    such as ``EinsteinJobsCompleted`` and ``EinsteinJobsCoredumped``.

:macro-def:`SCHEDD_COLLECT_STATS_BY_<Name>`
    Defines a string expression. The evaluated string is used in the
    naming of a set of *condor_schedd* statistics ClassAd attributes.
    The naming begins with ``<Name>``, an underscore character, and the
    evaluated string. Each character not permitted in an attribute name
    will be converted to the underscore character. For example,

    .. code-block:: condor-config

          SCHEDD_COLLECT_STATS_BY_Host = splitSlotName(RemoteHost)[1]

    a set of statistics attributes will be created and kept. If the
    string expression were to evaluate to ``"storm.04.cs.wisc.edu"``,
    the names of two of these attributes will be
    ``Host_storm_04_cs_wisc_edu_JobsCompleted`` and
    ``Host_storm_04_cs_wisc_edu_JobsCoredumped``.

:macro-def:`SCHEDD_EXPIRE_STATS_BY_<Name>`
    The number of seconds after which the *condor_schedd* daemon will
    stop collecting and discard the statistics for a subset identified
    by ``<Name>``, if no event has occurred to cause any counter or
    statistic for the subset to be updated. If this variable is not
    defined for a particular ``<Name>``, then the default value will be
    ``60*60*24*7``, which is one week's time.

:macro-def:`SIGNIFICANT_ATTRIBUTES`
    A comma and/or space separated list of job ClassAd attributes that
    are to be added to the list of attributes for determining the sets
    of jobs considered as a unit (an auto cluster) in negotiation, when
    auto clustering is enabled. When defined, this list replaces the
    list that the *condor_negotiator* would define based upon machine
    ClassAds.

:macro-def:`ADD_SIGNIFICANT_ATTRIBUTES`
    A comma and/or space separated list of job ClassAd attributes that
    will always be added to the list of attributes that the
    *condor_negotiator* defines based upon machine ClassAds, for
    determining the sets of jobs considered as a unit (an auto cluster)
    in negotiation, when auto clustering is enabled.

:macro-def:`REMOVE_SIGNIFICANT_ATTRIBUTES`
    A comma and/or space separated list of job ClassAd attributes that
    are removed from the list of attributes that the
    *condor_negotiator* defines based upon machine ClassAds, for
    determining the sets of jobs considered as a unit (an auto cluster)
    in negotiation, when auto clustering is enabled.

:macro-def:`SCHEDD_SEND_RESCHEDULE`
    A boolean value which defaults to true.  Set to false for 
    schedds like those in the HTCondor-CE that have no negotiator
    associated with them, in order to reduce spurious error messages
    in the SchedLog file.

:macro-def:`SCHEDD_AUDIT_LOG`
    The path and file name of the *condor_schedd* log that records
    user-initiated commands that modify the job queue. If not defined,
    there will be no *condor_schedd* audit log.

:macro-def:`MAX_SCHEDD_AUDIT_LOG`
    Controls the maximum amount of time that a log will be allowed to
    grow. When it is time to rotate a log file, it will be saved to a
    file with an ISO timestamp suffix. The oldest rotated file receives
    the file name suffix ``.old``. The ``.old`` files are overwritten
    each time the maximum number of rotated files (determined by the
    value of :macro:`MAX_NUM_SCHEDD_AUDIT_LOG`) is exceeded. A value of 0
    specifies that the file may grow without bounds. The following
    suffixes may be used to qualify the integer:

        ``Sec`` for seconds
        ``Min`` for minutes
        ``Hr`` for hours
        ``Day`` for days
        ``Wk`` for weeks

:macro-def:`MAX_NUM_SCHEDD_AUDIT_LOG`
    The integer that controls the maximum number of rotations that the
    *condor_schedd* audit log is allowed to perform, before the oldest
    one will be rotated away. The default value is 1.

:macro-def:`SCHEDD_USE_SLOT_WEIGHT`
    A boolean that defaults to ``False``. When ``True``, the
    *condor_schedd* does use configuration variable :macro:`SLOT_WEIGHT` to
    weight running and idle job counts in the submitter ClassAd.

:macro-def:`EXTENDED_SUBMIT_COMMANDS`
    A long form ClassAd that defines extended submit commands and their associated
    job ad attributes for a specific Schedd.  :tool:`condor_submit` will query the
    destination schedd for this ClassAd and use it to modify the internal
    table of submit commands before interpreting the submit file.

    Each entry in this ClassAd will define a new submit command, the value will
    indicate the required data type to the submit file parser with the data type
    given by example from the value according to this list of types

    -  *string-list* - a quoted string containing a comma. e.g. ``"a,b"``. *string-list* values
       are converted to canonical form.
    -  *filename* - a quoted string beginning with the word file.  e.g. ``"filename"``. *filename* values
       are converted to fully qualified file paths using the same rules as other submit filenames.
    -  *string* - a quoted string that does not match the above special rules. e.g. ``"string"``. *string* values
       can be provided quoted or unquoted in the submit file.  Unquoted values will have leading and trailing
       whitespace removed.
    -  *unsigned-integer* - any non-negative integer e.g. ``0``. *unsigned-integer* values are evaluated as expressions
       and submit will fail if the result does not convert to an unsigned integer.  A simple integer value
       will be stored in the job.
    -  *integer* - any negative integer e.g. ``-1``.  *integer* values are evaluated as expressions and submit
       will fail if the result does not convert to an integer.  A simple integer value will be stored in the job.
    -  *boolean* - any boolean value e.g. ``true``. *boolean* values are evaluated as expressions and submit will
       fail if the result does not convert to ``true`` or ``false``.
    -  *expression* - any expression or floating point number that is not one of the above. e.g. ``a+b``. *expression*
       values will be parsed as a classad expression and stored in the job.
    -  *error* - the literal ``error`` will tell submit to generate an error when the command is used. 
       this provides a way for admins to disable existing submit commands.
    -  *undefined* - the literal ``undefined`` will be treated by :tool:`condor_submit` as if that
       attribute is not in this ad. This is intended to aid composibility of this ad across multiple
       configuration files.

    The following example will add four new submit commands and disable the use of the
    the ``accounting_group_user`` submit command.

    .. code-block:: condor-config

          EXTENDED_SUBMIT_COMMANDS @=end
             LongJob = true
             Project = "string"
             FavoriteFruit = "a,b"
             SomeFile = "filename"
             accounting_group_user = error
          @end

:macro-def:`EXTENDED_SUBMIT_HELPFILE`
    A URL or file path to text describing how the *condor_schedd* extends the submit schema. Use this to document
    for users the extended submit commands defined by the configuration variable :macro:`EXTENDED_SUBMIT_COMMANDS`.
    :tool:`condor_submit` will display this URL or the text of this file when the user uses the ``-capabilities`` option.

:macro-def:`SUBMIT_TEMPLATE_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain a set of submit commands.  Each name in the list will be used in the name of
    the configuration variable :macro:`SUBMIT_TEMPLATE_<Name>`.
    Names are not case-sensitive. There is no default value.  Submit templates are
    used by :tool:`condor_submit` when parsing submit files, so administrators or users can
    add submit templates to the configuration of :tool:`condor_submit` to customize the
    schema or to simplify the creation of submit files.

:macro-def:`SUBMIT_TEMPLATE_<Name>`
    A single submit template containing one or more submit commands.
    The template can be invoked with or without arguments.  The template
    can refer arguments by number using the ``$(<N>)`` where ``<N>`` is
    a value from 0 thru 9.  ``$(0)`` expands to all of the arguments,
    ``$(1)`` to the first argument, ``$(2)`` to the second argument, and so on.
    The argument number can be followed by ``?`` to test if the argument
    was specified, or by ``+`` to expand to that argument and all subsequent
    arguments.  Thus ``$(0)`` and ``$(1+)`` will expand to the same thing.

    For example:

    .. code-block:: condor-config

          SUBMIT_TEMPLATE_NAMES = $(SUBMIT_TEMPLATE_NAMES) Slurm
          SUBMIT_TEMPLATE_Slurm @=tpl
             if ! $(1?)
                error : Template:Slurm requires at least 1 argument - Slurm(project, [queue [, resource_args...])
             endif
             universe = Grid
             grid_resource = batch slurm $(3)
             batch_project = $(1)
             batch_queue = $(2:Default)
          @tpl

    This could be used in a submit file in this way:

    .. code-block:: condor-submit

          use template : Slurm(Blue Book)


:macro-def:`JOB_TRANSFORM_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain a set of rules governing the transformation of jobs during
    submission. Each name in the list will be used in the name of
    configuration variable :macro:`JOB_TRANSFORM_<Name>`. Transforms are
    applied in the order in which names appear in this list. Names are
    not case-sensitive. There is no default value.

:macro-def:`JOB_TRANSFORM_<Name>`
    A single job transform specified as a set of transform rules.
    The syntax for these rules is specified in :ref:`classads/transforms:ClassAd Transforms`
    The transform rules are applied to jobs that match
    the transform's ``REQUIREMENTS`` expression as they are submitted.
    ``<Name>`` corresponds to a name listed in :macro:`JOB_TRANSFORM_NAMES`.
    Names are not case-sensitive. There is no default value.
    For jobs submitted as late materialization factories, the factory Cluster ad is transformed
    at submit time.  When job ads are later materialized, attribute values set by the transform
    will override values set by the job factory for those attributes.

:macro-def:`SUBMIT_REQUIREMENT_NAMES`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    represent an expression evaluated to decide whether or not to reject
    a job submission. Each name in the list will be used in the name of
    configuration variable :macro:`SUBMIT_REQUIREMENT_<Name>`. There is no
    default value.

:macro-def:`SUBMIT_REQUIREMENT_<Name>`
    A boolean expression evaluated in the context of the
    *condor_schedd* daemon ClassAd, which is the ``SCHEDD.`` or ``MY.``
    name space and the job ClassAd, which is the ``JOB.`` or ``TARGET.``
    name space. When ``False``, it causes the *condor_schedd* to reject
    the submission of the job or cluster of jobs. ``<Name>`` corresponds
    to a name listed in :macro:`SUBMIT_REQUIREMENT_NAMES`. There is no
    default value.

:macro-def:`SUBMIT_REQUIREMENT_<Name>_REASON`
    An expression that evaluates to a string, to be printed for the job
    submitter when :macro:`SUBMIT_REQUIREMENT_<Name>` evaluates to ``False``
    and the *condor_schedd* rejects the job. There is no default value.

:macro-def:`SCHEDD_RESTART_REPORT`
    The complete path to a file that will be written with report
    information. The report is written when the *condor_schedd* starts.
    It contains statistics about its attempts to reconnect to the
    *condor_startd* daemons for all jobs that were previously running.
    The file is updated periodically as reconnect attempts succeed or
    fail. Once all attempts have completed, a copy of the report is
    emailed to address specified by :macro:`CONDOR_ADMIN`. The default value
    is ``$(LOG)/ScheddRestartReport``. If a blank value is set, then no
    report is written or emailed.

:macro-def:`JOB_SPOOL_PERMISSIONS`
    Control the permissions on the job's spool directory. Defaults to
    ``user`` which sets permissions to 0700. Possible values are
    ``user``, ``group``, and ``world``. If set to ``group``, then the
    directory is group-accessible, with permissions set to 0750. If set
    to ``world``, then the directory is created with permissions set to
    0755.

:macro-def:`CHOWN_JOB_SPOOL_FILES`
    Prior to HTCondor 8.5.0 on unix, the condor_schedd would chown job
    files in the SPOOL directory between the condor account and the
    account of the job submitter. Now, these job files are always owned
    by the job submitter by default. To restore the older behavior, set
    this parameter to ``True``. The default value is ``False``.

:macro-def:`IMMUTABLE_JOB_ATTRS`
    A comma and/or space separated list of attributes provided by the
    administrator that cannot be changed, once they have committed
    values. No attributes are in this list by default.

:macro-def:`SYSTEM_IMMUTABLE_JOB_ATTRS`
    A predefined comma and/or space separated list of attributes that
    cannot be changed, once they have committed values. The hard-coded
    value is: :ad-attr:`Owner` :ad-attr:`ClusterId` :ad-attr:`ProcId` :ad-attr:`MyType`
    :ad-attr:`TargetType`.

:macro-def:`PROTECTED_JOB_ATTRS`
    A comma and/or space separated list of attributes provided by the
    administrator that can only be altered by the queue super-user, once
    they have committed values. No attributes are in this list by
    default.

:macro-def:`SYSTEM_PROTECTED_JOB_ATTRS`
    A predefined comma and/or space separated list of attributes that
    can only be altered by the queue super-user, once they have
    committed values. The hard-code value is empty.

:macro-def:`ALTERNATE_JOB_SPOOL`
    A ClassAd expression evaluated in the context of the job ad. If the
    result is a string, the value is used an an alternate spool
    directory under which the job's files will be stored. This alternate
    directory must already exist and have the same file ownership and
    permissions as the main :macro:`SPOOL` directory. Care must be taken that
    the value won't change during the lifetime of each job.

:macro-def:`UNUSED_CLAIM_TIMEOUT`
    An integer value that is only used by the dedicated scheduler when
    scheduling parallel universe jobs. It specifies the number of
    seconds the schedd will keep a claimed slot, even when idle.  Zero
    seconds means the schedd will keep a claim for an unbounded amount
    of time.  Default is 300 seconds.

:macro-def:`<OAuth2Service>_CLIENT_ID`
    The client ID string for an OAuth2 service named ``<OAuth2Service>``.
    The client ID is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_CLIENT_SECRET_FILE`
    The path to the file containing the client secret string
    for an OAuth2 service named ``<OAuth2Service>``.
    The client secret is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_RETURN_URL_SUFFIX`
    The path (``https://<hostname>/<path>``)
    that an OAuth2 service named ``<OAuth2Service>``
    should be directed when returning
    after a user permits the submit host access
    to their account.
    Most often, this should be set to name of the OAuth2 service
    (e.g. ``box``, ``gdrive``, ``onedrive``, etc.).
    The derived return URL is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_AUTHORIZATION_URL`
    The URL that the companion OAuth2 credmon WSGI application
    should redirect a user to
    in order to request access for a user's credentials
    for the OAuth2 service named ``<OAuth2Service>``.
    This URL should be found in the service's API documentation.
    The authorization URL is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_TOKEN_URL`
    The URL that the *condor_credmon_oauth* should use
    in order to refresh a user's tokens
    for the OAuth2 service named ``<OAuth2Service>``.
    This URL should be found in the service's API documentation.
    The token URL is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`CHECKPOINT_DESTINATION_MAPFILE`
    The location on disk of the file which maps from checkpoint destinations
    to how invoke the corresponding clean-up plug-in.  Defaults to
    ``$(ETC)/checkpoint-destination-mapfile``.

:macro-def:`AUTO_USE_FEATURE_PelicanRetryPolicy`
    A boolean that defaults to True.  When True, the 
    configuration template :macro:`use feature:PelicanRetryPolicy`
    will be automatically included in the schedd's configuration.

:macro-def:`SCHEDD_CHECKPOINT_CLEANUP_TIMEOUT`
    There's only so long that the *condor_schedd* is willing to let clean-up
    for a single job (including all of its checkpoints) take.  This macro
    defines that duration (as an integer number of seconds).

:macro-def:`DEFAULT_NUM_EXTRA_CHECKPOINTS`
    By default, how many "extra" checkpoints should HTCondor store for a
    self-checkpoint job using third-party storage.

:macro-def:`USE_JOBSETS`
    Boolean to enable the use of job sets with the `htcondor jobset` command.
    Defaults to false.

:macro-def:`ENABLE_HTTP_PUBLIC_FILES`
    A boolean that defaults to false.  When true, the schedd will
    use an external http server to transfer public input file.

:macro-def:`HTTP_PUBLIC_FILES_ADDRESS`
    The full web address (hostname + port) where your web server is serving files (default:
    127.0.0.1:80)

:macro-def:`HTTP_PUBLIC_FILES_ROOT_DIR`
    Absolute path to the local directory where the web service is serving files from.

:macro-def:`HTTP_PUBLIC_FILES_USER`
   User security level used to write links to the directory specified by
   HTTP_PUBLIC_FILES_ROOT_DIR. There are three valid options for
   this knob:  **<user>**, **<condor>** or **<%username%>**
