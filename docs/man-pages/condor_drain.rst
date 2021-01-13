*condor_drain*
===============

Control draining of an execute machine
:index:`condor_drain<single: condor_drain; HTCondor commands>`\ :index:`condor_drain command`

Synopsis
--------

**condor_drain** [**-help** ]

**condor_drain** [**-debug** ] [**-pool** *pool-name*]
[**-graceful | -quick | -fast**] [**-reason** *reason-text*]
[**-resume-on-completion | -restart-on-completion | -exit-on-completion**]
[**-check** *expr*] [**-start** *expr*] *machine-name*

**condor_drain** [**-debug** ] [**-pool** *pool-name*] **-cancel**
[**-request-id** *id*] *machine-name*

Description
-----------

*condor_drain* is an administrative command used to control the
draining of all slots on an execute machine. When a machine is draining,
it will not accept any new jobs unless the **-start** expression
specifies otherwise. Which machine to drain is specified by the argument
*machine-name*, and will be the same as the machine ClassAd attribute
``Machine``.

How currently running jobs are treated depends on the draining schedule
that is chosen with a command-line option:

 **-graceful**
    Initiate a graceful eviction of the job. This means all promises
    that have been made to the job are honored, including
    ``MaxJobRetirementTime``. The eviction of jobs is coordinated to
    reduce idle time. This means that if one slot has a job with a long
    retirement time and the other slots have jobs with shorter
    retirement times, the effective retirement time for all of the jobs
    is the longer one. If no draining schedule is specified,
    **-graceful** is chosen by default.
 **-quick**
    ``MaxJobRetirementTime`` is not honored. Eviction of jobs is
    immediately initiated. Jobs are given time to shut down and produce
    checkpoints, according to the usual policy, that is, given by
    ``MachineMaxVacateTime``.
 **-fast**
    Jobs are immediately hard-killed, with no chance to gracefully shut
    down or produce a checkpoint.

If you specify **-graceful**, you may also specify **-start**. On a
gracefully-draining machine, some jobs may finish retiring before
others. By default, the resources used by the newly-retired jobs do not
become available for use by other jobs until the machine exits the
draining state (see below). The **-start** expression you supply
replaces the draining machine's normal ``START`` expression for the
duration of the draining state, potentially making those resources
available. See the
:ref:`admin-manual/policy-configuration:*condor_startd* Policy Configuration`
section for more information.

Once draining is complete, the machine will enter the Drained/Idle
state. To resume normal operation (negotiation) at that time or any
previous time during draining, the **-cancel** option may be used. The
**-resume-on-completion** option results in automatic resumption of
normal operation once draining has completed, and may be used when
initiating draining. This is useful for forcing a machine with a
partitionable slots to join all of the resources back together into one
machine, facilitating de-fragmentation and whole machine negotiation.

Options
-------

 **-help**
    Display brief usage information and exit.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-pool** *pool-name*
    Specify an alternate HTCondor pool, if the default one is not
    desired.
 **-graceful**
    (the default) Honor the maximum vacate and retirement time policy.
 **-quick**
    Honor the maximum vacate time, but not the retirement time policy.
 **-fast**
    Honor neither the maximum vacate time policy nor the retirement time
    policy.
 **-reason** *reason-text*
    Set the drain reason to *reason-text*. While the *condor_startd* is draining
    it will advertise the given reason. If this option is not used the
    reason defaults to the name of the user that started the drain.
 **-resume-on-completion**
    When done draining, resume normal operation, such that potentially
    the whole machine could be claimed.
 **-restart-on-completion**
    When done draining, restart the *condor_startd* daemon so that
    configuration changes will take effect.
 **-exit-on-completion**
    When done draining, shut down the *condor_startd* daemon and tell
    the *condor_master* not to restart it automatically.
 **-check** *expr*
    Abort draining, if ``expr`` is not true for all slots to be drained.
 **-start** *expr*
    The ``START`` expression to use while the machine is draining. You
    can't reference the machine's existing ``START`` expression.
 **-cancel**
    Cancel a prior draining request, to permit the *condor_negotiator*
    to use the machine again.
 **-request-id** *id*
    Specify a specific draining request to cancel, where *id* is given
    by the ``DrainingRequestId`` machine ClassAd attribute.

Exit Status
-----------

*condor_drain* will exit with a non-zero status value if it fails and
zero status if it succeeds.

