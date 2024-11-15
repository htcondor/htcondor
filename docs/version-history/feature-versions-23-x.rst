Version 23 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 23.10.18
----------------

Release Notes:

.. HTCondor version 23.10.18 released on Month Date, 2024.

- HTCondor version 23.10.18 planned release date is November 19, 2024.


- This version includes all the updates from :ref:`lts-version-history-23018`.

New Features:

- None.

Bugs Fixed:

- An unresponsive libvirtd daemon no longer causes the *condor_startd*
  to block indefinitely.
  :jira:`2644`

- When resolving a hostname to a list of IP addresses, avoid using
  IPv6 link-local addresses.
  This change was done incorrectly in 23.9.6.
  :jira:`2746`

Version 23.10.2
---------------

Release Notes:

- HTCondor version 23.10.2 released on October 30, 2024.

- This version includes all the updates from :ref:`lts-version-history-23017`.

New Features:

- None.

Bugs Fixed:

- If HTCondor output transfer (including the standard output and error logs)
  fails after an input transfer failure, HTCondor now reports the
  input transfer failure (instead of the output transfer failure).
  :jira:`2645`

Version 23.10.1
---------------

Release Notes:

- HTCondor version 23.10.1 released on October 3, 2024.

- This version includes all the updates from :ref:`lts-version-history-23015`.

- If a process in a job cannot be killed, perhaps because it is blocked in 
  a shared filesystem or GPU, we no longer count that job's cpu and peak
  memory usage in the
  next job that runs on that slot, when running on cgroup systems.
  :jira:`2639`
  :jira:`2647`

- The per job epoch history file is now enabled by default. See
  :macro:`JOB_EPOCH_HISTORY` for default value.

- HTCondor tarballs now contain `Pelican 7.10.7 <https://github.com/PelicanPlatform/pelican/releases/tag/v7.10.7>`_

- HTCondor no longer supports job execute directory encryption via ``eCryptFS``.
  This mainly effects execution points with an ``EL7`` OS.

New Features:

- Job execute directories can now be encrypted on Linux EP's utilizing
  :macro:`STARTD_ENFORCE_DISK_LIMITS`. Encryption of the job execute directory
  will occur when requested by the job via :subcom:`encrypt_execute_directory`
  or for all jobs when :macro:`ENCRYPT_EXECUTE_DIRECTORY` is ``True``.
  :jira:`2558`

- Improved efficiency of the *condor_starter* when collecting :ad-attr:`DiskUsage` and
  :ad-attr:`ScratchDirFileCount` when running on an EP using Logical Volume Management
  to enforce disk usage.
  :jira:`2456`

- When using :macro:`STARTD_ENFORCE_DISK_LIMITS`, the per-job scratch directory no longer
  contains a ``lost+found`` directory. Because this was owned by ``root``, it could
  cause problems with code that tried to read the whole scratch directory.
  :jira:`2564`

- Change :macro:`CGROUP_IGNORE_CACHE_MEMORY` default to ``true``.
  when ``true``, kernel cache pages do not count towards the :ad-attr:`MemoryUsage` in
  a job.
  :jira:`2521`
  :jira:`2565`

- In certain cases, when a connection to a :macro:`COLLECTOR_HOST` specified
  by (DNS) name is lost, HTCondor will now look the name up (in DNS) again
  before attempting to reconnect.  The intention is to allow collectors to
  change their IP address without requiring daemons connecting to it to be
  restarted or reconfigured.
  :jira:`2579`

- You can now configure HTCondor's network communications to use
  integrity checking and/or encryption with requiring authentication
  between client and server.
  :jira:`2567`

- Added three new nouns to the HTCondor CLI tool: :tool:`htcondor server`,
  :tool:`htcondor ap`, and :tool:`htcondor cm`. Each of theses nouns have a
  ``status`` verb to help show the health of various HTCondor installations.
  :jira:`2580`

- Added a new verb to :tool:`htcondor credential`, ``listall``, which allows the
  administrator to see the OAuth2 credentials known to HTCondor.
  :jira:`2505`

- When container universe jobs using Singularity or Apptainer runtimes
  need to create temporary scratch files to convert images format, they
  now use the job's scratch directory, not ``/tmp`` to do so.
  :jira:`2620`

- Docker universe jobs that RequestGpus should now keep their GPUs even after a
  systemd reconfig, which previously unmapped those gpus. See
  https://github.com/NVIDIA/nvidia-container-toolkit/issues/381
  for details.
  :jira:`2591`

- Container and Docker universe jobs now always transfer the executable listed
  in the submit file, even if it is an absolute path.  Earlier versions of
  HTCondor assumed absolute paths referred to programs within the container.
  The old way can be restored by setting the config knob
  :macro:`SUBMIT_CONTAINER_NEVER_XFER_ABSOLUTE_CMD` to ``true``, as it defaults to ``false``.
  :jira:`2595`

- :tool:`condor_userprio` now shows the submitter floor, if one has been
  defined.
  :jira:`2603`

