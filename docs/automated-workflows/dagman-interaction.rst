Running and Managing DAGMan
===========================

Once once a workflow has been setup in a ``.dag`` file, all that
is left is to submit the prepared workflow. A key concept to understand
regarding the submission and management of a DAGMan workflow is
that the DAGMan process itself is ran as a HTCondor job (often referred
to as the DAGMan proper job) that will in turn manage and submit
all the various jobs and scripts defined in the workflow.

:index:`DAG submission<single: DAGMan; DAG submission>`

DAG Submission
--------------

A DAG is submitted using the tool *condor_submit_dag*. The manual
page for :doc:`/man-pages/condor_submit_dag` details the
command. The simplest of DAG submissions has the syntax

.. code-block:: console

    $ condor_submit_dag DAGInputFileName

and the current working directory contains the DAG input file.

The diamond-shaped DAG example may be submitted with

.. code-block:: console

    $ condor_submit_dag diamond.dag

Do not submit the same DAG, with same DAG input file, from within the
same directory, such that more than one of this same DAG is running at
the same time. It will fail in an unpredictable manner, as each instance
of this same DAG will attempt to use the same file to enforce
dependencies.

To increase robustness and guarantee recoverability, the
*condor_dagman* process is run as an HTCondor job. As such, it needs a
submit description file. *condor_submit_dag* generates this needed
submit description file, naming it by appending ``.condor.sub`` to the
name of the DAG input file. This submit description file may be edited
if the DAG is submitted with

.. code-block:: console

    $ condor_submit_dag -no_submit diamond.dag

causing *condor_submit_dag* to create the submit description file, but
not submit *condor_dagman* to HTCondor. To submit the DAG, once the
submit description file is edited, use

.. code-block:: console

    $ condor_submit diamond.dag.condor.sub

Since the *condor_dagman* process is an actual HTCondor job, the job
Cluster Id produced for this DAGMan proper job is used to help mark
all jobs ran by DAGMan. This is done by adding the job classad attribute 
``DAGManJobId`` for all submitted jobs to the produced Job Id.

:index:`DAG monitoring<single: DAGMan; DAG monitoring>`

DAG Monitoring
--------------

After submission, the progress of the DAG can be monitored by looking at
the job event log file(s), observing the e-mail that job submission to
HTCondor causes, or by using *condor_q*.

Using just *condor_q* while a DAGMan workflow is running will display
condensed information regarding the overall workflow progress under the
DAGMan proper job as follows:

.. code-block:: console

    $ condor_q
    $ OWNER   BATCH_NAME          SUBMITTED   DONE  RUN  IDLE  TOTAL  JOB_IDS
    $ Cole    diamond.dag+1024    1/1 12:34   1     2    -     4      1025.0 ... 1026.0

Using *condor_q* with the *-dag* and *-nobatch* flags will display information
about the DAGMan proper job and all jobs currently submitted/running as
part of the DAGMan workflow as follows:

.. code-block:: console

    $ condor_q -dag -nobatch
    $ ID       OWNER/NODENAME  SUBMITTED    RUN_TIME ST PRI SIZE CMD
    $ 1024.0   Cole            1/1 12:34  0+01:13:19 R  0   0.4  condor_dagman ...
    $ 1025.0    |-Node_B       1/1 13:44  0+00:03:19 R  0   0.4  diamond.sh ...
    $ 1026.0    |-Node_C       1/1 13:45  0+00:02:19 R  0   0.4  diamond.sh ...

In addition to basic job management, the DAGMan proper job holds a lot of extra
information within its job classad that can queried with the *-l* or the more
recommended *-af* *<Attributes>* flags for *condor_q* in association with the
DAGMan proper Job Id.

.. code-block:: console

    $ condor_q <dagman-job-id> -af Attribute-1 ... Attribute-N
    $ condor_q -l <dagman-job-id>

There is also a large amount of information logged in an extra file. The
name of this extra file is produced by appending ``.dagman.out`` to the
name of the DAG input file; for example, if the DAG input file is
``diamond.dag``, this extra file is named ``diamond.dag.dagman.out``. The 
``.dagman.out`` file is an important resource for debugging; save this
file if a problem occurs. The ``dagman.out`` is appended to, rather than
overwritten, with each new DAGMan run.

:index:`DAG status in a job ClassAd<single: DAGMan; DAG status in a job ClassAd>`

Status Information for the DAG in a ClassAd
'''''''''''''''''''''''''''''''''''''''''''

The *condor_dagman* job places information about the status of the DAG
into its own job ClassAd. The attributes are fully described in
:doc:`/classad-attributes/job-classad-attributes`. The attributes are

+-----------------+------------------+------------------+
|                 | DAG_Status       | DAG_InRecovery   |
| DAG Info        +------------------+------------------+
|                 | DAG_AdUpdateTime |                  |
+-----------------+------------------+------------------+
|                 | DAG_NodesTotal   | DAG_NodesDone    |
|                 +------------------+------------------+
|                 | DAG_NodesPrerun  | DAG_NodesPostrun |
|                 +------------------+------------------+
| Node Info       | DAG_NodesReady   | DAG_NodesUnready |
|                 +------------------+------------------+
|                 | DAG_NodesFailed  | DAG_NodesFutile  |
|                 +------------------+------------------+
|                 | DAG_NodesQueued  |                  |
+-----------------+------------------+------------------+
|                 | DAG_JobsSubmitted| DAG_JobsCompleted|
|                 +------------------+------------------+
| DAG Process Info| DAG_JobsIdle     | DAG_JobsRunning  |
|                 +------------------+------------------+
|                 | DAG_JobsHeld     |                  |
+-----------------+------------------+------------------+

