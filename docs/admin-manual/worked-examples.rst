From Policy to Configuration
============================

This section presents different English-language policies and describes
how to convert them into HTCondor configuration.  Options for different
implementations will be discussed.

Load-Balancing
--------------

In this scenario, your pool has more than enough resources to run the
available jobs (lucky you!).  However, those jobs will run faster if
their machines have more memory and/or fewer other processes contending
for disk i/o.  Your policy is therefore to spread the work out among
your machines as much as possible.

[FIXME: previous section should describe checklist for deciding where
to implement a policy.]

.. rubric:: Choice of Daemon

This is a global policy -- how many jobs a given machine should start
depends on how many machines and how many jobs there are -- which means
it needs to be implemented in the negotiator.  Since the policy is to
influence how HTCondor allocates resources, this makes sense.

.. rubric:: Configuration

Unfortunately, the manual section [FIXME: link] that explains explains the
negotiation cycle does not do so in enough detail to mention the
configuration macro ``NEGOTIATOR_PRE_JOB_RANK``, so you'd have to have
found this one by looking through the list of negotiator-specific
macros [FIXME: link].

When looking for matches, the negotiator checks every machine to make sure
it finds the best match.  Matching machines are normally ranked by the job's
preference (as determined  the ``rank`` attribute in the job ClassAd), but
``NEGOTIATOR_PRE_JOB_RANK`` supercedes this.  The following configuration
sorts all claimed slots first (which is a little unfortunate), then unclaimed
slots in slot ID order.

.. code-block:: condor-config

    NEGOTIATOR_PRE_JOB_RANK = isUndefined(RemoteOwner) * (- SlotId)

This will cause slots to fill in slot ID order.

.. rubric:: Caveats

* If you're using partitionable slots, you will need to change the
  configuration on each of your schedds, as well.  By default, to improve
  efficiency, when the negotiator matches a partitionable slot, it gives the
  whole thing to the schedd, which then starts as many jobs as will fit.
  This almost entirely undoes the effect of preferring lower-slot-ID
  matches.  To prevent the schedd from using the whole partitionable slot,
  add the following to the schedd configuration and restart it.

  ```CLAIM_PARTITIONABLE_LEFTOVERS = false```

  This will reduce how quickly you can fill up your pool, because each
  partitionable slot will only start one job per negotiation cycle.

* Likewise, schedds can reuse claims if ``CLAIM_WORKLIFE`` is nonzero;
  this prevents the negotiator from giving the schedd a slot on a
  less-busy machine.  The pool could therefore become unbalanced for
  up to the claim worklife.

Fast and Slow Machines
----------------------

In this scenario, some of your machines are noticeably faster than the
others, but this either doesn't show up in the existing machine ad
attributes, or you don't want to make your users change their jobs to
prefer the faster machines.  Your policy is therefore to pack jobs onto
these machines in preference to the others.

.. rubric:: Choice of Daemon

This is a global policy, which generally means that it needs to be
implemented in the negotiator.  Since the policy is to influence how
HTCondor allocates resources, this makes sense.

.. rubric:: Configuration

