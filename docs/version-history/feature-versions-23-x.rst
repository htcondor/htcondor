Version 23 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 23.1.0
--------------

Release Notes:

.. HTCondor version 23.1.0 released on Month Date, 2023.

- HTCondor version 23.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2300`.

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