- :tool:`condor_submit` will now output a better error when message provided a DAG input file.
  :jira:`2485`

- Added support for querying ``Slot`` and ``StartDaemon`` ad types to python bindings.
  :jira:`2474`

- Rather than report no memory usage, Docker universe jobs now over-report memory usage
  (by including memory used for caching) when running on modern kernels.
  :jira:`2573`

- DAGMan can now use the new :macro:`DAGMAN_INHERIT_ATTRS` knob to specify a list of
  job ClassAd attributes to pass from the root DAGMan job proper to all submitted jobs
  (including SubDAGs). Use :macro:`DAGMAN_INHERIT_ATTRS_PREFIX` to add a prefix to the
  ClassAd attributes passed down to managed jobs.
  :jira:`1845`

- :tool:`condor_watch_q` is now capable of tracking the shared DAGMan `*.nodes.log` file
  before any of the jobs associated with a DAGMan workflow are submitted.
  :jira:`2602`

- The shell prompt when running :tool:`condor_ssh_to_job` to a job inside an Apptainer
  or Singularity container now contains the slot name, instead of "Apptainer" or
  "Singularity".
  :jira:`2571`

- Implemented :meth:`htcondor2.Schedd.refreshGSIProxy`.
  :jira:`2577`

- A self-checkpointing job which specifies neither its checkpoint files nor
  its output files no longer includes files produced by or internal to
  HTCondor in its checkpoint.  This avoids a problem where such a checkpoint,
  when transferred to a job's sandbox after rescheduling, would fail to
  overwrite an existing HTCondor file, preventing the job from resuming.
  :jira:`2566`

- Transfer plugin ClassAds that are written to the epoch history file on
  an access point can now be fetched by :tool:`condor_adstash`.
  :jira:`2435`

Bugs Fixed:

- Fix issue where PID Namespaces and :tool:`condor_ssh_to_job` did not work
  on platforms using cgroups v2 such as Enterprise Linux 9.
  :jira:`2548`
  :jira:`2590`

- Fixed a bug where all job sandboxes would be world readable with ``755``
  file permissions on EP's using :macro:`STARTD_ENFORCE_DISK_LIMITS`
  regardless of :macro:`JOB_EXECDIR_PERMISSIONS`
  :jira:`2635`

- HTCondor no longer instructs file transfer plug-ins to transfer directories;
  this has never been part of the plug-in API and doing so accidentally could
  cause spurious file-transfer failures if the job specified
  :subcom:`output_destination`.
  :jira:`2594`

- Fixed a bug where HPC annexes ignored :macro:`TCP_FORWARDING_HOST`,
  preventing them from connecting to APs which had that set.
  :jira:`2575`

- An empty :class:`htcondor2.Submit` no longer crashes when converted to
  a string.
  :jira:`2577`

- Passing :meth:`htcondor2.Schedd.edit` an :class:`classad2.ExprTree`
  representing a ClassAd list now works.
  :jira:`2577`

- Jobs which set :subcom:`success_exit_code` once again get their
  :subcom:`output` and :subcom:`error` files back even on failure.
  :jira:`2539`

- Fixed a bug where job submission to personal HTCondor could fail
  when IDTOKENS authentication was used.
  :jira:`2584`

- HTCondor now sets :ad-attr:`HoldReasonSubCode` to the exit code
  (shifted left by eight bits) of a failed file-transfer plug-in
  in an additional case that only happens during output transfer.
  :jira:`2555`

Version 23.9.6
--------------

Release Notes:

- HTCondor version 23.9.6 released on August 8, 2024.

- This version includes all the updates from :ref:`lts-version-history-23014`.

- HTCondor tarballs now contain `Pelican 7.9.9 <https://github.com/PelicanPlatform/pelican/releases/tag/v7.9.9>`_

- DAGMan now enforces that the :dag-cmd:`PROVISIONER` node only submits
  one job.
  :jira:`2492`

New Features:

- Added new cgroup knob, :macro:`CGROUP_IGNORE_CACHE_MEMORY` that defaults to false.
  When true, kernel cache pages do not count towards the :ad-attr:`MemoryUsage` in 
  a job.
  :jira:`2521`

- The ClassAd language no longer supports unit suffixes on numeric literals.
  This was almost always a cause for confusion and bugs in ClassAd expressions.
  Note that unit suffixes are still allowed in the submit language in 
  :subcom:`request_disk` and :subcom:`request_memory`, but not in arbitrary 
  ClassAd expressions.
  :jira:`2455`

- Linux systems running cgroup v2 will now hide GPUs that have
  not been provisioned to the slots (usually because they did not
  :subcom:`request_gpus`).
  :jira:`2470`

- Added ability for DAGMan to produce job credentials when submitting jobs directly to
  the *condor_schedd*. This behavior can be disabled via :macro:`DAGMAN_PRODUCE_JOB_CREDENTIALS`.
  :jira:`1711`

