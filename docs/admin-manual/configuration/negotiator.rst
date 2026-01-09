:index:`Negotiator Daemon Options<single: Configuration; Negotiator Daemon Options>`

.. _negotiator_config_options:

Negotiator Daemon Configuration Options
=======================================

These macros affect the *condor_negotiator*.

:macro-def:`NEGOTIATOR`
    The full path to the *condor_negotiator* binary.

:macro-def:`NEGOTIATOR_NAME`
    Used to give an alternative value to the ``Name`` attribute in the
    *condor_negotiator* 's ClassAd and the ``NegotiatorName``
    attribute of its accounting ClassAds. This configuration macro is
    useful in the situation where there are two *condor_negotiator*
    daemons running on one machine, and both report to the same
    *condor_collector*. Different names will distinguish the two
    daemons.

    See the description of :macro:`MASTER_NAME`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`NEGOTIATOR_INTERVAL`
    Sets the maximum time the *condor_negotiator* will wait before
    starting a new negotiation cycle, counting from the start of the
    previous cycle.
    It is defined in seconds and defaults to 60 (1 minute).

:macro-def:`NEGOTIATOR_MIN_INTERVAL`
    Sets the minimum time the *condor_negotiator* will wait before
    starting a new negotiation cycle, counting from the start of the
    previous cycle.
    It is defined in seconds and defaults to 5.

:macro-def:`NEGOTIATOR_UPDATE_INTERVAL`
    This macro determines how often the *condor_negotiator* daemon
    sends a ClassAd update to the *condor_collector*. It is defined in
    seconds and defaults to 300 (every 5 minutes).

:macro-def:`NEGOTIATOR_CYCLE_DELAY`
    An integer value that represents the minimum number of seconds that
    must pass before a new negotiation cycle may start. The default
    value is 20. :macro:`NEGOTIATOR_CYCLE_DELAY` is intended only for use by
    HTCondor experts.

:macro-def:`NEGOTIATOR_TIMEOUT`
    Sets the timeout that the negotiator uses on its network connections
    to the *condor_schedd* and *condor_startd* s. It is defined in
    seconds and defaults to 30.

:macro-def:`NEGOTIATION_CYCLE_STATS_LENGTH`
    Specifies how many recent negotiation cycles should be included in
    the history that is published in the *condor_negotiator* 's ad.
    The default is 3 and the maximum allowed value is 100. Setting this
    value to 0 disables publication of negotiation cycle statistics. The
    statistics about recent cycles are stored in several attributes per
    cycle. Each of these attribute names will have a number appended to
    it to indicate how long ago the cycle happened, for example:
    ``LastNegotiationCycleDuration0``,
    ``LastNegotiationCycleDuration1``,
    ``LastNegotiationCycleDuration2``, .... The attribute numbered 0
    applies to the most recent negotiation cycle. The attribute numbered
    1 applies to the next most recent negotiation cycle, and so on. See
    :doc:`/classad-attributes/negotiator-classad-attributes` for a list of
    attributes that are published.

:macro-def:`NEGOTIATOR_NUM_THREADS`
    An integer that specifies the number of threads the negotiator should
    use when trying to match a job to slots.  The default is 1.  For
    sites with large number of slots, where the negotiator is running
    on a large machine, setting this to a larger value may result in
    faster negotiation times.  Setting this to more than the number
    of cores will result in slow downs.  An administrator setting this
    should also consider what other processes on the machine may need
    cores, such as the collector, and all of its forked children,
    the condor_master, and any helper programs or scripts running there.

:macro-def:`PRIORITY_HALFLIFE`
    This macro defines the half-life of the user priorities. See
    :ref:`users-manual/job-scheduling:user priority` on
    User Priorities for details. It is defined in seconds and defaults
    to 86400 (1 day).

:macro-def:`DEFAULT_PRIO_FACTOR`
    Sets the priority factor for local users as they first submit jobs,
    as described in :doc:`/admin-manual/cm-configuration`.
    Defaults to 1000.

:macro-def:`NICE_USER_PRIO_FACTOR`
    Sets the priority factor for nice users, as described in
    :doc:`/admin-manual/cm-configuration`.
    Defaults to 10000000000.

:macro-def:`NICE_USER_ACCOUNTING_GROUP_NAME`
    Sets the name used for the nice-user accounting group by :tool:`condor_submit`.
    Defaults to nice-user.

