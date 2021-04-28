Development Release Series 8.9
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 8.9.13
--------------

Release Notes:

- HTCondor version 8.9.13 released on March 30, 2021.

- We fixed a bug in how the IDTOKENS authentication method reads its
  signing keys.  You can use the new `condor_check_password` tool to
  determine if your signing keys were truncated by this bug.  If they
  were, we recommend that you reissue all tokens signed by the affected
  key(s).  The `condor_check_password` tool may also be used to truncate
  the affected key(s) before the first byte causing the problem, which
  will allow existing tokens to continue to work, but is, perhaps
  substantially, less cryptographically secure.  For more information,
  see https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=UpgradingToEightNineThirteen.
  :jira:`295`
  :jira:`369`

- We have changed the default configuration file.  It no longer sets
  ``use security : host_based``.  This may cause your existing, insecure
  security configuration to stop working.  Consider updating your
  configuration and see the next item.
  :jira:`301`

- We have added a new configuration file (``config.d/00-htcondor-9.0.config``)
  to the packaging.  It won't be replaced by future upgrades, but it sets a
  new default for security, ``use security : recommended_v9_0``, whose
  contents could be updated in future point releases.  You may safely delete
  this file if you're upgrading.  This file has an extensive explanatory
  comment, which includes the work-around for the previous item's problem.
  :jira:`339`

- HTCondor will now access tokens in directory ``/etc/condor/tokens.d`` as
  user root, meaning this directory and its contents should be only accessible
  by user root for maximum security.  If upgrading from an earlier v8.9.x release,
  it may currently be accessible by user ``condor``, so recommend that
  admins issue command ``chown -R root:root /etc/condor/tokens.d``.
  :jira:`266`

- As an improved security measure, HTCondor can now prohibit Linux jobs
  from running setuid executables by default.  We believe the only common setuid
  program this impacts is ``ping``.  To prohibit jobs from running setuid binaries,
  set ``DISABLE_SETUID`` to ``true`` in the configuration of the worker
  node.
  :jira:`256`

- SCITOKENS is now in the default list of authentication methods.
  :jira:`47`

- Added configuration parameter ``LEGACY_ALLOW_SEMANTICS``, which re-enables
  the behavior of HTCondor 8.8 and prior when ``ALLOW_DAEMON`` or
  ``DENY_DAEMON`` is not defined.

  This parameter is intended to ease the transition of existing HTCondor
  configurations from 8.8 to 9.0, and should not be used long-term or in
  new installations.
  :jira:`263`

- On Windows, the *condor_master* now prefers TCP over UDP when forwarding
  ``condor_reconfig`` and ``condor_off`` commands to other daemons.
  :jira:`273`

- The `condor_credmon_oauth` now writes logs to the path in
  ``CREDMON_OAUTH_LOG`` if defined, which matches the typical log config
  parameter naming pattern, instead of using the path defined in
  ``SEC_CREDENTIAL_MONITOR_OAUTH_LOG``.
  :jira:`122`

Known Issues:

- The MUNGE security method is not working at this time.

New Features:

- By default, all HTCondor communications and HTCondor file transfers
  are protected by hardware-accelerated integrity and AES encryption.
  HTCondor automatically falls back to Triple-DES or Blowfish when
  inter-operating with a previous version.
  :jira:`222`

- HTCondor now detects instances of multi-instance GPUs.  These devices will
  be reported as individual GPUs, each named as required to access them via
  ``CUDA_VISIBLE_DEVICES``.  Some of the ``-extra`` and ``-dynamic``
  properties are unavailable for these devices.  Due to driver limitations,
  if MIG is enabled on any GPU, HTCondor will detect all devices on the system
  using NVML; some ``-extra`` properties will not be reported for these
  systems as a result.

  This feature does not include reporting the utilization of MIG instances.
  :jira:`137`

- If ``SCITOKENS_FILE`` is not specified, HTCondor will now follow WLCG's
  bearer token discovery protocol to find it.  See
  https://zenodo.org/record/3937438 for details.
  :jira:`92`

- Improvements made to error messages when jobs go on hold due to
  timeouts transferring files via HTTP.
  :jira:`355`

- HTCondor now creates a number of directories on start-up, rather than
  fail later on when it needs them to exist.  See the ticket for details.
  :jira:`73`

- The default value for the knob DISABLE_SETUID is now false, so that
  condor_ssh_to_job works on machines with SELinux enabled.
  :jira:`358`

- HTCondor daemons that read the configuration files as root when they start
  up will now also read the configuration files as root when they are reconfigured.
  :jira:`314`

- To make the debugging logs more consistent, the slot name is always
  appended to the StarterLog.  Previously, a single slot startd's
  StarterLog would have no suffix.  Now it will be called StarterLog.slot1.
  :jira:`178`

- Added command-line options to *condor_gpu_discovery* to report GPUs
  multiple times.  If your GPU jobs are small and known to be well-behaved,
  this makes it easier for them to share a GPU.
  :jira:`106`

- Enhanced the optional job completion sent to the submitter to now
  include the batch name, if defined, and the submitting directory,
  so the user has a better idea of which job the job id is.
  :jira:`71`

- Added submit commands ``batch_project`` and ``batch_runtime``, used to
  set job attributes ``BatchProject`` and ``BatchRuntime`` for **batch**
  grid universe jobs.
  :jira:`44`

- Added a new tool/daemon :ref:`condor_adstash` that polls for job history
  ClassAds and pushes their contents to Elasticsearch. Setting ``use
  feature: adstash`` will run *condor_adstash* as a daemon, though
  care should be taken to configure it for your pool and Elasticsearch
  instance. See the :ref:`admin-manual/monitoring:Elasticsearch`
  documentation in the admin manual for more detail.

- When token authentication (IDTOKENS or SCITOKENS) is used, HTCondor will
  now record the subject, issuer, scopes, and groups, from the token used to
  submit jobs.
  :jira:`90`

- The python ``schedd.submit`` method now honors the **spool** argument
  even when the first argument is a ``Submit`` object.
  :jira:`131`

- When singularity is enabled, when there is an error running singularity
  test before the job, the first line of singularity stderr is logged to
  the hold message in the job.
  :jira:`133`

- When the startd initializes, it runs the ``condor_starter`` with the
  -classad option to probe the features this starter support.  As a
  side-effect, the starter logs some information to a StarterLog file.
  This StarterLog is almost never of interest when debugging jobs. To
  make that more clear, this starter log is now named StarterLog.testing.
  :jira:`132`

- The *condor_collector* can now use a projection when forwarding ads to a
  View Collector.  A new configuration variable ``COLLECTOR_FORWARD_PROJECTION``
  can be configured to enabled this.
  :jira:`51`

- The *condor_drain* command now has a ``-reason`` argument and will supply a default
  reason value if it is not used.  The *condor_defrag* daemon will always pass ``defrag``
  as the reason so that draining initiated by the administrator can be distinguished
  by draining initiated by *condor_defrag*.
  :jira:`77`

- The  *condor_defrag* daemon will now supply a ``-reason`` argument of ``defrag``
  and will ignore machines that have have a draining reason that is not ``defrag``.
  :jira:`89`

- Added a new a ClassAd function to help write submit transforms.  You can now use unresolved()
  to check for existing constraints on a particular attribute (or attribute regex).
  :jira:`66`

- Added TensorFlow environment variables ``TF_NUM_THREADS`` and
  ``TF_LOOP_PARALLEL_ITERATIONS`` to the list of environment variables
  exported by the *condor_starter* per these
  `recommendations <https://github.com/theislab/diffxpy/blob/master/docs/parallelization.rst>`_.
  :jira:`185`

- Certificate map files can now use the ``@include`` directive to include another file
  or all of the files in a directory.
  :jira:`46`

- The ClassAd returned by Python binding ``htcondor.SecMan.ping()`` now
  includes extra information about the daemon's X.509 certificate if
  SSL or GSI authentication was used.
  :jira:`43`

- Added configuration parameter ``GRIDMANAGER_LOG_APPEND_SELECTION_EXPR``,
  which allows each *condor_gridmanager* process to write to a separate
  daemon log file.
  When this parameter is set to ``True``, the evaluated value of
  ``GRIDMANAGER_SELECTION_EXPR`` (if set) will be appended to the
  filename specified by ``GRIDMANAGER_LOG``.
  :jira:`102`

- Added command-line argument ``-jsonl`` to *condor_history*.
  This will print the output in JSON Lines format (one ClassAd per line
  in JSON format).
  :jira:`35`

- Running ``condor_ping`` with no arguments now defaults to doing a ping
  against the *condor_schedd* at authorization level `WRITE`.
  :ticket:`7892`
  :jira:`246`

- Adjusted configuration defaults for *condor_c-gahp* so that a restart
  of the remote *condor_collector* or *condor_schedd*  doesn't result in
  a prolonged interruption of communication.
  Previously, communication could be interrupted for up to a day.
  :jira:`313`

Bugs Fixed:

- Fixed a bug where daemons would leak memory whenever sending updates
  to the collector, at the rate of a few megabytes per day.  This leak
  was introduced earlier in the HTCondor v8.9.x series.
  :jira:`323`

- Malformed or missing SciToken can result in a schedd abort when using ``SCHEDD_AUDIT_LOG``.
  :jira:`315`

- Fixed a problem where ``condor_watch_q`` would crash when updating totals for DAGman jobs.
  :jira:`201`

- Sufficiently old versions of the CUDA libraries no longer cause
  `condor_gpu_discovery` to segfault.
  :jira:`343`

- Fixed a bug introduced in version 8.9.12, where non-root clients could not
  use IDTOKENS.
  :jira:`370`

- Fixed a bug where an IDTOKEN could be sent to a user who had authenticated
  with the ANONYMOUS method after the auto-approval period had expired.
  :jira:`231`

- HTCondor daemons used to access tokens in ``/etc/condor/tokens.d`` as user ``condor``, now
  instead tokens are accessed as user ``root``.
  :jira:`266`

- Fixed a bug where jobs that asked for ``transfer_output_files = .`` would
  be put on hold if they were evicted and restarted.
  :jira:`267`

- The ``preserve_relative_paths`` submit command now properly allows jobs
  to run on HTCondor versions 8.9.10 and later.
  :jira:`189`

- Utilization is now properly reported if ``GPU_DISCOVERY_EXTRA`` includes
  ``-uuid``.
  :jira:`137`

