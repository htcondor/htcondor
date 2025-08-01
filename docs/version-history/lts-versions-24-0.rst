Version 24.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
24.0.y versions. New features will be added in the 24.x.y feature versions.

The details of each version are described below.

Version 24.0.12
---------------

Release Notes:

.. HTCondor version 24.0.12 released on September 18, 2025.

- HTCondor version 24.0.12 planned release date is September 18, 2025.

New Features:

.. include-history:: features 24.0.12 23.10.29 23.0.29

Bugs Fixed:

.. include-history:: bugs 24.0.12 23.10.29 23.0.29

Version 24.0.11
---------------

Release Notes:

.. HTCondor version 24.0.11 released on August 21, 2025.

- HTCondor version 24.0.11 planned release date is August 21, 2025.

New Features:

.. include-history:: features 24.0.11 23.10.28 23.0.28

Bugs Fixed:

.. include-history:: bugs 24.0.11 23.10.28 23.0.28

Version 24.0.10
---------------

Release Notes:

- HTCondor version 24.0.10 released on July 28, 2025.

New Features:

.. include-history:: features 24.0.10 23.10.27 23.0.27

Bugs Fixed:

.. include-history:: bugs 24.0.10 23.10.27 23.0.27

Version 24.0.9
--------------

Release Notes:

- HTCondor version 24.0.9 released on June 26, 2025.

New Features:

.. include-history:: features 24.0.9 23.10.26 23.0.26

Bugs Fixed:

.. include-history:: bugs 24.0.9 23.10.26 23.0.26

Version 24.0.8
--------------

Release Notes:

- HTCondor version 24.0.8 released on June 12, 2025.

New Features:

.. include-history:: features 24.0.8 23.10.25 23.0.25

Bugs Fixed:

.. include-history:: bugs 24.0.8 23.10.25 23.0.25

Version 24.0.7
--------------

Release Notes:

- HTCondor version 24.0.7 released on April 22, 2025.

New Features:

.. include-history:: features 24.0.7 23.10.24 23.0.24

Bugs Fixed:

.. include-history:: bugs 24.0.7 23.10.24 23.0.24

Version 24.0.6
--------------

Release Notes:

- HTCondor version 24.0.6 released on March 27, 2025.

New Features:

- None.

Bugs Fixed:

- *Security Item*: This release of HTCondor fixes a security-related bug
  described at

  - `http://htcondor.org/security/vulnerabilities/HTCONDOR-2025-0001 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2025-0001>`_.

  :jira:`2900`

Version 24.0.5
--------------

Release Notes:

- HTCondor version 24.0.5 released on March 4, 2025.

New Features:

.. include-history:: features 24.0.5 23.10.21 23.0.21

Bugs Fixed:

.. include-history:: bugs 24.0.5 23.10.21 23.0.21

Version 24.0.4
--------------

Release Notes:

- HTCondor version 24.0.4 released on February 4, 2025.

New Features:

.. include-history:: features 24.0.4 23.10.20 23.0.20

Bugs Fixed:

.. include-history:: bugs 24.0.4 23.10.20 23.0.20

.. _lts-version-history-2403:

Version 24.0.3
--------------

Release Notes:

- HTCondor version 24.0.3 released on January 6, 2025.

New Features:

- Add new knob :macro:`CGROUP_POLLING_INTERVAL` which defaults to 5 (seconds), to
  control how often a cgroup system polls for resource usage.
  :jira:`2802`

Bugs Fixed:

- EPs spawned by `htcondor annex` no longer crash on start-up.
  :jira:`2745`

- When resolving a hostname to a list of IP addresses, avoid using
  IPv6 link-local addresses.
  This change was done incorrectly in 23.9.6.
  :jira:`2746`

- :meth:`htcondor2.Submit.from_dag` and :meth:`htcondor.Submit.from_dag` now
  correctly raises an HTCondor exception when the processing of DAGMan
  options and submit time DAG commands fails.
  :jira:`2736`

- Fixed confusing job hold message that would state a job requested
  ``0.0 GB`` of disk via :subcom:`request_disk` when exceeding disk
  usage on Execution Points using :macro:`STARTD_ENFORCE_DISK_LIMITS`.
  :jira:`2753`

- You can now locate a collector daemon in the htcondor2 Python bindings.
  :jira:`2738`

- Fixed a bug in *condor_qusers* tool where the ``add`` argument would always
  enable rather than add a user.
  :jira:`2775`

- Fixed a bug where cgroup systems did not report peak memory, as intended
  but current instantaneous memory instead.
  :jira:`2800` :jira:`2804`

- Fixed an inconsistency in cgroup v1 systems where the memory reported
  by condor included memory used by the kernel to cache disk pages.
  :jira:`2807`

- Fixed a bug on cgroup v1 systems where jobs that were killed by the
  Out of Memory killer did not go on hold.
  :jira:`2806`

- Fixed incompatibility of :tool:`condor_adstash` with v2.x of the OpenSearch Python Client.
  :jira:`2614`

- The ``-subsystem`` argument of *condor_status* is once again case-insensitive for credd
  and defrag subsystem types.
  :jira:`2796`

.. _lts-version-history-2402:

Version 24.0.2
--------------

Release Notes:

- HTCondor version 24.0.2 released on November 26, 2024.

New Features:

