Running and Managing DAGMan
===========================

Once once a workflow has been setup in a ``.dag`` file, all that
is left is to submit the prepared workflow. A key concept to understand
regarding the submission and management of a DAGMan workflow is
that the DAGMan process itself is ran as a HTCondor Scheduler universe
job that runs under the schedd on the AP (often referred to as the
DAGMan proper job) that will in turn manage and submit all the various
jobs and scripts defined in the workflow.

Basic DAG Controls
------------------

:index:`DAG submission<single: DAGMan; DAG submission>`

DAG Submission
^^^^^^^^^^^^^^

.. sidebar:: Example: Submitting Diamond DAG

    .. code-block:: console

        $ condor_submit_dag diamond.dag

To submit a DAG simply use :tool:`condor_submit_dag` with the DAG input file
from the current working directory that the DAG input file is stored. This
will automatically generate an HTCondor scheduler universe job submit file
to execute :tool:`condor_dagman` and submit this job to HTCondor. This file
generated submit file is named ``<DAG Input Filename>.condor.sub``. If desired,
the generated submit description file can be modified prior to job submission
by doing the following:

.. code-block:: console

    $ condor_submit_dag -no_submit diamond.dag
    $ vim diamond.dag.condor.sub
    $ condor_submit diamond.dag.condor.sub

Since the :tool:`condor_dagman` process is an actual HTCondor job, all jobs
managed by DAGMan are marked with the DAGMan proper jobs :ad-attr:`ClusterId`.
This value is set to the managed jobs ClassAd attribute :ad-attr:`DAGManJobId`.

.. warning::

    Do not submit the same DAG, with the same DAG input file, from the same
    working directory at the same time. This will cause unpredictable behavior
    and failures since both DAGMan jobs will attempt to use the same files to
    execute.

:index:`single submission of multiple, independent DAGs<single: DAGMan; Single submission of multiple, independent DAGs>`