- Container universe jobs running under Singularity or Apptainer now
  run with a contained home directory, when HTCondor file transfer is
  enabled.  This means the jobs get the $HOME environment variable set
  to the scratch directory, and an /etc/passwd entry inside the container
  with the home directory entry pointed to the same place.
  :jira:`2274`

- When resolving a hostname to a list of IP addresses, avoid using
  IPv6 link-local addresses.
  :jira:`2453`

- Added the ``credential`` verb to the ``htcondor`` tool, which may help
  in debugging certain kinds of problems.  See
  :ref:`the man page <man-pages/htcondor:Credential Verbs>` for details.
  :jira:`2483`

- Added new knob :macro:`CREATE_CGROUP_WITHOUT_ROOT` which allows a 
  non-rootly condor to create cgroups for jobs.  Only works on 
  cgroup v2 systems. Currently defaults to false, but might change 
  in the future.
  :jira:`2493`

- :tool:`condor_suspend` now currently reports number of suspended
  processes in the event log, on Linux systems running with root.
  :jira:`2490`

- Improved the tools that write a token to a file.
  Most noticeable is the addition of a -file option to write the token
  to an arbitrary file.
  Also, the -token option only takes a bare filename.
  The given file is overwritten instead of appended to.
  :jira:`2425`

- Reduced the default value for :macro:`MAX_SHADOW_EXCEPTIONS` from
  5 to 2.  Results from many pools revealed that once a shadow excepted
  running a job on a claim, retrying it usually also failed.
  :jira:`2300`

- The MODIFY_REQUEST_EXPR_REQUEST<RES> configuration variables
  (such as :macro:`MODIFY_REQUEST_EXPR_REQUESTMEMORY`)
  can now be prefixed with `SLOT_TYPE_<N>_` to be specialized by slot type.
  :jira:`2512`

- Added more special DAGMan script macros to reference information pertaining
  to the scripts associated DAG and node. See :ref:`DAG Script Macros` for more
  details.
  :jira:`2488`

- The identifier ``condor_pool`` is no longer used for the IDTOKENS
  and PASSWORD authentication methods; ``condor`` is used instead. 
  When authenticating with an older peer, ``condor_pool`` is still
  used, but is treated identically to ``condor`` for authorization
  rules (i.e. ALLOW_DAEMON).
  :jira:`2486`

- Added new special value ``{:local_ips:}`` that can be used in
  authorization ALLOW and DENY rules to represent all IP addresses
  that are usable on the local machine.
  :jira:`2466`

- Added Added support for querying ``Slot`` and ``StartDaemon`` ad types to python bindings.
  :jira:`2474`

- If a file transfer plugin is broken in such a way that it cannot be executed,
  HTCondor no longer puts a job that uses it on hold, but back to idle so it can try
  again.
  :jira:`2400`

Bugs Fixed:

- Fixed a bug on ``EL9`` where user-level checkpointing jobs would
  get killed on restart.
  :jira:`2491`

- Fixed a bug where if :macro:`DOCKER_IMAGE_CACHE_SIZE` was set very small,
  Docker images run by Docker universe jobs would never be removed from the Docker image cache.
  :jira:`2547`

- Fixed a bug where the ``-compact`` option of *condor_status* did not produce aggregated output for
  each machine.  This was particularly noticeable when the ``-gpus`` option was also used.
  :jira:`2556`

- Fixed a bug introduced in 23.7.2 that caused the *condor_schedd* and
  *condor_negotiator* to crash when the list subscript operator was used
  in a ClassAd expression.
  :jira:`2561`

Version 23.8.1
--------------

Release Notes:

- HTCondor version 23.8.1 released on June 27, 2024.

- This version includes all the updates from :ref:`lts-version-history-23012`.

- The HTCondor Docker images are now based on Alma Linux 9.
  :jira:`2504`

- HTCondor Docker images are now available for the ARM64 CPU architecture.
  :jira:`2188`

New Features:

- ``IDTOKEN`` files whose access permissions are too open are now ignored. (Group and other access must be none.)
  :jira:`232`

- Added new ``-SubmitMethod`` flag to :tool:`condor_submit_dag` which controls whether
  DAGMan directly submits jobs to the local *condor_schedd* queue or externally runs
  :tool:`condor_submit`.
  :jira:`2406`

- Added an ``-edit`` option to the :tool:`condor_qusers`.  This option allows
  and administrator to add custom attributes to a User ClassAd in the *condor_schedd*.
  :jira:`2381`

- The *condor_gangliad* memory consumption has been reduced, and it also places less load on
  the *condor_collector*.  Specifically, it now uses a projection when querying the collector
  if the configuration knob :macro:`GANGLIAD_WANT_PROJECTION` is set to True. Currently the default for
  this knob is False, but after additional testing, an upcoming release will default to True.
  :jira:`2394`

- Added an ``-long``, ``-format`` and ``-autoformat`` options to the :tool:`condor_ping`.
  These options give predictable output for programs that wish to parse the results
  of running the command.
  :jira:`2449`

