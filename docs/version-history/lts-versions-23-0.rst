Version 23.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
23.0.y versions. New features will be added in the 23.x.y feature versions.

.. warning::
    The configuration macros JOB_ROUTER_DEFAULTS, JOB_ROUTER_ENTRIES, JOB_ROUTER_ENTRIES_CMD,
    and JOB_ROUTER_ENTRIES_FILE are deprecated and will be removed for V24 of HTCondor. New
    configuration syntax for the job router is defined using JOB_ROUTER_ROUTE_NAMES and
    JOB_ROUTER_ROUTE_<name>. Note: The removal will occur during the lifetime of the
    HTCondor V23 feature series.
    :jira:`1968`

The details of each version are described below.

.. _lts-version-history-2304:

Version 23.0.4
--------------

Release Notes:

.. HTCondor version 23.0.4 released on Month Date, 2023.

- HTCondor version 23.0.4 not yet released.

New Features:

- None.

Bugs Fixed:

- None.

.. _lts-version-history-2303:

Version 23.0.3
--------------

Release Notes:

.. HTCondor version 23.0.3 released on Month Date, 2023.

- HTCondor version 23.0.3 not yet released.

New Features:

- Improve ``htcondor job status`` command to display information about
  a jobs goodput.
  :jira:`1982`

- Added ``ROOT_MAX_THREADS`` to :macro:`STARTER_NUM_THREADS_ENV_VARS` default value.
  :jira:`2137`

- Preliminary support for openSUSE LEAP 15.
  :jira:`2156`

Bugs Fixed:

- The file transfer plugin documents that an exit code of 0
  is success, 1 is failure, and 2 is reserved for future work to
  handle the need to refresh credentials.  The definition has now
  changed so that any non-zero exit codes are treated as an error
  putting the job on hold.
  :jira:`2205`

- Fixed a bug where any file I/O error (such as disk full) was
  ignored by the *condor_starter* when writing the classad file
  that controlled file transfer plugins.  As a result, in rare
  cases, file transfer plugins could be unknowingly given
  incomplete sets of files to transfer.
  :jira:`2203`

- Fixed a crash in the Python bindings when job submit fails due to
  any reason.  A common reason might be when :macro:`SUBMIT_REQUIREMENT_NAMES`
  fails.
  :jira:`1931`

- There is a fixed size limit of 5120 bytes for chip commands.  The
  starter now returns an error, and the chirp_client prints out
  an error when requested to send a chirp command over this limit.
  Previously, these were silently ignored.
  :jira:`2157`

- Linux jobs with a invalid ``#!`` interpreter now get a better error
  message when the EP is running as root.  This was enhanced in 10.0,
  but a bug prevented the enhancement from fully working with a rootly EP.
  :jira:`1698`

- Fixed a bug where the DAGMan job proper for a DAG with a final
  node could stay stuck in the removed job state.
  :jira:`2147`

- Fixed a bug where the Python-based HTChirp client had its max line length set
  much shorter than is allowed by the HTCondor Chirp server. The client now
  also throws a relevant error when this max limit is hit while sending commands
  to the server.
  :jira:`2142`

- Updated the usage and man page of the *condor_drain* tool to include information
  about the ``-reconfig-on-completion`` option.
  :jira:`2164`

- Correctly identify ``GPUsAverageUsage`` and ``GPUsMemoryUsage`` as floating point
  values for *condor_adstash*.
  :jira:`2170`

- Fixed a bug where *condor_adstash* would get wedged due to a logging failure.
  :jira:`2166`

.. _lts-version-history-2302:

Version 23.0.2
--------------

Release Notes:

- HTCondor version 23.0.2 released on November 20, 2023.

New Features:

- None.

Bugs Fixed:

- Fixed a bug when Hashicorp Vault is configured to issue data transfer tokens
  (which is not the default), job submission could hang and then fail.
  Reverted a change to *condor_submit* that disconnected the output stream of
  :macro:`SEC_CREDENTIAL_STORER` to the user's console, which broke OIDC flow.
  :jira:`2078`

- Fixed a bug that could result in job sandboxes not being cleaned up 
  for **batch** grid jobs submitted to a remote cluster. 
  :jira:`2073`

- Improved cleanup of ssh-agent processes when submitting **batch**
  grid universe jobs to a remote cluster via ssh.
  :jira:`2118`

- Fixed a bug where the *condor_negotiator* could fail to contact a
  *condor_schedd* that's on the same private network.
  :jira:`2115`

- Fixed :macro:`CGROUP_MEMORY_LIMIT_POLICY` = ``custom`` for cgroup v2 systems.
  :jira:`2133`

- Implemented :macro:`DISABLE_SWAP_FOR_JOB` support for cgroup v2 systems.
  :jira:`2127`

- Fixed a bug in the OAuth and Vault credmons where log files would not
  rotate according to the configuration.
  :jira:`2013`

- Fixed a bug in the *condor_schedd* where it would not create a permanent User
  record when a queue super user submitted a job for a different owner.  This 
  bug would sometimes cause the *condor_schedd* to crash after a job for a new
  user was submitted.
  :jira:`2131`

- Fixed a bug that could cause jobs to be created incorrectly when a using
  ``initialdir`` and ``max_idle`` or ``max_materialize`` in the same submit file.
  :jira:`2092`

