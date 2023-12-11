Version 23 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 23.4.0
--------------

Release Notes:

.. HTCondor version 23.4.0 released on Month Date, 2023.

- HTCondor version 23.4.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2304`.

New Features:

- None.

Bugs Fixed:

- None.

Version 23.3.0
--------------

Release Notes:

.. HTCondor version 23.3.0 released on Month Date, 2023.

- HTCondor version 23.3.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2303`.

New Features:

- Improve hold message when jobs on cgroup system exceed their memory limits.
  :jira:`1533`

- Improved the ``-convertoldroutes`` option of :tool:`condor_transform_ads`
  and added a new ``-help convert`` option. These changes are meant to assist
  in the conversion of CE's away from the deprecated transform syntax.
  :jira:`2146`

- Container universe now works when file transfer is disabled or not used.
  :jira:`1329`

- Added ability for DAGMan node script **STDOUT** and/or **STDERR** streams
  be captured in a user defined debug file. For more information visit
  DAGMan script :ref:`automated-workflows/dagman-scripts:Debug File`
  :jira:`2159`

- Startd now advertises when jobs are running with cgroup enforcement in
  the slot attribute ``CgroupEnforced``
  :jira:`1532`

- START_CRON_LOG_NON_ZERO_EXIT now also logs the stderr of the startd cron
  job to the StartLog.
  :jira:`1138`

Bugs Fixed:

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

