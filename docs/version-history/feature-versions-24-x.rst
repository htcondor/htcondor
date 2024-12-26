Version 24 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 24.3.0
--------------

Release Notes:

.. HTCondor version 24.3.0 released on Month Date, 2025.

- HTCondor version 24.3.0 planned release date is January 9, 2025.

- Methods in :class:`htcondor2.Schedd` which take ``job_spec`` arguments now
  accept a cluster ID in the form of an :class:`int`.  These functions
  (:meth:`htcondor2.Schedd.act`, :meth:`htcondor2.Schedd.edit`,
  :meth:`htcondor2.Schedd.export_jobs`, :meth:`htcondor2.Schedd.retrieve`,
  and :meth:`htcondor2.Schedd.unexport_jobs`) now also raise :class:`TypeError`
  if their ``job_spec`` argument is not a :class:`str`, :class:`list` of
  :class:`str`, :class:`classad2.ExprTree`, or :class:`int`.
  :jira:`2745`

New Features:

- Added singularity launcher wrapper script that runs inside the container
  and launches the job proper.  If this fails to run, HTCondor detects there
  is a problem with the container runtime, not the job, and reruns the
  job elsewhere.  Controlled by parameter :macro:`USE_SINGULARITY_LAUNCHER`
  :jira:`1446`

- If the startd detects that an exited or evicted job has leftover, unkillable
  processes, it now marks that slot as "broken", and will not reassign the resources
  for that slot to any other jobs.  Disabled if :macro:`STARTD_LEFTOVER_PROCS_BREAK_SLOTS`
  is set to false.
  :jira:`2756`

- Added new submit command for container universe, :subcom:`mount_under_scratch`
  that allows user to create writeable ephemeral directories in their otherwise
  read only container images.
  :jira:`2728`

- Added new ``AUTO`` option to :macro:`LVM_HIDE_MOUNT` that creates a mount
  namespace for ephemeral logical volumes if the job is compatible with mount
  hiding (i.e not Docker jobs). The ``AUTO`` value is now the default value.
  :jira:`2717`

- EP's using :macro:`STARTD_ENFORCE_DISK_LIMITS` will now advertise
  :ad-attr:`IsEnforcingDiskUsage` in the machine ad.
  :jira:`2734`

- When the *condor_startd* interrupts a job's execution, the specific
  reason is now reflected in the job attributes
  :ad-attr:`VacateReason` and :ad-attr:`VacateReasonCode`.
  :jira:`2713`

- Environment variables from the job that start with ``PELICAN_`` will now be
  set in the environment of the pelican file transfer plugin when it is invoked
  to do file transfer. This is intended to allow jobs to turn on enhanced logging
  in the plugin.
  :jira:`2674`

- The ``-subsystem`` argument of *condor_status* is once again case-insensitive for credd
  and defrag subsystem types.
  :jira:`2796`

- Add new knob :macro:`CGROUP_POLLING_INTERVAL` which defaults to 5 (seconds), to
  control how often a cgroup system polls for resource usage.
  :jira:`2802`

- Updated the *condor_credmon_oauth* and created a new ``condor-credmon-multi`` RPM package which,
  when installed, allows user credentials added via Vault and user credentials generated
  via a local issuer to exist simultaneously without conflict (e.g. the Vault credmon
  will not attempt to refresh locally issued credentials).
  :jira:`2408`

Bugs Fixed:

- Fixed a bug introduced in 24.2.0 where the daemons failed to start
  if configured to use only a network interface that didn't an IPv6
  address.
  Also, the daemons will no longer bind and advertise an address that
  doesn't match the value of :macro:`NETWORK_INTERFACE`.
  :jira:`2799`

- The :tool:`htcondor job submit` command now issues credentials
  like :tool:`condor_submit`.
  :jira:`2745`

- EPs spawned by `htcondor annex` no longer crash on start-up.
  :jira:`2745`

- When resolving a hostname to a list of IP addresses, avoid using
  IPv6 link-local addresses.
  This change was done incorrectly in 23.9.6.
  :jira:`2746`

- :meth:`htcondor2.Submit.from_dag` and :meth:`htcondor.Submit.from_dag` now
  correctly raises an HTCondor exception when the processing of DAGMan
  options and submit time DAG commands fails.
  :jira:`2736`

- Fixed confusing job hold message that would state a job requested
  ``0.0 GB`` of disk via :subcom:`request_disk` when exceeding disk
  usage on Execution Points using :macro:`STARTD_ENFORCE_DISK_LIMITS`.
  :jira:`2753`

- You can now locate a collector daemon in the htcondor2 Python bindings.
  :jira:`2738`

- Fixed a bug in *condor_qusers* tool where the ``add`` argument would always
  enable rather than add a user.
  :jira:`2775`

- Fixed an inconsistency in cgroup v1 systems where the memory reported
  by condor included memory used by the kernel to cache disk pages.
  :jira:`2807`

