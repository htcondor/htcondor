Upgrading from an 25.0 LTS version to an 26.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 25.0 LTS version of HTCondor to a 26.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

- The use of multiple ``QUEUE`` statements in a single submit
  description file is disabled by default as of v25.14.1.
  For jobs not using late materialization, setting
  :macro:`SUBMIT_PREVENT_MULTI_QUEUE` to ``False`` temporarily restores support.
  This functionality is planned to be removed during the lifetime of
  the V26 feature series.
  :jira:`3878`

- Fixed a bug where the *condor_credmon_oauth* would not create and/or
  refresh credentials containing a user-supplied handle. Consequently,
  underscores (and other special characters) are no longer allowed in
  credential provider names by default. This behavior change can be
  reverted by setting :macro:`CREDMON_ALLOW_SPECIAL_CHAR_NAMES` to true,
  though this will disable the use of user-supplied handles.
  See :ref:`provider_name_restrictions` for details.
  :jira:`2860`

- :tool:`condor_run` no longer passes :jdl:`getenv = true` to the
  generated submit file.  There is a new option ``-getenv`` to
  allow this.
  :jira:`3549`

- DAGMan's configurable throttles set by the user are now limited
  to the values set in the root configuration. This can be disabled
  by setting :macro:`DAGMAN_DISABLE_ADMIN_THROTTLE_LIMITING` to ``True`` in
  the administrator's configuration.
  :jira:`3584`

- DAGMan now uses the new DAG file parser introduced for :tool:`condor_dag_checker`. The old
  parser will be deprecated in the future but can be used by setting :macro:`DAGMAN_USE_OLD_FILE_PARSER`
  to ``True``.
  :jira:`3304`

- Removed deprecated full rescue file for DAGMan. With this comes the removal of
  the configuration option ``DAGMAN_WRITE_PARTIAL_RESCUE`` and the command line
  option ``-DumpRescue`` for :tool:`condor_submit_dag`.  This has been deprecated since
  2011, so there should be no problems with compatibility.
  :jira:`3565`

- The old ClassAd syntax is no longer supported in job transforms.
  :jira:`3538`

Upgrading from a 25.0 LTS version of HTCondor to a 26.0 LTS version will bring
new features introduced in the 25.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: :doc:`Version 25 Feature Releases <../version-history/feature-versions-25-x>`):

- Common Files
  In some situations, HTCondor no longer needs to transfer the same file to the same EP once for
  each job that uses it.  This can be done automatically for (some) container images, and with the
  help of the submitter for other immutable input files.
  
  See :ref:`common_file_transfer` in the user manual for an introduction, and
  :ref:`admin_common_file_transfer` in the administrator's manual for details.
- Add new ``self-service`` form of Annex, where the user is responsible for
  submitting the Annex EPs to the Slurm cluster of their choice.
  :jira:`3645`

- :tool:`condor_dag_checker` now has options to process and check the existence
  and validity of node submit descriptions, sub-DAG files, and scripts.
  :jira:`3540`

- Added new DAGMan :dag-cmd:`TOLERANCE` command to allow users to have
  finer control on what constitutes failure with a job list in a single
  node and whether DAGMan should immediately remove job list upon failure
  or wait for jobs to finish.
  :jira:`3838`

- DAGMan :dag-cmd:`PARENT/CHILD` dependencies can now be declared *WEAK*,
  meaning a child node only requires its parent(s) to finish executing
  rather than to succeed. See :ref:`Weak Dependencies<DAG weak dependencies>`.
  :jira:`2690`

- Scheduler universe jobs such as DAGMan will now go into cool down
  for :macro:`SCHEDULER_UNIVERSE_COOL_DOWN_DURATION` seconds when the
  *condor_schedd* fails to launch the job or the scheduler job exits with an error.
  :jira:`3769`
  :jira:`3067`

- Added ability to set DAGMan throttles via :tool:`htcondor dag throttle` and
  :meth:`htcondor2.DAGMan.throttle` while DAGMan is executing.
  :jira:`3584`

- HTCondor now correctly detects and advertises the NVIDIA driver version
  for NVIDIA GPUs which are configured to run in "MIG" mode.
  :jira:`3626`

- Support detection and advertising of AMD GPUs on ROCM 7 systems.
  :jira:`3880`

- Container universe jobs that use the apptainer or singularity runtime
  now detect AMD or nvidia gpus, and add --nv or --rocm appropriately
  to the apptainer exec command line.  This should allow container
  universe jobs on AMD GPU systems to properly access their GPUs.
  :jira:`3887`

- The credmon can now obtain and renew OAuth2 access tokens from a
  `Pelican <https://pelicanplatform.org/>`_ federation (such as the OSDF), so
  that jobs can read and write federation objects with ``osdf://`` and
  ``pelican://`` URLs.  Services are declared with
  ``PELICAN_CREDMON_PROVIDER_NAMES`` and a set of ``<ServiceName>_PELICAN_*``
  options; see :ref:`installing_credmon_pelican`.
  :jira:`3784`