- A job can now be put into a cool-down state after a failed execution
  attempt.
  If the expression given by new configuration parameter
  :macro:`SYSTEM_ON_VACATE_COOL_DOWN` evaluates to a positive integer,
  then the job will not be run again until after that number of
  seconds elapses.
  New job attributes :ad-attr:`VacateReason`,
  :ad-attr:`VacateReasonCode`, and :ad-attr:`VacateReasonSubCode` are
  set after a failed execution attempt and can be referenced in the
  cool-down expression.
  :jira:`2134`

- V2 cgroups created for jobs will now be in the cgroup tree the daemons
  are born in.  This tree is marked as Delegated in the systemd unit file,
  so that HTCondor is the sole manipulator of these trees, following the
  systemd "one writer" cgroup rule.
  :jira:`2445`

- New configuration parameter :macro:`CGROUP_LOW_MEMORY_LIMIT` allows an administrator
  of a Linux cgroup v2 system to set the "memory.low" setting in a job's cgroup
  to encourage cacheable memory pages to be reclaimed faster.
  :jira:`2391`

- Local universe jobs on Linux are now put into their own cgroups.  New knob
  :macro:`USE_CGROUPS_FOR_LOCAL_UNIVERSE` disables it.
  :jira:`2440`

- Sandbox file transfers will now timeout if no progress has been made either
  on a single read or write.  The default timeout is one hour (3600 seconds), controlled
  by :macro:`STARTER_FILE_XFER_STALL_TIMEOUT`.  Note this doesn't limit the *total* 
  time for sandbox transfers, as long as it is making some progress.  This can help jobs
  reading or writing to down NFS servers.  When the timeout is hit, the job is evicted,
  set back to idle and can start again.
  :jira:`1395`

- For **batch** grid universe jobs, the HOME environment variable is no
  longer set to the job's current working directory.
  :jira:`2413`

- When an IDToken or SciToken has restricted authorization levels,
  additional levels that are usually implied by those levels are now
  also included.
  For example, a token that provides ADVERTISE_SCHEDD authorization
  now also provides READ authorization.
  :jira:`2424`

- Added option to :tool:`condor_adstash` to populate the database with
  job epoch histories, not just the final history entry.
  :jira:`2076`

Bugs Fixed:

- Fixed a bug where :tool:`condor_submit` -i did not work on a 
  cgroup v2 system.
  :jira:`2438`

- Fixed a bug that prevented the *condor_startd* from advertising
  :ad-attr:`DockerCachedImageSizeMb`
  :jira:`2458`

- Fixed a bug where transfer of Kerberos credentials from the
  *condor_shadow* to the *condor_starter* would fail if the daemons
  weren't explicitly configured to trust each other.
  :jira:`2411`

- Fixed a rare bug where certain errors reported by a file transfer
  plugin were not reported to the *condor_starter*.
  :jira:`2464`

- Fixed a bug where backfill slots did not account for Memory used by
  active primary slots correctly.
  :jira:`2462`

Version 23.7.2
--------------

Release Notes:

- HTCondor version 23.7.2 released on May 16, 2024.

- This version includes all the updates from :ref:`lts-version-history-23010`.

- The use of multiple :subcom:`queue` statements in a single submit description
  file is now deprecated. This functionality is planned to be removed during the
  lifetime of the **V24** feature series.
  :jira:`2338`

- The semantics of :subcom:`skip_if_dataflow` have been changed to make
  more sense.  The restrictions have been :ref:`documented <dataflow>`.
  :jira:`1899`

- HTCondor tarballs now contain `Pelican 7.8.2 <https://github.com/PelicanPlatform/pelican/releases/tag/v7.8.2>`_
  :jira:`2399`

- When removing a large dag, the schedd now removes any existing child
  dag jobs in a non-blocking way, making the schedd more responsive during
  this removal.
  :jira:`2364`

- **NOTE**: Soon, ``IDTOKEN`` files with permissive file protections will be ignored.
  In particular, the ``/etc/condor/tokens.d`` directory and the tokens contained
  within should be only accessible by the ``root`` account.

New Features:

- Periodic policy expressions like :subcom:`periodic_remove` are now checked
  for during file input transfer.  Previously, HTCondor didn't start running these
  checks until the file transfer was finished at the job proper started.
  :jira:`2362`

- A local universe job can now specify a container image, and it will run
  with that Singularity or Apptainer container runtime.
  :jira:`2180`

- File transfer plugins that are installed on the EP can now advertise extra
  attributes into the STARTD ads.
  :jira:`1051`

- DAGMan can now write a rescue DAG and abort when :tool:`condor_dagman` has
  been pending on nodes for :macro:`DAGMAN_CHECK_QUEUE_INTERVAL` seconds and the
  associated jobs are not found in the local *condor_schedd* queue.
  :jira:`1546`

- In the unlikely event that a shadow exception event happens, the text is
  now saved in the job ad attribute :ad-attr:`LastShadowException` for
  further debugging.
  :jira:`1896`

- We now compute the path to the proper python3 interpreter for :tool:`condor_watch_q`
  at compile time.  This should not change anything, but if it does break, the
  guilty ticket is:
  :jira:`1146`

