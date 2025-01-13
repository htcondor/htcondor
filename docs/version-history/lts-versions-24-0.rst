Version 24.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
24.0.y versions. New features will be added in the 24.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-2404:

Version 24.0.4
--------------

Release Notes:

.. HTCondor version 24.0.4 released on Month Date, 2024.

- HTCondor version 24.0.4 planned release date is Month Date, 2024.

New Features:

- None.

Bugs Fixed:

- None.

.. _lts-version-history-2403:

Version 24.0.3
--------------

Release Notes:

.. HTCondor version 24.0.3 released on Month Date, 2025.

- HTCondor version 24.0.3 planned release date is January 9, 2025.

New Features:

- None.

Bugs Fixed:

- EPs spawned by `htcondor annex` no longer crash on start-up.
  :jira:`2745`

- :meth:`htcondor2.Submit.from_dag` and :meth:`htcondor.Submit.from_dag` now
  correctly raises an HTCondor exception when the processing of DAGMan
  options and submit time DAG commands fails.
  :jira:`2736`

- You can now locate a collector daemon in the htcondor2 python bindings.
  :jira:`2738`

- Fixed incompatibility of :tool:`condor_adstash` with v2.x of the OpenSearch Python Client.
  :jira:`2614`

.. _lts-version-history-2402:

Version 24.0.2
--------------

Release Notes:

- HTCondor version 24.0.2 released on November 26, 2024.

New Features:

- Added a new config parameter, 
  :macro:`STARTER_ALWAYS_HOLD_ON_OOM` which defaults to true.
  When true, if a job is killed with an OOM signal, it is put on
  hold.  When false, the system tries to determine if the job was out
  of memory, or the system was, and if the latter, evicts the job
  and sets it back to idle.
  :jira:`2686`

Bugs Fixed:

- Fixed a bug that prevents :tool:`condor_ssh_to_job` from working
  with sftp and scp modes.
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
