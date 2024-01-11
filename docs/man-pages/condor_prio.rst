      

*condor_prio*
==============

change priority of jobs in the HTCondor queue
:index:`condor_prio<single: condor_prio; HTCondor commands>`\ :index:`condor_prio command`

Synopsis
--------

**condor_prio** *-p* *priority* | *+value* |
*-value* [*-n* *schedd_name*] [**username** | **ClusterId** ]

Description
-----------

*condor_prio* changes the priority of one or more jobs in the HTCondor
queue. If the job identification is given by *cluster.process*,
*condor_prio* attempts to change the priority of the single job with
job ClassAd attributes :ad-attr:`ClusterId` and :ad-attr:`ProcId`. If described by
*cluster*, *condor_prio* attempts to change the priority of all
processes with the given :ad-attr:`ClusterId` job ClassAd attribute. If
*username* is specified, *condor_prio* attempts to change priority of
all jobs belonging to that user. For **-a**, *condor_prio* attempts to
change priority of all jobs in the queue.

The user must set a new priority with the **-p** option, or specify a
priority adjustment.

The priority of a job can be any integer, with higher numbers
corresponding to greater priority. For adjustment of the current
priority, *+value* increases the priority by the amount given with
*value*. *-value* decreases the priority by the amount given with
*value*.

Only the owner of a job or the super user can change the priority.

The priority changed by *condor_prio* is only used when comparing to
the priority jobs owned by the same user and submitted from the same
machine.

Options
-------

 **-a** 
    Change priority of all jobs in the queue
 **-n** *schedd_name*
    Change priority of jobs queued at the specified *condor_schedd* in
    the local pool.
 **-pool** *pool_name* **-n** *schedd_name*
    Change priority of jobs queued at the specified *condor_schedd* in
    the specified pool.

Exit Status
-----------

*condor_prio* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