- Fixed a bug on cgroup v1 systems where jobs that were killed by the
  Out of Memory killer did not go on hold.
  :jira:`2806`

- Fixed a bug where cgroup systems did not report peak memory, as intended
  but current instantaneous memory instead.
  :jira:`2800` :jira:`2804`

- Fixed incompatibility of :tool:`condor_adstash` with v2.x of the OpenSearch Python Client.
  :jira:`2614`

Version 24.2.2
--------------

Release Notes:

- HTCondor version 24.2.2 released on December 4, 2024.

New Features:

- None.

Bugs Fixed:

- If knob :macro:`EXECUTE` is explicitly set to a blank string in the config file for 
  whatever reason, the execution point (startd) may attempt to remove all files from
  the root partition (everything in /) upon startup.
  :jira:`2760`

Version 24.2.1
--------------

Release Notes:

- HTCondor version 24.2.1 released on November 26, 2024.

- This version includes all the updates from :ref:`lts-version-history-2402`.

- The DAGMan metrics file has changed the name of metrics referring to ``jobs``
  to accurately refer to modern terminology as ``nodes``. To revert back to old
  terminology set :macro:`DAGMAN_METRICS_FILE_VERSION` = ``1``.
  :jira:`2682`

New Features:

- DAGMan will now correctly submit late materialization jobs to an Access
  Point when :macro:`DAGMAN_USE_DIRECT_SUBMIT` = ``True``.
  :jira:`2673`

- Added new submit command :subcom:`primary_unix_group`, which takes a string
  which must be one of the user's supplemental groups, and sets the primary 
  group to that value.
  :jira:`2702`

- Improved DAGMan metrics file to use updated terminology and contain more
  metrics.
  :jira:`2682`

- A *condor_startd* which has :macro:`ENABLE_STARTD_DAEMON_AD` enabled will no longer
  abort when it cannot create the required number of slots of the correct size on startup.
  It will now continue to run; reporting the failure to the collector in the daemon ad.  Slots
  that can be fully provisioned will work normally. Slots that cannot be fully provisioned
  will exist but advertise themselves as broken. This is now the default behavior because
  daemon ads are enabled by default. The *condor_status* tool has a new option ``-broken``
  which displays broken slots and their reason for being broken. Use this option with
  the ``-startd`` option to display machines that are fully or partly broken.
  :jira:`2500`

- A new job attribute :ad-attr:`FirstJobMatchDate` will be set for all jobs of a single submission
  to the current time when the first job of that submission is matched to a slot.
  :jira:`2676`

- Added new job ad attribute :ad-attr:`InitialWaitDuration`, recording
  the number of seconds from when a job was queued to when the first launch
  happened.
  :jira:`2666`

- :tool:`condor_ssh_to_job` when entering an Apptainer container now sets the supplemental
  unix group ids in the same way that vanilla jobs have them set.
  :jira:`2695`

- IPv6 networking is now fully supported on Windows.
  :jira:`2601`

- Daemons will no longer block trying to invalidate their ads in a dead
  collector when shutting down.
  :jira:`2709`

- Added option ``FAST`` to configuration parameter
  :macro:`MASTER_NEW_BINARY_RESTART`. This will cause the *condor_master*
  to do a fast restart of all the daemons when it detects new binaries.
  :jira:`2708`

Bugs Fixed:

- None.

Version 24.1.1
--------------

Release Notes:

- HTCondor version 24.1.1 released on October 31, 2024.

- This version includes all the updates from :ref:`lts-version-history-2401`.

New Features:

- Added ``get`` to the ``htcondor credential`` noun, which prints the contents
  of a stored OAuth2 credential.
  :jira:`2626`

- Added :meth:`htcondor2.set_ready_state` for those brave few writing daemons
  in the Python bindings.
  :jira:`2615`

- When blah_debug_save_submit_info is set in blah.config, the ``stdout ``
  and ``stderr`` of the blahp's wrapper script is saved under the given 
  directory. 
  :jira:`2636`

- The DAG command :dag-cmd:`SUBMIT-DESCRIPTION` and node inline submit
  descriptions now work when :macro:`DAGMAN_USE_DIRECT_SUBMIT` = ``False``.
  :jira:`2607`

- Docker universe jobs now check the Architecture field in the image,
  and if it doesn't match the architecture of the EP, the job is put
  on hold.  The new parameter :macro:`DOCKER_SKIP_IMAGE_ARCH_CHECK` skips this.
  :jira:`2661`

- Added a configuration template, :macro:`use feature:DefaultCheckpointDestination`.
  :jira:`2403`

Bugs Fixed:

- If HTCondor detects that an invalid checkpoint has been downloaded for a
  self-checkpoint jobs using third-party storage, that checkpoint is now
  marked for deletion and the job rescheduled.
  :jira:`1258`

