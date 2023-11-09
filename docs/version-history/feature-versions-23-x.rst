Version 23 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 23.2.0
--------------

Release Notes:

.. HTCondor version 23.2.0 released on Month Date, 2023.

- HTCondor version 23.2.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2302`.

New Features:

- Linux EPs now advertise the startd attribute HasRotationalScratch to be
  true when HTCondor detects that the execute directory is on a rotational
  hard disk and false when the kernel reports it to be on SSD, NVME or tmpfs.
  :jira:`2085`

- HTCondor daemons on Linux no longer run very slowly when the ulimit
  for the maximum number of open files is very high.
  :jira:`2128`

- Added *periodic_vacate* to the submit language and SYSTEM_PERIODIC_VACATE
  to the config syttem
  :jira:`2114`

- Added TimeSlotBusy and TimeExecute to the event log terminate events
  to indicate how much wall time a job used total (including file transfer)
  and just for the job execution proper, respectively..
  :jira:`2101`

- The default trusted CAs for OpenSSL are now always used by default 
  in addition to any specified by :macro:`AUTH_SSL_SERVER_CAFILE`, 
  :macro:`AUTH_SSL_CLIENT_CAFILE`, :macro:`AUTH_SSL_SERVER_CADIR`, and 
  :macro:`AUTH_SSL_CLIENT_CADIR`. 
  The new configuration parameters :macro:`AUTH_SSL_SERVER_USE_DEFAULT_CAS`
  and :macro:`AUTH_SSL_CLIENT_USE_DEFAULT_CAS` can be used to disable 
  use of the default CAs for OpenSSL. 
  :jira:`2090`

- Most files that HTCondor generates are now written in binary mode on
  Windows. As a result, each line in these files will end in just a
  line feed character, without a preceding carriage return character.
  Files written by jobs are unaffected by this change.
  :jira:`2098`

- Using *condor_store_cred* to set a pool password on Windows now
  requires ADMINISTRATOR authorization with the *condor_master* (instead
  of CONFIG authorization).
  :jira:`2106`

- Somewhat improved the performance of the _DEBUG flag D_FDS.  But please
  don't use this unless absolutely needed.
  :jira:`2050`

- When *condor_remote_cluster* installs binaries on an EL7 machine, it
  now uses the latest 23.0.X release. Before, it would fail, as
  current versions of HTCondor are not available on EL7.
  :jira:`2125`

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

- Improved *condor_watch_q* to filter tracked jobs based on cluster IDs
  either provided by the ``-clusters`` option or found in association
  to batch names provided by the ``-batches`` option. This helps limit
  the amount of output lines when using an aggregate/shared log file.
  :jira:`2046`

- Added new ``-larger-than`` flag to *condor_watch_q* that filters tracked
  jobs to only include jobs with cluster IDs greater than or equal to the
  provided cluster ID.
  :jira:`2046`

- The Access Point can now be told to use a non-standard ssh port when sending
  jobs to a remote scheduling system (such as Slurm).
  You can now specify an alternate ssh port with *condor_remote_cluster*.
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

