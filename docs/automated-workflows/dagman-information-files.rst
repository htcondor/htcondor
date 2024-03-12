Informational Files
===================

:index:`workflow metrics<single: DAGMan; Workflow metrics>`

Workflow Metrics
----------------

.. sidebar:: Example Metrics File Contents

    .. code-block:: json

        {
            "client":"condor_dagman",
            "version":"23.5.0",
            "type":"metrics",
            "start_time":1375313459.603,
            "end_time":1375313491.498,
            "duration":31.895,
            "exitcode":1,
            "dagman_id":"26",
            "parent_dagman_id":"11",
            "rescue_dag_number":0,
            "jobs":4,
            "jobs_failed":1,
            "jobs_succeeded":3,
            "dag_jobs":0,
            "dag_jobs_failed":0,
            "dag_jobs_succeeded":0,
            "total_jobs":4,
            "total_jobs_run":4,
            "dag_status":2
        }

For every DAG, a JSON formatted metrics file named ``<DAG input file>.metrics``
is created when DAGMan exits. In a workflow with nested DAGs, each nested DAG
will create its own metrics file. The metrics file will contain the following
information:

-  ``client``: The name of the client workflow software (:tool:`condor_dagman`).
-  ``version``: The version of the client workflow software (:tool:`condor_dagman`).
-  ``type``: The type of data, ``"metrics"``.
-  ``start_time``: The start time of the client, in epoch seconds, with millisecond
   precision.
-  ``end_time``: The end time of the client, in epoch seconds, with millisecond
   precision.
-  ``duration``: The duration of the client, in seconds, with millisecond precision.
-  ``exitcode``: The :tool:`condor_dagman` exit code.
-  ``dagman_id``: The :tool:`condor_dagman` instances :ad-attr:`ClusterId` value.
-  ``parent_dagman_id``: The :ad-attr:`ClusterId` value of this DAGs parent
   :tool:`condor_dagman` instance; empty if this DAG is not a SUBDAG.
-  ``rescue_dag_number``: The number of the Rescue DAG being run; 0 if not running
   a Rescue DAG.
-  ``jobs``: The number of nodes in the DAG input file, not including SUBDAG nodes.
-  ``jobs_failed``: The number of failed nodes in the workflow, not including
   SUBDAG nodes.
-  ``jobs_succeeded``: The number of successful nodes in the workflow, not including
   SUBDAG nodes; this includes jobs that succeeded after retries.
-  ``dag_jobs``: The number of SUBDAG nodes in the DAG input file.
-  ``dag_jobs_failed``: The number of SUBDAG nodes that failed.
-  ``dag_jobs_succeeded``: The number of SUBDAG nodes that succeeded.
-  ``total_jobs``: The total number of jobs in the DAG input file.
-  ``total_jobs_run``: The total number of nodes executed in a DAG. It should be
   equal to ``jobs_succeeded + jobs_failed + dag_jobs_succeeded + dag_jobs_failed``.
-  ``dag_status``: The final :ad-attr:`DAG_Status` of the DAG.

If :macro:`DAGMAN_REPORT_GRAPH_METRICS[and DAGMan metrics file]` is set to True then the
additionally following metrics will be recorded:

-  ``graph_height``: The height of the DAG.
-  ``graph_width``: The width of the DAG.
-  ``graph_num_edges``: The number of edges (connections) in the DAG.
-  ``graph_num_vertices``: The number of vertices (nodes) in the DAG.

.. sidebar:: Sample Node Status File Contents

    .. code-block:: condor-classad

        [
          Type = "DagStatus";
          DagFiles = {
            "diamond.dag"
          };
          Timestamp = 1399674138;
          DagStatus = 3;
          NodesTotal = 12;
          NodesDone = 11;
          NodesPre = 0;
          NodesQueued = 1;
          NodesPost = 0;
          NodesReady = 0;
          NodesUnready = 0;
          NodesFailed = 0;
          JobProcsHeld = 0;
          JobProcsIdle = 1;
        ]
        [
          Type = "NodeStatus";
          Node = "A";
          NodeStatus = 5;
          StatusDetails = "";
          RetryCount = 0;
          JobProcsQueued = 0;
          JobProcsHeld = 0;
        ]
        ...
        [
          Type = "NodeStatus";
          Node = "D";
          NodeStatus = 3;
          StatusDetails = "idle";
          RetryCount = 0;
          JobProcsQueued = 1;
          JobProcsHeld = 0;
        ]
        [
          Type = "StatusEnd";
          EndTime = 1399674138;
          NextUpdate = 1399674141;
        ]

