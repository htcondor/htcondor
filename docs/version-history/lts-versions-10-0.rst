Version 10.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
10.0.y versions. New features will be added in the 10.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-1004:

Version 10.0.4
--------------

Release Notes:

.. HTCondor version 10.0.4 released on Month Date, 2023.

- HTCondor version 10.0.4 not yet released.

- Ubuntu 18.04 (Bionic Beaver) is no longer supported, since its end of life
  is April 30th, 2023.

New Features:

- None.

Bugs Fixed:

- None.

.. _lts-version-history-1003:

Version 10.0.3
--------------

Release Notes:

.. HTCondor version 10.0.3 released on Month Date, 2023.

- HTCondor version 10.0.3 not yet released.

- If you set :macro:`CERTIFICATE_MAPFILE_ASSUME_HASH_KEYS` and use ``/`` to
  mark the beginning and end of a regular expression, the character sequence
  ``\\`` in the mapfile now passes a single ``\`` to the regular expression
  engine.  This allows you to pass the sequence ``\/`` to the regular
  expression engine (put ``\\\/`` in the map file), which was not previously
  possible.  If the macro above is set and you have a ``\\`` in your map file,
  you will need to replace it with ``\\\\``.
  :jira:`1573`

- For *condor_annex* users: Amazon Web Services is deprecating the Node.js
  12.x runtime.  If you ran the *condor_annex* setup command with a previous
  version of HTCondor, you'll need to update your setup.  Go to the AWS
  CloudFormation `console <https://console.aws.amazon.com/cloudformation/>`_
  and look for the stack named ``HTCondorAnnex-LambdaFunctions``.  (You
  may have to switch regions.)  Click on that stack's radio button, hit
  the delete button in the table header, and confirm.  Wait for the delete
  to finish.  Then run ``condor_annex -aws-region region-name-N -setup``
  for the region.  Repeat for each region of interest.
  :jira:`1627`.

New Features:

- Allow remote submission of **batch** grid universe jobs via ssh to work
  with sites that were configured with the old *bosco_cluster* tool.
  :jira:`1632`

Bugs Fixed:

- Fixed a rare bug in the late materialization code that could
  cause a schedd crash.
  :jira:`1581`

- Fixed bug where the *condor_shadow* would crash during job removal.
  :jira:`1585`

- Fixed a bug where two *condor_schedd* daemons in a High Availability
  configuration could be active at the same time.
  :jira:`1590`

- Improved the HTCondor's systemd configuration to not start HTCondor until the
  system attempts (and mostly likely succeeds) to mount remote filesystems.
  :jira:`1594`

- Fixed bug where a *condor_dagman* node with ``RETRY`` capabilites would instantly
  restart that node everytime it saw a job proc failure. This would result in nodes
  with multi-proc jobs to resubmit the entire node multiple times causing internal
  issues for DAGMan.
  :jira:`1607`

- Fixed a bug where the *condor_master* of a glidein submitted to
  SLURM via HTCondor-CE would try to talk to the *condor_gridmanager*
  of the HTCondor-CE.
  :jira:`1604`

- Fixed a bug in the *condor_schedd* that could result in the ``TotalSubmitProcs``
  attribute of a late materialization job being set to a value smaller than the
  correct value shortly after the *condor_schedd* was restarted.
  :jira:`1603`

- If a job's requested credentials are not available when the job is
  about to start, the job is now placed on hold.
  :jira:`1600`

- Fixed a bug that would cause the *condor_schedd* to hang if an
  invalid condor cron argument was submitted
  :jira:`1624`

- Fixed a bug where cron jobs put on hold due to invalid time specifications
  would be unable to be removed from the job queue with tools.
  :jira:`1629`

- Fixed how the *condor_gridmanager* handles failed ARC CE jobs.
  Before, it would endlessly re-query the status of jobs that failed
  during submission to the LRMS behind ARC CE.
  If ARC CE reports a job as FAILED because the job exited with a
  non-zero exit code, the *condor_gridmanager* now treats it as a
  successful execution.
  :jira:`1583`

.. _lts-version-history-1002:

Version 10.0.2
--------------

Release Notes:

- HTCondor version 10.0.2 released on March 2, 2023.

- HTCondor Python wheel is now available for Python 3.11 on PyPI.
  :jira:`1586`

- The macOS tarball is now being built on macOS 11.
  :jira:`1610`

New Features:

- Added configuration option called :macro:`ALLOW_TRANSFER_REMAP_TO_MKDIR` to allow
  a transfer output remap to create directories in allowed places if they
  do not exist at transfer output time.
  :jira:`1480`