:macro-def:`REMOTE_PRIO_FACTOR`
    Defines the priority factor for remote users, which are those users
    who who do not belong to the local domain. See
    :doc:`/admin-manual/cm-configuration` for details.
    Defaults to 10000000.

:macro-def:`ACCOUNTANT_DATABASE_FILE`
    Defines the full path of the accountant database log file.
    The default value is ``$(SPOOL)/Accountantnew.log``

:macro-def:`ACCOUNTANT_LOCAL_DOMAIN`
    Describes the local UID domain. This variable is used to decide if a
    user is local or remote. A user is considered to be in the local
    domain if their UID domain matches the value of this variable.
    Usually, this variable is set to the local UID domain. If not
    defined, all users are considered local.

:macro-def:`MAX_ACCOUNTANT_DATABASE_SIZE`
    This macro defines the maximum size (in bytes) that the accountant
    database log file can reach before it is truncated (which re-writes
    the file in a more compact format). If, after truncating, the file
    is larger than one half the maximum size specified with this macro,
    the maximum size will be automatically expanded. The default is 1
    megabyte (1000000).

:macro-def:`NEGOTIATOR_DISCOUNT_SUSPENDED_RESOURCES`
    This macro tells the negotiator to not count resources that are
    suspended when calculating the number of resources a user is using.
    Defaults to false, that is, a user is still charged for a resource
    even when that resource has suspended the job.

:macro-def:`NEGOTIATOR_SOCKET_CACHE_SIZE`
    This macro defines the maximum number of sockets that the
    *condor_negotiator* keeps in its open socket cache. Caching open
    sockets makes the negotiation protocol more efficient by eliminating
    the need for socket connection establishment for each negotiation
    cycle. The default is currently 500. To be effective, this parameter
    should be set to a value greater than the number of
    *condor_schedd* s submitting jobs to the negotiator at any time.
    If you lower this number, you must run :tool:`condor_restart` and not
    just :tool:`condor_reconfig` for the change to take effect.

:macro-def:`NEGOTIATOR_INFORM_STARTD`
    Boolean setting that controls if the *condor_negotiator* should
    inform the *condor_startd* when it has been matched with a job. The
    default is ``False``. When this is set to the default value of
    ``False``, the *condor_startd* will never enter the Matched state,
    and will go directly from Unclaimed to Claimed. Because this
    notification is done via UDP, if a pool is configured so that the
    execute hosts do not create UDP command sockets (see the
    :macro:`WANT_UDP_COMMAND_SOCKET` setting for details), the
    *condor_negotiator* should be configured not to attempt to contact
    these *condor_startd* daemons by using the default value.

:macro-def:`NEGOTIATOR_PRE_JOB_RANK`
    Resources that match a request are first sorted by this expression.
    If there are any ties in the rank of the top choice, the top
    resources are sorted by the user-supplied rank in the job ClassAd,
    then by :macro:`NEGOTIATOR_POST_JOB_RANK`, then by :macro:`PREEMPTION_RANK`
    (if the match would cause preemption and there are still any ties in
    the top choice). MY refers to attributes of the machine ClassAd and
    TARGET refers to the job ClassAd. The purpose of the pre job rank is
    to allow the pool administrator to override any other rankings, in
    order to optimize overall throughput. For example, it is commonly
    used to minimize preemption, even if the job rank prefers a machine
    that is busy. If explicitly set to be undefined, this expression has
    no effect on the ranking of matches. The default value prefers to
    match multi-core jobs to dynamic slots in a best fit manner:

    .. code-block:: condor-config

          NEGOTIATOR_PRE_JOB_RANK = (10000000 * My.Rank) + \
           (1000000 * (RemoteOwner =?= UNDEFINED)) - (100000 * Cpus) - Memory