Single Submission of Multiple, Independent DAGs
'''''''''''''''''''''''''''''''''''''''''''''''

.. sidebar:: Example: Submitting Multiple Independent DAGs

    .. code-block:: console

        $ condor_submit_dag A.dag B.dag C.dag

Multiple independent DAGs described in various DAG input files can be submitted
in a single instance of :tool:`condor_submit_dag` resulting in one :tool:`condor_dagman`
job managing all DAGs. This is done by internally combining all independent
DAGs into one large DAG with no inter-dependencies between the individual
DAGs. To avoid possible node name collisions when producing the large DAG,
DAGMan renames all the nodes. The renaming of nodes is controlled by
:macro:`DAGMAN_MUNGE_NODE_NAMES`.

When multiple DAGs are submitted like this, DAGMan sets the first DAG description
file provided on the command line as it's primary DAG file, and uses the primary
DAG file when writing various files such as the ``*.dagman.out``. In the case of
failure, DAGMan will produce a rescue file named ``<Primary DAG>_multi.rescue<XXX>``.
See :ref:`Rescue DAG` section for more information.

The success or failure of the independent DAGs is well defined. When
multiple, independent DAGs are submitted with a single command, the
success of the composite DAG is defined as the logical AND of the
success of each independent DAG, and failure is defined as the logical
OR of the failure of any of the independent DAGs.

:index:`DAG monitoring<single: DAGMan; DAG monitoring>`

DAG Monitoring
^^^^^^^^^^^^^^

After submission, the progress of the DAG can be monitored by looking at
the job event log file(s), observing the e-mail that job submission to
HTCondor causes, or by using :tool:`condor_q`. Using just :tool:`condor_q`
while a DAGMan workflow is running will display condensed information
regarding the overall workflow progress under the DAGMan proper job as follows:

.. code-block:: console

    $ condor_q
    $ OWNER   BATCH_NAME          SUBMITTED   DONE  RUN  IDLE  TOTAL  JOB_IDS
    $ Cole    diamond.dag+1024    1/1 12:34   1     2    -     4      1025.0 ... 1026.0

Using :tool:`condor_q` with the *-dag* and *-nobatch* flags will display information
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
recommended *-af* *<Attributes>* flags for :tool:`condor_q` in association with the
DAGMan proper Job Id.

.. code-block:: console

    $ condor_q <dagman-job-id> -af Attribute-1 ... Attribute-N
    $ condor_q -l <dagman-job-id>

A large amount of information about DAG progress and errors can be found in
the debug log file named ``<DAG Input File>.dagman.out``. This file should
be saved if errors occur. This file also doesn't get removed between DAG
new executions, and all logged messages are appended to the file.

:index:`DAG status in a job ClassAd<single: DAGMan; DAG status in a job ClassAd>`

Status Information for the DAG in a ClassAd
'''''''''''''''''''''''''''''''''''''''''''

.. sidebar:: View DAG Progress

    Get a detailed DAG status report via :tool:`htcondor dag status`:

    .. code-block:: console

        $ htcondor dag status <dagman-job-id>

    .. code-block:: text

        DAG 1024 [diamond.dag] has been running for 00:00:49
        DAG has submitted 3 job(s), of which:
                1 is submitted and waiting for resources.
                1 is running.
                1 has completed.
        DAG contains 4 node(s) total, of which:
            [#] 1 has completed.
            [=] 2 are running: 2 jobs.
            [-] 1 is waiting on other nodes to finish.
        DAG is running normally.
        [#########===================----------] DAG is 25.00% complete.

The :tool:`condor_dagman` job places information about its status in its ClassAd
as the following job ad attributes:

+-----------------+-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_Status`       | :ad-attr:`DAG_InRecovery`   |
| DAG Info        +-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_AdUpdateTime` |                             |
+-----------------+-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_NodesTotal`   | :ad-attr:`DAG_NodesDone`    |
|                 +-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_NodesPrerun`  | :ad-attr:`DAG_NodesPostrun` |
|                 +-----------------------------+-----------------------------+
| Node Info       | :ad-attr:`DAG_NodesReady`   | :ad-attr:`DAG_NodesUnready` |
|                 +-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_NodesFailed`  | :ad-attr:`DAG_NodesFutile`  |
|                 +-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_NodesQueued`  |                             |
+-----------------+-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_JobsSubmitted`| :ad-attr:`DAG_JobsCompleted`|
|                 +-----------------------------+-----------------------------+
| DAG Process Info| :ad-attr:`DAG_JobsIdle`     | :ad-attr:`DAG_JobsRunning`  |
|                 +-----------------------------+-----------------------------+
|                 | :ad-attr:`DAG_JobsHeld`     |                             |
+-----------------+-----------------------------+-----------------------------+

.. note::
    Most of this information is also available in the ``dagman.out`` file, and
    DAGMan updates these classad attributes every 2 minutes.

:index:`DAG removal<single: DAGMan; DAG removal>`

Removing a DAG
^^^^^^^^^^^^^^

.. sidebar:: Removing a DAG

    .. code-block:: console

        $ condor_q -nobatch
        -- Submitter: user.cs.wisc.edu : <128.105.175.125:36165> : user.cs.wisc.edu
         ID      OWNER          SUBMITTED     RUN_TIME ST PRI SIZE CMD
          9.0   taylor         10/12 11:47   0+00:01:32 R  0   8.7  condor_dagman -f ...
         11.0   taylor         10/12 11:48   0+00:00:00 I  0   3.6  B.exe

            2 jobs; 1 idle, 1 running, 0 held
        $ condor_rm 9.0

To remove a DAG simply use :tool:`condor_rm[Removing a DAG]` on the
:tool:`condor_dagman` job. This will remove both the DAGMan proper job
and all node jobs, including sub-DAGs, from the HTCondor queue.

A removed DAG will be considered failed unless the DAG has a FINAL node
that succeeds.

In the case where a machine is scheduled to go down, DAGMan will clean
up memory and exit. However, it will leave any submitted jobs in the
HTCondor queue.

:index:`suspending a running DAG<single: DAGMan; Suspending a running DAG>`

.. _Suspending a DAG:

Suspending a Running DAG
^^^^^^^^^^^^^^^^^^^^^^^^

It may be desired to temporarily suspend a running DAG. For example, the
load may be high on the access point, and therefore it is desired to
prevent DAGMan from submitting any more jobs until the load goes down.
There are two ways to suspend (and resume) a running DAG.

- Use :tool:`condor_hold[with DAGMan]`/:tool:`condor_release` on the :tool:`condor_dagman` job.

    After placing the :tool:`condor_dagman` job on hold, no new node jobs will
    be submitted, and no scripts will be run. Any node jobs already in the
    HTCondor queue will continue undisturbed. Any running PRE or POST scripts
    will be killed. If the :tool:`condor_dagman` job is left on hold, it will
    remain in the HTCondor queue after all of the currently running node jobs
    are finished. To resume the DAG, use :tool:`condor_release` on the
    :tool:`condor_dagman` job.

    .. note::

        While the :tool:`condor_dagman` job is on hold, no updates will
        be made to the ``*.dagman.out`` file.

- Use a DAG halt file.

    A DAG can be suspended by halting it with a halt file. This is a
    special file named ``<DAG Input Filename>.halt`` that DAGMan will
    periodically check exists. If found then the DAG enters the halted
    state where no PRE scripts are not run and node jobs stop being
    submitted. Running node jobs will continue undisturbed, POST scripts
    will run, and the ``*.dagman.out`` log will still be updated.

    Once all running node jobs and POST scripts have finished, DAGMan
    will write a Rescue DAG and exit.

    .. note::

        If a halt file exists at DAG submission time, it it removed.

.. warning::

    Neither :tool:`condor_hold` nor a DAG halt is propagated to sub-DAGS. In
    other word if a parent DAG is held or halted, any sub-DAGs will continue
    to submit node jobs. However, these effects are applied to DAG splices
    since they are merged into the parent DAG and are controlled by a single
    :tool:`condor_dagman` instance.

:index:`file paths in DAGs<single: DAGMan; File paths in DAGs>`

File Paths in DAGs
------------------

.. sidebar:: Example File Paths with DAGMan

    A DAG and its node job submit file in the
    same ``example`` directory. Once ran, ``A.out``
    and ``A.log`` are expected in the directory.

    .. code-block:: condor-dagman

        # sample.dag
        JOB A A.submit

    .. code-block:: condor-submit

        # A.submit
        executable = programA
        input      = A.in
        output     = A.out
        log        = A.log

    .. code-block:: text

        example/
        ├── A.input
        ├── A.submit
        ├── sample.dag
        └── programA

:tool:`condor_dagman` assumes all relative paths in a DAG input file and its
node job submit descriptions are relative to the current working directory
where :tool:`condor_submit_dag` was ran. Meaning all files declared in a DAG
or its jobs are expected to be found or will be written relative to the DAGs
working directory. All jobs will be submitted and all scripts will be ran
from the DAGs working directory.

For simple DAG structures this may be alright, but not for complex DAGs.
To help reduce confusion of where things run or files are written, the **JOB**
command takes an option keyword **DIR <path>**. This will cause DAGMan to submit
the node job and run the node scripts from the directory specified.

.. code-block:: condor-dagman

    JOB A A.submit DIR dirA

.. code-block:: text

    example/
    ├── sample.dag
    └── dirA
        ├── A.input
        ├── A.submit
        └── programA

If dealing with multiple independent DAGs separated into different directories
as described below then a single :tool:`condor_submit_dag` submission from the
parent directory will fail to successfully execute since all paths are now relative
to the parent directory.

.. sidebar:: Example Paths with Independent DAGs

    Given the directory structure on the left, the following
    will fail

    .. code-block:: console

          $ cd parent
          $ condor_submit_dag dag1/one.dag dag2/two.dag

    But using *-UseDagDir* will execute each individual DAG
    as intended

    .. code-block:: console

          $ cd parent
          $ condor_submit_dag -usedagdir dag1/one.dag dag2/two.dag

.. code-block:: text

    parent/
    ├── dag1
    │   ├── A.input
    │   ├── A.submit
    │   ├── one.dag
    │   └── programA
    └── dag2
        ├── B.input
        ├── B.submit
        ├── programB
        └── two.dag

Use the :tool:`condor_submit_dag` *-UseDagDir* flag to execute each individual
DAG in there relative directories. For this example, ``one.dag`` would run from
the ``dag1`` directory and ``two.dag`` would run from ``dag2``. All produced
DAGMan files will be relative to the primary DAG (first DAG specified on the
command line).

.. warning::

    Use of *-usedagdir* does not work in conjunction with a JOB command that
    specifies a working directory via the **DIR** keyword. Using both will be
    detected and generate an error.

:index:`large numbers of jobs<single: DAGMan; Large numbers of jobs>`

Managing Large Numbers of Jobs
------------------------------

DAGMan provides lots of useful mechanisms to help submit and manage large
numbers of jobs. This can be useful whether a DAG is structured via
dependencies or just a bag of loose jobs. Notable features of DAGMan are:

* Throttling
    Throttling limits the number of submitted jobs at any point in time.
* Retry of jobs that fail
    Automatically re-run a failed job to attempt a successful execution.
    For more information visit :ref:`Retry DAG Nodes`.
* Scripts associated with node jobs
    Perform simple tasks on the Access Point before and/or after a node
    jobs execution. For more information visit DAGMan :ref:`DAG Node Scripts`.

.. sidebar:: Example Large DAG Unique Submit File

    .. code-block:: condor-submit

        # Generated Submit: job2.sub
        executable = /path/to/executable
        log = job2.log
        input = job2.in
        output = job2.out
        arguments = "-file job2.out"
        request_cpus   = 1
        request_memory = 1024M
        request_disk   = 10240K
        queue

It is common for a large grouping of similar jobs to ran under a DAG. It
is also very common for some external program or script to produce these
large DAGs and needed files. There are generally two ways of organizing
DAGs with large number of jobs to manage:

#. Using a unique submit description for each job in the DAG
    In this setup, a single DAG input file containing ``n`` jobs with
    a unique submit description file (see right) for each node such as:

    .. code-block:: condor-dagman

        # Large DAG Example: sweep.dag w/ unique submit files
        JOB job0 job0.sub
        JOB job1 job1.sub
        JOB job2 job2.sub
        ...
        JOB job999 job999.sub

    The benefit of this method is the individual jobs can easily be submitted
    separately at any time but at the cost of producing ``n`` unique files
    that need to be stored and managed.

.. sidebar:: Example Large DAG Shared Submit File

    .. code-block:: condor-submit

        # Generic Submit: common.sub
        executable = /path/to/executable
        log = job$(runnumber).log
        input = job$(runnumber).in
        output = job$(runnumber).out
        arguments = "-file job$(runnumber).out"
        request_cpus   = 1
        request_memory = 1024M
        request_disk   = 10240K
        queue

#. Using a shared submit description file and :ref:`DAGMan VARS`
    In this setup, a single DAG input file containing ``n`` jobs share
    a single submit description (see right) and utilize custom macros
    added to each job for variance by DAGMan is described such as:

    .. code-block:: condor-dagman

        # Large DAG example: sweep.dag w/ shared submit file
        JOB job0 common.sub
        VARS job0 runnumber="0"
        JOB job1 common.sub
        VARS job1 runnumber="1"
        JOB job2 common.sub
        VARS job2 runnumber="2"
        ...
        JOB job999 common.sub
        VARS job999 runnumber="999"

    The benefit to this method is that less files need to be produced,
    stored, and managed at the cost of more complexity and a double in
    size to the DAG input file.

.. note::

    Even though DAGMan can assist with the management of large number of jobs,
    DAGs managing several thousands worth of jobs will produce lots of various
    files making directory traversal difficult. Consider how the directory structure
    should look for large DAGs prior to creating and running.

.. _DAGMan throttling:

DAGMan Throttling
^^^^^^^^^^^^^^^^^

To prevent possible overloading of the *condor_schedd* and resources on the
Access Point that :tool:`condor_dagman` executes on, DAGMan comes with built
in capabilities to help throttle/limit the load on the Access Point.

:index:`throttling<single: DAGMan; Throttling>`

Throttling at DAG Submission
''''''''''''''''''''''''''''

