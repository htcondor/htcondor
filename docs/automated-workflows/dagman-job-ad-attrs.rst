Setting ClassAd attributes in the DAG file
==========================================

:index:`SET_JOB_ATTR command<single: DAG input file; SET_JOB_ATTR command>`
:index:`setting ClassAd attributes in a DAG<single: DAGMan; Setting ClassAd attributes in a DAG>`

The *SET_JOB_ATTR* keyword within the DAG input file specifies an
attribute/value pair to be set in the DAGMan proper job's ClassAd.
The syntax for *SET_JOB_ATTR* is

.. code-block:: condor-dagman

    SET_JOB_ATTR AttributeName = AttributeValue

As an example, if the DAG input file contains:

.. code-block:: condor-dagman

    SET_JOB_ATTR TestNumber = 17

the ClassAd of the DAGMan job itself will have an attribute
``TestNumber`` with the value ``17``.

The attribute set by the *SET_JOB_ATTR* command is set only in the
ClassAd of the DAGMan job itself - it is not propagated to node jobs of
the DAG.

Values with spaces can be set by surrounding the string containing a
space with single or double quotes. (Note that the quote marks
themselves will be part of the value.)

Only a single attribute/value pair can be specified per *SET_JOB_ATTR*
command. If the same attribute is specified multiple times in the DAG
(or in multiple DAGs run by the same DAGMan instance) the last-specified
value is the one that will be utilized. An attribute set in the DAG file
can be overridden by specifying

.. code-block:: text

    -append 'My.<attribute> = <value>'

on the *condor_submit_dag* command line.