:macro-def:`NEGOTIATOR_POST_JOB_RANK`
    Resources that match a request are first sorted by
    :macro:`NEGOTIATOR_PRE_JOB_RANK`. If there are any ties in the rank of
    the top choice, the top resources are sorted by the user-supplied
    rank in the job ClassAd, then by :macro:`NEGOTIATOR_POST_JOB_RANK`, then
    by :macro:`PREEMPTION_RANK` (if the match would cause preemption and
    there are still any ties in the top choice). ``MY.`` refers to
    attributes of the machine ClassAd and ``TARGET.`` refers to the job
    ClassAd. The purpose of the post job rank is to allow the pool
    administrator to choose between machines that the job ranks equally.
    The default value is

    .. code-block:: condor-config

          NEGOTIATOR_POST_JOB_RANK = \
           (RemoteOwner =?= UNDEFINED) * \
           (ifThenElse(isUndefined(KFlops), 1000, Kflops) - \
           SlotID - 1.0e10*(Offline=?=True))

:macro-def:`PREEMPTION_REQUIREMENTS`
    When considering user priorities, the negotiator will not preempt a
    job running on a given machine unless this expression evaluates to
    ``True``, and the owner of the idle job has a better priority than
    the owner of the running job. The :macro:`PREEMPTION_REQUIREMENTS`
    expression is evaluated within the context of the candidate machine
    ClassAd and the candidate idle job ClassAd; thus the MY scope prefix
    refers to the machine ClassAd, and the TARGET scope prefix refers to
    the ClassAd of the idle (candidate) job. There is no direct access
    to the currently running job, but attributes of the currently
    running job that need to be accessed in :macro:`PREEMPTION_REQUIREMENTS`
    can be placed in the machine ClassAd using :macro:`STARTD_JOB_ATTRS`.
    If not explicitly set in the HTCondor configuration file, the default
    value for this expression is ``False``. :macro:`PREEMPTION_REQUIREMENTS`
    should include the term ``(SubmitterGroup =?= RemoteGroup)``, if a
    preemption policy that respects group quotas is desired. Note that
    this variable does not influence other potential causes of preemption,
    such as the :macro:`RANK` of the *condor_startd*, or :macro:`PREEMPT` expressions. See
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    for a general discussion of limiting preemption.

:macro-def:`PREEMPTION_REQUIREMENTS_STABLE`
    A boolean value that defaults to ``True``, implying that all
    attributes utilized to define the :macro:`PREEMPTION_REQUIREMENTS`
    variable will not change within a negotiation period time interval.
    If utilized attributes will change during the negotiation period
    time interval, then set this variable to ``False``.

:macro-def:`PREEMPTION_RANK`
    Resources that match a request are first sorted by
    :macro:`NEGOTIATOR_PRE_JOB_RANK`. If there are any ties in the rank of
    the top choice, the top resources are sorted by the user-supplied
    rank in the job ClassAd, then by :macro:`NEGOTIATOR_POST_JOB_RANK`, then
    by :macro:`PREEMPTION_RANK` (if the match would cause preemption and
    there are still any ties in the top choice). MY refers to attributes
    of the machine ClassAd and TARGET refers to the job ClassAd. This
    expression is used to rank machines that the job and the other
    negotiation expressions rank the same. For example, if the job has
    no preference, it is usually preferable to preempt a job with a
    small :ad-attr:`ImageSize` instead of a job with a large :ad-attr:`ImageSize`. The
    default value first considers the user's priority and chooses the
    user with the worst priority. Then, among the running jobs of that
    user, it chooses the job with the least accumulated run time:

    .. code-block:: condor-config

          PREEMPTION_RANK = (RemoteUserPrio * 1000000) - \
           ifThenElse(isUndefined(TotalJobRunTime), 0, TotalJobRunTime)

:macro-def:`PREEMPTION_RANK_STABLE`
    A boolean value that defaults to ``True``, implying that all
    attributes utilized to define the :macro:`PREEMPTION_RANK` variable will
    not change within a negotiation period time interval. If utilized
    attributes will change during the negotiation period time interval,
    then set this variable to ``False``.

:macro-def:`NEGOTIATOR_SLOT_CONSTRAINT`
    An expression which constrains which machine ClassAds are fetched
    from the *condor_collector* by the *condor_negotiator* during a
    negotiation cycle.

:macro-def:`NEGOTIATOR_SUBMITTER_CONSTRAINT`
    An expression which constrains which submitter ClassAds are fetched
    from the *condor_collector* by the *condor_negotiator* during a
    negotiation cycle.
    The *condor_negotiator* will ignore the jobs of submitters whose
    submitter ads don't match this constraint.