- Fixed a bug with singularity support where the job's cwd wasn't
  being set to the scratch directory when `SINGULARITY_TARGET_DIR` wasn't
  also set.
  :jira:`91`

- The tool ``condor_store_cred`` will now accept and use a handle for an OAuth
  cred, and the *condor_credd* will now honor the handle in the stored filename.
  :jira:`291`

- Condor-C (grid universe type **condor**) now works correctly when jobs
  use different SciTokens.
  :jira:`99`

- Fixed a bug where file transfers using HTTP may fail unnecessarily after
  30 seconds if the HTTP server does not include a Content-Length header.
  :jira:`354`

- The local issuer in the ``condor_credmon_oauth`` gives more useful
  log output if it detects that the private key was generated with
  something other than the expected EC algorithm.
  :jira:`305`

- Fixed a bug with *condor_dagman* direct job submission where certain submit
  errors were not getting reported in the debug output.
  :jira:`85`

- The minihtcondor DEB package no longer aborts if installed on
  systems that do not have systemd installed, as is common with Debian and Ubuntu
  docker containers.
  :jira:`362`

- The ``condor_procd`` now detects and compensates for incomplete reads of
  the ``/proc`` file system.
  :jira:`33`

Version 8.9.12
--------------

Release Notes:

- HTCondor version 8.9.12 was released on March 25, 2021, and pulled the
  next day for significant incompatibilities with previous releases.

New Features:

- None.

Bugs Fixed:

- None.


Version 8.9.11
--------------

Release Notes:

- HTCondor version 8.8.11 released on January 27, 2021.

New Features:

- None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes security-related bugs
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0001.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0001.html>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0002.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0002.html>`_.

   :ticket:`7893`
   :ticket:`7894`

Version 8.9.10
--------------

Release Notes:

- HTCondor version 8.9.10 released on November 24, 2020.

- For *condor_annex* users: Amazon Web Services is deprecating support for
  the Python 2.7 runtime used by *condor_annex*.  If you ran the
  *condor_annex* setup command with a previous version of HTCondor, you
  should update your setup to use the new runtime.  (Go to the AWS Lambda
  `console <https://console.aws.amazon.com/lambda>`_ and look for the
  ``HTCondorAnnex-CheckConnectivity`` function; click on it.  Scroll
  down to "Runtime settings"; click the "Edit" button.  Select "Python 3.8"
  from the drop-down list under "Runtime".  Then hit the "Save" button.
  You'll have to repeat this for each region you're using.)
  :jira:`24`

New Features:

- Added support for OAuth, SciTokens, and Kerberos credentials in local
  universe jobs.
  :ticket:`7693`

- The python ``schedd.submit`` method now accepts a ``Submit`` object and itemdata
  to define the jobs, to be submitted.  The use of a ClassAd to define the job is now deprecated
  for this method
  :ticket:`7853`

- A new Python method ``schedd.edit`` can be used to set multiple attributes for a job specification
  with a single call to this method.
  :jira:`28`

- Added a new ``SCRIPT HOLD`` feature to DAGMan, allowing users to define a
  script executable that runs when a job goes on hold.
  :jira:`65`

- Added a new ``SUBMIT-DESCRIPTION`` command to DAGMan, which allows inline
  jobs to share submit descriptions.
  :jira:`64`

- You may now tag instances from the command line of `condor_annex`.  Use
  the ``-tag <name> <value>`` command-line option once for each tag.
  :ticket:`7834`

- When running a singularity job, the starter first runs `singularity test`
  if this returns non-zero, the job is put on hold.
  :ticket:`7801`

- Added support for requesting GPUs with grid universe jobs of type `batch`.
  :ticket:`7757`

- Added new configuration variable :macro:`MIN_FLOCK_LEVEL`, which can be
  used to specify how many of the remote HTCondor pools listed in
  ``FLOCK_COLLECTOR_HOSTS`` should always be flocked to.
  The default is 0.
  :jira:`62`

- Job attributes set by the job using the Chirp command
  ``set_job_attr_delayed`` are now propagated back to the originating
  *condor_schedd* by the Job Router and Condor-C (a.k.a grid universe type
  ``condor``).
  :jira:`63`

- A new configuration variable :macro:`DEFAULT_DRAINING_START_EXPR` can be used to define
  what the ``START`` value of a slot should be while it is draining. This configuration variable
  is used when the command to drain does not have an override value for ``START``.
  :jira:`67`

- When a :macro:`SEC_CREDENTIAL_PRODUCER` is configured for *condor_submit* it now
  assumes that the CREDD is the current version when does not know what version it is,
  which is common when the CREDD is running on a different machine than *condor_submit*.
  :jira:`76`

- The ``--add`` option of *bosco_cluster* now attempts to install a version
  of HTCondor on the remote cluster that closely matches the version installed
  locally.
  The new ``--url`` option can be used to specify the URL from which the
  HTCondor binaries should be fetched.
  :jira:`21`

- The Python scripts distributed with HTCondor (except those dealing
  with the OAuth credmon) have been upgraded to run under Python 3.
  :ticket:`7698`
  :ticket:`7844`
  :ticket:`7872`

- Added the ability to have finer grain control over the SSH connection when
  using the remote gahp. One can now specify the SSH port and also
  whether or not SSH BatchMode is used.
  :jira:`18`
  :jira:`19`

-  The *condor_useprio* tool now displays any submitter ceilings that are set.
   :ticket:`7837`

- Added statistics to the collector ad about CCB.
  :ticket:`7842`

Bugs Fixed:

- Fixed a bug introduced in 8.9.9 that, only when accounting groups with quotas
  were defined that caused the matchmaker to stop making new matches after several
  negotiation cycles.
  :jira:`83`

- The *condor_credd* now signals the OAuth credmon, not the Kerberos credmon,
  when processing a locally-issued credential.
  :ticket:`7889`

- Fixed a bug in DAGMan where a ``_gotEvents`` warning kept appearing
  incorrectly in the output file.
  :jira:`15`
  
- Fixed a bug which caused the ``condor-annex-ec2`` script to exit prematurely
  on some systemd platforms.
  :jira:`22`

- Fixed a bug specific to MacOS X which could cause the shared port daemon's
  initial childalive message to be lost.  This would cause `condor_who` to
  wrongly think that HTCondor hadn't started up until the shared port daemon
  sent its second childalive message.
  :ticket:`7866`

Version 8.9.9
-------------

Known Issues:

- If group quotas are in use, the negotiator will eventually stop making
  matches. This defect was introduced in HTCondor 8.9.9. It will be fixed in
  HTCondor 8.9.10 to be released on November 24, 2020.
  In the meantime, one may revert the Central Manager machine to HTCondor
  8.9.8, leaving the remainder of the pool at HTCondor 8.9.9.

Release Notes:

-  HTCondor version 8.9.9 released on October 26, 2020.

-  The RPMs have been restructured to require additional packages from EPEL.
   In addition to the boost libraries, the RPMs depend on the Globus, munge,
   SciTokens, and VOMS libraries in EPEL.
   :ticket:`7681`

-  When the *condor_startd* is running as root on a Linux machine,
   unless CGROUP_MEMORY_LIMIT_POLICY is ``none``, HTCondor now always
   sets both the soft and hard cgroup memory limit for a job. When
   CGROUP_MEMORY_LIMIT_POLICY is ``soft``, the soft limit is set to the
   slot size, and the hard limit is set to the TotalMemory of the whole
   startd.  When CGROUP_MEMORY_LIMIT_POLICY is ``hard``, the hard limit
   is set to the slot size, and the soft limit is set 90% lower.
   Also added knob DISABLE_SWAP_FOR_JOB, which when set to ``true``, 
   prevents the job from using any swap space. This knob defaults to ``false``.
   :ticket:`7882`

- When running on a Linux system with cgroups enabled, the ``MemoryUsage``
  attribute of a job no longer includes the memory used by the kernel disk
  cache.
  :ticket:`7882`

-  We deprecated the exceptions raised by the
   :ref:`apis/python-bindings/index:Python Bindings`.  The new
   exceptions all inherit from :class:`~htcondor.HTCondorException` or
   :class:`~classad.ClassAdException`, according to the originating module.  For
   backwards-compatibility, the new exceptions all also inherit the class
   of each exception type they replaced.
   :ticket:`6935`

-  We changed the default value of ``PROCD_ADDRESS`` on Windows to make it
   less likely for multiple instances of HTCondor on the machine to collide.
   :ticket:`7789`

-  The *condor_schedd* will no longer modify a job's ``User`` attribute when the job's
   ``NiceUser`` attribute is set.  The ``nice_user`` submit keyword is now implemented
   entirely by *condor_submit*.   Because of this change the ``nice_user`` mechanism
   will only work when *condor_submit* and the *condor_schedd* are both version 8.9.9 or later.
   :ticket:`7783`

New Features:

-  You may now instruct HTCondor to record certain information about the
   files present in the top level of a job's sandbox and the job's environment
   variables.  The list of files is recorded when transfer-in completes
   and again when transfer-out starts.  Set ``manifest`` to ``true`` in your
   submit file to enable, or ``manifest_dir`` to specify where the lists
   are recorded.  See the :ref:`man-pages/condor_submit:*condor_submit*`
   man page for details.
   :ticket:`7381`

   This feature is not presently available on Windows.

- DAGMan now waits for ``PROVISIONER`` nodes to reach a ready status before 
  submitting any other jobs.
  :ticket:`7610`

- Added a ``-Dot`` argument to *condor_dagman* which tells DAGMan to simply
  output a .dot file graphic representation of the dag, then exit immediately
  without submitting any jobs.
  :ticket:`7796`

- Set a variety of defaults into *condor_dagman* so it can now easily be
  invoked directly from the command line using ``condor_dagman mydag.dag``
  :ticket:`7806`

- Singularity jobs now ignore bind mount directories if the source
  directory for the bind mount does not exist on the host machine
  :ticket:`7807`

- Singularity jobs now ignore bind mount directories if the target
  directory for the bind mount does not exist in the image and
  SINGULARITY_IGNORE_MISSING_BIND_TARGET is set to ``true``
  (default is ``false``).
  :ticket:`7846`

- Improved startup time of the daemons.
  :ticket:`7799`

