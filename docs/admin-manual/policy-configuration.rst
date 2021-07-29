Policy Configuration for Execute Hosts and for Submit Hosts
===========================================================

.. note::
    Configuration templates make it easier to implement certain
    policies; see information on policy templates here:
    :ref:`admin-manual/configuration-templates:available configuration templates`.

*condor_startd* Policy Configuration
------------------------------------

:index:`condor_startd policy<single: condor_startd policy; configuration>`
:index:`of machines, to implement a given policy<single: of machines, to implement a given policy; configuration>`
:index:`configuration<single: configuration; startd>`

This section describes the configuration of machines, such that they,
through the *condor_startd* daemon, implement a desired policy for when
remote jobs should start, be suspended, (possibly) resumed, vacate (with
a checkpoint) or be killed. This policy is the heart of HTCondor's
balancing act between the needs and wishes of resource owners (machine
owners) and resource users (people submitting their jobs to HTCondor).
Please read this section carefully before changing any of the settings
described here, as a wrong setting can have a severe impact on either
the owners of machines in the pool or the users of the pool.

*condor_startd* Terminology
''''''''''''''''''''''''''''

Understanding the configuration requires an understanding of ClassAd
expressions, which are detailed in the :doc:`/misc-concepts/classad-mechanism`
section.
:index:`condor_startd`

Each machine runs one *condor_startd* daemon. Each machine may contain
one or more cores (or CPUs). The HTCondor construct of a slot describes
the unit which is matched to a job. Each slot may contain one or more
integer number of cores. Each slot is represented by its own machine
ClassAd, distinguished by the machine ClassAd attribute ``Name``, which
is of the form ``slot<N>@hostname``. The value for ``<N>`` will also be
defined with machine ClassAd attribute ``SlotID``.

Each slot has its own machine ClassAd, and within that ClassAd, its own
state and activity. Other policy expressions are propagated or inherited
from the machine configuration by the *condor_startd* daemon, such that
all slots have the same policy from the machine configuration. This
requires configuration expressions to incorporate the ``SlotID``
attribute when policy is intended to be individualized based on a slot.
So, in this discussion of policy expressions, where a machine is
referenced, the policy can equally be applied to a slot.

The *condor_startd* daemon represents the machine on which it is
running to the HTCondor pool. The daemon publishes characteristics about
the machine in the machine's ClassAd to aid matchmaking with resource
requests. The values of these attributes may be listed by using the
command:

.. code-block:: console

    $ condor_status -l hostname

The ``START`` Expression
''''''''''''''''''''''''

The most important expression to the *condor_startd* is the ``START``
:index:`START` expression. This expression describes the
conditions that must be met for a machine or slot to run a job. This
expression can reference attributes in the machine's ClassAd (such as
``KeyboardIdle`` and ``LoadAvg``) and attributes in a job ClassAd (such
as ``Owner``, ``Imagesize``, and ``Cmd``, the name of the executable the
job will run). The value of the ``START`` expression plays a crucial
role in determining the state and activity of a machine.

The ``Requirements`` expression is used for matching machines with jobs.

In situations where a machine wants to make itself unavailable for
further matches, the ``Requirements`` expression is set to ``False``.
When the ``START`` expression locally evaluates to ``True``, the machine
advertises the ``Requirements`` expression as ``True`` and does not
publish the ``START`` expression.

Normally, the expressions in the machine ClassAd are evaluated against
certain request ClassAds in the *condor_negotiator* to see if there is
a match, or against whatever request ClassAd currently has claimed the
machine. However, by locally evaluating an expression, the machine only
evaluates the expression against its own ClassAd. If an expression
cannot be locally evaluated (because it references other expressions
that are only found in a request ClassAd, such as ``Owner`` or
``Imagesize``), the expression is (usually) undefined. See
theh :doc:`/misc-concepts/classad-mechanism` section for specifics on
how undefined terms are handled in ClassAd expression evaluation.

A note of caution is in order when modifying the ``START`` expression to
reference job ClassAd attributes. When using the ``POLICY : Desktop``
configuration template, the ``IS_OWNER`` expression is a function of the
``START`` expression:

.. code-block:: condor-classad-expr

    START =?= FALSE

See a detailed discussion of the ``IS_OWNER`` expression in
:ref:`admin-manual/policy-configuration:*condor_startd* policy configuration`.
However, the machine locally evaluates the ``IS_OWNER`` expression to determine
if it is capable of running jobs for HTCondor. Any job ClassAd attributes
appearing in the ``START`` expression, and hence in the ``IS_OWNER`` expression,
are undefined in this context, and may lead to unexpected behavior. Whenever
the ``START`` expression is modified to reference job ClassAd
attributes, the ``IS_OWNER`` expression should also be modified to
reference only machine ClassAd attributes.

.. note::
    If you have machines with lots of real memory and swap space such
    that the only scarce resource is CPU time, consider defining
    ``JOB_RENICE_INCREMENT`` :index:`JOB_RENICE_INCREMENT` so that
    HTCondor starts jobs on the machine with low priority. Then, further
    configure to set up the machines with:

    .. code-block:: condor-config

        START = True
        SUSPEND = False
        PREEMPT = False
        KILL = False

In this way, HTCondor jobs always run and can never be kicked off from
activity on the machine. However, because they would run with the low
priority, interactive response on the machines will not suffer. A
machine user probably would not notice that HTCondor was running the
jobs, assuming you had enough free memory for the HTCondor jobs such
that there was little swapping.

The ``RANK`` Expression
'''''''''''''''''''''''

A machine may be configured to prefer certain jobs over others using the
``RANK`` expression. It is an expression, like any other in a machine
ClassAd. It can reference any attribute found in either the machine
ClassAd or a job ClassAd. The most common use of this expression is
likely to configure a machine to prefer to run jobs from the owner of
that machine, or by extension, a group of machines to prefer jobs from
the owners of those machines. :index:`example<single: example; configuration>`

For example, imagine there is a small research group with 4 machines
called tenorsax, piano, bass, and drums. These machines are owned by the
4 users coltrane, tyner, garrison, and jones, respectively.

Assume that there is a large HTCondor pool in the department, and this
small research group has spent a lot of money on really fast machines
for the group. As part of the larger pool, but to implement a policy
that gives priority on the fast machines to anyone in the small research
group, set the ``RANK`` expression on the machines to reference the
``Owner`` attribute and prefer requests where that attribute matches one
of the people in the group as in

.. code-block:: condor-config

    RANK = Owner == "coltrane" || Owner == "tyner" \
        || Owner == "garrison" || Owner == "jones"

The ``RANK`` expression is evaluated as a floating point number.
However, like in C, boolean expressions evaluate to either 1 or 0
depending on if they are ``True`` or ``False``. So, if this expression
evaluated to 1, because the remote job was owned by one of the preferred
users, it would be a larger value than any other user for whom the
expression would evaluate to 0.

A more complex ``RANK`` expression has the same basic set up, where
anyone from the group has priority on their fast machines. Its
difference is that the machine owner has better priority on their own
machine. To set this up for Garrison's machine (``bass``), place the
following entry in the local configuration file of machine ``bass``:

.. code-block:: condor-config

    RANK = (Owner == "coltrane") + (Owner == "tyner") \
        + ((Owner == "garrison") * 10) + (Owner == "jones")

Note that the parentheses in this expression are important, because the
``+`` operator has higher default precedence than ``==``.

The use of ``+`` instead of ``||`` allows us to distinguish which terms
matched and which ones did not. If anyone not in the research group
quartet was running a job on the machine called ``bass``, the ``RANK``
would evaluate numerically to 0, since none of the boolean terms
evaluates to 1, and 0+0+0+0 still equals 0.

Suppose Elvin Jones submits a job. His job would match the ``bass``
machine, assuming ``START`` evaluated to ``True`` for him at that time.
The ``RANK`` would numerically evaluate to 1. Therefore, the Elvin Jones
job could preempt the HTCondor job currently running. Further assume
that later Jimmy Garrison submits a job. The ``RANK`` evaluates to 10 on
machine ``bass``, since the boolean that matches gets multiplied by 10.
Due to this, Jimmy Garrison's job could preempt Elvin Jones' job on the
``bass`` machine where Jimmy Garrison's jobs are preferred.

The ``RANK`` expression is not required to reference the ``Owner`` of
the jobs. Perhaps there is one machine with an enormous amount of
memory, and others with not much at all. Perhaps configure this
large-memory machine to prefer to run jobs with larger memory
requirements:

.. code-block:: condor-config

    RANK = ImageSize

That's all there is to it. The bigger the job, the more this machine
wants to run it. It is an altruistic preference, always servicing the
largest of jobs, no matter who submitted them. A little less altruistic
is the ``RANK`` on Coltrane's machine that prefers John Coltrane's jobs
over those with the largest ``Imagesize``:

.. code-block:: condor-config

    RANK = (Owner == "coltrane" * 1000000000000) + Imagesize

This ``RANK`` does not work if a job is submitted with an image size of
more 10\ :sup:`12` Kbytes. However, with that size, this ``RANK``
expression preferring that job would not be HTCondor's only problem!

Machine States
''''''''''''''

:index:`of a machine<single: of a machine; state>` :index:`machine state`

A machine is assigned a state by HTCondor. The state depends on whether
or not the machine is available to run HTCondor jobs, and if so, what
point in the negotiations has been reached. The possible states are
:index:`Owner<single: Owner; machine state>` :index:`owner state`

 Owner
    The machine is being used by the machine owner, and/or is not
    available to run HTCondor jobs. When the machine first starts up, it
    begins in this state. :index:`Unclaimed<single: Unclaimed; machine state>`
    :index:`unclaimed state`
 Unclaimed
    The machine is available to run HTCondor jobs, but it is not
    currently doing so. :index:`Matched<single: Matched; machine state>`
    :index:`matched state`
 Matched
    The machine is available to run jobs, and it has been matched by the
    negotiator with a specific schedd. That schedd just has not yet
    claimed this machine. In this state, the machine is unavailable for
    further matches. :index:`Claimed<single: Claimed; machine state>`
    :index:`claimed state`
 Claimed
    The machine has been claimed by a schedd.
    :index:`Preempting<single: Preempting; machine state>`
    :index:`preempting state`
 Preempting
    The machine was claimed by a schedd, but is now preempting that
    claim for one of the following reasons.

    #. the owner of the machine came back
    #. another user with higher priority has jobs waiting to run
    #. another request that this resource would rather serve was found

    :index:`Backfill<single: Backfill; machine state>`
    :index:`backfill state`
 Backfill
    The machine is running a backfill computation while waiting for
    either the machine owner to come back or to be matched with an
    HTCondor job. This state is only entered if the machine is
    specifically configured to enable backfill jobs.
    :index:`Drained<single: Drained; machine state>`
    :index:`drained state`
 Drained
    The machine is not running jobs, because it is being drained. One
    reason a machine may be drained is to consolidate resources that
    have been divided in a partitionable slot. Consolidating the
    resources gives large jobs a chance to run.

.. figure:: /_images/machine-states-transitions.png
  :width: 600
  :alt: Machine states and the possible transitions between the states
  :align: center
  
  Machine states and the possible transitions between the states.


Each transition is labeled with a letter. The cause of each transition
is described below.

- Transitions out of the Owner state

    A
       The machine switches from Owner to Unclaimed whenever the
       ``START`` expression no longer locally evaluates to FALSE. This
       indicates that the machine is potentially available to run an
       HTCondor job.
    N
       The machine switches from the Owner to the Drained state whenever
       draining of the machine is initiated, for example by
       *condor_drain* or by the *condor_defrag* daemon.

- Transitions out of the Unclaimed state

    B
       The machine switches from Unclaimed back to Owner whenever the
       ``START`` expression locally evaluates to FALSE. This indicates
       that the machine is unavailable to run an HTCondor job and is in
       use by the resource owner.
    C
       The transition from Unclaimed to Matched happens whenever the
       *condor_negotiator* matches this resource with an HTCondor job.
    D
       The transition from Unclaimed directly to Claimed also happens if
       the *condor_negotiator* matches this resource with an HTCondor
       job. In this case the *condor_schedd* receives the match and
       initiates the claiming protocol with the machine before the
       *condor_startd* receives the match notification from the
       *condor_negotiator*.
    E
       The transition from Unclaimed to Backfill happens if the machine
       is configured to run backfill computations (see
       the :doc:`/admin-manual/setting-up-special-environments` section)
       and the ``START_BACKFILL`` expression evaluates to TRUE.
    P
       The transition from Unclaimed to Drained happens if draining of
       the machine is initiated, for example by *condor_drain* or by
       the *condor_defrag* daemon.

- Transitions out of the Matched state

    F
       The machine moves from Matched to Owner if either the ``START``
       expression locally evaluates to FALSE, or if the
       ``MATCH_TIMEOUT``\ :index:`MATCH_TIMEOUT` timer expires.
       This timeout is used to ensure that if a machine is matched with
       a given *condor_schedd*, but that *condor_schedd* does not
       contact the *condor_startd* to claim it, that the machine will
       give up on the match and become available to be matched again. In
       this case, since the ``START`` expression does not locally
       evaluate to FALSE, as soon as transition **F** is complete, the
       machine will immediately enter the Unclaimed state again (via
       transition **A**). The machine might also go from Matched to
       Owner if the *condor_schedd* attempts to perform the claiming
       protocol but encounters some sort of error. Finally, the machine
       will move into the Owner state if the *condor_startd* receives a
       *condor_vacate* command while it is in the Matched state.
    G
       The transition from Matched to Claimed occurs when the
       *condor_schedd* successfully completes the claiming protocol
       with the *condor_startd*.

- Transitions out of the Claimed state

    H
       From the Claimed state, the only possible destination is the
       Preempting state. This transition can be caused by many reasons:

       -  The *condor_schedd* that has claimed the machine has no more
          work to perform and releases the claim
       -  The ``PREEMPT`` expression evaluates to ``True`` (which
          usually means the resource owner has started using the machine
          again and is now using the keyboard, mouse, CPU, etc.)
       -  The *condor_startd* receives a *condor_vacate* command
       -  The *condor_startd* is told to shutdown (either via a signal
          or a *condor_off* command)
       -  The resource is matched to a job with a better priority
          (either a better user priority, or one where the machine rank
          is higher)