:macro-def:`NEGOTIATOR_JOB_CONSTRAINT`
    An expression which constrains which job ClassAds are considered for
    matchmaking by the *condor_negotiator*. This parameter is read by
    the *condor_negotiator* and sent to the *condor_schedd* for
    evaluation. *condor_schedd* s older than version 8.7.7 will ignore
    this expression and so will continue to send all jobs to the
    *condor_negotiator*.

:macro-def:`NEGOTIATOR_TRIM_SHUTDOWN_THRESHOLD`
    This setting is not likely to be customized, except perhaps within a
    glidein setting. An integer expression that evaluates to a value
    within the context of the *condor_negotiator* ClassAd, with a
    default value of 0. When this expression evaluates to an integer X
    greater than 0, the *condor_negotiator* will not make matches to
    machines that contain the ClassAd attribute ``DaemonShutdown`` which
    evaluates to ``True``, when that shut down time is X seconds into
    the future. The idea here is a mechanism to prevent matching with
    machines that are quite close to shutting down, since the match
    would likely be a waste of time.

:macro-def:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT`  or :macro-def:`GROUP_DYNAMIC_MACH_CONSTRAINT`
    This optional expression specifies which machine ClassAds should be
    counted when computing the size of the pool. It applies both for
    group quota allocation and when there are no groups. The default is
    to count all machine ClassAds. When extra slots exist for special
    purposes, as, for example, suspension slots or file transfer slots,
    this expression can be used to inform the *condor_negotiator* that
    only normal slots should be counted when computing how big each
    group's share of the pool should be.

    The name :macro:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT` replaces
    ``GROUP_DYNAMIC_MACH_CONSTRAINT`` as of HTCondor version 7.7.3.
    Using the older name causes a warning to be logged, although the
    behavior is unchanged.

:macro-def:`NEGOTIATOR_DEBUG`
    This macro (and other settings related to debug logging in the
    negotiator) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER`
    The maximum number of seconds the *condor_negotiator* will spend
    with each individual submitter during one negotiation cycle. Once
    this time limit has been reached, the *condor_negotiator* will skip
    over requests from this submitter until the next negotiation cycle.
    It defaults to 60 seconds.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_SCHEDD`
    The maximum number of seconds the *condor_negotiator* will spend
    with each individual *condor_schedd* during one negotiation cycle.
    Once this time limit has been reached, the *condor_negotiator* will
    skip over requests from this *condor_schedd* until the next
    negotiation cycle. It defaults to 120 seconds.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_CYCLE`
    The maximum number of seconds the *condor_negotiator* will spend in
    total across all submitters during one negotiation cycle. Once this
    time limit has been reached, the *condor_negotiator* will skip over
    requests from all submitters until the next negotiation cycle. It
    defaults to 1200 seconds.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_PIESPIN`
    The maximum number of seconds the *condor_negotiator* will spend
    with a submitter in one pie spin. A negotiation cycle is composed of
    at least one pie spin, possibly more, depending on whether there are
    still machines left over after computing fair shares and negotiating
    with each submitter. By limiting the maximum length of a pie spin or
    the maximum time per submitter per negotiation cycle, the
    *condor_negotiator* is protected against spending a long time
    talking to one submitter, for example someone with a very slow
    *condor_schedd* daemon. But, this can result in unfair allocation
    of machines or some machines not being allocated at all. See
    :doc:`/admin-manual/cm-configuration`
    for a description of a pie slice. It defaults to 120 seconds.

:macro-def:`NEGOTIATOR_DEPTH_FIRST`
    A boolean value which defaults to false. When partitionable slots
    are enabled, and this parameter is true, the negotiator tries to
    pack as many jobs as possible on each machine before moving on to
    the next machine.

:macro-def:`USE_RESOURCE_REQUEST_COUNTS`
    A boolean value that defaults to ``True``. When ``True``, the
    latency of negotiation will be reduced when there are many jobs next
    to each other in the queue with the same auto cluster, and many
    matches are being made. When ``True``, the *condor_schedd* tells
    the *condor_negotiator* to send X matches at a time, where X equals
    number of consecutive jobs in the queue within the same auto
    cluster.