Unfortunately, the manual section [LINK that explains explains the negotiation
cycle does not do so in enough detail to mention the configuration macro
``NEGOTIATOR_PRE_JOB_RANK``, so you'd have to have found this one by looking
through the list of negotiator-specific macros [LINK].

When looking for matches, the negotiator checks every machine to make sure
it finds the best match.  Matching machines are normally ranked by the job's
preference (as determined  the ``rank`` attribute in the job ClassAd), but
``NEGOTIATOR_PRE_JOB_RANK`` supercedes this.  The following configuration
FIXME FIXME FIXME

.. code-block:: condor-config

    FIXME FIXME FIXME

.. rubric:: Caveats

This policy is only effective when it has more than one machine to
choose from, which may not happen often if the pool is small and
the job pressure large.

Reducing Slot Fragmentation
---------------------------

In this scenario, you have a bipartite workload, with a large number of jobs
requiring substantially more resources, a large number of jobs requiring
substantially fewer resources, and very few in between.  However, perhaps
because the smaller jobs are split among more users, as time goes by, the
pool's resources become increasingly fragmented, resulting in longer waits
for the larger jobs and reduced throughput overall, as you have to drain
(defragment) the pool more often.  You therefore want a policy which
minimizes fragmentation.

.. rubric:: Choice of Daemon

This is a global policy, which generally means that it needs to be
implemented in the negotiator.  Since the policy is to influence how
HTCondor allocates resources, this makes sense.

.. rubric:: Configuration

Unfortunately, the manual section [FIXME: link] that explains explains the
negotiation cycle does not do so in enough detail to mention the
configuration macro ``NEGOTIATOR_PRE_JOB_RANK``, so you'd have to have
found this one by looking through the list of negotiator-specific
macros [FIXME: link].

When looking for matches, the negotiator checks every machine to make sure
it finds the best match.  Matching machines are normally ranked by the job's
preference (as determined  the ``rank`` attribute in the job ClassAd), but
``NEGOTIATOR_PRE_JOB_RANK`` supercedes this.  The following configuration
sorts slots by the number of CPUs, fewest first:

.. code-block:: condor-config

    FIXME FIXME FIXME

This forces the negotiator to find smaller matches before larger ones,
which makes it less likely that matching a small job will fragment a
slot.

.. rubric:: Caveats

As usual for policies involving ``NEGOTIATOR_PRE_JOB_RANK``,
this policy is only effective when it has more than one machine to
choose from, which may not happen often if the pool is small and
the job pressure large.

.. rubric:: Alternative

The following sketches a complicated scheme to avoid this problem.  Please
skip it and go on to the next section unless you're and advanced user.

Fundamentally, the problem with the configuration above is that it can't
not match a small job to a larger slot, or even wait a while to see if a
smaller slot comes free (or a submitter with a larger job becomes higher
priority).  Of course, you could configure a fixed subset of your startds
to refuse to run single-core jobs, but that both gives up some flexibility
that might be necessary to maximize utilization, and means that how many
multi-core jobs can run depends on which specific startds are available
at the moment.  However, running your user jobs on multi-core glide-ins
solves both problems: because the glide-ins are jobs, they can run on any
machine in the pool; and because the glide-ins are startds, they can be
configured to shut themselves down after not getting claimed for a certain
amount of time without causing their resources to vanish out of the pool.
(You should also configure them to not start new jobs more than, say,
24 hours after they started, so as to give another submitter a chance at
the resources.)  You can use concurrency limits to manage how many glide-ins
of this type are running at a time, which may simplify the script used to
generate them.  Have each of these glide-ins advertise an attribute that
you use a submit transform to make all of your user's multi-core jobs
require, so that the negotiator only schedules the glide-ins on multi-core
slots.  If your multi-core submitters don't keep enough jobs in the queue
to keep the glide-ins matched, you may need to defragment when new glide-ins
arrive; you can trigger that from the script which watches the queue and
submits the glide-ins.  (If your jobs must run as the submitting user, you'll
need to submit user-specific glide-ins, which will cause a little more
friction.)

Job are Using too Much Memory
-----------------------------

In this scenario, user jobs are slowing to a crawl, or failing entirely,
because the execute machines are running out of memory.  To remedy this,
you want a policy where the the execute machine won't run more jobs than
will fit in its available RAM.  To be precise, you'd rather it not even
start a job that won't fit, because if you just kill it later, you've
wasted bandwidth getting it started and possibly time by preventing it
from starting somewhere else where might have finished.

.. rubric:: Choice of Daemon

This policy cares about a per-machine resource, so the startd seems like
the obvious daemon to implement this policy.

.. rubric:: Configuration

But how does the startd know how much memory a job is going to use.  In
theory, this is a very difficult question, but in practice, if you make
your users tell you, they'll usually be right.

HTCondor has first-class support for this, in the form of the submit
command ``request_memory`` [FIXME: link].   By default, setting
``request_memory`` ensures that the job never runs in a slot which does
not have at least that much memory allocated.  However, the default is not to
require ``request_memory`` to be set, nor does HTCondor by default prevent
jobs from using more memory than they requested.

So let's say the policy you want is "all jobs on my cluster must set
``request_memory``, and any jobs which uses more than ``request_memory``
should immediately be put on hold."  (Simply killing the job would allow it
run again somewhere else, and the user may not even notice.  Putting a job
on hold signals the user that their attention is required to deal with a
problem with their job.)  The execute nodes in your cluster can enforce
these policies.

.. code-block:: condor-config

    # Put jobs on hold if they exceed the amount of memory allocated to them.
    use policy : hold_if_memory_exceeded

    # Don't run jobs which don't request memory.
    START = $(START) && (TARGET.RequestMemory isnt undefined)

