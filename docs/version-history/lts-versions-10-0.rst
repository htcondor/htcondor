Version 10.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
10.0.y versions. New features will be added in the 10.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-1000:

Version 10.0.0
--------------

Release Notes:

.. HTCondor version 10.0.0 released on Month Date, 2022.

- HTCondor version 10.0.0 not yet released.

New Features:

- None.

Bugs Fixed:

- Fixed a bug where if a job created a symlink to a file, the contents of
  that file would be counted in the job's `DiskUsage`.  Previously,
  symlinks to directories were (correctly) ignored, but not symlinks to
  files.
  :jira:`1354`

- Suppressed a Singularity or Apptainer warning that would appear
  in a job's stderr file, warning about the inability to set the
  HOME environment variable if the job or the system explicitly tried
  to set it.
  :jira:`1386`

- Fixed a bug where on certain Linux kernels, the ProcLog would be filled
  with thousands of errors of the form  "Internal cgroup error when 
  retrieving iowait statistics".  This error was harmless, but filled
  the ProcLog with noise.
  :jira:`1385`

- Fixed bug where certain **submit file** variables like ``accounting_group`` and
  ``accounting_group_user`` couldn't be declared specifically for DAGMan jobs because
  DAGMan would always write over the variables at job submission time.
  :jira:`1277`

- Fixed a bug where SciTokens authentication wasn't available on macOS
  and Python wheels distributions.
  :jira:`1328`

- Fixed job submission to newer ARC CE releases.
  :jira:`1327`

- Fixed a bug where a pre-created security session may not be used
  when connecting to a daemon over IPv6.
  The peers would do a full round of authentication and authorization,
  which may fail.
  This primarily happened with both peers had ``PREFER_IPV4`` set to
  ``False``.
  :jira:`1341`

- The *condor_negotiator* no longer sends the admin capability
  attribute of  machine ads to the *condor_schedd*.
  :jira:`1349`

- Fixed a bug in DAGMan where **Node** jobs that could not write to their **UserLog**
  would cause the **DAG** to get stuck indefinitely while waiting for pending **Nodes**.
  :jira:`1305`

- Fixed a bug where ``s3://`` URLs host or bucket names shorter than 14
  characters caused the shadow to dump core.
  :jira:`1378`

- Fixed a bug in the hibernation code that caused HTCondor to ignore
  the active Suspend-To-Disk option.
  :jira:`1357`

- Fixed a bug where some administrator client tools did not properly
  use the remote administrator capability (configuration parameter
  ``SEC_ENABLE_REMOTE_ADMINISTRATION``).
  :jira:`1371`

- When a ``JOB_TRANSFORM_*`` transform changes an attribute at submit time in a late
  materialization factory, it no longer marks that attribute as fixed for all jobs.  This
  change makes it possible for a transform to modify rather than simply replacing an attribute
  that that the user wishes to vary per job.
  :jira:`1369`

- Fixed bug where **Collector**, **Negotiator**, and **Schedd** core files that are naturally
  large would be deleted by *condor_preen* because the file sizes exceeded the max file size.
  :jira:`1377`

- Fixed a bug that could cause a daemon or tool to crash when
  connecting to a daemon using a security session.
  This particularly affected the *condor_schedd*.
  :jira:`1372`