:macro-def:`NEGOTIATOR_RESOURCE_REQUEST_LIST_SIZE`
    An integer tuning parameter used by the *condor_negotiator* to
    control the number of resource requests fetched from a
    *condor_schedd* per network round-trip. With higher values, the
    latency of negotiation can be significantly be reduced when
    negotiating with a *condor_schedd* running HTCondor version 8.3.0
    or more recent, especially over a wide-area network. Setting this
    value too high, however, could cause the *condor_schedd* to
    unnecessarily block on network I/O. The default value is 200. If
    :macro:`USE_RESOURCE_REQUEST_COUNTS` is set to ``False``, then this
    variable will be unconditionally set to a value of 1.

:macro-def:`NEGOTIATOR_MATCH_EXPRS`
    A comma-separated list of macro names that are inserted as ClassAd
    attributes into matched job ClassAds. The attribute name in the
    ClassAd will be given the prefix ``NegotiatorMatchExpr``, if the
    macro name does not already begin with that. Example:

    .. code-block:: condor-config

          NegotiatorName = "My Negotiator"
          NEGOTIATOR_MATCH_EXPRS = NegotiatorName

    As a result of the above configuration, jobs that are matched by
    this *condor_negotiator* will contain the following attribute when
    they are sent to the *condor_startd*:

    .. code-block:: condor-config

          NegotiatorMatchExprNegotiatorName = "My Negotiator"

    The expressions inserted by the *condor_negotiator* may be useful
    in *condor_startd* policy expressions, when the *condor_startd*
    belongs to multiple HTCondor pools.

:macro-def:`NEGOTIATOR_MATCHLIST_CACHING`
    A boolean value that defaults to ``True``. When ``True``, it enables
    an optimization in the *condor_negotiator* that works with auto
    clustering. In determining the sorted list of machines that a job
    might use, the job goes to the first machine off the top of the
    list. If :macro:`NEGOTIATOR_MATCHLIST_CACHING` is ``True``, and if the
    next job is part of the same auto cluster, meaning that it is a very
    similar job, the *condor_negotiator* will reuse the previous list
    of machines, instead of recreating the list from scratch.

:macro-def:`NEGOTIATOR_CONSIDER_PREEMPTION`
    For expert users only. A boolean value that defaults to ``True``.
    When ``False``, it can cause the *condor_negotiator* to run faster
    and also have better spinning pie accuracy. Only set this to
    ``False`` if :macro:`PREEMPTION_REQUIREMENTS` is ``False``, and if all
    *condor_startd* rank expressions are ``False``.

