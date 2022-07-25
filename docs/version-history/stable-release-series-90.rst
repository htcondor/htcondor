Version 9.0 LTS Releases
========================

These are Long Term Support (LTS) releases of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
9.0.y releases. New features will be added in the 9.x.y feature releases.

The details of each version are described below.

.. _lts-version-history-9017:

Version 9.0.17
--------------

Release Notes:

.. HTCondor version 9.0.16 released on Month Date, 2022.

- HTCondor version 9.0.16 not yet released.

New Features:

- Increased the length of the password generated for Windows default
  slot user accounts from 14 characters to 32 characters, and added
  some code to insure that complexity measures that look at
  character set and not length will still be satisfied.
  :jira:`1232`

Bugs Fixed:

- When a failure occurs with a grid universe job of type ``batch``,
  the local job is now always put on hold, instead of the remote job
  being canceled and automatically resubmitted.
  :jira:`1226`

- Job attribute ``GridJobId`` is no longer altered for **batch** grid
  universe jobs when the job enters ``Removed`` status.
  :jira:`1224`

- Fixed a bug where the *condor_gridmanager* would delete the job's
  X.509 proxy file when it meant to delete a temporary copy of the
  proxy file.
  :jira:`1223`

- Fixed a bug where forwarding a refreshed X.509 proxy for a **batch**
  grid universe job would fail.
  :jira:`1222`

.. _lts-version-history-9016:

Version 9.0.16
--------------

Release Notes:

.. HTCondor version 9.0.16 released on Month Date, 2022.

- HTCondor version 9.0.16 not yet released.

New Features:

- Singularity jobs now mount /tmp and /var/tmp under the scratch
  directory, not in tmpfs.
  :jira:`1193`

- For **batch** grid universe jobs, report resources provisioned by the batch
  scheduler when available.
  :jira:`1199`

Bugs Fixed:

- Fixed a bug on Windows that caused a misleading error message about
  the SharedPortEndpoint when a daemon exits.
  :jira:`1178`

- Fixed a bug where the *condor_check_config* tool raised an UnboundLocalError
  due to an undefined variable.
  :jira:`1186`

- Fixed a file descriptor leak when using SciTokens for authentication.
  :jira:`1188`

- Fixed a bug in *condor_gpu_discovery* which would cause the tool to crash
  when OpenCL devices were detected and ``GPU_DEVICE_ORDINAL`` was set in the environment.
  :jira:`1191`

- Fix a bug that could cause daemons to crash if their log rotates
  during shutdown.
  :jira:`1200`

- Fixed a bug where the *condor_starter* would wait forever for a
  reconnect from the *condor_shadow* if a network failure occurred
  during cleanup after the job completed.
  :jira:`1213`

- The ``condor-credmon-oath`` package now properly pulls in ``python3-mod_wsgi``
  on Enterprise Linux 8.
  :jira:`1217`
  
.. _lts-version-history-9015:

Version 9.0.15
--------------

- HTCondor version 9.0.15 released on July 21, 2022.

New Features:

- For **batch** grid universe jobs, report resources provisioned by the batch
  scheduler when available.
  :jira:`1199`

Bugs Fixed:

- None.

.. _lts-version-history-9014:

Version 9.0.14
--------------

Release Notes:

- HTCondor version 9.0.14 released on July 12, 2022.

New Features:

- Made SciTokens mapping failures more prominent in the daemon logs.
  :jira:`1072`

- The manual page, usage and logging of the *condor_set_shutdown* tool
  has been improved to clarify what this tool does and how to use it.
  :jira:`1102`

Bugs Fixed:

- Fixed a bug where if a job's output and error were directed to the same
  file, no other output files would be transferred.
  :jira:`1101`

- Ensure that the matching set of Python bindings is installed when HTCondor
  is upgraded on RPM based platforms.
  :jira:`1127`

- Fixed a bug that caused ``$(OPSYSANDVER)`` to expand to nothing in a JOB_TRANSFORM.
  :jira:`1121`

