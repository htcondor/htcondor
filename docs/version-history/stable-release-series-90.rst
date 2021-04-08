Stable Release Series 9.0
=========================

This is the stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
9.0.x releases. New features will be added in the 9.1.x development
series.

The details of each version are described below.

Version 9.0.X
-------------

Release Notes:

.. HTCondor version 9.0.X released on Month Date, 2021.

- HTCondor version 9.0.X not yet released.

New Features:

- A new tool *condor_check_config* can be used after an upgrade when you had a working
  condor configuration before the upgrade. It will report configuration values that should be changed.
  In this version the tool for a few things related to the change to a more secure configuration by default.
  :jira:`384`


Bugs Fixed:

- Fixed a memory leak in the job router, usually triggered when job
  policy expressions cause removal of the job.
  :jira:`408`

