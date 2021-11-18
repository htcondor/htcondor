Version 9 Feature Releases
==========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 9.5.0
-------------

Release Notes:

.. HTCondor version 9.5.0 released on Month Date, 2021.

- HTCondor version 9.5.0 not yet released.

New Features:

- Docker universe jobs can now be user-level checkpointed by setting
  checkpoint_exit_code in submit files.
  :jira:`841`

Bugs Fixed:

- Fixed a bug where if the submit file set checkpoint_exit_code, and the administrator
  enabled singularity support on the execute node, the job would go on hold at checkpoint time.
  :jira:`837`

Version 9.4.0
-------------

Release Notes:

.. HTCondor version 9.4.0 released on Month Date, 2021.

- HTCondor version 9.4.0 not yet released.

New Features:

- Submission and basic management (list, status, and removal) of :ref:`job_sets` added
  to the :ref:`htcondor_command` CLI tool.
  :jira:`793`

- A new configuration variable ``EXTENDED_SUBMIT_COMMANDS`` can now be used to
  extend the submit language by configuration in the *condor_schedd*.
  :jira:`802`

- In a HAD configuration, the negotiator is now more robust when trying
  to update to collectors that may have failed.  It will no longer block
  and timeout for an extended period of time should this happen.
  :jira:`816`

- SINGULARITY_EXTRA_ARGUMENTS can now be a classad expression, so that the
  extra arguments can depend on the job.
  :jira:`570`

- The :ref:`classad_eval` tool now supports evaluating ClassAd expressions in
  the context of a match.  To specify the target ad, use the new
  ``-target-file`` command-line option.  You may also specify the
  context ad with ``-my-file``, a synonym for ``-file``.  The `classad_eval`
  tool also now supports the ``-debug`` and ``-help`` flags.
  :jira:`707`

- Added a config parameter HISTORY_CONTAINS_JOB_ENVIRONMENT which defaults to true.
  When false, the job's environment attribute is not saved in the history file.  For
  some sites, this can substantially reduce the size of the history file, and allow
  the history to contain many more jobs before rotation.
  :jira:`497`

- Added an attribute to the job ClassAd ``LastRemoteWallClockTime``.  It holds
  the wall clock time of the most recent completed job execution.
  :jira:`751`

- ``JOB_TRANSFORM_*`` and ``SUBMIT_REQUIREMENT_*`` operations in the *condor_schedd*
  are now applied to late materialization job factories at submit time.
  :jira:`756`

- Added option ``--rgahp-nologin`` to **remote_gahp**, which removes the
  ``-l`` option normally given to ``bash`` when starting a remote **blahpd**
  or **condor_ft-gahp**.
  :jira:`734`

- Herefile support was added to configuration templates, and the template
  ``use FEATURE : AssignAccountingGroup`` was converted to from the old
  transform  syntax to the the native transform syntax which requires that support.
  :jira:`796`

- The GPU monitor will no longer run if ``use feature:GPUs`` is enabled
  but GPU discovery did not detect any GPUs.  This mechanism is available
  for other startd cron jobs; see :macro:`STARTD_CRON_<JobName>_CONDITION`.
  :jira:`667`

- Added a new feature where a uesr can export some of their jobs from the
  *condor_schedd* in the form of a job-queue file intended to be used by
  a new temporary *condor_schedd*.
  After the temporary *condor_schedd* runs the jobs, the results can be
  imported back to the original *condor_schedd*.
  This is experimental code that is not suitable for production use.
  :jira:`179`

- The :ref:`htcondor_command` CLI tool now automates the
  setup of our CHTC Slurm cluster when requesting to run jobs on these
  resources.
  :jira:`783`

- When running *remote_gahp* interactively to start a remote
  *condor_ftp-gahp* instance, the user no longer has to set a fake
  ``CONDOR_INHERIT`` environment variable.
  :jira:`819`

Bugs Fixed:

- Fixed a bug that prevented the *condor_procd* (and thus all of condor) from starting
  when running under QEMU emulation.  Condor can now build and run under QEMU ARM
  emulation with this fix.
  :jira:`761`

- Fixed several unlikely bugs when parsing the time strings in classads
  :jira:`814`

- Fixed a bug when computing the identity of a job's X.509 credential that
  isn't a proxy.
  :jira:`800`

- Fixed some bugs which could cause the counts of transferred files
  reported in the job ad to be inaccurate.
  :jira:`813`


Version 9.3.1
-------------

Release Notes:

- HTCondor version 9.3.1 released on November 9, 2021.

New Features:

- Added new submit command ``allowed_job_duration``, which limits how long
  a job can run, expressed in seconds.
  If a job exceeds this limit, it is placed on hold.
  :jira:`794`