- Fixed a bug in the Python bindings that prevented context managed
  ``htcondor.SecMan`` sessions from working.
  :jira:`924`
  
- Fixed a bug where ``RemoteUserCpu`` and ``RemoteSysCpu`` attributes are occasionally
  set to ``0`` for successfully completed jobs.
  :jira:`1162`

- Make ``condor-externals`` package dependency less strict to ease transition
  between CHTC and OSG RPM repositories.
  :jira:`1177`

.. _lts-version-history-9013:

Version 9.0.13
--------------

Release Notes:

- HTCondor version 9.0.13 released on May 26, 2022.

New Features:

- If the configuration macro ``[SCHEDD|STARTD]_CRON_LOG_NON_ZERO_EXIT`` is
  set to true, the corresponding daemon will write the cron job's non-zero
  exit code to the log, followed by the cron job's output.
  :jira:`971`

- *condor_config_val* will now print an ``@=end/@end`` pair rather than simply ``=``
  when printing multi-line configuration values for ``-dump``, ``-summary``, and ``-verbose``
  mode output.
  :jira:`1032`

- Add a ``SEC_CREDENTIAL_STORECRED_OPTS`` variable to *condor_vault_storer*
  to enable sending additional options to every *condor_store_cred* command.
  :jira:`1091`

- Recognize the new format of vault tokens, beginning with ``hvs.`` in addition
  to the old format beginning with ``s.`` .
  :jira:`1091`

Bugs Fixed:

- The *condor_run* tool now reports job submit errors
  and warnings to the terminal rather than writing them into a log file.
  :jira:`1002`

- Fixed a bug where Kerberos Authentication would fail for DAGMan.
  :jira:`1060`

- Fix problem that can cause HTCondor to not start up when the network
  configuration is complex.
  Long hostnames, multiple CCB addresses, having both IPv4 and IPv6 addresses,
  and long private network names all contribute to complexity.
  :jira:`1070`

- Updated the Windows build of HTCondor to use SSL 1.1.1m.
  :jira:`840`

.. _lts-version-history-9012:

Version 9.0.12
--------------

Release Notes:

- HTCondor version 9.0.12 released on April 19, 2022.

New Features:

- None.

Bugs Fixed:

- Fixed a bug in the parallel universe that caused the *condor_schedd* to crash
  with partitionable slots.
  :jira:`986`

- Fixed a bug that could cause a daemon to erase its security session
  to its family of daemon processes and subsequently crash when trying to
  connect to one of those daemons.
  :jira:`937`

- Fixed a bug that prevented the High-Availability Daemon (HAD) from
  working when user-based security is enabled.
  :jira:`891`

- In a HAD configuration, the negotiator is now more robust when trying
  to update to collectors that may have failed.  It will no longer block
  and timeout for an extended period of time should this happen.
  :jira:`816`

- The Job Router no longer sets an incorrect ``User`` job attribute
  when routing a job between two *condor_schedd* s with different
  values for configuration parameter ``UID_DOMAIN``.
  :jira:`1005`

- Fixed a bug in the startd drain command in the Python bindings that prevented
  it from working with zero arguments.
  :jira:`936`

- Fixed a bug that prevented administrators from setting certain rare custom
  Linux parameters in the linux_kernel_tuning_script.
  :jira:`990`

- DAGMan now publishes its status (total number of nodes, nodes done, nodes
  failed, etc.) to the job ad immediately at startup.
  :jira:`968`

- Fixed a bug where a credential file with an underscore in its filename could
  not be used by the curl plugin when doing HTTPS transfers with a bearer token.
  It can now be accessed by replacing "_" with "." in the URL scheme.
  :jira:`1011`

- Fixed several unlikely bugs when parsing the time strings in ClassAds.
  :jira:`998`

- *condor_version* now reports the build ID on Debian and Ubuntu platforms.
  :jira:`749`

.. _lts-version-history-9011:

Version 9.0.11
--------------

Release Notes:

- HTCondor version 9.0.11 released on March 15, 2022.

New Features:

- The *condor_job_router* can now create an IDTOKEN and send it them along
  with a routed job for use by the job. This is controlled by a new
  configuration variable ``JOB_ROUTER_CREATE_IDTOKEN_NAMES`` and a new route
  option ``SendIDTokens``.
  :jira:`735`

Bugs Fixed:

- HTCondor will now properly transfer checkpoints if ``stream_output``
  or ``stream_error`` is set and ``output`` or ``error``, respectively,
  is not an absolute path.
  :jira:`736`

- A problem where HTCondor would not create a directory on the execute
  node before trying to transfer a file into it should no longer occur.  (This
  would cause the job which triggered this problem to go on hold.)  One
  way to trigger this problem was by setting ``preserve_relative_paths``
  and specifying the same directory in both ``transfer_input_files`` and
  ``transfer_checkpoint_files``.
  :jira:`857`

- The *condor_annex* tool no longer duplicates the first tag if given multiple
  ``-tag`` options on the command line.  You can now set longer user data on
  the command-line.
  :jira:`910`

- Fixed a bug in the *condor_job_router* that could result in routes and transforms
  substituting a default configuration value rather than the value
  from the configuration files when a route or transform was applied
  :jira:`902`

- For **batch** grid universe jobs, a small default memory value is no
  longer generated when **request_memory** is not specified in the submit
  file.
  This restores the behavior in versions 9.0.1 and prior.
  :jira:`904`

- Fixed a bug in the FileTransfer mechanism where URL transfers caused
  subsequent failures to report incorrect error messages.
  :jira:`915`

- Fixed a bug in the *condor_dagman* parser which caused ``SUBMIT-DESCRIPTION``
  statements to return an error even after parsing correctly.
  :jira:`928`

- Fix problem where **condor_ssh_to_job** may fail to connect to a job
  running under an HTCondor tarball installation (glidein) built from an RPM
  based platform.
  :jira:`942`

- The Python bindings no longer segfault when the ``htcondor.Submit``
  constructor is passed a dictionary with an entry whose value is ``None``.
  :jira:`950`

.. _lts-version-history-9010:

Version 9.0.10
--------------

Release Notes:

-  HTCondor version 9.0.10 released on March 15, 2022.

New Features:

-  None.

Bugs Fixed:

