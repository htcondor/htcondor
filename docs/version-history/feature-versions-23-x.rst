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

Bugs Fixed:

- None.

Version 23.1.0
--------------

Release Notes:

.. HTCondor version 23.1.0 released on Month Date, 2023.

- HTCondor version 23.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2301`.

- Enterprise Linux 7 support is discontinued with this release.

New Features:

- You can now specify an alternate ssh port with
  *condor_remote_cluster*.
  :jira:`2002`

- Improved *condor_watch_q* to filter tracked jobs based on cluster IDs
  either provided by the ``-clusters`` option or found in association
  to batch names provided by the ``-batches`` option. This helps limit
  the amount of output lines when using an aggregate/shared log file.
  :jira:`2046`

- Improved performance of *condor_schedd*, and other daemons, by cacheing the
  value in /etc/localtime, so that debugging logs aren't always stat'ing that
  file.
  :jira:`2064`

- Added new ``-larger-than`` flag to *condor_watch_q* that filters tracked
  jobs to only include jobs with cluster IDs greater than or equal to the
  provided cluster ID.
  :jira:`2046`

- Job running in cgroup v2 systems can now subdivide the cgroup they
  have been given, so that pilots can enforce sub-limits of the resources
  they are given.
  :jira:`2058`

- The curl_plugin tool now recognizes the environment variable
  X509_CERT_DIR and configures libcurl to search  the given directory for
  CA certificates.
  :jira:`2065`

Bugs Fixed:

- None.