- Transitions out of the Preempting state

    I
       The resource will move from Preempting back to Claimed if the
       resource was matched to a job with a better priority.
    J
       The resource will move from Preempting to Owner if the
       ``PREEMPT`` expression had evaluated to TRUE, if *condor_vacate*
       was used, or if the ``START`` expression locally evaluates to
       FALSE when the *condor_startd* has finished evicting whatever
       job it was running when it entered the Preempting state.

- Transitions out of the Backfill state

    K
       The resource will move from Backfill to Owner for the following
       reasons:

       -  The ``EVICT_BACKFILL`` expression evaluates to TRUE
       -  The *condor_startd* receives a *condor_vacate* command
       -  The *condor_startd* is being shutdown

    L
       The transition from Backfill to Matched occurs whenever a
       resource running a backfill computation is matched with a
       *condor_schedd* that wants to run an HTCondor job.
    M
       The transition from Backfill directly to Claimed is similar to
       the transition from Unclaimed directly to Claimed. It only occurs
       if the *condor_schedd* completes the claiming protocol before
       the *condor_startd* receives the match notification from the
       *condor_negotiator*.

- Transitions out of the Drained state

    O
       The transition from Drained to Owner state happens when draining
       is finalized or is canceled. When a draining request is made, the
       request either asks for the machine to stay in a Drained state
       until canceled, or it asks for draining to be automatically
       finalized once all slots have finished draining.

