Development Release Series 9.1
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 9.1.0
-------------

Release Notes:

.. HTCondor version 9.1.0 released on Month Date, 2021.

- HTCondor version 9.1.0 not yet released.

- The *condor_convert_history* command was removed.
  :jira:`392`

New Features:

- The "ToE tag" entry in the job event log now includes the exit code or
  signal number, if and as appropriate.
  :jira:`429`

- gdb and strace should now work in Docker Universe jobs.
  :jira:`349`

- The *condor_startd* on platforms that support Docker now
  runs a simple Docker container at startup to verify that
  docker universe completely works.  This can be disabled with the
  knob DOCKER_PERFORM_TEST
  :jira:`325`

Bugs Fixed:

- None.

