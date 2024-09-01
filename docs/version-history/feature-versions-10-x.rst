Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.9.0
--------------

Release Notes:

- HTCondor version 10.9.0 released on September 28, 2023.

- This version includes all the updates from :ref:`lts-version-history-1009`.

New Features:

- None.

Bugs Fixed:

- None.

Version 10.8.0
--------------

Release Notes:

- HTCondor version 10.8.0 released on September 14, 2023.

- The packaged builds (RPMs and debs) have been reorganized.
  We no longer wish to support the ClassAd library and it has been folded into
  the main ``condor`` package. The ``condor-blahp`` and ``condor-procd`` packages
  have also been folded into the ``condor`` package.
  :jira:`1981`

- On Debian based systems, the HTCondor's ``libexec`` directory has moved to
  the more standard ``/usr/libexec/condor``.
  :jira:`1981`

- The Debian packaging has been aligned with the RPM packaging.
  The package names are now ``condor`` and ``minicondor``.
  The ``condor-kbdd`` package has been split out, since many installations
  are server based and do not require the keyboard daemon and all of its
  dependencies on the X Window system. Also, the ``condor-vm-gahp`` package
  has been split out for sites that do not want to support VM Universe and
  the ``libvirt`` dependencies that come along with it.
  :jira:`1987`

- This version includes all the updates from :ref:`lts-version-history-1008`.

New Features:

- In an HTCondor Execution Point started by root on Linux, the default
  for cgroups memory has changed to be enforcing.  This means that
  jobs that use more then their provisioned memory will be put
  on hold with an appropriate hold message. *condor_q -hold* will show
  that message.  The previous default can be restored by setting
  :macro:`CGROUP_MEMORY_LIMIT_POLICY` = none on the Execution points.
  :jira:`1974`

- Added a ``-gpus`` option to :tool:`condor_status`. With this option :tool:`condor_status`
  will show only machines that have GPUs provisioned; and it will show information
  about the GPU properties.
  :jira:`1958`

- The output of :tool:`condor_status` when using the ``-compact`` option has been improved
  to show a separate row for the second and subsequent slot type for machines that have
  multiple slot types. Also the totals now count slots that have the ``BackfillSlot``
  attribute under the ``Backfill`` or ``BkIdle`` columns.
  :jira:`1957`

- Added new DAG command ``ENV`` for DAGMan. This command allows users to specify
  environment variables to be added into the DAGMan job proper's environment either
  by setting values explicitly or getting them from the environment the job is
  submitted from.
  :jira:`1955`

- Improved output for ``htcondor dag status`` command to include more information
  about the specified DAG.
  :jira:`1951`

- Updated DAGMan to utilize the ``-reason`` flag to add a message about why
  a job was removed when DAGMan removes managed jobs via :tool:`condor_rm` for some
  reason.
  :jira:`1950`

- Partitionable slots can now be directly claimed by a *condor_schedd*
  (i.e. the :ad-attr:`State` of the partitionable slot changes to ``Claimed``).
  While a slot is claimed, no other *condor_schedd* is able to create
  new dynamic slots to run jobs.
  This is controlled by the new configuration parameter
  :macro:`ENABLE_CLAIMABLE_PARTITIONABLE_SLOTS` and is disabled by
  default.
  :jira:`1824`

- By default, the user event logs are no longer fsync'd by the *condor_schedd*.  This
  should improve the performance of the *condor_schedd*, especially when the user's event
  logs are on non-solid state disks.  There is a knob to revert to the old
  semantics, ENABLE_USERLOG_FSYNC, which defaults to false.
  :jira:`1550`

- A new configuration variable :macro:`ALLOW_SUBMIT_FROM_KNOWN_USERS_ONLY` was
  added to allow administrators to restrict job submission to users that have
  already been added to the *condor_schedd* using the :tool:`condor_qusers` tool.
  :jira:`1934`

- Updated *condor_upgrade_check* script to check and warn about known incompatibilities
  introduced in the feature series for HTCondor ``V10`` that can cause issues when
  upgrading to a newer version (i.e. HTCondor ``V23``).
  :jira:`1960`

