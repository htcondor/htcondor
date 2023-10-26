Configuration for Execution Points
==================================

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
remote jobs should start, be suspended, (possibly) resumed, vacate
or be killed. This policy is the heart of HTCondor's
balancing act between the needs and wishes of resource owners (machine
owners) and resource users (people submitting their jobs to HTCondor).
Please read this section carefully before changing any of the settings
described here, as a wrong setting can have a severe impact on either
the owners of machines in the pool or the users of the pool.

*condor_startd* Terminology
''''''''''''''''''''''''''''

Understanding the configuration requires an understanding of ClassAd
expressions, which are detailed in the :doc:`/classads/classad-mechanism`
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

The most important expression to the *condor_startd* is the
:macro:`START` expression. This expression describes the
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
theh :doc:`/classads/classad-mechanism` section for specifics on
how undefined terms are handled in ClassAd expression evaluation.

A note of caution is in order when modifying the ``START`` expression to
reference job ClassAd attributes. When using the ``POLICY : Desktop``
configuration template, the ``IS_OWNER`` expression is a function of the
``START`` expression:

.. code-block:: condor-classad-expr

    START =?= FALSE

See a detailed discussion of the ``IS_OWNER`` expression in
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`.
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
    :macro:`JOB_RENICE_INCREMENT` so that
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

.. mermaid:: 
   :caption: Machine states and the possible transitions between the states
   :align: center

   stateDiagram-v2
     direction LR
     [*]--> Owner
     Owner --> Unclaimed: A
     Unclaimed --> Matched: C
     Unclaimed --> Owner: B
     Unclaimed --> Drained: P
     Unclaimed --> Backfill: E
     Unclaimed --> Claimed: D
     Backfill  --> Owner: K
     Backfill  --> Matched: L
     Backfill  --> Claimed: M
     Matched --> Claimed: G
     Matched --> Owner: F
     Claimed --> Preempting: H
     Preempting --> Owner: J
     Preempting --> Claimed: I
     Owner --> Drained: N
     Drained --> Owner: O


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
       :macro:`MATCH_TIMEOUT` timer expires.
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
       -  The :macro:`PREEMPT` expression evaluates to ``True`` (which
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

       -  The :macro:`EVICT_BACKFILL` expression evaluates to TRUE
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
starts at the value set by the configuration variable
:macro:`ALIVE_INTERVAL`. It may be modified when a job is started.
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
       often the activity occurs is determined by the :macro:`RUNBENCHMARKS`
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
       The :macro:`MaxJobRetirementTime` expression determines how long to
       wait (counting since the time the job started). Once the job
       finishes or the retirement time expires, the Preempting state is
       entered.

-  Preempting The Preempting state is used for evicting an HTCondor job
   from a given machine. When the machine enters the Preempting state,
   it checks the :macro:`WANT_VACATE` expression to determine its activity.

    Vacating
       In the Vacating activity, the job is given a chance to exit
       cleanly.  This may include uploading intermediate files.  As
       soon as the job finishes exiting,
       the machine moves into either the Owner state or the
       Claimed state, depending on the reason for its preemption.
       :index:`Killing<single: Killing; machine activity>`
    Killing
       Killing means that the machine has requested the running job to
       exit the machine immediately.

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
:macro:`IS_OWNER` evaluates to TRUE. If the
``IS_OWNER`` expression evaluates to FALSE, then the machine transitions
to the Unclaimed state. The default value of ``IS_OWNER`` is FALSE,
which is intended for dedicated resources. But when the
``POLICY : Desktop`` configuration template is used, the ``IS_OWNER``
expression is optimized for a shared resource

.. code-block:: condor-classad-expr

    START =?= FALSE

So, the machine will remain in the Owner state as long as the ``START``
expression locally evaluates to FALSE.
The :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
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
every :macro:`UPDATE_INTERVAL` to see if
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

When in the Unclaimed state, the :macro:`RUNBENCHMARKS` expression is relevant.
If ``RUNBENCHMARKS`` evaluates to TRUE while the machine is in the
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

If the machine has been configured to perform backfill jobs (see the
:doc:`/admin-manual/setting-up-special-environments` section), while it is in
Unclaimed/Idle it will evaluate the :macro:`START_BACKFILL` expression. Once
``START_BACKFILL`` evaluates to TRUE, the machine will enter the Backfill/Idle
state (transition **7**) to begin the process of running backfill jobs.

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
state too long. The timer is set with the
:macro:`MATCH_TIMEOUT` configuration file macro. It is specified
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
activities. In addition, the *condor_vacate*
command affects the machine when it is in the Claimed state.

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

While Claimed, the :macro:`POLLING_INTERVAL`
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
determined by the configuration variable
:macro:`MachineMaxVacateTime`. This may be adjusted by the setting
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
of which is defined by the :macro:`KILLING_TIMEOUT` macro
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

:macro:`START`
    When TRUE, the machine is willing to spawn a remote HTCondor job.

:macro:`RUNBENCHMARKS`
    While in the Unclaimed state, the machine will run benchmarks
    whenever TRUE.

:macro:`MATCH_TIMEOUT`
    If the machine has been in the Matched state longer than this value,
    it will transition to the Owner state.

:macro:`WANT_SUSPEND`
    If ``True``, the machine evaluates the ``SUSPEND`` expression to see
    if it should transition to the Suspended activity. If any value
    other than ``True``, the machine will look at the ``PREEMPT``
    expression.

:macro:`SUSPEND`
    If ``WANT_SUSPEND`` is ``True``, and the machine is in the
    Claimed/Busy state, it enters the Suspended activity if ``SUSPEND``
    is ``True``.

:macro:`CONTINUE`
    If the machine is in the Claimed/Suspended state, it enter the Busy
    activity if ``CONTINUE`` is ``True``.

:macro:`PREEMPT`
    If the machine is either in the Claimed/Suspended activity, or is in
    the Claimed/Busy activity and ``WANT_SUSPEND`` is FALSE, the machine
    enters the Claimed/Retiring state whenever ``PREEMPT`` is TRUE.

:macro:`CLAIM_WORKLIFE`
    This expression specifies the number of seconds after which a claim
    will stop accepting additional jobs. This configuration macro is
    fully documented here: :ref:`admin-manual/configuration-macros:condor_startd
    configuration file macros`.

:macro:`MachineMaxVacateTime`
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
    hard-killed. The :macro:`KILL` expression may be used
    to abort the graceful shutdown of the job at any time.

:macro:`MAXJOBRETIREMENTTIME`
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
    :macro:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION`.

:macro:`WANT_VACATE`
    This is checked only when the ``PREEMPT`` expression is ``True`` and
    the machine enters the Preempting state. If ``WANT_VACATE`` is
    ``True``, the machine enters the Vacating activity. If it is
    ``False``, the machine will proceed directly to the Killing
    activity.

:macro:`KILL`
    If the machine is in the Preempting/Vacating state, it enters
    Preempting/Killing whenever ``KILL`` is ``True``.

:macro:`KILLING_TIMEOUT`
    If the machine is in the Preempting/Killing state for longer than
    ``KILLING_TIMEOUT`` seconds, the *condor_startd* sends a SIGKILL to
    the *condor_starter* and all its children to try to kill the job as
    quickly as possible.

:macro:`RANK`
    If this expression evaluates to a higher number for a pending
    resource request than it does for the current request, the machine
    may preempt the current request (enters the Preempting/Vacating
    state). When the preemption is complete, the machine enters the
    Claimed/Idle state with the new resource request claiming it.

:macro:`START_BACKFILL`
    When TRUE, if the machine is otherwise idle, it will enter the
    Backfill state and spawn a backfill computation (using BOINC).

:macro:`EVICT_BACKFILL`
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
mostly jobs that cannot produce checkpoints, it
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

Preemption causes a running job to be suspended or killed, such that another
job can run. Preemption is disabled by the default configuration.
Configuration file examples disable preemption, but contain directions for
enabling preemption.
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
   evicted, giving it a chance to write a checkpoint, and then is killed. The
   job will return to the idle state in the job queue,
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
------------------------------

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
jobs are free to over subscribe to shared resources. Consider an example
where two slots are each defined with 50% of available RAM. The
resultant ClassAd for each slot will advertise one half the available
RAM. Users may submit jobs with RAM requirements that match these slots.
However, jobs run on either slot are free to consume more than 50% of
available RAM. HTCondor will not directly enforce a RAM utilization
limit on either slot. If a shared resource enforcement capability is
needed, it is possible to write a policy that will evict a job that
over subscribes to shared resources, as described in
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`.

Dividing System Resources in Multi-core Machines
''''''''''''''''''''''''''''''''''''''''''''''''

Within a machine the shared system resources of cores, RAM, swap space
and disk space will be divided for use by the slots. There are two main
ways to go about dividing the resources of a multi-core machine:

Evenly divide all resources.
    Prior to HTCondor 23.0 the *condor_startd* will automatically divide the
    machine into multiple slots by default, placing one core in each slot, and evenly
    dividing all shared resources among the slots. Beginning with HTCondor 23.0
    the *condor_startd* will create a single partitionable slot by default.

    In HTCondor 23.0 you can use the configuration template ``use FEATURE : StaticSlots``
    to configure a number of static slots. If used without arguments this
    configuration template will define a number of single core static slots equal to
    the number of detected cpu cores.

    To simply configure static slots in any version, configure :macro:`NUM_SLOTS` to the
    integer number of slots desired. ``NUM_SLOTS`` may not be used to make HTCondor advertise
    more slots than there are cores on the machine. The number of cores
    is defined by :macro:`NUM_CPUS`.

Define slot types.
    Instead of the default slot configuration, the machine may
    have definitions of slot types, where each type is provided with a
    fraction of shared system resources. Given the slot type definition,
    control how many of each type are reported at any given time with
    further configuration.

    Configuration variables define the slot types, as well as variables
    that list how much of each system resource goes to each slot type.

    Configuration variable :macro:`SLOT_TYPE_<N>`, where <N> is an integer (for
    example, ``SLOT_TYPE_1``) defines the slot type. Note that there may be
    multiple slots of each type. The number of slots created of a given type is
    configured with ``NUM_SLOTS_TYPE_<N>``.

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
    partition containing the slot's :macro:`EXECUTE` or 
    :macro:`SLOT<N>_EXECUTE` directory. If every slot is in a
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
    In addition to GPUs (see :ref:`admin-manual/ep-policy-configuration:Configuring GPUs`.)
    the administrator can define other types of custom resources.

    The resource names and quantities of available resources are defined
    using configuration variables of the form
    :macro:`MACHINE_RESOURCE_<name>`,
    as shown in this example:

    .. code-block:: condor-config

        MACHINE_RESOURCE_Cogs = 16
        MACHINE_RESOURCE_actuator = 8

    If the configuration uses the optional configuration variable
    :macro:`MACHINE_RESOURCE_NAMES` to
    enable and disable local machine resources, also add the resource
    names to this variable. For example:

    .. code-block:: condor-config

        if defined MACHINE_RESOURCE_NAMES
          MACHINE_RESOURCE_NAMES = $(MACHINE_RESOURCE_NAMES) Cogs actuator
        endif

    Local machine resource names defined in this way may now be used in
    conjunction with :macro:`SLOT_TYPE_<N>`,
    using all the same syntax described earlier in this section. The
    following example demonstrates the definition of static and
    partitionable slot types with local machine resources:

    .. code-block:: condor-config

        # declare one partitionable slot with half of the Cogs, 6 actuators, and
        # 50% of all other resources:
        SLOT_TYPE_1 = cogs=50%,actuator=6,50%
        SLOT_TYPE_1_PARTITIONABLE = TRUE
        NUM_SLOTS_TYPE_1 = 1

        # declare two static slots, each with 25% of the Cogs, 1 actuator, and
        # 25% of all other resources:
        SLOT_TYPE_2 = cogs=25%,actuator=1,25%
        SLOT_TYPE_2_PARTITIONABLE = FALSE
        NUM_SLOTS_TYPE_2 = 2

    A job may request these local machine resources using the syntax
    **request_<name>** :index:`request_<name><single: request_<name>; submit commands>`,
    as described in :ref:`admin-manual/ep-policy-configuration:*condor_startd*
    policy configuration`. This example shows a portion of a submit description
    file that requests cogs and an actuator:

    .. code-block:: condor-submit

        universe = vanilla

        # request two cogs and one actuator:
        request_cogs = 2
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

    From the example given, the ``Cogs`` resource would be represented by
    the ClassAd attributes ``TotalCogs``, ``DetectedCogs``,
    ``TotalSlotCogs``, and ``Cogs``. In the job ClassAd, the amount of the
    requested machine resource appears in a job ClassAd attribute named
    ``Request<name>``. For this example, the two attributes will be
    ``RequestCogs`` and ``RequestActuator``.

    The number of each type and the
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

-  :macro:`SLOTS_CONNECTED_TO_CONSOLE`, with definition at
   the :ref:`admin-manual/configuration-macros:condor_startd configuration file
   macros` section

-  :macro:`SLOTS_CONNECTED_TO_KEYBOARD`, with definition at
   the :ref:`admin-manual/configuration-macros:condor_startd configuration file
   macros` section

-  :macro:`DISCONNECTED_KEYBOARD_IDLE_BOOST`, with definition at
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

Configuring GPUs
----------------

:index:`configuration<single: configuration; GPUs>`
:index:`to use GPUs<single: to use GPUs; configuration>`

HTCondor supports incorporating GPU resources and making them available
for jobs. First, GPUs must be detected as available resources. Then,
machine ClassAd attributes advertise this availability. Both detection
and advertisement are accomplished by having this configuration for each
execute machine that has GPUs:

.. code-block:: text

      use feature : GPUs

Use of this configuration template invokes the *condor_gpu_discovery*
tool to create a custom resource, with a custom resource name of
``GPUs``, and it generates the ClassAd attributes needed to advertise
the GPUs. *condor_gpu_discovery* is invoked in a mode that discovers
and advertises both CUDA and OpenCL GPUs.

This configuration template refers to macro :macro:`GPU_DISCOVERY_EXTRA`,
which can be used to define additional command line arguments for the
*condor_gpu_discovery* tool. For example, setting

.. code-block:: text

      use feature : GPUs
      GPU_DISCOVERY_EXTRA = -extra

causes the *condor_gpu_discovery* tool to output more attributes that
describe the detected GPUs on the machine.

*condor_gpu_discovery* defaults to using nested ClassAds for GPU properties.  The administrator
can be explicit about which form to use for properties by adding either the
``-nested`` or ``-not-nested`` option to :macro:`GPU_DISCOVERY_EXTRA`. 

The format -- nested or not -- of GPU properties in the slot ad is the same as published
by *condor_gpu_discovery*.  The use of nested GPU property ads is necessary
to do GPU matchmaking and to properly support heterogeneous GPUs.  

For resources like GPUs that have individual properties, when configuring slots
the slot configuration can specify a constraint on those properties
for the purpose of choosing which GPUs are assigned to which slots.  This serves
the same purpose as the ``require_gpus`` submit keyword, but in this case
it controls the slot configuration on startup.

The resource constraint can be specified by following the resource quantity 
with a colon and then a constraint expression.  The constraint expression can
refer to resource property attributes like the GPU properties from
*condor_gpu_discovery* ``-nested`` output.  If the constraint expression is 
a string literal, it will be matched automatically against the resource id,
otherwise it will be evaluated against each of the resource property ads.

When using resource constraints, it is recommended that you put each
resource quantity on a separate line as in the following example, otherwise the
constraint expression may be truncated.

    .. code-block:: condor-config

        # Assuming a machine that has two types of GPUs, 2 of which have Capability 8.0
        # and the remaining GPUs are less powerful

        # declare a partitionable slot that has the 2 powerful GPUs
        # and 90% of the other resources:
        SLOT_TYPE_1 @=slot
           GPUs = 2 : Capability >= 8.0
           90%
        @slot
        SLOT_TYPE_1_PARTITIONABLE = TRUE
        NUM_SLOTS_TYPE_1 = 1

        # declare a small static slot and assign it a specific GPU by id
        SLOT_TYPE_2 @=slot
           GPUs = 1 : "GPU-6a96bd13"
           CPUs = 1
		   Memory = 10
        @slot
        SLOT_TYPE_2_PARTITIONABLE = FALSE
        NUM_SLOTS_TYPE_2 = 1

        # declare two static slots that split up the remaining resources which may or may not include GPUs
        SLOT_TYPE_3 = auto
        SLOT_TYPE_3_PARTITIONABLE = FALSE
        NUM_SLOTS_TYPE_3 = 2



Configuring STARTD_ATTRS on a per-slot basis
--------------------------------------------

The :macro:`STARTD_ATTRS`  settings can be configured on a per-slot basis. The
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
-----------------------------------------------------

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
setting configuration variable
:macro:`SLOT_TYPE_<N>_PARTITIONABLE` to ``True``. The value of
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
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`.
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

-  :macro:`JOB_DEFAULT_REQUESTCPUS`
-  :macro:`JOB_DEFAULT_REQUESTMEMORY`
-  :macro:`JOB_DEFAULT_REQUESTDISK`

The value of these variables can be ClassAd expressions. The default
values for these variables, should they not be set are

.. code-block:: condor-config

    JOB_DEFAULT_REQUESTCPUS = 1
    JOB_DEFAULT_REQUESTMEMORY = \
        ifThenElse(MemoryUsage =!= UNDEFINED, MemoryUsage, 1)
    JOB_DEFAULT_REQUESTDISK = DiskUsage

Note that these default values are chosen such that jobs matched to
partitionable slots function similar to static slots.
These variables do not apply to **batch** grid universe jobs.

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

- :macro:`MODIFY_REQUEST_EXPR_REQUESTCPUS` = quantize(RequestCpus, {1})
- :macro:`MODIFY_REQUEST_EXPR_REQUESTMEMORY` = quantize(RequestMemory, {128})
- :macro:`MODIFY_REQUEST_EXPR_REQUESTDISK` = quantize(RequestDisk, {1024})

Startd Cron
-----------

:index:`Startd Cron`
:index:`Schedd Cron`

Daemon ClassAd Hooks
''''''''''''''''''''

:index:`Daemon ClassAd Hooks<single: Daemon ClassAd Hooks; Hooks>`
:index:`Daemon ClassAd Hooks`
:index:`Startd Cron`
:index:`Schedd Cron`
:index:`see Daemon ClassAd Hooks<single: see Daemon ClassAd Hooks; Startd Cron functionality>`
:index:`see Daemon ClassAd Hooks<single: see Daemon ClassAd Hooks; Schedd Cron functionality>`

Overview
''''''''

The Startd Cron and Schedd Cron *Daemon ClassAd Hooks* mechanism are
used to run executables (called jobs) directly from the *condor_startd* and *condor_schedd* daemons.
The output from these jobs is incorporated into the machine ClassAd
generated by the respective daemon. This mechanism and associated jobs
have been identified by various names, including the Startd Cron,
dynamic attributes, and a distribution of executables collectively known
as Hawkeye.

Pool management tasks can be enhanced by using a daemon's ability to
periodically run executables. The executables are expected to generate
ClassAd attributes as their output; these ClassAds are then incorporated
into the machine ClassAd. Policy expressions can then reference dynamic
attributes (created by the ClassAd hook jobs) in the machine ClassAd.

Job output
''''''''''

