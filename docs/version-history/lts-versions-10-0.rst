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

- Fixed bug were certain **Job** variables like ``accounting_group`` and ``accounting_group_user``
  couldn't be declared specifically for DAGMan Jobs because DAGMan would always write over
  the variables at **Job** submission time.
  :jira:`1277`