-  *Security Items*: This release of HTCondor fixes security-related bugs
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0001 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0001>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0002 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0002>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003>`_.

   :jira:`724`
   :jira:`730`
   :jira:`985`

.. _lts-version-history-909:

Version 9.0.9
-------------

Release Notes:

- HTCondor version 9.0.9 released on January 13, 2022.

- Since CentOS 8 has been retired, we now build for Enterprise Linux 8 on
  Rocky Linux 8.
  :jira:`911`

- Debian 11 (bullseye) has been added as a supported platform.
  :jira:`94`

New Features:

- The OAUTH credmon is packaged for the Enterprise Linux 8 platform.
  :jira:`825`

Bugs Fixed:

- When a grid universe job of type ``condor`` fails on the remote system,
  the local job is now put on hold, instead of automatically resubmitted.
  :jira:`871`

- Fixed a bug where a running parallel universe job would go to idle
  status when the job policy indicated it should be held.
  :jira:`869`

- Fixed a bug running jobs in a Singularity container where
  the environment variables added by HTCondor could include incorrect
  pathnames to the location of the job's scratch directory.
  This occurred when setting the ``SINGULARITY_TARGET_DIR`` configuration option.
  :jira:`885`

- Fixed a bug where the *condor_job_router* could crash while trying to
  report an invalid router configuration when C-style comments were used
  before an old syntax route ClassAd. As a result of this fix the job router
  now treats C-style comments as a indication that the route is old syntax.
  :jira:`864`

- Fixed a bug where binary bytes were trying to be written via an ASCII file
  handler in *condor_credmon_oauth* when using Python 3.
  :jira:`633`

- Fixed a bug in **condor_top** where two daemon ClassAds were assumed
  to be the same if some specific attributes were missing from the
  latest ClassAd. Also **condor_top** now exits early if no stats are
  provided by the queried daemon.
  :jira:`880`

- Fixed a bug where the user job log could be written in the wrong
  directory when a spooled job's output was retrieved with
  *condor_transfer_data*.
  :jira:`886`

- Fixed a bug in *condor_adstash* where setting a list of *condor_startds*
  to query in the configuration lead to no *condor_startds* being queried.
  :jira:`888`

.. _lts-version-history-908:

Version 9.0.8
-------------

Release Notes:

- HTCondor version 9.0.8 released on December 2, 2021.

New Features:

- None.

Bugs Fixed:

- Fixed a bug where very large values of ImageSize and other job attributes
  that have _RAW equivalents would get rounded incorrectly, and end up negative.
  :jira:`780`

- Fixed a bug with the handling of ``MAX_JOBS_PER_OWNER`` in the *condor_schedd*
  where it was treated as a per-factory limit rather than as a per-owner limit for jobs
  submitted with the ``max_idle`` or ``max_materialize`` submit keyword.
  :jira:`755`

- Fixed a bug in how the **condor_schedd** selects a new job to run on a
  dynamic slot after the previous job completes.
  The **condor_schedd** could choose a job that requested more disk space
  than the slot provided, resulting in the **condor_startd** refusing to
  start the job.
  :jira:`798`

- Fixed daemon log message that could allow unintended processes to use
  the **condor_shared_port** service.
  :jira:`725`

- Fixed a bug in the ClassAds function ``substr()`` that could cause a
  crash if the ``offset`` argument was out of range.
  :jira:`823`

- Fixed bugs in the Kerberos authentication code that cause a crash on
  macOS and can leak memory.
  :jira:`200`

- Fixed a bug where if **condor_schedd** fails to claim a **condor_startd**,
  the job matched to that **condor_startd** won't be rematched for up to
  20 minutes.
  :jira:`769`

.. _lts-version-history-907:

Version 9.0.7
-------------

Release Notes:

- HTCondor version 9.0.7 released on November 2, 2021.

New Features:

- The configuration parameter ``SEC_TOKEN_BLACKLIST_EXPR`` has been renamed
  to ``SEC_TOKEN_REVOCATION_EXPR``.
  The old name is still recognized if the new one isn't set.
  :jira:`744`

Bugs Fixed:

- *condor_watch_q* no longer has a limit on the number of job event log files
  it can watch.
  :jira:`658`

- Fix a bug in *condor_watch_q* which would cause it to fail when run
  on older kernels.
  :jira:`745`

- Fixed a bug where *condor_gpu_discovery* could segfault on some older versions
  of the nVidia libraries. This would result in GPUs not being detected.
  The bug was introduced in HTCondor 9.0.6 and is known to occur with CUDA run time 10.1.
  :jira:`760`

- Fixed a bug that could crash the *condor_startd* when claiming a slot
  with p-slot preemption.
  :jira:`737`

- Fixed a bug where the ``NumJobStarts`` and ``JobCurrentStartExecutingDate``
  job ad attributes weren't updated if the job began executing while the
  *condor_shadow* and *condor_starter* were disconnected.
  :jira:`752`

- Ensure the HTCondor uses version 0.6.2 or later SciTokens library so that
  WLCG tokens can be read.
  :jira:`801`

.. _lts-version-history-906:

Version 9.0.6
-------------

Release Notes:

- HTCondor version 9.0.6 released on September 23, 2021.

New Features:

- Added a new option ``-log-steps`` to *condor_job_router_info*.  When used with the
  ``-route-jobs`` option, this option will log each step of the route transforms
  as they are applied.
  :jira:`578`

- The stdin passed to *condor_job_router* hooks of type ``_TRANSLATE_JOB`` will
  now be passed information on the route in a format that is the same as what was passed
  in 8.8 LTS releases.  It will always be a ClassAd, and include the route ``Name`` as
  an attribute.
  :jira:`646`

- Added configuration parameter ``AUTH_SSL_REQUIRE_CLIENT_CERTIFICATE``,
  a boolean value which defaults to ``False``.
  If set to ``True``, then clients that authenticate to a daemon using
  SSL must present a valid SSL credential.
  :jira:`236`

- The location of database files for the *condor_schedd* and the *condor_negotiator* can
  now be configured directly by using the configuration variables ``JOB_QUEUE_LOG`` and
  ``ACCOUNTANT_DATABASE_FILE`` respectively.  Formerly you could control the directory
  of the negotiator database by configuring ``SPOOL`` but not otherwise, and the
  configuration variable ``JOB_QUEUE_LOG`` existed but was not visible.
  :jira:`601`

- The *condor_watch_q* command now refuses to watch the queue if
  doing so would require using more kernel resources ("inotify watches")
  than allowed.  This limit can be increased by your system
  administrator, and we expect to remove this limitation in a future
  version of the tool.
  :jira:`676`

Bugs Fixed:

- The ``CUDA_VISIBLE_DEVICES`` environment variable may now contain ``CUDA<n>``
  and ``GPU-<uuid>`` formatted values, in addition to integer values.
  :jira:`669`

- Updated *condor_gpu_discovery* to be compatible with version 470 of
  nVidia's drivers.
  :jira:`620`

- If run with only the CUDA runtime library available, *condor_gpu_discovery*
  and *condor_gpu_utilization* no longer crash.
  :jira:`668`

- Fixed a bug in *condor_gpu_discovery* that could result in no output or a segmentation fault
  when the ``-opencl`` argument was used.
  :jira:`729`

- Fixed a bug that prevented Singularity jobs from running when the singularity
  binary emitted many warning messages to stderr.
  :jira:`698`

- The Windows MSI installer has been updated so that it no longer reports that a script
  failed during installation on the latest version of Windows 10.  This update also changes
  the permissions of the configuration files created by the installer so the installing user has
  edit access and all users have read access.
  :jira:`684`

- Fixed a bug that prevented *condor_ssh_to_job* from working to a personal
  or non-rootly condor.
  :jira:`485`

- The *bosco_cluster* tool now clears out old installation files when
  the *--add* option is used to update an existing installation.
  :jira:`577`

- Fixed a bug that could cause the *condor_had* daemon to fail at startup
  when the local machine has multiple IP addresses.
  This bug is particularly likely to happen if ``PREFER_IPV4`` is set to
  ``False``.
  :jira:`625`

- For the machine ad attributes ``OpSys*`` and configuration parameters
  ``OPSYS*``, treat macOS 11.X as if it were macOS 10.16.X.
  This represents the major version numbers in a consistent, if somewhat
  inaccurate manner.
  :jira:`626`

- Fixed a bug that ignored the setting of per-Accounting Group
  GROUP_AUTOREGROUP from working.  Global autoregroup worked correctly.
  :jira:`632`

- A self-checkpointing job's output and error logs will no longer be
  interrupted by eviction if the job specifies ``transfer_checkpoint_files``;
  HTCondor now automatically considers them part of the checkpoint the way it
  automatically considers them part of the output.
  :jira:`656`

- HTCondor now transfers the standard output and error logs when
  ``when_to_transfer_output`` is ``ON_SUCCESS`` and ``transfer_output_files``
  is empty.
  :jira:`673`

- Fixed a bug that could cause the starter to crash after transferring files under
  certain rare circumstances.   This also corrected a problem which may have
  been causing the number of bytes transferred to be undercounted.
  :jira:`722`

.. _lts-version-history-905:

Version 9.0.5
-------------

Release Notes:

- HTCondor version 9.0.5 released on August 18, 2021.

New Features:

- If the SCITOKENS authentication method succeeds (that is, the client
  presented a valid SciToken) but the user-mapping fails, HTCondor will
  try the next authentication method in the list instead of failing.
  :jira:`589`

- The `bosco_cluster` command now creates backup files when the ``--override``
  option is used.
  :jira:`591`

- Improved the detection of Red Hat Enterprise Linux based distributions.
  Previously, only ``CentOS`` was recognized. Now, other distributions such
  as ``Scientific Linux`` and ``Rocky`` should be recognized.
  :jira:`609`

- The ``condor-boinc`` package is no longer required to be installed with
  HTCondor, thus making ``condor-boinc`` optional.
  :jira:`644`

Bugs Fixed:

- Fixed a bug on the Windows platform where *condor_submit* would crash
  rarely after successfully submitting a job.  This caused problems for programs
  that look at the return status of *condor_submit*, including *condor_dagman*
  :jira:`579`

- The job attribute ``ExitCode`` is no longer missing from the job ad after
  ``OxExitHold`` triggers.
  :jira:`599`

- Fixed a bug where running *condor_who* as a non-root user on a Unix
  system would print a confusing warning to stderr about running as
  non-root.
  :jira:`590`

- Fixed a bug where ``condor_gpu_discovery`` would not report any GPUs if
  any MIG-enabled GPU on the system were configured in certain ways.  Fixed
  a bug which could cause ``condor_gpu_discovery``'s output to become
  unparseable after certain errors.
  :jira:`476`

- HTCondor no longer ignores files in a job's spool directory if they happen
  to share a name with an entry in ``transfer_input_files``.  This allows
  jobs to specify the same file in ``transfer_input_files`` and in
  ``transfer_checkpoint_files``, and still resume properly after a checkpoint.
  :jira:`583`

- Fixed a bug where jobs running on Linux machines with cgroups enabled
  would not count files created in /dev/shm in the MemoryUsage attribute.
  :jira:`586`

- Fixed a bug in the *condor_now* tool, where the *condor_schedd* would
  not use an existing security session to run the selected job on the
  claimed resources.
  This could often lead to the job being unable to start.
  :jira:`603`


.. _lts-version-history-904:

Version 9.0.4
-------------

Release Notes:

-  HTCondor version 9.0.4 released on July 29, 2021.

New Features:

-  None.

Bugs Fixed:

-  *Security Items*: This release of HTCondor fixes security-related bugs
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004>`_.

   :jira:`509`
   :jira:`587`