The output of the job is incorporated into one or more ClassAds when the
job exits. When the job outputs the special line:

.. code-block:: text

      - update:true

the output of the job is merged into all proper ClassAds, and an update
goes to the *condor_collector* daemon.

As of version 8.3.0, it is possible for a Startd Cron job (but not a
Schedd Cron job) to define multiple ClassAds, using the mechanism
defined below:

-  An output line starting with ``'-'`` has always indicated
   end-of-ClassAd. The ``'-'`` can now be followed by a uniqueness tag
   to indicate the name of the ad that should be replaced by the new ad.
   This name is joined to the name of the Startd Cron job to produced a
   full name for the ad. This allows a single Startd Cron job to return
   multiple ads by giving each a unique name, and to replace multiple
   ads by using the same unique name as a previous invocation. The
   optional uniqueness tag can also be followed by the optional keyword
   ``update:<bool>``, which can be used to override the Startd Cron
   configuration and suppress or force immediate updates.

   In other words, the syntax is:

   - [*name* ] [**update:** *bool*]

-  Each ad can contain one of four possible attributes to control what
   slot ads the ad is merged into when the *condor_startd* sends
   updates to the collector. These attributes are, in order of highest
   to lower priority (in other words, if ``SlotMergeConstraint``
   matches, the other attributes are not considered, and so on):

   -  **SlotMergeConstraint** *expression*: the current ad is merged
      into all slot ads for which this expression is true. The
      expression is evaluated with the slot ad as the TARGET ad.
   -  **SlotName|Name** *string*: the current ad is merged into all
      slots whose ``Name`` attributes match the value of ``SlotName`` up
      to the length of ``SlotName``.
   -  **SlotTypeId** *integer*: the current ad is merged into all ads
      that have the same value for their ``SlotTypeId`` attribute.
   -  **SlotId** *integer*: the current ad is merged into all ads that
      have the same value for their ``SlotId`` attribute.

