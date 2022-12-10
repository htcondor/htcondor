Version 10.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
10.0.y versions. New features will be added in the 10.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-1001:

Version 10.0.1
--------------

Release Notes:

.. HTCondor version 10.0.1 released on Month Date, 2022.

- HTCondor version 10.0.1 not yet released.

New Features:

- The Windows installer now uses the localized name of the Users group
  so that it can be installed on non-english Windows platforms
  :jira:`1474`

Bugs Fixed:

- Fixed a bug where *condor_rm*'ing with the -forcex option
  on a scheduler universe job could cause a schedd crash.
  :jira:`1472`

- Fixed a bug where Debian, Ubuntu and other Linux platforms with
  swap accounting disabled in the kernel would never put
  a job on hold if it exceeded RequestMemory and
  MEMORY_LIMIT_POLICY was set to hard or soft.
  :jira:`1466`

- Fixed bug in the curl plugin where it would crash on EL8
  systems when using a file:// url type
  :jira:`1426`

- Fixed bug in where the multifile curl plugin would fail to timeout
  due lack of upload or download progress if a large amount of bytes
  where transfered at some point.
  :jira:`1403`
  
- Fixed a bug that prevented the starter from properly mounting
  thinpool provisioned ephemeral scratch directories.
  :jira:`1419`

- Fixed bugs in the container universe that prevented
  apptainer-only systems from running container universe jobs
  with docker-repo style images.
  :jira:`1412`

- Docker universe and container universe job that use the docker runtime now detect
  when the unix uid or gid has the high bit set, which docker does not support.
  :jira:`1421`

- Fixed bug where the multifile curl plugin would fail to recieve scitoken
  if it was in raw format rather than json.
  :jira:`1447`
  
- Fixed a bug where SSL authentication with the *condor_collector* could
  fail when the provided hostname is not a DNS CNAME.
  :jira:`1443`

- Fixed a Vault credmon bug where tokens were being refreshed too often.
  :jira:`1017`

- Fixed a Vault credmon bug where the CA certificates used were not based on the
  HTCondor configuration.
  :jira:`1179`

- Fixed the *condor_gridmanager* to recognize when it has the final 
  data for an ARC job in the FAILED status with newer versions of ARC CE. 
  Before, the *condor_gridmanager* would leave the job marked as 
  RUNNING and retry querying the ARC CE server endlessly. 
  :jira:`1448`

- Fixed AES encryption failures on macOS Ventura.
  :jira:`1458`

- Fixed a bug that would cause tools that have the ``-printformat`` argument to segfault
  when the format file contained a ``FIELDPREFIX``,``FIELDSUFFIX``,``RECORDPREFIX`` or ``RECORDSUFFIX``.
  :jira:`1464`

.. _lts-version-history-1000:

Version 10.0.0
--------------

Release Notes:

- HTCondor version 10.0.0 released on November 10, 2022.

New Features:

- The default for ``TRUST_DOMAIN``, which is used by with IDTOKEN authentication
  has been changed to ``$(UID_DOMAIN)``.  If you have already created IDTOKENs for 
  use in your pool, you should configure ``TRUST_DOMAIN`` to the issuer value of a valid token.
  :jira:`1381`

- The *condor_transform_ads* tool now has a ``-jobtransforms`` argument that reads
  transforms from the configuration.  This provides a convenient way to test the
  ``JOB_TRANSFORM_<NAME>`` configuration variables.
  :jira:`1312`

- Added new automatic configuration variable ``DETECTED_CPUS_LIMIT`` which gets set
  to the minimum of ``DETECTED_CPUS`` from the configuration and ``OMP_NUM_THREADS``
  and ``SLURM_CPU_ON_NODES`` from the environment.
  :jira:`1307`

Bugs Fixed:

- Fixed a bug where if a job created a symbolic link to a file, the contents of
  that file would be counted in the job's `DiskUsage`.  Previously,
  symbolic links to directories were (correctly) ignored, but not symbolic links to
  files.
  :jira:`1354`

- Fixed a bug where if SINGULARITY_TARGET_DIR is set, condor_ssh_to
  job would start the interactive shell in the root directory of
  the job, not in the current working directory of the job.
  :jira:`1406`

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

- Fixed a bug that could cause digits to be truncated reading resource usage information
  from the job event log via the Python or C++ APIs for reading event logs. Note this only
  happens for very large values of requested or allocated disk, memory.
  :jira:`1263`

- Fixed a bug where GPUs that were marked as OFFLINE in the **Startd** would still be available
  for matchmaking in the ``AvailableGPUs`` attribute.
  :jira:`1397`

- The executables within the tarball distribution now use ``RPATH`` to find
  shared libraries.  Formerly, ``RUNPATH`` was used and tarballs became
  susceptible to failures when independently compiled HTCondor libraries were
  present in the ``LD_LIBRARY_PATH``.
  :jira:`1405`
