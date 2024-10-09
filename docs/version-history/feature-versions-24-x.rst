Version 24 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 24.2.1
--------------

Release Notes:

.. HTCondor version 24.2.1 released on Month Date, 2024.

- HTCondor version 24.2.1 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2402`.

New Features:

- None.

Bugs Fixed:

- None.

Version 24.1.1
--------------

Release Notes:

.. HTCondor version 24.1.0 released on Month Date, 2024.

- HTCondor version 24.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2401`.

New Features:

- Added ``get`` to the ``htcondor credential`` noun, which prints the contents
  of a stored OAuth2 credential.
  :jira:`2626`

- Added :meth:`htcondor2.set_ready_state` for those brave few writing daemons
  in the Python bindings.
  :jira:`2615`

- When blah_debug_save_submit_info is set in blah.config, the stdout 
  and stderr of the blahp's wrapper script is saved under the given 
  directory. 
  :jira:`2636`

- The DAG command :dag-cmd:`SUBMIT-DESCRIPTION` and node inline submit
  descriptions now work when :macro:`DAGMAN_USE_DIRECT_SUBMIT` = ``False``.
  :jira:`2607`

- Docker universe jobs now check the Architecture field in the image,
  and if it doesn't match the architecture of the EP, the job is put
  on hold.  The new parameter :macro:`DOCKER_SKIP_IMAGE_ARCH_CHECK` skips this.
  :jira:`2661`

Bugs Fixed:

- If HTCondor output transfer (including the standard output and error logs)
  fails after an input transfer failure, HTCondor now reports the
  input transfer failure (instead of the output transfer failure).
  :jira:`2645`

- If HTCondor detects that an invalid checkpoint has been downloaded for a
  self-checkpoint jobs using third-party storage, that checkpoint is now
  marked for deletion and the job rescheduled.
  :jira:`1258`