.. _lts-version-history-903:

Version 9.0.3
-------------

Release Notes:

-  HTCondor version 9.0.3 released on July 27, 2021 and pulled two days later when an issue was found with a patch.

New Features:

-  None.

Bugs Fixed:

-  None.

.. _lts-version-history-902:

Version 9.0.2
-------------

Release Notes:

- HTCondor version 9.0.2 released on July 8, 2021.

- Removed support for GRAM grid jobs.
  :jira:`561`

New Features:

- HTCondor can now be configured to only use FIPS 140-2 approved security
  functions by using the new configuration template: ``use security:FIPS``.
  :jira:`319`

- Added new command-line flag to `condor_gpu_discovery`, ``-divide``,
  which functions like ``-repeat``, except that it divides the GPU attribute
  ``GlobalMemoryMb`` by the number of repeats (and adds the GPU
  attribute ``DeviceMemoryMb``, which is the undivided total).  To enable
  this new behavior, modify ``GPU_DISCOVERY_EXTRA`` appropriately.
  :jira:`454`

- The maximum line length for ``STARTD_CRON`` and ``SCHEDD_CRON`` job output
  has been extended from 8k bytes to 64k bytes.
  :jira:`498`

- Added two new commands to *condor_submit* - ``use_scitokens`` and ``scitokens_file``.
  :jira:`508`