Bugs Fixed:

- None.


Version 9.3.0
-------------

Release Notes:

- HTCondor version 9.3.0 released on November 3, 2021.

- This version includes all the updates from :ref:`lts-version-history-907`.

- As we transition from identity based authentication and authorization
  (X.509 certificates) to capability based authorization (bearer tokens),
  we have removed Globus GSI support from this release.
  :jira:`697`

- Submission to ARC CE via the GridFTP interface (grid universe type
  **nordugrid**) is no longer supported.
  Submission to ARC CE's REST interface can be done using the **arc**
  type in the grid universe.
  :jira:`697`

New Features:

- HTCondor will now, if configured, put some common cloud-related attributes
  in the slot ads.  Check the manual :ref:`for details <CommonCloudAttributesConfiguration>`.
  :jira:`616`

- Revamped machine ad attribute ``OpSys*`` and configuration parameter
  ``OPSYS*`` values for macOS.
  The OS name is now ``macOS`` and the version number no longer ignores
  the initial ``10.`` or ``11.`` of the actual OS version.
  For example, for macOS 10.15.4, the value of machine attribute
  ``OpSysLongName`` is now ``"macOS 10.15"`` instead of ``"MacOSX 15.4"``.
  :jira:`627`

- Added an example template for a custom file transfer plugin, which can be
  used to build new plugins.
  :jira:`728`

- Added a new generic knob for setting the slot user for all slots.  Configure
  ''NOBODY_SLOT_USER`` for all slots, instead of configuring a ``SLOT<N>_USER`` for each slot.
  :jira:`720`

- Improved and simplified how HTCondor locates the blahp software.
  Configuration parameter ``GLITE_LOCATION`` has been replaced by
  ``BLAHPD_LOCATION``.
  :jira:`713`

- Added new attributes to the job ClassAd which records the number of files 
  transferred between the *condor_shadow* and *condor_starter* only during
  the last run of the job.
  :jira:`741`

- When declining to put a job on hold due to the temporary scratch
  directory disappearing, verify that the directory is expected to exist
  and require that the job not be local universe.
  :jira:`680`

Bugs Fixed:

- None.

Version 9.2.0
-------------

Release Notes:

- HTCondor version 9.2.0 released on September 23, 2021.

New Features:

- Added a ``SERVICE`` node type to *condor_dagman*: a special node which runs
  in parallel to a DAG for the duration of its workflow. This can be used to
  run tasks that monitor or report on a DAG workflow without directly
  impacting it.
  :jira:`437`

- Added new configuration parameter ``NEGOTIATOR_MIN_INTERVAL``, which
  sets the minimum amount of the time between the start of one
  negotiation cycle and the next.
  :jira:`606`

- The *condor_userprio* tool now accepts one or more username arguments and will report
  priority and usage for only those users
  :jira:`559`

- Added a new ``-yes`` command-line argument to the *condor_annex*, allowing
  it to request EC2 instances without manual user confirmation.
  :jira:`443`

Bugs Fixed:

- HTCondor no longer crashes on start-up if ``COLLECTOR_HOST`` is set to
  a string with a colon and a port number, but no host part.
  :jira:`602`

- Changed the default value of configuration parameter ``MAIL`` to
  */usr/bin/mail* on Linux.
  This location is valid on all of our supported Linux platforms, unlike
  the previous default value of */bin/mail*.
  :jira:`581`

- Removed unnecessary limit on history ad polling and fixed some
  configuration parameter checks in *condor_adstash*.
  :jira:`629`

Version 9.1.6
-------------

Release Notes:

- HTCondor version 9.1.6 limited release on September 14, 2021.

New Features:

- None.

Bugs Fixed:

- Fixed a bug that prevented Singularity jobs from running when the singularity
  binary emitted many warning messages to stderr.
  :jira:`698`

Version 9.1.5
-------------

Release Notes:

- HTCondor version 9.1.5 limited release on September 8, 2021.

New Features:

- The number of files transferred between the *condor_shadow* and
  *condor_starter* is now recorded in the job ad with the new attributes.
  :jira:`679`

Bugs Fixed:

- None.

Version 9.1.4
-------------

Release Notes:

- HTCondor version 9.1.4 limited release on August 31, 2021.

New Features:

- Jobs are no longer put on hold if a failure occurs due to the scratch
  execute directory unexpectedly disappearing. Instead, the jobs will
  return to idle status to be re-run.
  :jira:`664`

Bugs Fixed:

- Fixed a problem introduced in HTCondor version 9.1.3 where
  X.509 proxy delegation to older versions of HTCondor would fail.
  :jira:`674`

Version 9.1.3
-------------

Release Notes:

