      

condor\_drain
=============

Control draining of an execute machine

Synopsis
--------

**condor\_drain** [**-help**\ ]

**condor\_drain** [**-debug**\ ] [**-pool  **\ *pool-name*] [**-graceful
\| -quick \| -fast**\ ] [**-resume-on-completion**\ ]
[**-check  **\ *expr*] [**-start  **\ *expr*] *machine-name*

**condor\_drain** [**-debug**\ ] [**-pool  **\ *pool-name*] **-cancel**
[**-request-id  **\ *id*] *machine-name*

Description
-----------

*condor\_drain* is an administrative command used to control the
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
replaces the draining machine’s normal ``START`` expression for the
duration of the draining state, potentially making those resources
available. See section
`3.7.1 <PolicyConfigurationforExecuteHostsandforSubmitHosts.html#x35-2630003.7.1>`__
for more information.

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
 **-pool **\ *pool-name*
    Specify an alternate HTCondor pool, if the default one is not
    desired.
 **-graceful**
    (the default) Honor the maximum vacate and retirement time policy.
 **-quick**
    Honor the maximum vacate time, but not the retirement time policy.
 **-fast**
    Honor neither the maximum vacate time policy nor the retirement time
    policy.
 **-resume-on-completion**
    When done draining, resume normal operation, such that potentially
    the whole machine could be claimed.
 **-check **\ *expr*
    Abort draining, if ``expr`` is not true for all slots to be drained.
 **-start **\ *expr*
    The ``START`` expression to use while the machine is draining. You
    can’t reference the machine’s existing ``START`` expression.
 **-cancel**
    Cancel a prior draining request, to permit the *condor\_negotiator*
    to use the machine again.
 **-request-id **\ *id*
    Specify a specific draining request to cancel, where *id* is given
    by the ``DrainingRequestId`` machine ClassAd attribute.

Exit Status
-----------

*condor\_drain* will exit with a non-zero status value if it fails and
zero status if it succeeds.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