Note that most of this information is also available in the
``dagman.out`` file.

:index:`editing a running DAG<single: DAGMan; Editing a running DAG>`

Editing a Running DAG
---------------------

Certain properties of a running DAG can be changed after the workflow has been
started. The values of these properties are published in the *condor_dagman*
job ad; changing any of these properties using *condor_qedit* will also update
the internal DAGMan value.

Currently, you can change the following attributes:

+-------------------------+-----------------------------------------------------+
| **Attribute Name**      | **Attribute Description**                           |
+-------------------------+-----------------------------------------------------+
| *DAGMan_MaxJobs*        | Maximum number of running jobs                      |
+-------------------------+-----------------------------------------------------+
| *DAGMan_MaxIdle*        | Maximum number of idle jobs                         |
+-------------------------+-----------------------------------------------------+
| *DAGMan_MaxPreScripts*  | Maximum number of running PRE scripts               |
+-------------------------+-----------------------------------------------------+
| *DAGMan_MaxPostScripts* | Maximum number of running POST scripts              |
+-------------------------+-----------------------------------------------------+

To edit one of these properties, use the *condor_qedit* tool with the job ID of
the *condor_dagman* job, for example:

.. code-block:: console

    $ condor_qedit <dagman-job-id> DAGMan_MaxJobs 1000

To view all the properties of a *condor_dagman* job:

.. code-block:: console

    $ condor_q -l <dagman-job-id> | grep DAG

:index:`DAG removal<single: DAGMan; DAG removal>`

Removing a DAG
--------------

To remove an entire DAG, consisting of the *condor_dagman* job, plus
any jobs submitted to HTCondor, remove the *condor_dagman* job by
running *condor_rm*. For example,

.. code-block:: console

    $ condor_q -nobatch
    -- Submitter: user.cs.wisc.edu : <128.105.175.125:36165> : user.cs.wisc.edu
     ID      OWNER          SUBMITTED     RUN_TIME ST PRI SIZE CMD
      9.0   taylor         10/12 11:47   0+00:01:32 R  0   8.7  condor_dagman -f ...
     11.0   taylor         10/12 11:48   0+00:00:00 I  0   3.6  B.out
     12.0   taylor         10/12 11:48   0+00:00:00 I  0   3.6  C.out

        3 jobs; 2 idle, 1 running, 0 held

    $ condor_rm 9.0

When a *condor_dagman* job is removed, all node jobs (including
sub-DAGs) of that *condor_dagman* will be removed by the
*condor_schedd*. As of version 8.5.8, the default is that
*condor_dagman* itself also removes the node jobs (to fix a race
condition that could result in "orphaned" node jobs). (The
*condor_schedd* has to remove the node jobs to deal with the case of
removing a *condor_dagman* job that has been held.)

The previous behavior of *condor_dagman* itself not removing the node
jobs can be restored by setting the :macro:`DAGMAN_REMOVE_NODE_JOBS`
configuration macro to ``False``. This will decrease the load on the
*condor_schedd*, at the cost of allowing the possibility of "orphaned"
node jobs.

A removed DAG will be considered failed unless the DAG has a FINAL node
that succeeds.

In the case where a machine is scheduled to go down, DAGMan will clean
up memory and exit. However, it will leave any submitted jobs in the
HTCondor queue.

:index:`suspending a running DAG<single: DAGMan; Suspending a running DAG>`

Suspending a Running DAG
------------------------

It may be desired to temporarily suspend a running DAG. For example, the
load may be high on the access point, and therefore it is desired to
prevent DAGMan from submitting any more jobs until the load goes down.
There are two ways to suspend (and resume) a running DAG.

-  Use *condor_hold*/*condor_release* on the *condor_dagman* job.

   After placing the *condor_dagman* job on hold, no new node jobs will
   be submitted, and no PRE or POST scripts will be run. Any node jobs
   already in the HTCondor queue will continue undisturbed. Any running
   PRE or POST scripts will be killed. If the *condor_dagman* job is
   left on hold, it will remain in the HTCondor queue after all of the
   currently running node jobs are finished. To resume the DAG, use
   *condor_release* on the *condor_dagman* job.

   Note that while the *condor_dagman* job is on hold, no updates will
   be made to the ``dagman.out`` file.

-  Use a DAG halt file.

   The second way of suspending a DAG uses the existence of a
   specially-named file to change the state of the DAG. When in this
   halted state, no PRE scripts will be run, and no node jobs will be
   submitted. Running node jobs will continue undisturbed. A halted DAG
   will still run POST scripts, and it will still update the
   ``dagman.out`` file. This differs from behavior of a DAG that is
   held. Furthermore, a halted DAG will not remain in the queue
   indefinitely; when all of the running node jobs have finished, DAGMan
   will create a Rescue DAG and exit.

   To resume a halted DAG, remove the halt file.

   The specially-named file must be placed in the same directory as the
   DAG input file. The naming is the same as the DAG input file
   concatenated with the string ``.halt``. For example, if the DAG input
   file is ``test1.dag``, then ``test1.dag.halt`` will be the required
   name of the halt file.

   As any DAG is first submitted with *condor_submit_dag*, a check is
   made for a halt file. If one exists, it is removed.

**Note that neither condor_hold nor a DAG halt is propagated to sub-DAGs.**
In other words, if you *condor_hold* or create a halt file for a
DAG that has sub-DAGs, any sub-DAGs that are already in the queue will
continue to submit node jobs.

A *condor_hold* or DAG halt does, however, apply to splices, because
they are merged into the parent DAG and controlled by a single
*condor_dagman* instance.
