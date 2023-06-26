Node Priorities
===============

:index:`PRIORITY command<single: DAG input file; PRIORITY command>`
:index:`node priorities<single: DAGMan; Node priorities>`

Setting Priorities for Nodes
----------------------------

The *PRIORITY* command assigns a priority to a DAG node (and to the
HTCondor job(s) associated with the node). The syntax for *PRIORITY* is

.. code-block:: condor-dagman

    PRIORITY <JobName | ALL_NODES> PriorityValue

The priority value is an integer (which can be negative). A larger
numerical priority is better. The default priority is 0.

The node priority affects the order in which nodes that are ready (all
of their parent nodes have finished successfully) at the same time will
be submitted. The node priority also sets the node job's priority in the
queue (that is, its ``JobPrio`` attribute), which affects the order in
which jobs will be run once they are submitted (see
:ref:`users-manual/priorities-and-preemption:job priority` for more
information). The node priority only affects the
order of job submission within a given DAG; but once jobs are submitted,
their ``JobPrio`` value affects the order in which they will be run
relative to all jobs submitted by the same user.

Sub-DAGs can have priorities, just as "regular" nodes can. (The priority
of a sub-DAG will affect the priorities of its nodes: see "effective
node priorities" below.) Splices cannot be assigned a priority, but
individual nodes within a splice can be assigned priorities.

Note that node priority does not override the DAG dependencies. Also
note that node priorities are not guarantees of the relative order in
which nodes will be run, even among nodes that become ready at the same
time - so node priorities should not be used as a substitute for
parent/child dependencies. In other words, priorities should be used
when it is preferable, but not required, that some jobs run before
others. (The order in which jobs are run once they are submitted can be
affected by many things other than the job's priority; for example,
whether there are machines available in the pool that match the job's
requirements.)

PRE scripts can affect the order in which jobs run, so DAGs containing
PRE scripts may not submit the nodes in exact priority order, even if
doing so would satisfy the DAG dependencies.

Node priority is most relevant if node submission is throttled (via the
*-maxjobs* or *-maxidle* command-line arguments or the
:macro:`DAGMAN_MAX_JOBS_SUBMITTED` or :macro:`DAGMAN_MAX_JOBS_IDLE`
configuration variables), or if there are not enough resources in the pool to
immediately run all submitted node jobs. This is often the case for DAGs
with large numbers of "sibling" nodes, or DAGs running on heavily-loaded
pools.

**Example**

Adding *PRIORITY* for node C in the diamond-shaped DAG:

.. code-block:: condor-dagman

    # File name: diamond.dag

    JOB  A  A.condor
    JOB  B  B.condor
    JOB  C  C.condor
    JOB  D  D.condor
    PARENT A CHILD B C
    PARENT B C CHILD D
    RETRY  C 3
    PRIORITY C 1

This will cause node C to be submitted (and, mostly likely, run) before
node B. Without this priority setting for node C, node B would be
submitted first because the "JOB" statement for node B comes earlier in
the DAG file than the "JOB" statement for node C.

Effective node priorities
-------------------------

**The "effective" priority for a node (the priority controlling the order
in which nodes are actually submitted, and which is assigned to JobPrio)
is the sum of the explicit priority (specified in the DAG file) and the
priority of the DAG itself.** DAG priorities also default to 0, so they
are most relevant for sub-DAGs (although a top-level DAG can be submitted
with a non-zero priority by specifying a **-priority** value on the
*condor_submit_dag* command line). **This algorithm for calculating
effective priorities is a simplification introduced in version 8.5.7 (a
node's effective priority is no longer dependent on the priorities of
its parents).**

Here is an example to clarify:

.. code-block:: condor-dagman

    # File name: priorities.dag

    JOB A A.sub
    SUBDAG EXTERNAL B SD.dag
    PARENT A CHILD B
    PRIORITY A 60
    PRIORITY B 100

.. code-block:: condor-dagman

    # File name: SD.dag

    JOB SA SA.sub
    JOB SB SB.sub
    PARENT SA CHILD SB
    PRIORITY SA 10
    PRIORITY SB 20

In this example (assuming that priorities.dag is submitted with the
default priority of 0), the effective priority of node A will be 60, and
the effective priority of sub-DAG B will be 100. Therefore, the
effective priority of node SA will be 110 and the effective priority of
node SB will be 120.

The effective priorities listed above are assigned by DAGMan. There is
no way to change the priority in the submit description file for a job,
as DAGMan will override any
**priority** :index:`priority<single: priority; submit commands>` command placed
in a submit description file (unless the effective node priority is 0;
in this case, any priority specified in the submit file will take
effect).
