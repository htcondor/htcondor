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

- gdb and strace should now work in Docker Universe jobs.
  :jira:`349`

- Docker universe jobs are now run under the built-in docker
  init process, which means that zombie processes are automatically
  reaped.  This can be turned off with the knob 
  *DOCKER_RUN_UNDER_INIT* = false
  :jira:`462`

- The *condor_startd* on platforms that support Docker now
  runs a simple Docker container at startup to verify that
  docker universe completely works.  This can be disabled with the
  knob DOCKER_PERFORM_TEST
  :jira:`325`

Bugs Fixed:

- None.

