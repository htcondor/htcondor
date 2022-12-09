Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.2.0
--------------

Release Notes:

.. HTCondor version 10.2.0 released on Month Date, 2022.

- HTCondor version 10.2.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-1001`.

- We changed the semantics of relative paths in the ``output``, ``error``, and
  ``transfer_output_remaps`` submit file commands.  These commands now create
  the directories named in relative paths if they do not exist.  This could
  cause jobs that used to go on hold (because they couldn't write their
  ``output`` or ``error`` files, or a remapped output file) to instead succeed.
  :jira:`1325`

New Features:

- *condor_q* default behavior of displaying the cumulative run time has changed
  to now display the current run time for jobs in running, tranfering output,
  and suspended states while displaying the previous run time for jobs in idle or held
  state unless passed ``-cumulative-time`` to show the jobs cumulative run time for all runs.
  :jira:`1064`

- Docker Universe submit files now support *docker_pull_policy = always*, so
  that docker will check to see if the cached image is out of date.  This increases
  the network activity, may cause increased throttling when pulling from docker hub,
  and is recommended to be used with care.
  :jira:`1482`

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

- Added configuration knob :macro:`SINGULARITY_USE_PID_NAMESPACES`.
  :jira:`1431`

- *condor_history* will now stop searching history files once all requested job ads are
  found if passed ClusterIds or ClusterId.ProcId pairs.
  :jira:`1364`

- Improved *condor_history* search speeds when searching for matching jobs, matching clusters,
  and matching owners.
  :jira:`1382`

- The *CompletionDate* attribute of jobs is now undefined until such time as the job completes
  previously it was 0.
  :jira:`1393`

- The local issuer credmon can optionally add group authorizations to users' tokens by setting
  ``LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`` and ``LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE``.
  :jira:`1402`

- The ``JOB_INHERITS_STARTER_ENVIRONMENT`` configuration variable now accepts a list
  of match patterns just like the submit command ``getenv`` does.
  :jira:`1339`

- Docker universe and container universe job that use the docker runtime now detect
  when the unix uid or gid has the high bit set, which docker does not support.
  :jira:`1421`

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

Version 10.1.3
--------------

Release Notes:

.. HTCondor version 10.1.3 released on Month Date, 2022.

- HTCondor version 10.1.3 not yet released.

New Features:

- Jobs run in Singularity or Apptainer container runtmes now use the
  SINGULARITY_VERBOSITY flag, which controlls the verbosity of the runtime logging
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

.. HTCondor version 10.1.2 released on Month Date, 2022.

- HTCondor version 10.1.2 not yet released.

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
  with docker-repo style images
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

