Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.1.2
--------------

Release Notes:

.. HTCondor version 10.1.2 released on Month Date, 2022.

- HTCondor version 10.1.2 not yet released.

New Features:

- OpenCL jobs can now run inside a Singularity container launched by HTCondor if the
  OpenCL drivers are present on the host in directory ``/etc/OpenCL/vendors``.
  :jira:`1410`

Bugs Fixed:

- None.

Version 10.1.1
--------------

Release Notes:

- HTCondor version 10.1.1 released on November 10, 2022.

- HTCondor version 10.1.1 not yet released.

New Features:

- Improvements to job hooks, including configuration knob STARTER_DEFAULT_JOB_HOOK_KEYWORD,
  the new hook PREPARE_JOB_BEFORE_TRANSFER,
  and the ability to preserve stderr from job hooks into the StarterLog or StartdLog.
  See the :ref:`admin-manual/Hooks` manual section.
  :jira:`1400`

Bugs Fixed:

- Fixed bugs in the container universe that prevented 
  apptainer-only systems from running container universe jobs
  with docker-repo style images
  :jira:`1412`

Version 10.1.0
--------------

Release Notes:

- HTCondor version 10.1.0 released on November 10, 2022.

- This version includes all the updates from :ref:`lts-version-history-1000`.

New Features:

- None.

Bugs Fixed:

- None.

