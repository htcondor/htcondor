:index:`DAGMan Quick Reference<single: DAGMan; DAGMan Quick Reference>`

Quick Reference
===============

DAG Commands
------------

General
^^^^^^^

:dag-cmd-def:`INCLUDE` (see :ref:`Full Description<DAG Include cmd>`)
    Parse the provided file as if it was inline to the current file.

    .. code-block:: condor-dagman

        INCLUDE filename

:dag-cmd-def:`JOB` (see :ref:`Full Description<DAGMan JOB>`)
    Create a normal DAG node to execute a specified HTCondor job.

    .. code-block:: condor-dagman

        JOB NodeName SubmitDescription [DIR directory] [NOOP] [DONE]

:dag-cmd-def:`PARENT/CHILD` (see :ref:`Full Description<DAG node dependencies>`)
    Create dependencies between two or more DAG nodes.

    .. code-block:: condor-dagman

        PARENT ParentNodeName [ParentNodeName2 ... ] CHILD  ChildNodeName [ChildNodeName2 ... ]

:dag-cmd-def:`SPLICE` (see :ref:`Full Description<DAG splicing>`)
    Incorporate the specified DAG file into the structure of another DAG.

    .. code-block:: condor-dagman

        SPLICE SpliceName DagFileName [DIR directory]

:dag-cmd-def:`SUBDAG` (see :ref:`Full Description<subdag-external>`)
    Specify a DAG workflow to be submitted by :tool:`condor_submit_dag` and managed
    by a parent DAG.

    .. code-block:: condor-dagman

        SUBDAG EXTERNAL JobName DagFileName [DIR directory] [NOOP] [DONE]

:dag-cmd-def:`SUBMIT-DESCRIPTION` (see :ref:`Full Description<DAG submit description cmd>`)
    Create an inline job submit description that can be applied to multiple
    DAG nodes.

    .. code-block:: condor-dagman

        SUBMIT-DESCRIPTION DescriptionName {
            # submit attributes go here
        }

Node Behavior
^^^^^^^^^^^^^

:dag-cmd-def:`DONE`
    Mark a DAG node as done causing neither the associated job or scripts to execute.

    .. code-block:: condor-dagman

        DONE NodeName

:dag-cmd-def:`PRE_SKIP` (see :ref:`Full Description<Node pre skip cmd>`)
    Inform DAGMan to skip the remaining node execution if that nodes specified PRE
    script exits with a specified code.

    .. code-block:: condor-dagman

        PRE_SKIP <NodeName | ALL_NODES> non-zero-exit-code

:dag-cmd-def:`PRIORITY` (see :ref:`Full Description<DAG Node Priorities>`)
    Assign a node priority to control DAGMan node submission.

    .. code-block:: condor-dagman

        PRIORITY <NodeName | ALL_NODES> PriorityValue

:dag-cmd-def:`RETRY` (see :ref:`Full Description<Retry DAG Nodes>`)
    Inform DAGMan to retry a node up to a specified number of times when a failure
    occurs.

    .. code-block:: condor-dagman

        RETRY <NodeName | ALL_NODES> NumberOfRetries [UNLESS-EXIT value]

:dag-cmd-def:`SCRIPT` (see :ref:`Full Description<DAG Node Scripts>`)
    Apply a script to be executed on the AP for a specified node.

    .. code-block:: condor-dagman

        # PRE-Script
        SCRIPT [DEFER status time] [DEBUG filename type] PRE <NodeName | ALL_NODES> ExecutableName [arguments]
        # POST-Script
        SCRIPT [DEFER status time] [DEBUG filename type] POST <NodeName | ALL_NODES> ExecutableName [arguments]
        # HOLD-Script
        SCRIPT [DEFER status time] [DEBUG filename type] HOLD <NodeName | ALL_NODES> ExecutableName [arguments]

:dag-cmd-def:`VARS` (see :ref:`Full Description<DAGMan VARS>`)
    Specify a list of **key="Value"** pairs of information to be applied to the
    specified node as referable submit macros.

    .. code-block:: condor-dagman

        VARS <NodeName | ALL_NODES> [PREPEND | APPEND] macroname="string" [macroname2="string2" ... ]

Special Nodes
^^^^^^^^^^^^^

:dag-cmd-def:`FINAL` (see :ref:`Full Description<final-node>`)
    Create a DAG node guaranteed to run at the end of a DAG regardless
    of successful or failed execution.

    .. code-block:: condor-dagman

        FINAL NodeName SubmitDescription [DIR directory] [NOOP]