For example, if the Startd Cron job returns:

.. code-block:: text

      Value=1
      SlotId=1
      -s1
      Value=2
      SlotId=2
      -s2
      Value=10
      - update:true

it will set ``Value=10`` for all slots except slot1 and slot2. On those
slots it will set ``Value=1`` and ``Value=2`` respectively. It will also
send updates to the collector immediately.

Configuration
'''''''''''''

Configuration variables related to Daemon ClassAd Hooks are defined in
:ref:`admin-manual/configuration-macros:Configuration File Entries Relating to Daemon ClassAd Hooks: Startd Cron and Schedd Cron`

Here is a complete configuration example. It defines all three of the
available types of jobs: ones that use the *condor_startd*, benchmark
jobs, and ones that use the *condor_schedd*.

.. code-block:: condor-config

    #
    # Startd Cron Stuff
    #
    # auxiliary variable to use in identifying locations of files
    MODULES = $(ROOT)/modules

    STARTD_CRON_CONFIG_VAL = $(RELEASE_DIR)/bin/condor_config_val
    STARTD_CRON_MAX_JOB_LOAD = 0.2
    STARTD_CRON_JOBLIST =

    # Test job
    STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) test
    STARTD_CRON_TEST_MODE = OneShot
    STARTD_CRON_TEST_RECONFIG_RERUN = True
    STARTD_CRON_TEST_PREFIX = test_
    STARTD_CRON_TEST_EXECUTABLE = $(MODULES)/test
    STARTD_CRON_TEST_KILL = True
    STARTD_CRON_TEST_ARGS = abc 123
    STARTD_CRON_TEST_SLOTS = 1
    STARTD_CRON_TEST_JOB_LOAD = 0.01

    # job 'date'
    STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) date
    STARTD_CRON_DATE_MODE = Periodic
    STARTD_CRON_DATE_EXECUTABLE = $(MODULES)/date
    STARTD_CRON_DATE_PERIOD = 15s
    STARTD_CRON_DATE_JOB_LOAD = 0.01

    # Job 'foo'
    STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) foo
    STARTD_CRON_FOO_EXECUTABLE = $(MODULES)/foo
    STARTD_CRON_FOO_PREFIX = Foo
    STARTD_CRON_FOO_MODE = Periodic
    STARTD_CRON_FOO_PERIOD = 10m
    STARTD_CRON_FOO_JOB_LOAD = 0.2

    #
    # Benchmark Stuff
    #
    BENCHMARKS_JOBLIST = mips kflops

    # MIPS benchmark
    BENCHMARKS_MIPS_EXECUTABLE = $(LIBEXEC)/condor_mips
    BENCHMARKS_MIPS_JOB_LOAD = 1.0

    # KFLOPS benchmark
    BENCHMARKS_KFLOPS_EXECUTABLE = $(LIBEXEC)/condor_kflops
    BENCHMARKS_KFLOPS_JOB_LOAD = 1.0

    #
    # Schedd Cron Stuff. Unlike the Startd,
    # a restart of the Schedd is required for changes to take effect
    #
    SCHEDD_CRON_CONFIG_VAL = $(RELEASE_DIR)/bin/condor_config_val
    SCHEDD_CRON_JOBLIST =

    # Test job
    SCHEDD_CRON_JOBLIST = $(SCHEDD_CRON_JOBLIST) test
    SCHEDD_CRON_TEST_MODE = OneShot
    SCHEDD_CRON_TEST_RECONFIG_RERUN = True
    SCHEDD_CRON_TEST_PREFIX = test_
    SCHEDD_CRON_TEST_EXECUTABLE = $(MODULES)/test
    SCHEDD_CRON_TEST_PERIOD = 5m
    SCHEDD_CRON_TEST_KILL = True
    SCHEDD_CRON_TEST_ARGS = abc 123

