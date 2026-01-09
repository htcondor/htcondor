:index:`Startd Daemon Options<single: Configuration; Startd Daemon Options>`

.. _startd_config_options:

Startd Daemon Configuration Options
===================================

.. note::

    If you are running HTCondor on a multi-CPU machine, be sure to
    also read :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy
    configuration` which describes how to set up and configure HTCondor on
    multi-core machines.

These settings control general operation of the *condor_startd*.
Examples using these configuration macros, as well as further
explanation is found in the :doc:`/admin-manual/ep-policy-configuration`
section.

:macro-def:`START`
    A boolean expression that, when ``True``, indicates that the machine
    is willing to start running an HTCondor job. :macro:`START` is considered
    when the *condor_negotiator* daemon is considering evicting the job
    to replace it with one that will generate a better rank for the
    *condor_startd* daemon, or a user with a higher priority.

:macro-def:`DEFAULT_DRAINING_START_EXPR`
    An alternate :macro:`START` expression to use while draining when the
    drain command is sent without a ``-start`` argument.  When this
    configuration parameter is not set and the drain command does not specify
    a ``-start`` argument, :macro:`START` will have the value ``undefined``
    and ``Requirements`` will be ``false`` while draining. This will prevent new
    jobs from matching.  To allow evictable jobs to match while draining,
    set this to an expression that matches only those jobs.

:macro-def:`SUSPEND`
    A boolean expression that, when ``True``, causes HTCondor to suspend
    running an HTCondor job. The machine may still be claimed, but the
    job makes no further progress, and HTCondor does not generate a load
    on the machine.

:macro-def:`PREEMPT`
    A boolean expression that, when ``True``, causes HTCondor to stop a
    currently running job once :macro:`MAXJOBRETIREMENTTIME` has expired.
    This expression is not evaluated if :macro:`WANT_SUSPEND` is ``True``.
    The default value is ``False``, such that preemption is disabled.

:macro-def:`WANT_HOLD`
    A boolean expression that defaults to ``False``. When ``True`` and
    the value of :macro:`PREEMPT` becomes ``True`` and :macro:`WANT_SUSPEND` is
    ``False`` and :macro:`MAXJOBRETIREMENTTIME` has expired, the job is put
    on hold for the reason (optionally) specified by the variables
    :macro:`WANT_HOLD_REASON` and :macro:`WANT_HOLD_SUBCODE`. As usual, the job
    owner may specify
    :subcom:`periodic_release[and WANT_HOLD]`
    and/or :subcom:`periodic_remove[and WANT_HOLD]`
    expressions to react to specific hold states automatically. The
    attribute :ad-attr:`HoldReasonCode` in the job ClassAd is set to the value
    21 when :macro:`WANT_HOLD` is responsible for putting the job on hold.

    Here is an example policy that puts jobs on hold that use too much
    virtual memory:

    .. code-block:: condor-config

        VIRTUAL_MEMORY_AVAILABLE_MB = (VirtualMemory*0.9)
        MEMORY_EXCEEDED = ImageSize/1024 > $(VIRTUAL_MEMORY_AVAILABLE_MB)
        PREEMPT = ($(PREEMPT)) || ($(MEMORY_EXCEEDED))
        WANT_SUSPEND = ($(WANT_SUSPEND)) && ($(MEMORY_EXCEEDED)) =!= TRUE
        WANT_HOLD = ($(MEMORY_EXCEEDED))
        WANT_HOLD_REASON = \
           ifThenElse( $(MEMORY_EXCEEDED), \
                       "Your job used too much virtual memory.", \
                       undefined )

:macro-def:`WANT_HOLD_REASON`
    An expression that defines a string utilized to set the job ClassAd
    attribute :ad-attr:`HoldReason` when a job is put on hold due to
    :macro:`WANT_HOLD`. If not defined or if the expression evaluates to
    ``Undefined``, a default hold reason is provided.

:macro-def:`WANT_HOLD_SUBCODE`
    An expression that defines an integer value utilized to set the job
    ClassAd attribute :ad-attr:`HoldReasonSubCode` when a job is put on hold
    due to :macro:`WANT_HOLD`. If not defined or if the expression evaluates
    to ``Undefined``, the value is set to 0. Note that
    :ad-attr:`HoldReasonCode` is always set to 21.

:macro-def:`CONTINUE`
    A boolean expression that, when ``True``, causes HTCondor to
    continue the execution of a suspended job.

:macro-def:`KILL`
    A boolean expression that, when ``True``, causes HTCondor to
    immediately stop the execution of a vacating job, without delay. The
    job is hard-killed, so any attempt by the job to clean
    up will be aborted. This expression should normally be ``False``.
    When desired, it may be used to abort the graceful shutdown of a job
    earlier than the limit imposed by :macro:`MachineMaxVacateTime`.

:macro-def:`PERIODIC_CHECKPOINT`
    A boolean expression that, when ``True``, causes HTCondor to
    initiate a checkpoint of the currently running job.  This setting
    applies to vm universe jobs that have set
    :subcom:`vm_checkpoint[and PERIODIC_CHECKPOINT]`
    to ``True`` in the submit description file.

:macro-def:`RANK`
    A floating point value that HTCondor uses to compare potential jobs.
    A larger value for a specific job ranks that job above others with
    lower values for :macro:`RANK`.

:macro-def:`MaxJobRetirementTime`
    An expression evaluated in the context of the slot ad and the Job
    ad that should evaluate to a number of seconds.  This is the
    number of seconds after a running job has been requested to
    be preempted or evicted, that it is allowed to remain
    running in the Preempting/Retiring state.  It can Be
    thought of as a "minimum guaranteed runtime".

:macro-def:`ADVERTISE_PSLOT_ROLLUP_INFORMATION`
    A boolean value that defaults to ``True``, causing the
    *condor_startd* to advertise ClassAd attributes that may be used in
    partitionable slot preemption. The attributes are

    -  :ad-attr:`ChildAccountingGroup`
    -  :ad-attr:`ChildActivity`
    -  ``ChildCPUs``
    -  :ad-attr:`ChildCurrentRank`
    -  :ad-attr:`ChildEnteredCurrentState`
    -  :ad-attr:`ChildMemory`
    -  :ad-attr:`ChildName`
    -  :ad-attr:`ChildRemoteOwner`
    -  :ad-attr:`ChildRemoteUser`
    -  :ad-attr:`ChildRetirementTimeRemaining`
    -  :ad-attr:`ChildState`
    -  :ad-attr:`PslotRollupInformation`

:macro-def:`STARTD_PARTITIONABLE_SLOT_ATTRS`
    A list of additional from the above default attributes from dynamic
    slots that will be rolled up into a list attribute in their parent
    partitionable slot, prefixed with the name Child.

:macro-def:`WANT_SUSPEND`
    A boolean expression that, when ``True``, tells HTCondor to evaluate
    the :macro:`SUSPEND` expression to decide whether to suspend a running
    job. When ``True``, the :macro:`PREEMPT` expression is not evaluated.
    When not explicitly set, the *condor_startd* exits with an error.
    When explicitly set, but the evaluated value is anything other than
    ``True``, the value is utilized as if it were ``False``.

:macro-def:`WANT_VACATE`
    A boolean expression that, when ``True``, defines that a preempted
    HTCondor job is to be vacated, instead of killed. This means the job
    will be soft-killed and given time to clean up. The amount of time
    given depends on :macro:`MachineMaxVacateTime` and :macro:`KILL`.
    The default value is ``True``.

:macro-def:`IS_OWNER`
    A boolean expression that determines when a machine ad should enter
    the :ad-attr:`Owner` state. While in the :ad-attr:`Owner` state, the machine ad
    will not be matched to any jobs. The default value is ``False``
    (never enter :ad-attr:`Owner` state). Job ClassAd attributes should not be
    used in defining :macro:`IS_OWNER`, as they would be ``Undefined``.

:macro-def:`STARTD_HISTORY`
    A file name where the *condor_startd* daemon will maintain a job
    history file in an analogous way to that of the history file defined
    by the configuration variable :macro:`HISTORY`. It will be rotated in the
    same way, and the same parameters that apply to the :macro:`HISTORY` file
    rotation apply to the *condor_startd* daemon history as well. This
    can be read with the :tool:`condor_history` command by passing the name
    of the file to the -file option of :tool:`condor_history`.

    .. code-block:: console

        $ condor_history -file `condor_config_val LOG`/startd_history

:macro-def:`STARTER`
    This macro holds the full path to the *condor_starter* binary that
    the *condor_startd* should spawn. It is normally defined relative
    to ``$(SBIN)``.

:macro-def:`KILLING_TIMEOUT`
    The amount of time in seconds that the *condor_startd* should wait
    after sending a fast shutdown request to *condor_starter* before
    forcibly killing the job and *condor_starter*. The default value is
    30 seconds.

:macro-def:`POLLING_INTERVAL`
    When a *condor_startd* enters the claimed state, this macro
    determines how often the state of the machine is polled to check the
    need to suspend, resume, vacate or kill the job. It is defined in
    terms of seconds and defaults to 5.

:macro-def:`UPDATE_INTERVAL`
    Determines how often the *condor_startd* should send a ClassAd
    update to the *condor_collector*. The *condor_startd* also sends
    update on any state or activity change, or if the value of its
    :macro:`START` expression changes. See
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    on *condor_startd* states, *condor_startd* Activities, and
    *condor_startd* :macro:`START` expression for details on states,
    activities, and the :macro:`START` expression. This macro is defined in
    terms of seconds and defaults to 300 (5 minutes).

:macro-def:`UPDATE_OFFSET`
    An integer value representing the number of seconds of delay that
    the *condor_startd* should wait before sending its initial update.
    The default is 0. The time of all other periodic updates sent after
    this initial update is determined by ``$(UPDATE_INTERVAL)``. Thus,
    the first update will be sent after ``$(UPDATE_OFFSET)`` seconds,
    and the second update will be sent after ``$(UPDATE_OFFSET)`` +
    ``$(UPDATE_INTERVAL)``. This is useful when used in conjunction with
    the ``$RANDOM_INTEGER()`` macro for large pools, to spread out the
    updates sent by a large number of *condor_startd* daemons when all
    of the machines are started at the same time.
    The example configuration

    .. code-block:: condor-config

          startd.UPDATE_INTERVAL = 300
          startd.UPDATE_OFFSET   = $RANDOM_INTEGER(0,300)


    causes the initial update to occur at a random number of seconds
    falling between 0 and 300, with all further updates occurring at
    fixed 300 second intervals following the initial update.

:macro-def:`MachineMaxVacateTime`
    An integer expression representing the number of seconds the machine
    is willing to wait for a job that has been soft-killed to gracefully
    shut down. The default value is 600 seconds (10 minutes). This
    expression is evaluated when the job starts running. The job may
    adjust the wait time by setting :ad-attr:`JobMaxVacateTime`. If the job's
    setting is less than the machine's, the job's specification is used.
    If the job's setting is larger than the machine's, the result
    depends on whether the job has any excess retirement time. If the
    job has more retirement time left than the machine's maximum vacate
    time setting, then retirement time will be converted into vacating
    time, up to the amount of :ad-attr:`JobMaxVacateTime`. The :macro:`KILL`
    expression may be used to abort the graceful shutdown of the job
    at any time. At the time when the job is preempted, the
    :macro:`WANT_VACATE` expression may be used to skip the graceful
    shutdown of the job.

:macro-def:`MAXJOBRETIREMENTTIME`
    When the *condor_startd* wants to evict a job, a job which has run
    for less than the number of seconds specified by this expression
    will not be hard-killed. The *condor_startd* will wait for the job
    to finish or to exceed this amount of time, whichever comes sooner.
    Time spent in suspension does not count against the job. The default
    value of 0 (when the configuration variable is not present) means
    that the job gets no retirement time. If the job vacating policy
    grants the job X seconds of vacating time, a preempted job will be
    soft-killed X seconds before the end of its retirement time, so that
    hard-killing of the job will not happen until the end of the
    retirement time if the job does not finish shutting down before
    then. Note that in peaceful shutdown mode of the *condor_startd*,
    retirement time is treated as though infinite. In graceful shutdown
    mode, the job will not be preempted until the configured retirement
    time expires or :macro:`SHUTDOWN_GRACEFUL_TIMEOUT` expires. In fast shutdown
    mode, retirement time is ignored. See :macro:`MAXJOBRETIREMENTTIME` in
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    for further explanation.

    By default the *condor_negotiator* will not match jobs to a slot
    with retirement time remaining. This behavior is controlled by
    :macro:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION`.

    There is no default value for this configuration variable.