-  Added a machine-ad attribute, ``LastDrainStopTime``, which records the last
   time a drain command was cancelled.  Added two attributes to the defrag
   daemon's ad, ``RecentCancelsList`` and ``RecentDrainsList``, which record
   information about the last ten cancel or drain commands, respectively,
   that the defrag daemon sent.
   :ticket:`7732`

-  The accounting group that the ``nice_user`` submit command puts jobs into is now
   configurable by setting ``NICE_USER_ACCOUNTING_GROUP_NAME`` in the configuration
   of *condor_submit*.
   :ticket:`7792`

- Python 3 bindings are now available on macOS. They are linked against
  Python 3.8 provided by python.org.
  :ticket:`7090`

-  Added `oauth-services` method to the python-bindings :class:`~htcondor.Submit` class. 
   The python-bindings :class:`~htcondor.CredCheck` class can now be used to check if the
   OAuth services that a job needs are present before the job is submitted.
   :ticket:`7606`

-  The Python API daemon objects :class:`~htcondor.Schedd`, :class:`~htcondor.Startd`,
   :class:`~htcondor.Negotiator` and :class:`~htcondor.Credd` now have a location member
   whose value can be passed to the constructor of a class of the same type to create a new
   object pointing to the same HTCondor daemon.
   :ticket:`7670`

-  The Python API daemon object :class:`~htcondor.Schedd` constructor now accepts None
   and interprets that to be the address of the local HTCondor Schedd.
   :ticket:`7668`

-  The Python API now includes the job status enumeration.
   :ticket:`7726`

-  The Python API methods that take a constraint argument will now accept an :class:``~classad.ExprTree``
   in addition to the native Python types, string, bool, int and None.
   :ticket:`7657`

- Updated the ``htcondor.Submit.from_dag()`` Python binding to support the
  full range of command-line arguments available to *condor_submit_dag*.
  :ticket:`7823`

- Added the :mod:`htcondor.personal` module to the Python bindings. Its primary
  feature is the :class:`htcondor.personal.PersonalPool` class, which is
  responsible for managing the life-cycle of a "personal" single-machine
  HTCondor pool. A personal pool can (for example) be used for testing and
  development of HTCondor workflows before deploying to a larger pool.
  Personal pools do not require administrator/root privileges.
  HTCondor itself must still be installed on your system.
  :ticket:`7745`

- Added a family of version comparison functions to ClassAds.
  :ticket:`7504`

- Added the OAuth2 Credmon (aka "SciTokens Credmon") daemon
  (*condor_credmon_oauth*), WSGI application, helper libraries, example
  configuration, and documentation to HTCondor for Enterprise Linux 7
  platforms.
  :ticket:`7741`

- The *bosco_cluster* can optionally specify the remote installation directory.
  :ticket:`7843`

- HTCondor lets the administrator know when a SciToken mapping contains a
  trailing slash and optionally allow it to map. It is easy for an administrator
  to overlook the trailing slash when cutting a pasting from a browser.
  :ticket:`7557`

Bugs Fixed:

-  Fixed a bug that could cause the *condor_schedd* to abort if a SUBMIT_REQUIREMENT
   prevented a late materialization job from materializing.
   :ticket:`7874`

-  ``condor_annex -check-setup`` now respects the configuration setting
   ``ANNEX_DEFAULT_AWS_REGION``.  In addition, ``condor_annex -setup`` now
   sets ``ANNEX_DEFAULT_AWS_REGION`` if it hasn't already been set.  This
   makes first-time setup in a non-default region much less confusing.
   :ticket:`7832`

-  Fixed a bug introduced in 8.9.6 where enabling pid namespaces in the startd
   would make every job go on hold.
   :ticket:`7797`

-  *condor_watch_q* now correctly groups jobs submitted by DAGMan after
   *condor_watch_q* has started running.
   :ticket:`7800`

-  Fixed a bug in the ClassAd library where calling the ClassAd sum function
   on an empty list returned undefined.  It now returns 0.
   :ticket:`7838`

-  Fixed a bug in Docker Universe that caused a confusing warning message
   about an unaccessible file in /root/.docker 
   :ticket:`7805`

-  Fixed a bug in the *condor_collector* that caused it to handle queries
   from the *condor_negotiator* at normal priority instead of high priority.
   :ticket:`7729`

-  Fixed attribute ``ProportionalSetSizeKb`` to behave the same as
   ``ResidentSetSize`` in the slot ad.
   :ticket:`7787`

-  Removed the Java benchmark ``JavaMFlops`` from the machine ad.
   :ticket:`7795`

-  Read IDTOKENS used by daemons with the correct UID.
   :ticket:`7767`

-  Fixed the Python ``htcondor.Submit.from_dag()`` binding so it now throws an
   ``IOError`` exception when the specified .dag file is not found.
   :ticket:`7808`

-  Fixed a bug that would cause a job to go on hold with a memory usage
   exceeded message in the rare case where the usage could not be obtained.
   :ticket:`7886`

-  *condor_q* no longer prints misleading message about the matchmaker
   when asked to analyze a job.
   :ticket:`5834`

Version 8.9.8
-------------

Release Notes:

- HTCondor version 8.9.8 released on August 6, 2020.

- Fixed some issues with the *condor_schedd* validating attribute values and actions from
  *condor_qedit*. Certain edits could cause the *condor_schedd* to enter an invalid state
  and in some cases would required editing of the job queue to restore the *condor_schedd*
  to operation. While no security exploits are known to be possible, mischievous
  users could potentially disrupt the operation of the *condor_schedd*. A more detailed
  description and workaround for these issues can be found in the ticket.
  :ticket:`7784`

- The ``SHARED_PORT_PORT`` setting is now honored. If you are using
  a non-standard port on machines other than the Central Manager, this
  bug fix will a require configuration change in order to specify
  the non-standard port.
  :ticket:`7697`

-  API change in the Python bindings.  The :class:`classad.ExprTree` constructor
   now tries to parse the entire string passed to it.  Failure results in a
   :class:`SyntaxError`.  This prevents strings like ``"foo = bar"`` from silently
   being parsed as just ``foo`` and causing unexpected results.
   :ticket:`7607`

-  API change in the Python bindings.  The :class:`classad.ExprTree` constructor
   now accepts :class:`classad.ExprTree` (creating an identical copy)
   in addition to strings, making it easier to handle inputs uniformly.
   :ticket:`7654`

-  API change in the Python bindings: we deprecated ``Schedd.negotiate()``.
   :ticket:`7524`

-  API change in the Python bindings: we deprecated the classes
   ``htcondor.Negotiator``, ``htcondor.FileLock``, ``htcondor.EventIterator``,
   and ``htcondor.LogReader``,  as well as the functions ``htcondor.lock()``
   and ``htcondor.read_events()``.
   :ticket:`7690`

- API change in the Python bindings: the methods
  :meth:`htcondor.Schedd.query`,
  :meth:`htcondor.Schedd.xquery`, and
  :meth:`htcondor.Schedd.history`
  now use the argument names ``constraint`` and ``projection``
  (for the query condition and the attributes to return from the query)
  consistently.
  The old argument names (``requirements`` and ``attr_list``) are deprecated,
  but will still work (raising a :class:`FutureWarning` when used) until a future
  release.
  :ticket:`7630`

-  Removed the *condor_dagman* ``node_scheduler`` module, which contains
   earlier implementations of several DAGMan components and has not been used
   in a long time.
   :ticket:`7674`

New Features:

-  Added a new Python bindings sub-package, :mod:`htcondor.dags`, which contains
   tools for writing DAGMan input files programmatically using
   high-level abstractions over the basic DAGMan constructs.
   There is a new tutorial at :doc:`/apis/python-bindings/tutorials/index`
   walking through a basic use case.
   :mod:`htcondor.dags` is very new and its API has not fully stabilized;
   it is possible that there will be deprecations and breaking changes
   in the near future.
   Bug reports and feature requests greatly encouraged!
   :ticket:`7682`

-  Added a new Python bindings subpackage, :mod:`htcondor.htchirp`.
   This subpackage provides the :class:`HTChirp` and :func:`condor_chirp`
   objects for using the Chirp protocol inside a ``+WantIOProxy =
   true`` job.
   :ticket:`7330`

-  Added a new tool, *condor_watch_q*, a live-updating job status tracker
   that does not repeatedly query the *condor_schedd* like ``watch condor_q``
   would. It includes options for colored output, progress bars, and a minimal
   language for exiting when certain conditions are met.
   The man page can be found here: :ref:`condor_watch_q`.
   *condor_watch_q* is still under development;
   several known issues are summarized in the ticket.
   :ticket:`7343`

-  When the *condor_master* starts in background mode, which is the default,
   control is not returned until the background *condor_master* has created
   the MasterLog and is ready to accept commands.
   :ticket:`7667`

-  Added options ``-short-uuid`` and ``-uuid`` to the *condor_gpu_discovery*
   tool. These options use the NVIDIA uuid assigned to each GPU to produce
   stable identifiers for each GPU so that devices can be taken offline without
   causing confusion about which of the remaining devices a job is using.
   :ticket:`7696`

-  Configuration variables of the form :macro:`OFFLINE_MACHINE_RESOURCE_<TAG>` such as
   :macro:`OFFLINE_MACHINE_RESOURCE_GPUs` will now take effect on a *condor_reconfig*.
   :ticket:`7651`

-  HTCondor now supports setting an upper bound on the number of cores user can
   be given.  This is called the submitter ceiling. The ceiling can be set with
   the ``condor_userprio -setceiling`` command line option.
   :ticket:`7702`

-  The *condor_startd* now detects whether user namespaces can be created by
   unprivileged processes.  If so, it advertises the ClassAd attribute
   ``HasUserNamespaces``. In this case, container managers like
   singularity can be run without setuid root.
   :ticket:`7625`

-  Added a :macro:`SEC_CREDENTIAL_SWEEP_DELAY` configuration parameter which
   specifies how long, in seconds, we should wait before cleaning up unused
   credentials.
   :ticket:`7484`

-  *classad_eval* now allows its first (ClassAd) argument to be just the
   interior of a single ClassAd.  That is, you no longer need to surround
   the first argument with square brackets.  This means that
   ``classad_eval 'x = y; y = 7' 'x'`` will now correctly return ``7``.
   :ticket:`7621`

