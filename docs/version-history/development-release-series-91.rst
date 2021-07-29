Development Release Series 9.1
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 9.1.2
-------------

Release Notes:

-  HTCondor version 9.1.2 released on July 29, 2021.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes a security-related bug
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html>`_.

   :jira:`509`
   :jira:`587`

Version 9.1.1
-------------

Release Notes:

-  HTCondor version 9.1.1 released on July 27, 2021 and pulled two days later when an issue was found with a patch.

New Features:

-  None.

Bugs Fixed:

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

