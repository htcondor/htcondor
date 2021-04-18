Stable Release Series 9.0
=========================

This is the stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
9.0.x releases. New features will be added in the 9.1.x development
series.

The details of each version are described below.

Version 9.0.0
-------------

Release Notes:

- HTCondor version 9.0.0 released on April 14, 2021.

- Removed support for CREAM and Unicore grid jobs, glexec privilege separation, DRMAA, and *condor_cod*.

Known Issue:

- MUNGE security is temporarily broken.

New Features:

- A new tool *condor_check_config* can be used after an upgrade when you had a working
  condor configuration before the upgrade. It will report configuration values that should be changed.
  In this version the tool for a few things related to the change to a more secure configuration by default.
  :jira:`384`

- The *condor_gpu_discovery* tool now defaults to using ``-short-uuid`` form for GPU ids on machines
  where the CUDA driver library has support for them. A new option ``-by-index`` has been added
  to select index-based GPU ids.
  :jira:`145`


Bugs Fixed:

- Fixed a bug introduced in 8.9.12 where the condor_job_router inside a CE would crash when
  evaluating periodic expressions
  :jira:`402`

Version 9.0.1
-------------

Release Notes:

.. HTCondor version 9.0.1 released on Month Date, 2021.

- HTCondor version 9.0.1 not yet released.

New Features:

- None.

Bugs Fixed:

- Fixed a memory leak in the job router, usually triggered when job
  policy expressions cause removal of the job.
  :jira:`408`

- Fixed a bug in the way ``AutoClusterAttrs`` was calculated that could
  in matchmaking ignoring attributes changed by ``job_machine_attrs``.
  :jira:`414`

