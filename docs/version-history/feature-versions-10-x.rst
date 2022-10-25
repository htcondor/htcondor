Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.1.0
--------------

Release Notes:

.. HTCondor version 10.1.0 released on Month Date, 2022.

- HTCondor version 10.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-100x`.

- We changed the semantics of relative paths in the ``output``, ``error``, and
  ``transfer_output_remaps`` submit file commands.  These commands now create
  the directories named in relative paths if they do not exist.  This could
  cause jobs that used to go on hold (because they couldn't write their
  ``output`` or ``error`` files, or a remapped output file) to instead succeed.
  :jira:`1325`

New Features:

- *condor_q* default behavior of displaying the cumulative run time has changed
  to now display the current run time for jobs in running, tranfering output,
  and suspended states while displaying the previous run time for jobs in idle or held
  state unless passed ``-cumulative-time`` to show the jobs cumulative run time for all runs.
  :jira:`1064`

- *condor_history* will now stop searching history files once all requested job ads are
  found if passed ClusterIds or ClusterId.ProcId pairs.
  :jira:`1364`

Bugs Fixed:

- None.