-  *classad_eval* now allows you to freely mix (partial) ClassAds,
   single attribute assignments, and the expressions you want to evaluate.
   This means that ``classad_eval 'x = y' 'y = 7' 'x'`` will now return
   ``7``.  The ad used to evaluate an expression will be printed before
   the expression's result, unless doing so would repeat the previous
   expression's ad; use the ``-quiet`` flag to disable.
   :ticket:`7341`

-  Improved the efficiency of process monitoring in macOS.
   :ticket:`7708`

-  The *condor_startd* now handles :macro:`STARTD_SLOT_ATTRS` after
   :macro:`STARTD_ATTRS` and :macro:`STARTD_PARTITIONABLE_SLOT_ATTRS`
   so that custom slot attributes describing the resources of
   dynamic children can be referred to by :macro:`STARTD_SLOT_ATTRS`
   :ticket:`7588`

-  Updated *condor_q* so when called with the ``-dag`` flag and a DAGMan job
   ID, it will display all jobs running under any nested sub-DAGs.
   :ticket:`7483`

-  Direct job submission in *condor_dagman* now reports warning messages related
   to job submission (for example, possible typos in submit arguments) to help
   debug problems with jobs not running correctly.
   :ticket:`7568`

-  *condor_dagman* now allows jobs to be described with an inline submit
   description, instead of referencing a separate submit file. See the
   :ref:`users-manual/dagman-workflows:inline submit descriptions` section for
   more details.
   :ticket:`7352`

-  Improved messaging for the *condor_drain* tool to indicate that it is only
   draining the single specified *condor_startd*. If the target host has 
   multiple *condor_startd* daemons running, the other instances will not be
   drained.
   :ticket:`7664`

-  Added new authentication method names ``FAMILY`` and ``MATCH``.
   These represent automated establishment of trust between daemons.
   They can not be used as values for configuration parameters such as
   :macro:`SEC_DEFAULT_AUTHENTICATION_METHODS`.
   ``FAMILY`` represents a security session between daemons within the same
   family of OS processes.
   ``MATCH`` represents a security session between daemons mediated through
   a central manager (*condor_collector* and *condor_negotiator*) that both
   daemons trust.
   These values will be most visible in the attribute
   ``AuthenticationMethod`` in ClassAds advertised in the *condor_collector*.
   :ticket:`7683`

- Added a new submit file option, ``docker_network_type = none``, which
  causes a docker universe job to not have any network connectivity.
  :ticket:`7701`

- Docker jobs now respect CPU Affinity.
  :ticket:`7627`

- Added a ``debug`` option to *bosco_cluster* to help diagnose ssh failures.
  :ticket:`7712`

- The *condor_submit* executable will not abort if the submitting user has a
  gid of 0.  Jobs still will not run with root privileges, but this allows jobs to
  be submitted which are assigned an ``Owner`` via the result of user mapping
  from authentication.
  :ticket:`7662`

- The *condor_store_cred* tool can now be used to manage different
  kinds of credentials, including Password, Kerberos, and OAuth.
  :ticket:`6868`

Bugs Fixed:

- Fixed a segmentation fault in the *condor_schedd* that could happen on some platforms
  when handling certain *condor_startd* failures after invoking *condor_now*.
  :ticket:`7692`

- *classad_eval* no longer ignores trailing garbage in its first (ClassAd)
  argument.  This prevents  ``classad_eval 'x = y; y = 7' 'x'`` from
  incorrectly returning ``undefined``.
  :ticket:`7621`

- An ID token at the end of a file lacking a trailing newline is no longer ignored.
  :ticket:`7499`

- *condor_token_request_list* will now correctly list requests with request IDs
  starting with the number ``0``.
  :ticket:`7641`

- Fixed a bug introduced in 8.9.3 that cause the *condor_chirp* tool to crash
  when passed the ``getfile`` argument.
  :ticket:`7612`

- Added ``OMP_THREAD_LIMIT`` to list of environment variables to let programs like
  ``R`` know the maximum number of threads it should use.
  :ticket:`7649`

- Fixed a bug in Docker Universe that prevented administrator-defined
  bind-mounts from working correctly.
  :ticket:`7635`

- If the administrator of an execute machine has disabled file transfer plugins
  by setting :macro:`ENABLE_URL_TRANSFERS` to ``False``, then the machine Ad in
  the collector will no longer advertise support, which will prevent jobs from
  matching there and attempting to run.
  :ticket:`7707`

- Fixed a bug in *condor_dagman* where completed jobs incorrectly showed a 
  warning message related to job events.
  :ticket:`7548`

- Stopped HTCondor from sweeping OAuth credentials too aggressively, during the
  window between credential creation and job submission.  The *condor_credd*
  will now wait :macro:`SEC_CREDENTIAL_SWEEP_INTERVAL` seconds before cleaning
  them up, and the default is 300 seconds.
  :ticket:`7484`

- When authenticating, clients now only suggest methods that it supports,
  rather than providing a list of methods where it will reject some. This
  improves the initial security handshake.
  :ticket:`7500`

- For RPM installations, the HTCondor Python bindings RPM will now be
  automatically installed whenever the `condor` RPM is installed.
  :ticket:`7647`

- Bosco will use the newer version (1.3) of the tarballs on Enterprise Linux
  7 and 8.
  :ticket:`7753`

- HTCondor no longer probes the file transfer plugins except in the starter
  and then only if they are actually being used.  This was potentially adding
  delays to starting individual shadows, which when starting a lot of shadows
  could lead to scalability issues on a submit machine.
  :ticket:`7688`

Version 8.9.7
-------------

Release Notes:

- HTCondor version 8.9.7 released on May 20, 2020.

- The ``TOKEN`` authentication method has been renamed to ``IDTOKENS`` to
  better differentiate it from the ``SCITOKENS`` method.  All sites are
  encouraged to update their configurations accordingly; however, the
  configuration files and wire protocol remains backward compatible with
  prior releases.
  :ticket:`7540`

- HTCondor now advertises ``CUDAMaxSupportedVersion`` (when appropriate).  This
  attribute is an integer representation of the highest CUDA version the
  machine's driver supports.  HTCondor no longer advertises the attribute
  ``CUDARuntimeVersion``.
  :ticket:`7413`

- If you know what a shared port ID is, it may interest you to learn that
  starters in this version of HTCondor use their slot names, if available,
  in their shared port IDs.
  :ticket:`7510`

New Features:

- You may now specify that HTCondor only transfer files when the job
  succeeds (as defined by ``success_exit_code``).  Set ``when_to_transfer_output``
  to ``ON_SUCCESS``.  When you do, HTCondor will transfer files only when the
  job exits (in the sense of ``ON_EXIT``) with the specified success code.  This
  is intended to prevent unsuccessful jobs from going on hold because they
  failed to produce the expected output (file(s)).
  :ticket:`7270`

- HTCondor may now preserve the relative paths you specify when transferring
  files.  See the :doc:`/man-pages/condor_submit` man page about
  ``preserve_relative_paths``.
  :ticket:`7338`

- You may now specify a distinct list of files for use with the vanilla
  universe's support for application-level checkpointing
  (``checkpoint_exit_code``).  Use ``transfer_checkpoint_files`` if you'd
  like to shorten your ``transfer_output_files`` list by removing files
  only needed for checkpoints.  See the :doc:`/man-pages/condor_submit`
  man page.
  :ticket:`7269`

- The *condor_job_router* configuration and transform language has changed.
  The Job Router will still read the old configuration and transforms, but
  the new configuration syntax is much more flexible and powerful.

  - Routes are now a modified form of job transform. :macro:`JOB_ROUTER_ROUTE_NAMES``
    defines both the order and which routes are enabled
  - Multiple pre-route and post-route transforms that apply to all routes can be defined.
  - The Routes and transforms use the same syntax and transform engine as 
    :macro:`SUBMIT_TRANSFORM_NAMES`.

  :ticket:`7432`

- HTCondor now offers a submit command, ``cuda_version``, so that jobs can
  describe which CUDA version (if any) they use.  HTCondor will use that
  information to match the job with a machine whose driver supports that
  version of CUDA.  See the :doc:`/man-pages/condor_submit` man page.
  :ticket:`7413`

- Tokens can be blacklisted by setting the :macro:`SEC_TOKEN_BLACKLIST_EXPR`
  configuration parameter to an expression matching the token contents.
  Further, a unique ID has been added to all generated tokens, allowing
  individual tokens to be blacklisted.
  :ticket:`7449`
  :ticket:`7450`

- If the *condor_master* cannot authenticate with the collector then it will
  automatically attempt to request an ID token (which the collector
  administrator can subsequently approve).  This now matches the behavior of
  the *condor_schedd* and *condor_startd*. :ticket:`7447`

- The *condor_token_request_list* can now print out pending token requests
  when invoked with the ``-json`` flag. :ticket:`7454`

- Request IDs used for *condor_token_request* are now zero-padded, ensuring
  they are always a fixed length. :ticket:`7461`

- All token generation and usage is now logged using HTCondor's audit log
  mechanism. :ticket:`7450`

- The new :macro:`SEC_TOKEN_REQUEST_LIMITS` configuration parameter allows
  administrators to limit the authorizations available to issued tokens.
  :ticket:`7455`

- HTCondor now allows OAuth tokens and Kerberos credentials to be
  enabled on the same machine.  This involves some changes to the
  way these two features are configured.  *condor_store_cred* and the Python
  bindings has new commands to allow Kerberos and OAuth credentials to be stored
  and queried.
  :ticket:`7462`

- The submit command ``getenv`` can now be a list of environment variables
  to import and not just ``True`` or ``False``.
  :ticket:`7572`

- The *condor_history* command now has a ``startd`` option to query the *condor_startd*
  history file.  This works for both local and remote queries.
  :ticket:`7538`

- The ``-submitters`` argument to *condor_q`* now correctly shows jobs for the
  given submitter name, even when the submitter name is an accounting group.
  :ticket:`7616`

- The accountant ads that *condor_userprio* displays have two new attributes.
  The ``SubmitterLimit`` contains the fair share, in number of cores, that this
  submitter should have access to, if they have sufficient jobs, and they all match.
  The ``SubmitterShares`` is the percentage of the pool they should have access to.
  :ticket:`7626`
  :ticket:`7453`

- When running on a Linux system with cgroups enabled, the MemoryUsage
  attribute of a job now includes the memory used by the kernel disk
  cache.  This helps users set Request_Memory to more useful values.
  :ticket:`7442`

