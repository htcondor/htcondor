*condor_dag_checker*
====================

Process provided DAG file(s) for syntactical and logical issues.

:index:`condor_dag_checker<double: condor_dag_checker; HTCondor commands>`

Synopsis
--------

**condor_dag_checker** [**-help**]

**condor_dag_checker** [*OPTIONS*] *DAG_File* [*DAG_File* ...]

**condor_dag_checker** [**-AllowIllegalChars**] [**-UseDagDir**] [**-[No]JoinNodes**]
[**-json** | **-Statistics**]

Description
-----------

Process provided DAG file(s) for any issues that would prevent the
creation and execution of a DAG. Issues include, but are not limited
to, incorrect DAG command syntax, referencing undefined nodes, and
cyclic dependencies.

Options
-------

 **-AllowIllegalChars**
    Allow use of illegal characters in node names.
 **-[No]JoinNodes**
    Enable/Disable use of join nodes in produced DAG. Enabled
    by default.
 **-UseDagDir**
    Switch into the DAG file's directory prior to parsing.
 **-json**
    Print results into JSON format.
 **-Statistics**
    Print statistics about the parsed DAG.

General Remarks
---------------

This tool does not currently verify the existence files specified
for various DAG components (i.e. scripts and node submit descriptions).

This tool does not process internal DAG files specified via the
:dag-cmd:`SUBDAG` command as they can be dynamically generated. To
verify both the root DAG file and any SubDAG files, simply specify
all DAG files on the command line:

.. code-block:: console

    $ condor_dag_checker root.dag sub-dag1.dag sub-dag2.dag sub-sub-dag.dag

This tool will process files specified by the :dag-cmd:`INCLUDE` and
:dag-cmd:`SPLICE` commands.

Exit Status
-----------

0  -  Success (No issues detected in DAG files(s))

1  -  Failure (Issue detected with DAG file(s))

2  -  Tool execution failure

Examples
--------

Example checking DAG file ``diamond.dag`` for issues:

.. code-block:: console

    $ condor_dag_checker diamond.dag

Example getting statistics about DAG file ``diamond.dag``:

.. code-block:: console

    $ condor_dag_checker -statistics diamond.dag

Example getting JSON formatted information about DAG file ``diamond.dag``:

.. code-block:: console

    $ condor_dag_checker -json diamond.dag

Example checking multiple DAG files for issues:

.. code-block:: console

    $ condor_dag_checker first.dag second.dag third.dag

Example use DAG file directories during processing:

.. code-block:: console

    $ condor_dag_checker -usedagdir subdir1/simple.dag subdir2/simple.dag

Example comparing DAG file with and without join nodes:

.. code-block:: console

    $ condor_dag_checker -stat -JoinNodes
    $ condor_dag_checker -stat -NoJoinNodes

See Also
--------

- None

Availability
------------

Linux, MacOS, Windows

Introduced in v24.10.1 of HTCondor