- Reduced `condor_shadow` memory usage by 40% or more on machines with many
  (more than 64) cores.  This allows a correspondingly greater number of shadows and thus
  jobs to run on these submit machines.
  :jira:`540`

- Added support for using an authenticated SMTP relay on port 587 to
  condor_mail.exe on Windows.
  :jira:`303`

- The `condor_job_router_info` tool will now show info for a rootly JobRouter
  even when the tool is not running as root.  This change affects the way
  jobs are matched when using the ``-match`` or ``-route`` options.
  :jira:`525`

- *condor_gpu_discovery* now recognizes Capability 8.6 devices and reports the
  correct number of cores per Compute Unit.
  :jira:`544`

- Added command line option ``--copy-ssh-key`` to *bosco_cluster*. When set
  to `no`, this option prevents *bosco_cluster* from installing an ssh
  key on the remote system, and assume passwordless ssh is already
  possible.
  :jira:`270`

- Update to be able to link in scitokens-cpp library directly, rather than
  always using dlopen(). This allows SciTokens to be used with the conda-forge
  build of HTCondor.
  :jira:`541`

Bugs Fixed:

- When a Singularity container is started, and the test is run before the job,
  and the test fails, the job is now put back to idle instead of held.
  :jira:`539`

- Fixed Munge authentication, which was broken starting with HTCondor 8.9.9.
  :jira:`378`