- Self-checkpointing jobs may now include the time spent generating
  successfully-stored checkpoints as part of their `CommittedTime`
  job ad attribute.
  :jira:`1942`

Bugs Fixed:

- Fixed a bug introduced in 10.5.0 that caused jobs to fail to start
  if they requested an OAuth credential whose service name included
  an asterisk.
  :jira:`1966`

- Fixed bugs in :tool:`condor_store_cred` that could cause it to crash or
  write incorrect data for the pool password.
  :jira:`1587`

- Fixed a bug with :tool:`condor_ssh_to_job` where it would fail if the Execution
  point was behind CCB, and the command was run immediately after the job
  started.
  :jira:`1979`

- Some support scripts for the ``htcondor annex`` command are now
  properly installed as executable.
  :jira:`1984`

- Fixed a bug where :tool:`condor_remote_cluster` could get stuck in a loop
  while installing files into an NFS directory.
  :jira:`2023`

Version 10.7.1
--------------

- HTCondor version 10.7.1 released on August 9, 2023.

New Features:

- None.

Bugs Fixed:

- Fixed inefficiency in DAGMan setting a nodes descendants to futile status
  which would result in DAGMan taking an extremely long time when a node fails
  in a very large and bushy DAG.
  :jira:`1945`

Version 10.7.0
--------------

Release Notes:

- HTCondor version 10.7.0 released on July 31, 2023.

- This version includes all the updates from :ref:`lts-version-history-1007`.

- Add support for Debian 12 (bookworm).
  :jira:`1938`

New Features:

- A single HTCondor pool can now have multiple *condor_defrag* daemons running
  and they will not interfere with each other so long as each has
  :macro:`DEFRAG_REQUIREMENTS` that select mutually exclusive subsets of the pool.
  :jira:`1903`

- If a job does not define any of the periodic policy expressions (like
  periodic_hold), HTCondor no longer sets a default value (like false) in the
  job ad.  The system knows that if these aren't set, to take the default action.
  This removes about 10% of the attributes in a job ad, with corresponding 
  benefits for all consumers of the job ad.
  :jira:`1919`

- Added submit command **want_io_proxy**.
  This replaces the old command **+WantIOProxy**.
  :jira:`1875`

- Apptainer is now included in the tarballs.
  :jira:`1932`


Bugs Fixed:

- Fixed bug introduced in 10.5.0 on cgroup v1 systems where the
  user and system CPU time measured was low by a factor of 10,000.
  :jira:`1920`

- Fixed a bug introduced in ``V10.5.0`` of HTCondor where the ``.job.ad`` and
  ``.machine.ad`` failed to be written to a ``local`` universe jobs scratch
  directory because of the *condor_starter* having the wrong permissions.
  :jira:`1912`

- If the collector is storing offline ads via COLLECTOR_PERSISTENT_AD_LOG
  the :tool:`condor_preen` tool will no longer delete that file
  :jira:`1874`

- Fixed a bug where empty execute sandboxes failed to be cleaned up on the
  Execution Point when using Startd disk enforcement.
  :jira:`1821`

- When using Startd disk enforcement, if a *condor_starter* running a container
  or VM universe job is abruptly killed (like SIGABRT) then the *condor_startd*
  would fail to cleanup the running docker container or VM and underlying logical
  volume.
  :jira:`1895`

Version 10.6.0
--------------

Release Notes:

- HTCondor version 10.6.0 released on June 29, 2023.

- This version includes all the updates from :ref:`lts-version-history-1006`.

New Features:

- Added the :ref:`man-pages/condor_qusers:*condor_qusers*` command to monitor and control users at the Access Point.
  Users disabled at the Access Point are no longer allowed to submit jobs.  Jobs submitted
  before the user was disabled are allowed to run to completion.  When a user
  is disabled, an optional reason string can be provided.  The reason will be
  included in the error message from :tool:`condor_submit` when submission is refused
  because the user is disabled.
  :jira:`1723`
  :jira:`1835`

- Mitigate a memory leak in the *arc_gahp* with libcurl when it uses
  NSS for security.
  When an *arc_gahp* process has handled a certain number of commands,
  a new *arc_gahp* is started and old process exits.
  The number of commands that triggers a new process is controlled by
  new configuration parameter :macro:`ARC_GAHP_COMMAND_LIMIT`.
  :jira:`1778`

