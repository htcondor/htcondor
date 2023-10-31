Version 23 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 23.1.0
--------------

Release Notes:

- HTCondor version 23.1.0 released on October 31, 2023.

- This version includes all the updates from :ref:`lts-version-history-2300`.

- Enterprise Linux 7 support is discontinued with this release.

- We have added HTCondor Python wheels for the aarch64 CPU architecture on Pypi.
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
  value in /etc/localtime, so that debugging logs aren't always stat'ing that
  file.
  :jira:`2064`

Bugs Fixed:

- None.

