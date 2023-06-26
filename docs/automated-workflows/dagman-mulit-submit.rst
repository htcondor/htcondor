Single Submission of Multiple, Independent DAGs
===============================================

:index:`single submission of multiple, independent DAGs<single: DAGMan; Single submission of multiple, independent DAGs>`

A single use of *condor_submit_dag* may execute multiple, independent
DAGs. Each independent DAG has its own, distinct DAG input file. These
DAG input files are command-line arguments to *condor_submit_dag*.

Internally, all of the independent DAGs are combined into a single,
larger DAG, with no dependencies between the original independent DAGs.
As a result, any generated Rescue DAG file represents all of the
original independent DAGs with a single DAG. The file name of this
Rescue DAG is based on the DAG input file listed first within the
command-line arguments. For example, assume that three independent DAGs
are submitted with

.. code-block:: console

      $ condor_submit_dag A.dag B.dag C.dag

The first listed is ``A.dag``. The remainder of the specialized file
name adds a suffix onto this first DAG input file name, ``A.dag``. The
suffix is ``_multi.rescue<XXX>``, where ``<XXX>`` is substituted by the
3-digit number of the Rescue DAG created as defined in
:ref:`automated-workflows/dagman-resubmit-failed:the rescue dag` section. The first
time a Rescue DAG is created for the example, it will have the file name
``A.dag_multi.rescue001``.

Other files such as ``dagman.out`` and the lock file also have names
based on this first DAG input file.

The success or failure of the independent DAGs is well defined. When
multiple, independent DAGs are submitted with a single command, the
success of the composite DAG is defined as the logical AND of the
success of each independent DAG. This implies that failure is defined as
the logical OR of the failure of any of the independent DAGs.

By default, DAGMan internally renames the nodes to avoid node name
collisions. If all node names are unique, the renaming of nodes may be
disabled by setting the configuration variable :macro:`DAGMAN_MUNGE_NODE_NAMES` to
``False``