- Container universe jobs may now specify the *container_image* to
  be an image transferred via a file transfer plugin.
  :jira:`1820`

- Added two new functions for using ClassAd expressions. The ``stringListSubsetMatch`` and
  ``stringListISubsetMatch`` functions can be used to check if all of the members of a
  stringlist are also in a target stringlist.  A single ``stringListSubsetMatch`` function
  call can replace a whole set of ``stringListMember`` calls once the whole pool is
  updated to 10.6.0.
  :jira:`1817`

- Added a new automatic submit file macro ``$(JobId)`` which expands to the full
  id of the submitted job.
  :jira:`1836`

- The job's executable is no longer renamed to *condor_exec.exe* when
  the job's sandbox is transferred to the Execution Point.
  :jira:`1227`

Bugs Fixed:

- condor_restd service in the htcondor/mini container no longer crashes
  on startup due to the `en_US.UTF-8` locale being unavailable.
  :jira:`1785`

- Fixed a bug that would very rarely cause :tool:`condor_wait` to hang forever.
  :jira:`1792`

Version 10.5.1
--------------

- HTCondor version 10.5.1 released on June 6, 2023.

New Features:

- None.

Bugs Fixed:

- For grid universe jobs of type **batch**, detecting if a Slurm
  system is functioning now works with older versions of Slurm.
  :jira:`1826`

Version 10.5.0
--------------

Release Notes:

- HTCondor version 10.5.0 released on June 5, 2023.

- This version includes all the updates from :ref:`lts-version-history-1004`.

- Add support for Amazon Linux 2023. VOMS authentication is omitted on this
  platform.
  :jira:`1742`

New Features:

- Added new **Save File** functionality to DAGMan which allows users to
  specify DAG nodes as save points to record the current DAG's progress
  in a file similar to a rescue file. These files can then be specified
  with the new :tool:`condor_submit_dag` flag ``load_save`` to re-run the
  DAG from that point of progression. For more information visit :ref:`DAG Save Files`.
  :jira:`1636`

- The admin knob :macro:`SUBMIT_ALLOW_GETENV` when set to false, now allows
  submit files to use any value but *true* for their ``getenv = ...``
  commands.
  :jira:`1671`

- Improved throughput when submitting a large number of ARC CE jobs.
  Previously, jobs could remain stalled for a long time in the ARC CE
  server waiting for their input sandbox to be transferred while other
  were being submitted.
  :jira:`1666`

- The *arc_gahp* can now issue multiple HTTPS requests in parallel in
  different threads. This is controlled by the new configuration
  parameter :macro:`ARC_GAHP_USE_THREADS`.
  :jira:`1690`

- The Execute event in the user log now prints out slot name, sandbox path
  and resource quantities of execution slot.
  :jira:`1722`

- Added new submit command ``ulog_execute_attrs`` for a jobs submit file. This
  command takes a comma-separated list of machine ClassAd attributes to be
  written to the user logs execute event.
  :jira:`1759`

- Added new DAGMan configuration macro :macro:`DAGMAN_RECORD_MACHINE_ATTRS`
  to give a list of machine attributes that will be added to DAGMan submitted
  jobs for recording in the various produced job ads and userlogs.
  :jira:`1717`

- The :tool:`condor_transform_ads` tool can now read a configuration file containing
  ``JOB_TRANSFORM_<name>`` or ``JOB_ROUTER_ROUTE_<name>`` and then apply
  any or all of the transforms declared in that file.  This makes it
  easier to test job transforms before deploying them.
  :jira:`1710`

- Linux Cgroup support has been redone in a way that doesn't depend on
  using the procd.  There should be no user visible changes in
  the usual cases.
  :jira:`1589`

Bugs Fixed:

- Expanded default list of environment variables to include in the DAGMan
  proper manager jobs getenv to include ``HOME``, ``USER``, ``LANG``, and
  ``LC_ALL``. Thus resulting in these variables appearing in the DAGMan
  manager jobs environment.
  :jira:`1725`