:macro-def:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION`
    A boolean value that when ``False`` (the default), prevents the
    *condor_negotiator* from matching jobs to claimed slots that cannot
    immediately be preempted due to :macro:`MAXJOBRETIREMENTTIME`.

:macro-def:`ALLOW_PSLOT_PREEMPTION`
    A boolean value that defaults to ``False``. When set to ``True`` for
    the *condor_negotiator*, it enables a new matchmaking mode in which
    one or more dynamic slots can be preempted in order to make enough
    resources available in their parent partitionable slot for a job to
    successfully match to the partitionable slot.

:macro-def:`STARTD_AD_REEVAL_EXPR`
    A boolean value evaluated in the context of each machine ClassAd
    within a negotiation cycle that determines whether the ClassAd from
    the *condor_collector* is to replace the stashed ClassAd utilized
    during the previous negotiation cycle. When ``True``, the ClassAd
    from the *condor_collector* does replace the stashed one. When not
    defined, the default value is to replace the stashed ClassAd if the
    stashed ClassAd's sequence number is older than its potential
    replacement.

:macro-def:`NEGOTIATOR_UPDATE_AFTER_CYCLE`
    A boolean value that defaults to ``False``. When ``True``, it will
    force the *condor_negotiator* daemon to publish an update to the
    *condor_collector* at the end of every negotiation cycle. This is
    useful if monitoring statistics for the previous negotiation cycle.

:macro-def:`NEGOTIATOR_READ_CONFIG_BEFORE_CYCLE`
    A boolean value that defaults to ``False``. When ``True``, the
    *condor_negotiator* will re-read the configuration prior to
    beginning each negotiation cycle. Note that this operation will
    update configured behaviors such as concurrency limits, but not data
    structures constructed during a full reconfiguration, such as the
    group quota hierarchy. A full reconfiguration, for example as
    accomplished with :tool:`condor_reconfig`, remains the best way to
    guarantee that all *condor_negotiator* configuration is completely
    updated.

:macro-def:`<NAME>_LIMIT`
    An integer value that defines the amount of resources available for
    jobs which declare that they use some consumable resource as
    described in :ref:`admin-manual/cm-configuration:concurrency
    limits`. ``<Name>`` is a string invented to uniquely describe the resource.

:macro-def:`CONCURRENCY_LIMIT_DEFAULT`
    An integer value that describes the number of resources available
    for any resources that are not explicitly named defined with the
    configuration variable :macro:`<NAME>_LIMIT`. If not defined, no limits
    are set for resources not explicitly identified using
    :macro:`<NAME>_LIMIT`.

:macro-def:`CONCURRENCY_LIMIT_DEFAULT_<NAME>`
    If set, this defines a default concurrency limit for all resources
    that start with ``<NAME>.``

The following configuration macros affect negotiation for group users.

:macro-def:`GROUP_NAMES`
    A comma-separated list of the recognized group names, case
    insensitive. If undefined (the default), group support is disabled.
    Group names must not conflict with any user names. That is, if there
    is a physics group, there may not be a physics user. Any group that
    is defined here must also have a quota, or the group will be
    ignored. Example:

    .. code-block:: condor-config

            GROUP_NAMES = group_physics, group_chemistry


:macro-def:`GROUP_QUOTA_<groupname>`
    A floating point value to represent a static quota specifying an
    integral number of machines for the hierarchical group identified by
    ``<groupname>``. It is meaningless to specify a non integer value,
    since only integral numbers of machines can be allocated. Example:

    .. code-block:: condor-config

            GROUP_QUOTA_group_physics = 20
            GROUP_QUOTA_group_chemistry = 10


    When both static and dynamic quotas are defined for a specific
    group, the static quota is used and the dynamic quota is ignored.

:macro-def:`GROUP_QUOTA_DYNAMIC_<groupname>`
    A floating point value in the range 0.0 to 1.0, inclusive,
    representing a fraction of a pool's machines (slots) set as a
    dynamic quota for the hierarchical group identified by
    ``<groupname>``. For example, the following specifies that a quota
    of 25% of the total machines are reserved for members of the
    group_biology group.

    .. code-block:: condor-config

           GROUP_QUOTA_DYNAMIC_group_biology = 0.25


    The group name must be specified in the :macro:`GROUP_NAMES` list.

    This section has not yet been completed

:macro-def:`GROUP_PRIO_FACTOR_<groupname>`
    A floating point value greater than or equal to 1.0 to specify the
    default user priority factor for <groupname>. The group name must
    also be specified in the :macro:`GROUP_NAMES` list.
    :macro:`GROUP_PRIO_FACTOR_<groupname>` is evaluated when the negotiator
    first negotiates for the user as a member of the group. All members
    of the group inherit the default priority factor when no other value
    is present. For example, the following setting specifies that all
    members of the group named group_physics inherit a default user
    priority factor of 2.0:

    .. code-block:: condor-config

            GROUP_PRIO_FACTOR_group_physics = 2.0


:macro-def:`GROUP_AUTOREGROUP`
    A boolean value (defaults to ``False``) that when ``True``, causes
    users who submitted to a specific group to also negotiate a second
    time with the ``<none>`` group, to be considered with the
    independent job submitters. This allows group submitted jobs to be
    matched with idle machines even if the group is over its quota. The
    user name that is used for accounting and prioritization purposes is
    still the group user as specified by :ad-attr:`AccountingGroup` in the job
    ClassAd.

:macro-def:`GROUP_AUTOREGROUP_<groupname>`
    This is the same as :macro:`GROUP_AUTOREGROUP`, but it is settable on a
    per-group basis. If no value is specified for a given group, the
    default behavior is determined by :macro:`GROUP_AUTOREGROUP`, which in
    turn defaults to ``False``.

:macro-def:`GROUP_ACCEPT_SURPLUS`
    A boolean value that, when ``True``, specifies that groups should be
    allowed to use more than their configured quota when there is not
    enough demand from other groups to use all of the available
    machines. The default value is ``False``.

:macro-def:`GROUP_ACCEPT_SURPLUS_<groupname>`
    A boolean value applied as a group-specific version of
    :macro:`GROUP_ACCEPT_SURPLUS`. When not specified, the value of
    :macro:`GROUP_ACCEPT_SURPLUS` applies to the named group.

:macro-def:`GROUP_QUOTA_ROUND_ROBIN_RATE`
    The maximum sum of weighted slots that should be handed out to an
    individual submitter in each iteration within a negotiation cycle.
    If slot weights are not being used by the *condor_negotiator*, as
    specified by ``NEGOTIATOR_USE_SLOT_WEIGHTS = False``, then this
    value is just the (unweighted) number of slots. The default value is
    a very big number, effectively infinite. Setting the value to a
    number smaller than the size of the pool can help avoid starvation.
    An example of the starvation problem is when there are a subset of
    machines in a pool with large memory, and there are multiple job
    submitters who desire all of these machines. Normally, HTCondor will
    decide how much of the full pool each person should get, and then
    attempt to hand out that number of resources to each person. Since
    the big memory machines are only a subset of pool, it may happen
    that they are all given to the first person contacted, and the
    remainder requiring large memory machines get nothing. Setting
    :macro:`GROUP_QUOTA_ROUND_ROBIN_RATE` to a value that is small compared
    to the size of subsets of machines will reduce starvation at the
    cost of possibly slowing down the rate at which resources are
    allocated.

:macro-def:`GROUP_QUOTA_MAX_ALLOCATION_ROUNDS`
    An integer that specifies the maximum number of times within one
    negotiation cycle the *condor_negotiator* will calculate how many
    slots each group deserves and attempt to allocate them. The default
    value is 3. The reason it may take more than one round is that some
    groups may not have jobs that match some of the available machines,
    so some of the slots that were withheld for those groups may not get
    allocated in any given round.

:macro-def:`NEGOTIATOR_USE_SLOT_WEIGHTS`
    A boolean value with a default of ``True``. When ``True``, the
    *condor_negotiator* pays attention to the machine ClassAd attribute
    :ad-attr:`SlotWeight`. When ``False``, each slot effectively has a weight
    of 1.

:macro-def:`FORCE_NEGOTIATOR_SLOT_WEIGHT`
    A boolean value with a default of ``False``. When ``True``, the
    *condor_negotiator* will ignore the machine ClassAd attribute
    :ad-attr:`SlotWeight` and use the expression in :macro:`SLOT_WEIGHT`
    of the negotiator config as the weight of the slot instead.

:macro-def:`NEGOTIATOR_USE_WEIGHTED_DEMAND`
    A boolean value that defaults to ``True``. When ``False``, the
    behavior is the same as for HTCondor versions prior to 7.9.6. If
    ``True``, when the *condor_schedd* advertises ``IdleJobs`` in the
    submitter ClassAd, which represents the number of idle jobs in the
    queue for that submitter, it will also advertise the total number of
    requested cores across all idle jobs from that submitter,
    :ad-attr:`WeightedIdleJobs`. If partitionable slots are being used, and if
    hierarchical group quotas are used, and if any hierarchical group
    quotas set :macro:`GROUP_ACCEPT_SURPLUS` to ``True``, and if
    configuration variable :ad-attr:`SlotWeight` :index:`SlotWeight` is
    set to the number of cores, then setting this configuration variable
    to ``True`` allows the amount of surplus allocated to each group to
    be calculated correctly.

:macro-def:`GROUP_SORT_EXPR`
    A floating point ClassAd expression that controls the order in which
    the *condor_negotiator* considers groups when allocating resources.
    The smallest magnitude positive value goes first. The default value
    is set such that group ``<none>`` always goes last when considering
    group quotas, and groups are considered in starvation order (the
    group using the smallest fraction of its resource quota is
    considered first).

:macro-def:`NEGOTIATOR_ALLOW_QUOTA_OVERSUBSCRIPTION`
    A boolean value that defaults to ``True``. When ``True``, the
    behavior of resource allocation when considering groups is more like
    it was in the 7.4 stable series of HTCondor. In implementation, when
    ``True``, the static quotas of subgroups will not be scaled when the
    sum of these static quotas of subgroups sums to more than the
    group's static quota. This behavior is desirable when using static
    quotas, unless the sum of subgroup quotas is considerably less than
    the group's quota, as scaling is currently based on the number of
    machines available, not assigned quotas (for static quotas).