- Fixed a bug in the Windows MSI installer where installation would only succeed
  at the default location of ``C:\Condor``.
  :jira:`543`

- Fixed a bug that prevented docker universe jobs from running on machines
  whose hostnames were longer than about 60 characters.
  :jira:`473`

- Fixed a bug that prevented *bosco_cluster* from detecting the remote host's
  platform when it is running Scientific Linux 7.
  :jira:`503`

- Fixed a bug that caused the ``query-krb`` and ``delete-krb`` options of *condor_store_cred*
  to fail.  This bug also affected the Python bindings ``query_user_cred`` and ``delete_user_cred``
  methods
  :jira:`533`

- Attribute ``GridJobId`` is no longer removed from the job ad of grid-type
  ``batch`` jobs when the job enters ``Completed`` or ``Removed`` status.
  :jira:`534`

- Fixed a bug that could prevent HTCondor from noticing new events in job
  event logs, if those logs were being written from one machine and read
  from another via AFS.
  :jira:`463`

- Using expressions for values in the ads of grid universe jobs of type
  `batch` now works correctly.
  :jira:`507`

- Fixed a bug that prevented a  personal condor from running in a private
  user namespace.
  :jira:`550`

- Fixed a bug in the *condor_who* program that caused it to hang on Linux
  systems, especially those running AFS or other shared filesystems.
  :jira:`530`
  :jira:`573`

- Fixed a bug that cause the *condor_master* to hang for up to two minutes
  when shutting down, if it was configured to be a personal condor.
  :jira:`548`

- When a grid universe job of type ``nordugrid`` fails on the remote system,
  the local job is now put on hold, instead of automatically resubmitted.
  :jira:`535`

- Fixed a bug that caused SSL authentication to crash on rare occasions.
  :jira:`428`

- Added the missing Ceiling attribute to negotiator user priorities in the
  Python bindings.
  :jira:`560`

- Fixed a bug in DAGMan where `SUBMIT-DESCRIPTION` statements were incorrectly
  logging duplicate description warnings.
  :jira:`511`

- Add the libltdl library to the HTCondor tarball. This library was
  inadvertently omitted when streamlining the build process in version 8.9.12.
  :jira:`576`


.. _lts-version-history-901:

Version 9.0.1
-------------

Release Notes:

- HTCondor version 9.0.1 released on May 17, 2021.

- The installer for Windows will now replace the ``condor_config``
  file even on an update.  You must use ``condor_config.local`` or
  a configuration directory to customize the configuration if you wish
  to preserve configuration changes across updates.

Known Issues:

- There is a known issue with the installer for Windows where it does
  not honor the Administrator Access list set in the MSI permissions
  dialog on a fresh install.  Instead it will always set the
  Administrator access to the default value.

- MUNGE security is temporarily broken.

New Features:

- The Windows MSI installer now sets up user-based authentication and creates
  an IDTOKEN for local administration.
  :jira:`407`

- When the ``AssignAccountingGroup`` configuration template is in effect
  and a user submits a job with a requested accounting group that they are not
  permitted to use, the submit will be rejected with an error message.
  This configuration template has a new optional second argument that can be used
  to quietly ignore the requested accounting group instead.
  :jira:`426`

