File Paths in DAGs
==================

:index:`file paths in DAGs<single: DAGMan; File paths in DAGs>`

*condor_dagman* assumes that all relative paths in a DAG input file and
the associated HTCondor submit description files are relative to the
current working directory when *condor_submit_dag* is run. This works
well for submitting a single DAG. It presents problems when multiple
independent DAGs are submitted with a single invocation of
*condor_submit_dag*. Each of these independent DAGs would logically be
in its own directory, such that it could be run or tested independent of
other DAGs. Thus, all references to files will be designed to be
relative to the DAG's own directory.

Consider an example DAG within a directory named ``dag1``. There would
be a DAG input file, named ``one.dag`` for this example. Assume the
contents of this DAG input file specify a node job with

.. code-block:: condor-dagman

      JOB A  A.submit

Further assume that partial contents of submit description file
``A.submit`` specify

.. code-block:: condor-submit

      executable = programA
      input      = A.input

Directory contents are

.. code-block:: text

    dag1/
    ├── A.input
    ├── A.submit
    ├── one.dag
    └── programA


All file paths are correct relative to the ``dag1`` directory.
Submission of this example DAG sets the current working directory to
``dag1`` and invokes *condor_submit_dag*:

.. code-block:: console

      $ cd dag1
      $ condor_submit_dag one.dag

Expand this example such that there are now two independent DAGs, and
each is contained within its own directory. For simplicity, assume that
the DAG in ``dag2`` has remarkably similar files and file naming as the
DAG in ``dag1``. Assume that the directory contents are

.. code-block:: text

    parent/
    ├── dag1
    │   ├── A.input
    │   ├── A.submit
    │   ├── one.dag
    │   └── programA
    └── dag2
        ├── B.input
        ├── B.submit
        ├── programB
        └── two.dag


The goal is to use a single invocation of *condor_submit_dag* to run
both dag1 and dag2. The invocation

.. code-block:: console

      $ cd parent
      $ condor_submit_dag dag1/one.dag dag2/two.dag

does not work. Path names are now relative to ``parent``, which is not
the desired behavior.

The solution is the *-usedagdir* command line argument to
*condor_submit_dag*. This feature runs each DAG as if
*condor_submit_dag* had been run in the directory in which the
relevant DAG file exists. A working invocation is

.. code-block:: console

      $ cd parent
      $ condor_submit_dag -usedagdir dag1/one.dag dag2/two.dag

Output files will be placed in the correct directory, and the
``.dagman.out`` file will also be in the correct directory. A Rescue DAG
file will be written to the current working directory, which is the
directory when *condor_submit_dag* is invoked. The Rescue DAG should
be run from that same current working directory. The Rescue DAG includes
all the path information necessary to run each node job in the proper
directory.

Use of *-usedagdir* does not work in conjunction with a JOB node
specification within the DAG input file using the *DIR* keyword. Using
both will be detected and generate an error.
