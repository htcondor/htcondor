Version 24.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
24.0.y versions. New features will be added in the 24.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-2402:

Version 24.0.2
--------------

Release Notes:

.. HTCondor version 24.0.2 released on Month Date, 2024.

- HTCondor version 24.0.2 not yet released.

New Features:

- Added a new config parameter, 
  :macro:`STARTER_ALWAYS_HOLD_ON_OOM` which defaults to true.
  When true, if a job is killed with an OOM signal, it is put on
  hold.  When false, the system tries to determine if the job was out
  of memory, or the system was, and if the latter, evicts the job
  and sets it back to idle.

Bugs Fixed:

- Fixed a bug where a daemon would repeatedly try to use its family
  security session when authenticating with another daemon that
  doesn't know about the session.
  :jira:`2685`

- Fixed a bug where a job would sometimes match but then fail to start on a machine
  with a START expression that referenced the :ad-attr:`KeyboardIdle` attribute.
  :jira:`2689`

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

- :meth:`htcondor2.Submit.itemdata` now correctly accepts an optional
  ``qargs`` parameter (as in version 1).
  :jira:`2618`

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
  happen if the job is stuck in NFS or running GPU code.
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
