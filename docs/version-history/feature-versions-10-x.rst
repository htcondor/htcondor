Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.1.0
--------------

Release Notes:

.. HTCondor version 10.1.0 released on Month Date, 2022.

- HTCondor version 10.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-1000`.

New Features:

- *condor_q* default behavior of displaying the cumulative run time has changed
  to now display the current run time for jobs in running, tranfering output,
  and suspended states while displaying the previous run time for jobs in idle or held
  state unless passed ``-cumulative-time`` to show the jobs cumulative run time for all runs.
  :jira:`1064`

- *condor_history* will now stop searching history files once all requested job ads are
  found if passed ClusterIds or ClusterId.ProcId pairs.
  :jira:`1364`

- Improved *condor_history* search speeds when searching for matching jobs, matching clusters,
  and matching owners.
  :jira:`1382`

- The *CompletionDate* attribute of jobs is now undefined until such time as the job completes
  previously it was 0.
  :jira:`1393`

- The local issuer credmon can optionally add group authorizations to users' tokens by setting
  ``LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`` and ``LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE``.
  :jira:`1402`

Bugs Fixed:

- None.