:index:`node status file<single: DAGMan; Node status file>`

.. _node-status-file:

Current Node Status File
------------------------

DAGMan has the option to write the DAG and its node statuses to a file
periodically. This is intended for a user or script to use for monitoring
the DAG. To have DAGMan write the node status file simply use the
**NODE_STATUS_FILE** commands syntax as follows:

.. code-block:: condor-dagman

    NODE_STATUS_FILE filename [minimumUpdateTime] [ALWAYS-UPDATE]

The node status file is a collection of ClassAds in New ClassAd format.
There is one ClassAd for the overall status of the DAG, one ClassAd for
the status of each node, and one ClassAd with the time at which the node
status file was completed as well as the time of the next update.

The status file may be updated once per :macro:`DAGMAN_USER_LOG_SCAN_INTERVAL[and the Node Status File]`
in combination with the optional *minimumUpdateTime* value which defaults
to 60 seconds. The status file is also updated a final time when the DAG
completes either successfully or not.

Normally the node status file is only updated if the status of some node
has changed since the last file update. If provided the optional
*ALWAYS-UPDATE* keyword then DAGMan will always update the status file
even if no nodes have changed status.

The following example would result the file ``my.dag.status`` that will be
rewritten with the current DAG status information at intervals of 30 seconds
or more:

.. code-block:: condor-dagman

    NODE_STATUS_FILE my.dag.status 30

Possible ``DagStatus`` and ``NodeStatus`` attribute values are:

-  0 (STATUS_NOT_READY): At least one parent has not yet finished or
   the node is a FINAL node.
-  1 (STATUS_READY): All parents have finished, but the node is not yet
   running.
-  2 (STATUS_PRERUN): The node's PRE script is running.
-  3 (STATUS_SUBMITTED): The node's HTCondor job(s) are in the queue.
-  4 (STATUS_POSTRUN): The node's POST script is running.
-  5 (STATUS_DONE): The node has completed successfully.
-  6 (STATUS_ERROR): The node has failed.
-  7 (STATUS_FUTILE): The node will never run because an ancestor node failed.

An *ancestor* is a node that a another node depends on either directly or indirectly
through a chain of **PARENT/CHILD** relationships. Provided the DAG visualized below,
node **G**'s *ancestors* are nodes **A**, **B**, **D**, and **F**.

.. mermaid::
    :align: center
    :caption: DAG Ancestor Tree Visualized

    flowchart LR
        A & B --> C & D
        D --> E & F
        F --> G

.. note::

    A *NODE_STATUS_FILE* command inside any splice is ignored, and if multiple
    DAG files are specified then the first specification takes precedence.

:index:`machine-readable event history<single: DAGMan; Machine-readable event history>`

.. _DAGMan Machine Readable History:

Machine-Readable Event History
------------------------------

DAGMan can produce a machine-readable history of events called the job state
log. This log was designed for use by the `Pegasus Workflow Management System <https://pegasus.isi.edu/>`_
which operates as a layer on top of DAGMan. The job state log can be used
to monitor the state of the DAGMan workflow. The job state log is produced
when the **JOBSTATE_LOG** command is declared with the following syntax:

.. code-block:: condor-dagman

    JOBSTATE_LOG filename

The job state log is a filtered and easily machine-readable version of the
``*.dagman.out`` debug log file. It contains all the node events and some
additional meta information. Unlike the node status file, the job state log
is appended to. Meaning it contains the entire DAG history rather than just
the current snapshot.

There are 5 line types in the job state log. Each line begins with a Unix
timestamp in the form of seconds since the Epoch. Fields within each line
are separated by a single space character.