- Added the OpenBLAS environment variable ``OPENBLAS_NUM_THREADS`` to the list
  of environment variables exported by the *condor_starter* per these
  `recommendations <https://github.com/xianyi/OpenBLAS/wiki/faq#how-can-i-use-openblas-in-multi-threaded-applications>`_.
  :jira:`444`

- HTCondor now parses ``/usr/share/condor/config.d/`` for configuration before
  ``/etc/condor/config.d``, so that packagers have a convenient place to adjust
  the HTCondor configuration.
  :jira:`45`

- Added a boolean option ``LOCAL_CREDMON_TOKEN_USE_JSON`` for the local issuer
  *condor_credmon_oauth* that is used to decide whether or not the bare token
  string in a generated access token file is wrapped in JSON. Default is
  ``LOCAL_CREDMON_TOKEN_USE_JSON = true`` (wrap token in JSON).
  :jira:`367`

Bugs Fixed:

- Fixed a bug where sending an updated proxy to an execute node could
  cause the *condor_starter* to segfault when AES encryption was enabled
  (which is the default).
  :jira:`456`
  :jira:`490`

- Fixed a bug with jobs that require running on a different machine
  after a failure by referring to MachineAttrX attributes in their
  requirements expression.
  :jira:`434`

- Fixed a bug in the way ``AutoClusterAttrs`` was calculated that could
  cause matchmaking to ignore attributes changed by ``job_machine_attrs``.
  :jira:`414`

- Fixed a bug in the implementation of the submit commands ``max_retries``
  and ``success_exit_code`` which would cause jobs which exited on a
  signal to go on hold (instead of exiting or being retried).
  :jira:`430`

- Fixed a memory leak in the job router, usually triggered when job
  policy expressions cause removal of the job.
  :jira:`408`

- Fixed some bugs that caused ``bosco_cluster --add`` to fail.
  Allow ``remote_gahp`` to work with older Bosco installations via
  the ``--rgahp-script`` option.
  Fixed security authorization failure between *condor_gridmanager*
  and *condor_ft-gahp*.
  :jira:`433`
  :jira:`438`
  :jira:`451`
  :jira:`452`
  :jira:`487`

- Fixed a bug in *condor_submit* when a ``SEC_CREDENTIAL_PRODUCER`` was
  configured that could result in *condor_submit* reporting that the
  Queue statement of a submit file was missing or invalid.
  :jira:`427`

- Fixed a bug in the local issuer *condor_credmon_oauth* where SciTokens version
  2.0 tokens were being generated without an "aud" claim. The "aud" claim is now
  set to ``LOCAL_ISSUER_TOKEN_AUDIENCE``. The "ver" claim can be changed from
  the default of "scitokens:2.0" by setting ``LOCAL_ISSUER_TOKEN_VERSION``.
  :jira:`445`

- Fixed several bugs that could result in the *condor_token_* tools aborting with
  a c++ runtime error on newer versions of Linux.
  :jira:`449`

.. _lts-version-history-900:

Version 9.0.0
-------------

Release Notes:

- HTCondor version 9.0.0 released on April 14, 2021.

- The installer for Windows platforms was not ready for 9.0.0.
  Windows support will appear in 9.0.1.

- Removed support for CREAM and Unicore grid jobs, glexec privilege separation, DRMAA, and *condor_cod*.

Known Issues:

- MUNGE security is temporarily broken.

- The *bosco_cluster* command is temporarily broken.

New Features:

- A new tool *condor_check_config* can be used after an upgrade when you had a working
  condor configuration before the upgrade. It will report configuration values that should be changed.
  In this version the tool for a few things related to the change to a more secure configuration by default.
  :jira:`384`

- The *condor_gpu_discovery* tool now defaults to using ``-short-uuid`` form for GPU ids on machines
  where the CUDA driver library has support for them. A new option ``-by-index`` has been added
  to select index-based GPU ids.
  :jira:`145`

Bugs Fixed:

- Fixed a bug introduced in 8.9.12 where the condor_job_router inside a CE would crash when
  evaluating periodic expressions
  :jira:`402`
