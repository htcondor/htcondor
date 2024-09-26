Version 24 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 24.1.0
--------------

Release Notes:

.. HTCondor version 24.1.0 released on Month Date, 2024.

- HTCondor version 24.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-2401`.

New Features:

- Added :meth:`htcondor2.set_ready_state` for those brave few writing daemons
  in the Python bindings.
  :jira:`2615`

Bugs Fixed:

- If HTCondor output transfer (including the standard output and error logs)
  fails after an input transfer failure, HTCondor now reports the
  input transfer failure (instead of the output transfer failure).
  :jira:`2645`

- If HTCondor detects that an invalid checkpoint has been downloaded for a
  self-checkpoint jobs using third-party storage, that checkpoint is now
  marked for deletion and the job rescheduled.
  :jira:`1258`