- Improve how an AP administrator can automatically provide OAuth
  credentials for all jobs.
  Configuration knob :macro:`SUBMIT_ADD_LOCAL_CREDMON_PROVIDERS` specifies
  whether the AP is providing credentials for all jobs (default is true).
  Configuration knob :macro:`SUBMIT_ADD_LOCAL_CREDMON_PROVIDER_NAMES`
  specifies which credential names are being provided for all jobs
  (default is all credentials that the AP can generate by itself).
  Remote submitters no longer need to alter their configuration files to
  submit to an AP that provides credentials for all jobs.
  :jira:`3391`

- Fixed a bug where the *condor_credmon_oauth* would not create and/or
  refresh credentials containing a user-supplied handle. Consequently,
  underscores (and other special characters) are no longer allowed in
  credential provider names by default. This behavior change can be
  reverted by setting :macro:`CREDMON_ALLOW_SPECIAL_CHAR_NAMES` to true,
  though this will disable the use of user-supplied handles.
  See :ref:`provider_name_restrictions` for details.
  :jira:`2860`

- Add :ad-attr:`Healthy` as a new *condor_startd* :ref:`expression <condor_startd_health_expression>`
  that indicates if the *condor_startd* has passed Administrator defined health checks.
  :jira:`3611`

- :tool:`condor_q`, :tool:`condor_qusers`, :tool:`condor_status`, and :tool:`condor_who` now
  support an ``-aaf`` option,
  for append autoformat, which displays columns like the `-af` option does, but
  appends them to the default output.
  :jira:`3604,3684`
  :jira:`3571`

- Add new submit command :subcom:`retry_request_disk`, which operates
  like :subcom:`retry_request_memory` but for the ``Disk`` resource.
  :jira:`3552`

- :tool:`condor_config_val` now accepts a ``-trace`` option which prints the file, line and previous value
  each time a value is replaced as the configuration files are read.
  :jira:`3699`

- Added new ``-duration`` option to :tool:`condor_userprio` to create a persistent
  time-limited *lease* for ceiling values set with ``-setceiling``. The prior
  ceiling value is automatically restored when after the lease expires or is
  canceled with the ``-cancelceilinglease`` option.
  :jira:`3518`

- :tool:`condor_userprio` ``-setfloor`` and ``-setfactor`` now accept a
  ``-duration`` option to install a time-limited *lease* on the floor or
  priority factor: the prior value is automatically restored after the
  given number of seconds. Leases are persistent across
  *condor_negotiator* restarts. New ``-cancelfloorlease`` and
  ``-cancelfactorlease`` options cancel an in-effect lease. This
  mirrors the existing ``-setceiling`` lease support.
  :jira:`3764`

- Introducing *condor_metricd*, a new daemon that publishes HTCondor
  metrics from the *condor_collector* to multiple monitoring backends.
  The same binary is also installed as *condor_gangliad* for backwards
  compatibility, and inspects ``argv[0]`` to select its mode at startup.
  In addition to the Ganglia backend, *condor_metricd* now supports a
  Prometheus text-format file export backend controlled by the new
  :macro:`PROMETHEUS_METRICS_FILE`, :macro:`PROMETHEUS_DEFAULT_LABELS`, and
  :macro:`PROMETHEUS_METRICS_INCLUDE_TIMESTAMP` knobs. New metric-definition
  keywords ``ExportMetric``, ``PrometheusLabels``, and ``Counter`` allow
  per-metric routing and labeling.
  :jira:`3374`

- Added CCB tunneling, which lets an entire set of daemons on a restricted host
  (for example an HPC worker node that cannot make direct outbound TCP
  connections) send *and* receive all of their connections through a Connection
  Broker. A daemon or tool routes its outbound connections through a broker with
  :macro:`OUTBOUND_CCB_ADDRESS`; a broker relays them with
  :macro:`CCB_OUTBOUND_PROXY`. The :tool:`condor_master` can run and orchestrate a
  local inside CCB automatically with :macro:`USE_OUTBOUND_CCB`. Inbound
  connections to a tunneled daemon route recursively through every broker in the
  path while the end-to-end CEDAR security session is preserved, and a client
  authenticates only to the entry broker.
  :jira:`3828`

- HTCondor Connection Brokering (CCB) now supports a streaming mode that
  brokers connections between two daemons that are *both* behind a CCB,
  without opening any incoming ports. When the requesting daemon is itself
  CCB-routed, the CCB server has the target daemon connect back to the CCB
  server and then relays bytes between the two connections; the end-to-end
  security handshake is still performed directly between the two daemons, so
  the CCB server never decrypts their traffic. Streaming mode is enabled by
  default and controlled by the new configuration knobs
  :macro:`CCB_SERVER_STREAMING` and :macro:`CCB_CLIENT_STREAMING`.
  :jira:`3805`

- Introducing the :ref:`archive-librarian`. A new daemon that maintains an
  SQLite3 database of indexes to historical job records located in an Access
  Point's :macro:`HISTORY` archive.
  :jira:`3143`

- :tool:`condor_rm` now accepts a **-held** option that restricts the
  removal to jobs in the held state. It may be combined with the usual
  constraints (a cluster, a ``cluster.proc``, a user, or **-constraint**),
  so that only the held jobs among those selected are removed.
  :jira:`3710`

- owned capacity units