- Docker universe now works inside an unprivileged personal HTCondor,
  if you give the user starting the personal condor rights to run the
  docker commands.
  :ticket:`7485`

- The *condor_master* and other condor daemons can now run as PID 1.
  This is useful when starting HTCondor inside a container.
  :ticket:`7472`

- When worker nodes are running on CPUs that support the AVX512 instructions,
  the *condor_startd* now advertises that fact with has_avx512 attributes.
  :ticket:`7528`

- Added ``GOMAXPROCS`` to the default list of environment variables that are
  set to the number of CPU cores allocated to the job.
  :ticket:`7418`

- Added the option for *condor_dagman* to remove jobs after reducing
  MaxJobs to a value lower than the number of currently running jobs. This
  behavior is controlled by the
  :macro:`DAGMAN_REMOVE_JOBS_AFTER_LIMIT_CHANGE` macro, which defaults to False.
  :ticket:`7368`

- The new configuration parameter :macro:`NEGOTIATOR_SUBMITTER_CONSTRAINT`
  defines an expression which constrains which submitter ads are considered for
  matchmaking by the *condor_negotiator*.
  :ticket:`7490`

- Removed the unused and always set to zero job attribute LocalUserCpu
  and LocalSysCpu
  :ticket:`7546`

- *condor_submit* now treats ``request_gpu`` as a typo and suggests
  that ``request_gpus`` may have been what was intended.  This is the 
  same way that it treats ``request_cpu``.
  :ticket:`7421`

- Feature to enhance the reliability of *condor_ssh_to_job* is now on
  by default: :macro:`CONDOR_SSH_TO_JOB_FAKE_PASSWD_ENTRY` is now ``true``
  :ticket:`7536`

- Enhanced the dataflow jobs that we introduced in version 8.9.5. In
  addition to output files, we now also check the executable and stdin files.
  If any of these are newer than the input files, we consider this to be a
  dataflow job and we skip it if :macro:`SHADOW_SKIP_DATAFLOW_JOBS` set to ``True``.
  :ticket:`7488`

- When HTCondor is running as root on a Linux machine, it now makes /dev/shm
  a private mount for jobs.  This means that files written to /dev/shm in
  one job aren't visible to other jobs, and that HTCondor now cleans up
  any leftover files in /dev/shm when the job exits.  If you want to the
  old behavior of a shared /dev/shm, you can set :macro:`MOUNT_PRIVATE_DEV_SHM` 
  to ``false``.
  :ticket:`7443` 

- When configuration parameter :macro:`HAD_USE_PRIMARY` is set to ``True``,
  the collectors will be queried in the order in which they appear in
  :macro:`HAD_LIST`.
  Otherwise, the order in which the collectors are queried will be
  randomized (before, this was always done).
  :ticket:`7556`

- Added a very basic ``PROVISIONER`` node type to the *condor_dagman* parse
  language and plumbing. When this work is completed in a future release, it
  will allow users to provision remote compute resources (ie. Amazon EC2, 
  Argonne Cooley) as part of their DAG workflows, then run their jobs on
  these resources.
  :ticket:`5622`

- A new attribute ``ScratchDirFileCount`` was added to the Job ClassAd and to
  the Startd ClassAd. It contains the number of files in the job sandbox for the current job.
  This attribute will be refreshed as the same time that ``DiskUsage`` is refreshed.
  :ticket:`7486`

- A new configuration macro :macro:`SUBMIT_GENERATE_CUSTOM_RESOURCE_REQUIREMENTS` can be
  used to disable the behavior of *condor_submit* to generate Requirements clauses
  for job attributes that begin with Request
  :ticket:`7513`

- Made some performance improvements in the *condor_collector*.
  This includes new configuration parameter
  :macro:`COLLECTOR_FORWARD_CLAIMED_PRIVATE_ADS`, which reduces the amount
  of data forwarded between *condor_collector*\ s.
  :ticket:`7440`
  :ticket:`7423`

- *condor_install* can now generate a script to set environment variables
  for the "fish" shell. :ticket:`7505`

Bugs Fixed:

- The Box.com file transfer plugin now implements the chunked upload
  method, which means that uploads of 50 MB or greater are now
  possible. Prior to this implementation, jobs uploading large files
  would unexpectedly go on hold.
  :ticket:`7531`

- The *curl_plugin* previously implemented a minimum speed timeout with an
  option flag that caused memory problems in older versions of libcurl.
  We've reimplemented timeouts now using a callback that manually enforces
  a minimum 1 byte/second transfer speed.
  :ticket:`7414` 

- Some URLs for keys in AWS S3 buckets were previously of the form
  ``s3://<bucket>.s3-<region>.amazonaws.com/<key>``.  Not all regions support
  this form of address; instead, you must use URLs of the form
  ``s3://<bucket>.s3.<region>.amazonaws.com/<key>``.  HTCondor now allows
  and requires the latter; you will have to change older submit files.
  :ticket:`7517`

- Amazon's S3 service used to allow bucket names with underscores or capital
  letters.  HTCondor can now download from and upload to buckets with this
  sort of name.
  :ticket:`7477`

- The *condor_token* family of tools now respect the ``-debug`` command
  line flag. :ticket:`7448`

- The *condor_token_request_list* tool now respects the ``-reqid`` flag.
  :ticket:`7448`

- Tokens with authorization limits no longer need to explicitly list
  the ``ALLOW`` authorization, fixing a regression from 8.9.4. :ticket:`7456`

- Fixed a bug where Kerberos principals were being set incorrectly when
  :macro:`KERBEROS_SERVER_PRINCIPAL` was set.
  :ticket:`7577`

- The packaged versions of HTCondor automatically creates the directories to
  hold pool passwords, tokens, and Kerberos and OAuth credentials.
  :ticket:`7117`

- The HTCondor central manager will generate a pool password if needed on
  startup or reconfiguration. :ticket:`7634`

- Fixed a bug in reading service account credentials when submitting
  to Google Compute Engine (grid universe, grid-type ``gce``).
  :ticket:`7555`

- To work around an issue where long-running *gce_gahp* process enter a state
  where they can no longer authenticate with GCE, the daemon now restarts once
  every 24 hours.  This does not affect the jobs themselves.
  See :ref:`gce_configuration_variables`.
  :ticket:`7401`

- Fixed a bug that prevented the *condor_schedd* from effectively flocking
  to pools when resource request list prefetching is enabled, which is the
  default in HTCondor version 8.9
  :ticket:`7549`
  :ticket:`7539`

- It is now safe to call functions from the Python bindings ``htcondor`` module
  on multiple threads simultaneously. See the
  :ref:`python-bindings-thread-safety` section in the
  Python bindings documentation for more details.
  :ticket:`7359`

- Our ``htcondor.Submit.from_dag()`` Python binding now throws an exception
  when it fails, giving the programmer a chance to catch and recover. 
  Previously this just caused Python to fall over and die immediately.
  :ticket:`7337`

- The RPM packaging now obsoletes the standard universe package so that it will
  deleted upon upgrade.
  :ticket:`7444`

- Restored setting RUNPATH instead of RPATH for the libcondor_utils
  shared library and the Python bindings.
  The accidental change to setting RPATH in 8.9.5 altered how libraries
  were found when ``LD_LIBRARY_PATH`` is set.
  :ticket:`7584`

- The location for the CA certificates on Debian and Ubuntu systems is now
  properly set. :ticket:`7569`

- Fixed a bug where the *condor_schedd* and *condor_negotiator* couldn't
  talk to each other if one was version 8.9.3 and the other was version
  8.9.4 or later.
  :ticket:`7615`

Version 8.9.6
-------------

Release Notes:

-  HTCondor version 8.9.6 released on April 6, 2020.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes security-related bugs
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0001.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0001.html>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0002.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0002.html>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0003.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0003.html>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0004.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0004.html>`_.

   :ticket:`7356`
   :ticket:`7427`
   :ticket:`7507`

Version 8.9.5
-------------

Release Notes:

-  HTCondor version 8.9.5 released on January 2, 2020.

New Features:

-  Implemented a *dataflow* mode for jobs. When enabled, a job whose
   1) pre-declared output files already exist, and 2) output files are
   more recent than its input files, is considered a dataflow job and
   gets skipped. This feature can be enabled by setting the
   :macro:`SHADOW_SKIP_DATAFLOW_JOBS` configuration option to ``True``.
   :ticket:`7231`

-  Added a new tool, *classad_eval*, that can evaluate a ClassAd expression in
   the context of ClassAd attributes, and print the result in ClassAd format.
   :ticket:`7339`

-  You may now specify ports to forward into your Docker container.  See
   :ref:`Docker and Networking` for details.
   :ticket:`7322`

-  Added the ability to edit certain properties of a running *condor_dagman*
   workflow: **MaxJobs**, **MaxIdle**, **MaxPreScripts**, **MaxPostScripts**.
   A user can call *condor_qedit* to set new values in the job ad, which will
   then be updated in the running workflow.
   :ticket:`7236`

-  Jobs which must use temporary credentials for S3 access may now specify
   the "session token" in their submit files.  Set ``+EC2SessionToken``
   to the name of a file whose only content is the session token.  Temporary
   credentials have a limited lifetime, which HTCondor does not help you
   manage; as a result, file transfers may fail because the temporary
   credentials expired.
   :ticket:`7407`

-  Improved the performance of the negotiator by simplifying the definition of
   the *condor_startd*'s ``WithinResourceLimits`` attribute when custom
   resources are defined.
   :ticket:`7323`

-  If you configure a *condor_startd* with different SLOT_TYPEs,
   you can use the SLOT_TYPE as a prefix for configuration entries.
   This can be useful to set different BASE_GROUPs
   for different slot types within the same *condor_startd*. For example,
   ``SLOT_TYPE_1.BASE_CGROUP = hi_prio``
   :ticket:`7390`

-  Added a new knob :macro:`SUBMIT_ALLOW_GETENV`. This defaults to ``true``. When
   set to ``false``, a submit file with `getenv = true` will become an error.
   Administrators may want to set this to ``false`` to prevent users from
   submitting jobs that depend on the local environment of the submit machine.
   :ticket:`7383`

-  *condor_submit* will no longer set the ``Owner`` attribute of jobs
   it submits to the name of the current user. It now leaves this attribute up
   to the *condor_schedd* to set.  This change was made because the
   *condor_schedd* will reject the submission if the ``Owner`` attribute is set
   but does not match the name of the mapped authenticated user submitting the
   job, and it is difficult for *condor_submit* to know what the mapped name is
   when there is a map file configured.
   :ticket:`7355`

-  Added ability for a *condor_startd* to log the state of Ads when shutting
   down using :macro:`STARTD_PRINT_ADS_ON_SHUTDOWN` and 
   :macro:`STARTD_PRINT_ADS_FILTER`.
   :ticket:`7328`

Bugs Fixed:

-  ``condor_submit -i`` now works with Docker universe jobs.
   :ticket:`7394`

-  Fixed a bug that happened on a Linux *condor_startd* running as root where
   a running job getting close to the ``RequestMemory`` limit, could get stuck,
   and neither get held with an out of memory error, nor killed, nor allowed
   to run.
   :ticket:`7367`

-  The Python 3 bindings no longer cause a segmentation fault when putting a
   :class:`~classad.ClassAd` constructed from a Python dictionary into another
   :class:`~classad.ClassAd`.
   :ticket:`7371`

-  The Python 3 bindings were missing the division operator for
   :class:`~classad.ExprTree`.
   :ticket:`7372`

-  When calling :meth:`classad.ClassAd.setdefault` without a default, or
   with a default of None, if the default is used, it is now treated as the
   :attr:`classad.Value.Undefined` ClassAd value.
   :ticket:`7370`

-  Fixed a bug where when file transfers fail with an error message containing
   a newline (``\n``) character, the error message would not be propagated to
   the job's hold message.
   :ticket:`7395`

-  SciTokens support is now available on all Linux and MacOS platforms.
   :ticket:`7406`

-  Fixed a bug that caused the Python bindings included in the tarball
   package to fail due to a missing library dependency.
   :ticket:`7435`

-  Fixed a bug where the library that is pre-loaded to provide a sane passwd
   entry when using *condor_ssh_to_job* was placed in the wrong directory
   in the RPM packaging.
   :ticket:`7408`

Version 8.9.4
-------------

Release Notes:

- HTCondor version 8.9.4 released on November 19, 2019.

- The Python bindings are now packaged as extendable modules.
  :ticket:`6907`

- The format of the aborted event has changed.  This will
  only affect you if you're not using one the readers provided by HTCondor.
  :ticket:`7191`

- :macro:`DAGMAN_USE_JOIN_NODES` is now on by default.
  :ticket:`7271`

New Features:

- HTCondor now supports secure download and upload to and from S3.  See
  the *condor_submit* man page and :ref:`file_transfer_using_a_url`.
  :ticket:`7289`

- Reduced the memory needed for *condor_dagman* to load a DAG that has
  a large number of PARENT and CHILD statements.
  :ticket:`7170`

- Optimized *condor_dagman* startup speed by removing unnecessary 3-second
  sleep.
  :ticket:`7273`

- Added a new option to *condor_q*.  `-idle` shows only idle jobs and
  their requested resources.
  :ticket:`7241`

- `SciTokens <https://scitokens.org>`_ support is now available.
  :ticket:`7248`

- Added a new tool, :ref:`condor_evicted_files`,
  to help users find files that HTCondor is holding on to for them (as
  a result of a job being evicted when
  ``when_to_transfer_output = ON_EXIT_OR_EVICT``, or checkpointing when
  ``CheckpointExitCode`` is set).
  :ticket:`7038`

- Added ``erase_output_and_error_on_restart`` as a new submit command.  It
  defaults to ``true``; if set to ``false``, and ``when_to_transfer_output`` is
  ``ON_EXIT_OR_EVICT``, HTCondor will append to the output and error logs
  when the job restarts, instead of erasing them (and starting the logs
  over).  This may make the output and error logs more useful when the
  job self-checkpoints.
  :ticket:`7189`

- Added ``$(SUBMIT_TIME)``, ``$(YEAR)``, ``$(MONTH)``, and ``$(DAY)`` as
  built-in submit variables. These expand to the time of submission.
  :ticket:`7283`

- GPU monitoring is now on by default.  It reports ``DeviceGPUsAverageUsage``
  and ``DeviceGPUsMemoryPeakUsage`` for slots with GPUs assigned.  These values
  are for the lifetime of the *condor_startd*.  Also, we renamed ``GPUsUsage`` to
  ``GPUsAverageUsage`` because all other usage values are peaks.  We also
  now report GPU memory usage in the job termination event.
  :ticket:`7201`

- Added new configuration parameter for execute machines,
  :macro:`CONDOR_SSH_TO_JOB_FAKE_PASSWD_ENTRY`, which defaults to ``false``.
  When ``true``, condor LD_PRELOADs into unprivileged sshd it *condor_startd*
  a special version of the Linux getpwnam() library call, which forces
  the user's shell to /bin/bash and the home directory to the scratch directory.
  This allows *condor_ssh_to_job* to work on sites that don't create
  login shells for slots users, or who want to run as nobody.
  :ticket:`7260`

- The ``htcondor.Submit.from_dag()`` static method in the Python bindings,
  which creates a Submit description from a DAG file, now supports keyword
  arguments (in addition to positional arguments), and the ``options`` argument
  is now optional:

  .. code-block:: python

     dag_args = { "maxidle": 10, "maxpost": 5 }

     # with keyword arguments for filename and options
     dag_submit = htcondor.Submit.from_dag(filename = "mydagfile.dag", options = dag_args)

     # or like this, with no options
     dag_submit = htcondor.Submit.from_dag(filename = "mydagfile.dag")

  :ticket:`7278`

- Added an example of a multi-file plugin to transfer files from a locally
  mounted Gluster file system. This script is also designed to be a template
  for other file transfer plugins, as the logic to download or upload files is
  clearly indicated and could be easily changed to support different file
  services.
  :ticket:`7212`

- Added a new multi-file transfer plugin for downloading files from
  Microsoft OneDrive user accounts. This supports URLs like
  "onedrive://path/to/file" and using the plugin requires the administrator
  configure the *condor_credd* to allow users to obtain Microsoft OneDrive
  tokens and requires the user request Microsoft OneDrive tokens in their
  submit file. :ticket:`7171`

- Externally-issued SciTokens can be exchanged for an equivalent HTCondor-issued
  token, enabling authorization flows in some cases where SciTokens could
  not otherwise be used (such as when the remote daemon has no host certificate).
  :ticket:`7281`

- The *condor_annex* tool will now check during setup for instance credentials
  if none were specified.
  :ticket:`7097`

- The *condor_schedd* now keeps track of which submitters it has advertised to
  flocked pools.  The *condor_schedd* will only honor matchmaking requests
  from flocked pool for submitters it did not advertise to the flock pool.  This
  new logic only applies to auto-created authorizations (introduced in 8.9.3)
  and not NEGOTIATOR-level authorizations setup by pool administrators.
  :ticket:`7100`

- Added Python bindings for the TOKEN request API.
  :ticket:`7162`

- In addition to administrators, token requests can be approved by the user whose
  identity is requested.
  :ticket:`7159`

Bugs Fixed:

- The *curl_plugin* now correctly advertises ``file`` and ``ftp`` as
  supported methods.
  :ticket:`7357`

-  Fixed a bug where *condor_ssh_to_job* to a Docker universe job landed
   outside the container if the container had not completely started.
   :ticket:`7246`

- Fixed a bug where Docker universe jobs were always hard-killed (sent
  SIGKILL).  The appropriate signals are now being sent for hold, remove,
  and soft kill (defaulting to SIGTERM).  This gives Docker jobs a chance
  to shut down cleanly.
  :ticket:`7247`

- *condor_submit* and the python bindings ``Submit`` object will no longer treat
  submit commands that begin with ``request_<tag>`` as custom resource requests unless
  ``<tag>`` does not begin with an underscore, and is at least 2 characters long.
  :ticket:`7172`

- The python bindings ``Submit`` object now converts keys of the form ``+Attr``
  to ``MY.Attr`` when setting and getting values into the ``Submit`` object.
  The ``Submit`` object had been storing ``+Attr`` keys and then converting
  these keys to the correct ``MY.Attr`` form on an ad-hoc basis, this could lead
  to some very strange error conditions.
  :ticket:`7261`

- In some situations, notably with Amazon AWS, our *curl_plugin* requests URLs
  which return an HTTP 301 or 302 redirection but do not include a Location
  header. These were previously considered successful transfers. We've fixed
  this so they are now considered failures, and the jobs go on hold.
  :ticket:`7292`

- Our *curl_plugin* is designed to partially retry downloads which did not
  complete successfully (HTTP Content-Length header reporting a different number
  than bytes downloaded). However partial retries do not work with some proxy
  servers, causing jobs to go on hold. We've updated the plugin to not attempt
  partial retries when a proxy is detected.
  :ticket:`7259`

- The timeout for *condor_ssh_to_job* connection has been restored to the
  previous setting of 20 seconds. Shortening the timeout avoids getting into
  a deadlock between the *condor_schedd*, *condor_starter*, and
  *condor_shadow*.
  :ticket:`7193`

- Fixed a performance issue in the *curl_plugin*, where our low-bandwidth
  timeout caused 100% CPU utilization due to an old libcurl bug.
  :ticket:`7316`

- The Condor Connection Broker (CCB) will allow daemons to register at the
  ``ADVERTISE_STARTD``, ``ADVERTISE_SCHEDD``, and ``ADVERTISE_MASTER`` authorization
  level.  This reduces the minimum authorization needed by daemons that are located
  behind NATs.
  :ticket:`7225`

Version 8.9.3
-------------

Release Notes:

- HTCondor version 8.9.3 released on September 12, 2019.

- If you run a CCB server, please note that the default value for
  :macro:`CCB_RECONNECT_FILE` has changed.  If your configuration does not
  set :macro:`CCB_RECONNECT_FILE`, CCB will forget about existing connections
  after you upgrade.  To avoid this problem,
  set :macro:`CCB_RECONNECT_FILE` to its default path before upgrading.  (Look in
  the ``SPOOL`` directory for a file ending in ``.ccb_reconnect``.  If you
  don't see one, you don't have to do anything.)
  :ticket:`7135`