The hold-if-memory-exceeded policy is rather complicated and
explained elsewhere [FIXME: link].  The ``START`` [FIXME: link] expression
determines if an execute machine will run a job; in this case, it's set
to its previous value (``True``, by default) and the requirement that the
job (referred to as ``TARGET`` for no good reason) attribute ``RequestMemory``
is not undefined.  (Most but not all submit file commands turn into a single
job-ad attribute with a similar name, usually the same letters with all the
underscores removed.  Sadly, we don't publish a list of these pairs anywhere.)
The condition is ``isnt undefined`` because ``undefined`` is a special value,
but ``defined`` is not.  See the ClassAd man page [FIXME: link] and
reference manual [FIXME: link] for details.

.. rubric:: Caveats

You will probably want to investigate ``SUBMIT_REQUIREMENTS`` [FIXME: link]
before implementing this policy -- it will never hurt a job to have a defined
``request_memory``, but it may help the user to be told at submit time,
rather than execute time, that they forgot to set one.

FIXME: simple example #2
------------------------

Prioritize GPU jobs?  (so that cpu-bound jobs don't block them from the GPU)

FIXME: simple example #3
------------------------

Something about group quotas, including enforcement.

FIXME: other examples?
----------------------

- Partitionable slots and draining?

- Whether or not to preempt?

- Backfill?

- run non-GPU jobs on the GPU machines last?


HTCondor Condo Model
--------------------

At the University of Examples, a genetics research group, the computational
biochemistry group, and the college of agriculture are all running their
own compute clusters.  When these clusters are busy, they are very busy;
but they spend some fair amount of their time idle, because the users need
to spend some time thinking (or learning) between job submissions.  Of
course, they would all benefit from using each other's idle machines during
their own periods of busy-ness: this would reduce the time their own users
spend waiting for their jobs, and not cost them very much (the machines
being left powered on while waiting for work).

Each groups has its own concerns:

the genetics research group (GRG)
    Although the GRG does not deal with human genomes, and so they don't have
    to deal with privacy law, they definitely don't want anyone else to have
    access to their genomic data.

    The GRG's researchers also greatly benefit from having low-latency access
    to GPUs.  The submit short jobs to get quick feedback as they develop
    their software.  Because their production jobs are usually rather long,
    the GRG's users are worried about how long it will take for their own
    jobs to start if they allow non-GRG users onto their machines.

the computational biochemistry group (CBG)
    The CBG's jobs are heavily parallel.  They don't ever need more than
    one machine's worth of cores -- and are frequently happy with only
    half a machine -- but they're concerned that they won't benefit as
    much from the other groups' machines if the other groups are all
    running single-core jobs; and likewise, that running single-core
    jobs on their own machines will make it harder for their own jobs
    to get started promptly.

    The CBG's cluster is rather small for their needs, so they're
    particularly interested in what can be done to help them meet
    paper deadlines.

the college of agriculture (CoA)
    The CoA hasn't historically been a big user of computation, but
    that's starting to change.  As the largest group in this example,
    the CoA's pool has the most cores, but they're mostly opportunistic,
    only operating after office hours.  The CoA is interested in expanding
    its pool both to support its researchers and to enable new courses in
    agronomic computing.  Expanding the pool by collaborating with the
    GRG and CBG is very attractive from the standpoint of cost-efficiency.

    The CoA, like the GRG, has an interest in small low-latency jobs, but
    mostly for instructional purposes.  The CoA's researchers have also
    expressed concern about student software interfering with their
    research.  To help minimize FERPA concerns, the CoA has volunteered to
    make their contribution to the pool in the form of the personnel
    assigned to manage and maintain it.

Explicit Policies
!!!!!!!!!!!!!!!!!

The Condo Model
!!!!!!!!!!!!!!!

Other Options
!!!!!!!!!!!!!

.. admonition:: [FIXME]

    This section is important but my be better off called out as
    its own section somewhere; it assumes a lot of knowledge and isn't
    really written for the new-comers we expect to be reading it...

One of the primary advantages of the condo model is that the machine owners
get many of the benefits of administering the own machines and their own
pool without having to pay the overhead of actually doing so.  With modern
administrative tools, the primary personnel cost for administering a pool
is not size (number of cores) but complexity (number of different owners)
and rate of change (also related to the number of different owners).

The great disadvantage of the condo model is that it combines the complexity
of its constituent subgroups: policies within a group must become part of the
policy managing the groups overall.  Thus, as the complexity of the
constituent groups goes up, and as the number of different constituent groups
rises, the condo becomes harder to manage.

The obvious alternative is for the different groups to coordinate
between their independent pools.  This coordination can take many different
technical forms, but at the administrative level, coordination between pools
implies agreements about security but not necessarily about policy.  That is,
for two different HTCondor pools to communicate, they must agree on how to
authenticate and authorize, but having done so, they two pools may not
agree on anything else (and refuse to run each other's jobs).

.. rubric:: Flocking

Both startds and schedds can flock, although schedd flocking is more
common.  In either case, the daemon talks to one or more central managers
in one or more other pools.  In both cases, flocking means accepting
matches from a remote negotiator: the schedd does so directly, to run the
jobs in its queue, and the startd does so indirectly, but trusing the
remote collector with a copy of its capability.

The disadvantage of flocking is that the flocking daemon must be able to
communicate with multiple remote daemons: not just the remote collector,
but for the schedd, each possible remote startd; and for the startd, each
possible remote schedd.

The advantage of flocking is that the flocking daemon runs jobs exactly
as it does without flocking.  This means the daemon's administrator can
look in the same places and use the same tools to analyze problems or
monitor operations.  For the schedd, it means that the submitter monitors
and interacts with their job as usual, and the submmitter doesn't
need to be aware of flocking to benefit it from it.

Depending on how the promises made to the submitters vary between pools,
allowing flocking to happen by default may be a disadvantage.  (You can
instead use :ref:`admin-manual/policy-configuration:Job Transforms` to force jobs
submitted to your schedd require execution in the local pool by default,
and configure your startds to advertise the attribute which defines them
as the local pool.)

Jobs run via flocking will count against a submitter's usage (priority) and
quota.

.. rubric:: Explicit use of Condor-C

One of the :doc:`../grid-computing/index` job types, Condor-C allows the
submitter to specify in their job which pool should execute in the job submit
file.  This can be a benefit -- it allows the knowledgable user to
target the pool which will work best for their jobs -- but also a
drawback -- it prevents portable jobs from running on whichever pool
is available.

Requiring knowledgable submitters may be a benefit or a problem, depending
on your submitters.  [FIXME]  Additionally, using Condor-C means that some
job operations will not behave exactly as expected.

Administratively, setting up Condor-C requires only that the sending schedd
be able to talk to the receiving schedd.  However, because it involves
another piece of machinery (the grid manager), debugging problems becomes
harder.

Finally, a job running via Condor-C will not count against the submitter's
usage (priority) or quota.

.. rubric:: Using the Job Router

The :doc:`../grid-computing/job-router` can be used to automate the process
of transforming idle jobs into Condor-C jobs.  Jobs which fail remain idle
can continue to be transformed into different Condor-C jobs, so this solution
addresses the late-binding problem of using Condor-C explicitly.  It can also
remove the need for knowledegable users.  Like flocking, it defaults to
implicitly handling all idle jobs, but it can be configured only to transform
jobs which are idle and have signalled that it's OK to run them outside of
their local pool.  The other advantages and disadvantages are the same as
for explicitly using Condor-C.

Administratively, configuring a job router can be a non-trivial task.  It
also adds another place for problems to happen.  The other requirements
are identical to explicitly using Condor-C.

.. rubric:: Using Glide-Ins

A glide-in is a HTCondor startd run as job on another batch system, including
a different HTCondor pool.  As with flocking, such a startd becomes part of
your pool: submitters' jobs will accrue usage and use quota, but since it is
part of your pool, the user experience is identical... barring policies
which can't be implemented by the glide-in.

A glide-in's startd is limited in two major ways: first, it isn't running as
root.  That means it's up to the host batch system to decide if it can run
Docker jobs (usually not) and to use cgroups, for example.  It's also up
to the host batch system to decide how long the guest startd gets to run,
and what to do if its resource usage (including the resource usage of its
jobs) gets out of hand.  Note that the startd must be configured to respect
the resource limits imposed on it by the host batch system in order to be
effective.  Second, a glide-in's startd is running as a particular user and
can't change which one.  This allows for unwanted cross-talk between jobs
unless the startd only runs jobs from one user, which is of course less
efficient.  (On the other hand, it may make tracking who's actually using
the remote pool easier for the remote pool's administrator.)

Administratively, [FIXME].