The Claimed State and Leases
''''''''''''''''''''''''''''

:index:`claimed, the claim lease<single: claimed, the claim lease; machine state>`
:index:`claim lease`

When a *condor_schedd* claims a *condor_startd*, there is a claim
lease. So long as the keep alive updates from the *condor_schedd* to
the *condor_startd* continue to arrive, the lease is reset. If the
lease duration passes with no updates, the *condor_startd* drops the
claim and evicts any jobs the *condor_schedd* sent over.

The alive interval is the amount of time between, or the frequency at
which the *condor_schedd* sends keep alive updates to all
*condor_schedd* daemons. An alive update resets the claim lease at the
*condor_startd*. Updates are UDP packets.

Initially, as when the *condor_schedd* starts up, the alive interval
starts at the value set by the configuration variable ``ALIVE_INTERVAL``
:index:`ALIVE_INTERVAL`. It may be modified when a job is started.
The job's ClassAd attribute ``JobLeaseDuration`` is checked. If the
value of ``JobLeaseDuration/3`` is less than the current alive interval,
then the alive interval is set to either this lower value or the imposed
lowest limit on the alive interval of 10 seconds. Thus, the alive
interval starts at ``ALIVE_INTERVAL`` and goes down, never up.

If a claim lease expires, the *condor_startd* will drop the claim. The
length of the claim lease is the job's ClassAd attribute
``JobLeaseDuration``. ``JobLeaseDuration`` defaults to 40 minutes time,
except when explicitly set within the job's submit description file. If
``JobLeaseDuration`` is explicitly set to 0, or it is not set as may be
the case for a Web Services job that does not define the attribute, then
``JobLeaseDuration`` is given the Undefined value. Further, when
undefined, the claim lease duration is calculated with
``MAX_CLAIM_ALIVES_MISSED * alive interval``. The alive interval is the
current value, as sent by the *condor_schedd*. If the *condor_schedd*
reduces the current alive interval, it does not update the
*condor_startd*.

Machine Activities
''''''''''''''''''

:index:`machine activity`
:index:`of a machine<single: of a machine; activity>`

Within some machine states, activities of the machine are defined. The
state has meaning regardless of activity. Differences between activities
are significant. Therefore, a "state/activity" pair describes a machine.
The following list describes all the possible state/activity pairs.

-  Owner :index:`Idle<single: Idle; machine activity>`

    Idle
       This is the only activity for Owner state. As far as HTCondor is
       concerned the machine is Idle, since it is not doing anything for
       HTCondor.

   :index:`Unclaimed<single: Unclaimed; machine activity>`

-  Unclaimed

    Idle
       This is the normal activity of Unclaimed machines. The machine is
       still Idle in that the machine owner is willing to let HTCondor
       jobs run, but HTCondor is not using the machine for anything.
       :index:`Benchmarking<single: Benchmarking; machine activity>`
    Benchmarking
       The machine is running benchmarks to determine the speed on this
       machine. This activity only occurs in the Unclaimed state. How
       often the activity occurs is determined by the ``RUNBENCHMARKS``
       expression.

-  Matched

    Idle
       When Matched, the machine is still Idle to HTCondor.

-  Claimed

    Idle
       In this activity, the machine has been claimed, but the schedd
       that claimed it has yet to activate the claim by requesting a
       *condor_starter* to be spawned to service a job. The machine
       returns to this state (usually briefly) when jobs (and therefore
       *condor_starter*) finish. :index:`Busy<single: Busy; machine activity>`
    Busy
       Once a *condor_starter* has been started and the claim is
       active, the machine moves to the Busy activity to signify that it
       is doing something as far as HTCondor is concerned.
       :index:`Suspended<single: Suspended; machine activity>`
    Suspended
       If the job is suspended by HTCondor, the machine goes into the
       Suspended activity. The match between the schedd and machine has
       not been broken (the claim is still valid), but the job is not
       making any progress and HTCondor is no longer generating a load
       on the machine. :index:`Retiring<single: Retiring; machine activity>`
    Retiring
       When an active claim is about to be preempted for any reason, it
       enters retirement, while it waits for the current job to finish.
       The ``MaxJobRetirementTime`` expression determines how long to
       wait (counting since the time the job started). Once the job
       finishes or the retirement time expires, the Preempting state is
       entered.

-  Preempting The Preempting state is used for evicting an HTCondor job
   from a given machine. When the machine enters the Preempting state,
   it checks the ``WANT_VACATE`` expression to determine its activity.
   :index:`Vacating<single: Vacating; machine activity>`

    Vacating
       In the Vacating activity, the job that was running is in the
       process of checkpointing. As soon as the checkpoint process
       completes, the machine moves into either the Owner state or the
       Claimed state, depending on the reason for its preemption.
       :index:`Killing<single: Killing; machine activity>`
    Killing
       Killing means that the machine has requested the running job to
       exit the machine immediately, without checkpointing.

   :index:`Backfill<single: Backfill; machine activity>`
-  Backfill

    Idle
       The machine is configured to run backfill jobs and is ready to do
       so, but it has not yet had a chance to spawn a backfill manager
       (for example, the BOINC client).
    Busy
       The machine is performing a backfill computation.
    Killing
       The machine was running a backfill computation, but it is now
       killing the job to either return resources to the machine owner,
       or to make room for a regular HTCondor job.

   :index:`Drained<single: Drained; machine activity>`
-  Drained

    Idle
       All slots have been drained.
    Retiring
       This slot has been drained. It is waiting for other slots to
       finish draining.

The following diagram gives the overall view of all machine states and
activities and shows the possible transitions from one to another within the
HTCondor system. Each transition is labeled with a number on the diagram, and
transition numbers referred to in this manual will be **bold**.
:index:`machine state and activities figure`
:index:`state and activities figure`
:index:`activities and state figure`

.. figure:: /_images/machine-states-activities.png
  :width: 700
  :alt: Machine States and Activities
  :align: center

  Machine States and Activities


Various expressions are used to determine when and if many of these
state and activity transitions occur. Other transitions are initiated by
parts of the HTCondor protocol (such as when the *condor_negotiator*
matches a machine with a schedd). The following section describes the
conditions that lead to the various state and activity transitions.

State and Activity Transitions
''''''''''''''''''''''''''''''

:index:`transitions<single: transitions; machine state>`
:index:`transitions<single: transitions; machine activity>`
:index:`transitions<single: transitions; state>` :index:`transitions<single: transitions; activity>`

This section traces through all possible state and activity transitions
within a machine and describes the conditions under which each one
occurs. Whenever a transition occurs, HTCondor records when the machine
entered its new activity and/or new state. These times are often used to
write expressions that determine when further transitions occurred. For
example, enter the Killing activity if a machine has been in the
Vacating activity longer than a specified amount of time.

Owner State
"""""""""""

:index:`Owner<single: Owner; machine state>` :index:`owner state`

When the startd is first spawned, the machine it represents enters the
Owner state. The machine remains in the Owner state while the expression
``IS_OWNER`` :index:`IS_OWNER` evaluates to TRUE. If the
``IS_OWNER`` expression evaluates to FALSE, then the machine transitions
to the Unclaimed state. The default value of ``IS_OWNER`` is FALSE,
which is intended for dedicated resources. But when the
``POLICY : Desktop`` configuration template is used, the ``IS_OWNER``
expression is optimized for a shared resource

.. code-block:: condor-classad-expr

    START =?= FALSE

So, the machine will remain in the Owner state as long as the ``START``
expression locally evaluates to FALSE.
The :ref:`admin-manual/policy-configuration:*condor_startd* policy configuration`
section provides more detail on the
``START`` expression. If the ``START`` locally evaluates to TRUE or
cannot be locally evaluated (it evaluates to UNDEFINED), transition
**1** occurs and the machine enters the Unclaimed state. The
``IS_OWNER`` expression is locally evaluated by the machine, and should
not reference job ClassAd attributes, which would be UNDEFINED.

The Owner state represents a resource that is in use by its interactive
owner (for example, if the keyboard is being used). The Unclaimed state
represents a resource that is neither in use by its interactive user,
nor the HTCondor system. From HTCondor's point of view, there is little
difference between the Owner and Unclaimed states. In both cases, the
resource is not currently in use by the HTCondor system. However, if a
job matches the resource's ``START`` expression, the resource is
available to run a job, regardless of if it is in the Owner or Unclaimed
state. The only differences between the two states are how the resource
shows up in *condor_status* and other reporting tools, and the fact
that HTCondor will not run benchmarking on a resource in the Owner
state. As long as the ``IS_OWNER`` expression is TRUE, the machine is in
the Owner State. When the ``IS_OWNER`` expression is FALSE, the machine
goes into the Unclaimed State.

Here is an example that assumes that the ``POLICY : Desktop``
configuration template is in use. If the ``START`` expression is

.. code-block:: condor-config

    START = KeyboardIdle > 15 * $(MINUTE) && Owner == "coltrane"

and if ``KeyboardIdle`` is 34 seconds, then the machine would remain in
the Owner state. Owner is undefined, and anything && FALSE is FALSE.

If, however, the ``START`` expression is

.. code-block:: condor-config

    START = KeyboardIdle > 15 * $(MINUTE) || Owner == "coltrane"

and ``KeyboardIdle`` is 34 seconds, then the machine leaves the Owner
state and becomes Unclaimed. This is because FALSE || UNDEFINED is
UNDEFINED. So, while this machine is not available to just anybody, if
user coltrane has jobs submitted, the machine is willing to run them.
Any other user's jobs have to wait until ``KeyboardIdle`` exceeds 15
minutes. However, since coltrane might claim this resource, but has not
yet, the machine goes to the Unclaimed state.

While in the Owner state, the startd polls the status of the machine
every ``UPDATE_INTERVAL`` :index:`UPDATE_INTERVAL` to see if
anything has changed that would lead it to a different state. This
minimizes the impact on the Owner while the Owner is using the machine.
Frequently waking up, computing load averages, checking the access times
on files, computing free swap space take time, and there is nothing time
critical that the startd needs to be sure to notice as soon as it
happens. If the ``START`` expression evaluates to TRUE and five minutes
pass before the startd notices, that's a drop in the bucket of
high-throughput computing.

The machine can only transition to the Unclaimed state from the Owner
state. It does so when the ``IS_OWNER`` expression no longer evaluates
to TRUE. With the ``POLICY : Desktop`` configuration template, that
happens when ``START`` no longer locally evaluates to FALSE.

Whenever the machine is not actively running a job, it will transition
back to the Owner state if ``IS_OWNER`` evaluates to TRUE. Once a job is
started, the value of ``IS_OWNER`` does not matter; the job either runs
to completion or is preempted. Therefore, you must configure the
preemption policy if you want to transition back to the Owner state from
Claimed Busy.

If draining of the machine is initiated while in the Owner state, the
slot transitions to Drained/Retiring (transition **36**).

Unclaimed State
"""""""""""""""

:index:`Unclaimed<single: Unclaimed; machine state>`
:index:`unclaimed state`

If the ``IS_OWNER`` expression becomes TRUE, then the machine returns to
the Owner state. If the ``IS_OWNER`` expression becomes FALSE, then the
machine remains in the Unclaimed state. The default value of
``IS_OWNER`` is FALSE (never enter Owner state). If the
``POLICY : Desktop`` configuration template is used, then the
``IS_OWNER`` expression is changed to

.. code-block:: condor-config

    START =?= FALSE

so that while in the Unclaimed state, if the ``START`` expression
locally evaluates to FALSE, the machine returns to the Owner state by
transition **2**.

When in the Unclaimed state, the ``RUNBENCHMARKS``
:index:`RUNBENCHMARKS` expression is relevant. If
``RUNBENCHMARKS`` evaluates to TRUE while the machine is in the
Unclaimed state, then the machine will transition from the Idle activity
to the Benchmarking activity (transition **3**) and perform benchmarks
to determine ``MIPS`` and ``KFLOPS``. When the benchmarks complete, the
machine returns to the Idle activity (transition **4**).

The startd automatically inserts an attribute, ``LastBenchmark``,
whenever it runs benchmarks, so commonly ``RunBenchmarks`` is defined in
terms of this attribute, for example:

.. code-block:: condor-config

    RunBenchmarks = (time() - LastBenchmark) >= (4 * $(HOUR))

This macro calculates the time since the last benchmark, so when this
time exceeds 4 hours, we run the benchmarks again. The startd keeps a
weighted average of these benchmarking results to try to get the most
accurate numbers possible. This is why it is desirable for the startd to
run them more than once in its lifetime.

.. note::
    ``LastBenchmark`` is initialized to 0 before benchmarks have ever
    been run. To have the *condor_startd* run benchmarks as soon as the
    machine is Unclaimed (if it has not done so already), include a term
    using ``LastBenchmark`` as in the example above.

.. note::
    If ``RUNBENCHMARKS`` is defined and set to something other than
    FALSE, the startd will automatically run one set of benchmarks when it
    first starts up. To disable benchmarks, both at startup and at any time
    thereafter, set ``RUNBENCHMARKS`` to FALSE or comment it out of the
    configuration file.

From the Unclaimed state, the machine can go to four other possible
states: Owner (transition **2**), Backfill/Idle, Matched, or
Claimed/Idle.

Once the *condor_negotiator* matches an Unclaimed machine with a
requester at a given schedd, the negotiator sends a command to both
parties, notifying them of the match. If the schedd receives that
notification and initiates the claiming procedure with the machine
before the negotiator's message gets to the machine, the Match state is
skipped, and the machine goes directly to the Claimed/Idle state
(transition **5**). However, normally the machine will enter the Matched
state (transition **6**), even if it is only for a brief period of time.

If the machine has been configured to perform backfill jobs (see
the :doc:`/admin-manual/setting-up-special-environments` section),
while it is in Unclaimed/Idle it will evaluate the ``START_BACKFILL``
:index:`START_BACKFILL` expression. Once ``START_BACKFILL``
evaluates to TRUE, the machine will enter the Backfill/Idle state
(transition **7**) to begin the process of running backfill jobs.

If draining of the machine is initiated while in the Unclaimed state,
the slot transitions to Drained/Retiring (transition **37**).

Matched State
"""""""""""""

:index:`Matched<single: Matched; machine state>` :index:`matched state`

The Matched state is not very interesting to HTCondor. Noteworthy in
this state is that the machine lies about its ``START`` expression while
in this state and says that ``Requirements`` are ``False`` to prevent
being matched again before it has been claimed. Also interesting is that
the startd starts a timer to make sure it does not stay in the Matched
state too long. The timer is set with the ``MATCH_TIMEOUT``
:index:`MATCH_TIMEOUT` configuration file macro. It is specified
in seconds and defaults to 120 (2 minutes). If the schedd that was
matched with this machine does not claim it within this period of time,
the machine gives up, and goes back into the Owner state via transition
**8**. It will probably leave the Owner state right away for the
Unclaimed state again and wait for another match.

At any time while the machine is in the Matched state, if the ``START``
expression locally evaluates to FALSE, the machine enters the Owner
state directly (transition **8**).

If the schedd that was matched with the machine claims it before the
``MATCH_TIMEOUT`` expires, the machine goes into the Claimed/Idle state
(transition **9**).

Claimed State
"""""""""""""

:index:`Claimed<single: Claimed; machine state>` :index:`claimed state`

The Claimed state is certainly the most complex state. It has the most
possible activities and the most expressions that determine its next
activities. In addition, the *condor_checkpoint* and *condor_vacate*
commands affect the machine when it is in the Claimed state.

In general, there are two sets of expressions that might take effect,
depending on the universe of the job running on the claim: vanilla,
and all others.  The normal expressions look like the following:

.. code-block:: condor-config

    WANT_SUSPEND            = True
    WANT_VACATE             = $(ActivationTimer) > 10 * $(MINUTE)
    SUSPEND                 = $(KeyboardBusy) || $(CPUBusy)
    ...

The vanilla expressions have the string"_VANILLA" appended to their
names. For example:

.. code-block:: condor-config

    WANT_SUSPEND_VANILLA    = True
    WANT_VACATE_VANILLA     = True
    SUSPEND_VANILLA         = $(KeyboardBusy) || $(CPUBusy)
    ...

Without specific vanilla versions, the normal versions will be used for
all jobs, including vanilla jobs. In this manual, the normal expressions
are referenced.

While Claimed, the ``POLLING_INTERVAL`` :index:`POLLING_INTERVAL`
takes effect, and the startd polls the machine much more frequently to
evaluate its state.

If the machine owner starts typing on the console again, it is best to
notice this as soon as possible to be able to start doing whatever the
machine owner wants at that point. For multi-core machines, if any slot
is in the Claimed state, the startd polls the machine frequently. If
already polling one slot, it does not cost much to evaluate the state of
all the slots at the same time.

There are a variety of events that may cause the startd to try to get
rid of or temporarily suspend a running job. Activity on the machine's
console, load from other jobs, or shutdown of the startd via an
administrative command are all possible sources of interference. Another
one is the appearance of a higher priority claim to the machine by a
different HTCondor user.

Depending on the configuration, the startd may respond quite differently
to activity on the machine, such as keyboard activity or demand for the
cpu from processes that are not managed by HTCondor. The startd can be
configured to completely ignore such activity or to suspend the job or
even to kill it. A standard configuration for a desktop machine might be
to go through successive levels of getting the job out of the way. The
first and least costly to the job is suspending it.
If suspending the job for a short while does
not satisfy the machine owner (the owner is still using the machine
after a specific period of time), the startd moves on to vacating the
job. Vanilla jobs are sent a
soft kill signal so that they can gracefully shut down if necessary; the
default is SIGTERM. If vacating does not satisfy the machine owner
(usually because it is taking too long and the owner wants their machine
back now), the final, most drastic stage is reached: killing. Killing is
a quick death to the job, using a hard-kill signal that cannot be
intercepted by the application. For vanilla jobs that do no special
signal handling, vacating and killing are equivalent.

The ``WANT_SUSPEND`` expression determines if the machine will evaluate
the ``SUSPEND`` expression to consider entering the Suspended activity.
The ``WANT_VACATE`` expression determines what happens when the machine
enters the Preempting state. It will go to the Vacating activity or
directly to Killing. If one or both of these expressions evaluates to
FALSE, the machine will skip that stage of getting rid of the job and
proceed directly to the more drastic stages.

When the machine first enters the Claimed state, it goes to the Idle
activity. From there, it has two options. It can enter the Preempting
state via transition **10** (if a *condor_vacate* arrives, or if the
``START`` expression locally evaluates to FALSE), or it can enter the
Busy activity (transition **11**) if the schedd that has claimed the
machine decides to activate the claim and start a job.

From Claimed/Busy, the machine can transition to three other
state/activity pairs. The startd evaluates the ``WANT_SUSPEND``
expression to decide which other expressions to evaluate. If
``WANT_SUSPEND`` is TRUE, then the startd evaluates the ``SUSPEND``
expression. If ``WANT_SUSPEND`` is any value other than TRUE, then the
startd will evaluate the ``PREEMPT`` expression and skip the Suspended
activity entirely. By transition, the possible state/activity
destinations from Claimed/Busy:

Claimed/Idle
    If the starter that is serving a given job exits (for example
    because the jobs completes), the machine will go to Claimed/Idle
    (transition **12**).
    Claimed/Retiring
    If ``WANT_SUSPEND`` is FALSE and the ``PREEMPT`` expression is
    ``True``, the machine enters the Retiring activity (transition
    **13**). From there, it waits for a configurable amount of time for
    the job to finish before moving on to preemption.

    Another reason the machine would go from Claimed/Busy to
    Claimed/Retiring is if the *condor_negotiator* matched the machine
    with a "better" match. This better match could either be from the
    machine's perspective using the startd ``RANK`` expression, or it
    could be from the negotiator's perspective due to a job with a
    higher user priority.

    Another case resulting in a transition to Claimed/Retiring is when
    the startd is being shut down. The only exception is a "fast"
    shutdown, which bypasses retirement completely.

Claimed/Suspended
    If both the ``WANT_SUSPEND`` and ``SUSPEND`` expressions evaluate to
    TRUE, the machine suspends the job (transition **14**).

If a *condor_checkpoint* command arrives, or the
``PERIODIC_CHECKPOINT`` expression evaluates to TRUE, there is no state
change. The startd has no way of knowing when this process completes, so
periodic checkpointing can not be another state. Periodic checkpointing
remains in the Claimed/Busy state and appears as a running job.

From the Claimed/Suspended state, the following transitions may occur:

Claimed/Busy
    If the ``CONTINUE`` expression evaluates to TRUE, the machine
    resumes the job and enters the Claimed/Busy state (transition
    **15**) or the Claimed/Retiring state (transition **16**), depending
    on whether the claim has been preempted.

Claimed/Retiring
    If the ``PREEMPT`` expression is TRUE, the machine will enter the
    Claimed/Retiring activity (transition **16**).

Preempting
    If the claim is in suspended retirement and the retirement time
    expires, the job enters the Preempting state (transition **17**).
    This is only possible if ``MaxJobRetirementTime`` decreases during
    the suspension.

For the Claimed/Retiring state, the following transitions may occur:

Preempting
    If the job finishes or the job's run time exceeds the value defined
    for the job ClassAd attribute ``MaxJobRetirementTime``, the
    Preempting state is entered (transition **18**). The run time is
    computed from the time when the job was started by the startd minus
    any suspension time. When retiring due to *condor_startd* daemon
    shutdown or restart, it is possible for the administrator to issue a
    peaceful shutdown command, which causes ``MaxJobRetirementTime`` to
    effectively be infinite, avoiding any killing of jobs. It is also
    possible for the administrator to issue a fast shutdown command,
    which causes ``MaxJobRetirementTime`` to be effectively 0.

Claimed/Busy
    If the startd was retiring because of a preempting claim only and
    the preempting claim goes away, the normal Claimed/Busy state is
    resumed (transition **19**). If instead the retirement is due to
    owner activity (``PREEMPT``) or the startd is being shut down, no
    unretirement is possible.

Claimed/Suspended
    In exactly the same way that suspension may happen from the
    Claimed/Busy state, it may also happen during the Claimed/Retiring
    state (transition **20**). In this case, when the job continues from
    suspension, it moves back into Claimed/Retiring (transition **16**)
    instead of Claimed/Busy (transition **15**).

Preempting State
""""""""""""""""

:index:`Preempting<single: Preempting; machine state>`
:index:`preempting state`

The Preempting state is less complex than the Claimed state. There are
two activities. Depending on the value of ``WANT_VACATE``, a machine
will be in the Vacating activity (if ``True``) or the Killing activity
(if ``False``).

While in the Preempting state (regardless of activity) the machine
advertises its ``Requirements`` expression as ``False`` to signify that
it is not available for further matches, either because it is about to
transition to the Owner state, or because it has already been matched
with one preempting match, and further preempting matches are disallowed
until the machine has been claimed by the new match.

The main function of the Preempting state is to get rid of the
*condor_starter* associated with the resource. If the *condor_starter*
associated with a given claim exits while the machine is still in the
Vacating activity, then the job successfully completed a graceful
shutdown.  For other jobs, this means the application was given an
opportunity to do a graceful shutdown, by intercepting the soft kill
signal.

If the machine is in the Vacating activity, it keeps evaluating the
``KILL`` expression. As soon as this expression evaluates to TRUE, the
machine enters the Killing activity (transition **21**). If the Vacating
activity lasts for as long as the maximum vacating time, then the
machine also enters the Killing activity. The maximum vacating time is
determined by the configuration variable ``MachineMaxVacateTime``
:index:`MachineMaxVacateTime`. This may be adjusted by the setting
of the job ClassAd attribute ``JobMaxVacateTime``.

When the starter exits, or if there was no starter running when the
machine enters the Preempting state (transition **10**), the other
purpose of the Preempting state is completed: notifying the schedd that
had claimed this machine that the claim is broken.

At this point, the machine enters either the Owner state by transition
**22** (if the job was preempted because the machine owner came back) or
the Claimed/Idle state by transition **23** (if the job was preempted
because a better match was found).

If the machine enters the Killing activity, (because either
``WANT_VACATE`` was ``False`` or the ``KILL`` expression evaluated to
``True``), it attempts to force the *condor_starter* to immediately
kill the underlying HTCondor job. Once the machine has begun to hard
kill the HTCondor job, the *condor_startd* starts a timer, the length
of which is defined by the ``KILLING_TIMEOUT``
:index:`KILLING_TIMEOUT` macro
(:ref:`admin-manual/configuration-macros:condor_startd configuration file
macros`). This macro is defined in seconds and defaults to 30. If this timer
expires and the machine is still in the Killing activity, something has gone
seriously wrong with the *condor_starter* and the startd tries to vacate the job
immediately by sending SIGKILL to all of the *condor_starter* 's
children, and then to the *condor_starter* itself.

Once the *condor_starter* has killed off all the processes associated
with the job and exited, and once the schedd that had claimed the
machine is notified that the claim is broken, the machine will leave the
Preempting/Killing state. If the job was preempted because a better
match was found, the machine will enter Claimed/Idle (transition
**24**). If the preemption was caused by the machine owner (the
``PREEMPT`` expression evaluated to TRUE, *condor_vacate* was used,
etc), the machine will enter the Owner state (transition **25**).

Backfill State
""""""""""""""

:index:`Backfill<single: Backfill; machine state>` :index:`backfill state`

The Backfill state is used whenever the machine is performing low
priority background tasks to keep itself busy. For more information
about backfill support in HTCondor, see the
:ref:`admin-manual/setting-up-special-environments:configuring htcondor for
running backfill jobs` section. This state is only used if the machine has been
configured to enable backfill computation, if a specific backfill manager has
been installed and configured, and if the machine is otherwise idle (not being
used interactively or for regular HTCondor computations). If the machine
meets all these requirements, and the ``START_BACKFILL`` expression
evaluates to TRUE, the machine will move from the Unclaimed/Idle state
to Backfill/Idle (transition **7**).

Once a machine is in Backfill/Idle, it will immediately attempt to spawn
whatever backfill manager it has been configured to use (currently, only
the BOINC client is supported as a backfill manager in HTCondor). Once
the BOINC client is running, the machine will enter Backfill/Busy
(transition **26**) to indicate that it is now performing a backfill
computation.

.. note::
    On multi-core machines, the *condor_startd* will only spawn a
    single instance of the BOINC client, even if multiple slots are
    available to run backfill jobs. Therefore, only the first machine to
    enter Backfill/Idle will cause a copy of the BOINC client to start
    running. If a given slot on a multi-core enters the Backfill state and a
    BOINC client is already running under this *condor_startd*, the slot
    will immediately enter Backfill/Busy without waiting to spawn another
    copy of the BOINC client.

If the BOINC client ever exits on its own (which normally wouldn't
happen), the machine will go back to Backfill/Idle (transition **27**)
where it will immediately attempt to respawn the BOINC client (and
return to Backfill/Busy via transition **26**).

As the BOINC client is running a backfill computation, a number of
events can occur that will drive the machine out of the Backfill state.
The machine can get matched or claimed for an HTCondor job, interactive
users can start using the machine again, the machine might be evicted
with *condor_vacate*, or the *condor_startd* might be shutdown. All of
these events cause the *condor_startd* to kill the BOINC client and all
its descendants, and enter the Backfill/Killing state (transition
**28**).

Once the BOINC client and all its children have exited the system, the
machine will enter the Backfill/Idle state to indicate that the BOINC
client is now gone (transition **29**). As soon as it enters
Backfill/Idle after the BOINC client exits, the machine will go into
another state, depending on what caused the BOINC client to be killed in
the first place.

If the ``EVICT_BACKFILL`` expression evaluates to TRUE while a machine
is in Backfill/Busy, after the BOINC client is gone, the machine will go
back into the Owner/Idle state (transition **30**). The machine will
also return to the Owner/Idle state after the BOINC client exits if
*condor_vacate* was used, or if the *condor_startd* is being shutdown.

When a machine running backfill jobs is matched with a requester that
wants to run an HTCondor job, the machine will either enter the Matched
state, or go directly into Claimed/Idle. As with the case of a machine
in Unclaimed/Idle (described above), the *condor_negotiator* informs
both the *condor_startd* and the *condor_schedd* of the match, and the
exact state transitions at the machine depend on what order the various
entities initiate communication with each other. If the *condor_schedd*
is notified of the match and sends a request to claim the
*condor_startd* before the *condor_negotiator* has a chance to notify
the *condor_startd*, once the BOINC client exits, the machine will
immediately enter Claimed/Idle (transition **31**). Normally, the
notification from the *condor_negotiator* will reach the
*condor_startd* before the *condor_schedd* attempts to claim it. In
this case, once the BOINC client exits, the machine will enter
Matched/Idle (transition **32**).

Drained State
"""""""""""""

:index:`Drained<single: Drained; machine state>` :index:`drained state`

The Drained state is used when the machine is being drained, for example
by *condor_drain* or by the *condor_defrag* daemon, and the slot has
finished running jobs and is no longer willing to run new jobs.

Slots initially enter the Drained/Retiring state. Once all slots have
been drained, the slots transition to the Idle activity (transition
**33**).

If draining is finalized or canceled, the slot transitions to Owner/Idle
(transitions **34** and **35**).

State/Activity Transition Expression Summary
''''''''''''''''''''''''''''''''''''''''''''

:index:`transitions summary<single: transitions summary; machine state>`
:index:`transitions summary<single: transitions summary; machine activity>`
:index:`transitions summary<single: transitions summary; state>`
:index:`transitions summary<single: transitions summary; activity>`

This section is a summary of the information from the previous sections.
It serves as a quick reference.

``START`` :index:`START`
    When TRUE, the machine is willing to spawn a remote HTCondor job.

``RUNBENCHMARKS`` :index:`RUNBENCHMARKS`
    While in the Unclaimed state, the machine will run benchmarks
    whenever TRUE.

``MATCH_TIMEOUT`` :index:`MATCH_TIMEOUT`
    If the machine has been in the Matched state longer than this value,
    it will transition to the Owner state.

``WANT_SUSPEND`` :index:`WANT_SUSPEND`
    If ``True``, the machine evaluates the ``SUSPEND`` expression to see
    if it should transition to the Suspended activity. If any value
    other than ``True``, the machine will look at the ``PREEMPT``
    expression.

``SUSPEND`` :index:`SUSPEND`
    If ``WANT_SUSPEND`` is ``True``, and the machine is in the
    Claimed/Busy state, it enters the Suspended activity if ``SUSPEND``
    is ``True``.

``CONTINUE`` :index:`CONTINUE`
    If the machine is in the Claimed/Suspended state, it enter the Busy
    activity if ``CONTINUE`` is ``True``.

``PREEMPT`` :index:`PREEMPT`
    If the machine is either in the Claimed/Suspended activity, or is in
    the Claimed/Busy activity and ``WANT_SUSPEND`` is FALSE, the machine
    enters the Claimed/Retiring state whenever ``PREEMPT`` is TRUE.

``CLAIM_WORKLIFE`` :index:`CLAIM_WORKLIFE`
    This expression specifies the number of seconds after which a claim
    will stop accepting additional jobs. This configuration macro is
    fully documented here: :ref:`admin-manual/configuration-macros:condor_startd
    configuration file macros`.

``MachineMaxVacateTime`` :index:`MachineMaxVacateTime`
    When the machine enters the Preempting/Vacating state, this
    expression specifies the maximum time in seconds that the
    *condor_startd* will wait for the job to finish. The job may adjust
    the wait time by setting ``JobMaxVacateTime``. If the job's setting
    is less than the machine's, the job's is used. If the job's setting
    is larger than the machine's, the result depends on whether the job
    has any excess retirement time. If the job has more retirement time
    left than the machine's maximum vacate time setting, then retirement
    time will be converted into vacating time, up to the amount of
    ``JobMaxVacateTime``. Once the vacating time expires, the job is
    hard-killed. The ``KILL`` :index:`KILL` expression may be used
    to abort the graceful shutdown of the job at any time.

``MAXJOBRETIREMENTTIME`` :index:`MAXJOBRETIREMENTTIME`
    If the machine is in the Claimed/Retiring state, jobs which have run
    for less than the number of seconds specified by this expression
    will not be hard-killed. The *condor_startd* will wait for the job
    to finish or to exceed this amount of time, whichever comes sooner.
    Time spent in suspension does not count against the job. If the job
    vacating policy grants the job X seconds of vacating time, a
    preempted job will be soft-killed X seconds before the end of its
    retirement time, so that hard-killing of the job will not happen
    until the end of the retirement time if the job does not finish
    shutting down before then. The job may provide its own expression
    for ``MaxJobRetirementTime``, but this can only be used to take less
    than the time granted by the *condor_startd*, never more. For
    convenience, nice_user jobs are submitted
    with a default retirement time of 0, so they will never wait in
    retirement unless the user overrides the default.

    The machine enters the Preempting state with the goal of finishing
    shutting down the job by the end of the retirement time. If the job
    vacating policy grants the job X seconds of vacating time, the
    transition to the Preempting state will happen X seconds before the
    end of the retirement time, so that the hard-killing of the job will
    not happen until the end of the retirement time, if the job does not
    finish shutting down before then.

    This expression is evaluated in the context of the job ClassAd, so
    it may refer to attributes of the current job as well as machine
    attributes.

    By default the *condor_negotiator* will not match jobs to a slot
    with retirement time remaining. This behavior is controlled by
    ``NEGOTIATOR_CONSIDER_EARLY_PREEMPTION``
    :index:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION`.

``WANT_VACATE`` :index:`WANT_VACATE`
    This is checked only when the ``PREEMPT`` expression is ``True`` and
    the machine enters the Preempting state. If ``WANT_VACATE`` is
    ``True``, the machine enters the Vacating activity. If it is
    ``False``, the machine will proceed directly to the Killing
    activity.

``KILL`` :index:`KILL`
    If the machine is in the Preempting/Vacating state, it enters
    Preempting/Killing whenever ``KILL`` is ``True``.

``KILLING_TIMEOUT`` :index:`KILLING_TIMEOUT`
    If the machine is in the Preempting/Killing state for longer than
    ``KILLING_TIMEOUT`` seconds, the *condor_startd* sends a SIGKILL to
    the *condor_starter* and all its children to try to kill the job as
    quickly as possible.

``PERIODIC_CHECKPOINT``
    If the machine is in the Claimed/Busy state and
    ``PERIODIC_CHECKPOINT`` is TRUE, the user's job begins a periodic
    checkpoint.

``RANK`` :index:`RANK`
    If this expression evaluates to a higher number for a pending
    resource request than it does for the current request, the machine
    may preempt the current request (enters the Preempting/Vacating
    state). When the preemption is complete, the machine enters the
    Claimed/Idle state with the new resource request claiming it.

``START_BACKFILL`` :index:`START_BACKFILL`
    When TRUE, if the machine is otherwise idle, it will enter the
    Backfill state and spawn a backfill computation (using BOINC).

``EVICT_BACKFILL`` :index:`EVICT_BACKFILL`
    When TRUE, if the machine is currently running a backfill
    computation, it will kill the BOINC client and return to the
    Owner/Idle state.

:index:`transitions<single: transitions; machine state>`
:index:`transitions<single: transitions; machine activity>`
:index:`transitions<single: transitions; state>` :index:`transitions<single: transitions; activity>`

Examples of Policy Configuration
''''''''''''''''''''''''''''''''

This section describes various policy configurations, including the
default policy. :index:`default with HTCondor<single: default with HTCondor; policy>`
:index:`default policy<single: default policy; HTCondor>`

**Default Policy**

These settings are the default as shipped with HTCondor. They have been
used for many years with no problems. The vanilla expressions are
identical to the regular ones. (They are not listed here. If not
defined, the standard expressions are used for vanilla jobs as well).

The following are macros to help write the expressions clearly.

``StateTimer``
    Amount of time in seconds in the current state.

``ActivityTimer``
    Amount of time in seconds in the current activity.

``ActivationTimer``
    Amount of time in seconds that the job has been running on this
    machine.

``LastCkpt``
    Amount of time since the last periodic checkpoint.

``NonCondorLoadAvg``
    The difference between the system load and the HTCondor load (the
    load generated by everything but HTCondor).

``BackgroundLoad``
    Amount of background load permitted on the machine and still start
    an HTCondor job.

``HighLoad``
    If the ``$(NonCondorLoadAvg)`` goes over this, the CPU is considered
    too busy, and eviction of the HTCondor job should start.

``StartIdleTime``
    Amount of time the keyboard must to be idle before HTCondor will
    start a job.

``ContinueIdleTime``
    Amount of time the keyboard must to be idle before resumption of a
    suspended job.

``MaxSuspendTime``
    Amount of time a job may be suspended before more drastic measures
    are taken.

``KeyboardBusy``
    A boolean expression that evaluates to TRUE when the keyboard is
    being used.

``CPUIdle``
    A boolean expression that evaluates to TRUE when the CPU is idle.

``CPUBusy``
    A boolean expression that evaluates to TRUE when the CPU is busy.

``MachineBusy``
    The CPU or the Keyboard is busy.

``CPUIsBusy``
    A boolean value set to the same value as ``CPUBusy``.

``CPUBusyTime``
    The value 0 if ``CPUBusy`` is False; the time in seconds since
    ``CPUBusy`` became True.

These variable definitions exist in the example configuration file in
order to help write legible expressions. They are not required, and
perhaps will go unused by many configurations.

.. code-block:: condor-config

    ##  These macros are here to help write legible expressions:
    MINUTE          = 60
    HOUR            = (60 * $(MINUTE))
    StateTimer      = (time() - EnteredCurrentState)
    ActivityTimer   = (time() - EnteredCurrentActivity)
    ActivationTimer = (time() - JobStart)
    LastCkpt        = (time() - LastPeriodicCheckpoint)

    NonCondorLoadAvg        = (LoadAvg - CondorLoadAvg)
    BackgroundLoad          = 0.3
    HighLoad                = 0.5
    StartIdleTime           = 15 * $(MINUTE)
    ContinueIdleTime        = 5 * $(MINUTE)
    MaxSuspendTime          = 10 * $(MINUTE)

    KeyboardBusy            = KeyboardIdle < $(MINUTE)
    ConsoleBusy             = (ConsoleIdle  < $(MINUTE))
    CPUIdle                = $(NonCondorLoadAvg) <= $(BackgroundLoad)
    CPUBusy                = $(NonCondorLoadAvg) >= $(HighLoad)
    KeyboardNotBusy         = ($(KeyboardBusy) == False)
    MachineBusy             = ($(CPUBusy) || $(KeyboardBusy)

Preemption is disabled as a default. Always desire to start jobs.

.. code-block:: condor-config

    WANT_SUSPEND         = False
    WANT_VACATE          = False
    START                = True
    SUSPEND              = False
    CONTINUE             = True
    PREEMPT              = False
    # Kill jobs that take too long leaving gracefully.
    MachineMaxVacateTime = 10 * $(MINUTE)
    KILL                 = False

Periodic checkpointing specifies that for jobs smaller than 60 Mbytes,
take a periodic checkpoint every 6 hours. For larger jobs, only take a
checkpoint every 12 hours.

.. code-block:: condor-config

    PERIODIC_CHECKPOINT     = ( (ImageSize < 60000) && \
                                ($(LastCkpt) > (6 * $(HOUR))) ) || \
                              ( $(LastCkpt) > (12 * $(HOUR)) )

:index:`at UW-Madison<single: at UW-Madison; policy>`

At UW-Madison, we have a fast network. We simplify our expression
considerably to

.. code-block:: condor-config

    PERIODIC_CHECKPOINT     = $(LastCkpt) > (3 * $(HOUR))

:index:`test job<single: test job; policy>`

**Test-job Policy Example**

This example shows how the default macros can be used to set up a
machine for running test jobs from a specific user. Suppose we want the
machine to behave normally, except if user coltrane submits a job. In
that case, we want that job to start regardless of what is happening on
the machine. We do not want the job suspended, vacated or killed. This
is reasonable if we know coltrane is submitting very short running
programs for testing purposes. The jobs should be executed right away.
This works with any machine (or the whole pool, for that matter) by
adding the following 5 expressions to the existing configuration:

.. code-block:: condor-config

      START      = ($(START)) || Owner == "coltrane"
      SUSPEND    = ($(SUSPEND)) && Owner != "coltrane"
      CONTINUE   = $(CONTINUE)
      PREEMPT    = ($(PREEMPT)) && Owner != "coltrane"
      KILL       = $(KILL)

Notice that there is nothing special in either the ``CONTINUE`` or
``KILL`` expressions. If Coltrane's jobs never suspend, they never look
at ``CONTINUE``. Similarly, if they never preempt, they never look at
``KILL``. :index:`time of day<single: time of day; policy>`

**Time of Day Policy**

HTCondor can be configured to only run jobs at certain times of the day.
In general, we discourage configuring a system like this, since there
will often be lots of good cycles on machines, even when their owners
say "I'm always using my machine during the day." However, if you submit
mostly vanilla jobs or other jobs that cannot produce checkpoints, it
might be a good idea to only allow the jobs to run when you know the
machines will be idle and when they will not be interrupted.

To configure this kind of policy, use the ``ClockMin`` and ``ClockDay``
attributes. These are special attributes which are automatically
inserted by the *condor_startd* into its ClassAd, so you can always
reference them in your policy expressions. ``ClockMin`` defines the
number of minutes that have passed since midnight. For example, 8:00am
is 8 hours after midnight, or 8 \* 60 minutes, or 480. 5:00pm is 17
hours after midnight, or 17 \* 60, or 1020. ``ClockDay`` defines the day
of the week, Sunday = 0, Monday = 1, and so on.

To make the policy expressions easy to read, we recommend using macros
to define the time periods when you want jobs to run or not run. For
example, assume regular work hours at your site are from 8:00am until
5:00pm, Monday through Friday:

.. code-block:: condor-config

    WorkHours = ( (ClockMin >= 480 && ClockMin < 1020) && \
                  (ClockDay > 0 && ClockDay < 6) )
    AfterHours = ( (ClockMin < 480 || ClockMin >= 1020) || \
                   (ClockDay == 0 || ClockDay == 6) )

Of course, you can fine-tune these settings by changing the definition
of ``AfterHours`` :index:`AfterHours` and ``WorkHours``
:index:`WorkHours` for your site.

To force HTCondor jobs to stay off of your machines during work hours:

.. code-block:: condor-config

    # Only start jobs after hours.
    START = $(AfterHours)

    # Consider the machine busy during work hours, or if the keyboard or
    # CPU are busy.
    MachineBusy = ( $(WorkHours) || $(CPUBusy) || $(KeyboardBusy) )

This ``MachineBusy`` macro is convenient if other than the default
``SUSPEND`` and ``PREEMPT`` expressions are used.
:index:`desktop/non-desktop<single: desktop/non-desktop; policy>`
:index:`desktop/non-desktop<single: desktop/non-desktop; preemption>`

**Desktop/Non-Desktop Policy**

Suppose you have two classes of machines in your pool: desktop machines
and dedicated cluster machines. In this case, you might not want
keyboard activity to have any effect on the dedicated machines. For
example, when you log into these machines to debug some problem, you
probably do not want a running job to suddenly be killed. Desktop
machines, on the other hand, should do whatever is necessary to remain
responsive to the user.

There are many ways to achieve the desired behavior. One way is to make
a standard desktop policy and a standard non-desktop policy and to copy
the desired one into the local configuration file for each machine.
Another way is to define one standard policy (in the global
configuration file) with a simple toggle that can be set in the local
configuration file. The following example illustrates the latter
approach.

For ease of use, an entire policy is included in this example. Some of
the expressions are just the usual default settings.

.. code-block:: condor-config

    # If "IsDesktop" is configured, make it an attribute of the machine ClassAd.
    STARTD_ATTRS = IsDesktop

    # Only consider starting jobs if:
    # 1) the load average is low enough OR the machine is currently
    #    running an HTCondor job
    # 2) AND the user is not active (if a desktop)
    START = ( ($(CPUIdle) || (State != "Unclaimed" && State != "Owner")) \
              && (IsDesktop =!= True || (KeyboardIdle > $(StartIdleTime))) )

    # Suspend (instead of vacating/killing) for the following cases:
    WANT_SUSPEND = ( $(SmallJob) || $(JustCpu) \
                     || $(IsVanilla) )

    # When preempting, vacate (instead of killing) in the following cases:
    WANT_VACATE  = ( $(ActivationTimer) > 10 * $(MINUTE) \
                     || $(IsVanilla) )

    # Suspend jobs if:
    # 1) The CPU has been busy for more than 2 minutes, AND
    # 2) the job has been running for more than 90 seconds
    # 3) OR suspend if this is a desktop and the user is active
    SUSPEND = ( ((CpuBusyTime > 2 * $(MINUTE)) && ($(ActivationTimer) > 90)) \
                || ( IsDesktop =?= True && $(KeyboardBusy) ) )

    # Continue jobs if:
    # 1) the CPU is idle, AND
    # 2) we've been suspended more than 5 minutes AND
    # 3) the keyboard has been idle for long enough (if this is a desktop)
    CONTINUE = ( $(CPUIdle) && ($(ActivityTimer) > 300) \
                 && (IsDesktop =!= True || (KeyboardIdle > $(ContinueIdleTime))) )

    # Preempt jobs if:
    # 1) The job is suspended and has been suspended longer than we want
    # 2) OR, we don't want to suspend this job, but the conditions to
    #    suspend jobs have been met (someone is using the machine)
    PREEMPT = ( ((Activity == "Suspended") && \
                ($(ActivityTimer) > $(MaxSuspendTime))) \
               || (SUSPEND && (WANT_SUSPEND == False)) )

    # Replace 0 in the following expression with whatever amount of
    # retirement time you want dedicated machines to provide.  The other part
    # of the expression forces the whole expression to 0 on desktop
    # machines.
    MAXJOBRETIREMENTTIME = (IsDesktop =!= True) * 0

    # Kill jobs if they have taken too long to vacate gracefully
    MachineMaxVacateTime = 10 * $(MINUTE)
    KILL = False

With this policy in the global configuration, the local configuration
files for desktops can be easily configured with the following line:

.. code-block:: condor-config

    IsDesktop = True

In all other cases, the default policy described above will ignore
keyboard activity. :index:`disabling preemption<single: disabling preemption; policy>`
:index:`enabling preemption<single: enabling preemption; policy>`
:index:`disabling and enabling<single: disabling and enabling; preemption>`

**Disabling and Enabling Preemption**

Preemption causes a running job to be suspended or killed, such that
another job can run. As of HTCondor version 8.1.5, preemption is
disabled by the default configuration. Previous versions of HTCondor had
configuration that enabled preemption. Upon upgrade, the previous
behavior will continue, if the previous configuration files are used.
New configuration file examples disable preemption, but contain
directions for enabling preemption.
:index:`suspending jobs instead of evicting them<single: suspending jobs instead of evicting them; policy>`

**Job Suspension**

As new jobs are submitted that receive a higher priority than currently
executing jobs, the executing jobs may be preempted. If the preempted
jobs are not capable of writing checkpoints, they lose whatever forward
progress they have made, and are sent back to the job queue to await
starting over again as another machine becomes available. An alternative
to this is to use suspension to freeze the job while some other task
runs, and then unfreeze it so that it can continue on from where it left
off. This does not require any special handling in the job, unlike most
strategies that take checkpoints. However, it does require a special
configuration of HTCondor. This example implements a policy that allows
the job to decide whether it should be evicted or suspended. The jobs
announce their choice through the use of the invented job ClassAd
attribute ``IsSuspendableJob``, that is also utilized in the
configuration.

The implementation of this policy utilizes two categories of slots,
identified as suspendable or nonsuspendable. A job identifies which
category of slot it wishes to run on. This affects two aspects of the
policy:

-  Of two jobs that might run on a slot, which job is chosen. The four
   cases that may occur depend on whether the currently running job
   identifies itself as suspendable or nonsuspendable, and whether the
   potentially running job identifies itself as suspendable or
   nonsuspendable.

   #. If the currently running job is one that identifies itself as
      suspendable, and the potentially running job identifies itself as
      nonsuspendable, the currently running job is suspended, in favor
      of running the nonsuspendable one. This occurs independent of the
      user priority of the two jobs.
   #. If both the currently running job and the potentially running job
      identify themselves as suspendable, then the relative priorities
      of the users and the preemption policy determines whether the new
      job will replace the existing job.
   #. If both the currently running job and the potentially running job
      identify themselves as nonsuspendable, then the relative
      priorities of the users and the preemption policy determines
      whether the new job will replace the existing job.
   #. If the currently running job is one that identifies itself as
      nonsuspendable, and the potentially running job identifies itself
      as suspendable, the currently running job continues running.

-  What happens to a currently running job that is preempted. A job that
   identifies itself as suspendable will be suspended, which means it is
   frozen in place, and will later be unfrozen when the preempting job
   is finished. A job that identifies itself as nonsuspendable is
   evicted, which means it writes a checkpoint, when possible, and then
   is killed. The job will return to the idle state in the job queue,
   and it can try to run again in the future.

:index:`eval()<single: eval(); ClassAd functions>`

.. code-block:: condor-config

    # Lie to HTCondor, to achieve 2 slots for each real slot
    NUM_CPUS = $(DETECTED_CORES)*2
    # There is no good way to tell HTCondor that the two slots should be treated
    # as though they share the same real memory, so lie about how much
    # memory we have.
    MEMORY = $(DETECTED_MEMORY)*2

    # Slots 1 through DETECTED_CORES are nonsuspendable and the rest are
    # suspendable
    IsSuspendableSlot = SlotID > $(DETECTED_CORES)

    # If I am a suspendable slot, my corresponding nonsuspendable slot is
    # my SlotID plus $(DETECTED_CORES)
    NonSuspendableSlotState = eval(strcat("slot",SlotID-$(DETECTED_CORES),"_State")

    # The above expression looks at slotX_State, so we need to add
    # State to the list of slot attributes to advertise.
    STARTD_SLOT_ATTRS = $(STARTD_SLOT_ATTRS) State

    # For convenience, advertise these expressions in the machine ad.
    STARTD_ATTRS = $(STARTD_ATTRS) IsSuspendableSlot NonSuspendableSlotState

    MyNonSuspendableSlotIsIdle = \
      (NonSuspendableSlotState =!= "Claimed" && NonSuspendableSlotState =!= "Preempting")

    # NonSuspendable slots are always willing to start jobs.
    # Suspendable slots are only willing to start if the NonSuspendable slot is idle.
    START = \
      IsSuspendableSlot!=True && IsSuspendableJob=!=True || \
      IsSuspendableSlot && IsSuspendableJob==True && $(MyNonSuspendableSlotIsIdle)

    # Suspend the suspendable slot if the other slot is busy.
    SUSPEND = \
      IsSuspendableSlot && $(MyNonSuspendableSlotIsIdle)!=True

    WANT_SUSPEND = $(SUSPEND)

    CONTINUE = ($(SUSPEND)) != True

Note that in this example, the job ClassAd attribute
``IsSuspendableJob`` has no special meaning to HTCondor. It is an
invented name chosen for this example. To take advantage of the policy,
a job that wishes to be suspended must submit the job so that this
attribute is defined. The following line should be placed in the job's
submit description file:

.. code-block:: condor-submit

    +IsSuspendableJob = True

:index:`utilizing interactive jobs<single: utilizing interactive jobs; policy>`

 Configuration for Interactive Jobs

Policy may be set based on whether a job is an interactive one or not.
Each interactive job has the job ClassAd attribute

.. code-block:: condor-classad

    InteractiveJob = True

and this may be used to identify interactive jobs, distinguishing them
from all other jobs.

As an example, presume that slot 1 prefers interactive jobs. Set the
machine's ``RANK`` to show the preference:

.. code-block:: condor-config

    RANK = ( (MY.SlotID == 1) && (TARGET.InteractiveJob =?= True) )

Or, if slot 1 should be reserved for interactive jobs:

.. code-block:: condor-config

    START = ( (MY.SlotID == 1) && (TARGET.InteractiveJob =?= True) )

Multi-Core Machine Terminology
''''''''''''''''''''''''''''''

:index:`configuration<single: configuration; SMP machines>`
:index:`configuration<single: configuration; multi-core machines>`

Machines with more than one CPU or core may be configured to run more
than one job at a time. As always, owners of the resources have great
flexibility in defining the policy under which multiple jobs may run,
suspend, vacate, etc.

Multi-core machines are represented to the HTCondor system as shared
resources broken up into individual slots. Each slot can be matched and
claimed by users for jobs. Each slot is represented by an individual
machine ClassAd. In this way, each multi-core machine will appear to the
HTCondor system as a collection of separate slots. As an example, a
multi-core machine named ``vulture.cs.wisc.edu`` would appear to
HTCondor as the multiple machines, named ``slot1@vulture.cs.wisc.edu``,
``slot2@vulture.cs.wisc.edu``, ``slot3@vulture.cs.wisc.edu``, and so on.
:index:`dividing resources in multi-core machines`

The way that the *condor_startd* breaks up the shared system resources
into the different slots is configurable. All shared system resources,
such as RAM, disk space, and swap space, can be divided evenly among all
the slots, with each slot assigned one core. Alternatively, slot types
are defined by configuration, so that resources can be unevenly divided.
Regardless of the scheme used, it is important to remember that the goal
is to create a representative slot ClassAd, to be used for matchmaking
with jobs.

HTCondor does not directly enforce slot shared resource allocations, and
jobs are free to oversubscribe to shared resources. Consider an example
where two slots are each defined with 50% of available RAM. The
resultant ClassAd for each slot will advertise one half the available
RAM. Users may submit jobs with RAM requirements that match these slots.
However, jobs run on either slot are free to consume more than 50% of
available RAM. HTCondor will not directly enforce a RAM utilization
limit on either slot. If a shared resource enforcement capability is
needed, it is possible to write a policy that will evict a job that
oversubscribes to shared resources, as described in
:ref:`admin-manual/policy-configuration:*condor_startd* policy configuration`.

Dividing System Resources in Multi-core Machines
''''''''''''''''''''''''''''''''''''''''''''''''

Within a machine the shared system resources of cores, RAM, swap space
and disk space will be divided for use by the slots. There are two main
ways to go about dividing the resources of a multi-core machine:

Evenly divide all resources.
    By default, the *condor_startd* will automatically divide the
    machine into slots, placing one core in each slot, and evenly
    dividing all shared resources among the slots. The only
    specification may be how many slots are reported at a time. By
    default, all slots are reported to HTCondor.

    How many slots are reported at a time is accomplished by setting the
    configuration variable ``NUM_SLOTS`` :index:`NUM_SLOTS` to the
    integer number of slots desired. If variable ``NUM_SLOTS`` is not
    defined, it defaults to the number of cores within the machine.
    Variable ``NUM_SLOTS`` may not be used to make HTCondor advertise
    more slots than there are cores on the machine. The number of cores
    is defined by ``NUM_CPUS`` :index:`NUM_CPUS`.

Define slot types.
    Instead of an even division of resources per slot, the machine may
    have definitions of slot types, where each type is provided with a
    fraction of shared system resources. Given the slot type definition,
    control how many of each type are reported at any given time with
    further configuration.

    Configuration variables define the slot types, as well as variables
    that list how much of each system resource goes to each slot type.

    Configuration variable ``SLOT_TYPE_<N>``
    :index:`SLOT_TYPE_<N>`, where <N> is an integer (for example,
    ``SLOT_TYPE_1``) defines the slot type. Note that there may be
    multiple slots of each type. The number of slots created of a given
    type is configured with ``NUM_SLOTS_TYPE_<N>``.

    The type can be defined by:

    -  A simple fraction, such as 1/4
    -  A simple percentage, such as 25%
    -  A comma-separated list of attributes, with a percentage,
       fraction, numerical value, or ``auto`` for each one.
    -  A comma-separated list that includes a blanket value that serves
       as a default for any resources not explicitly specified in the
       list.

    A simple fraction or percentage describes the allocation of the
    total system resources, including the number of CPUS or cores. A
    comma separated list allows a fine tuning of the amounts for
    specific resources.

    The number of CPUs and the total amount of RAM in the machine do not
    change over time. For these attributes, specify either absolute
    values or percentages of the total available amount (or ``auto``).
    For example, in a machine with 128 Mbytes of RAM, all the following
    definitions result in the same allocation amount.

    .. code-block:: condor-config

        SLOT_TYPE_1 = mem=64

        SLOT_TYPE_1 = mem=1/2

        SLOT_TYPE_1 = mem=50%

        SLOT_TYPE_1 = mem=auto

    Amounts of disk space and swap space are dynamic, as they change
    over time. For these, specify a percentage or fraction of the total
    value that is allocated to each slot, instead of specifying absolute
    values. As the total values of these resources change on the
    machine, each slot will take its fraction of the total and report
    that as its available amount.

    The disk space allocated to each slot is taken from the disk
    partition containing the slot's ``EXECUTE`` or ``SLOT<N>_EXECUTE``
    :index:`SLOT<N>_EXECUTE` directory. If every slot is in a
    different partition, then each one may be defined with up to
    100% for its disk share. If some slots are in the same partition,
    then their total is not allowed to exceed 100%.

    The four predefined attribute names are case insensitive when
    defining slot types. The first letter of the attribute name
    distinguishes between these attributes. The four attributes, with
    several examples of acceptable names for each:

    -  Cpus, C, c, cpu
    -  ram, RAM, MEMORY, memory, Mem, R, r, M, m
    -  disk, Disk, D, d
    -  swap, SWAP, S, s, VirtualMemory, V, v

    As an example, consider a machine with 4 cores and 256 Mbytes of
    RAM. Here are valid example slot type definitions. Types 1-3 are all
    equivalent to each other, as are types 4-6. Note that in a real
    configuration, all of these slot types would not be used together,
    because they add up to more than 100% of the various system
    resources. This configuration example also omits definitions of
    ``NUM_SLOTS_TYPE_<N>``, to define the number of each slot type.

    .. code-block:: condor-config

          SLOT_TYPE_1 = cpus=2, ram=128, swap=25%, disk=1/2

          SLOT_TYPE_2 = cpus=1/2, memory=128, virt=25%, disk=50%

          SLOT_TYPE_3 = c=1/2, m=50%, v=1/4, disk=1/2

          SLOT_TYPE_4 = c=25%, m=64, v=1/4, d=25%

          SLOT_TYPE_5 = 25%

          SLOT_TYPE_6 = 1/4

    The default value for each resource share is ``auto``. The share may
    also be explicitly set to ``auto``. All slots with the value
    ``auto`` for a given type of resource will evenly divide whatever
    remains, after subtracting out explicitly allocated resources given
    in other slot definitions. For example, if one slot is defined to
    use 10% of the memory and the rest define it as ``auto`` (or leave
    it undefined), then the rest of the slots will evenly divide 90% of
    the memory between themselves.

    In both of the following examples, the disk share is set to
    ``auto``, number of cores is 1, and everything else is 50%:

    .. code-block:: condor-config

        SLOT_TYPE_1 = cpus=1, ram=1/2, swap=50%

        SLOT_TYPE_1 = cpus=1, disk=auto, 50%

    Note that it is possible to set the configuration variables such
    that they specify an impossible configuration. If this occurs, the
    *condor_startd* daemon fails after writing a message to its log
    attempting to indicate the configuration requirements that it could
    not implement.

    In addition to the standard resources of CPUs, memory, disk, and
    swap, the administrator may also define custom resources on a
    localized per-machine basis.

    The resource names and quantities of available resources are defined
    using configuration variables of the form
    ``MACHINE_RESOURCE_<name>`` :index:`MACHINE_RESOURCE_<name>`,
    as shown in this example:

    .. code-block:: condor-config

        MACHINE_RESOURCE_gpu = 16
        MACHINE_RESOURCE_actuator = 8

    If the configuration uses the optional configuration variable
    ``MACHINE_RESOURCE_NAMES`` :index:`MACHINE_RESOURCE_NAMES` to
    enable and disable local machine resources, also add the resource
    names to this variable. For example:

    .. code-block:: condor-config

        if defined MACHINE_RESOURCE_NAMES
          MACHINE_RESOURCE_NAMES = $(MACHINE_RESOURCE_NAMES) gpu actuator
        endif

    Local machine resource names defined in this way may now be used in
    conjunction with ``SLOT_TYPE_<N>`` :index:`SLOT_TYPE_<N>`,
    using all the same syntax described earlier in this section. The
    following example demonstrates the definition of static and
    partitionable slot types with local machine resources:

    .. code-block:: condor-config

        # declare one partitionable slot with half of the GPUs, 6 actuators, and
        # 50% of all other resources:
        SLOT_TYPE_1 = gpu=50%,actuator=6,50%
        SLOT_TYPE_1_PARTITIONABLE = TRUE
        NUM_SLOTS_TYPE_1 = 1

        # declare two static slots, each with 25% of the GPUs, 1 actuator, and
        # 25% of all other resources:
        SLOT_TYPE_2 = gpu=25%,actuator=1,25%
        SLOT_TYPE_2_PARTITIONABLE = FALSE
        NUM_SLOTS_TYPE_2 = 2

    A job may request these local machine resources using the syntax
    **request_<name>** :index:`request_<name><single: request_<name>; submit commands>`,
    as described in :ref:`admin-manual/policy-configuration:*condor_startd*
    policy configuration`. This example shows a portion of a submit description
    file that requests GPUs and an actuator:

    .. code-block:: condor-submit

        universe = vanilla

        # request two GPUs and one actuator:
        request_gpu = 2
        request_actuator = 1

        queue

    The slot ClassAd will represent each local machine resource with the
    following attributes:

        ``Total<name>``: the total quantity of the resource identified
        by ``<name>``
        ``Detected<name>``: the quantity detected of the resource
        identified by ``<name>``; this attribute is currently equivalent
        to ``Total<name>``
        ``TotalSlot<name>``: the quantity of the resource identified by
        ``<name>`` allocated to this slot
        ``<name>``: the amount of the resource identified by ``<name>``
        available to be used on this slot

    From the example given, the ``gpu`` resource would be represented by
    the ClassAd attributes ``TotalGpu``, ``DetectedGpu``,
    ``TotalSlotGpu``, and ``Gpu``. In the job ClassAd, the amount of the
    requested machine resource appears in a job ClassAd attribute named
    ``Request<name>``. For this example, the two attributes will be
    ``RequestGpu`` and ``RequestActuator``.

    The number of each type being reported can be changed at run time,
    by issuing a reconfiguration command to the *condor_startd* daemon
    (sending a SIGHUP or using *condor_reconfig*). However, the
    definitions for the types themselves cannot be changed with
    reconfiguration. To change any slot type definitions, use
    *condor_restart*

    .. code-block:: console

        $ condor_restart -startd

    for that change to take effect.

Configuration Specific to Multi-core Machines
'''''''''''''''''''''''''''''''''''''''''''''

