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

- None.

Bugs Fixed:

- None.

.. _lts-version-history-2401:

Version 24.0.1
--------------

Release Notes:

.. HTCondor version 24.0.1 released on Month Date, 2024.

- HTCondor version 24.0.1 not yet released.

- :macro:`LVM_USE_THIN_PROVISIONING` now defaults to ``False``. This affects
  Execution Points using :macro:`STARTD_ENFORCE_DISK_LIMITS`.

New Features:

- :tool:`condor_gpu_discovery` can now detect GPUs using AMD's HIP 6 library.
  HIP detection will be used if the new ``-hip`` option is used or if no
  detection method is specified and no CUDA devices are detected.
  :jira:`2509`

Bugs Fixed:

- On Windows the :tool:`htcondor` tool now uses the Python C API to try and
  launch the python interpretor.  This will fail with a message
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

- :tool:`htcondor annex` now reports a proper error if you request an annex
  from a GPU-enabled queue but don't specify how many GPUs per node you
  want (and the queue does not always allocate whole nodes).
  :jira:`2633`

- Fixed a bug where the ``-divide`` flag to :tool:`condor_gpu_discovery` would
  be ignored on servers with only one type of GPU device.
  :jira:`2669`

- Fixed a bug introduced in HTCSS v23.8.1 which prevented an EP from running 
  multiple jobs on a single GPU device when ``-divde`` or ``-repeat`` was added
  to to config knob :macro:`GPU_DISCOVERY_EXTRA`. Also fixed problems with any non-fungible
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

- If HTCondor output transfer (including the standard output and error logs)
  fails after an input transfer failure, HTCondor now reports the
  input transfer failure (instead of the output transfer failure).
  :jira:`2645`
