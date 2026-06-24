*condor_prio*
=============

Change priority of jobs in the HTCondor queue.

:index:`condor_prio<double: condor_prio; HTCondor commands>`

Synopsis
--------

**condor_prio** **-p** *priority* | **+***value* | **-***value* [**-n** *schedd-name*] [**-pool** *pool-name*] [**-a** | *cluster* | *cluster.proc* | *username*]

Description
-----------

*condor_prio* changes the priority of one or more jobs in the HTCondor
queue. If the job identification is given by *cluster.proc*,
*condor_prio* attempts to change the priority of the single job with
job ClassAd attributes :ad-attr:`ClusterId` and :ad-attr:`ProcId`. If described by
*cluster*, *condor_prio* attempts to change the priority of all
processes with the given :ad-attr:`ClusterId` job ClassAd attribute. If
*username* is specified, *condor_prio* attempts to change priority of
all jobs belonging to that user. For **-a**, *condor_prio* attempts to
change priority of all jobs in the queue.

The user must set a new priority with the **-p** option, or specify a
priority adjustment using **+value** or **-value**.

The priority of a job can be any integer, with higher numbers
corresponding to greater priority. For adjustment of the current
priority, **+value** increases the priority by the amount given with
*value*. **-value** decreases the priority by the amount given with
*value*.

Only the owner of a job or the super user can change the priority.

The priority changed by *condor_prio* is only used when comparing to
the priority of jobs owned by the same user and submitted from the same
machine.

Options
-------

 **-p** *priority*
    Set the priority to the specified integer value.
 **+value**
    Increase the priority by the specified amount.
 **-value**
    Decrease the priority by the specified amount.
 **-a**
    Change priority of all jobs in the queue.
 **-n** *schedd-name*
    Change priority of jobs queued at the specified *condor_schedd* in
    the local pool.
 **-pool** *pool-name*
    Specify the pool containing the *condor_schedd*.
 *cluster*
    Change priority for all jobs in the specified cluster.
 *cluster.proc*
    Change priority for a specific job.
 *username*
    Change priority for all jobs belonging to the specified user.

Examples
--------

Set job 123.0 to priority 10:

.. code-block:: console

    $ condor_prio -p 10 123.0

Increase priority of all jobs in cluster 456 by 5:

.. code-block:: console

    $ condor_prio +5 456

Decrease priority of all jobs belonging to user alice by 3:

.. code-block:: console

    $ condor_prio -3 alice

Exit Status
-----------

0  -  Success

1  -  Failure

See Also
--------

:tool:`condor_submit`, :tool:`condor_q`, :tool:`condor_userprio`, :tool:`condor_now`

Availability
------------

Linux, MacOS, Windows