:index:`SMP machines<single: SMP machines; configuration>`
:index:`multi-core machines<single: multi-core machines; configuration>`

Each slot within a multi-core machine is treated as an independent
machine, each with its own view of its state as represented by the
machine ClassAd attribute ``State``. The policy expressions for the
multi-core machine as a whole are propagated from the *condor_startd*
to the slot's machine ClassAd. This policy may consider a slot state(s)
in its expressions. This makes some policies easy to set, but it makes
other policies difficult or impossible to set.

An easy policy to set configures how many of the slots notice console or
tty activity on the multi-core machine as a whole. Slots that are not
configured to notice any activity will report ``ConsoleIdle`` and
``KeyboardIdle`` times from when the *condor_startd* daemon was
started, plus a configurable number of seconds. A multi-core machine
with the default policy settings can add the keyboard and console to be
noticed by only one slot. Assuming a reasonable load average, only the
one slot will suspend or vacate its job when the owner starts typing at
their machine again. The rest of the slots could be matched with jobs
and continue running them, even while the user was interactively using
the machine. If the default policy is used, all slots notice tty and
console activity and currently running jobs would suspend.

This example policy is controlled with the following configuration
variables.

-  ``SLOTS_CONNECTED_TO_CONSOLE``
   :index:`SLOTS_CONNECTED_TO_CONSOLE`, with definition at
   the :ref:`admin-manual/configuration-macros:condor_startd configuration file
   macros` section

