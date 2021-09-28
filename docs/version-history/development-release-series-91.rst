Development Release Series 9.1
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 9.4.0
-------------

Release Notes:

.. HTCondor version 9.4.0 released on Month Date, 2021.

- HTCondor version 9.4.0 not yet released.

New Features:

- None.

Bugs Fixed:

- None.

Version 9.3.0
-------------

Release Notes:

.. HTCondor version 9.3.0 released on Month Date, 2021.

- HTCondor version 9.3.0 not yet released.

New Features:

- Revamped machine ad attribute ``OpSys*`` and configuration parameter
  ``OPSYS*`` values for macOS.
  The OS name is now ``macOS`` and the version number no longer ignores
  the initial ``10.`` or ``11.`` of the actual OS version.
  For example, for macOS 10.15.4, the value of machine attribute
  ``OpSysLongName`` is now ``"macOS 10.15"`` instead of ``"MacOSX 15.4"``.
  :jira:`627`

- Improved and simplified how HTCondor locates the blahp software.
  Configuration parameter ``GLITE_LOCATION`` has been replaced by
  ``BLAHPD_LOCATION``.
  :jira:`713`

- Added an example template for a custom file transfer plugin, which can be
  used to build new plugins.
  :jira:`728`

- When declining to put a job on hold due to the temporary scratch
  directory disappearing, verify that the directory is expected to exist
  and require that the job not be local universe.
  :jira:`680`

_ Added a new generic knob for setting the slot user for all slots.  Configure
  ''NOBODY_SLOT_USER`` for all slots, instead of configuring a ``SLOT<N>_USER`` for each slot.
  :jira:`720`

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