#. **DAGMan Start**:
    A meta-event identifying the :tool:`condor_dagman` job start. Where
    **DAGJobId** is the :ad-attr:`ClusterId` and :ad-attr:`ProcId` of
    the DAGMan job.

    .. code-block:: text

        timestamp INTERNAL *** DAGMAN_STARTED DAGJobID ***

#. **DAGMan Exit**:
    A meta-event identifying the :tool:`condor_dagman` job exit. Where **ExitCode**
    is the DAGMan jobs exit code.

    .. code-block:: text

        timestamp INTERNAL *** DAGMAN_FINISHED ExitCode ***

#. **Recovery Started**:
    A meta-event identifying DAGMan has entered recovery mode. While in recovery, node
    events are only printed if they were not already printed prior to recovery mode
    start.

    .. code-block:: text

        timestamp INTERNAL *** RECOVERY_STARTED ***

#. **Recovery Finish/Failure**:
    A meta-event identifying DAGMan recovery mode completion or failure.

    .. code-block:: text

        timestamp INTERNAL *** RECOVERY_FINISHED ***
                           or
        timestamp INTERNAL *** RECOVERY_FAILURE ***

#. **Node Events**:
    A meta-event identifying job and script events of a specified node.

    .. code-block:: text

        timestamp NodeName EventName CondorID JobTag - SequenceNumber

    The *NodeName* is the DAG identifier for the node as specified by the **JOB**
    command.

    The *EventName* is one of the many defined event or meta-events
    as listed below:

    +---------------------+---------------------+---------------------+
    | PRE_SCRIPT_STARTED  | PRE_SCRIPT_SUCCESS  | PRE_SCRIPT_FAILURE  |
    +---------------------+---------------------+---------------------+
    | SUBMIT_FAILURE      | JOB_SUCCESS         | JOB_FAILURE         |
    +---------------------+---------------------+---------------------+
    | POST_SCRIPT_STARTED | POST_SCRIPT_SUCCESS | POST_SCRIPT_FAILURE |
    +---------------------+---------------------+---------------------+

    The *CondorId* is the node job's :ad-attr:`ClusterId` and :ad-attr:`ProcId`.
    Meta-events that take prior to successful job submission will not have an
    assigned *CondorId*.

    The *JobTag* is an externally defined tag to assist any workflow managers
    built on top of the job state log. *JobTag* defaults to the dash character
    (``-``) when no tag is specified. This is defined by setting the following
    custom job ad attributes in the job's submit description:

    .. code-block:: condor-submit

        +job_tag_name = "+job_tag_value"
        +job_tag_value = "<JobTag>"

    If utilizing Pegasus this can be bypassed by setting:

    .. code-block:: condor-submit

        +pegasus_site = "<JobTag>"

    The *SequenceNumber* is a monotonically-increasing number that represents
    each node run attempt due to retries or if the DAG is rerun from a rescue
    file.

Below is example contents of a job state log assuming *JobTag* was set to ``local``:

.. code-block:: text

    1292620511 INTERNAL *** DAGMAN_STARTED 4972.0 ***
    1292620523 NodeA PRE_SCRIPT_STARTED - local - 1
    1292620523 NodeA PRE_SCRIPT_SUCCESS - local - 1
    1292620525 NodeA SUBMIT 4973.0 local - 1
    1292620525 NodeA EXECUTE 4973.0 local - 1
    1292620526 NodeA JOB_TERMINATED 4973.0 local - 1
    1292620526 NodeA JOB_SUCCESS 0 local - 1
    1292620526 NodeA POST_SCRIPT_STARTED 4973.0 local - 1
    1292620531 NodeA POST_SCRIPT_TERMINATED 4973.0 local - 1
    1292620531 NodeA POST_SCRIPT_SUCCESS 4973.0 local - 1
    1292620535 INTERNAL *** DAGMAN_FINISHED 0 ***

.. note::

    Only one job state log can exist per DAGMan process. If multiple are declared
    then the first one found will take effect and the remainder will output a
    warning at parse time.