- Fixed a bug on cgroup v2 systems where memory limits over 2 gigabytes would
  not be enforced correctly.
  :jira:`1775`

- HTCondor no longer puts jobs using cgroup v1 into the blkio controller.
  HTCondor never put limits on the i/o, and some kernel version panicked
  and crashed when they had active jobs in the blkio controller.
  :jira:`1786`

- Forced condor_ssh_to_job to never try to use a Control Master, which would
  break ssh_to_job.  Also raised the timeout for ssh_to_job which might
  be needed for slow WANs.
  :jira:`1782`

- Fixed a bug when running with root on a Linux systems with cgroup v1
  that would print a warning to the StarterLog claiming
  Warning: cannot chown /sys/fs/cgroup/cpu,cpuset
  :jira:`1672`

- Fixed a bug where :tool:`condor_history` would fail to find history files
  for a remote query if the various history configuration macros were
  specified with subsystem prefixes i.e. ``SCHEDD.HISTORY = /path``
  :jira:`1739`

- When started on a systemd system, HTCondor will now wait for the SSSD
  service to start.  Previously it only waited for ypbind.
  :jira:`1655`

- Fixed a bug in :tool:`condor_preen` that would remove any recorded job epoch
  history files stored in the spool directory.
  :jira:`1738`

Version 10.4.3
--------------

Release Notes:

- HTCondor version 10.4.3 released on May 9, 2023.

- Tarballs in this release contain the recent scitokens-cpp 1.0.1 library.
  :jira:`1779`

New Features:

- None.

Bugs Fixed:

- The ce-audit collector plug-in should no longer crash.
  :jira:`1774`

Version 10.4.2
--------------

- HTCondor version 10.4.2 released on May 2, 2023.

New Features:

- None.

Bugs Fixed:

- Fixed a bug introduced in HTCondor 10.0.3 that caused remote
  submission of **batch** grid universe jobs via ssh to fail when
  attempting to do file transfer.
  :jira:`1747`

- Fixed a bug where the HTCondor-CE would fail to handle any of its
  jobs after a restart.
  :jira:`1755`

Version 10.4.1
--------------

Release Notes:

- HTCondor version 10.4.1 released on April 12, 2023.

- Preliminary support for Ubuntu 20.04 (Focal Fossa) on PowerPC (ppc64el).
  :jira:`1668`

New Features:

- None.

Bugs Fixed:

- :tool:`condor_remote_cluster` now works correctly when the hardware
  architecture of the remote machine isn't x86_64.
  :jira:`1670`

Version 10.4.0
--------------

Release Notes:

- HTCondor version 10.4.0 released on April 6, 2023.

- This version includes all the updates from :ref:`lts-version-history-1003`.

- HTCondor will no longer pass all environment variables to the DAGMan proper manager jobs environment.
  This may result in DAGMan and its various parts (primarily PRE, POST,& HOLD Scripts) to start failing
  or change behavior due to missing needed environment variables. To revert back to the old behavior or
  add the missing environment variables to the DAGMan proper jobs environment set the
  :macro:`DAGMAN_MANAGER_JOB_APPEND_GETENV` configuration option.
  :jira:`1580`

- The *condor_startd* will no longer advertise *CpuBusy* or *CpuBusyTime*
  unless the configuration template ``use POLICY : DESKTOP`` or ``use POLICY : UWCS_DESKTOP``
  is used. Those templates will cause *CpuBusyTime* to be advertised as a time value and not
  a duration value. The policy expressions in those templates have been modified
  to account for this fact. If you have written policy expressions of your own that reference
  *CpuBusyTime* you will need to modify them to use ``$(CpuBusyTimer)`` from one of those templates
  or make the equivalent change.
  :jira:`1502`

New Features:

- DAGMan no longer sets ``getenv = true`` in the ``.condor.sub`` file  while adding the
  ability to better control the environment passed to the DAGMan proper job.
  ``getenv`` will default to ``CONDOR_CONFIG,_CONDOR_*,PATH,PYTHONPATH,PERL*,PEGASUS_*,TZ``
  in the ``.condor.sub`` file which can be appended to via the
  :macro:`DAGMAN_MANAGER_JOB_APPEND_GETENV` or the new :tool:`condor_submit_dag` flag
  ``include_env``. Also added new :tool:`condor_submit_dag` flag ``insert_env`` to
  directly set key=value pairs of information into the ``.condor.sub`` environment.
  :jira:`1580`

