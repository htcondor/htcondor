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

.. _lts-version-history-23014:

Version 23.0.14
---------------

Release Notes:

.. HTCondor version 23.0.14 released on Month Date, 2024.

- HTCondor version 23.0.14 not yet released.

New Features:

- None.

Bugs Fixed:

- Fixed a couple bugs in when credentials managed by the
  *condor_credd* are cleaned up. In some situations, credentials would
  be removed while jobs requiring them were queued or even running,
  resulting in the jobs being held.
  :jira:`2467`

- Fixed a bug where an illformed scitoken could crash a *condor_schedd*.
  :jira:`2504`

- Fixed a bug where resource claiming would fail if the *condor_schedd*
  had :macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION` enabled and the
  *condor_startd* had it disabled.
  :jira:`2484`

.. _lts-version-history-23012:

Version 23.0.12
---------------

Release Notes:

- HTCondor version 23.0.12 released on June 13, 2024.

New Features:

- *condor_history* will now pass along the ``-forwards`` and ``-scanlimit``
  flags when doing a remote history query.
  :jira:`2448`

Bugs Fixed:

- When submitting to a remote batch scheduler via ssh, improve error
  handling when the initial ssh connection failures and a subsequent
  attempt succeeds.
  Before, transfers of job sandboxes would fail after such an error.
  :jira:`2398`

- Fixed a bug where the *condor_procd* could crash on Windows EPs
  using the default Desktop policy.
  :jira:`2444`

- Fixed bug where *condor_submit_dag* would crash when DAG file contained
  a line of only whitespace with no terminal newline.
  :jira:`2463`

- Fixed a bug that prevented the *condor_startd* from advertising
  ``DockerCachedImageSizeMb``
  :jira:`2458`

- Fixed a rare bug where certain errors reported by a file transfer
  plugin were not reported to the *condor_starter*.
  :jira:`2464`

- Removed confusing message in StartLog at shutdown about trying to
  kill illegal pid.
  :jira:`1012`

- Container universe now works when file transfer is disabled or not used.
  :jira:`1329`

- Fixed a bug where transfer of Kerberos credentials from the
  *condor_shadow* to the *condor_starter* would fail if the daemons
  weren't explicitly configured to trust each other.
  :jira:`2411`

.. _lts-version-history-23010:

Version 23.0.10
---------------

Release Notes:

- HTCondor version 23.0.10 released on May 9, 2024.

- Preliminary support for Ubuntu 22.04 (Noble Numbat).
  :jira:`2407`

- In the tarballs, the *apptainer* executable has been moved to the ``usr/libexec`` directory.
  :jira:`2397`

New Features:

- Updated *condor_upgrade_check* to warn about the deprecated functionality of having
  multiple queue statements in a single submit description file.
  :jira:`2338`

- Updated *condor_upgrade_check* to verify that :macro:`SEC_TOKEN_SYSTEM_DIRECTORY` and
  all stored tokens have the correct ownership and file permissions.
  :jira:`2372`

Bugs Fixed:

- Fixed bug where the ``HoldReasonSubcode`` was not the documented value
  for jobs put on hold because of errors running a file transfer plugin.
  :jira:`2373`

- Fixed a crash when using the *condor_upgrade_check* tool when using
  a python version older than **3.8**. This bug was introduced in V23.0.4.
  :jira:`2393`

- Fixed a very rare bug where on a busy AP, the shadow might send a KILL signal
  to a random, non-HTCondor process, if process IDs are reused quickly.
  :jira:`2357`

- The scitoken credmon "ver" entry is now properly named "scitoken:2.0".  It was formerly
  named "scitokens:2.0" (note plural).  The reference python scitoken implementation
  uses the singular.  The C++ scitokens implementation incorrectly used the plural up to
  version 0.6.0.  The old name can be restored with the config knob 
  :macro:`LOCAL_CREDMON_TOKEN_VERSION` to scitokens:2.0
  :jira:`2285`

- Fixed a bug where DAGMan would crash when directly submitting a node job
  with a queue for each statement that was provided less item data values
  in a row than declared custom variables.
  :jira:`2351`

- Fixed a bug where an error message from the *condor_starter* could
  create job event log entries with newlines in them, which broke the
  event log parser.
  :jira:`2343`

- Fixed a bug in the ``-better-analyze`` option of *condor_q* that could result
  in ``[-1]`` and no expression text being displayed for some analysis steps.
  :jira:`2355`

- Fixed a bug where a bad DN value was used during SSL authentication
  when the client didn't present a credential.
  :jira:`2396`

.. _lts-version-history-2308:

Version 23.0.8
--------------

Release Notes:

- HTCondor version 23.0.8 released on April 11, 2024.

New Features:

- None.

Bugs Fixed:

- Fixed a bug that caused **ssh-agent** processes to be leaked when
  using *grid* universe remote batch job submission over SSH.
  :jira:`2286`

- Fixed a bug where DAGMan would crash when the provisioner node was
  given a parent node.
  :jira:`2291`

- Fixed a bug that prevented the use of ``ftp:`` URLs in the file
  transfer plugin.
  :jira:`2273`

- Fixed a bug where a job that's matched to an offline slot ad remains
  idle forever.
  :jira:`2304`

- Fixed a bug where the *condor_shadow* would not write a job
  termination event to the job log for a completed job if the
  *condor_shadow* failed to reconnect to the *condor_starter* prior
  to completing cleanup. This would result in DAGMan workflows being
  stuck waiting forever for jobs to finish.
  :jira:`2292`

- Fixed bug where the Shadow failed to write its job ad to :macro:`JOB_EPOCH_HISTORY`
  when it failed to reconnect to the Starter.
  :jira:`2289`

- Fixed a bug in the Windows MSI installer that would cause installation to fail
  when the install path had a space in the path name, such as when installing to
  ``C:\Program Files``
  :jira:`2302`

- Fixed a bug where the :macro:`USER_JOB_WRAPPER` was allowed to create job
  event log information events with newlines in them, which broke the event
  log parser.
  :jira:`2305`

- Fixed ``SyntaxWarning`` raised by Python 3.12 in **condor_adstash**.
  :jira:`2312`

- Improved use of Vault for job credentials. Reject some invalid use
  cases and avoid redundant work with frequent job submission.
  :jira:`2038`
  :jira:`2232`

- Fixed an issue where HTCondor could not be installed on Debian or Ubuntu
  platforms if there was more that one ``condor`` user in LDAP.
  :jira:`2306`

Version 23.0.6
--------------

Release Notes:

- HTCondor version 23.0.6 released on March 14, 2024.

New Features:

- Speed up starting of daemons on Linux systems configured with
  very large number of file descriptors.
  :jira:`2270`

Bugs Fixed:

- Fixed bug in DAGMan where nodes that had retries would incorrectly
  set its descendants to the Futile state if the node job got removed.
  :jira:`2240`

- Fixed bug in the event log reader that would rarely cause DAGMan
  to lose track of a job, and wait forever for a job that had
  really finished, with DAGMan not realizing that said job had
  indeed finished.
  :jira:`2236`

- Fixed *condor_test_token* to access the SciTokens cache as the correct
  user when run as root.
  :jira:`2241`

- Fixed a bug that caused a crash if a configuration file or submit
  description file contained an empty multi-line value.
  :jira:`2249`

- Fixed a bug where a submit transform or a job router route could crash on a
  two argument transform statement that had missing arguments.
  :jira:`2280`

- Fixed error handing for the ``-format`` and ``-autoformat`` options of
  the *condor_qusers* tool when the argument to those options was not a valid
  expression.
  :jira:`2269`

- Fixed a bug where the **condor_collector** generated an invalid host
  certificate for itself on macOS.
  :jira:`2272`

.. _lts-version-history-2304:

Version 23.0.4
--------------

Release Notes:

- HTCondor version 23.0.4 released on February 8, 2024.

New Features:

- The **condor_starter** will now set the environment variable ``NVIDIA_VISIBLE_DEVICES`` either
  to ``none`` or to a list of the full uuid of each GPU device assigned to the slot.
  :jira:`2242`

- When the HTCondor Keyboard daemon (**condor_kbdd**) is installed, a
  configuration file is included to automatically enable user input monitoring.
  :jira:`2255`

- The **condor_starter** can now be configured to capture the stdout and stderr
  of file transfer plugins and write that output into the StarterLog.
  :jira:`1459`

- Updated **condor_upgrade_check** script for better support and
  maintainability. This update includes new flags/functionality
  and removal of old checks for upgrading between V9 and V10 of
  HTCondor.
  :jira:`2168`

Bugs Fixed:

- Fixed a bug in the HTCondor Keyboard daemon where activity detected by the
  X Screen Saver extension was ignored.
  :jira:`2255`

- Search engine timeout settings for **condor_adstash** now apply to all search
  engine operations, not just the initial request to the search engine.
  :jira:`2167`

- Ensure Perl dependencies are present for the **condor_gather_info** script.
  The **condor_gather_info** script now properly reports the User login name.
  Also, report the contents of ``/etc/os-release```.
  :jira:`2094`