- If a collector defines a local-name, but not a :macro:`COLLECTOR_NAME`,
  the local name is now used as the default name.
  :jira:`1105`

- Most daemon log messages about tasks in the :macro:`STARTD_CRON_JOBLIST`,
  :macro:`BENCHMARKS_JOBLIST` or :macro:`SCHEDD_CRON_JOBLIST` that were
  logged as ``D_FULLDEBUG`` messages are now logged using the new message
  category ``D_CRON``.
  :jira:`2308`

- A new ``-jobset`` display option was added to :tool:`condor_q`.  If jobsets are enabled
  in the *condor_schedd* it will show information from the jobset ads.
  :jira:`2358`

- If a schedd has a schedd-specific SPOOL directory (set by
  schedd_name.SPOOL), the schedd now creates that directory
  with the proper ownership and permissions.
  :jira:`907`

- The file specified using the submit command :subcom:`starter_log` is now
  returned on both success and on failure when the submit command
  :subcom:`when_to_transfer_output` is set to ``ON_SUCCESS``.  In addition,
  a failure to transfer input is now treated as a failure for purposes of
  of ``ON_SUCCESS``.
  :jira:`2347`

- Removed some of the logging while loading the security configuration and moved
  some of the logging to ``D_SECURITY:2`` to make the ``-debug:D_SECURITY`` option
  of the various tools more useful.
  :jira:`2369`

Bugs Fixed:

- Fixed a bug where :tool:`condor_submit` -i did not work on a
  cgroup v2 system.
  :jira:`2438`

- Fixed bug on cgroup v2 systems where a race condition could cause a job to run
  in the wrong cgroup v2 for a very short amount of time.  If this job spawned a sub-job,
  the child job would forever live in the wrong cgroup.
  :jira:`2423`

- Fixed a bug where using :subcom:`output_destination` would still create
  directories on the access point.
  :jira:`2353`

Version 23.6.2
--------------

- HTCondor version 23.6.2 released on April 16, 2024.

New Features:

- None.

Bugs Fixed:

- Fixed bug where the :ad-attr:`HoldReasonSubCode` was not the documented value
  for jobs put on hold because of errors running a file transfer plugin.
  :jira:`2373`

Version 23.6.1
--------------

Release Notes:

- HTCondor version 23.6.1 released on April 15, 2024.

- **NOTE**: Soon, ``IDTOKEN`` files with permissive file protections will be ignored.
  In particular, the ``/etc/condor/tokens.d`` directory and the tokens contained
  within should be only accessible by the ``root`` account.

- This version includes all the updates from :ref:`lts-version-history-2308`.

New Features:

- Allow the *condor_startd* to force a job that doesn't ask to run inside a
  Docker or Apptainer container inside one with new parameters
  :macro:`USE_DEFAULT_CONTAINER` and :macro:`DEFAULT_CONTAINER_IMAGE`
  :jira:`2317`

- Added new submit command :subcom:`docker_override_entrypoint` to allow
  Docker universe jobs to override the entrypoint in the image.
  :jira:`2321`

- :tool:`condor_q` ``-better-analyze`` now emits the units for memory and
  disk.
  :jira:`2333`

- Updated :tool:`get_htcondor` to allow the aliases ``lts`` for **stable**
  and ``feature`` for **current** when passed to the *--channel* option.
  :jira:`775`

- Add htcondor job ``out``, ``err``, and ``log`` verbs to the :tool:`htcondor` CLI tool.
  :jira:`2182`

- The *condor_startd* now honors the environment variable ``OMP_NUM_THREADS``
  when setting the number of cores available.  This allows 
  glideins to pass an allocated number of cores from a base batch
  system to the glidein easily.
  :jira:`727`

- If the EP is started under another batch system that limits the amount
  of memory to the EP via a cgroup limit, the *condor_startd* now advertises
  this much memory available for jobs.
  :jira:`727`

- Added new job ad attribute :ad-attr:`JobSubmitFile` which contains
  the filename of the submit file, if any.
  :jira:`2319`

- When the :subcom:`docker_network_type` is set to ``host``, Docker universe
  now sets the hostname inside the container to the same as the host,
  to ease networking from inside the container to outside the container.
  :jira:`2294`

- For vanilla universe jobs not running under container universe, that
  manually start Apptainer or Singularity, the environment variables
  ``APPTAINER_CACHEDIR`` and ``SINGULARITY_CACHEDIR`` are now set to the scratch
  directory to insure any files they create are cleaned up on job exit.
  :jira:`2337`

- :tool:`condor_submit` with the -i (interactive) flag, and also run
  with a submit file, now transfers the executable to the interactive job.
  :jira:`2315`

- Added the environment variable ``PYTHON_CPU_COUNT`` to the set of environment
  variables set for jobs to indicate how many CPU cores are provisioned.
  Python 3.13 uses this override the detected count of CPU cores.
  :jira:`2330`

- Added -file option to :tool:`condor_token_list`
  :jira:`575`

