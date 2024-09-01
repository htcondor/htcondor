Recipe: How to Fill a Pool Breadth-First
========================================

Some pool administrators prefer a policy where, when there are fewer jobs
than total cores in their pool, those jobs are "spread out" as much as
possible, so that each machine is running the fewest number of jobs.  If each
machine is identical, such a policy may result in better performance than one
in which each machine is "filled up" before assigning jobs to the next machine,
but may use more power to do so.

For a Pool with Partitionable Slots
-----------------------------------

HTCondor uses partitionable slots by default.

The following recipe assumes that
:ref:`consumption policies<consumption-policy>`
have not been enabled.

For efficiency reasons, HTCondor usually tries to match as many jobs to a
single machine as it can.  The main idea is to configure HTCondor to instead
to prefer to match as many machines it can, given the number of jobs available.
The downside of doing so is that each job can only match one new job per
negotiation cycle, so it could take a lot longer to get jobs started.

On the schedd, you need to unset :macro:`CLAIM_PARTITIONABLE_LEFTOVERS`.
Set by default, this macro tells the schedd to try to start as
many jobs as it can on each match given to it by the negotiator.  Since the
negotiator matches jobs to entire partitionable slots, that could be a
large number.

On the schedd, make the following configuration change.
This requires a ``condor_reconfig`` of the schedd to take effect.

.. code-block:: condor-config

    CLAIM_PARTITIONABLE_LEFTOVERS = false

On the central manager, you need to unset :macro:`NEGOTIATOR_DEPTH_FIRST`.
Set by default, this macro tells the negotiator to try and match as many
jobs as it can to the same slot.  Since the
negotiator matches jobs to entire partitionable slots, that could be a
large number.

On the central manager, make the following configuration change.
This requires a ``condor_reconfig`` of the negotiator to take effect.

.. code-block:: condor-config

    NEGOTIATOR_DEPTH_FIRST = false

For a Pool with Static Slots
----------------------------

If you've configured your pool with static slots, the situation is much
simpler.

The main idea is to set the :macro:`NEGOTIATOR_PRE_JOB_RANK` expression in the
negotiator to prefer to give to the schedds machines that are already
running the fewest numbers of jobs.  We use :macro:`NEGOTIATOR_PRE_JOB_RANK`
instead of :macro:`NEGOTIATOR_POST_JOB_RANK` so that the job's ``RANK``
expression doesn't come into play.  If you trust your users to override this
policy, you could use :macro:`NEGOTIATOR_POST_JOB_RANK` instead.  (Note that
because this policy happens in the negotiator, if :macro:`CLAIM_WORKLIFE` is
set to a high value, the schedds are free to reuse the slots they have been
assigned for some time, which may cause the policy to be out of balance for
the :macro:`CLAIM_WORKLIFE` duration.)

.. code-block:: condor-config

    NEGOTIATOR_PRE_JOB_RANK = isUndefined(RemoteOwner) * (- SlotId)

Changing this will require a ``condor_reconfig`` of the negotiator to take
effect.
