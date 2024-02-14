Recipe: How to Fill a Pool Breadth-First
========================================

Some pool administrators prefer a policy where, when there are fewer jobs
than total cores in their pool, those jobs are "spread out" as much as
possible, so that each machine is running the fewest number of jobs.  If each
machine is identical, such a policy may result in better performance than one
in which each machine is "filled up" before assigning jobs to the next machine,
but may use more power to do so.

The main idea is to set the :macro:`NEGOTIATOR_PRE_JOB_RANK` expression in the
negotiator to prefer to give to the schedds machines that are already
running the fewest numbers of jobs.  We use :macro:`NEGOTIATOR_PRE_JOB_RANK`
instead of :macro:`NEGOTIATOR_POST_JOB_RANK` so that the job's ``RANK``
expression doesn't come into play.  If you trust your users to override this
policy, you could use :macro:`NEGOTIATOR_POST_JOB_RANK` instead.  Note that
because this policy happens in the negotiator, if :macro:`CLAIM_WORKLIFE` is
set to a high value, the schedds are free to reuse the slots they have been
assigned for some time, which may cause the policy to be out of balance for
the :macro:`CLAIM_WORKLIFE` duration.

For a Pool with Static Slots
----------------------------

HTCondor uses static slots by default.

.. code-block::

    NEGOTIATOR_PRE_JOB_RANK = isUndefined(RemoteOwner) * (- SlotId)

Changing this will require a ``condor_reconfig`` of the negotiator to take
effect.

For a Pool with Partitionable Slots
-----------------------------------

By default, when the condor negotiator matches a partitionable slot to a
job, it gives the whole partitionable slot to the schedd to use for as
many jobs as possible.  This creates a depth-first kind of pool filling,
one where jobs tend to be packed as much as possible on the fewest
number of machines.  The best way to turn this off is to disable the
schedd-side partitionable slot splitting.  The downside of this change
is that each machine can only match one new job per negotiation cycle.
Also, if ConsumptionPolicy has been enabled, this will also cause a
depth-first filling of machines.

On the schedd, make the following configuration change.
This requires a ``condor_reconfig`` of the schedd to take effect.

.. code-block::

    CLAIM_PARTITIONABLE_LEFTOVERS = false

Then on the central manager, make the following configuration change.
This requires a ``condor_reconfig`` of the negotiator to take effect.

.. code-block::

    NEGOTIATOR_DEPTH_FIRST = false
