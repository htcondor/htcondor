Version 24.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
24.0.y versions. New features will be added in the 24.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-2401:

Version 24.0.1
--------------

Release Notes:

.. HTCondor version 24.0.1 released on Month Date, 2024.

- HTCondor version 24.0.1 not yet released.

New Features:

- None.

Bugs Fixed:

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

- Fixed bug where :tool:`condor_watch_q` would display ``None`` for jobs with
  no :ad-attr:`JobBatchName` instead of the expected :ad-attr:`ClusterId`.
  :jira:`2625`
