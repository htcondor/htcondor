Version 23 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 23.7.1
--------------

Release Notes:

.. HTCondor version 23.7.1 released on Month Date, 2024.

- HTCondor version 23.7.1 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2308`.

- The use of multiple :subcom:`queue` statements in a single submit description
  file is now deprecated. This functionality is planned to be removed during the
  lifetime of the **V24** feature series.
  :jira:`2338`

New Features:

- In the unlikely event that a shadow exception event happens, the text is
  now saved in the job ad attribute :ad-attr:`LastShadowException` for
  further debugging.
  :jira:`1896`

- Most daemon log messages about tasks in the :macro:`STARTD_CRON_JOBLIST`,
  :macro:`BENCHMARKS_JOBLIST` or :macro:`SCHEDD_CRON_JOBLIST` that were
  logged as ``D_FULLDEBUG`` messages are now logged using the new message
  category ``D_CRON``.
  :jira:`2308`

- A local universe job can now specify a container image, and it will run
  with that singularity or apptainer container runtime.
  :jira:`2180`

- The file specified using the submit command :subcom:`starter_log` is now
  returned on both success and on failure when the submit command
  :subcom:`when_to_transfer_output` is set to ``ON_SUCCESS``.  In addition,
  a failure to transfer input is now treated as a failure for purposes of
  of ``ON_SUCCESS``.
  :jira:`2347`

Bugs Fixed:

- None.

Version 23.6.1
--------------

Release Notes:

.. HTCondor version 23.6.1 released on Month Date, 2024.

- HTCondor version 23.6.1 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2308`.

New Features:

- Updated :tool:`get_htcondor` to allow the aliases ``lts`` for **stable**
  and ``feature`` for **current** when passed to the *--channel* option.
  :jira:`775`

- Allow the startd to force a job that doesn't ask to run inside a
  docker or apptainer container inside one with new parameters
  :macro:`USE_DEFAULT_CONTAINER` and :macro:`DEFAULT_CONTAINER_IMAGE`
  :jira:`2317`

- :tool:`condor_q` -better now emits the units for memory and
  disk.
  :jira:`2333`

- Add htcondor job out|err|log verbs to the :tool:`htcondor` cli tool.
  :jira:`2182`

- The startd now honors the environment variable OMP_NUM_THREADS
  when setting the number of cores available.  This allows 
  glideins to pass an allocated number of cores from a base batch
  system to the glidein easily.
  :jira:`727`

- If the EP is started under another batch system that limits the amount
  of memory to the EP via a cgroup limit, the startd now advertises
  this much memory available for jobs.
  :jira:`727`

- Added new job ad attribute :ad-attr:`JobSubmitFile` which contains
  the filename of the submit file, if any.
  :jira:`2319`

- When the :subcom:`docker_network_type` is set to host, docker universe
  now sets the hostname inside the container to the same as the host,
  to ease networking from inside the container to outside the container.
  :jira:`2294`

- For vanilla universe jobs not running under container universe, that
  manually start apptainer or singularity, the environment variables
  APPTAINER_CACHEDIR and SINGULARITY_CACHEDIR are now set to the scratch
  directory to insure any files they create are cleaned up on job exit.
  :jira:`2337`

- :tool:`condor_submit` with the -i (interactive) flag, and also run
  with a submit file, now transfers the executable to the interactive job.
  :jira:`2315`

- Added the environment variable PYTHON_CPU_COUNT to the set of environment
  variables set for jobs to indicate how many cpu cores are provisioned.
  Python 3.13 uses this override the detected count of cpu cores.
  :jira:`2330`

- Added -file option to :tool:`condor_token_list`
  :jira:`575`

- The configuration parameter :macro:`ETC` can now be used to relocate
  files that are normally place under /etc/condor on unix platforms.
  :jira:`2290`

- The submit file expansion $(CondorScratchDir) now works for local
  universe.
  :jira:`2324`

- For jobs that go through the grid universe or Job Router, the
  terminate event will now include extended resource allocation and
  usage information when available.
  :jira:`2281`

- IDTOKEN files whose access permissions are not restricted to the file
  owner are now ignored.
  :jira:`232`

- The package containing the Pelican OSDF file transfer plugin is now
  a weak dependency for HTCondor.
  :jira:`2295`

- DAGMan no longer suppresses email notifications for jobs it manages by default.
  To revert behavior of suppressing notifications set :macro:`DAGMAN_SUPPRESS_NOTIFICATION`
  to **False**.
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
  either of these variables are set equal to the number of cpus, all slots will be connected.
  :jira:`2331`

Bugs Fixed:

- Fixed a bug in the :tool:`htcondor eventlog read` command that would fail
  when events were written on leap day.
  :jira:`2318`

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

- In some rare cases where docker universe could not start a container,
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

- When :tool:`condor_remote_cluster` installs binaries on an EL7 machine, it
  now uses the latest 23.0.x release. Before, it would fail, as
  current feature versions of HTCondor are not available on EL7.
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