- Fixed bug in DAGMan where held jobs that were removed would cause a
  warning about the internal count of held job procs being incorrect.
  :jira:`2102`

- Fixed a bug in *condor_transfer_data* where using the ``-addr``
  flag would automatically apply the ``-all`` flag to transfer
  all job data back making the use of ``-addr`` with a Job ID
  constraint fail.
  :jira:`2105`

- Fixed warnings about use of deprecated HTCondor Python binding methods
  in the `htcondor dag submit` command.
  :jira:`2104`

- Fixed several small bugs with Trust On First Use (TOFU) for SSL
  authentication.
  Added configuration parameter
  :macro:`BOOTSTRAP_SSL_SERVER_TRUST_PROMPT_USER`, which can be used to
  prevent tools from prompting the user about trusting the server's
  SSL certificate.
  :jira:`2080`

- Fixed bug in the *condor_userlog* tool where it would crash
  when reading logs with parallel universe jobs in it.
  :jira:`2099`

.. _lts-version-history-2301:

Version 23.0.1
--------------

Release Notes:

- HTCondor version 23.0.1 released on October 31, 2023.

- We added a HTCondor Python wheel for Python 3.12 on PyPI.
  :jira:`2117`

- The HTCondor tarballs now contain apptainer version 1.2.4.
  :jira:`2111`

New Features:

- None.

Bugs Fixed:

- Fixed a bug introduced in HTCondor 10.6.0 that prevented USE_PID_NAMESPACES from working.
  :jira:`2088`

- Fix a bug where HTCondor fails to install on Debian and Ubuntu platforms when the ``condor``
  user is present and the ``/var/lib/condor`` directory is not.
  :jira:`2074`

- Fixed a bug where execution times reported for ARC CE jobs were
  inflated by a factor of 60.
  :jira:`2068`

- Fixed a bug in DAGMan where ``Service`` nodes that failed caused the DAGMan process to fail
  an assertion check and crash.
  :jira:`2051`

- The job attributes ``CpusProvisioned``, ``DiskProvisioned``, and
  ``MemoryProvisioned`` are now updated for Condor-C and Job Router jobs.
  :jira:`2069`

- Updated HTCondor Windows binaries that are statically linked to the curl library to use curl version 8.4.0.
  The update was due to a report of a vulnerability, CVE-2023-38545, which affects earlier versions of curl.
  :jira:`2084`

- Fixed a bug on Windows where jobs would be inappropriately put on hold with an out of memory
  error if they returned an exit code with high bits set
  :jira:`2061`

- Fixed a bug where jobs put on hold by the shadow were not writing their ad to the
  job epoch history file.
  :jira:`2060`

- Fixed a rare race condition where *condor_rm*'ing a parallel universe job would not remove
  the job if the rm happened after the job was matched but before it fully started
  :jira:`2070`

.. _lts-version-history-2300:

Version 23.0.0
--------------

Release Notes:

- HTCondor version 23.0.0 released on September 29, 2023.

New Features:

- A *condor_startd* without any slot types defined will now default to a single partitionable slot rather
  than a number of static slots equal to the number of cores as it was in previous versions.
  The configuration template ``use FEATURE : StaticSlots`` was added for admins wanting the old behavior.
  :jira:`2026`

- The ``TargetType`` attribute is no longer a required attribute in most Classads.  It is still used for
  queries to the *condor_collector* and it remains in the Job ClassAd and the Machine ClassAd because
  of older versions of HTCondor require it to be present.
  :jira:`1997`

- The ``-dry-run`` option of *condor_submit* will now print the output of a ``SEC_CREDENTIAL_STORER`` script.
  This can be useful when developing such a script.
  :jira:`2014`

- Added ability to query epoch history records from the Python bindings.
  :jira:`2036`

- The default value of :macro:`SEC_DEFAULT_AUTHENTICATION_METHODS` will now be visible
  in *condor_config_val*. The default for :macro:`SEC_*_AUTHENTICATION_METHODS`
  will inherit from this value, and thus no ``READ`` and ``CLIENT`` will no longer
  automatically have ``CLAIMTOBE``.
  :jira:`2047`

- Added new tool *condor_test_token*, which will create a SciToken
  with configurable contents (including issuer) which will be accepted
  for a short period of time by the local HTCondor daemons.
  :jira:`1115`

Bugs Fixed:

- Fixed a bug that would cause the *condor_startd* to crash in rare cases
  when jobs go on hold
  :jira:`2016`

- Fixed a bug where if a user-level checkpoint could not be transferred from
  the starter to the AP, the job would go on hold.  Now it will retry, or
  go back to idle.
  :jira:`2034`

- Fixed a bug where the *CommittedTime* attribute was not set correctly
  for Docker Universe jobs doing user level check-pointing.
  :jira:`2014`

- Fixed a bug where *condor_preen* was deleting files named '*OfflineAds*'
  in the spool directory.
  :jira:`2019`

- Fixed a bug where the *blahpd* would incorrectly believe that an LSF
  batch scheduler was not working.
  :jira:`2003`

- Fixed the Execution Point's detection of whether libvirt is working
  properly for the VM universe.
  :jira:`2009`

- Fixed a bug where container universe did not work for late materialization jobs
  submitted to the *condor_schedd*
  :jira:`2031`

- Fixed a bug where the *condor_startd* could crash if a new match is
  made at the end a drain request.
  :jira:`2032`

