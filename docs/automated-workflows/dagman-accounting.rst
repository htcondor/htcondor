DAGMan and Accounting Groups
============================

:index:`accounting groups<single: DAGMan; Accounting groups>`

*condor_dagman* propagates
:subcom:`accounting_group<and DAGman>` and :subcom:`accounting_group_user<and DAGman>`
values specified for *condor_dagman* itself to all jobs within the DAG
(including sub-DAGs).

The :subcom:`accounting_group<>` and :subcom:`accounting_group_user<>`
values can be specified using the **-append** flag to
*condor_submit_dag*, for example:

.. code-block:: console

    $ condor_submit_dag -append accounting_group=group_physics -append \
      accounting_group_user=albert relativity.dag

See :ref:`admin-manual/cm-configuration:group accounting`
for a discussion of group accounting and
:ref:`admin-manual/cm-configuration:accounting groups with
hierarchical group quotas` for a discussion of accounting groups with
hierarchical group quotas.

As of version 10.0.0, any explicitly set accounting group information
within a DAGMan nodes job description will take precedence over the
accounting information propagated down through DAGMan. This allows
for easy setting of accounting information for all DAG jobs while
giving a way for specific jobs to run with different accounting information.
