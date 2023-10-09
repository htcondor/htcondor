.. _node-status-file:

Capturing the Status of Nodes in a File
=======================================

:index:`NODE_STATUS_FILE command<single: DAG input file; NODE_STATUS_FILE command>`
:index:`node status file<single: DAGMan; Node status file>`

DAGMan can capture the status of the overall DAG and all DAG nodes in a
node status file, such that the user or a script can monitor this
status. This file is periodically rewritten while the DAG runs. To
enable this feature, the DAG input file must contain a line with the
*NODE_STATUS_FILE* command.

The syntax for a *NODE_STATUS_FILE* command is

.. code-block:: condor-dagman

    NODE_STATUS_FILE statusFileName [minimumUpdateTime] [ALWAYS-UPDATE]

The status file is written on the machine on which the DAG is submitted;
its location is given by *statusFileName*, and it may be a full path and
file name.

The optional *minimumUpdateTime* specifies the minimum number of seconds
that must elapse between updates to the node status file. This setting
exists to avoid having DAGMan spend too much time writing the node
status file for very large DAGs. If no value is specified, this value
defaults to 60 seconds (as of version 8.5.8; previously, it defaulted to
0). The node status file can be updated at most once per
:macro:`DAGMAN_USER_LOG_SCAN_INTERVAL` no matter how small the
*minimumUpdateTime* value. Also, the node status file will be updated
when the DAG finishes, whether successfully or not, even if
*minimumUpdateTime* seconds have not elapsed since the last update.

Normally, the node status file is only updated if the status of some
nodes has changed since the last time the file was written. However, the
optional *ALWAYS-UPDATE* keyword specifies that the node status file
should be updated every time the minimum update time (and
:macro:`DAGMAN_USER_LOG_SCAN_INTERVAL`), has passed, even if no
nodes have changed status since the last time the file was updated. The
file will change slightly, because timestamps will be updated. For
performance reasons, large DAGs with approximately 10,000 or more nodes
are poor candidates for using the *ALWAYS-UPDATE* option.

As an example, if the DAG input file contains the line

.. code-block:: condor-dagman

    NODE_STATUS_FILE my.dag.status 30

the file ``my.dag.status`` will be rewritten at intervals of 30 seconds
or more.

This node status file is overwritten each time it is updated. Therefore,
it only holds information about the current status of each node; it does
not provide a history of the node status.


.. versionchanged:: 8.1.6

    HTCondor version 8.1.6 changes the format of the node status file.

The node status file is a collection of ClassAds in New ClassAd format.
There is one ClassAd for the overall status of the DAG, one ClassAd for
the status of each node, and one ClassAd with the time at which the node
status file was completed as well as the time of the next update.

Here is an example portion of a node status file:

.. code-block:: condor-classad

    [
      Type = "DagStatus";
      DagFiles = {
        "job_dagman_node_status.dag"
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
      Node = "C";
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
-  7 (STATUS_FUTILE): The node will never run because ancestor node failed.

An *ancestor* is a node that a another node depends on either directly or indirectly
through a chain of **PARENT/CHILD** relationships. For example, the **DAG** shown below
would result in node **G**'s *ancestors* to be nodes **A**, **B**, **D**, and **F**
because the **PARENT** to **CHILD** relationships appear as ``A & B -> D -> F -> G``

.. code-block:: text

    Example DAG Visualized
          A     B
          └──┬──┘
          C──┴──D
              E─┴─F
                  │
                  G


A *NODE_STATUS_FILE* command inside any splice is ignored. If multiple
DAG files are specified on the *condor_submit_dag* command line, and
more than one specifies a node status file, the first specification
takes precedence.