- The configuration parameter :macro:`ETC` can now be used to relocate
  files that are normally place under ``/etc/condor`` on Unix platforms.
  :jira:`2290`

- The submit file expansion ``$(CondorScratchDir)`` now works for local
  universe.
  :jira:`2324`

- For jobs that go through the grid universe or Job Router, the
  terminate event will now include extended resource allocation and
  usage information when available.
  :jira:`2281`

- The package containing the Pelican OSDF file transfer plugin is now
  a weak dependency for HTCondor.
  :jira:`2295`

- Include a weak dependency on ``bash-completion`` so the ``htcondor`` CLI
  command has ``<TAB>`` completions.
  :jira:`2311`

- DAGMan no longer suppresses email notifications for jobs it manages by default.
  To revert behavior of suppressing notifications set :macro:`DAGMAN_SUPPRESS_NOTIFICATION`
  to **True**.
  :jira:`2323`

- Added configuration knobs :macro:`GANGLIAD_WANT_RESET_METRICS`  and 
  :macro:`GANGLIAD_RESET_METRICS_FILE`, enabling *condor_gangliad* to
  be configured to reset aggregate metrics to a value of zero when they are
  no longer being updated.  Previously aggregate metrics published to
  Ganglia retained the last value published indefinitely.
  :jira:`2346`

- The Job Router route keyword ``GridResource`` is now always
  optional. The job attribute ``GridResource`` can be set instead via
  a ``SET`` or similar command in the route definition.
  :jira:`2329`

- The configuration variables :macro:`SLOTS_CONNECTED_TO_KEYBOARD` and
  :macro:`SLOTS_CONNECTED_TO_CONSOLE` now apply to partitionable slots but do
  not count them as slots.  As a consequence of this change, when
  either of these variables are set equal to the number of CPUs, all slots will be connected.
  :jira:`2331`

Bugs Fixed:

- Fixed a bug in the :tool:`htcondor eventlog read` command that would fail
  when events were written on leap day.
  :jira:`2318`

Version 23.5.3
--------------

- HTCondor version 23.5.3 released on March 25, 2024.

- HTCondor tarballs now contain `Pelican 7.6.2 <https://github.com/PelicanPlatform/pelican/releases/tag/v7.6.2>`_

New Features:

- None.

Bugs Fixed:

- None.

Version 23.5.2
--------------

Release Notes:

- HTCondor version 23.5.2 released on March 14, 2024.

- This version includes all the updates from :ref:`lts-version-history-2306`.

- The library libcondorapi has been removed from the distribution.  We know of
  no known user for this C++ event log reading code, and all of our known users
  use the Python bindings for this, as we recommend.
  :jira:`2278`

New Features:

- The old ClassAd-based syntax for defining Job Router routes is now
  disabled by default.
  It can be enabled by setting configuration parameter
  :macro:`JOB_ROUTER_USE_DEPRECATED_ROUTER_ENTRIES` to ``True``.
  Support for the old syntax will be removed entirely before HTCondor
  version 24.0.0.
  :jira:`2260`

- Added ability for administrators to specify whether Startd disk enforcement creates
  thin or thick provisioned logical volumes for a jobs ephemeral execute directory.
  This is controlled by the new configuration knob :macro:`LVM_USE_THIN_PROVISIONING`.
  :jira:`1783`

- GPU detection is now enabled by default on all execute nodes via a new configuration variable
  :macro:`STARTD_DETECT_GPUS`.  This new configuration variable supplies arguments to
  *condor_gpu_discovery* for use when GPU discovery is not otherwise explicitly enabled in the configuration.
  :jira:`2264`

- On Linux systems with cgroup v1 enabled, HTCondor now uses the "devices" cgroup
  to prevent the job from accessing unassigned GPUs.  This can be disabled
  by setting the new knob :macro:`STARTER_HIDE_GPU_DEVICES` to false.
  :jira:`1152`

- Added new submit commands for constraining GPU properties. When these commands
  are use the ``RequireGPUs`` expression is generated automatically by submit and
  desired values are stored as job attributes. The new submit commands are :subcom:`gpus_minimum_memory`,
  :subcom:`gpus_minimum_runtime`, :subcom:`gpus_minimum_capability` and :subcom:`gpus_maximum_capability`.
  :jira:`2201`

- The new submit commands :subcom:`starter_debug` and :subcom:`starter_log`
  can be used to have the *condor_starter* write a second copy of its
  daemon log and have that file transferred to the Access Point with the
  job's output sandbox.
  :jira:`2296`

- During SSL authentication, VOMS attributes can be included when
  mapping to an HTCondor identity.
  To do so, configuration parameters :macro:`USE_VOMS_ATTRIBUTES` and
  :macro:`AUTH_SSL_USE_VOMS_IDENTITY` must be set to ``True``.
  :jira:`2256`

- The ``$CondorVersion`` string contains the Git SHA for official CHTC builds of HTCondor.
  :jira:`532`

