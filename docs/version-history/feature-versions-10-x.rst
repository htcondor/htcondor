Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.4.0
--------------

Release Notes:

.. HTCondor version 10.4.0 released on Month Date, 2023.

- HTCondor version 10.4.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-1003`.

- We changed the semantics of relative paths in the ``output``, ``error``, and
  ``transfer_output_remaps`` submit file commands.  These commands now create
  the directories named in relative paths if they do not exist.  This could
  cause jobs that used to go on hold (because they couldn't write their
  ``output`` or ``error`` files, or a remapped output file) to instead succeed.
  :jira:`1325`

- The *condor_startd* will no longer advertise *CpuBusy* or *CpuBusyTime*
  unless the configuration template ``use FEATURE : DESKTOP`` or ``use FEATURE : UWCS_DESKTOP``
  is used. Those templates will cause *CpuBusyTime* to be advertised as a time value and not
  a duration value. The policy expressions in those templates have been modified
  to account for this fact. If you have written policy expressions of your own that reference
  *CpuBusyTime* you will need to modify them to use ``$(CpuBusyTimer)`` from one of those templates
  or make the equivalent change.

New Features:

- The *condor_startd* can now be configured to evaluate a set of expressions
  defined by :macro:`STARTD_LATCH_EXPRS`.  For each expression, the last
  evaluated value will be advertised as well as the time that the evaluation
  changed to that value.  This new generic mechanism was used to add a new
  slot attribute *NumDynamicSlotsTime* that is the last time a dynamic slot
  was created or destroyed.
  :jira:`1502`

- Added an attribute to the *condor_schedd* classad that advertises the number of
  late materialization jobs that have been submitted, but have not yet materialized.
  The new attribute is called ``JobsUnmaterialized``
  :jira:`1591`

- Add new field ```ContainerDuration``` to TransferInput attribute of 
  jobs that measure the number of seconds to transfer the 
  Apptainer/Singularity image.
  :jira:`1588`

- For grid universe jobs of type **batch**, add detection of when the
  target batch system is unreachable or not functioning. When this is
  the case, HTCondor marks the resource as unavailable instead of
  putting the affected jobs on hold. This matches the behavior for
  other grid universe job types.
  :jira:`1582`


Bugs Fixed:

- Fixed bug where the *condor_shadow* would crash during job removal
  :jira:`1585`

- Fixed a bug where *condor_history* would fail if the job history
  file doesn't exist.
  :jira:`1578`

- Fixed a bug in the view server where it would assert and exit if
  the view server stats file are deleted at just the wrong time.
  :jira:`1599`

Version 10.3.0
--------------

Release Notes:

.. HTCondor version 10.3.0 released on Month Date, 2023.

- HTCondor version 10.3.0 not yet released.

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

- This versions changes the semantics of the ``output_destination`` submit
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

- Linux worker nodes now advertise *DockerCachedImageSizeMb*, the number of
  megabytes that are used in the docker image cache.
  :jira:`1494`

- When a file-transfer plug-in aborts due to lack of progress, the message
  now includes the ``https_proxy`` environment variable, and the phrasing
  has been changed to avoid suggesting that the plug-in respected it (or
  ``http_proxy``).
  :jira:`1473`

- The *linux_kernel_tuning_script*, run by the *condor_master* at startup,
  no longer tries to mount the various cgroup filesystems.  We assume that
  any reasonable Linux system will have done this in a manner that it 
  deems appropriate.
  :jira:`1528`

- Added capabilities for per job run instance history recording. Where during
  the *condor_shadow* daemon's shutdown it will write the current job ad
  to a file designated by :macro:`JOB_EPOCH_HISTORY` and/or a directory
  specified by :macro:`JOB_EPOCH_HISTORY_DIR`. These per run instance
  job ad records can be read via *condor_history* using the new ``-epochs``
  option. This behavior is not turned on by default. Setting either of the
  job epoch location config knobs above will turn on this behavior.
  :jira:`1104`

- Added new *condor_history* ``-search`` option that takes a filename
  to find all matching condor time rotated files ``filename.YYYYMMDDTHHMMSS``
  to read from instead of using any default files.
  :jira:`1514`

- Added new *condor_history* ``-directory`` option to use a history sources
  alternative configured directory knob such as :macro:`JOB_EPOCH_HISTORY_DIR`
  to search for history.
  :jira:`1514`

- The *linux_kernel_tuning_script*, run by the *condor_master* at startup,
  now tries to increase the value of /proc/sys/fs/pipe-user-pages-soft
  to 128k, if it was below this.  This improves the scalability of the
  schedd when running more than 16k jobs from any one user.
  :jira:`1556`

- Added ability to set a gangliad metrics lifetime (DMAX value) within the
  metric definition language with the new ``Lifetime`` keyword.
  :jira:`1547`

- Added configuration knob :macro:`GANGLIAD_MIN_METRIC_LIFETIME` to set
  the minimum value for gangliads calculated metric lifetime (DMAX value)
  for all metrics without a specified ``Lifetime``.
  :jira:`1547`

Bugs Fixed:

- The HTCondor starter now removes any cgroup that it has created for
  a job when it exits.
  :jira:`1500`

- Added support for older cgroup v2 systems with missing memory.peak
  files in the memory controller.
  :jira:`1529`

- Fixed bug where ``condor_history`` would occasionally fail to display
  all matching user requested job ids.
  :jira:`1506`

- Fixed bugs in how the *condor_collector* generated its own CA and host
  certificate files.
  Configuration parameter ``COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE`` now
  defaults to ``True`` on Unix platforms.
  Configuration parameters ``AUTH_SSL_SERVER_CERTFILE`` and 
  ``AUTH_SSL_SERVER_KEYFILE`` can now be a list of files. The first pair of
  files with valid credentials is used.
  :jira:`1455`

- Added missing environment variables for the SciTokens plugin.
  :jira:`1516`

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

- Fixed bugs with configuration knob ``SINGULARITY_USE_PID_NAMESPACES``.
  :jira:`1574`

Version 10.2.1
--------------

- HTCondor version 10.2.1 released on January 24, 2023.

New Features:

- Improved scalability of *condor_schedd* when running more than 1,000 jobs
  from the same user.
  :jira:`1549`

- *condor_ssh_to_job* should now work in glidein and other environments
  where the job or HTCondor is running as a Unix user id that doesn't
  have an entry in the /etc/passwd database.
  :jira:`1543`

Bugs Fixed:

- In the Python bindings, the attribute ``ServerTime`` is now included
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
  via the *condor_userprio* tool, in the same way that the ceiling can be set or get
  :jira:`557`

- Improved the validity testing of the Singularity / Apptainer container runtime software
  at *condor_startd* startup.  If this testing fails, slot attribute ``HasSingularity`` will be
  set to ``false``, and attribute ``SingularityOfflineReason`` will contain error information.
  Also in the event of Singularity errors, more information is recorded into the *condor_starter*
  log file.
  :jira:`1431`

- *condor_q* default behavior of displaying the cumulative run time has changed
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

- *condor_history* will now stop searching history files once all requested job ads are
  found if passed ClusterIds or ClusterId.ProcId pairs.
  :jira:`1364`

- Improved *condor_history* search speeds when searching for matching jobs, matching clusters,
  and matching owners.
  :jira:`1382`

- The local issuer credmon can optionally add group authorizations to users' tokens by setting
  ``LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`` and ``LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE``.
  :jira:`1402`

- The ``JOB_INHERITS_STARTER_ENVIRONMENT`` configuration variable now accepts a list
  of match patterns just like the submit command ``getenv`` does.
  :jira:`1339`

- Declaring either ``container_image`` or ``docker_image`` without a defined ``universe``
  in a submit file will now automatically setup job for respective ``universe`` based on
  image type.
  :jira:`1401`

- Added new Scheduler ClassAd attribute ``EffectiveFlockList`` that represents the
  *condor_collector* addresses that a *condor_schedd* is actively sending flocked jobs.
  :jira:`1389`

- Added new DAGMan node status called *Futile* that represents a node that will never run
  due to the failure of a node that the *Futile* node depends on either directly or
  indirectly through a chain of **PARENT/CHILD** relationships. Also, added a new ClassAd
  attribute ``DAG_NodesFutile`` to count the number of *Futile* nodes in a **DAG**.
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
  New configuration parameter ``SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES``
  must be set to ``True`` to enable this usage.
  :jira:`1498`

Bugs Fixed:

- Fixed bug where ``HasSingularity`` would be advertised as true in cases
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
  See the :ref:`admin-manual/Hooks` manual section.
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
  See the :ref:`admin-manual/Hooks` manual section.
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