-  ``SLOTS_CONNECTED_TO_KEYBOARD``
   :index:`SLOTS_CONNECTED_TO_KEYBOARD`, with definition at
   the :ref:`admin-manual/configuration-macros:condor_startd configuration file
   macros` section

-  ``DISCONNECTED_KEYBOARD_IDLE_BOOST``
   :index:`DISCONNECTED_KEYBOARD_IDLE_BOOST`, with definition at
   the :ref:`admin-manual/configuration-macros:condor_startd configuration file
   macros` section

Each slot has its own machine ClassAd. Yet, the policy expressions for
the multi-core machine are propagated and inherited from configuration
of the *condor_startd*. Therefore, the policy expressions for each slot
are the same. This makes the implementation of certain types of policies
impossible, because while evaluating the state of one slot within the
multi-core machine, the state of other slots are not available.
Decisions for one slot cannot be based on what other slots are doing.

Specifically, the evaluation of a slot policy expression works in the
following way.

#. The configuration file specifies policy expressions that are shared
   by all of the slots on the machine.
#. Each slot reads the configuration file and sets up its own machine
   ClassAd.
#. Each slot is now separate from the others. It has a different ClassAd
   attribute ``State``, a different machine ClassAd, and if there is a
   job running, a separate job ClassAd. Each slot periodically evaluates
   the policy expressions, changing its own state as necessary. This
   occurs independently of the other slots on the machine. So, if the
   *condor_startd* daemon is evaluating a policy expression on a
   specific slot, and the policy expression refers to ``ProcID``,
   ``Owner``, or any attribute from a job ClassAd, it always refers to
   the ClassAd of the job running on the specific slot.

