Upgrading from an 10.0 LTS version to an 23.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 10.0 LTS version of HTCondor to a 23.0 LTS version will bring
new features introduced in the 10.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Version 10 Feature Releases <../version-history/feature-versions-10-x.html>`_):

- Placeholder

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
  to creating and utilizing one partitionable slot. This causes Startd ``RANK`` expressions
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