#. Total nodes/clusters:
    The total number of DAG nodes that can be submitted to the HTCondor queue at a time.
    This is specified either at submit time via :tool:`condor_submit_dag`\s **-maxjobs**
    option or via the configuration option :macro:`DAGMAN_MAX_JOBS_SUBMITTED`.
#. Idle procs:
    The total number of idle procs associated with jobs managed by DAGMan in the HTCondor
    queue at a time. If DAGMan submits a job that goes over this limit then DAGMan will
    wait until the number of idle procs under its management drops below this max value
    prior to submitting ready jobs. This is specified either at submit time via
    :tool:`condor_submit_dag`\s **-maxidle** option or via the configuration option
    :macro:`DAGMAN_MAX_JOBS_IDLE`.
#. PRE/POST script:
    The total number of PRE and POST scripts DAGMan will execute at a time on the
    Access Point. These limits can either be specified via :tool:`condor_submit_dag`\s
    **-maxpre** and **-maxpost** options or via the configuration options
    :macro:`DAGMAN_MAX_PRE_SCRIPTS` and :macro:`DAGMAN_MAX_POST_SCRIPTS`.

:index:`editing DAG throttles<single: DAGMan; Editing DAG throttles>`

Editing DAG Throttles
'''''''''''''''''''''

The following throttling properties of a running DAG can be changed after the workflow
has been started. The values of these properties are published in the :tool:`condor_dagman`
job ad; changing any of these properties using :tool:`condor_qedit` will also update
the internal DAGMan value.

.. sidebar:: Edit DAGMan Limits

    To edit one of these properties, use the :tool:`condor_qedit`
    tool with the job ID of the :tool:`condor_dagman`

    .. code-block:: console

        $ condor_qedit <dagman-job-id> DAGMan_MaxJobs 1000

Currently, you can change the following attributes:

+----------------------------------+-----------------------------------------------------+
| **Attribute Name**               | **Attribute Description**                           |
+----------------------------------+-----------------------------------------------------+
| :ad-attr:`DAGMan_MaxJobs`        | Maximum number of running jobs                      |
+----------------------------------+-----------------------------------------------------+
| :ad-attr:`DAGMan_MaxIdle`        | Maximum number of idle jobs                         |
+----------------------------------+-----------------------------------------------------+
| :ad-attr:`DAGMan_MaxPreScripts`  | Maximum number of running PRE scripts               |
+----------------------------------+-----------------------------------------------------+
| :ad-attr:`DAGMan_MaxPostScripts` | Maximum number of running POST scripts              |
+----------------------------------+-----------------------------------------------------+

:index:`CATEGORY command<single: DAG Commands; CATEGORY command>`
:index:`MAXJOBS command<single: DAG Commands; MAXJOBS command>`
:index:`throttling nodes by category<single: DAGMan; Throttling nodes by category>`

Throttling Nodes by Category
''''''''''''''''''''''''''''

.. sidebar:: Throttling by Category

    **CATEGORY** and **MAXJOBS** command syntax

    .. code-block:: condor-dagman

        CATEGORY <JobName | ALL_NODES> CategoryName

    .. code-block:: condor-dagman

        MAXJOBS CategoryName MaxJobsValue

    .. note::

        Category names cannot contain white space.
        Please see :ref:`DAG Splice Limitations` in association with categories.

DAGMan also allows the limiting of the number of running nodes (submitted job
clusters) within a DAG at a finer grained control with the **CATEGORY** and
**MAXJOBS** commands. The **CATEGORY** command will assign a DAG node to a
category that can be referenced by the **MAXJOBS** command to limit the number
of submitted job clusters on a per category basis.

If the number of submitted job clusters for a given category reaches the
limit, no further job clusters in that category will be submitted until
other job clusters within the category terminate. If **MAXJOBS** is not set
for a defined category, then there is no limit placed on the number of
submissions within that category.

The configuration variable :macro:`DAGMAN_MAX_JOBS_SUBMITTED` and the
:tool:`condor_submit_dag` *-maxjobs* command-line option are still enforced
if these *CATEGORY* and *MAXJOBS* throttles are used.