- The Log file specified by a job, and by the :macro:`EVENT_LOG` configuration variable
  will now have the year in the event time. Formerly, only the day and month were
  printed.  This change makes these logs unreadable by versions of DAGMan and *condor_wait*
  that are older 8.8.4 or 8.9.2.  The configuration variable :macro:`DEFAULT_USERLOG_FORMAT_OPTIONS`
  can be used to revert to the old time format or to opt in to UTC time and/or fractional seconds.
  :ticket:`6940`

- The format of the terminated and aborted events has changed.  This will
  only affect you if you're not using one the readers provided by HTCondor.
  :ticket:`6984`

New Features:

- ``TOKEN`` authentication is enabled by default if the HTCondor administrator
  does not specify a preferred list of authentication methods.  In this case,
  ``TOKEN`` is only used if the user has at least one usable token available.
  :ticket:`7070`  Similarly, ``SSL`` authentication is enabled by default and
  used if there is a server certificate available. 
  :ticket:`7074`

- The *condor_collector* daemon will automatically generate a pool password file at the
  location specified by :macro:`SEC_PASSWORD_FILE` if no file is already present.  This should
  ease the setup of ``TOKEN`` and ``POOL`` authentication for a new HTCondor pool. 
  :ticket:`7069`

- Added a new multifile transfer plugin for downloading and uploading
  files from/to Google Drive user accounts. This supports URLs like
  "gdrive://path/to/file" and using the plugin requires the administrator
  configure the *condor_credd* to allow users to obtain Google Drive
  tokens and requires the user request Google Drive tokens in their
  submit file. 
  :ticket:`7136`

- The Box.com multifile transfer plugin now supports uploads. The
  plugin will be used when a user lists a "box://path/to/file" URL as
  the output location of file when using ``transfer_output_remaps``.
  :ticket:`7085`

- Added a Python binding for *condor_submit_dag*. A new method,
  ``htcondor.Submit.from_dag()`` class creates a Submit description based on a
  .dag file:

  .. code-block:: python

        dag_args = { "maxidle": 10, "maxpost": 5 }
        dag_submit = htcondor.Submit.from_dag("mydagfile.dag", dag_args)

  The resulting ``dag_submit`` object can be submitted to a *condor_schedd* and
  monitored just like any other Submit description object in the Python bindings.
  :ticket:`6275`

- The Python binding's ``JobEventLog`` can now be pickled and unpickled,
  allowing users to preserve job-reading progress between process restarts.
  :ticket:`6944`

- A number of ease-of-use changes were made for submitting jobs from Python.
  In the Python method ``Schedd::queue_with_itemdata``,
  the keyword argument was renamed from ``from`` (which, unfortunately, is also
  a Python keyword) to ``itemdata``.  :ticket:`7064`
  Both this method and the ``Submit`` object can now accept a wider range of objects,
  as long as they can be converted to strings. :ticket:`7065`
  The ``Submit`` class's constructor now behaves in the same way as a Python dictionary
  :ticket:`7067`

- The ``Undefined`` and ``Error`` values in Python no longer cast silently to integers.
  Previously, ``Undefined`` and ``Error`` evaluated to ``True`` when used in a
  conditional; now, ``Undefined`` evaluates to ``False`` and evaluating ``Error`` results
  in a ``RuntimeError`` exception.  :ticket:`7109`

- Improved the speed of matchmaking in pools with partitionable slots
  by simplifying the slot's WithinResourceLimits expression.  This new
  definition for this expression now ignores the job's
  _condor_RequestXXX attributes, which were never set.
  In pools with simple start expressions, this can double the speed of
  matchmaking.
  :ticket:`7131`

- Improved the speed of matchmaking in pools that don't support
  standard universe by unconditionally removing standard universe related
  expressions in the slot START expression.
  :ticket:`7123`

- Reduced DAGMan's memory footprint when running DAGs with nodes
  that use the same submit file and/or current working directory.
  :ticket:`7121`

- The terminated and abort events now include "Tickets of Execution", which
  specify when the job terminated, who requested the termination, and the
  mechanism used to make the request (as both a string an integer).  This
  information is also present in the job ad (in the ``ToE`` attribute).
  Presently, tickets are only issued for normal job terminations (when the
  job terminated itself of its own accord), and for terminations resulting
  from the ``DEACTIVATE_CLAIM`` command.  We expect to support tickets for
  the other mechanisms in future releases.
  :ticket:`6984`

- Added new submit parameters ``cloud_label_names`` and
  ``cloud_label_<name>``, which allowing the setting of labels on the
  cloud instances created for **gce** grid jobs.
  :ticket:`6993`

- The *condor_schedd* automatically creates a security session for
  the negotiator if :macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION` is enabled
  (the default setting).  HTCondor pool administrators no longer need to
  setup explicit authentication from the negotiator to the *condor_schedd*; any
  negotiator trusted by the collector is automatically trusted by the collector.
  :ticket:`6956`

- Daemons will now print a warning in their log file when a client uses
  an X.509 credential for authentication that contains VOMS extensions that
  cannot be verified.
  These warnings can be silenced by setting configuration parameter
  :macro:`USE_VOMS_ATTRIBUTES` to ``False``.
  :ticket:`5916`

- When submitting jobs to a multi-cluster Slurm configuration under the
  grid universe, the cluster to submit to can be specified using the
  ``batch_queue`` submit attribute (e.g. ``batch_queue = debug@cluster1``).
  :ticket:`7167`

- HTCondor now sets numerous environment variables
  to tell the job (or libraries being used by the job) how many CPU cores
  have been provisioned.  Also added the configuration knob :macro:`STARTER_NUM_THREADS_ENV_VARS`
  to allow the administrator to customize this set of environment
  variables.
  :ticket:`7296`

Bugs Fixed:

- Fixed a bug where *condor_schedd* would not start if the history file
  size, named by MAX_HISTORY_SIZE was more than 2 Gigabytes.
  :ticket:`7023`

- The default :macro:`CCB_RECONNECT_FILE` name now includes the shared port ID
  instead of the port number, if available, which prevents multiple CCBs
  behind the same shared port from interfering with each other's state file.
  :ticket:`7135`

- Fixed a large memory leak when using SSL authentication.
  :ticket:`7145`

-  The ``TOKEN`` authentication method no longer fails if the ``/etc/condor/passwords.d``
   is missing.  
   :ticket:`7138`

-  Hostname-based verification for SSL now works more reliably from command-line tools.
   In some cases, the hostname was dropped internally in HTCondor, causing the SSL certificate
   verification to fail because only an IP address was available.
   :ticket:`7073`

- Fixed a bug that could cause the *condor_schedd* to crash when handling
  a query for the slot ads that it has claimed.
  :ticket:`7210`

- Eliminated needless work done by the *condor_schedd* when contacted by
  the negotiator when :macro:`CURB_MATCHMAKING` or :macro:`MAX_JOBS_RUNNING`
  prevent the *condor_schedd* from accepting any new matches.
  :ticket:`6749`

- HTCondor's Docker Universe jobs now more reliably disable the setuid
  capability from their jobs.  Docker Universe has also done this, but the
  method used has recently changed, and the new way should work going forward.
  :ticket:`7111`

- HTCondor users and daemons can request security tokens used for authentication.
  This allows the HTCondor pool administrator to simply approve or deny token
  requests instead of having to generate tokens and copy them between hosts.
  The *condor_schedd* and *condor_startd* will automatically request tokens from any collector
  they cannot authenticate with; authorizing these daemons can be done by simply
  having the collector administrator approve the request from the collector.
  Strong security for new pools can be bootstrapped by installing an auto-approval rule
  for host-based security while the pool is being installed.  :ticket:`7006`
  :ticket:`7094` :ticket:`7080`

- Changed the *condor_annex* default AMIs to run Docker jobs.  As a result,
  they no longer default to encrypted execute directories.
  :ticket:`6690`

- Improved the handling of parallel universe Docker jobs and the ability to rm and hold
  them.
  :ticket:`7076`

- Singularity jobs no longer mount the user's home directory by default.
  To re-enable this, set the knob ``SINGULARITY_MOUNT_HOME = true``.
  :ticket:`6676`

Version 8.9.2
-------------

Release Notes:

-  HTCondor version 8.9.2 released on June 4, 2019.

-  The default setting for :macro:`CREDD_OAUTH_MODE` is now ``true``.  This only
   affects people who were using the *condor_credd* to manage Kerberos credentials
   in the :macro:`SEC_CREDENTIAL_DIRECTORY`.
   :ticket:`7046`

Known Issues:

-  This release introduces a large memory leak when SSL authentication fails.
   This will be fixed in the next release.
   :ticket:`7145`

New Features:

-  The default file transfer plugin for HTTP/HTTPS will timeout transfers
   that make no progress as opposed to waiting indefinitely.  :ticket:`6971`

-  Added a new multifile transfer plugin for downloading files from Box.com user accounts. This
   supports URLs like "box://path/to/file" and using the plugin requires the administrator to configure the
   *condor_credd* to allow users to obtain Box.com tokens and requires the user request Box.com
   tokens in their submit file. :ticket:`7007`

-  The HTCondor manual has been migrated to
   `Read the Docs <https://htcondor.readthedocs.io/en/latest/>`_.
   :ticket:`6908`

-  Python bindings docstrings have been improved. The Python built-in ``help``
   function should now give better results on objects and function in the bindings.
   :ticket:`6953`

-  The system administrator can now configure better time stamps for the global event log
   and for all jobs that specify a user log or DAGMan nodes log. There are two new configuration
   variables that control this; :macro:`EVENT_LOG_FORMAT_OPTIONS` controls the format of the global event log
   and :macro:`DEFAULT_USERLOG_FORMAT_OPTIONS` controls formatting of user log and DAGMan nodes logs.  These
   configuration variables can individually enable UTC time, ISO 8601 time stamps, and fractional seconds.
   :ticket:`6941`

-  The implementation of SSL authentication has been made non-blocking, improving
   scalability and responsiveness when this method is used. :ticket:`6981`

-  SSL authentication no longer requires a client X509 certificate present in
   order to establish a security session.  If no client certificate is available,
   then the client is mapped to the user ``unauthenticated``. :ticket:`7032`

-  During SSL authentication, clients now verify that the server hostname matches
   the host's X509 certificate, using the rules from RFC 2818.  This matches the
   behavior most users expected in the first place.  To restore the prior behavior,
   where any valid certificate (regardless of hostname) is accepted by default, set
   :macro:`SSL_SKIP_HOST_CHECK` to ``true``. :ticket:`7030`

