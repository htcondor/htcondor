*condor_dag_checker*
====================

Process provided DAG file(s) for syntactical and logical issues.

:index:`condor_dag_checker<double: condor_dag_checker; HTCondor commands>`

Synopsis
--------

**condor_dag_checker** [**-help**]

**condor_dag_checker** [*OPTIONS*] *DAG_File* [*DAG_File* ...]

**condor_dag_checker** [**-AllowIllegalChars**] [**-UseDagDir**] [**-[No]JoinNodes**]
[**-json** | **-Statistics**] [**-CheckExternalFiles**] [**-CheckJDL**] [**-CheckScripts**]
[**-CheckSubDags**] [**-Strict**]

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
 **-CheckExternalFiles**
    Process external submit and Sub-DAG files if they exist.
 **-CheckJDL**
    Best effort process node submit descriptions similar to
    :tool:`condor_submit` *-dry-run*
 **-CheckScripts**
    Check that node scripts exist.
 **-CheckSubDags**
    Best effort process Sub-DAG files.
 **-[No]JoinNodes**
    Enable/Disable use of join nodes in produced DAG. Enabled
    by default.
 **-json**
    Print results into JSON format.
 **-Statistics**
    Print statistics about the parsed DAG.
 **-Strict**
    Require external files checked to exists during processing
    or be considered an error.
 **-UseDagDir**
    Switch into the DAG file's directory prior to parsing.

General Remarks
---------------

This tool by default does not check any external files that are used
by the DAG (i.e. scripts, submit files, Sub-DAGs) because these can be
dynamically generated during DAGMan's lifetime. Using **-CheckExternalFiles**
will enable this tool to also process Sub-DAGs and submit files (akin to
:tool:`condor_submit` *-dry-run*). This is best effort and only process
the external files if they exist. Use **-Strict** to report an error if
these external files do not exist.

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

    $ condor_dag_checker -stat -JoinNodes sample.dag
    $ condor_dag_checker -stat -NoJoinNodes sample.dag

Example processing Sub-DAGs best effort:

.. code-block:: console

    $ condor_dag_checker -CheckSubDags russian-nested.dag

Example processing node submit descriptions best effort:

.. code-block:: console

    $ condor_dag_checker -CheckJDL sample.dag

Example verifying all node scripts exist:

.. code-block:: console

    $ condor_dag_checker -CheckScripts sample.dag

Example checking all external files for existence and validity:

.. code-block:: console

    $ condor_dag_checker -CheckExternalFiles -strict sample.dag

See Also
--------

- None

Availability
------------

Linux, MacOS, Windows

Introduced in v24.10.1 of HTCondor
