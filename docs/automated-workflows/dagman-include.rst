INCLUDE
=======

:index:`INCLUDE command<single: DAG input file; INCLUDE command>`

The *INCLUDE* command allows the contents of one DAG file to be parsed
as if they were physically included in the referencing DAG file. The
syntax for *INCLUDE* is

.. code-block:: condor-dagman

    INCLUDE FileName

For example, if we have two DAG files like this:

.. code-block:: condor-dagman

    # File name: foo.dag

    JOB  A  A.sub
    INCLUDE bar.dag

.. code-block:: condor-dagman

    # File name: bar.dag

    JOB  B  B.sub
    JOB  C  C.sub

this is equivalent to the single DAG file:

.. code-block:: condor-dagman

    JOB  A  A.sub
    JOB  B  B.sub
    JOB  C  C.sub

Note that the included file must be in proper DAG syntax. Also, there
are many cases where a valid included DAG file will cause a parse error,
such as the included files defining nodes with the same name.

*INCLUDE*\ s can be nested to any depth (be sure not to create a cycle
of includes!).

Example: Using INCLUDE to simplify multiple similar workflows
-------------------------------------------------------------

One use of the *INCLUDE* command is to simplify the DAG files when we
have a single workflow that we want to run on a number of data sets. In
that case, we can do something like this:

.. code-block:: condor-dagman

    # File name: workflow.dag
    # Defines the structure of the workflow

    JOB Split split.sub
    JOB Process00 process.sub
    ...
    JOB Process99 process.sub
    JOB Combine combine.sub
    PARENT Split CHILD Process00 ... Process99
    PARENT Process00 ... Process99 CHILD Combine

.. code-block:: condor-submit

    # File name: split.sub

    executable = my_split
    input = $(dataset).phase1
    output = $(dataset).phase2
    ...

.. code-block:: condor-dagman

    # File name: data57.vars

    VARS Split dataset="data57"
    VARS Process00 dataset="data57"
    ...
    VARS Process99 dataset="data57"
    VARS Combine dataset="data57"

.. code-block:: condor-dagman

    # File name: run_dataset57.dag

    INCLUDE workflow.dag
    INCLUDE data57.vars

Then, to run our workflow on dataset 57, we run the following command:

.. code-block:: console

    $ condor_submit_dag run_dataset57.dag

This avoids having to duplicate the *JOB* and *PARENT/CHILD* commands
for every dataset - we can just re-use the ``workflow.dag`` file, in
combination with a dataset-specific vars file.