-  HTCondor will now utilize OpenSSL for random number generation when
   cryptographically secure (e.g., effectively impossible to guess beforehand) random
   numbers are needed.  Previous random number generation always utilized a method
   that was not appropriate for cryptographic contexts.  As a side-effect of this
   change, HTCondor can no longer be built without OpenSSL support. :ticket:`6990`

-  A new authentication method, ``TOKEN``, has been added.  This method provides
   the pool administrator with more fine-grained authorization control (making it
   appropriate for end-user use) and provides the ability for multiple pool passwords
   to exist within a single setup. :ticket:`6947`

-  Authentication can be done using `SciTokens <https://scitokens.org>`_.  If the
   client saves the token to the file specified in :macro:`SCITOKENS_FILE`, that token
   will be used to authenticate with the remote server.  Further, for HTCondor-C
   jobs, the token file can be specified by the job attribute ``ScitokensFile``.
   :ticket:`7011`

-  *condor_submit* and the python bindings submit now use a table to convert most submit keywords
   to job attributes. This should make adding new submit keywords in the future quicker and more reliable.
   :ticket:`7044`

-  File transfer plugins can now be supplied by the job. :ticket:`6855`

-  Add job ad attribute ``JobDisconnectedDate``.
   When the *condor_shadow* and *condor_starter* are disconnected from each other,
   this attribute is set to the time at which the disconnection happened.
   :ticket:`6978`

-  HTCondor EC2 components are now packaged for Debian and Ubuntu.
   :ticket:`7043`

Bugs Fixed:

-  *condor_status -af:r* now properly prints nested ClassAds.  The handling
   of undefined attribute references has also been corrected, so that that
   they print ``undefined`` instead of the name of the undefined attribute.
   :ticket:`6979`

-  X.509 proxies now work properly with job materialization.
   In particular, the job attributes describing the X.509 credential
   are now set properly.
   :ticket:`6972`

-  Argument names for all functions in the Python bindings
   (including class constructors and methods) have been normalized.
   We don't expect any compatibility problems with existing code.
   :ticket:`6963`

-  In the Python bindings, the default argument for ``use_tcp`` in
   :class:`Collector.advertise` is now ``True`` (it was previously ``False``,
   which was very outdated).
   :ticket:`6983`

-  Reduced the number of DNS resolutions that may be performed while
   establishing a network connection. Slow DNS queries could cause a
   connection to fail due to the peer timing out.
   :ticket:`6968`

Version 8.9.1
-------------

Release Notes:

-  HTCondor version 8.9.1 released on April 17, 2019.

New Features:

-  The deprecated ``HOSTALLOW...`` and ``HOSTDENY...`` configuration knobs
   have been removed. Please use ``ALLOW...`` and ``DENY...``. :ticket:`6921`

-  Implemented a new version of the curl_plugin with multi-file
   support, allowing it to transfer many files in a single invocation of
   the plugin. :ticket:`6499`
   :ticket:`6859`

-  The performance of HTCondor's File Transfer mechanism has improved
   when sending multiple files, especially in wide-area network
   settings. :ticket:`6884`

-  Added support for passing HTTPS authentication credentials to file
   transfer plugins, using specially customized protocols. :ticket:`6858`

-  If a job requests GPUs and is a Docker Universe job, HTCondor
   automatically mounts the nVidia GPU devices. :ticket:`6910`

-  If a job requests GPUs, and Singularity is enabled, HTCondor
   automatically passes the **-nv** flag to Singularity to tell it to
   mount the nVidia GPUs. :ticket:`6898`

-  Added a new submit file option, ``docker_network_type = host``, which
   causes a docker universe job to use the host's network, instead of
   the default NATed interface. :ticket:`6906`

-  Added a new configuration knob, :macro:`DOCKER_EXTRA_ARGUMENTS`, to allow administrators
   to add arbitrary docker command line options to the docker create
   command. :ticket:`6900`

-  We've added six new events to the job event log, recording details
   about file transfer. For both file transfer -in (before/to the job)
   and -out (after/from the job), we log if the transfer was queued,
   when it started, and when it finished. If the event was queued, the
   start event will note for how long; the first transfer event written
   will additionally include the starter's address, which has not
   otherwise been printed.

   We've also added several transfer-related attributes to the job ad.
   For jobs which do file transfer, we now set
   ``JobCurrentFinishTransferOutputDate``, to complement
   ``JobCurrentStartTransferOutputDate``, as well as the corresponding
   attributes for input transfer: ``JobCurrentStartTransferInputDate``
   and ``JobCurrentFinishTransferInputDate``. The new attributes are
   added at the same time as ``JobCurrentStartTransferOutputDate``, that
   is, at job termination. This set of attributes use the older and more
   deceptive definitions of file transfer timing. To obtain the times
   recorded by the new events, instead reference ``TransferInQueued``,
   ``TransferInStarted``, ``TransferInFinished``, ``TransferOutQueued``,
   ``TransferOutStarted``, and ``TransferOutFinished``. HTCondor sets
   these attributes (roughly) at the time they occur. :ticket:`6854`

-  Added support for output file remaps for URLs. This allows users to
   specify a URL where they want individual output files to go, and once
   a job is complete, we automatically uploads the files there. We are
   preserving the older implementation (OutputDestination), which puts
   all output files in the same place, for backwards compatibility.
   :ticket:`6876`

-  Added options ``f`` (return full target string) and ``g`` (perform
   multiple substitutions) to ClassAd function ``regexps()``. Added new
   ClassAd functions ``replace()`` (equivalent to ``regexps()`` with
   ``f`` option) and ``replaceall()`` (equivalent to ``regexps()`` with
   ``fg`` options). :ticket:`6848`

-  When jobs are run without file transfer on, usually because there is
   a shared file system, HTCondor used to unconditionally set the jobs
   argv[0] to the string *condor_exec.exe*. This breaks jobs that look
   at their own argv[0], in ways that are very hard to debug. In this
   release of HTCondor, we no longer do this. :ticket:`6943`

Bugs Fixed:

-  Avoid killing jobs using between 90% and 99% of memory limit.
   :ticket:`6925`

-  Improved how ``"Chirp"`` handles a network disconnection between the
   *condor_starter* and *condor_shadow*. ``"Chirp"`` commands now
   return a error and no longer cause the *condor_starter* to exit
   (killing the job). :ticket:`6873`

-  Fixed a bug that could cause *condor_submit* to send invalid job
   ClassAds to the *condor_schedd* when the executable attribute was
   not the same for all jobs in that submission. :ticket:`6719`

Version 8.9.0
-------------

Release Notes:

-  HTCondor version 8.9.0 released on February 28, 2019.

Known Issues:

This release may require configuration changes to work as before. During
this release series, we are making changes to make it easier to deploy
secure pools. This release contains two security related configuration
changes.

-  Absent any configuration, the default behavior is to deny
   authorization to all users.

-  In the configuration files, if ``ALLOW_DAEMON`` or ``DENY_DAEMON``
   are omitted, ``ALLOW_WRITE`` or ``DENY_WRITE`` are no longer used in
   their place.

   On most pools, the easiest way to get the previous behavior is to add
   the following to your configuration:

   .. code-block:: text

       ALLOW_READ = *
       ALLOW_DAEMON = $(ALLOW_WRITE)

   The main configuration file (``/etc/condor/condor_config``) already
   implements the above change by calling ``use SECURITY : HOST_BASED``.

   With the addition of the automatic security session for a family of
   HTCondor daemons and the existing match password authentication
   between the execute and submit daemons, most hosts in a pool may not
   require changes to the configuration files. On the central manager,
   you do need to ensure ``DAEMON`` level access for your submit nodes.
   Also, CCB requires ``DAEMON`` level access.

New Features:

-  Changed the default security behavior to deny authorization by
   default. Also, neither ``ALLOW_DAEMON`` nor ``DENY_DAEMON`` fall back
   to using the corresponding ``ALLOW_WRITE`` or ``DENY_WRITE`` when
   reading configuration files. :ticket:`6824`

-  A family of HTCondor daemons can now share a security session that
   allows them to trust each other without doing a security negotiation
   when a network connection is made amongst them. This "family"
   security session can be disabled by setting the new configuration
   parameter :macro:`SEC_USE_FAMILY_SESSION` to ``False``. :ticket:`6788`

-  Scheduler Universe jobs now start in order of priority, instead of
   random order. This is most typically used for DAGMan. When running
   *condor_submit_dag* against a .dag file, you can use the -priority
   <N> flag to set the priority for the overall *condor_dagman* job.
   When the *condor_schedd* is starting new Scheduler Universe jobs,
   the highest priority queued job will start first. If all queued
   Scheduler Universe jobs have equal priority, they get started in
   order of submission. :ticket:`6703`

-  Normally, HTCondor requires the user to specify their credentials
   when using EC2 (via the grid universe or via *condor_annex*). This
   allows users to use different accounts from the same machine.
   However, if a user started an EC2 instance with the privileges
   necessary to start other instances, and ran HTCondor in that
   instance, HTCondor was unable to use that instance's privileges; the
   user still had to specify their credentials. Instead, the user may
   now specify ``FROM INSTANCE`` instead of the name of a credential
   file to indicate that HTCondor should use the instance's credentials.

   By default, any user with access to a privileged EC2 instance has
   access to that instance's privileges. If you would like to make use
   of this feature, please read `HTCondor Annex Customization
   Guide <../cloud-computing/annex-customization-guide.html>`_ before
   adding privileges (an instance role) to an instance which allows
   access by other users, specifically including the submitting of jobs
   to or running jobs on that instance. :ticket:`6789`

-  The *condor_now* tool now supports vacating more than one job; the
   additional jobs' resources will be coalesced into a single slot, on
   which the now-job will be run. :ticket:`6694`

-  In the Python bindings, the ``JobEventLog`` class now has a ``close``
   method. It is also now its own iterable context manager (implements
   ``__enter__`` and ``__exit__``). The ``JobEvent`` class now
   implements ``__str__`` and ``__repr__``. :ticket:`6814`

-  the *condor_hdfs* daemon which allowed the hdfs daemons to run under
   the *condor_master* has been removed from the contributed source.
   :ticket:`6809`

Bugs Fixed:

-  Fixed potential authentication failures between the *condor_schedd*
   and *condor_startd* when multiple *condor_startd* s are using the
   same shared port server. :ticket:`5604`