- Added a new configuration parameter, 
  :macro:`STARTER_ALWAYS_HOLD_ON_OOM` which defaults to true.
  When true, if a job is killed with an OOM signal, it is put on
  hold.  When false, the system tries to determine if the job was out
  of memory, or the system was, and if the latter, evicts the job
  and sets it back to idle.
  :jira:`2686`

Bugs Fixed:

- Fixed a bug that prevents :tool:`condor_ssh_to_job` from working
  with ``sftp`` and ``scp`` modes.
  :jira:`2687`

- Fixed a bug where a daemon would repeatedly try to use its family
  security session when authenticating with another daemon that
  doesn't know about the session.
  :jira:`2685`

- Fixed a bug where a job would sometimes match but then fail to start on a machine
  with a START expression that referenced the :ad-attr:`KeyboardIdle` attribute.
  :jira:`2689`

- :meth:`htcondor2.Submit.itemdata` now correctly accepts an optional
  ``qargs`` parameter (as in version 1).
  :jira:`2618`

- Stop signaling the *condor_credmon_oauth* daemon on every job submission
  when there's no work for it to do. This will hopefully reduce the
  frequency of some errors in the *condor_credmon_oauth*.
  :jira:`2653`

- Fixed a bug that could cause the *condor_schedd* to crash if a job's
  ClassAd contained a $$() macro that couldn't be expanded.
  :jira:`2730`

- Docker universe jobs now check the Architecture field in the image,
  and if it doesn't match the architecture of the EP, the job is put
  on hold.  The new parameter :macro:`DOCKER_SKIP_IMAGE_ARCH_CHECK` skips this.
  :jira:`2661`

.. _lts-version-history-2401:

Version 24.0.1
--------------

Release Notes:

- HTCondor version 24.0.1 released on October 31, 2024.

- :macro:`LVM_USE_THIN_PROVISIONING` now defaults to ``False``. This affects
  Execution Points using :macro:`STARTD_ENFORCE_DISK_LIMITS`.

- HTCondor tarballs now contain `Pelican 7.10.11 <https://github.com/PelicanPlatform/pelican/releases/tag/v7.10.11>`_

New Features:

- :tool:`condor_gpu_discovery` can now detect GPUs using AMD's HIP 6 library.
  HIP detection will be used if the new ``-hip`` option is used or if no
  detection method is specified and no CUDA devices are detected.
  :jira:`2509`

Bugs Fixed:

- On Windows the :tool:`htcondor` tool now uses the Python C API to try and
  launch the python interpreter.  This will fail with a message
  box about installing python if python 3.9 is not in the path.
  :jira:`2650`

- :meth:`htcondor2.Submit.from_dag` now recognizes ``DoRecov`` as a
  synonym for ``DoRecovery``.  This improves compatibility with
  version 1.
  :jira:`2613`

- :meth:`htcondor2.Submit.itemdata` now (correctly) returns an iterator over
  dictionaries if the :obj:`htcondor2.Submit` object specified variable
  names in its ``queue`` statement.
  :jira:`2613`

- When you specify item data using a :class:`dict`, HTCondor will now
  correctly reject values containing newlines.
  :jira:`2616`

- When docker universe jobs failed with a multi-line errors from
  docker run, the job used to fail with an "unable to inspect container"
  message.  Now the proper hold message is set and the job goes on
  hold as expected.
  :jira:`2679`

- :tool:`htcondor annex` now reports a proper error if you request an annex
  from a GPU-enabled queue but don't specify how many GPUs per node you
  want (and the queue does not always allocate whole nodes).
  :jira:`2633`

- Fixed a bug where HTCondor systems configured to use cgroups on Linux
  to measure memory would reuse the peak memory from the previous job
  in a slot, if any process in the former job was unkillable.  This can
  happen if the job is stuck in NFS or running GPU code. Instead, 
  HTCondor polls the current memory and keeps the peak itself internally.
  :jira:`2647`

- Fixed a bug where the ``-divide`` flag to :tool:`condor_gpu_discovery` would
  be ignored on servers with only one type of GPU device.
  :jira:`2669`

- Fixed a bug introduced in HTCSS v23.8.1 which prevented an EP from running 
  multiple jobs on a single GPU device when ``-divide`` or ``-repeat`` was added
  to to configuration knob :macro:`GPU_DISCOVERY_EXTRA`. Also fixed problems with any non-fungible
  machine resource inventory that contained repeated identifiers.
  :jira:`2678`

- Fixed a bug where :tool:`condor_watch_q` would display ``None`` for jobs with
  no :ad-attr:`JobBatchName` instead of the expected :ad-attr:`ClusterId`.
  :jira:`2625`

- :meth:`htcondor2.Schedd.submit` now correctly raises a :obj:`TypeError`
  when passed a description that is not a :obj:`htcondor2.Submit` object.
  :jira:`2631`

- When submitting jobs to an SGE cluster via the grid universe, the
  blahp no longer saves the output of its wrapper script in the user's
  home directory (where the files would accumulate and never be
  cleaned up).
  :jira:`2630`

- Improved the error message when job submission as a disallowed user
  fails (i.e. submitting as the 'condor' or 'root' user).
  :jira:`2638`

- Fixed bug in :tool:`htcondor server status` that caused incorrect output
  if :macro:`DAEMON_LIST` contained commas.
  :jira:`2667`

- Fixed the new default security configuration to work with older binaries.
  :jira:`2701`

- An unresponsive libvirtd daemon no longer causes the *condor_startd*
  to block indefinitely.
  :jira:`2644`
