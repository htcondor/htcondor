DAG Manager Job Specifications
==============================

Some DAG file commands can be used to alter information about the
DAG manager job itself such as adding custom classad attributes and
setting information in the job environment.

:index:`SET_JOB_ATTR command<single: DAG input file; SET_JOB_ATTR command>`
:index:`Setting Classad Attributes in the DAGMan Job<single: DAGMan; Setting Classad Attributes in the DAGMan Job>`

Classad Attributes in the DAG Manager Job
'''''''''''''''''''''''''''''''''''''''''

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

:index:`ENV command<single: DAG input file; ENV command>`
:index:`Setting DAGMan job environment variables<single: DAGMan; Setting DAGMan job environment variables>`

Environment Variables in the DAG Manager Job
''''''''''''''''''''''''''''''''''''''''''''

The *ENV* keyword within the DAG input file can be used to specify
environment variables to set into the DAGMan jobs environment or get
from the environment that the DAGMan job was submitted from. It is
important to know that the environment variables in the DAG manager
jobs environment effect scripts and node jobs that rely environment
variables since scripts and node jobs are submitted from the DAGMan
jobs environment. The syntax is:

.. code-block:: condor-dagman

    ENV GET VAR-1 VAR-2 ... VAR-N
    #  or
    ENV SET Key=Value;Key=Value; ...

The *GET* keyword takes a list of environment variables names to be added
to the DAGMan jobs ``getenv`` command in the ``.condor.sub`` file for the DAG.

The *SET* keyword takes a semi-colon delimited list of **key=value** pairs of
information to add into DAGMan jobs ``environment`` command in the ``.condor.sub``
file for the DAG. These added **key=value** must follow the normal HTCondor
job environment rules.