- Added job attributes :ad-attr:`JobCurrentReconnectAttempt` and
  :ad-attr:`TotalJobReconnectAttempts` to count the number of
  reconnect attempts in progress, and total for the lifetime of
  the job, respectively.
  :jira:`2258`

- Improve the reliability of the user log reader code by changing it to do line oriented reads and to seek less.
  :jira:`2254`

Bugs Fixed:

- In some rare cases where Docker universe could not start a container,
  it would not remove that container until the next time the start
  restarted.  Now it is removed as soon as possible.
  :jira:`2263`

- In rare cases, the values of TimeSlotBusy and TimeExecute would be incorrect in the
  job event log when the job was disconnected or did not start properly.
  :jira:`2265`

- Fixed a bug that can cause the condor_gridmanager to abort when multiple
  grid universe jobs share the same proxy file to be used to authenticate
  with the remote job scheduling service.
  :jira:`2334`

Version 23.4.0
--------------

Release Notes:

- HTCondor version 23.4.0 released on February 8, 2024.

- This version includes all the updates from :ref:`lts-version-history-2304`.

New Features:

- Added configuration parameter :macro:`SUBMIT_REQUEST_MISSING_UNITS`, to warn or prevent submitting
  with RequestDisk or RequestMemory without a units suffix.
  :jira:`1837`

- On RPM-based distributions, a new package ``condor-credmon-local`` is now
  available which provides the
  :ref:`local SciTokens issuer credmon <installing_credmon_local>` without
  installing extra packages required by the OAuth credmon.
  The ``condor-credmon-local`` package is now a dependency of the
  ``condor-credmon-oauth`` package.
  :jira:`2197`

- The :tool:`htcondor` command line tools eventlog read command now
  optionally takes more than one eventlog to process at once.
  :jira:`2220`

- Docker universe now passes --log-driver none by default when running jobs,
  but can be disabled with :macro:`DOCKER_LOG_DRIVER_NONE` knob.
  :jira:`2190`

- Jobs that are assigned nVidia GPUs now have the environment variable
  NVIDIA_VISIBLE_DEVICES set in addition to, and with the same value as
  CUDA_VISIBLE_DEVICES, as newer nVidia run-times prefer the former.
  :jira:`2189`

- Added job classad attribute :ad-attr:`ContainerImageSource`, a string which is
  is set to the source of the image transfer.
  :jira:`1797`

- If :macro:`PER_JOB_HISTORY_DIR` is set, it is now a fatal error to write a historical job
  to the history file, just like the normal history file.
  :jira:`2027`

- :tool:`condor_submit` now generates requirements expressions for
  **condor** grid universe jobs like it does for vanilla universe
  jobs.
  This can be disabled by setting the new configuration parameter
  :macro:`SUBMIT_GENERATE_CONDOR_C_REQUIREMENTS` to ``False``.
  :jira:`2204`

Bugs Fixed:

- Fixed a bug introduced in 23.3.0 wherein 
  :macro:`NEGOTIATOR_SLOT_CONSTRAINT` was completely ignored.
  :jira:`2245`

Version 23.3.1
--------------

- HTCondor version 23.3.1 released on January 23, 2024.

- HTCondor tarballs now contain `Pelican 7.4.0 <https://github.com/PelicanPlatform/pelican/releases/tag/v7.4.0>`_

New Features:

- None.

Bugs Fixed:

- None.

Version 23.3.0
--------------

Release Notes:

- HTCondor version 23.3.0 released on January 4, 2024.

- Limited support for Enterprise Linux 7 in the 23.x feature versions.
  Since we are developing new features, the Enterprise Linux 7 build may
  drop features or be dropped entirely. In particular, Python 2 and
  OATH credmon support will be removed during the 23.x development cycle.
  :jira:`2194`

- This version includes all the updates from :ref:`lts-version-history-2303`.

New Features:

- Improved the ``-convertoldroutes`` option of :tool:`condor_transform_ads`
  and added a new ``-help convert`` option. These changes are meant to assist
  in the conversion of CE's away from the deprecated transform syntax.
  :jira:`2146`

- Added ability for DAGMan node script **STDOUT** and/or **STDERR** streams
  be captured in a user defined debug file. For more information visit
  DAGMan script :ref:`Script Debugging`
  :jira:`2159`

- Improve hold message when jobs on cgroup system exceed their memory limits.
  :jira:`1533`

- Startd now advertises when jobs are running with cgroup enforcement in
  the slot attribute :ad-attr:`CgroupEnforced`
  :jira:`1532`

- START_CRON_LOG_NON_ZERO_EXIT now also logs the stderr of the startd cron
  job to the StartLog.
  :jira:`1138`

Bugs Fixed:

- Container universe now works when file transfer is disabled or not used.
  :jira:`1329`

- Removed confusing message in StartLog at shutdown about trying to
  kill illegal pid.
  :jira:`1012`

Version 23.2.0
--------------

Release Notes:

- HTCondor version 23.2.0 released on November 29, 2023.

- This version includes all the updates from :ref:`lts-version-history-2302`.