To set a different policy for the slots within a machine, incorporate
the slot-specific machine ClassAd attribute ``SlotID``. A ``SUSPEND``
policy that is different for each of the two slots will be of the form

.. code-block:: condor-config

    SUSPEND = ( (SlotID == 1) && (PolicyForSlot1) ) || \
              ( (SlotID == 2) && (PolicyForSlot2) )

where (PolicyForSlot1) and (PolicyForSlot2) are the desired expressions
for each slot.

Load Average for Multi-core Machines
''''''''''''''''''''''''''''''''''''

:index:`CondorLoadAvg<single: CondorLoadAvg; ClassAd machine attribute>`
:index:`LoadAvg<single: LoadAvg; ClassAd machine attribute>`
:index:`TotalCondorLoadAvg<single: TotalCondorLoadAvg; ClassAd machine attribute>`
:index:`TotalLoadAvg<single: TotalLoadAvg; ClassAd machine attribute>`

Most operating systems define the load average for a multi-core machine
as the total load on all cores. For example, a 4-core machine with 3
CPU-bound processes running at the same time will have a load of 3.0. In
HTCondor, we maintain this view of the total load average and publish it
in all resource ClassAds as ``TotalLoadAvg``.

HTCondor also provides a per-core load average for multi-core machines.
This nicely represents the model that each node on a multi-core machine
is a slot, separate from the other nodes. All of the default,
single-core policy expressions can be used directly on multi-core
machines, without modification, since the ``LoadAvg`` and
``CondorLoadAvg`` attributes are the per-slot versions, not the total,
multi-core wide versions.

The per-core load average on multi-core machines is an HTCondor
invention. No system call exists to ask the operating system for this
value. HTCondor already computes the load average generated by HTCondor
on each slot. It does this by close monitoring of all processes spawned
by any of the HTCondor daemons, even ones that are orphaned and then
inherited by *init*. This HTCondor load average per slot is reported as
the attribute ``CondorLoadAvg`` in all resource ClassAds, and the total
HTCondor load average for the entire machine is reported as
``TotalCondorLoadAvg``. The total, system-wide load average for the
entire machine is reported as ``TotalLoadAvg``. Basically, HTCondor
walks through all the slots and assigns out portions of the total load
average to each one. First, HTCondor assigns the known HTCondor load
average to each node that is generating load. If there is any load
average left in the total system load, it is considered an owner load.
Any slots HTCondor believes are in the Owner state, such as ones that
have keyboard activity, are the first to get assigned this owner load.
HTCondor hands out owner load in increments of at most 1.0, so generally
speaking, no slot has a load average above 1.0. If HTCondor runs out of
total load average before it runs out of slots, all the remaining
machines believe that they have no load average at all. If, instead,
HTCondor runs out of slots and it still has owner load remaining,
HTCondor starts assigning that load to HTCondor nodes as well, giving
individual nodes with a load average higher than 1.0.

Debug Logging in the Multi-Core *condor_startd* Daemon
'''''''''''''''''''''''''''''''''''''''''''''''''''''''

This section describes how the *condor_startd* daemon handles its
debugging messages for multi-core machines. In general, a given log
message will either be something that is machine-wide, such as reporting
the total system load average, or it will be specific to a given slot.
Any log entries specific to a slot have an extra word printed out in the
entry with the slot number. So, for example, here's the output about
system resources that are being gathered (with ``D_FULLDEBUG`` and
``D_LOAD`` turned on) on a 2-core machine with no HTCondor activity, and
the keyboard connected to both slots:

.. code-block:: text

    11/25 18:15 Swap space: 131064
    11/25 18:15 number of Kbytes available for (/home/condor/execute): 1345063
    11/25 18:15 Looking up RESERVED_DISK parameter
    11/25 18:15 Reserving 5120 Kbytes for file system
    11/25 18:15 Disk space: 1339943
    11/25 18:15 Load avg: 0.340000 0.800000 1.170000
    11/25 18:15 Idle Time: user= 0 , console= 4 seconds
    11/25 18:15 SystemLoad: 0.340   TotalCondorLoad: 0.000  TotalOwnerLoad: 0.340
    11/25 18:15 slot1: Idle time: Keyboard: 0        Console: 4
    11/25 18:15 slot1: SystemLoad: 0.340  CondorLoad: 0.000  OwnerLoad: 0.340
    11/25 18:15 slot2: Idle time: Keyboard: 0        Console: 4
    11/25 18:15 slot2: SystemLoad: 0.000  CondorLoad: 0.000  OwnerLoad: 0.000
    11/25 18:15 slot1: State: Owner           Activity: Idle
    11/25 18:15 slot2: State: Owner           Activity: Idle

If, on the other hand, this machine only had one slot connected to the
keyboard and console, and the other slot was running a job, it might
look something like this:

.. code-block:: text

    11/25 18:19 Load avg: 1.250000 0.910000 1.090000
    11/25 18:19 Idle Time: user= 0 , console= 0 seconds
    11/25 18:19 SystemLoad: 1.250   TotalCondorLoad: 0.996  TotalOwnerLoad: 0.254
    11/25 18:19 slot1: Idle time: Keyboard: 0        Console: 0
    11/25 18:19 slot1: SystemLoad: 0.254  CondorLoad: 0.000  OwnerLoad: 0.254
    11/25 18:19 slot2: Idle time: Keyboard: 1496     Console: 1496
    11/25 18:19 slot2: SystemLoad: 0.996  CondorLoad: 0.996  OwnerLoad: 0.000
    11/25 18:19 slot1: State: Owner           Activity: Idle
    11/25 18:19 slot2: State: Claimed         Activity: Busy

Shared system resources are printed without the header, such as total
swap space, and slot-specific messages, such as the load average or
state of each slot, get the slot number appended.

Configuring GPUs
''''''''''''''''

:index:`configuration<single: configuration; GPUs>`
:index:`to use GPUs<single: to use GPUs; configuration>`

HTCondor supports incorporating GPU resources and making them available
for jobs. First, GPUs must be detected as available resources. Then,
machine ClassAd attributes advertise this availability. Both detection
and advertisement are accomplished by having this configuration for each
execute machine that has GPUs:

.. code-block:: text

      use feature : GPUs

Use of this configuration templdate invokes the *condor_gpu_discovery*
tool to create a custom resource, with a custom resource name of
``GPUs``, and it generates the ClassAd attributes needed to advertise
the GPUs. *condor_gpu_discovery* is invoked in a mode that discovers
and advertises both CUDA and OpenCL GPUs.

This configuration template refers to macro ``GPU_DISCOVERY_EXTRA``,
which can be used to define additional command line arguments for the
*condor_gpu_discovery* tool. For example, setting

.. code-block:: text

      use feature : GPUs
      GPU_DISCOVERY_EXTRA = -extra

causes the *condor_gpu_discovery* tool to output more attributes that
describe the detected GPUs on the machine.

Configuring STARTD_ATTRS on a per-slot basis
'''''''''''''''''''''''''''''''''''''''''''''

The ``STARTD_ATTRS`` :index:`STARTD_ATTRS`  settings can be configured on a per-slot basis. The
*condor_startd* daemon builds the list of items to advertise by
combining the lists in this order:

#. ``STARTD_ATTRS``
#. ``SLOT<N>_STARTD_ATTRS``

For example, consider the following configuration:

.. code-block:: text

    STARTD_ATTRS = favorite_color, favorite_season
    SLOT1_STARTD_ATTRS = favorite_movie
    SLOT2_STARTD_ATTRS = favorite_song

This will result in the *condor_startd* ClassAd for slot1 defining
values for ``favorite_color``, ``favorite_season``, and
``favorite_movie``. Slot2 will have values for ``favorite_color``,
``favorite_season``, and ``favorite_song``.

Attributes themselves in the ``STARTD_ATTRS`` list can also be defined
on a per-slot basis. Here is another example:

.. code-block:: text

    favorite_color = "blue"
    favorite_season = "spring"
    STARTD_ATTRS = favorite_color, favorite_season
    SLOT2_favorite_color = "green"
    SLOT3_favorite_season = "summer"

For this example, the *condor_startd* ClassAds are

slot1:

.. code-block:: text

    favorite_color = "blue"
    favorite_season = "spring"

slot2:

.. code-block:: text

    favorite_color = "green"
    favorite_season = "spring"

slot3:

.. code-block:: text

    favorite_color = "blue"
    favorite_season = "summer"

Dynamic Provisioning: Partitionable and Dynamic Slots
'''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`dynamic` :index:`dynamic<single: dynamic; slots>`
:index:`subdividing slots<single: subdividing slots; slots>` :index:`dynamic slots`
:index:`partitionable slots`

Dynamic provisioning, also referred to as partitionable or dynamic
slots, allows HTCondor to use the resources of a slot in a dynamic way;
these slots may be partitioned. This means that more than one job can
occupy a single slot at any one time. Slots have a fixed set of
resources which include the cores, memory and disk space. By
partitioning the slot, the use of these resources becomes more flexible.

Here is an example that demonstrates how resources are divided as more
than one job is or can be matched to a single slot. In this example,
Slot1 is identified as a partitionable slot and has the following
resources:

.. code-block:: text

    cpu = 10
    memory = 10240
    disk = BIG

Assume that JobA is allocated to this slot. JobA includes the following
requirements:

.. code-block:: text

    cpu = 3
    memory = 1024
    disk = 10240

The portion of the slot that is carved out is now known as a dynamic
slot. This dynamic slot has its own machine ClassAd, and its ``Name``
attribute distinguishes itself as a dynamic slot with incorporating the
substring ``Slot1_1``.

After allocation, the partitionable Slot1 advertises that it has the
following resources still available:

.. code-block:: text

    cpu = 7
    memory = 9216
    disk = BIG-10240

As each new job is allocated to Slot1, it breaks into ``Slot1_1``,
``Slot1_2``, ``Slot1_3`` etc., until the entire set of Slot1's available
resources have been consumed by jobs.

To enable dynamic provisioning, define a slot type. and declare at least
one slot of that type. Then, identify that slot type as partitionable by
setting configuration variable ``SLOT_TYPE_<N>_PARTITIONABLE``
:index:`SLOT_TYPE_<N>_PARTITIONABLE` to ``True``. The value of
``<N>`` within the configuration variable name is the same value as in
slot type definition configuration variable ``SLOT_TYPE_<N>``. For the
most common cases the machine should be configured for one slot,
managing all the resources on the machine. To do so, set the following
configuration variables:

.. code-block:: text

    NUM_SLOTS = 1
    NUM_SLOTS_TYPE_1 = 1
    SLOT_TYPE_1 = 100%
    SLOT_TYPE_1_PARTITIONABLE = TRUE

In a pool using dynamic provisioning, jobs can have extra, and desired,
resources specified in the submit description file:

.. code-block:: text

    request_cpus
    request_memory
    request_disk (in kilobytes)

This example shows a portion of the job submit description file for use
when submitting a job to a pool with dynamic provisioning.

.. code-block:: text

    universe = vanilla

    request_cpus = 3
    request_memory = 1024
    request_disk = 10240

    queue

Each partitionable slot will have the ClassAd attributes

.. code-block:: text

      PartitionableSlot = True
      SlotType = "Partitionable"

Each dynamic slot will have the ClassAd attributes

.. code-block:: text

      DynamicSlot = True
      SlotType = "Dynamic"

These attributes may be used in a ``START`` expression for the purposes
of creating detailed policies.

A partitionable slot will always appear as though it is not running a
job. If matched jobs consume all its resources, the partitionable slot
will eventually show as having no available resources; this will prevent
further matching of new jobs. The dynamic slots will show as running
jobs. The dynamic slots can be preempted in the same way as all other
slots.

Dynamic provisioning provides powerful configuration possibilities, and
so should be used with care. Specifically, while preemption occurs for
each individual dynamic slot, it cannot occur directly for the
partitionable slot, or for groups of dynamic slots. For example, for a
large number of jobs requiring 1GB of memory, a pool might be split up
into 1GB dynamic slots. In this instance a job requiring 2GB of memory
will be starved and unable to run. A partial solution to this problem is
provided by defragmentation accomplished by the *condor_defrag* daemon,
as discussed in
:ref:`admin-manual/policy-configuration:*condor_startd* policy configuration`.
:index:`partitionable slot preemption`
:index:`pslot preemption`

Another partial solution is a new matchmaking algorithm in the
negotiator, referred to as partitionable slot preemption, or pslot
preemption. Without pslot preemption, when the negotiator searches for a
match for a job, it looks at each slot ClassAd individually. With pslot
preemption, the negotiator looks at a partitionable slot and all of its
dynamic slots as a group. If the partitionable slot does not have
sufficient resources (memory, cpu, and disk) to be matched with the
candidate job, then the negotiator looks at all of the related dynamic
slots that the candidate job might preempt (following the normal
preemption rules described elsewhere). The resources of each dynamic
slot are added to those of the partitionable slot, one dynamic slot at a
time. Once this partial sum of resources is sufficient to enable a
match, the negotiator sends the match information to the
*condor_schedd*. When the *condor_schedd* claims the partitionable
slot, the dynamic slots are preempted, such that their resources are
returned to the partitionable slot for use by the new job.

To enable pslot preemption, the following configuration variable must be
set for the *condor_negotiator*:

.. code-block:: text

      ALLOW_PSLOT_PREEMPTION = True

When the negotiator examines the resources of dynamic slots, it sorts
the slots by their ``CurrentRank`` attribute, such that slots with lower
values are considered first. The negotiator only examines the cpu,
memory and disk resources of the dynamic slots; custom resources are
ignored.

Dynamic slots that have retirement time remaining are not considered
eligible for preemption, regardless of how configuration variable
``NEGOTIATOR_CONSIDER_EARLY_PREEMPTION`` is set.

When pslot preemption is enabled, the negotiator will not preempt
dynamic slots directly. It will preempt them only as part of a match to
a partitionable slot.

When multiple partitionable slots match a candidate job and the various
job rank expressions are evaluated to sort the matching slots, the
ClassAd of the partitionable slot is used for evaluation. This may cause
unexpected results for some expressions, as attributes such as
``RemoteOwner`` will not be present in a partitionable slot that matches
with preemption of some of its dynamic slots.

Defaults for Partitionable Slot Sizes
'''''''''''''''''''''''''''''''''''''

If a job does not specify the required number of CPUs, amount of memory,
or disk space, there are ways for the administrator to set default
values for all of these parameters.

First, if any of these attributes are not set in the submit description
file, there are three variables in the configuration file that
condor_submit will use to fill in default values. These are

    ``JOB_DEFAULT_REQUESTMEMORY``
    :index:`JOB_DEFAULT_REQUESTMEMORY`
    ``JOB_DEFAULT_REQUESTDISK`` :index:`JOB_DEFAULT_REQUESTDISK`
    ``JOB_DEFAULT_REQUESTCPUS`` :index:`JOB_DEFAULT_REQUESTCPUS`

The value of these variables can be ClassAd expressions. The default
values for these variables, should they not be set are

    ``JOB_DEFAULT_REQUESTMEMORY`` =
    ``ifThenElse(MemoryUsage =!= UNDEFINED, MemoryUsage, 1)``
    ``JOB_DEFAULT_REQUESTCPUS`` = ``1``
    ``JOB_DEFAULT_REQUESTDISK`` = ``DiskUsage``

Note that these default values are chosen such that jobs matched to
partitionable slots function similar to static slots.

Once the job has been matched, and has made it to the execute machine,
the *condor_startd* has the ability to modify these resource requests
before using them to size the actual dynamic slots carved out of the
partitionable slot. Clearly, for the job to work, the *condor_startd*
daemon must create slots with at least as many resources as the job
needs. However, it may be valuable to create dynamic slots somewhat
bigger than the job's request, as subsequent jobs may be more likely to
reuse the newly created slot when the initial job is done using it.

The *condor_startd* configuration variables which control this and
their defaults are

    ``MODIFY_REQUEST_EXPR_REQUESTCPUS`` = ``quantize(RequestCpus, {1})`` :index:`MODIFY_REQUEST_EXPR_REQUESTCPUS`
    ``MODIFY_REQUEST_EXPR_REQUESTMEMORY`` = ``quantize(RequestMemory, {128})`` :index:`MODIFY_REQUEST_EXPR_REQUESTMEMORY`
    ``MODIFY_REQUEST_EXPR_REQUESTDISK`` = ``quantize(RequestDisk, {1024})`` :index:`MODIFY_REQUEST_EXPR_REQUESTDISK`

condor_negotiator-Side Resource Consumption Policies
''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`consumption policy`
:index:`negotiator-side resource consumption policy<single: negotiator-side resource consumption policy; partitionable slots>`

For partitionable slots, the specification of a consumption policy
permits matchmaking at the negotiator. A dynamic slot carved from the
partitionable slot acquires the required quantities of resources,
leaving the partitionable slot with the remainder. This differs from
scheduler matchmaking in that multiple jobs can match with the
partitionable slot during a single negotiation cycle.

All specification of the resources available is done by configuration of
the partitionable slot. The machine is identified as having a resource
consumption policy enabled with

.. code-block:: text

      CONSUMPTION_POLICY = True

A defined slot type that is partitionable may override the machine value
with

.. code-block:: text

      SLOT_TYPE_<N>_CONSUMPTION_POLICY = True

A job seeking a match may always request a specific number of cores,
amount of memory, and amount of disk space. Availability of these three
resources on a machine and within the partitionable slot is always
defined and have these default values:

.. code-block:: text

      CONSUMPTION_CPUS = quantize(target.RequestCpus,{1})
      CONSUMPTION_MEMORY = quantize(target.RequestMemory,{128})
      CONSUMPTION_DISK = quantize(target.RequestDisk,{1024})

Here is an example-driven definition of a consumption policy. Assume a
single partitionable slot type on a multi-core machine with 8 cores, and
that the resource this policy cares about allocating are the cores.
Configuration for the machine includes the definition of the slot type
and that it is partitionable.

.. code-block:: text

      SLOT_TYPE_1 = cpus=8
      SLOT_TYPE_1_PARTITIONABLE = True
      NUM_SLOTS_TYPE_1 = 1

Enable use of the *condor_negotiator*-side resource consumption policy,
allocating the job-requested number of cores to the dynamic slot, and
use ``SLOT_WEIGHT`` :index:`SLOT_WEIGHT` to assess the user usage
that will affect user priority by the number of cores allocated. Note
that the only attributes valid within the ``SLOT_WEIGHT``
:index:`SLOT_WEIGHT` expression are Cpus, Memory, and disk. This
must the set to the same value on all machines in the pool.

.. code-block:: text

      SLOT_TYPE_1_CONSUMPTION_POLICY = True
      SLOT_TYPE_1_CONSUMPTION_CPUS = TARGET.RequestCpus
      SLOT_WEIGHT = Cpus

If custom resources are available within the partitionable slot, they
may be used in a consumption policy, by specifying the resource. Using a
machine with 4 GPUs as an example custom resource, define the resource
and include it in the definition of the partitionable slot:

.. code-block:: text

      MACHINE_RESOURCE_NAMES = gpus
      MACHINE_RESOURCE_gpus = 4
      SLOT_TYPE_2 = cpus=8, gpus=4
      SLOT_TYPE_2_PARTITIONABLE = True
      NUM_SLOTS_TYPE_2 = 1

Add the consumption policy to incorporate availability of the GPUs:

.. code-block:: text

      SLOT_TYPE_2_CONSUMPTION_POLICY = True
      SLOT_TYPE_2_CONSUMPTION_gpus = TARGET.RequestGpu
      SLOT_WEIGHT = Cpus

Defragmenting Dynamic Slots
'''''''''''''''''''''''''''