:index:`Hooks`

Docker Universe
---------------

:index:`set up<single: set up; docker universe>`
:index:`for the docker universe<single: for the docker universe; installation>`
:index:`docker<single: docker; universe>`
:index:`set up for the docker universe<single: set up for the docker universe; universe>`

The execution of a docker universe job causes the instantiation of a
Docker container on an execute host.

The docker universe job is mapped to a vanilla universe job, and the
submit description file must specify the submit command
**docker_image** :index:`docker_image<single: docker_image; submit commands>` to
identify the Docker image. The job's ``requirement`` ClassAd attribute
is automatically appended, such that the job will only match with an
execute machine that has Docker installed.
:index:`HasDocker<single: HasDocker; ClassAd machine attribute>`

The Docker service must be pre-installed on each execute machine that
can execute a docker universe job. Upon start up of the *condor_startd*
daemon, the capability of the execute machine to run docker universe
jobs is probed, and the machine ClassAd attribute ``HasDocker`` is
advertised for a machine that is capable of running Docker universe
jobs.

When a docker universe job is matched with a Docker-capable execute
machine, HTCondor invokes the Docker CLI to instantiate the
image-specific container. The job's scratch directory tree is mounted as
a Docker volume. When the job completes, is put on hold, or is evicted,
the container is removed.