- New configuration parameter `SEC_SCITOKENS_FOREIGN_TOKEN_ISSUERS``
  restricts which issuers' tokens will be accepted under
  ``SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES``.
  Updated default values allow EGI CheckIn tokens to be accepted under
  the SCITOKENS authentication method.
  :jira:`1515`

- The *condor_startd* can now be configured to evaluate a set of expressions
  defined by :macro:`STARTD_LATCH_EXPRS`.  For each expression, the last
  evaluated value will be advertised as well as the time that the evaluation
  changed to that value.  This new generic mechanism was used to add a new
  slot attribute *NumDynamicSlotsTime* that is the last time a dynamic slot
  was created or destroyed.
  :jira:`1502`

- Add new field ``ContainerDuration`` to TransferInput attribute of 
  jobs that measure the number of seconds to transfer the 
  Apptainer/Singularity image.
  :jira:`1588`

- For grid universe jobs of type **batch**, add detection of when the
  target batch system is unreachable or not functioning. When this is
  the case, HTCondor marks the resource as unavailable instead of
  putting the affected jobs on hold. This matches the behavior for
  other grid universe job types.
  Grid ads in the collector now contain attributes
  :ad-attr:`GridResourceUnavailableTimeReason` and
  :ad-attr:`GridResourceUnavailableTimeReasonCode`, which give details about
  why the remote scheduling system is considered unavailable.
  :jira:`1582`

- Added ability for DAGMan to automatically record the Node Retry attempt in that
  nodes job ad. This is done by setting the new configuration option :macro:`DAGMAN_NODE_RECORD_INFO`.
  :jira:`1634`

Bugs Fixed:

- Fixed a bug where if the docker command emitted warnings to stderr, the
  *condor_startd* would not correctly advertise the amount of used image cache.
  :jira:`1645`

- Fixed a bug where :tool:`condor_history` would fail if the job history
  file doesn't exist.
  :jira:`1578`

- Fixed a bug in the view server where it would assert and exit if
  the view server stats file are deleted at just the wrong time.
  :jira:`1599`

- Fixed a bug where *condor_shadow* was unable to write the job ad to the
  :macro:`JOB_EPOCH_HISTORY` file when located in condor owned directories
  such as the spool directory.
  :jira:`1631`

- Remove warning when installing HTCondor RPMs on Enterprise Linux 9.
  :jira:`1571`

Version 10.3.1
--------------

- HTCondor version 10.3.1 released on March 7, 2023.

New Features:

- The *condor_startd* now advertises whether there appears to be
  a useful /usr/sbin/sshd on the system, in order for :tool:`condor_ssh_to_job`
  to work.
  :jira:`1614`

Bugs Fixed:

- None.

Version 10.3.0
--------------

Release Notes:

- HTCondor version 10.3.0 released on March 6, 2023.

- This version includes all the updates from :ref:`lts-version-history-1002`.

- When HTCondor is configured to use cgroups, if the system
  as a whole is out of memory, and the kernel kills a job with the out
  of memory killer, HTCondor now checks to see if the job is below
  the provisioned memory.  If so, HTCondor now evicts the job, and
  marks it as idle, not held, so that it might start again on a 
  machine with sufficient resources. Previous, HTCondor would let
  this job attempt to run, hoping the next time the OOM killer fired
  it would pick a different process.
  :jira:`1512`

- This version changes the semantics of the ``output_destination`` submit
  command.  It no longer sends the files named by the ``output`` or
  ``error`` submit commands to the output destination.  Submitters may
  instead specify those locations with URLs directly.
  :jira:`1365`

New Features:

- When HTCondor has root, and is running with cgroups, the cgroup the job is
  in is writeable by the job. This allows the job (perhaps a glidein)
  to sub-divide the resource limits it has been given, and allocate
  subsets of those to its child processes.
  :jira:`1496`

- Added capabilities for per job run instance history recording. Where during
  the *condor_shadow* daemon's shutdown it will write the current job ad
  to a file designated by :macro:`JOB_EPOCH_HISTORY` and/or a directory
  specified by :macro:`JOB_EPOCH_HISTORY_DIR`. These per run instance
  job ad records can be read via :tool:`condor_history` using the new ``-epochs``
  option. This behavior is not turned on by default. Setting either of the
  job epoch location config knobs above will turn on this behavior.
  :jira:`1104`

- Added new :tool:`condor_history` ``-search`` option that takes a filename
  to find all matching condor time rotated files ``filename.YYYYMMDDTHHMMSS``
  to read from instead of using any default files.
  :jira:`1514`

- Added new :tool:`condor_history` ``-directory`` option to use a history sources
  alternative configured directory knob such as :macro:`JOB_EPOCH_HISTORY_DIR`
  to search for history.
  :jira:`1514`

- Added ability to set a gangliad metrics lifetime (DMAX value) within the
  metric definition language with the new ``Lifetime`` keyword.
  :jira:`1547`

- Added configuration knob :macro:`GANGLIAD_MIN_METRIC_LIFETIME` to set
  the minimum value for gangliads calculated metric lifetime (DMAX value)
  for all metrics without a specified ``Lifetime``.
  :jira:`1547`

- Added an attribute to the *condor_schedd* classad that advertises the number of
  late materialization jobs that have been submitted, but have not yet materialized.
  The new attribute is called ``JobsUnmaterialized``
  :jira:`1591`

- The *linux_kernel_tuning_script*, run by the :tool:`condor_master` at startup,
  now tries to increase the value of /proc/sys/fs/pipe-user-pages-soft
  to 128k, if it was below this.  This improves the scalability of the
  *condor_schedd* when running more than 16k jobs from any one user.
  :jira:`1556`

- The *linux_kernel_tuning_script*, run by the :tool:`condor_master` at startup,
  no longer tries to mount the various cgroup filesystems.  We assume that
  any reasonable Linux system will have done this in a manner that it
  deems appropriate.
  :jira:`1528`

- Linux worker nodes now advertise *DockerCachedImageSizeMb*, the number of
  megabytes that are used in the docker image cache.
  :jira:`1494`

- When a file-transfer plug-in aborts due to lack of progress, the message
  now includes the ``https_proxy`` (or ``http_proxy``) environment variable,
  and the phrasing has been changed to avoid suggesting that the plug-in
  actually respected it.
  :jira:`1473`

Bugs Fixed:

- Added support for older cgroup v2 systems with missing memory.peak
  files in the memory controller.
  :jira:`1529`

- The HTCondor starter now removes any cgroup that it has created for
  a job when it exits.
  :jira:`1500`

- Fixed bug where ``condor_history`` would occasionally fail to display
  all matching user requested job ids.
  :jira:`1506`

- Fixed bugs in how the *condor_collector* generated its own CA and host
  certificate files.
  Configuration parameter :macro:`COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE` now
  defaults to ``True`` on Unix platforms.
  Configuration parameters :macro:`AUTH_SSL_SERVER_CERTFILE` and 
  :macro:`AUTH_SSL_SERVER_KEYFILE` can now be a list of files. The first pair of
  files with valid credentials is used.
  :jira:`1455`

- Added missing environment variables for the SciTokens plugin.
  :jira:`1516`

Version 10.2.5
--------------

- HTCondor version 10.2.5 released on February 28, 2023.

New Features:

- None.

-Bugs Fixed:

- Fixed an issue where after a *condor_schedd* restart, the
  ``JobsUnmaterialized`` attribute in the *condor_schedd* ad may be an
  overcount of the number of unmaterialized jobs in rare cases.
  :jira:`1606`

Version 10.2.4
--------------

Release Notes:

- HTCondor version 10.2.4 released on February 24, 2023.

New Features:

- None.

Bugs Fixed:

- Fixed an issue where after a *condor_schedd* restart, the
  ``JobsUnmaterialized`` attribute in the *condor_schedd* ad may be an
  undercount of the number of unmaterialized jobs for previous submissions.
  :jira:`1591`

Version 10.2.3
--------------

- HTCondor version 10.2.3 released on February 21, 2023.

New Features:

- Added an attribute to the *condor_schedd* ClassAd that advertises the number of
  late materialization jobs that have been submitted, but have not yet materialized.
  The new attribute is called ``JobsUnmaterialized``.
  :jira:`1591`

Bugs Fixed:

- None.

Version 10.2.2
--------------

Release Notes:

- HTCondor version 10.2.2 released on February 7, 2023.

New Features:

- None.

Bugs Fixed:

- Fixed bugs with configuration knob :macro:`SINGULARITY_USE_PID_NAMESPACES`.
  :jira:`1574`

Version 10.2.1
--------------

- HTCondor version 10.2.1 released on January 24, 2023.

New Features:

- Improved scalability of *condor_schedd* when running more than 1,000 jobs
  from the same user.
  :jira:`1549`

- :tool:`condor_ssh_to_job` should now work in glidein and other environments
  where the job or HTCondor is running as a Unix user id that doesn't
  have an entry in the /etc/passwd database.
  :jira:`1543`

Bugs Fixed:

- In the Python bindings, the attribute :ad-attr:`ServerTime` is now included
  in job ads returned by ``Schedd.query()``.
  :jira:`1531`

- Fixed issue when HTCondor could not be installed on Ubuntu 18.04
  (Bionic Beaver).
  :jira:`1548`

Version 10.2.0
--------------

Release Notes:

- HTCondor version 10.2.0 released on January 5, 2023.

- This version includes all the updates from :ref:`lts-version-history-1001`.

- We changed the semantics of relative paths in the ``output``, ``error``, and
  ``transfer_output_remaps`` submit file commands.  These commands now create
  the directories named in relative paths if they do not exist.  This could
  cause jobs that used to go on hold (because they couldn't write their
  ``output`` or ``error`` files, or a remapped output file) to instead succeed.
  :jira:`1325`
  
- HTCondor can now put a job in a Linux control (cgroup), not only if it has
  root privilege, but also if the administrator or some external entity
  has made the cgroup HTCondor is configured to use writeable by the
  non-rootly user a personal condor or glidein is running as.
  :jira:`1465`

- File-transfer plug-ins may no longer take as long as they like to finish.
  After :macro:`MAX_FILE_TRANSFER_PLUGIN_LIFETIME` seconds, the starter will
  terminate the transfer and report a time-out failure (with ``ETIME``, 62,
  as the hold reason subcode).
  :jira:`1404`

New Features:

- Add support for Enterprise Linux 9 on x86_64 and aarch64 architectures.
  :jira:`1285`

- Add support to the *condor_starter* for tracking processes via cgroup v2
  on Linux distributions that support cgroup v2.
  :jira:`1457`

- The *condor_negotiator* now support setting a minimum floor number of cores that any
  given submitter should get, regardless of their fair share.  This can be set or queried
  via the :tool:`condor_userprio` tool, in the same way that the ceiling can be set or get
  :jira:`557`

- Improved the validity testing of the Singularity / Apptainer container runtime software
  at *condor_startd* startup.  If this testing fails, slot attribute :ad-attr:`HasSingularity` will be
  set to ``false``, and attribute ``SingularityOfflineReason`` will contain error information.
  Also in the event of Singularity errors, more information is recorded into the *condor_starter*
  log file.
  :jira:`1431`

- :tool:`condor_q` default behavior of displaying the cumulative run time has changed
  to now display the current run time for jobs in running, transferring output,
  and suspended states while displaying the previous run time for jobs in idle or held
  state unless passed ``-cumulative-time`` to show the jobs cumulative run time for all runs.
  :jira:`1064`

- Docker Universe submit files now support *docker_pull_policy = always*, so
  that docker will check to see if the cached image is out of date.  This increases
  the network activity, may cause increased throttling when pulling from docker hub,
  and is recommended to be used with care.
  :jira:`1482`

- Added configuration knob :macro:`SINGULARITY_USE_PID_NAMESPACES`.
  :jira:`1431`

- :tool:`condor_history` will now stop searching history files once all requested job ads are
  found if passed ClusterIds or ClusterId.ProcId pairs.
  :jira:`1364`

- Improved :tool:`condor_history` search speeds when searching for matching jobs, matching clusters,
  and matching owners.
  :jira:`1382`

- The local issuer credmon can optionally add group authorizations to users' tokens by setting
  ``LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`` and ``LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE``.
  :jira:`1402`

- The :macro:`JOB_INHERITS_STARTER_ENVIRONMENT` configuration variable now accepts a list
  of match patterns just like the submit command ``getenv`` does.
  :jira:`1339`

- Declaring either ``container_image`` or ``docker_image`` without a defined ``universe``
  in a submit file will now automatically setup job for respective ``universe`` based on
  image type.
  :jira:`1401`

- Added new Scheduler ClassAd attribute :ad-attr:`EffectiveFlockList` that represents the
  *condor_collector* addresses that a *condor_schedd* is actively sending flocked jobs.
  :jira:`1389`

- Added new DAGMan node status called *Futile* that represents a node that will never run
  due to the failure of a node that the *Futile* node depends on either directly or
  indirectly through a chain of **PARENT/CHILD** relationships. Also, added a new ClassAd
  attribute :ad-attr:`DAG_NodesFutile` to count the number of *Futile* nodes in a **DAG**.
  :jira:`1456`

- Improved error handling in the *condor_shadow* and *condor_starter*
  when they have trouble talking to each other.
  :jira:`1360`

- Added support for plugins that can perform the mapping of a
  validated SciToken to an HTCondor canonical user name during
  security authentication.
  :jira:`1463`

- EGI CheckIn tokens can now be used to authenticate via the SCITOKENS
  authentication method.
  New configuration parameter :macro:`SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES`
  must be set to ``True`` to enable this usage.
  :jira:`1498`

Bugs Fixed:

- Fixed bug where :ad-attr:`HasSingularity` would be advertised as true in cases
  where it wouldn't work.
  :jira:`1274`

Version 10.1.3
--------------

Release Notes:

- HTCondor version 10.1.3 limited release on November 22, 2022.

New Features:

- Jobs run in Singularity or Apptainer container runtimes now use the
  SINGULARITY_VERBOSITY flag, which controls the verbosity of the runtime logging
  to the job's stderr.  The default value is "-s" for silent, meaning only
  fatal errors are logged.  
  :jira:`1436`

- The PREPARE_JOB and PREPARE_JOB_BEFORE_TRANSFER job hooks can now return a ``HookStatusCode`` and 
  a ``HookStatusMessage`` to give better feedback to the user.
  See the :ref:`admin-manual/ep-policy-configuration:Startd Cron` manual section.
  :jira:`1416`

- The local issuer credmon can optionally add group authorizations to users' tokens by setting
  ``LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`` and ``LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE``.
  :jira:`1402`

Bugs Fixed:

- None.

Version 10.1.2
--------------

- HTCondor version 10.1.2 limited release on November 15, 2022.

New Features:

- OpenCL jobs can now run inside a Singularity container launched by HTCondor if the
  OpenCL drivers are present on the host in directory ``/etc/OpenCL/vendors``.
  :jira:`1410`

Bugs Fixed:

- None.

Version 10.1.1
--------------

Release Notes:

- HTCondor version 10.1.1 released on November 10, 2022.

New Features:

- Improvements to job hooks, including configuration knob STARTER_DEFAULT_JOB_HOOK_KEYWORD,
  the new hook PREPARE_JOB_BEFORE_TRANSFER,
  and the ability to preserve stderr from job hooks into the StarterLog or StartdLog.
  See the :ref:`admin-manual/ep-policy-configuration:Hooks` manual section.
  :jira:`1400`

Bugs Fixed:

- Fixed bugs in the container universe that prevented 
  apptainer-only systems from running container universe jobs
  with Docker repository style images
  :jira:`1412`

Version 10.1.0
--------------

Release Notes:

- HTCondor version 10.1.0 released on November 10, 2022.

- This version includes all the updates from :ref:`lts-version-history-1000`.

New Features:

- None.

Bugs Fixed:

- None.

