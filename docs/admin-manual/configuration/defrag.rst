:index:`Defrag Daemon Options<single: Configuration; Defrag Daemon Options>`

.. _defrag_config_options:

Defrag Daemon Configuration Options
===================================

These configuration variables affect the *condor_defrag* daemon. A
general discussion of *condor_defrag* may be found in
:ref:`admin-manual/cm-configuration:defragmenting dynamic slots`.

:macro-def:`DEFRAG_NAME`
    Used to give an prefix value to the ``Name`` attribute in the
    *condor_defrag* daemon's ClassAd. Defaults to the name given in the :macro:`DAEMON_LIST`.
    This esoteric configuration macro
    might be used in the situation where there are two *condor_defrag*
    daemons running on one machine, and each reports to the same
    *condor_collector*. Different names will distinguish the two
    daemons. See the description of :macro:`MASTER_NAME` in
    :ref:`master_config_options`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`DEFRAG_DRAINING_MACHINES_PER_HOUR`
    A floating point number that specifies how many machines should be
    drained per hour. The default is 0, so no draining will happen
    unless this setting is changed. Each *condor_startd* is considered
    to be one machine. The actual number of machines drained per hour
    may be less than this if draining is halted by one of the other
    defragmentation policy controls. The granularity in timing of
    draining initiation is controlled by :macro:`DEFRAG_INTERVAL`.
    The lowest rate of draining that is supported is one machine per
    day or one machine per :macro:`DEFRAG_INTERVAL`, whichever is
    lower. A fractional number of machines contributing to the value of
    :macro:`DEFRAG_DRAINING_MACHINES_PER_HOUR` is rounded to the nearest
    whole number of machines on a per day basis.

:macro-def:`DEFRAG_DRAINING_START_EXPR`
    A ClassAd expression that replaces the machine's :macro:`START`
    expression while it's draining. Slots which
    accepted a job after the machine began draining set the machine ad
    attribute :ad-attr:`AcceptedWhileDraining` to ``true``. When the last job
    which was not accepted while draining exits, all other jobs are
    immediately evicted with a ``MaxJobRetirementTime`` of 0; job vacate
    times are still respected. While the jobs which were accepted while
    draining are vacating, the :macro:`START` expression is ``false``. Using
    ``$(START)`` in this expression is usually a mistake: it will be
    replaced by the defrag daemon's :macro:`START` expression, not the value
    of the target machine's :macro:`START` expression (and especially not the
    value of its :macro:`START` expression at the time draining begins).

:macro-def:`DEFRAG_REQUIREMENTS`
    An expression that narrows the selection of which machines to drain.
    By default *condor_defrag* will drain all machines that are drainable.
    A machine, meaning a *condor_startd*, is matched if any of its
    partitionable slots match this expression. Machines are automatically excluded if
    they cannot be drained, are already draining, or if they match
    :macro:`DEFRAG_WHOLE_MACHINE_EXPR`.

    The *condor_defrag* daemon will always add the following requirements to :macro:`DEFRAG_REQUIREMENTS`

    .. code-block:: condor-classad-expr

          PartitionableSlot && Offline =!= true && Draining =!= true

:macro-def:`DEFRAG_CANCEL_REQUIREMENTS`
    An expression that is periodically evaluated against machines that are draining.
    When this expression evaluates to ``True``, draining will be cancelled.
    This defaults to 
    ``$(DEFRAG_WHOLE_MACHINE_EXPR)``\ :index:`DEFRAG_WHOLE_MACHINE_EXPR`.
    This could be used to drain partial rather than whole machines.
    Beginning with version 8.9.11, only machines that have no ``DrainReason``
    or a value of ``"Defrag"`` for ``DrainReason``
    will be checked to see if draining should be cancelled.  Beginning with
    10.7.0 The Defrag daemon will also check for its own name in the ``DrainReason``.

:macro-def:`DEFRAG_RANK`
    An expression that specifies which machines are more desirable to
    drain. The expression should evaluate to a number for each candidate
    machine to be drained. If the number of machines to be drained is
    less than the number of candidates, the machines with higher rank
    will be chosen. The rank of a machine, meaning a *condor_startd*,
    is the rank of its highest ranked slot. The default rank is
    ``-ExpectedMachineGracefulDrainingBadput``.

:macro-def:`DEFRAG_WHOLE_MACHINE_EXPR`
    An expression that specifies which machines are already operating as
    whole machines. The default is

    .. code-block:: condor-classad-expr

          Cpus == TotalSlotCpus

    A machine is matched if any Partitionable slot on the machine matches this
    expression and the machine is not draining or was drained by *condor_defrag*.
    Each *condor_startd* is considered to be one machine.
    Whole machines are excluded when selecting machines to drain. They
    are also counted against :macro:`DEFRAG_MAX_WHOLE_MACHINES`.

:macro-def:`DEFRAG_MAX_WHOLE_MACHINES`
    An integer that specifies the maximum number of whole machines. When
    the number of whole machines is greater than or equal to this, no
    new machines will be selected for draining. Each *condor_startd* is
    counted as one machine. The special value -1 indicates that there is
    no limit. The default is -1.

:macro-def:`DEFRAG_MAX_CONCURRENT_DRAINING`
    An integer that specifies the maximum number of draining machines.
    When the number of machines that are draining is greater than or
    equal to this, no new machines will be selected for draining. Each
    draining *condor_startd* is counted as one machine. The special
    value -1 indicates that there is no limit. The default is -1.

:macro-def:`DEFRAG_INTERVAL`
    An integer that specifies the number of seconds between evaluations
    of the defragmentation policy. In each cycle, the state of the pool
    is observed and machines are drained, if specified by the policy.
    The default is 600 seconds. Very small intervals could create
    excessive load on the *condor_collector*.

:macro-def:`DEFRAG_UPDATE_INTERVAL`
    An integer that specifies the number of seconds between times that
    the *condor_defrag* daemon sends updates to the collector. (See
    :ref:`classad-attributes/defrag-classad-attributes:defrag classad attributes`
    for information about the attributes in these updates.) The default is
    300 seconds.

:macro-def:`DEFRAG_SCHEDULE`
    A setting that specifies the draining schedule to use when draining
    machines. Possible values are ``graceful``, ``quick``, and ``fast``.
    The default is ``graceful``.

     graceful
        Initiate a graceful eviction of the job. This means all promises
        that have been made to the job are honored, including
        ``MaxJobRetirementTime``. The eviction of jobs is coordinated to
        reduce idle time. This means that if one slot has a job with a
        long retirement time and the other slots have jobs with shorter
        retirement times, the effective retirement time for all of the
        jobs is the longer one.
     quick
        ``MaxJobRetirementTime`` is not honored. Eviction of jobs is
        immediately initiated. Jobs are given time to shut down
        according to the usual policy, as given by
        :macro:`MachineMaxVacateTime`.
     fast
        Jobs are immediately hard-killed, with no chance to gracefully
        shut down.

:macro-def:`DEFRAG_LOG`
    The path to the *condor_defrag* daemon's log file. The default log
    location is ``$(LOG)/DefragLog``.