An administrator of a machine can optionally make additional directories
on the host machine readable and writable by a running container. To do
this, the admin must first give an HTCondor name to each directory with
the DOCKER_VOLUMES parameter. Then, each volume must be configured with
the path on the host OS with the DOCKER_VOLUME_DIR_XXX parameter.
Finally, the parameter DOCKER_MOUNT_VOLUMES tells HTCondor which of
these directories to always mount onto containers running on this
machine.

For example,

.. code-block:: condor-config

    DOCKER_VOLUMES = SOME_DIR, ANOTHER_DIR
    DOCKER_VOLUME_DIR_SOME_DIR = /path1
    DOCKER_VOLUME_DIR_ANOTHER_DIR = /path/to/no2
    DOCKER_MOUNT_VOLUMES = SOME_DIR, ANOTHER_DIR

The *condor_startd* will advertise which docker volumes it has
available for mounting with the machine attributes
HasDockerVolumeSOME_NAME = true so that jobs can match to machines with
volumes they need.

Optionally, if the directory name is two directories, separated by a
colon, the first directory is the name on the host machine, and the
second is the value inside the container. If a ":ro" is specified after
the second directory name, the volume will be mounted read-only inside
the container.

These directories will be bind-mounted unconditionally inside the
container. If an administrator wants to bind mount a directory only for
some jobs, perhaps only those submitted by some trusted user, the
setting :macro:`DOCKER_VOLUME_DIR_xxx_MOUNT_IF` may be used. This is a
class ad expression, evaluated in the context of the job ad and the
machine ad. Only when it evaluted to TRUE, is the volume mounted.
Extending the above example,

.. code-block:: condor-config

    DOCKER_VOLUMES = SOME_DIR, ANOTHER_DIR
    DOCKER_VOLUME_DIR_SOME_DIR = /path1
    DOCKER_VOLUME_DIR_SOME_DIR_MOUNT_IF = WantSomeDirMounted && Owner == "smith"
    DOCKER_VOLUME_DIR_ANOTHER_DIR = /path/to/no2
    DOCKER_MOUNT_VOLUMES = SOME_DIR, ANOTHER_DIR

In this case, the directory /path1 will get mounted inside the container
only for jobs owned by user "smith", and who set +WantSomeDirMounted =
true in their submit file.

In addition to installing the Docker service, the single configuration
variable :macro:`DOCKER` must be set. It defines the
location of the Docker CLI and can also specify that the
*condor_starter* daemon has been given a password-less sudo permission
to start the container as root. Details of the :macro:`DOCKER` configuration
variable are in the :ref:`admin-manual/configuration-macros:condor_startd
configuration file macros` section.

Docker must be installed as root by following these steps on an
Enterprise Linux machine.

#. Acquire and install the docker-engine community edition by following
   the installations instructions from docker.com
#. Set up the groups:

   .. code-block:: console

        $ usermod -aG docker condor

#. Invoke the docker software:

   .. code-block:: console

         $ systemctl start docker
         $ systemctl enable docker

#. Reconfigure the execute machine, such that it can set the machine
   ClassAd attribute ``HasDocker``:

   .. code-block:: console

         $ condor_reconfig

#. Check that the execute machine properly advertises that it is
   docker-capable with:

   .. code-block:: console

         $ condor_status -l | grep -i docker

   The output of this command line for a correctly-installed and
   docker-capable execute host will be similar to

   .. code-block:: condor-classad

        HasDocker = true
        DockerVersion = "Docker Version 1.6.0, build xxxxx/1.6.0"

By default, HTCondor will keep the 8 most recently used Docker images on the
local machine. This number may be controlled with the configuration variable
:macro:`DOCKER_IMAGE_CACHE_SIZE`, to increase or decrease the number of images,
and the corresponding disk space, used by Docker.

By default, Docker containers will be run with all rootly capabilities dropped,
and with setuid and setgid binaries disabled, for security reasons. If you need
to run containers with root privilege, you may set the configuration parameter
:macro:`DOCKER_DROP_ALL_CAPABILITIES` to an expression that evaluates to false.
This expression is evaluted in the context of the machine ad (my) and the job
ad (target).

Docker support an enormous number of command line options when creating
containers. While HTCondor tries to map as many useful options from submit
files and machine descriptions to command line options, an administrator may
want additional options passed to the docker container create command. To do
so, the parameter :macro:`DOCKER_EXTRA_ARGUMENTS` can be set, and condor will
append these to the docker container create command.

Docker universe jobs may fail to start on certain Linux machines when
SELinux is enabled. The symptom is a permission denied error when
reading or executing from the condor scratch directory. To fix this
problem, an administrator will need to run the following command as root
on the execute directories for all the startd machines:

.. code-block:: console

    $ chcon -Rt svirt_sandbox_file_t /var/lib/condor/execute


All docker universe jobs can request either host-based networking
or no networking at all.  The latter might be for security reasons.
If the worker node administrator has defined additional custom docker
networks, perhaps a VPN or other custom type, those networks can be
defined for HTCondor jobs to opt into with the docker_network_type
submit command.  Simple set

.. code-block:: condor-config

    DOCKER_NETWORKS = some_virtual_network, another_network


