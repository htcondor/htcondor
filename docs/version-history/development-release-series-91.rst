Development Release Series 9.1
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 9.1.1
-------------

Release Notes:

.. HTCondor version 9.1.1 released on Month Date, 2021.

- HTCondor version 9.1.1 not yet released.

New Features:

- The semantics of undefined user job policy expressions has changed.  A
  policy whose expression evaluates to undefined is now uniformly ignored,
  instead of either putting the job on hold or treated as false.
  :jira:`442`

- The "ToE tag" entry in the job event log now includes the exit code or
  signal number, if and as appropriate.
  :jira:`429`

- Docker universe jobs are now run under the built-in docker
  init process, which means that zombie processes are automatically
  reaped.  This can be turned off with the knob
  *DOCKER_RUN_UNDER_INIT* = false
  :jira:`462`

- Many services support the "S3" protocol.  To reduce confusion, we've
  added new aliases for the submit-file commands ``aws_access_key_id_file``
  and ``aws_secret_access_key_file``: ``s3_access_key_id_file`` and
  ``s3_secret_access_key_file``.  We also added support for ``gs://``-style
  Google Cloud Storage URLs, with the corresponding ``gs_access_key_id_file``
  and ``gs_secret_access_key_file`` aliases.  This support, and the aliases,
  use Google Cloud Storage's "interopability" API.  The HMAC access key ID
  and secret keys may be obtained from the Google Cloud web console's
  "Cloud Storage" section, the "Settings" menu item, under the
  "interoperability" tab.
  :jira:`453`

- Add new submit command ``batch_extra_submit_args`` for grid universe jobs
  of type ``batch``.
  This lets the user supply arbitrary command-line arguments to the submit
  command of the target batch system.
  These are supplied in addition to the command line arguments derived
  from other attributes of the job ClassAd.
  :jira:`526`

Bugs Fixed:

- None.

Version 9.1.0
-------------

Release Notes:

- HTCondor version 9.1.0 released on May 20, 2021.

- The *condor_convert_history* command was removed.
  :jira:`392`

New Features:

- Added support for submission to the ARC CE REST interface via the new
  grid universe type **arc**.
  :jira:`138`

- Added a new option in DAGMan to put failed jobs on hold and keep them in the
  queue when :macro:`DAGMAN_PUT_FAILED_JOBS_ON_HOLD` is True. For some types
  of transient failures, this allows users to fix whatever caused their job to
  fail and then release it, allowing the DAG execution to continue.
  :jira:`245`

- *gdb* and *strace* now work in Docker Universe jobs.
  :jira:`349`

- The *condor_startd* on platforms that support Docker now
  runs a simple Docker container at startup to verify that
  docker universe completely works.  This can be disabled with the
  knob DOCKER_PERFORM_TEST
  :jira:`325`

- On Linux machines with performance counter support, vanilla universe jobs
  now report the number of machine instructions executed
  :jira:`390`

Bugs Fixed:

- None.

