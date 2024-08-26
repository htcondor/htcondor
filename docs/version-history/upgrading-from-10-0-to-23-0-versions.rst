Upgrading from an 10.0 LTS version to an 23.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 10.0 LTS version of HTCondor to a 23.0 LTS version will bring
new features introduced in the 10.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Version 10 Feature Releases <../version-history/feature-versions-10-x.html>`_):

- A *condor_startd* without any slot types defined will now default to a single
  partitionable slot rather than a number of static slots equal to the number of
  cores as it was in previous versions. The configuration template
  ``use FEATURE : StaticSlots`` was added for admins wanting the old behavior.
  :jira:`2026`

- In an HTCondor Execution Point started by root on Linux, the default for cgroups
  memory has changed to be enforcing.  This means that jobs that use more then
  their provisioned memory will be put on hold with an appropriate hold message.
  The previous default can be restored by setting :macro:`CGROUP_MEMORY_LIMIT_POLICY`
  = none on the Execution points.
  :jira:`1974`

- Users can now define DAGMan save points to be able to save the state of a DAGs
  progess to a file and then re-run a DAG from that saved point of progress.
  :jira:`1636`

- DAGMan has much better user control of enviroment variables present
  in the DAGMan job propers environment via :tool:`condor_submit_dag`\'s new
  flags (``-include_env`` & ``-insert_env``) and/or the new DAG file
  description command ``ENV``.
  :jira:`1955`
  :jira:`1580`

- Added the :ref:`man-pages/condor_qusers:*condor_qusers*` command to monitor
  and control users at the Access Point. Users disabled at the Access Point
  are no longer allowed to submit jobs. Jobs submitted before the user was
  disabled are allowed to run to completion. When a user is disabled, an
  optional reason string can be provided.
  :jira:`1723`
  :jira:`1853`

- The *condor_negotiator* now support setting a minimum floor number of cores
  that any given submitter should get, regardless of their fair share. This
  can be set or queried via the :tool:`condor_userprio` tool, in the same way that
  the ceiling can be set or get.
  :jira:`557`

- Added a ``-gpus`` option to :tool:`condor_status`. With this option :tool:`condor_status`
  will show only machines that have GPUs provisioned; and it will show information
  about the GPU properties.
  :jira:`1958`

- The output of :tool:`condor_status` when using the ``-compact`` option has been improved
  to show a separate row for the second and subsequent slot type for machines that have
  multiple slot types. Also the totals now count slots that have the ``BackfillSlot``
  attribute under the ``Backfill`` or ``BkIdle`` columns.
  :jira:`1957`

- Container universe jobs may now specify the *container_image* to be an image
  transferred via a file transfer plugin.
  :jira:`1820`

- Support for Enterprise Linux 9, Amazon Linux 2023, and Debian 12.
  :jira:`1285`
  :jira:`1742`
  :jira:`1938`

- Administrators can specify a new history file for Access Points that records information
  about a job for each execution attempt. If enabled then this information can be queried
  via :tool:`condor_history` ``-epochs``.
  :jira:`1104`

- A single HTCondor pool can now have multiple *condor_defrag* daemons running and they
  will not interfere with each other so long as each has :macro:`DEFRAG_REQUIREMENTS`
  that select mutually exclusive subsets of the pool.
  :jira:`1903`

- Add :tool:`condor_test_token` tool to generate a short lived SciToken for testing.
  :jira:`1115`

- The job’s executable is no longer renamed to ``condor_exec.exe``.
  :jira:`1227`

Upgrading from a 10.0 LTS version of HTCondor to a 23.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of. To see if any of
the following items will affect an upgrade run ``condor_upgrade_check``.

- HTCondor will no longer pass all environment variables to the DAGMan proper manager
  jobs environment. This may result in DAGMan and its various parts (primarily PRE,
  POST,& HOLD Scripts) to start failing or change behavior due to missing needed
  environment variables. To revert back to the old behavior or add the missing
  environment variables to the DAGMan proper job set the :macro:`DAGMAN_MANAGER_JOB_APPEND_GETENV`
  configuration option.
  :jira:`1580`

- We added the ability for the *condor_schedd* to track users over time. Once
  you have upgraded to HTCondor 23, you may no longer downgrade to a version before
  HTCondor 10.5.0 or HTCondor 10.0.4 LTS.
  :jira:`1432`

- Execution Points without any administrator defined slot configuration will now default
  to creating and utilizing one partitionable slot. This causes Startd :macro:`RANK` expressions
  to have no effect. To revert an Execution Point to use static slots add
  ``use FEATURE:StaticSlots`` to the Execution Point configuration.
  :jira:`2026`

- The configuration expression constant ``CpuBusyTime`` no longer represents a time delta but
  rather a timestamp of when the CPU became busy. The new expression constant ``CpuBusyTimer``
  now represents the time delta of how long a CPU has been busy for.
  :jira:`1502`

- The configuration expression constants ``ActivationTimer``, ``ConsoleBusy``, ``CpuBusy``,
  ``CpuIdle``, ``JustCPU``, ``KeyboardBusy``, ``KeyboardNotBusy``, ``LastCkpt``, ``MachineBusy``,
  and ``NonCondorLoadAvg`` no longer exist by default for configuration expressions. To
  re-enable these constants either add ``use FEATURE:POLICY_EXPR_FRAGMENTS`` or one of the
  desktop policies to the configuration.
  :jira:`1502`

- The job router configuration macros :macro:`JOB_ROUTER_DEFAULTS`, :macro:`JOB_ROUTER_ENTRIES`,
  :macro:`JOB_ROUTER_ENTRIES_FILE`, and :macro:`JOB_ROUTER_ENTRIES_CMD` are deprecated and will
  be removed during the lifetime of the HTCondor **V23** feature series.
  :jira:`1968`
