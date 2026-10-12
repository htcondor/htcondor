Upgrading from an 25.0 LTS version to an 26.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 25.0 LTS version of HTCondor to a 26.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

- The use of multiple ``QUEUE`` statements in a single submit
  description file is disabled as of v25.14.0. This functionality
  is planned to finally be removed during the lifetime of
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
  by setting `DAGMAN_DISABLE_ADMIN_THROTTLE_LIMITING = False` in
  your configuration.
  :jira:`3584`

- DAGMan now uses the new DAG file parser introduced for :tool:`condor_dag_checker`. The old
  parser will be deprecated in the future but can be used by setting :macro:`DAGMAN_USE_OLD_FILE_PARSER`
  to ``True``.
  :jira:`3304`

- Removed deprecated full rescue file for DAGMan. With this comes the removal of
  the configuration option ``DAGMAN_WRITE_PARTIAL_RESCUE`` and the command line
  option ``-DumpRescue`` for :tool:`condor_submit_dag`.  This has been deprecated since
  2011, so there should be no problems with compatability.
  :jira:`3565`

- The old ClassAd syntax is no longer supported in job transforms.
  :jira:`3538`

Upgrading from a 25.0 LTS version of HTCondor to a 26.0 LTS version will bring
new features introduced in the 25.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: :doc:`Version 25 Feature Releases <../version-history/feature-versions-25-x>`):

- Common Files

- Self-service Annex

- DAGMan Improvments

- Added ability to set DAGMan throttles via :tool:`htcondor dag throttle` and
  :meth:`htcondor2.DAGMan.throttle` while DAGMan is executing.
  :jira:`3584`

- GPU Improvements

- credmon Improvements

- health checks

- append auto format

- retry with larger disk

- condor_config_val -trace

- durations for floor and ceiling

- metricd - Ganglia and/or Prometheus

- CCB Improvements (tunneling and proxy between two private networks

- Job Index (librarian)

- condor_rm -held

- owned capacity units
