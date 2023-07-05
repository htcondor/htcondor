ALL_NODES Option
================

:index:`ALL_NODES option<single: DAG input file; ALL_NODES option>`

In the following commands, a specific node name can be replaced by the
option *ALL_NODES*:

-  **SCRIPT**
-  **PRE_SKIP**
-  **RETRY**
-  **ABORT-DAG-ON**
-  **VARS**
-  **PRIORITY**
-  **CATEGORY**

This will cause the given command to apply to all nodes (except any
FINAL node) in that DAG.

The ALL_NODES never applies to a FINAL node. If the *ALL_NODES*
option is used in a DAG that has a FINAL node, the ``dagman.out`` file
will contain messages noting that the FINAL node is skipped when parsing
the relevant commands.

The *ALL_NODES* option is case-insensitive.

It is important to note that the *ALL_NODES* option does not apply
across splices and sub-DAGs. In other words, an *ALL_NODES* option
within a splice or sub-DAG will apply only to nodes within that splice
or sub-DAG; also, an *ALL_NODES* option in a parent DAG will PRIORITY DAG (again,
except any FINAL node).

As of version 8.5.8, the *ALL_NODES* option cannot be used when
multiple DAG files are specified on the *condor_submit_dag* command
line. Hopefully this limitation will be fixed in a future release.

When multiple commands (whether using the *ALL_NODES* option or not)
set a given property of a DAG node, the last relevant command overrides
earlier commands, as shown in the following examples:

For example, in this DAG:

.. code-block:: condor-dagman

    JOB A node.sub
    VARS A name="A"
    VARS ALL_NODES name="X"

the value of *name* for node A will be "X".

In this DAG:

.. code-block:: condor-dagman

    JOB A node.sub
    VARS A name="A"
    VARS ALL_NODES name="X"
    VARS A name="foo"

the value of *name* for node A will be "foo".

Here is an example DAG using the *ALL_NODES* option:

.. code-block:: condor-dagman

    # File: all_ex.dag
    JOB A node.sub
    JOB B node.sub
    JOB C node.sub

    SCRIPT PRE ALL_NODES my_script $JOB

    VARS ALL_NODES name="$(JOB)"

    # This overrides the above VARS command for node B.
    VARS B name="nodeB"

    RETRY all_nodes 3