- The submit language will no longer treat ``request_gpu_memory`` and ``request_gpus_memory``
  as requests for a custom resource of type ``gpu_memory`` or ``gpus_memory`` respectively.
  :jira:`2201`

- Fixed bug where DAG node jobs declared inline inside a DAG file
  would fail to set the Job ClassAd attribute ``JobSubmitMethod``.
  :jira:`2184`

- Fixed ``SyntaxWarning`` raised by Python 3.12 in scripts packaged
  with the Python bindings.
  :jira:`2212`

.. _lts-version-history-2303:

Version 23.0.3
--------------

Release Notes:

- HTCondor version 23.0.3 released on January 4, 2024.

- Preliminary support for openSUSE LEAP 15.
  :jira:`2156`

New Features:

- Improve ``htcondor job status`` command to display information about
  a jobs goodput.
  :jira:`1982`

- Added ``ROOT_MAX_THREADS`` to :macro:`STARTER_NUM_THREADS_ENV_VARS` default value.
  :jira:`2137`

Bugs Fixed:

- The file transfer plugin documents that an exit code of 0
  is success, 1 is failure, and 2 is reserved for future work to
  handle the need to refresh credentials.  The definition has now
  changed so that any non-zero exit codes are treated as an error
  putting the job on hold.
  :jira:`2205`

- Fixed a bug where any file I/O error (such as disk full) was
  ignored by the *condor_starter* when writing the ClassAd file
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

- Fixed a bug where the Python-based HTChirp client had its max line length set
  much shorter than is allowed by the HTCondor Chirp server. The client now
  also throws a relevant error when this max limit is hit while sending commands
  to the server.
  :jira:`2142`

- Linux jobs with a invalid ``#!`` interpreter now get a better error
  message when the Execution Point is running as root.  This was enhanced in 10.0,
  but a bug prevented the enhancement from fully working on a system
  installed Execution Point.
  :jira:`1698`

- Fixed a bug where the DAGMan job proper for a DAG with a final
  node could stay stuck in the removed job state.
  :jira:`2147`

- Correctly identify ``GPUsAverageUsage`` and ``GPUsMemoryUsage`` as floating point
  values for *condor_adstash*.
  :jira:`2170`

- Fixed a bug where *condor_adstash* would get wedged due to a logging failure.
  :jira:`2166`

- Updated the usage and man page of the *condor_drain* tool to include information
  about the ``-reconfig-on-completion`` option.
  :jira:`2164`

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