- HTCondor version 9.1.3 released on August 19, 2021.

- Globus GSI is no longer needed for X.509 proxy delegation

- GSI is no longer in the list of default authentication methods.
  To use GSI, you must enable it by setting one or more of the
  ``SEC_<access-level>_AUTHENTICATION_METHODS`` configuration parameters.
  :jira:`518`

New Features:

- The semantics of undefined user job policy expressions has changed.  A
  policy whose expression evaluates to undefined is now uniformly ignored,
  instead of either putting the job on hold or treated as false.
  :jira:`442`

- Added two new attributes to the job ClassAd, ``NumHolds`` and ``NumHoldsByReason``, 
  that are used to provide historical information about how often this
  job went on hold and why. Details on all job ClassAd attributes, including
  these two new attributes, can be found in section:
  :doc:`../classad-attributes/job-classad-attributes`
  :jira:`554`

- The "ToE tag" entry in the job event log now includes the exit code or
  signal number, if and as appropriate.
  :jira:`429`

- Docker universe jobs are now run under the built-in docker
  init process, which means that zombie processes are automatically
  reaped.  This can be turned off with the knob
  *DOCKER_RUN_UNDER_INIT* = false
  :jira:`462`

- Many services support the "S3" protocol.  To reduce confusion, we've
  added new aliases for the submit-file commands ``aws_access_key_id_file``
  and ``aws_secret_access_key_file``: ``s3_access_key_id_file`` and
  ``s3_secret_access_key_file``.  We also added support for ``gs://``-style
  Google Cloud Storage URLs, with the corresponding ``gs_access_key_id_file``
  and ``gs_secret_access_key_file`` aliases.  This support, and the aliases,
  use Google Cloud Storage's "interoperability" API.  The HMAC access key ID
  and secret keys may be obtained from the Google Cloud web console's
  "Cloud Storage" section, the "Settings" menu item, under the
  "interoperability" tab.
  :jira:`453`

- Add new submit command ``batch_extra_submit_args`` for grid universe jobs
  of type ``batch``.
  This lets the user supply arbitrary command-line arguments to the submit
  command of the target batch system.
  These are supplied in addition to the command line arguments derived
  from other attributes of the job ClassAd.
  :jira:`526`

- When GSI authentication is configured or used, a warning is now printed
  to daemon logs and the stderr of tools.
  These warnings can be suppressed by setting configuration parameters
  ``WARN_ON_GSI_CONFIGURATION`` and ``WARN_ON_GSI_USAGE`` to ``False``.
  :jira:`517`

- Introduced a new command-line tool, ``htcondor`` 
  (see :doc:`man page <../man-pages/htcondor>`) for managing HTCondor jobs
  and resources. This tool also includes new capabilities for running
  HTCondor jobs on Slurm machines which are temporarily acquired
  to act as HTCondor execution points.
  :jira:`252`


Bugs Fixed:

- Fixed a bug where jobs cannot start on Linux if the execute directory is placed
  under /tmp or /var/tmp.  The problem is this breaks the default MOUNT_UNDER_SCRATCH
  option.  As a result, if the administrator located EXECUTE under tmp, HTCondor can
  no longer make a private /tmp or /var/tmp directory for the job.
  :jira:`484`


Version 9.1.2
-------------

Release Notes:

-  HTCondor version 9.1.2 released on July 29, 2021.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes a security-related bug
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html>`_.

   :jira:`509`
   :jira:`587`

Version 9.1.1
-------------

Release Notes:

-  HTCondor version 9.1.1 released on July 27, 2021 and pulled two days later when an issue was found with a patch.

New Features:

-  None.

Bugs Fixed:

Version 9.1.0
-------------

Release Notes:

- HTCondor version 9.1.0 released on May 20, 2021.

- The *condor_convert_history* command was removed.
  :jira:`392`

New Features:

- Added support for submission to the ARC CE REST interface via the new
  grid universe type **arc**.
  :jira:`138`

- Added a new option in DAGMan to put failed jobs on hold and keep them in the
  queue when :macro:`DAGMAN_PUT_FAILED_JOBS_ON_HOLD` is True. For some types
  of transient failures, this allows users to fix whatever caused their job to
  fail and then release it, allowing the DAG execution to continue.
  :jira:`245`

- *gdb* and *strace* now work in Docker Universe jobs.
  :jira:`349`

- The *condor_startd* on platforms that support Docker now
  runs a simple Docker container at startup to verify that
  docker universe completely works.  This can be disabled with the
  knob DOCKER_PERFORM_TEST
  :jira:`325`

- On Linux machines with performance counter support, vanilla universe jobs
  now report the number of machine instructions executed
  :jira:`390`

Bugs Fixed:

- None.

