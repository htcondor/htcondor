Stable Release Series 9.0
=========================

This is the stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
9.0.x releases. New features will be added in the 9.1.x development
series.

The details of each version are described below.

Version 9.0.1
-------------

Release Notes:

.. HTCondor version 9.0.1 released on Month Date, 2021.

- HTCondor version 9.0.1 not yet released.

New Features:

- When The ``AssignAccountingGroup`` configuration template is in effect
  And a user submits a job with a requested accounting group that they are not
  permitted to use, the submit will be rejected with an error message.
  This configuration template has a new optional second argument that can be used
  to quietly ignore the requested accounting group instead.
  :jira:`426`

- Added the OpenBLAS environment variable ``OPENBLAS_NUM_THREADS`` to the list
  of environment variables exported by the *condor_starter* per these
  `recommendations <https://github.com/xianyi/OpenBLAS/wiki/faq#how-can-i-use-openblas-in-multi-threaded-applications>`_.
  :jira:`444`

- HTCondor now parses ``/usr/share/condor/config.d/`` for configuration before
  ``/etc/condor/config.d``, so that packagers have a convenient place to adjust
  the HTCondor configuration.
  :jira:`45`

Bugs Fixed:

- Fixed a bug in the implementation of the submit commands ``max_retries``
  and ``success_exit_code`` which would cause jobs which exited on a
  signal to go on hold (instead of exiting or being retried).
  :jira:`430`

- Fixed a memory leak in the job router, usually triggered when job
  policy expressions cause removal of the job.
  :jira:`408`

- Fixed a bug in the way ``AutoClusterAttrs`` was calculated that could
  in matchmaking ignoring attributes changed by ``job_machine_attrs``.
  :jira:`414`

- Fixed a bug in *condor_submit* when a ``SEC_CREDENTIAL_PRODUCER`` was
  configured that could result in *condor_submit* reporting that the
  Queue statement of a submit file was missing or invalid.
  :jira:`427`

- Fixed some bugs that caused ``bosco_cluster --add`` to fail.
  Allow ``remote_gahp`` to work with older Bosco installations via
  the ``--rgahp-script`` option.
  :jira:`438`
  :jira:`433`
  :jira:`451`

- Fixed a bug in the local issuer *condor_credmon_oauth* where SciTokens version
  2.0 tokens were being generated without an "aud" claim. The "aud" claim is now
  set to ``LOCAL_ISSUER_TOKEN_AUDIENCE``. The "ver" claim can be changed from
  the default of "scitokens:2.0" by setting ``LOCAL_ISSUER_TOKEN_VERSION``.
  :jira:`445`

Version 9.0.0
-------------

Release Notes:

- HTCondor version 9.0.0 released on April 14, 2021.

- The installer was for Windows platforms was not ready for 9.0.0.
  Windows support will appear in 9.0.1.

- Removed support for CREAM and Unicore grid jobs, glexec privilege separation, DRMAA, and *condor_cod*.

Known Issues:

- MUNGE security is temporarily broken.

- The *bosco_cluster* command is temporarily broken.

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