New Features:

- Added *periodic_vacate* to the submit language and SYSTEM_PERIODIC_VACATE
  to the configuration system.
  Historically, users used periodic_hold/release to evict “stuck” jobs,
  that is jobs that should finish in some amount of time,
  but sometimes run for an arbitrarily long time. Now with this new feature,
  for improved usability, users may use this single ``periodic_vacate`` submit
  command instead.
  :jira:`2114`

- Linux EPs now advertise the startd attribute HasRotationalScratch to be
  ``true`` when HTCondor detects that the execute directory is on a rotational
  hard disk and false when the kernel reports it to be on SSD, NVME, or tmpfs.
  :jira:`2085`

- Added ``TimeSlotBusy`` and ``TimeExecute`` to the event log terminate events
  to indicate how much wall time a job used total (including file transfer)
  and just for the job execution proper, respectively.
  :jira:`2101`

- Most files that HTCondor generates are now written in binary mode on
  Windows. As a result, each line in these files will end in just a
  line feed character, without a preceding carriage return character.
  Files written by jobs are unaffected by this change.
  :jira:`2098`

- HTCondor now uses the `Pelican Platform <https://pelicanplatform.org/>`_
  to do file transfers with the
  `Open Science Data Federation (OSDF) <https://osg-htc.org/services/osdf.html>`_.
  :jira:`2100`

- HTCondor now does a better job of cleaning up inner cgroups left behind
  by glidein pilots.
  :jira:`2081`

- Added new configuration option :macro:`<Keyword>_HOOK_PREPARE_JOB_ARGS`
  to allow the passing of arguments to specified prepare job hooks.
  :jira:`1851`

- The default trusted CAs for OpenSSL are now always used by default 
  in addition to any specified by :macro:`AUTH_SSL_SERVER_CAFILE`, 
  :macro:`AUTH_SSL_CLIENT_CAFILE`, :macro:`AUTH_SSL_SERVER_CADIR`, and 
  :macro:`AUTH_SSL_CLIENT_CADIR`. 
  The new configuration parameters :macro:`AUTH_SSL_SERVER_USE_DEFAULT_CAS`
  and :macro:`AUTH_SSL_CLIENT_USE_DEFAULT_CAS` can be used to disable 
  use of the default CAs for OpenSSL. 
  :jira:`2090`

- Using :tool:`condor_store_cred` to set a pool password on Windows now
  requires ``ADMINISTRATOR`` authorization with the :tool:`condor_master` (instead
  of ``CONFIG`` authorization).
  :jira:`2106`

- When :tool:`condor_remote_cluster` installs binaries on an ``EL7`` machine, it
  now uses the latest 23.0.x release. Before, it would fail, as
  current feature versions of HTCondor are not available on ``EL7``.
  :jira:`2125`

- HTCondor daemons on Linux no longer run very slowly when the ulimit
  for the maximum number of open files is very high.
  :jira:`2128`

- Somewhat improved the performance of the ``_DEBUG`` flag ``D_FDS``.  But please
  don't use this unless absolutely needed.
  :jira:`2050`

Bugs Fixed:

- None.

Version 23.1.0
--------------

Release Notes:

- HTCondor version 23.1.0 released on October 31, 2023.

- This version includes all the updates from :ref:`lts-version-history-2301`.

- Enterprise Linux 7 support is discontinued with this release.

- We have added HTCondor Python wheels for the aarch64 CPU architecture on PyPI.
  :jira:`2120`

New Features:

- Improved :tool:`condor_watch_q` to filter tracked jobs based on cluster IDs
  either provided by the ``-clusters`` option or found in association
  to batch names provided by the ``-batches`` option. This helps limit
  the amount of output lines when using an aggregate/shared log file.
  :jira:`2046`

- Added new ``-larger-than`` flag to :tool:`condor_watch_q` that filters tracked
  jobs to only include jobs with cluster IDs greater than or equal to the
  provided cluster ID.
  :jira:`2046`

- The Access Point can now be told to use a non-standard ssh port when sending
  jobs to a remote scheduling system (such as Slurm).
  You can now specify an alternate ssh port with :tool:`condor_remote_cluster`.
  :jira:`2002`

- Laid groundwork to allow an Execution Point running without root access to
  accurately limit the job's usage of CPU and Memory in real time via Linux
  kernel cgroups. This is particularly interesting for glidein pools.
  Jobs running in cgroup v2 systems can now subdivide the cgroup they
  have been given, so that pilots can enforce sub-limits of the resources
  they are given.
  :jira:`2058`

- HTCondor file transfers using HTTPS can now utilize CA certificates
  in a non-standard location.
  The curl_plugin tool now recognizes the environment variable
  ``X509_CERT_DIR`` and configures libcurl to search the given directory for
  CA certificates.
  :jira:`2065`

- Improved performance of *condor_schedd*, and other daemons, by caching the
  value in ``/etc/localtime``, so that debugging logs aren't always stat'ing that
  file.
  :jira:`2064`

Bugs Fixed:

- None.