- Improved scalability of *condor_schedd* when running more than 1,000 jobs
  from the same user.
  :jira:`1549`

- *condor_ssh_to_job* should now work in glidein and other environments
  where the job or HTCondor is running as a Unix user id that doesn't
  have an entry in the /etc/passwd database.
  :jira:`1543`

- VM universe jobs are now configured to pass through the host CPU model
  to the VM. This change enables VMs with newer kernels (such as Enterprise
  Linux 9) to operate in VM Universe.
  :jira:`1559`

- The *condor_remote_cluster* command was updated to fetch the Alma Linux
  tarballs for Enterprise Linux 8 and 9.
  :jira:`1562`

Bugs Fixed:

- In the python bindings, the attribute ``ServerTime`` is now included
  in job ads returned by ``Schedd.query()`` to support Fifemon.
  :jira:`1531`

- Fixed issue when HTCondor could not be installed on Ubuntu 18.04
  (Bionic Beaver).
  :jira:`1548`

- Attempting to use a file-transfer plug-in that doesn't exist is no longer
  silently ignored.  This could happen due to different bug, also fixed, where plug-ins
  specified only in ``transfer_output_remaps`` were not automatically added
  to a job's requirements.
  :jira:`1501`

- Fixed a bug where **condor_now** could not use the resources freed by
  evicting a job if its procID was 1.
  :jira:`1519`

- Fixed a bug that caused the *condor_startd* to exit when thinpool
  provisioned filesystems were enabled.
  :jira:`1524`

- Fixed a bug causing a Python warning when installing on Ubuntu 22.04.
  :jira:`1534`

- Fixed a bug where the *condor_history* tool would crash
  when doing a remote query with a constraint expression or specified
  job IDs.
  :jira:`1564`

.. _lts-version-history-1001:

Version 10.0.1
--------------

Release Notes:

- HTCondor version 10.0.1 released on January 5, 2023.

New Features:

- Add support for Ubuntu 22.04 LTS (Jammy Jellyfish).
  :jira:`1304`

- HTCondor now includes a file transfer plugin that support ``stash://``
  and ``osdf://`` URLs.
  :jira:`1332`

- The Windows installer now uses the localized name of the Users group
  so that it can be installed on non-English Windows platforms.
  :jira:`1474`

- OpenCL jobs can now run inside a Singularity container launched by HTCondor if the
  OpenCL drivers are present on the host in directory ``/etc/OpenCL/vendors``.
  :jira:`1410`

- The ``CompletionDate`` attribute of jobs is now undefined until such time as the job completes
  previously it was 0.
  :jira:`1393`

Bugs Fixed:

- Fixed a bug where Debian, Ubuntu and other Linux platforms with
  swap accounting disabled in the kernel would never put
  a job on hold if it exceeded RequestMemory and
  MEMORY_LIMIT_POLICY was set to hard or soft.
  :jira:`1466`

- Fixed a bug where using the ``-forcex`` option with *condor_rm*
  on a scheduler universe job could cause a *condor_schedd* crash.
  :jira:`1472`

- Fixed bugs in the container universe that prevented
  apptainer-only systems from running container universe jobs
  with Docker repository style images.
  :jira:`1412`

- Docker universe and container universe job that use the docker runtime now detect
  when the Unix uid or gid has the high bit set, which docker does not support.
  :jira:`1421`

- Grid universe **batch** works again on Debian and Ubuntu.
  Since 9.5.0, some required files had been missing.
  :jira:`1475`

- Fixed bug in the curl plugin where it would crash on Enterprise Linux 8
  systems when using a file:// url type.
  :jira:`1426`

- Fixed bug in where the multi-file curl plugin would fail to timeout
  due lack of upload or download progress if a large amount of bytes
  where transferred at some point.
  :jira:`1403`
  
- Fixed bug where the multi-file curl plugin would fail to receive a SciToken
  if it was in raw format rather than json.
  :jira:`1447`
  
- Fixed a bug that prevented the starter from properly mounting
  thinpool provisioned ephemeral scratch directories.
  :jira:`1419`

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
  when the format file contained a ``FIELDPREFIX``, ``FIELDSUFFIX``, ``RECORDPREFIX`` or ``RECORDSUFFIX``.
  :jira:`1464`

- Fixed a bug in the ``RENAME`` command of the transform language that could result in a
  crash of the *condor_schedd* or *condor_job_router*.
  :jira:`1486`

- For tarball installations, the *condor_configure* script now configures
  HTCondor to use user based security.
  :jira:`1461`

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
