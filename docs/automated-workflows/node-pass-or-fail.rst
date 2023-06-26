Node Success/Failure
====================

Progress towards completion of the DAG is based upon the success of the
nodes within the DAG. The success of a node is based upon the success of
the job(s), PRE script, and POST script. A job, PRE script, or POST
script with an exit value not equal to 0 is considered failed. **The
exit value of whatever component of the node was run last determines the
success or failure of the node.**

Table 2.1 lists the definition of node success and failure for all variations
of script and job success and failure, when :macro:`DAGMAN_ALWAYS_RUN_POST` is set
to ``False``. In this table, a dash (``-``) represents the case where a script
does not exist for the DAG, **S** represents success, and **F** represents
failure.

+-----+-----------+-----------+-------+
| PRE | JOB       | POST      | Node  |
+=====+===========+===========+=======+
| \-  | S         | \-        | **S** |
+-----+-----------+-----------+-------+
| \-  | F         | \-        | **F** |
+-----+-----------+-----------+-------+
| \-  | S         | S         | **S** |
+-----+-----------+-----------+-------+
| \-  | S         | F         | **F** |
+-----+-----------+-----------+-------+
| \-  | F         | S         | **S** |
+-----+-----------+-----------+-------+
| \-  | F         | F         | **F** |
+-----+-----------+-----------+-------+
| S   | S         | \-        | **S** |
+-----+-----------+-----------+-------+
| S   | F         | \-        | **F** |
+-----+-----------+-----------+-------+
| S   | S         | S         | **S** |
+-----+-----------+-----------+-------+
| S   | S         | F         | **F** |
+-----+-----------+-----------+-------+
| S   | F         | S         | **S** |
+-----+-----------+-----------+-------+
| S   | F         | F         | **F** |
+-----+-----------+-----------+-------+
| F   | not run   | \-        | **F** |
+-----+-----------+-----------+-------+
| F   | not run   | not run   | **F** |
+-----+-----------+-----------+-------+

Table 2.1: Node **S**\ uccess or **F**\ ailure definition with
``DAGMAN_ALWAYS_RUN_POST = False`` (the default).

Table 2.2 lists the definition of node success and failure only for the cases
where the PRE script fails, when :macro:`DAGMAN_ALWAYS_RUN_POST` is set to ``True``.

+-----+-----------+--------+-------+
| PRE | JOB       | POST   | Node  |
+=====+===========+========+=======+
| F   | not run   | \-     | **F** |
+-----+-----------+--------+-------+
| F   | not run   | S      | **S** |
+-----+-----------+--------+-------+
| F   | not run   | F      | **F** |
+-----+-----------+--------+-------+

Table 2.2: Node **S**\ uccess or **F**\ ailure definition with
``DAGMAN_ALWAYS_RUN_POST = True``.

:index:`PRE_SKIP command<single: DAG input file; PRE_SKIP command>`
:index:`skipping node execution<single: DAGMan; Skipping node execution>`

PRE_SKIP
--------

The behavior of DAGMan with respect to node success or failure can be
changed with the addition of a *PRE_SKIP* command. A *PRE_SKIP* line
within the DAG input file uses the syntax:

.. code-block:: condor-dagman

    PRE_SKIP <JobName | ALL_NODES> non-zero-exit-code

The PRE script of a node identified by *JobName* that exits with the
value given by *non-zero-exit-code* skips the remainder of the node
entirely. Neither the job associated with the node nor the POST script
will be executed, and the node will be marked as successful.

:index:`RETRY command<single: DAG input file; RETRY command>`
:index:`retrying failed nodes<single: DAGMan; Retrying failed nodes>`

Retrying Failed Nodes
---------------------

DAGMan can retry any failed node in a DAG by specifying the node in the
DAG input file with the *RETRY* command. The use of retry is optional.
The syntax for retry is

.. code-block:: condor-dagman

    RETRY <JobName | ALL_NODES> NumberOfRetries [UNLESS-EXIT value]

where *JobName* identifies the node. *NumberOfRetries* is an integer
number of times to retry the node after failure. The implied number of
retries for any node is 0, the same as not having a retry line in the
file. Retry is implemented on nodes, not parts of a node.

The diamond-shaped DAG example may be modified to retry node C:

.. code-block:: condor-dagman

        # File name: diamond.dag
    
        JOB  A  A.condor
        JOB  B  B.condor
        JOB  C  C.condor
        JOB  D  D.condor
        PARENT A CHILD B C
        PARENT B C CHILD D
        RETRY  C 3

If node C is marked as failed for any reason, then it is started over as
a first retry. The node will be tried a second and third time, if it
continues to fail. If the node is marked as successful, then further
retries do not occur.

Retry of a node may be short circuited using the optional keyword
*UNLESS-EXIT*, followed by an integer exit value. If the node exits with
the specified integer exit value, then no further processing will be
done on the node.

The macro ``$(RETRY)`` evaluates to an integer value, set to 0 first time
a node is run, and is incremented each time for each time the node is
retried. The macro ``$(MAX_RETRIES)`` is the value set for
*NumberOfRetries*. These macros may be used as arguments passed to a PRE
or POST script.

.. _abort-dag-on:

:index:`ABORT-DAG-ON command<single: DAG input file; ABORT-DAG-ON command>`
:index:`aborting a DAG<single: DAGMan; Aborting a DAG>`

Stopping the DAG on Node Failure
--------------------------------

The *ABORT-DAG-ON* command provides a way to abort the entire DAG if a
given node returns a specific exit code. The syntax for *ABORT-DAG-ON*
is

.. code-block:: condor-dagman

    ABORT-DAG-ON <JobName | ALL_NODES> AbortExitValue [RETURN DAGReturnValue]

If the return value of the node specified by *JobName* matches
*AbortExitValue*, the DAG is immediately aborted. A DAG abort differs
from a node failure, in that a DAG abort causes all nodes within the DAG
to be stopped immediately. This includes removing the jobs in nodes that
are currently running. A node failure differs, as it would allow the DAG
to continue running, until no more progress can be made due to
dependencies.

The behavior differs based on the existence of PRE and/or POST scripts.
If a PRE script returns the *AbortExitValue* value, the DAG is
immediately aborted. If the HTCondor job within a node returns the
*AbortExitValue* value, the DAG is aborted if the node has no POST
script. If the POST script returns the *AbortExitValue* value, the DAG
is aborted.

An abort overrides node retries. If a node returns the abort exit value,
the DAG is aborted, even if the node has retry specified.

When a DAG aborts, by default it exits with the node return value that
caused the abort. This can be changed by using the optional *RETURN*
keyword along with specifying the desired *DAGReturnValue*. The DAG
abort return value can be used for DAGs within DAGs, allowing an inner
DAG to cause an abort of an outer DAG.

A DAG return value other than 0, 1, or 2 will cause the *condor_dagman*
job to stay in the queue after it exits and get retried, unless the
``on_exit_remove`` expression in the ``.condor.sub`` file is manually
modified.

Adding *ABORT-DAG-ON* for node C in the diamond-shaped DAG

.. code-block:: condor-dagman

        # File name: diamond.dag
    
        JOB  A  A.condor
        JOB  B  B.condor
        JOB  C  C.condor
        JOB  D  D.condor
        PARENT A CHILD B C
        PARENT B C CHILD D
        RETRY  C 3
        ABORT-DAG-ON C 10 RETURN 1

causes the DAG to be aborted, if node C exits with a return value of 10.
Any other currently running nodes, of which only node B is a possibility
for this particular example, are stopped and removed. If this abort
occurs, the return value for the DAG is 1.