:macro-def:`CLAIM_WORKLIFE`
    This expression specifies the number of seconds after which a claim
    will stop accepting additional jobs. The default is 1200, which is
    20 minutes. Once the *condor_negotiator* gives a *condor_schedd* a
    claim to a slot, the *condor_schedd* will keep running jobs on that
    slot as long as it has more jobs with matching requirements, and
    :macro:`CLAIM_WORKLIFE` has not expired, and it is not preempted. Once
    :macro:`CLAIM_WORKLIFE` expires, any existing job may continue to run as
    usual, but once it finishes or is preempted, the claim is closed.
    When :macro:`CLAIM_WORKLIFE` is -1, this is treated as an infinite claim
    work life, so claims may be held indefinitely (as long as they are
    not preempted and the user does not run out of jobs, of course). A
    value of 0 has the effect of not allowing more than one job to run
    per claim, since it immediately expires after the first job starts
    running.

:macro-def:`MAX_CLAIM_ALIVES_MISSED`
    The *condor_schedd* sends periodic updates to each *condor_startd*
    as a keep alive (see the description of :macro:`ALIVE_INTERVAL`
    If the *condor_startd* does not receive any keep alive messages, it
    assumes that something has gone wrong with the *condor_schedd* and
    that the resource is not being effectively used. Once this happens,
    the *condor_startd* considers the claim to have timed out, it
    releases the claim, and starts advertising itself as available for
    other jobs. Because these keep alive messages are sent via UDP, they
    are sometimes dropped by the network. Therefore, the
    *condor_startd* has some tolerance for missed keep alive messages,
    so that in case a few keep alives are lost, the *condor_startd*
    will not immediately release the claim. This setting controls how
    many keep alive messages can be missed before the *condor_startd*
    considers the claim no longer valid. The default is 6.

:macro-def:`MATCH_TIMEOUT`
    The amount of time a startd will stay in Matched state without
    getting a claim request before reverting back to Unclaimed state.
    Defaults to 120 seconds.

:macro-def:`STARTD_HAS_BAD_UTMP`
    When the *condor_startd* is computing the idle time of all the
    users of the machine (both local and remote), it checks the ``utmp``
    file to find all the currently active ttys, and only checks access
    time of the devices associated with active logins. Unfortunately, on
    some systems, ``utmp`` is unreliable, and the *condor_startd* might
    miss keyboard activity by doing this. So, if your ``utmp`` is
    unreliable, set this macro to ``True`` and the *condor_startd* will
    check the access time on all tty and pty devices.

:macro-def:`CONSOLE_DEVICES`
    This macro allows the *condor_startd* to monitor console (keyboard
    and mouse) activity by checking the access times on special files in
    ``/dev``. Activity on these files shows up as :ad-attr:`ConsoleIdle` time
    in the *condor_startd* 's ClassAd. Give a comma-separated list of
    the names of devices considered the console, without the ``/dev/``
    portion of the path name. The defaults vary from platform to
    platform, and are usually correct.

    One possible exception to this is on Linux, where we use "mouse" as
    one of the entries. Most Linux installations put in a soft link from
    ``/dev/mouse`` that points to the appropriate device (for example,
    ``/dev/psaux`` for a PS/2 bus mouse, or ``/dev/tty00`` for a serial
    mouse connected to com1). However, if your installation does not
    have this soft link, you will either need to put it in (you will be
    glad you did), or change this macro to point to the right device.

    Unfortunately, modern versions of Linux do not update the access
    time of device files for USB devices. Thus, these files cannot be
    used to determine when the console is in use. Instead, use the
    *condor_kbdd* daemon, which gets this information by connecting to
    the X server.

:macro-def:`KBDD_BUMP_CHECK_SIZE`
    The number of pixels that the mouse can move in the X and/or Y
    direction, while still being considered a bump, and not keyboard
    activity. If the movement is greater than this bump size then the
    move is not a transient one, and it will register as activity. The
    default is 16, and units are pixels. Setting the value to 0
    effectively disables bump testing.

:macro-def:`KBDD_BUMP_CHECK_AFTER_IDLE_TIME`
    The number of seconds of keyboard idle time that will pass before
    bump testing begins. The default is 15 minutes.

:macro-def:`STARTD_JOB_ATTRS`
    When the machine is claimed by a remote user, the *condor_startd*
    can also advertise arbitrary attributes from the job ClassAd in the
    machine ClassAd. List the attribute names to be advertised.

    .. note::

        Since these are already ClassAd expressions, do not do anything
        unusual with strings. By default, the job ClassAd attributes
        JobUniverse, NiceUser, ExecutableSize and ImageSize are advertised
        into the machine ClassAd.

:macro-def:`STARTD_LATCH_EXPRS`
    Each time a slot is created, activated, or when periodic STARTD policy
    is evaluated HTCondor will evaluate expressions whose names are listed
    in this configuration variable.  If the evaluated value can be converted
    to an integer, and the value of the integer changes, the time of the change
    will be published.

    This macro should be a list of the names of configuration variables that contain
    an expression to be evaluated, the name of the configuration variable will be
    treated as the base name of attributes published for the macro. Thus expressions listed
    behave like :macro:`STARTD_ATTRS` with the additional behavior the most recent evaluated
    value will be advertised as ``<name>Value`` and the time the value changed will be
    advertised as ``<name>Time``.  Entries in this list can also be the names of standard
    slot attributes like ``NumDynamicSlots``, in which case the change time will be advertised
    but the evaluated value will not be advertised, since that would be redundant.

    It is not an error when the result of evaluation is undefined, in that case the STARTD
    will remember the time that the value became undefined but not advertise the time. If
    the evaluated value becomes defined again, the time that it changed from undefined to
    the new value will again be advertised.

    Example:

    .. code-block:: condor-config

          STARTD_LATCH_EXPRS = HalfFull NumDynamicSlots
          HalfFull = Cpus < (TotalSlotCPUs/2) || Memory < (TotalSlotMemory/2)

    For the configuration fragment above, the STARTD will advertise ``HalfFull`` as an expression,
    along with the last evaluated value of that expression as ``HalfFullValue``, and the time
    it changed to that value as ``HalfFullTime``.  It will also advertise the time that the number
    of dynamic slots changed to its current value as ``NumDynamicSlotsTime``. It will not advertise
    a ``NumDynamicSlotsValue`` because the ``<name>Value`` attribute is only advertised if ``<name>``
    is an expression in the configuration that is not simple literal value.


:macro-def:`STARTD_ATTRS`
    This macro is described in :macro:`<SUBSYS>_ATTRS`.

:macro-def:`SLOT<N>_STARTD_ATTRS`
    Like the above, but only applies to the numbered slot.

:macro-def:`STARTD_DEBUG`
    This macro (and other settings related to debug logging in the
    *condor_startd*) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`STARTD_ADDRESS_FILE`
    This macro is described in :macro:`<SUBSYS>_ADDRESS_FILE`

:macro-def:`ENABLE_STARTD_DAEMON_AD`
    Enable a daemon ad for the *condor_startd* that is separate from the slot ads used for matchmaking
    and running jobs.  Allowed values are True, False, and Auto.
    When the value is True, the *condor_startd* will advertise ``Slot`` ads describing the slot state
    and ``StartDaemon`` ads describing the overall state of the daemon.
    When the value is False, the *condor_startd* will advertise only ``Machine`` ads.
    When the value is Auto, the *condor_startd* will advertise ``Slot`` and ``StartDaemon`` ads to
    collectors that are HTCondor version 23.2 or later, and ``Machine`` ads to older collectors.
    The default value is Auto.

:macro-def:`SLOT_CONFIG_FAILURE_MODE`
    Controls how the *condor_startd* will handle errors during initial creation of slots when it starts.
    Allowed values are ``CLEAR``, ``CONTINUE``, and ``ABORT``.
    Beginning with HTCondor version 24.2 by default a *condor_startd* configured to advertise
    a ``StartDaemon`` ad will report slot setup failures in the daemon ad and ``CONTINUE`` on,
    configuring slots slots fit within the available resources, and marking slots that do not fit as broken.
    An older *condor_startd* will always abort rather than continue.
    If this configuration value is set to ``CLEAR`` then an error during slot configuration will cause
    the *condor_startd* to delete all slots and report errors in the ``StartDaemon`` ad.
    Details about slot configuration errors are always reported in the StartLog.

:macro-def:`STARTD_LEFTOVER_PROCS_BREAK_SLOTS`
    A boolean value that defaults to false.  When true, if a job exits and leaves behind an
    unkillable process, the startd will mark that slot as broken, and not reassign the
    resources in that slot to subsequent jobs.

:macro-def:`CONTINUE_TO_ADVERTISE_BROKEN_DYNAMIC_SLOTS`
    Controls whether the *condor_startd* will delete unclaimed dynamic slots that have a
    :ad-attr:`SlotBrokenReason` or not.  When set to True, a broken slot will not be deleted when
    it becomes unclaimed.  When set to False, a broken slot will be deleted when it becomes unclaimed
    but the resources of the dynamic slot will not be returned to the partitionable slot, instead
    the total slot resources of the partitionable slot will be adjusted to reflect the lost resources.
    In either case, the daemon ad of the *condor_startd* will advertise the broken resources,
    including which slot they were assigned to and which job and user were using the slot when
    it became broken. Default value is False.

:macro-def:`BROKEN_SLOT_CONTEXT_ATTRS`
    A list of attribute names to publish in the *condor_startd* daemon ad attribute :ad-attr:`BrokenContextAds`
    with values from the Job and Claim that was in effect when the resource was marked as broken.
    Resources that are broken on startup will not have any associated Job or Claim.
    Defaults to ``JobId,RemoteUser,RemoteScheddName``.

:macro-def:`STARTD_SHOULD_WRITE_CLAIM_ID_FILE`
    The *condor_startd* can be configured to write out the ``ClaimId``
    for the next available claim on all slots to separate files. This
    boolean attribute controls whether the *condor_startd* should write
    these files. The default value is ``True``.

:macro-def:`STARTD_CLAIM_ID_FILE`
    This macro controls what file names are used if the above
    :macro:`STARTD_SHOULD_WRITE_CLAIM_ID_FILE` is true. By default, HTCondor
    will write the ClaimId into a file in the
    ``$(LOG)``\ :index:`LOG` directory called
    ``.startd_claim_id.slotX``, where X is the value of :ad-attr:`SlotID`, the
    integer that identifies a given slot on the system, or 1 on a
    single-slot machine. If you define your own value for this setting,
    you should provide a full path, and HTCondor will automatically
    append the .slotX portion of the file name.

:macro-def:`STARTD_PRINT_ADS_ON_SHUTDOWN`
    The *condor_startd* can be configured to write out the slot ads into
    the daemon's log file as it is shutting down.  This is a boolean and the
    default value is ``False``.

:macro-def:`STARTD_PRINT_ADS_FILTER`
    When :macro:`STARTD_PRINT_ADS_ON_SHUTDOWN` above is set to ``True``, this
    macro can list which specific types of ads will get written to the log.
    The possible values are ``static```, ``partitionable``, and ``dynamic``.
    The list is comma separated and the default is to print all three types of
    ads.

:macro-def:`NUM_CPUS`
    An integer value, which can be used to lie to the *condor_startd*
    daemon about how many CPUs a machine has. When set, it overrides the
    value determined with HTCondor's automatic computation of the number
    of CPUs in the machine. Lying in this way can allow multiple
    HTCondor jobs to run on a single-CPU machine, by having that machine
    treated like a multi-core machine with multiple CPUs, which could
    have different HTCondor jobs running on each one. Or, a multi-core
    machine may advertise more slots than it has CPUs. However, lying in
    this manner will hurt the performance of the jobs, since now
    multiple jobs will run on the same CPU, and the jobs will compete
    with each other. The option is only meant for people who
    specifically want this behavior and know what they are doing. It is
    disabled by default.

    The default value is
    ``$(DETECTED_CPUS_LIMIT)``\ :index:`DETECTED_CPUS_LIMIT`.

    The *condor_startd* only takes note of the value of this
    configuration variable on start up, therefore it cannot be changed
    with a simple reconfigure. To change this, restart the
    *condor_startd* daemon for the change to take effect. The command
    will be

    .. code-block:: console

          $ condor_restart -startd

:macro-def:`MAX_NUM_CPUS`
    An integer value used as a ceiling for the number of CPUs detected
    by HTCondor on a machine. This value is ignored if :macro:`NUM_CPUS` is
    set. If set to zero, there is no ceiling. If not defined, the
    default value is zero, and thus there is no ceiling.

    Note that this setting cannot be changed with a simple reconfigure,
    either by sending a SIGHUP or by using the :tool:`condor_reconfig`
    command. To change this, restart the *condor_startd* daemon for the
    change to take effect. The command will be

    .. code-block:: console

          $ condor_restart -startd

:macro-def:`COUNT_HYPERTHREAD_CPUS`
    This configuration variable controls how HTCondor sees
    hyper-threaded processors. When set to the default value of
    ``True``, it includes virtual CPUs in the default value of
    ``DETECTED_CPUS``. On dedicated cluster nodes, counting virtual CPUs
    can sometimes improve total throughput at the expense of individual
    job speed. However, counting them on desktop workstations can
    interfere with interactive job performance.

:macro-def:`MEMORY`
    Normally, HTCondor will automatically detect the amount of physical
    memory available on your machine. Define :macro:`MEMORY` to tell HTCondor
    how much physical memory (in MB) your machine has, overriding the
    value HTCondor computes automatically. The actual amount of memory
    detected by HTCondor is always available in the pre-defined
    configuration macro :macro:`DETECTED_MEMORY`.

:macro-def:`RESERVED_MEMORY`
    How much memory (in MB) would you like reserved from HTCondor? By default,
    HTCondor considers all the physical memory of your machine as
    available to be used by HTCondor jobs. If :macro:`RESERVED_MEMORY` is
    defined, HTCondor subtracts it from the amount of memory it
    advertises as available.

:macro-def:`STARTD_NAME`
    Used to give an alternative value to the ``Name`` attribute in the
    *condor_startd* 's ClassAd. This esoteric configuration macro
    might be used in the situation where there are two *condor_startd*
    daemons running on one machine, and each reports to the same
    *condor_collector*. Different names will distinguish the two
    daemons. See the description of :macro:`MASTER_NAME`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`RUNBENCHMARKS`
    A boolean expression that specifies whether to run benchmarks. When
    the machine is in the Unclaimed state and this expression evaluates
    to ``True``, benchmarks will be run. If :macro:`RUNBENCHMARKS` is
    specified and set to anything other than ``False``, additional
    benchmarks will be run once, when the *condor_startd* starts. To
    disable start up benchmarks, set ``RunBenchmarks`` to ``False``.

:macro-def:`DedicatedScheduler`
    A string that identifies the dedicated scheduler this machine is
    managed by.
    :ref:`admin-manual/ap-policy-configuration:dedicated scheduling`
    details the use of a dedicated scheduler.

:macro-def:`STARTD_NOCLAIM_SHUTDOWN`
    The number of seconds to run without receiving a claim before
    shutting HTCondor down on this machine. Defaults to unset, which
    means to never shut down. This is primarily intended to facilitate
    glidein; use in other situations is not recommended.

:macro-def:`STARTD_PUBLISH_WINREG`
    A string containing a semicolon-separated list of Windows registry
    key names. For each registry key, the contents of the registry key
    are published in the machine ClassAd. All attribute names are
    prefixed with ``WINREG_``. The remainder of the attribute name is
    formed in one of two ways. The first way explicitly specifies the
    name within the list with the syntax

    .. code-block:: condor-config

          STARTD_PUBLISH_WINREG = AttrName1 = KeyName1; AttrName2 = KeyName2

    The second way of forming the attribute name derives the attribute
    names from the key names in the list. The derivation uses the last
    three path elements in the key name and changes each illegal
    character to an underscore character. Illegal characters are
    essentially any non-alphanumeric character. In addition, the percent
    character (%) is replaced by the string ``Percent``, and the string
    ``/sec`` is replaced by the string ``_Per_Sec``.

    HTCondor expects that the hive identifier, which is the first
    element in the full path given by a key name, will be the valid
    abbreviation. Here is a list of abbreviations:

    - ``HKLM`` is the abbreviation for ``HKEY_LOCAL_MACHINE``
    - ``HKCR`` is the abbreviation for ``HKEY_CLASSES_ROOT``
    - ``HKCU`` is the abbreviation for ``HKEY_CURRENT_USER``
    - ``HKPD`` is the abbreviation for ``HKEY_PERFORMANCE_DATA``
    - ``HKCC`` is the abbreviation for ``HKEY_CURRENT_CONFIG``
    - ``HKU`` is the abbreviation for ``HKEY_USERS``

    The ``HKPD`` key names are unusual, as they are not shown in
    *regedit*. Their values are periodically updated at the interval
    defined by :macro:`UPDATE_INTERVAL`. The others are not updated until
    :tool:`condor_reconfig` is issued.

    Here is a complete example of the configuration variable definition,

    .. code-block:: condor-config

            STARTD_PUBLISH_WINREG = HKLM\Software\Perl\BinDir; \
             BATFile_RunAs_Command = HKCR\batFile\shell\RunAs\command; \
             HKPD\Memory\Available MBytes; \
             BytesAvail = HKPD\Memory\Available Bytes; \
             HKPD\Terminal Services\Total Sessions; \
             HKPD\Processor\% Idle Time; \
             HKPD\System\Processes

    which generates the following portion of a machine ClassAd:

    .. code-block:: condor-classad

          WINREG_Software_Perl_BinDir = "C:\Perl\bin\perl.exe"
          WINREG_BATFile_RunAs_Command = "%SystemRoot%\System32\cmd.exe /C \"%1\" %*"
          WINREG_Memory_Available_MBytes = 5331
          WINREG_BytesAvail = 5590536192.000000
          WINREG_Terminal_Services_Total_Sessions = 2
          WINREG_Processor_Percent_Idle_Time = 72.350384
          WINREG_System_Processes = 166

:macro-def:`MOUNT_UNDER_SCRATCH`
    A ClassAd expression, which when evaluated in the context of the job
    and machine ClassAds, evaluates to a string that contains a comma separated list
    of directories. For each directory in the list, HTCondor creates a
    directory in the job's temporary scratch directory with that name,
    and makes it available at the given name using bind mounts. This is
    available on Linux systems which provide bind mounts and per-process
    tree mount tables, such as Red Hat Enterprise Linux 5. A bind mount
    is like a symbolic link, but is not globally visible to all
    processes. It is only visible to the job and the job's child
    processes. As an example:

    .. code-block:: condor-config

          MOUNT_UNDER_SCRATCH = ifThenElse(TARGET.UtsnameSysname =?= "Linux", "/tmp,/var/tmp", "")

    If the job is running on a Linux system, it will see the usual
    ``/tmp`` and ``/var/tmp`` directories, but when accessing files via
    these paths, the system will redirect the access. The resultant
    files will actually end up in directories named ``tmp`` or
    ``var/tmp`` under the job's temporary scratch directory. This is
    useful, because the job's scratch directory will be cleaned up after
    the job completes, two concurrent jobs will not interfere with each
    other, and because jobs will not be able to fill up the real
    ``/tmp`` directory. Another use case might be for home directories,
    which some jobs might want to write to, but that should be cleaned
    up after each job run. The default value is ``"/tmp,/var/tmp"``.

    If the job's execute directory is encrypted, ``/tmp`` and
    ``/var/tmp`` are automatically added to :macro:`MOUNT_UNDER_SCRATCH` when
    the job is run (they will not show up if :macro:`MOUNT_UNDER_SCRATCH` is
    examined with :tool:`condor_config_val`).

    .. note::

        The MOUNT_UNDER_SCRATCH mounts do not take place until the
        PreCmd of the job, if any, completes.
        (See :ref:`classad-attributes/job-classad-attributes:job classad attributes`
        for information on PreCmd.)

    Also note that, if :macro:`MOUNT_UNDER_SCRATCH` is defined, it must
    either be a ClassAd string (with double-quotes) or an expression
    that evaluates to a string.

    For Docker Universe jobs, any directories that are mounted under
    scratch are also volume mounted on the same paths inside the
    container. That is, any reads or writes to files in those
    directories goes to the host filesystem under the scratch directory.
    This is useful if a container has limited space to grow a filesystem.

:macro-def:`MOUNT_PRIVATE_DEV_SHM`
    This boolean value, which defaults to ``True`` tells the *condor_starter*
    to make /dev/shm on Linux private to each job.  When private, the
    starter removes any files from the private /dev/shm at job exit time.

:macro-def:`STARTD_RECOMPUTE_DISK`
    A boolean value that defaults to false.  When false, the startd will
    compute the free disk space on the partition where :macro:`EXECUTE`
    is mounted once at startup, and assume that only HTCondor jobs use
    space in that partition, and use that value to advertise the 
    :ad-attr:`Disk` attribute.  When true, the startd will periodically
    recalculate this value, which can cause inconsistencies when using
    partitionable slots.

The following macros control if the *condor_startd* daemon should create a
custom filesystem for the job's scratch directory. This allows HTCondor to
prevent the job from using more scratch space than provisioned.

:macro-def:`STARTD_ENFORCE_DISK_LIMITS`
    This boolean value, which is only evaluated on Linux systems, tells
    the *condor_startd* whether to make an ephemeral filesystem for the
    scratch execute directory for jobs.  The default is ``False``. This
    should only be set to true on HTCondor installations that have root
    privilege. When ``true``, you must set :macro:`LVM_VOLUME_GROUP_NAME`
    and :macro:`LVM_THINPOOL_NAME`, or alternatively set :macro:`LVM_BACKING_FILE`.
    If ``true`` and required pre-made LVM components are not defined
    then HTCondor will default to using the :macro:`LVM_BACKING_FILE`.

.. note::
    :macro:`LVM_THINPOOL_NAME` only needs to be set if Startd disk enforcement
    is using thin provisioning for logical volumes. This behavior is dictated
    by :macro:`LVM_USE_THIN_PROVISIONING`.

:macro-def:`LVM_USE_THIN_PROVISIONING`
    A boolean value that defaults to ``False``. When ``True`` HTCondor will create
    thin provisioned logical volumes from a backing thin pool logical volume for
    ephemeral execute directories. If ``False`` then HTCondor will create linear
    logical volumes for ephemeral execute directories.

:macro-def:`LVM_THINPOOL_NAME`
    A string value that represents an external pre-made Linux LVM thin-pool
    type logical volume to be used as a backing pool for ephemeral execute
    directories. This setting only matters when :macro:`STARTD_ENFORCE_DISK_LIMITS`
    is ``True``, and HTCondor has root privilege. This option does not have
    a default value.

:macro-def:`LVM_VOLUME_GROUP_NAME`
    A string value that represents an external pre-made Linux LVM volume
    group to be used to create logical volumes for ephemeral execute
    directories. This setting only matters when :macro:`STARTD_ENFORCE_DISK_LIMITS`
    is True, and HTCondor has root privilege. This option does not have a
    default value.

:macro-def:`LVM_BACKING_FILE`
    A string valued parameter that defaults to ``$(SPOOL)/startd_disk.img``.
    If a rootly HTCondor does not have pre-made Linux LVM components configured,
    a single large file will be used as the backing store for ephemeral file
    systems for execute directories. This parameter should be set to the path of
    a large, pre-created file to hold the blocks these filesystems are created from.

:macro-def:`LVM_BACKING_FILE_SIZE_MB`
    An integer value that represents the size in Megabytes to allocate for
    the ephemeral backing file described by :macro:`LVM_BACKING_FILE`. This
    option default to 10240 (10GB).

:macro-def:`LVM_THIN_LV_EXTRA_SIZE_MB`
    An integer value that represents size in Megabytes to be added onto the size
    of a thinly provisioned logical volume for an ephemeral execute directory.
    This option only applies when :macro:`LVM_USE_THIN_PROVISIONING` is ``True``.
    This extra space over will over provision the backing thin pool while providing
    a buffer to better catch over use of disk before a job gets ``ENOSPC`` errors.
    The default value is 2000 (2GB).

:macro-def:`LVM_HIDE_MOUNT`
    A value that enables LVM to create a mount namespace making the
    mount only visible to the job and associated starter. Any process
    outside of the jobs process tree will not be able to see the mount.
    This can be set to the following values:

    1. ``Auto`` (Default): Only create a mount namespace for jobs
       that are compatible.
    2. ``False``: Never create a mount namespace for any jobs.
    3. ``True``: Always create a mount namespace for all jobs.
       This will disallow execution of incompatible jobs on the EP.

    .. note::

        Docker and VM Universe jobs are not compatible with mount namespaces.

:macro-def:`LVM_CLEANUP_FAILURE_MAKES_BROKEN_SLOT`
    A boolean value that defaults to ``True``. When ``True`` EP slots
    will be marked as broken if the associated ephemeral logical volume
    is failed to be cleaned up.

:macro-def:`NO_JOB_NETWORKING`
    Either ``True`` or ``False``. When ``True``, disables access to the 
    network by a job.  Only jobs that opt into such machines will 
    match and run on such a machine. Defaults to ``False``.


The following macros control if the *condor_startd* daemon should
perform backfill computations whenever resources would otherwise be
idle. See :ref:`admin-manual/ep-policy-configuration:configuring
htcondor for running backfill jobs` for details.

:macro-def:`ENABLE_BACKFILL`
    A boolean value that, when ``True``, indicates that the machine is
    willing to perform backfill computations when it would otherwise be
    idle. This is not a policy expression that is evaluated, it is a
    simple ``True`` or ``False``. This setting controls if any of the
    other backfill-related expressions should be evaluated. The default
    is ``False``.

:macro-def:`BACKFILL_SYSTEM`
    A string that defines what backfill system to use for spawning and
    managing backfill computations. Currently, the only supported value
    for this is ``"BOINC"``, which stands for the Berkeley Open
    Infrastructure for Network Computing. See
    `http://boinc.berkeley.edu <http://boinc.berkeley.edu>`_ for more
    information about BOINC. There is no default value, administrators
    must define this.

:macro-def:`START_BACKFILL`
    A boolean expression that is evaluated whenever an HTCondor resource
    is in the Unclaimed/Idle state and the :macro:`ENABLE_BACKFILL`
    expression is ``True``. If :macro:`START_BACKFILL` evaluates to ``True``,
    the machine will enter the Backfill state and attempt to spawn a
    backfill computation. This expression is analogous to the
    :macro:`START` expression that controls when an HTCondor
    resource is available to run normal HTCondor jobs. The default value
    is ``False`` (which means do not spawn a backfill job even if the
    machine is idle and :macro:`ENABLE_BACKFILL` expression is ``True``). For
    more information about policy expressions and the Backfill state,
    see :doc:`/admin-manual/ep-policy-configuration`, especially the
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    section.

:macro-def:`EVICT_BACKFILL`
    A boolean expression that is evaluated whenever an HTCondor resource
    is in the Backfill state which, when ``True``, indicates the machine
    should immediately kill the currently running backfill computation
    and return to the Owner state. This expression is a way for
    administrators to define a policy where interactive users on a
    machine will cause backfill jobs to be removed. The default value is
    ``False``. For more information about policy expressions and the
    Backfill state, see :doc:`/admin-manual/ep-policy-configuration`, especially the
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    section.

:macro-def:`BOINC_Arguments` :macro-def:`BOINC_Environment` :macro-def:`BOINC_Error` :macro-def:`BOINC_Executable` :macro-def:`BOINC_InitialDir` :macro-def:`BOINC_Output` :macro-def:`BOINC_Owner` :macro-def:`BOINC_Universe`
     These relate to the BOINC backfill system.

The following macros only apply to the *condor_startd* daemon when it
is running on a multi-core machine. See the
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
section for details.

:macro-def:`STARTD_RESOURCE_PREFIX`
    A string which specifies what prefix to give the unique HTCondor
    resources that are advertised on multi-core machines. Previously,
    HTCondor used the term virtual machine to describe these resources,
    so the default value for this setting was ``vm``. However, to avoid
    confusion with other kinds of virtual machines, such as the ones
    created using tools like VMware or Xen, the old virtual machine
    terminology has been changed, and has become the term slot.
    Therefore, the default value of this prefix is now ``slot``. If
    sites want to continue using ``vm``, or prefer something other
    ``slot``, this setting enables sites to define what string the
    *condor_startd* will use to name the individual resources on a
    multi-core machine.

:macro-def:`SLOTS_CONNECTED_TO_CONSOLE`
    An integer which indicates how many of the machine slots the
    *condor_startd* is representing should be "connected" to the
    console. This allows the *condor_startd* to notice console
    activity. Slots with a SlotId less than or equal to the value
    will be connected. Defaults to 0 so that no slots are connected.
    :macro:`use POLICY:DESKTOP` and :macro:`use POLICY:DESKTOP_IDLE` set
    this to a very large number so that all slots will be connected.

:macro-def:`SLOTS_CONNECTED_TO_KEYBOARD`
    An integer which indicates how many of the machine slots the
    *condor_startd* is representing should be "connected" to the
    keyboard (for remote tty activity, as well as console activity).
    Slots with a SlotId less than or equal to the value
    will be connected. Defaults to 0 so that no slots are connected.
    :macro:`use POLICY:DESKTOP` and :macro:`use POLICY:DESKTOP_IDLE` set
    this to a very large number so that all slots will be connected.

:macro-def:`DISCONNECTED_KEYBOARD_IDLE_BOOST`
    If there are slots not connected to either the keyboard or the
    console, the corresponding idle time reported will be the time since
    the *condor_startd* was spawned, plus the value of this macro. It
    defaults to 1200 seconds (20 minutes). We do this because if the
    slot is configured not to care about keyboard activity, we want it
    to be available to HTCondor jobs as soon as the *condor_startd*
    starts up, instead of having to wait for 15 minutes or more (which
    is the default time a machine must be idle before HTCondor will
    start a job). If you do not want this boost, set the value to 0. If
    you change your START expression to require more than 15 minutes
    before a job starts, but you still want jobs to start right away on
    some of your multi-core nodes, increase this macro's value.

:macro-def:`STARTD_SLOT_ATTRS`
    The list of ClassAd attribute names that should be shared across all
    slots on the same machine. This setting was formerly know as
    :macro:`STARTD_VM_ATTRS` For each attribute in the list, the attribute's value is
    taken from each slot's machine ClassAd and placed into the machine
    ClassAd of all the other slots within the machine. For example, if
    the configuration file for a 2-slot machine contains

    .. code-block:: condor-config

                STARTD_SLOT_ATTRS = State, Activity, EnteredCurrentActivity

    then the machine ClassAd for both slots will contain attributes that
    will be of the form:

    .. code-block:: condor-classad

             slot1_State = "Claimed"
             slot1_Activity = "Busy"
             slot1_EnteredCurrentActivity = 1075249233
             slot2_State = "Unclaimed"
             slot2_Activity = "Idle"
             slot2_EnteredCurrentActivity = 1075240035

The following settings control the number of slots reported for a given
multi-core host, and what attributes each one has. They are only needed
if you do not want to have a multi-core machine report to HTCondor with
a separate slot for each CPU, with all shared system resources evenly
divided among them. Please read
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
for details on how to properly configure these settings to suit your
needs.

.. note::

    You cannot
    change the number or definition of the different slot types with a reconfig. If
    you change anything related to slot provisioning,
    you must restart the *condor_startd* for the change to
    take effect (for example, using ``condor_restart -startd``).

.. note::

    Prior to version 6.9.3, any settings that included the term
    ``slot`` used to use virtual machine or ``vm``. If searching for
    information about one of these older settings, search for the
    corresponding attribute names using ``slot``, instead.

:macro-def:`MAX_SLOT_TYPES`
    The maximum number of different slot types. Note: this is the
    maximum number of different types, not of actual slots. Defaults to
    10. (You should only need to change this setting if you define more
    than 10 separate slot types, which would be pretty rare.)

:macro-def:`SLOT_TYPE_<N>`
    This setting defines a given slot type, by specifying what part of
    each shared system resource (like RAM, swap space, etc) this kind of
    slot gets. This setting has no effect unless you also define
    :macro:`NUM_SLOTS_TYPE_<N>`. N can be any integer from 1 to the value of
    ``$(MAX_SLOT_TYPES)``, such as ``SLOT_TYPE_1``. The format of this
    entry can be somewhat complex, so please refer to
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    for details on the different possibilities.

:macro-def:`SLOT_TYPE_<N>_PARTITIONABLE`
    A boolean variable that defaults to ``False``. When ``True``, this
    slot permits dynamic provisioning, as specified in
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`.

:macro-def:`CLAIM_PARTITIONABLE_LEFTOVERS`
    A boolean variable that defaults to ``True``. When ``True`` within
    the configuration for both the *condor_schedd* and the
    *condor_startd*, and the *condor_schedd* claims a partitionable
    slot, the *condor_startd* returns the slot's ClassAd and a claim id
    for leftover resources. In doing so, the *condor_schedd* can claim
    multiple dynamic slots without waiting for a negotiation cycle.

:macro-def:`ENABLE_CLAIMABLE_PARTITIONABLE_SLOTS`
    A boolean variable that defaults to ``False``.
    When set to ``True`` in the configuration of both the
    *condor_startd* and the *condor_schedd*, and the *condor_schedd*
    claims a partitionable slot, the partitionable slot's :ad-attr:`State` will
    change to ``Claimed`` in addition to the creation of a ``Claimed``
    dynamic slot.
    While the slot is ``Claimed``, no other *condor_schedd* is able
    to create new dynamic slots to run jobs.

:macro-def:`MAX_PARTITIONABLE_SLOT_CLAIM_TIME`
    An integer that indicates the maximum amount of time that a
    partitionable slot can be in the ``Claimed`` state before
    returning to the Unclaimed state, expressed in seconds.
    The default value is 3600.

:macro-def:`MACHINE_RESOURCE_NAMES`
    A comma and/or space separated list of resource names that represent
    custom resources specific to a machine. These resources are further
    intended to be statically divided or partitioned, and these resource
    names identify the configuration variables that define the
    partitioning. If used, custom resources without names in the list
    are ignored.

:macro-def:`STARTD_DETECT_GPUS`
    The arguments passed to :tool:`condor_gpu_discovery` to detect GPUs when
    the configuration does not have a GPUs resource explicitly configured
    via ``MACHINE_RESOURCE_GPUS`` or  ``MACHINE_RESOURCE_INVENTORY_GPUS``.
    Use of the configuration template ``use FEATURE : GPUs`` will set
    ``MACHINE_RESOURCE_INVENTORY_GPUS`` and that will cause this configuration variable
    to be ignored.
    If the value of this configuration variable is set to ``false`` or ``0``
    or empty then automatic GPU discovery will be disabled, but a GPUs resource
    will still be defined if the configuration has ``MACHINE_RESOURCE_GPUS`` or
    ``MACHINE_RESOURCE_INVENTORY_GPUS`` or the configuration template ``use FEATURE : GPUs``.
    The default value is ``-properties $(GPU_DISCOVERY_EXTRA)``

:macro-def:`GPU_DISCOVERY_EXTRA`
    A string valued parameter that defaults to ``-extra``.  Used by
    :macro:`use feature:GPUs` and the default value of
    :macro:`STARTD_DETECT_GPUS` to allow you to pass additional
    command line arguments to the :tool:`condor_gpu_discovery` tool.

:macro-def:`MACHINE_RESOURCE_<name>`
    An integer that specifies the quantity of or list of identifiers for
    the customized local machine resource available for an SMP machine.
    The portion of this configuration variable's name identified with
    ``<name>`` will be used to label quantities of the resource
    allocated to a slot. If a quantity is specified, the resource is
    presumed to be fungible and slots will be allocated a quantity of
    the resource but specific instances will not be identified. If a
    list of identifiers is specified the quantity is the number of
    identifiers and slots will be allocated both a quantity of the
    resource and assigned specific resource identifiers.

:macro-def:`OFFLINE_MACHINE_RESOURCE_<name>`
    A comma and/or space separated list of resource identifiers for any
    customized local machine resources that are currently offline, and
    therefore should not be allocated to a slot. The identifiers
    specified here must match those specified by value of configuration
    variables :macro:`MACHINE_RESOURCE_<name>` or
    :macro:`MACHINE_RESOURCE_INVENTORY_<name>`, or the identifiers
    will be ignored. The ``<name>`` identifies the type of resource, as
    specified by the value of configuration variable
    :macro:`MACHINE_RESOURCE_NAMES`. This configuration variable is used to
    have resources that are detected and reported to exist by HTCondor,
    but not assigned to slots. A restart of the *condor_startd* is
    required for changes to resources assigned to slots to take effect.
    If this variable is changed and :tool:`condor_reconfig` command is sent
    to the Startd, the list of Offline resources will be updated, and
    the count of resources of that type will be updated,
    but newly offline resources will still be assigned to slots.
    If an offline resource is assigned to a Partitionable slot, it will
    never be assigned to a new dynamic slot but it will not be removed from
    the ``Assigned<name>`` attribute of an existing dynamic slot.

:macro-def:`MACHINE_RESOURCE_INVENTORY_<name>`
    Specifies a command line that is executed upon start up of the
    *condor_startd* daemon. The script is expected to output an
    attribute definition of the form

    .. code-block:: condor-classad

          Detected<xxx>=y

    or of the form

    .. code-block:: condor-classad

          Detected<xxx>="y, z, a, ..."

    where ``<xxx>`` is the name of a resource that exists on the
    machine, and ``y`` is the quantity of the resource or
    ``"y, z, a, ..."`` is a comma and/or space separated list of
    identifiers of the resource that exist on the machine. This
    attribute is added to the machine ClassAd, such that these resources
    may be statically divided or partitioned. A script may be a
    convenient way to specify a calculated or detected quantity of the
    resource, instead of specifying a fixed quantity or list of the
    resource in the configuration when set by
    :macro:`MACHINE_RESOURCE_<name>`.

    The script may also output an attribute of the form

    .. code-block:: condor-classad

        Offline<xxx>="y, z"

    where ``<xxx>`` is the name of the resource, and ``"y, z"`` is a comma and/or
    space separated list of resource identifiers that are also in the ``Detected<xxx>`` list.
    This attribute is added to the machine ClassAd, and resources ``y`` and ``z`` will not be
    assigned to any slot and will not be included in the count of resources of this type.
    This will override the configuration variable ``OFFLINE_MACHINE_RESOURCE_<xxx>``
    on startup. But ``OFFLINE_MACHINE_RESOURCE_<xxx>`` can still be used to take additional resources offline
    without restarting.

:macro-def:`ENVIRONMENT_FOR_Assigned<name>`
    A space separated list of environment variables to set for the job.
    Each environment variable will be set to the list of assigned
    resources defined by the slot ClassAd attribute ``Assigned<name>``.
    Each environment variable name may be followed by an equals sign and
    a Perl style regular expression that defines how to modify each
    resource ID before using it as the value of the environment
    variable. As a special case for CUDA GPUs, if the environment
    variable name is ``CUDA_VISIBLE_DEVICES``, then the correct Perl
    style regular expression is applied automatically.

    For example, with the configuration

    .. code-block:: condor-config

          ENVIRONMENT_FOR_AssignedGPUs = VISIBLE_GPUS=/^/gpuid:/

    and with the machine ClassAd attribute
    ``AssignedGPUs = "CUDA1, CUDA2"``, the job's environment will
    contain

    .. code-block:: condor-config

          VISIBLE_GPUS = gpuid:CUDA1, gpuid:CUDA2

:macro-def:`ENVIRONMENT_VALUE_FOR_UnAssigned<name>`
    Defines the value to set for environment variables specified in by
    configuration variable :macro:`ENVIRONMENT_FOR_Assigned<name>` when there
    is no machine ClassAd attribute ``Assigned<name>`` for the slot.
    This configuration variable exists to deal with the situation where
    jobs will use a resource that they have not been assigned because
    there is no explicit assignment. The CUDA runtime library (for GPUs)
    has this problem.

    For example, where configuration is

    .. code-block:: condor-config

          ENVIRONMENT_FOR_AssignedGPUs = VISIBLE_GPUS
          ENVIRONMENT_VALUE_FOR_UnAssignedGPUs = none

    and there is no machine ClassAd attribute ``AssignedGPUs``, the
    job's environment will contain

    .. code-block:: condor-config

          VISIBLE_GPUS = none

:macro-def:`MUST_MODIFY_REQUEST_EXPRS`
    A boolean value that defaults to ``False``. When ``False``,
    configuration variables whose names begin with
    ``MODIFY_REQUEST_EXPR`` are only applied if the job claim still
    matches the partitionable slot after modification. If ``True``, the
    modifications always take place, and if the modifications cause the
    claim to no longer match, then the *condor_startd* will simply
    refuse the claim.

:macro-def:`MODIFY_REQUEST_EXPR_REQUESTMEMORY`
    An integer expression used by the *condor_startd* daemon to modify
    the evaluated value of the :ad-attr:`RequestMemory` job ClassAd attribute,
    before it used to provision a dynamic slot. The default value is
    given by

    .. code-block:: text

          quantize(RequestMemory,{128})


:macro-def:`MODIFY_REQUEST_EXPR_REQUESTDISK`
    An integer expression used by the *condor_startd* daemon to modify
    the evaluated value of the :ad-attr:`RequestDisk` job ClassAd attribute,
    before it used to provision a dynamic slot. The default value is
    given by

    .. code-block:: text

          quantize(RequestDisk,{1024})


:macro-def:`MODIFY_REQUEST_EXPR_REQUESTCPUS`
    An integer expression used by the *condor_startd* daemon to modify
    the evaluated value of the :ad-attr:`RequestCpus` job ClassAd attribute,
    before it used to provision a dynamic slot. The default value is
    given by

    .. code-block:: text

          quantize(RequestCpus,{1})


:macro-def:`NUM_SLOTS_TYPE_<N>`
    This macro controls how many of a given slot type are actually
    reported to HTCondor. There is no default.

:macro-def:`NUM_SLOTS`
    An integer value representing the number of slots reported when the
    multi-core machine is being evenly divided, and the slot type
    settings described above are not being used. The default is one slot
    for each CPU. This setting can be used to reserve some CPUs on a
    multi-core machine, which would not be reported to the HTCondor
    pool. This value cannot be used to make HTCondor advertise more
    slots than there are CPUs on the machine. To do that, use
    :macro:`NUM_CPUS`.

The following variables set consumption policies for partitionable
slots.
The :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
section details consumption policies.

:macro-def:`CONSUMPTION_POLICY`
    A boolean value that defaults to ``False``. When ``True``,
    consumption policies are enabled for partitionable slots within the
    *condor_startd* daemon. Any definition of the form
    ``SLOT_TYPE_<N>_CONSUMPTION_POLICY`` overrides this global
    definition for the given slot type.

:macro-def:`CONSUMPTION_<Resource>`
    An expression that specifies a consumption policy for a particular
    resource within a partitionable slot. To support a consumption
    policy, each resource advertised by the slot must have such a policy
    configured. Custom resources may be specified, substituting the
    resource name for ``<Resource>``. Any definition of the form
    ``SLOT_TYPE_<N>_CONSUMPTION_<Resource>`` overrides this global
    definition for the given slot type. CPUs, memory, and disk resources
    are always advertised by *condor_startd*, and have the default
    values:

    .. code-block:: condor-config

        CONSUMPTION_CPUS = quantize(target.RequestCpus,{1})
        CONSUMPTION_MEMORY = quantize(target.RequestMemory,{128})
        CONSUMPTION_DISK = quantize(target.RequestDisk,{1024})

    Custom resources have no default consumption policy.

:macro-def:`SLOT_WEIGHT`
    An expression that specifies a slot's weight, used as a multiplier
    the *condor_negotiator* daemon during matchmaking to assess user
    usage of a slot, which affects user priority. Defaults to :ad-attr:`Cpus`.

    In the case of slots with consumption policies, the cost of each
    match is assessed as the difference in the slot weight expression
    before and after the resources consumed by the match are deducted
    from the slot. Only Memory, Cpus and Disk are valid attributes for
    this parameter.

:macro-def:`NUM_CLAIMS`
    Specifies the number of claims a partitionable slot will advertise
    for use by the *condor_negotiator* daemon. In the case of slots
    with a defined consumption policy, the *condor_negotiator* may
    match more than one job to the slot in a single negotiation cycle.
    For partitionable slots with a consumption policy, :macro:`NUM_CLAIMS`
    defaults to the number of CPUs owned by the slot. Otherwise, it
    defaults to 1.

The following configuration variables support java universe jobs.

:macro-def:`JAVA`
    The full path to the Java interpreter (the Java Virtual Machine).

:macro-def:`JAVA_CLASSPATH_ARGUMENT`
    The command line argument to the Java interpreter (the Java Virtual
    Machine) that specifies the Java Classpath. Classpath is a
    Java-specific term that denotes the list of locations (``.jar``
    files and/or directories) where the Java interpreter can look for
    the Java class files that a Java program requires.

:macro-def:`JAVA_CLASSPATH_SEPARATOR`
    The single character used to delimit constructed entries in the
    Classpath for the given operating system and Java Virtual Machine.
    If not defined, the operating system is queried for its default
    Classpath separator.

:macro-def:`JAVA_CLASSPATH_DEFAULT`
    A list of path names to ``.jar`` files to be added to the Java
    Classpath by default. The comma and/or space character delimits list
    entries.

:macro-def:`JAVA_EXTRA_ARGUMENTS`
    A list of additional arguments to be passed to the Java executable.

The following configuration variables control .NET version
advertisement.

:macro-def:`STARTD_PUBLISH_DOTNET`
    A boolean value that controls the advertising of the .NET framework
    on Windows platforms. When ``True``, the *condor_startd* will
    advertise all installed versions of the .NET framework within the
    :ad-attr:`DotNetVersions` attribute in the *condor_startd* machine
    ClassAd. The default value is ``True``. Set the value to ``false``
    to turn off .NET version advertising.

:macro-def:`DOT_NET_VERSIONS`
    A string expression that administrators can use to override the way
    that .NET versions are advertised. If the administrator wishes to
    advertise .NET installations, but wishes to do so in a format
    different than what the *condor_startd* publishes in its ClassAds,
    setting a string in this expression will result in the
    *condor_startd* publishing the string when :macro:`STARTD_PUBLISH_DOTNET`
    is ``True``. No value is set by default.

These macros control the power management capabilities of the
*condor_startd* to optionally put the machine in to a low power state
and wake it up later. 
See (:ref:`admin-manual/ep-policy-configuration:power management`). for more details

:macro-def:`HIBERNATE_CHECK_INTERVAL`
    An integer number of seconds that determines how often the
    *condor_startd* checks to see if the machine is ready to enter a
    low power state. The default value is 0, which disables the check.
    If not 0, the :macro:`HIBERNATE` expression is evaluated within the
    context of each slot at the given interval. If used, a value 300 (5
    minutes) is recommended.

    As a special case, the interval is ignored when the machine has just
    returned from a low power state, excluding ``"SHUTDOWN"``. In order
    to avoid machines from volleying between a running state and a low
    power state, an hour of uptime is enforced after a machine has been
    woken. After the hour has passed, regular checks resume.

:macro-def:`HIBERNATE`
    A string expression that represents lower power state. When this
    state name evaluates to a valid state other than ``"NONE"``, causes
    HTCondor to put the machine into the specified low power state. The
    following names are supported (and are not case sensitive):

    -  ``"NONE"``, ``"0"``: No-op; do not enter a low power state
    -  ``"S1"``, ``"1"``, ``"STANDBY"``, ``"SLEEP"``: On Windows, this
       is Sleep (standby)
    -  ``"S2"``, ``"2"``: On Windows, this is Sleep (standby)
    -  ``"S3"``, ``"3"``, ``"RAM"``, ``"MEM"``, ``"SUSPEND"``: On
       Windows, this is Sleep (standby)
    -  ``"S4"``, ``"4"``, ``"DISK"``, ``"HIBERNATE"``: Hibernate
    -  ``"S5"``, ``"5"``, ``"SHUTDOWN"``, ``"OFF"``: Shutdown (soft-off)

    The :macro:`HIBERNATE` expression is written in terms of the S-states as
    defined in the Advanced Configuration and Power Interface (ACPI)
    specification. The S-states take the form S<n>, where <n> is an
    integer in the range 0 to 5, inclusive. The number that results from
    evaluating the expression determines which S-state to enter. The
    notation was adopted because it appears to be the standard naming
    scheme for power states on several popular operating systems,
    including various flavors of Windows and Linux distributions. The
    other strings, such as ``"RAM"`` and ``"DISK"``, are provided for
    ease of configuration.

    Since this expression is evaluated in the context of each slot on
    the machine, any one slot has veto power over the other slots. If
    the evaluation of :macro:`HIBERNATE` in one slot evaluates to ``"NONE"``
    or ``"0"``, then the machine will not be placed into a low power
    state. On the other hand, if all slots evaluate to a non-zero value,
    but differ in value, then the largest value is used as the
    representative power state.

    Strings that do not match any in the table above are treated as
    ``"NONE"``.

:macro-def:`UNHIBERNATE`
    A boolean expression that specifies when an offline machine should
    be woken up. The default value is
    ``MachineLastMatchTime =!= UNDEFINED``. This expression does not do
    anything, unless there is an instance of *condor_rooster* running,
    or another program that evaluates the :ad-attr:`Unhibernate` expression of
    offline machine ClassAds. In addition, the collecting of offline
    machine ClassAds must be enabled for this expression to work. The
    variable :macro:`COLLECTOR_PERSISTENT_AD_LOG` explains this. The special
    attribute :ad-attr:`MachineLastMatchTime` is updated in the ClassAds of
    offline machines when a job would have been matched to the machine
    if it had been online. For multi-slot machines, the offline ClassAd
    for slot1 will also contain the attributes
    ``slot<X>_MachineLastMatchTime``, where ``X`` is replaced by the
    slot id of the other slots that would have been matched while
    offline. This allows the slot1 :macro:`UNHIBERNATE`
    expression to refer to all of the slots
    on the machine, in case that is necessary. By default,
    *condor_rooster* will wake up a machine if any slot on the machine
    has its :macro:`UNHIBERNATE` expression evaluate to ``True``.

:macro-def:`HIBERNATION_PLUGIN`
    A string which specifies the path and executable name of the
    hibernation plug-in that the *condor_startd* should use in the
    detection of low power states and switching to the low power states.
    The default value is ``$(LIBEXEC)/power_state``. A default
    executable in that location which meets these specifications is
    shipped with HTCondor.

    The *condor_startd* initially invokes this plug-in with both the
    value defined for :macro:`HIBERNATION_PLUGIN_ARGS` and the argument *ad*,
    and expects the plug-in to output a ClassAd to its standard output
    stream. The *condor_startd* will use this ClassAd to determine what
    low power setting to use on further invocations of the plug-in. To
    that end, the ClassAd must contain the attribute
    ``HibernationSupportedStates``, a comma separated list of low power
    modes that are available. The recognized mode strings are the same
    as those in the table for the configuration variable :macro:`HIBERNATE`.
    The optional attribute ``HibernationMethod`` specifies a string
    which describes the mechanism used by the plug-in. The default Linux
    plug-in shipped with HTCondor will produce one of the strings NONE,
    /sys, /proc, or pm-utils. The optional attribute
    ``HibernationRawMask`` is an integer which represents the bit mask
    of the modes detected.

    Subsequent *condor_startd* invocations of the plug-in have command
    line arguments defined by :macro:`HIBERNATION_PLUGIN_ARGS` plus the
    argument **set** *<power-mode>*, where *<power-mode>* is one of
    the supported states as given in the attribute
    ``HibernationSupportedStates``.

:macro-def:`HIBERNATION_PLUGIN_ARGS`
    Command line arguments appended to the command that invokes the
    plug-in. The additional argument *ad* is appended when the
    *condor_startd* initially invokes the plug-in.

:macro-def:`HIBERNATION_OVERRIDE_WOL`
    A boolean value that defaults to ``False``. When ``True``, it causes
    the *condor_startd* daemon's detection of the whether or not the
    network interface handles WOL packets to be ignored. When ``False``,
    hibernation is disabled if the network interface does not use WOL
    packets to wake from hibernation. Therefore, when ``True``
    hibernation can be enabled despite the fact that WOL packets are not
    used to wake machines.

:macro-def:`LINUX_HIBERNATION_METHOD`
    A string that can be used to override the default search used by
    HTCondor on Linux platforms to detect the hibernation method to use.
    This is used by the default hibernation plug-in executable that is
    shipped with HTCondor. The default behavior orders its search with:

    #. Detect and use the *pm-utils* command line tools. The
       corresponding string is defined with "pm-utils".
    #. Detect and use the directory in the virtual file system
       ``/sys/power``. The corresponding string is defined with "/sys".
    #. Detect and use the directory in the virtual file system
       ``/proc/ACPI``. The corresponding string is defined with "/proc".

    To override this ordered search behavior, and force the use of one
    particular method, set :macro:`LINUX_HIBERNATION_METHOD` to one of the
    defined strings.

:macro-def:`OFFLINE_EXPIRE_ADS_AFTER`
    An integer number of seconds specifying the lifetime of the
    persistent machine ClassAd representing a hibernating machine.
    Defaults to the largest 32-bit integer.

:macro-def:`DOCKER`
    Defines the path and executable name of the Docker CLI. The default
    value is /usr/bin/docker. Remember that the condor user must also be
    in the docker group for Docker Universe to work. See the Docker
    universe manual section for more details
    (:ref:`admin-manual/ep-policy-configuration:docker universe`).
    An example of the configuration for running the
    Docker CLI:

    .. code-block:: condor-config

          DOCKER = /usr/bin/docker


:macro-def:`DOCKER_VOLUMES`
    A list of directories on the host execute machine that might be volume
    mounted within the container. See the Docker Universe section for
    full details
    (:ref:`admin-manual/ep-policy-configuration:docker universe`).

:macro-def:`DOCKER_MOUNT_VOLUMES`
    A list of volumes, defined in the macro above, that will unconditionally
    be mounted inside the docker container.

:macro-def:`DOCKER_VOLUME_DIR_xxx_MOUNT_IF`
    This is a class ad expression, evaluated in the context of the job ad and the
    machine ad. Only when it evaluted to TRUE, is the volume named xxx mounted.

:macro-def:`DOCKER_IMAGE_CACHE_SIZE`
    The number of most recently used Docker images that will be kept on
    the local machine. The default value is 8.

:macro-def:`DOCKER_DROP_ALL_CAPABILITIES`
    A class ad expression, which defaults to true. Evaluated in the
    context of the job ad and the machine ad, when true, runs the Docker
    container with the command line option -drop-all-capabilities.
    Admins should be very careful with this setting, and only allow
    trusted users to run with full Linux capabilities within the
    container.

:macro-def:`DOCKER_PERFORM_TEST`
    When the *condor_startd* starts up, and on every
    :tool:`condor_reconfig`, it runs a simple Docker
    container to verify that Docker completely works.  If 
    DOCKER_PERFORM_TEST is false, this test is skipped.

:macro-def:`DOCKER_RUN_UNDER_INIT`
    A boolean value which defaults to true, which tells the execution
    point to run Docker universe jobs with the --init option.
    
:macro-def:`DOCKER_EXTRA_ARGUMENTS`
    Any additional command line options the administrator wants to be
    added to the Docker container create command line can be set with
    this parameter. Note that the admin should be careful setting this,
    it is intended for newer Docker options that HTCondor doesn't support
    directly.  Arbitrary Docker options may break Docker universe, for example
    don't pass the --rm flag in DOCKER_EXTRA_ARGUMENTS, because then
    HTCondor cannot get the final exit status from a Docker job.

:macro-def:`DOCKER_NETWORKS`
    An optional, comma-separated list of admin-defined networks that a job
    may request with the ``docker_network_type`` submit file command.
    Advertised into the slot attribute DockerNetworks.

:macro-def:`DOCKER_NETWORK_NAME`
    A string that defaults to "docker0".  This is the name of the network
    that a docker universe job can use to talk to the host machine.  This
    is used by :tool:`condor_chirp`.

:macro-def:`DOCKER_SHM_SIZE`
    An optional knob that can be configured to adapt the ``--shm-size`` Docker
    container create argument. Allowed values are integers in bytes.
    If not set, ``--shm-size`` will not be specified by HTCondor and Docker's
    default is used.
    This is used to configure the size of the container's ``/dev/shm`` size adapting
    to the job's requested memory.

:macro-def:`DOCKER_CACHE_ADVERTISE_INTERVAL`
    The *condor_startd* periodically advertises how much disk
    space the docker daemon is using to store images into the
    slot attribute DockerCachedImageSize.  This knob, which 
    defaults to 1200 (seconds), controls how often the start
    polls the docker daemon for this information.

:macro-def:`DOCKER_LOG_DRIVER_NONE`
    When this knob is true (the default), condor passes the command line
    option --log-driver none to the docker container it creates.  This
    prevents the docker daemon from duplicating the job's stdout and saving
    it in a docker-specific place on disk to be viewed with the docker logs
    command, saving space on disk for jobs with large stdout.

:macro-def:`DOCKER_SKIP_IMAGE_ARCH_CHECK`
    Defaults to false.  When true, HTCondor ignores the Architecture field
    in the docker image, and allows images of any architecture to attempt to 
    run on the EP.  When true, if the Architecture in the image is defined
    and does not match the EP, the job is put on hold.

:macro-def:`DOCKER_TRUST_LOCAL_IMAGES`
    Defaults to false.  When true, docker universe jobs can use docker images
    that have been prestaged into the local docker image cache, even if
    that image cannot be pulled from a repository, or if it doesn't exist
    in any repository.

:macro-def:`OPENMPI_INSTALL_PATH`
    The location of the Open MPI installation on the local machine.
    Referenced by ``examples/openmpiscript``, which is used for running
    Open MPI jobs in the parallel universe. The Open MPI bin and lib
    directories should exist under this path. The default value is
    ``/usr/lib64/openmpi``.

:macro-def:`OPENMPI_EXCLUDE_NETWORK_INTERFACES`
    A comma-delimited list of network interfaces that Open MPI should
    not use for MPI communications. Referenced by
    ``examples/openmpiscript``, which is used for running Open MPI jobs
    in the parallel universe.

    The list should contain any interfaces that your job could
    potentially see from any execute machine. The list may contain
    undefined interfaces without generating errors. Open MPI should
    exclusively use low latency/high speed networks it finds (e.g.
    InfiniBand) regardless of this setting. The default value is
    ``docker0``,\ ``virbr0``.

These macros allow a *condor_startd* to directly connect to a *condor_schedd*,
so that it can run jobs on this startd without sharing it with other
schedds.

:macro-def:`STARTD_DIRECT_ATTACH_SCHEDD_NAME`
    The name of the *condor_schedd* to which this *condor_startd* should
    directly connect. If empty, the *condor_startd* will not directly connect to
    a *condor_schedd*.

:macro-def:`STARTD_DIRECT_ATTACH_SCHEDD_POOL`
    If the *condor_schedd* to which this *condor_startd* should directly connect is
    in a different HTCondor pool that this *condor_startd* (that is, it reports
    to a different collector), then this macro should be set to the name of the
    collector the *condor_schedd* reports to.  If empty, assume that the *condor_schedd*
    reports to the same collector as this *condor_startd*.

:macro-def:`STARTD_DIRECT_ATTACH_SCHEDD_SUBMITTER`
    If the *condor_schedd* to which this *condor_startd* should directly connect
    should only run jobs from one submitter, this parameter names that submitter.
    If empty, the *condor_startd* will run jobs from any submitter.

:macro-def:`STARTD_DIRECT_ATTACH_INTERVAL`
    The number of seconds between attempts by this *condor_startd* to directly
    connect to the *condor_schedd*. The default is 300 seconds (5 minutes).
