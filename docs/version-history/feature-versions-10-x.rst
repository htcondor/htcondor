Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

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

- When a file-transfer plug-in aborts due to lack of progress, the message
  now includes the ``https_proxy`` environment variable, and the phrasing
  has been changed to avoid suggesting that the plug-in respected it (or
  ``http_proxy``).
  :jira:`1471`

- The *linux_kernel_tuning_script*, run by the *condor_master* at startup,
  no longer tries to mount the various cgroup filesystems.  We assume that
  any reasonable Linux system will have done this in a manner that it 
  deems appropriate.
  :jira:`1528`

- *condor_ssh_to_job* should now work in glidein and other environments
  where the job or HTCondor is running as a Unix user id that doesn't
  have an entry in the /etc/passwd database.
  :jira:`1543`

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
  defaults to ``True`` on unix platorms.
  Configuration parameters ``AUTH_SSL_SERVER_CERTFILE`` and 
  ``AUTH_SSL_SERVER_KEYFILE`` can now be a list of files. The first pair of
  files with valid credentials is used.
  :jira:`1455`

- Added missing environment variables for the SciTokens plugin.
  :jira:`1516`

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