And these two networks will be advertised by the startd, and
jobs that request these network type will only match to machines
that support it.  Note that HTCondor cannot test the validity
of these networks, and merely trusts that the administrator has
correctly configured them.

To deal with a potentially user influencing option, there is an optional knob that
can be configured to adapt the ``--shm-size`` Docker container create argument
taking the machine's and job's classAds into account.
Exemplary, setting the ``/dev/shm`` size to half the requested memory is achieved by:

.. code-block:: condor-config

    DOCKER_SHM_SIZE = Memory * 1024 * 1024 / 2

or, using a user provided value ``DevShmSize`` if available and within the requested
memory limit:

.. code-block:: condor-config

    DOCKER_SHM_SIZE = ifThenElse(DevShmSize isnt Undefined && isInteger(DevShmSize) && int(DevShmSize) <= (Memory * 1024 * 1024), int(DevShmSize), 2 * 1024 * 1024 * 1024)

    
Note: ``Memory`` is in MB, thus it needs to be scaled to bytes.

Apptainer and Singularity Support
---------------------------------

:index:`Singularity<single: Singularity; installation>` :index:`Singularity`

Singularity (https://sylabs.io/singularity/) is a container runtime system
popular in scientific and HPC communities.  Apptainer (https://apptainer.org)
is an open source fork of Singularity that is API and CLI compatible with
singularity. HTCondor can run jobs inside Singularity containers either in a
transparent way, where the job does not know that it is being contained, or,
the HTCondor administrator can configure the HTCondor startd so that a job can
opt into running inside a container.  This allows the operating system that the
job sees to be different than the one on the host system, and provides more
isolation between processes running in one job and another.

.. note::
    Everything in this document that pertains to Singularity is also true for the
    Apptainer container runtime.


The decision to run a job inside Singularity ultimately resides on the worker
node, although it can delegate that to the job.

By default, jobs will not be run in Singularity.

For Singularity to work, the administrator must install Singularity
on the worker node.  The HTCondor startd will detect this installation
at startup.  When it detects a usable installation, it will
advertise two attributes in the slot ad:

.. code-block:: condor-config

       HasSingularity = true
       SingularityVersion = "singularity version 3.7.0-1.el7"

If the detected Singularity installation fails to run test containers
at startd startup, ``HasSingularity`` will be set to ``false``, and
the slot ad attribute ``SingularityOfflineReason`` will contain an error string.

HTCondor will run a job under Singularity when the startd configuration knob
:macro:`SINGULARITY_JOB` evaluates to true.  This is evaluated in the context of the
slot ad and the job ad.  If it evaluates to false or undefined, the job will
run as normal, without singularity.

When :macro:`SINGULARITY_JOB` evaluates to true, a second HTCondor knob is required
to name the singularity image that must be run, :macro:`SINGULARITY_IMAGE_EXPR`.
This also is evaluated in the context of the machine and the job ad, and must
evaluate to a string.  This image name is passed to the singularity exec
command, and can be any valid value for a singularity image name.  So, it
may be a path to file on a local file system that contains an singularity
image, in any format that singularity supports.  It may be a string that
begins with ``docker://``, and refer to an image located on docker hub,
or other repository.  It can begin with ``http://``, and refer to an image
to be fetched from an HTTP server.  In this case, singularity will fetch
the image into the job's scratch directory, convert it to a .sif file and
run it from there.  Note this may require the job to request more disk space
that it otherwise would need. It can be a relative path, in which
case it refers to a file in the scratch directory, so that the image
can be transferred by HTCondor's file transfer mechanism.

Here's the simplest possible configuration file.  It will force all
jobs on this machine to run under Singularity, and to use an image
that it located in the file system in the path ``/cvfms/cernvm-prod.cern.ch/cvm3``:

.. code-block:: condor-config

      # Forces _all_ jobs to run inside singularity.
      SINGULARITY_JOB = true

      # Forces all jobs to use the CernVM-based image.
      SINGULARITY_IMAGE_EXPR = "/cvmfs/cernvm-prod.cern.ch/cvm3"

Another common configuration is to allow the job to select whether
to run under Singularity, and if so, which image to use.  This looks like:

.. code-block:: condor-config

      SINGULARITY_JOB = !isUndefined(TARGET.SingularityImage)
      SINGULARITY_IMAGE_EXPR = TARGET.SingularityImage

Then, users would add the following to their submit file (note the
quoting):

.. code-block:: condor-submit

      +SingularityImage = "/cvmfs/cernvm-prod.cern.ch/cvm3"

or maybe

.. code-block:: condor-submit

      +SingularityImage = "docker://ubuntu:20"

By default, singularity will bind mount the scratch directory that
contains transferred input files, working files, and other per-job
information into the container, and make this the initial working
directory of the job.  Thus, file transfer for singularity jobs works
just like with vanilla universe jobs.  Any new files the job
writes to this directory will be copied back to the submit node,
just like any other sandbox, subject to transfer_output_files,
as in vanilla universe.

Assuming singularity is configured on the startd as described
above, A complete submit file that uses singularity might look like

.. code-block:: condor-submit

     executable = /usr/bin/sleep
     arguments = 30
     +SingularityImage = "docker://ubuntu"

     Requirements = HasSingularity

     Request_Disk = 1024
     Request_Memory = 1024
     Request_cpus = 1

     should_transfer_files = yes
     transfer_input_files = some_input
     when_to_transfer_output = on_exit

     log = log
     output = out.$(PROCESS)
     error = err.$(PROCESS)

     queue 1


HTCondor can also transfer the whole singularity image, just like
any other input file, and use that as the container image.  Given
a singularity image file in the file named "image" in the submit
directory, the submit file would look like:

.. code-block:: condor-submit

     executable = /usr/bin/sleep
     arguments = 30
     +SingularityImage = "image"

     Requirements = HasSingularity

     Request_Disk = 1024
     Request_Memory = 1024
     Request_cpus = 1

     should_transfer_files = yes
     transfer_input_files = image
     when_to_transfer_output = on_exit

     log = log
     output = out.$(PROCESS)
     error = err.$(PROCESS)

     queue 1


The administrator can optionally
specify additional directories to be bind mounted into the container.
For example, if there is some common shared input data located on a
machine, or on a shared file system, this directory can be bind-mounted
and be visible inside the container. This is controlled by the
configuration parameter :macro:`SINGULARITY_BIND_EXPR`. This is an expression,
which is evaluated in the context of the machine and job ads, and which
should evaluated to a string which contains a space separated list of
directories to mount.

So, to always bind mount a directory named /nfs into the image, and
administrator could set

.. code-block:: condor-config

     SINGULARITY_BIND_EXPR = "/nfs"

Or, if a trusted user is allowed to bind mount anything on the host, an
expression could be

.. code-block:: condor-config

      SINGULARITY_BIND_EXPR = (Target.Owner == "TrustedUser") ? SomeExpressionFromJob : ""

If the source directory for the bind mount is missing on the host machine,
HTCondor will skip that mount and run the job without it.  If the image is
an exploded file directory, and the target directory is missing inside
the image, and the configuration parameter :macro:`SINGULARITY_IGNORE_MISSING_BIND_TARGET`
is set to true (the default is false), then this mount attempt will also
be skipped.  Otherwise, the job will return an error when run.

In general, HTCondor will try to set as many Singularity command line
options as possible from settings in the machine ad and job ad, as
make sense.  For example, if the slot the job runs in is provisioned with GPUs,
perhaps in response to a ``request_GPUs`` line in the submit file, the
Singularity flag ``-nv`` will be passed to Singularity, which should make
the appropriate nvidia devices visible inside the container.
If the submit file requests environment variables to be set for the job,
HTCondor passes those through Singularity into the job.

Before the *condor_starter* runs a job with singularity, it first
runs singularity test on that image.  If no test is defined inside
the image, it runs ``/bin/sh /bin/true``.  If the test returns non-zero,
for example if the image is missing, or malformed, the job is put
on hold.  This is controlled by the condor knob
:macro:`SINGULARITY_RUN_TEST_BEFORE_JOB`, which defaults to true.

If an administrator wants to pass additional arguments to the singularity exec
command instead of the defaults used by HTCondor, several parameters exist to
do this - see the *condor_starter* configuration parameters that begin with the
prefix SINGULARITY in defined in section
:ref:`admin-manual/configuration-macros:condor_starter configuration file
entries`.  There you will find parameters to customize things such as the use
of PID namespaces, cache directory, and several other options.  However, should
an administrator need to customize Singularity behavior that HTCondor does not
currently support, the parameter :macro:`SINGULARITY_EXTRA_ARGUMENTS` allows
arbitrary additional parameters to be passed to the singularity exec command.
Note that this can be a classad expression, evaluated in the context of the
slot ad and the job ad, where the slot ad can be referenced via "MY.", and the
job ad via the "TARGET." reference.  In this way, the admin could set different
options for different kinds of jobs.  For example, to pass the ``-w`` argument,
to make the image writable, an administrator could set

.. code-block:: condor-config

    SINGULARITY_EXTRA_ARGUMENTS = "-w"

There are some rarely-used settings that some administrators may
need to set. By default, HTCondor looks for the Singularity runtime
in ``/usr/bin/singularity``, but this can be overridden with the SINGULARITY
parameter:

.. code-block:: condor-submit

      SINGULARITY = /opt/singularity/bin/singularity

By default, the initial working directory of the job will be the
scratch directory, just like a vanilla universe job.  This directory
probably doesn't exist in the image's file system.  Usually,
Singularity will be able to create this directory in the image, but
unprivileged versions of singularity with certain image types may
not be able to do so.  If this is the case, the current directory
on the inside of the container can be set via a knob.  This will
still map to the scratch directory outside the container.

.. code-block:: condor-config

      # Maps $_CONDOR_SCRATCH_DIR on the host to /srv inside the image.
      SINGULARITY_TARGET_DIR = /srv

If :macro:`SINGULARITY_TARGET_DIR` is not specified by the admin,
it may be specified in the job submit file via the submit command
``container_target_dir``.  If both are set, the config knob
version takes precedence.

When the HTCondor starter runs a job under Singularity, it always
prints to the log the exact command line used.  This can be helpful
for debugging or for the curious.  An example command line printed
to the StarterLog might look like the following:

.. code-block:: text

    About to exec /usr/bin/singularity -s exec -S /tmp -S /var/tmp --pwd /execute/dir_462373 -B /execute/dir_462373 --no-home -C /images/debian /execute/dir_462373/demo 3

In this example, no GPUs have been requested, so there is no ``-nv`` option.
:macro:`MOUNT_UNDER_SCRATCH` is set to the default of ``/tmp,/var/tmp``, so condor
translates those into ``-S`` (scratch directory) requests in the command line.
The ``--pwd`` is set to the scratch directory, ``-B`` bind mounts the scratch
directory with the same name on the inside of the container, and the
``-C`` option is set to contain all namespaces.  Then the image is named,
and the executable, which in this case has been transferred by HTCondor
into the scratch directory, and the job's argument (3).  Not visible
in the log are any environment variables that HTCondor is setting for the job.

All of the singularity container runtime's logging, warning and error messages
are written to the job's stderr.  This is an unfortunate aspect of the runtime
we hope to fix in the future.  By default, HTCondor passes "-s" (silent) to
the singularity runtime, so that the only messages it writes to the job's
stderr are fatal error messages.  If a worker node administrator needs more
debugging information, they can change the value of the worker node config
parameter :macro:`SINGULARITY_VERBOSITY` and set it to -d or -v to increase
the debugging level.

The VM Universe
---------------

:index:`virtual machines`
:index:`for the vm universe<single: for the vm universe; installation>`
:index:`set up for the vm universe<single: set up for the vm universe; universe>`

**vm** universe jobs may be executed on any execution site with
Xen (via *libvirt*) or KVM. To do this, HTCondor must be informed of
some details of the virtual machine installation, and the execution
machines must be configured correctly.

What follows is not a comprehensive list of the options that help set up
to use the **vm** universe; rather, it is intended to serve as a
starting point for those users interested in getting **vm** universe
jobs up and running quickly. Details of configuration variables are in
the :ref:`admin-manual/configuration-macros:configuration file entries relating
to virtual machines` section.

Begin by installing the virtualization package on all execute machines,
according to the vendor's instructions. We have successfully used
Xen and KVM.

For Xen, there are three things that must exist on an execute machine to
fully support **vm** universe jobs.

#. A Xen-enabled kernel must be running. This running Xen kernel acts as
   Dom0, in Xen terminology, under which all VMs are started, called
   DomUs Xen terminology.
#. The *libvirtd* daemon must be available, and *Xend* services must be
   running.
#. The *pygrub* program must be available, for execution of VMs whose
   disks contain the kernel they will run.

For KVM, there are two things that must exist on an execute machine to
fully support **vm** universe jobs.

#. The machine must have the KVM kernel module installed and running.
#. The *libvirtd* daemon must be installed and running.

Configuration is required to enable the execution of **vm** universe
jobs. The type of virtual machine that is installed on the execute
machine must be specified with the :macro:`VM_TYPE`
variable. For now, only one type can be utilized per machine. For
instance, the following tells HTCondor to use KVM:

.. code-block:: condor-config

    VM_TYPE = kvm

The location of the *condor_vm-gahp* and its log file must also be
specified on the execute machine. On a Windows installation, these
options would look like this:

.. code-block:: condor-config

    VM_GAHP_SERVER = $(SBIN)/condor_vm-gahp.exe
    VM_GAHP_LOG = $(LOG)/VMGahpLog

Xen-Specific and KVM-Specific Configuration
'''''''''''''''''''''''''''''''''''''''''''

Once the configuration options have been set, restart the
*condor_startd* daemon on that host. For example:

.. code-block:: console

    $ condor_restart -startd leovinus

The *condor_startd* daemon takes a few moments to exercise the VM
capabilities of the *condor_vm-gahp*, query its properties, and then
advertise the machine to the pool as VM-capable. If the set up
succeeded, then *condor_status* will reveal that the host is now
VM-capable by printing the VM type and the version number:

.. code-block:: console

    $ condor_status -vm leovinus

After a suitable amount of time, if this command does not give any
output, then the *condor_vm-gahp* is having difficulty executing the VM
software. The exact cause of the problem depends on the details of the
VM, the local installation, and a variety of other factors. We can offer
only limited advice on these matters:

For Xen and KVM, the **vm** universe is only available when root starts
HTCondor. This is a restriction currently imposed because root
privileges are required to create a virtual machine on top of a
Xen-enabled kernel. Specifically, root is needed to properly use the
*libvirt* utility that controls creation and management of Xen and KVM
guest virtual machines. This restriction may be lifted in future
versions, depending on features provided by the underlying tool
*libvirt*.

When a vm Universe Job Fails to Start
'''''''''''''''''''''''''''''''''''''

If a vm universe job should fail to launch, HTCondor will attempt to
distinguish between a problem with the user's job description, and a
problem with the virtual machine infrastructure of the matched machine.
If the problem is with the job, the job will go on hold with a reason
explaining the problem. If the problem is with the virtual machine
infrastructure, HTCondor will reschedule the job, and it will modify the
machine ClassAd to prevent any other vm universe job from matching. vm
universe configuration is not slot-specific, so this change is applied
to all slots.

When the problem is with the virtual machine infrastructure, these
machine ClassAd attributes are changed:

-  ``HasVM`` will be set to ``False``
-  ``VMOfflineReason`` will be set to a somewhat explanatory string
-  ``VMOfflineTime`` will be set to the time of the failure
-  ``OfflineUniverses`` will be adjusted to include ``"VM"`` and ``13``

Since *condor_submit* adds ``HasVM == True`` to a vm universe job's
requirements, no further vm universe jobs will match.

Once any problems with the infrastructure are fixed, to change the
machine ClassAd attributes such that the machine will once again match
to vm universe jobs, an administrator has three options. All have the
same effect of setting the machine ClassAd attributes to the correct
values such that the machine will not reject matches for vm universe
jobs.

#. Restart the *condor_startd* daemon.
#. Submit a vm universe job that explicitly matches the machine. When
   the job runs, the code detects the running job and causes the
   attributes related to the vm universe to be set indicating that vm
   universe jobs can match with this machine.
#. Run the command line tool *condor_update_machine_ad* to set
   machine ClassAd attribute ``HasVM`` to ``True``, and this will cause
   the other attributes related to the vm universe to be set indicating
   that vm universe jobs can match with this machine. See the
   *condor_update_machine_ad* manual page for examples and details.


condor_negotiator-Side Resource Consumption Policies
----------------------------------------------------

:index:`consumption policy`
:index:`negotiator-side resource consumption policy<single: negotiator-side resource consumption policy; partitionable slots>`

.. warning::
   Consumption policies are an experimental feature and may not work well
   in combination with other HTCondor features.


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
use :macro:`SLOT_WEIGHT` to assess the user usage
that will affect user priority by the number of cores allocated. Note
that the only attributes valid within the 
:macro:`SLOT_WEIGHT` expression are Cpus, Memory, and disk. This
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


Enforcing scratch disk usage with on-the-fly, HTCondor managed, per-job scratch filesystems.
--------------------------------------------------------------------------------------------
:index:`DISK usage`
:index:`per job scratch filesystem`

.. warning::
   The per job filesystem feature is a work in progress and not currently supported.


On Linux systems, when HTCondor is started as root, it optionally has the ability to create
a custom filesystem for the job's scratch directory.  This allows HTCondor to prevent the job
from using more scratch space than provisioned.  This also requires that the disk is managed
with the LVM disk management system.  Three HTCondor configuration knobs need to be set for
this to work, in addition to the above requirements:

.. code-block:: condor-config

    THINPOOL_VOLUME_GROUP_NAME = vgname
    THINPOOL_NAME = htcondor
    STARTD_ENFORCE_DISK_LIMITS = true


THINPOOL_VOLUME_GROUP_NAME is the name of an existing LVM volume group, with enough 
disk space to provision all the scratch directories for all running jobs on a worker node.
THINPOOL_NAME is the name of the logical volume that the scratch directory filesystems will
be created on in the volume group.  Finally, STARTD_ENFORCE_DISK_LIMITS is a boolean.  When
true, if a job fills up the filesystem created for it, the starter will put the job on hold
with the out of resources hold code (34).  This is the recommended value.  If false, should
the job fill the filesystem, writes will fail with ENOSPC, and it is up to the job to handle these errors
and exit with an appropriate code in every part of the job that writes to the filesystem, including
third party libraries.

Note that the ephemeral filesystem created for the job is private to the job, so the contents
of that filesystem are not visible outside the process hierarchy.  The administrator can use
the nsenter command to enter this namespace, if they need to inspect the job's sandbox.
As this filesystem will never live through a system reboot, it is mounted with mount options
that optimize for performance, not reliability, and may improve performance for I/O heavy
jobs.
