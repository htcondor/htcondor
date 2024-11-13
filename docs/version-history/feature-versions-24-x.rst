Version 24 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 24.3.1
--------------

Release Notes:

.. HTCondor version 24.3.1 released on Month Date, 2024.

- HTCondor version 24.3.1 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2403`.

New Features:

- Added new ``AUTO`` option to :macro:`LVM_HIDE_MOUNT` that creates a mount
  namespace for ephemeral logical volumes if the job is compatible with mount
  hiding (i.e not Docker jobs). The ``AUTO`` value is now the default value.
  :jira:`2717`

- EP's using :macro:`STARTD_ENFORCE_DISK_LIMITS` will now advertise
  :ad-attr:`IsEnforcingDiskUsage` in the machine ad.
  :jira:`2743`

Bugs Fixed:

- None.

Version 24.2.1
--------------

Release Notes:

.. HTCondor version 24.2.1 released on Month Date, 2024.

- HTCondor version 24.2.1 not yet released.

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

- Added singularity launcher wrapper script that runs inside the container
  and launches the job proper.  If this fails to run, HTCondor detects there
  is a problem with the container runtime, not the job, and reruns the
  job elsewhere.  Controlled by parameter :macro:`USE_SINGULARITY_LAUNCHER`
  :jira:`1446`

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

- :tool:`condor_ssh_to_job` when entering an apptainer container now sets the supplemental
  unix group ids in the same way that vanilla jobs have them set.
  :jira:`2695`

- IPv6 networking is now fully supported on Windows.
  :jira:`2601`

- Daemons will no longer block trying to invalidate their ads in a dead
  collector when shutting down.

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

- Added new job ad attribute :ad-attr:`InitialWaitDuration`, recording
  the number of seconds from when a job was queued to when the first launch
  happend.
  :jira:`2666`

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