:index:`condor_defrag daemon`

When partitionable slots are used, some attention must be given to the
problem of the starvation of large jobs due to the fragmentation of
resources. The problem is that over time the machine resources may
become partitioned into slots suitable only for running small jobs. If a
sufficient number of these slots do not happen to become idle at the
same time on a machine, then a large job will not be able to claim that
machine, even if the large job has a better priority than the small
jobs.

One way of addressing the partitionable slot fragmentation problem is to
periodically drain all jobs from fragmented machines so that they become
defragmented. The *condor_defrag* daemon implements a configurable
policy for doing that. Its implementation is targeted at machines
configured to run whole-machine jobs and at machines that only have
partitionable slots. The draining of a machine configured to have both
partitionable slots and static slots would have a negative impact on
single slot jobs running in static slots.

To use this daemon, ``DEFRAG`` must be added to ``DAEMON_LIST``, and the
defragmentation policy must be configured. Typically, only one instance
of the *condor_defrag* daemon would be run per pool. It is a
lightweight daemon that should not require a lot of system resources.

Here is an example configuration that puts the *condor_defrag* daemon
to work:

.. code-block:: text

    DAEMON_LIST = $(DAEMON_LIST) DEFRAG
    DEFRAG_INTERVAL = 3600
    DEFRAG_DRAINING_MACHINES_PER_HOUR = 1.0
    DEFRAG_MAX_WHOLE_MACHINES = 20
    DEFRAG_MAX_CONCURRENT_DRAINING = 10

This example policy tells *condor_defrag* to initiate draining jobs
from 1 machine per hour, but to avoid initiating new draining if there
are 20 completely defragmented machines or 10 machines in a draining
state. A full description of each configuration variable used by the
*condor_defrag* daemon may be found in the 
:ref:`admin-manual/configuration-macros:condor_defrag configuration file
macros` section.

By default, when a machine is drained, existing jobs are gracefully
evicted. This means that each job will be allowed to use the remaining
time promised to it by ``MaxJobRetirementTime``. If the job has not
finished when the retirement time runs out, the job will be killed with
a soft kill signal, so that it has an opportunity to save a checkpoint
(if the job supports this).

By default, no new jobs will be allowed to start while the machine is
draining. To reduce unused time on the machine caused by some jobs
having longer retirement time than others, the eviction of jobs with
shorter retirement time is delayed until the job with the longest
retirement time needs to be evicted.

There is a trade off between reduced starvation and throughput. Frequent
draining of machines reduces the chance of starvation of large jobs.
However, frequent draining reduces total throughput. Some of the
machine's resources may go unused during draining, if some jobs finish
before others. If jobs that cannot produce checkpoints are killed
because they run past the end of their retirement time during draining,
this also adds to the cost of draining.

To reduce these costs, you may set the configuration macro
``DEFRAG_DRAINING_START_EXPR``
:index:`DEFRAG_DRAINING_START_EXPR`. If draining gracefully, the
defrag daemon will set the ``START`` :index:`START` expression for
the machine to this value expression. Do not set this to your usual
``START`` expression; jobs accepted while draining will not be given
their ``MaxRetirementTime``. Instead, when the last retiring job
finishes (either terminates or runs out of retirement time), all other
jobs on machine will be evicted with a retirement time of 0. (Those jobs
will be given their ``MaxVacateTime``, as usual.) The machine's
``START`` expression will become ``FALSE`` and stay that way until - as
usual - the machine exits the draining state.

We recommend that you allow only interruptible jobs to start on draining
machines. Different pools may have different ways of denoting
interruptible, but a ``MaxJobRetirementTime`` of 0 is probably a good
sign. You may also want to restrict the interruptible jobs'
``MaxVacateTime`` to ensure that the machine will complete draining
quickly.

To help gauge the costs of draining, the *condor_startd* advertises the
accumulated time that was unused due to draining and the time spent by
jobs that were killed due to draining. These are advertised respectively
in the attributes ``TotalMachineDrainingUnclaimedTime`` and
``TotalMachineDrainingBadput``. The *condor_defrag* daemon averages
these values across the pool and advertises the result in its daemon
ClassAd in the attributes ``AvgDrainingBadput`` and
``AvgDrainingUnclaimed``. Details of all attributes published by the
*condor_defrag* daemon are described in the :doc:`/classad-attributes/defrag-classad-attributes` section.

The following command may be used to view the *condor_defrag* daemon
ClassAd:

.. code-block:: console

    $ condor_status -l -any -constraint 'MyType == "Defrag"'

:index:`configuration<single: configuration; SMP machines>`
:index:`configuration<single: configuration; multi-core machines>`

*condor_schedd* Policy Configuration
-------------------------------------

:index:`condor_schedd policy<single: condor_schedd policy; configuration>`
:index:`policy configuration<single: policy configuration; submit host>`

There are two types of schedd policy: job transforms (which change the
ClassAd of a job at submission) and submit requirements (which prevent
some jobs from entering the queue). These policies are explained below.

Job Transforms
''''''''''''''

:index:`job transforms`

The *condor_schedd* can transform jobs as they are submitted.
Transformations can be used to guarantee the presence of required job
attributes, to set defaults for job attributes the user does not supply,
or to modify job attributes so that they conform to schedd policy; an
example of this might be to automatically set accounting attributes
based on the owner of the job while letting the job owner indicate a
preference.

There can be multiple job transforms. Each transform can have a
Requirements expression to indicate which jobs it should transform and
which it should ignore. Transforms without a Requirements expression
apply to all jobs. Job transforms are applied in order. The set of
transforms and their order are configured using the Configuration
variable ``JOB_TRANSFORM_NAMES`` :index:`JOB_TRANSFORM_NAMES`.

For each entry in this list there must be a corresponding
``JOB_TRANSFORM_<name>`` :index:`JOB_TRANSFORM_<name>`
configuration variable that specifies the transform rules. Transforms
use the same syntax as *condor_job_router* transforms; although unlike
the *condor_job_router* there is no default transform, and all
matching transforms are applied - not just the first one. (See the
:doc:`/grid-computing/job-router` section for information on the
*condor_job_router*.)

The following example shows a set of two transforms: one that
automatically assigns an accounting group to jobs based on the
submitting user, and one that shows one possible way to transform
Vanilla jobs to Docker jobs.

.. code-block:: text

    JOB_TRANSFORM_NAMES = AssignGroup, SL6ToDocker

    JOB_TRANSFORM_AssignGroup = [ eval_set_AccountingGroup = userMap("Groups",Owner,AccountingGroup); ]

    JOB_TRANSFORM_SL6ToDocker @=end
    [
       Requirements = JobUniverse==5 && WantSL6 && DockerImage =?= undefined;
       set_WantDocker = true;
       set_DockerImage = "SL6";
       copy_Requirements = "VanillaRequrements";
       set_Requirements = TARGET.HasDocker && VanillaRequirements
    ]
    @end

The AssignGroup transform above assumes that a mapfile that can map an
owner to one or more accounting groups has been configured via
``SCHEDD_CLASSAD_USER_MAP_NAMES``, and given the name "Groups".

The SL6ToDocker transform above is most likely incomplete, as it assumes
some custom attributes (``WantSL6`` and ``WantDocker`` and
``HasDocker``) that your pool may or may not use.

Submit Requirements
'''''''''''''''''''

:index:`submit requirements`

The *condor_schedd* may reject job submissions, such that rejected jobs
never enter the queue. Rejection may be best for the case in which there
are jobs that will never be able to run; for instance, a job specifying
an obsolete universe, like standard.
Another appropriate example might be to reject all jobs that
do not request a minimum amount of memory. Or, it may be appropriate to
prevent certain users from using a specific submit host.

Rejection criteria are configured. Configuration variable
``SUBMIT_REQUIREMENT_NAMES`` :index:`SUBMIT_REQUIREMENT_NAMES`
lists criteria, where each criterion is given a name. The chosen name is
a major component of the default error message output if a user attempts
to submit a job which fails to meet the requirements. Therefore, choose
a descriptive name. For the three example submit requirements described:

.. code-block:: text

    SUBMIT_REQUIREMENT_NAMES = NotStandardUniverse, MinimalRequestMemory, NotChris

The criterion for each submit requirement is then specified in
configuration variable ``SUBMIT_REQUIREMENT_<Name>``
:index:`SUBMIT_REQUIREMENT_<Name>`, where ``<Name>`` matches the
chosen name listed in ``SUBMIT_REQUIREMENT_NAMES``. The value is a
boolean ClassAd expression. The three example criterion result in these
configuration variable definitions:

.. code-block:: text

    SUBMIT_REQUIREMENT_NotStandardUniverse = JobUniverse != 1
    SUBMIT_REQUIREMENT_MinimalRequestMemory = RequestMemory > 512
    SUBMIT_REQUIREMENT_NotChris = Owner != "chris"

Submit requirements are evaluated in the listed order; the first
requirement that evaluates to ``False`` causes rejection of the job,
terminates further evaluation of other submit requirements, and is the
only requirement reported. Each submit requirement is evaluated in the
context of the *condor_schedd* ClassAd, which is the ``MY.`` name space
and the job ClassAd, which is the ``TARGET.`` name space. Note that
``JobUniverse`` and ``RequestMemory`` are both job ClassAd attributes.

Further configuration may associate a rejection reason with a submit
requirement with the ``SUBMIT_REQUIREMENT_<Name>_REASON``
:index:`SUBMIT_REQUIREMENT_<Name>_REASON`.

.. code-block:: text

    SUBMIT_REQUIREMENT_NotStandardUniverse_REASON = "This pool does not accept standard universe jobs."
    SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON = strcat( "The job only requested ", \
      RequestMemory, " Megabytes.  If that small amount is really enough, please contact ..." )
    SUBMIT_REQUIREMENT_NotChris_REASON = "Chris, you may only submit jobs to the instructional pool."

The value must be a ClassAd expression which evaluates to a string.
Thus, double quotes were required to make strings for both
``SUBMIT_REQUIREMENT_NotStandardUniverse_REASON`` and
``SUBMIT_REQUIREMENT_NotChris_REASON``. The ClassAd function strcat()
produces a string in the definition of
``SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON``.

Rejection reasons are sent back to the submitting program and will
typically be immediately presented to the user. If an optional
``SUBMIT_REQUIREMENT_<Name>_REASON`` is not defined, a default reason
will include the ``<Name>`` chosen for the submit requirement.
Completing the presentation of the example submit requirements, upon an
attempt to submit a standard universe job, *condor_submit* would print

.. code-block:: text

    Submitting job(s).
    ERROR: Failed to commit job submission into the queue.
    ERROR: This pool does not accept standard universe jobs.

Where there are multiple jobs in a cluster, if any job within the
cluster is rejected due to a submit requirement, the entire cluster of
jobs will be rejected.

Submit Warnings
'''''''''''''''

:index:`submit warnings`

Starting in HTCondor 8.7.4, you may instead configure submit warnings. A
submit warning is a submit requirement for which
``SUBMIT_REQUIREMENT_<Name>_IS_WARNING``
:index:`SUBMIT_REQUIREMENT_<Name>_IS_WARNING` is true. A submit
warning does not cause the submission to fail; instead, it returns a
warning to the user's console (when triggered via *condor_submit*) or
writes a message to the user log (always). Submit warnings are intended
to allow HTCondor administrators to provide their users with advance
warning of new submit requirements. For example, if you want to increase
the minimum request memory, you could use the following configuration.

.. code-block:: text

    SUBMIT_REQUIREMENT_NAMES = OneGig $(SUBMIT_REQUIREMENT_NAMES)
    SUBMIT_REQUIREMENT_OneGig = RequestMemory > 1024
    SUBMIT_REQUIREMENT_OneGig_REASON = "As of <date>, the minimum requested memory will be 1024."
    SUBMIT_REQUIREMENT_OneGig_IS_WARNING = TRUE

When a user runs *condor_submit* to submit a job with ``RequestMemory``
between 512 and 1024, they will see (something like) the following,
assuming that the job meets all the other requirements.

.. code-block:: text

    Submitting job(s).
    WARNING: Committed job submission into the queue with the following warning:
    WARNING: As of <date>, the minimum requested memory will be 1024.

    1 job(s) submitted to cluster 452.

The job will contain (something like) the following:

.. code-block:: text

    000 (452.000.000) 10/06 13:40:45 Job submitted from host: <128.105.136.53:37317?addrs=128.105.136.53-37317+[fc00--1]-37317&noUDP&sock=19966_e869_5>
        WARNING: Committed job submission into the queue with the following warning: As of <date>, the minimum requested memory will be 1024.
    ...

Marking a submit requirement as a warning does not change when or how it
is evaluated, only the result of doing so. In particular, failing a
submit warning does not terminate further evaluation of the submit
requirements list. Currently, only one (the most recent) problem is
reported for each submit attempt. This means users will see (as they
previously did) only the first failed requirement; if all requirements
passed, they will see the last failed warning, if any.