:dag-cmd-def:`PROVISIONER` (see :ref:`Full Description<DAG Provisioner Node>`)
    Create a DAG node responsible for provisioning resources to be utilized by other
    DAG nodes. Guaranteed to start before all other nodes.

    .. code-block:: condor-dagman

        PROVISIONER NodeName SubmitDescription

:dag-cmd-def:`SERVICE` (see :ref:`Full Description<DAG Service Node>`)
    Create a DAG node for specialized management/monitoring tasks. All service nodes
    are submitted prior to normal nodes.

    .. code-block:: condor-dagman

        SERVICE NodeName SubmitDescription

Throttling
^^^^^^^^^^

:dag-cmd-def:`CATEGORY` (see :ref:`Full Description<DAG throttling cmds>`)
    Assign a specified node to a DAG category.

    .. code-block:: condor-dagman

        CATEGORY <NodeName | ALL_NODES> CategoryName

:dag-cmd-def:`MAXJOBS` (see :ref:`Full Description<DAG throttling cmds>`)
    Set the max number of submitted set of jobs for a specified :dag-cmd:`CATEGORY`

    .. code-block:: condor-dagman

        MAXJOBS CategoryName MaxJobsValue

DAG Control
^^^^^^^^^^^

:dag-cmd-def:`ABORT-DAG-ON` (see :ref:`Full Description<abort-dag-on>`)
    Inform DAGMan to write a rescue file and exit when specified node exits with
    the specified value.

    .. code-block:: condor-dagman

        ABORT-DAG-ON <NodeName | ALL_NODES> AbortExitValue [RETURN DAGReturnValue]

:dag-cmd-def:`CONFIG` (see :ref:`Full Description<Per DAG Config>`)
    Specify custom DAGMan configuration file for the DAG.

    .. code-block:: condor-dagman

        CONFIG filename

:dag-cmd-def:`ENV` (see :ref:`Full Description<DAG ENV cmd>`)
    Modify the DAGMan proper job's environment by explicitly setting environment
    variables or filtering variables from the :tool:`condor_submit_dag`\ s environment
    at submit time.

    .. code-block:: condor-dagman

        ENV GET VAR-1 [VAR-2 ... ]
        #  or
        ENV SET Key=Value;Key=Value; ...

:dag-cmd-def:`SET_JOB_ATTR` (see :ref:`Full Description<DAG set-job-attrs>`)
    Set a ClassAd attribute in the DAGMan proper job's ad.

    .. code-block:: condor-dagman

        SET_JOB_ATTR AttributeName = AttributeValue

:dag-cmd-def:`REJECT`
    Mark the DAG input file as rejected to prevent execution.

    .. code-block:: condor-dagman

        REJECT

Special Files
^^^^^^^^^^^^^

:dag-cmd-def:`DOT` (see :ref:`Full Description<visualizing-dags-with-dot>`)
    Inform DAGMan to produce a Graphiz Dot file for visualizing a DAG.

    .. code-block:: condor-dagman

        DOT filename [UPDATE | DONT-UPDATE] [OVERWRITE | DONT-OVERWRITE] [INCLUDE <dot-file-header>]

:dag-cmd-def:`JOBSTATE_LOG` (see :ref:`Full Description<DAGMan Machine Readable History>`)
    Inform DAGMan to produce a machine-readable history file.

    .. code-block:: condor-dagman

        JOBSTATE_LOG filename

:dag-cmd-def:`NODE_STATUS_FILE` (see :ref:`Full Description<node-status-file>`)
    Inform DAGMan to produce a snapshot status file for the DAG nodes.

    .. code-block:: condor-dagman

        NODE_STATUS_FILE filename [minimumUpdateTime] [ALWAYS-UPDATE]

:dag-cmd-def:`SAVE_POINT_FILE` (see :ref:`Full Description<DAG Save Files>`)
    Inform DAGMan to write a save file the first time the specified node starts.

    .. code-block:: condor-dagman

        SAVE_POINT_FILE NodeName [Filename]

:index:`DAGMan Files<single: DAGMan; DAGMan Files>`

Produced Files
--------------

The following are always produced automatically by DAGMan on execution. Where the
primary DAG is the only or first DAG file specified at submit time.

#. :tool:`condor_dagman` scheduler universe job files:
    .. parsed-literal::

        <Primary DAG>.condor.sub | DAGMan proper jobs submit description file.
        <Primary DAG>.dagman.log | DAGMan proper jobs event :subcom:`log` file.
        <Primary DAG>.lib.out    | DAGMan proper jobs :subcom:`output` file.
        <Primary DAG>.lib.err    | DAGMan proper jobs :subcom:`error` file.

#. DAGMan informational files:
    .. parsed-literal::

        <Primary DAG>.dagman.out | DAGMan processes debug log file.
        <Primary DAG>.nodes.log  | Shared job event log file for all jobs managed by DAGMan (Heart of DAGMan).
        <Primary DAG>.metrics    | JSON formatted file containing DAGMan metrics outputted at DAGMan exit.
#. Other:
    .. parsed-literal::

        <Primary DAG>.rescue<XXX> | Rescue DAG file denoting completed work from previous execution (see :ref:`Rescue DAG`).

Referable DAG Information
-------------------------

DAGMan provides various pieces of DAG information to scripts and jobs in the
form of special referable macros and job ClassAd attributes.

Job Macros
^^^^^^^^^^

Macros referable by job submit description as ``$(<macro>)``

.. parsed-literal::

    **JOB**          | Name of the node this job is associated with.
    **RETRY**        | Current node retry attempt value. Set to 0 on first execution.
    **FAILED_COUNT** | Number of failed nodes currently in the DAG (intended for Final Node).
    **DAG_STATUS**   | Current :ad-attr:`DAG_Status` (intended for Final Node).

Job ClassAd Attributes
^^^^^^^^^^^^^^^^^^^^^^

ClassAd attributes added to the job ad of all jobs managed by DAGMan.

.. parsed-literal::

    :ad-attr:`DAGManJobId`        | Job-Id of the DAGMan job that submitted this job.
    :ad-attr:`DAGNodeName`        | The node name of which this job belongs.
    :ad-attr:`DAGManNodeRetry`    | The nodes current retry number. First execution is 0.\
     This is only included if :macro:`DAGMAN_NODE_RECORD_INFO` includes ``Retry``.
    :ad-attr:`DAGParentNodeNames` | List of parent node names. Note depending on the number\
     of parent nodes this may be left empty.
    :ad-attr:`DAG_Status`         | Current DAG status (Intended for Final Nodes).

Script Macros
^^^^^^^^^^^^^

Macros that can be passed to a script as optional arguments like ``$<macro>``

.. parsed-literal::

    For All Scripts:
        **JOB**               | Name of the node this script is associated with.
        **RETRY**             | The nodes current retry number. Set to 0 on first execution.
        **MAX_RETRIES**       | Maximum number of retries allowed for the node.
        **FAILED_COUNT**      | Current number of failed nodes in the DAG.
        **DAG_STATUS**        | The current :ad-attr:`DAG_Status`.
    Only for POST Scripts:
        **JOBID**             | The Job-Id of the job executed by node. It is the\
     :ad-attr:`ClusterId` and :ad-attr:`ProcId` of the last job in the set.
        **RETURN**            | The exit code of the first failed job in the set or 0 for a\
     successful job execution.
        **PRE_SCRIPT_RETURN** | Return value of the associated node's PRE Script.


DAG Submission and Management
-----------------------------

.. sidebar:: Tip for Querying All Jobs in a DAG

    When doing job queries to the AP queue or history, the constraint
    **-const "DAGManJobId==<DAG Job Id>"** can be used to return job
    ads for only the jobs submitted and managed by the specified DAG.

    **<DAG Job Id>** should be replaced with the :ad-attr:`ClusterId`
    of the DAGMan proper job.

For more in depth explanation of controlling a DAG see :ref:`DAG controls`

DAG Submission
^^^^^^^^^^^^^^

To submit a DAGMan workflow simply use :tool:`condor_submit_dag` an input file
describing the DAG.

.. code-block:: console

    $ condor_submit_dag diamond.dag

DAG Monitoring
^^^^^^^^^^^^^^

All the jobs managed by the DAG and the DAGMan proper job itself can be monitored
with the tools listed below. :tool:`condor_q` by default returns a condensed overview
of jobs managed by DAGMan currently in the queue. To see all jobs individually use
the **-nobatch** flag.

+-----------------------------+-----------------------------+-----------------------------+
| :tool:`condor_q`            | :tool:`condor_watch_q`      | :tool:`htcondor dag status` |
+-----------------------------+-----------------------------+-----------------------------+

Stopping a DAG
^^^^^^^^^^^^^^

Pause/Restart
    A DAG can temporarily be stopped by using :tool:`condor_hold` on the DAGMan
    proper job. To restart the DAG simply use :tool:`condor_release`.
Remove
    To remove a DAG simply use :tool:`condor_rm` on the DAGMan proper job.

