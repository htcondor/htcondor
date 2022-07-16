Version 9 Feature Releases
==========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 9.11.0
--------------

Release Notes:

.. HTCondor version 9.11.0 released on Month Date, 2022.

- HTCondor version 9.11.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-9015`.

- Removed support for the WriteUserLog class from libcondorapi.a.  This
  class was difficult to use correctly, and to our knowledge it is not
  currently in use.  Programmer who need to read the condor event
  log are recommended to do so from the HTCondor python bindings.
  :jira:`1163`

New Features:

- Added configuration templates ``PREEMPT_IF_DISK_EXCEEDED`` and ``HOLD_IF_DISK_EXCEEDED``
  :jira:`1173`

- The ``ADVERTISE_MASTER``, ``ADVERTISE_SCHEDD``, and
  ``ADVERTISE_STARTD`` authorization levels now also provide ``READ``
  level authorization.
  :jira:`1164`

- Singularity jobs now mount /tmp and /var/tmp under the scratch
  directory, not in tmpfs
  :jira:`1180`

- The default value for ``SCHEDD_ASSUME_NEGOTIATOR_GONE`` has been changed 
  from 20 minutes to a practically infinite value.  This is to prevent
  surprises when the schedd starts running vanilla universe jobs even when
  the admin has intentionally stopped the negotiator.
  :jira:`1185`

- If a job that is a Unix script with a ``#!`` interpreter fails to run because
  the interpreter doesn't exist, a clearer error message is written to the
  job log and in the job's ``HoldReason`` attribute.

- Added a new submit option ``container_target_dir`` that allows singularity
  jobs to specify the target directory
  :jira:`1171`

- When an **arc** grid universe job has both a token and an X.509
  proxy, now only the token is used for authentication with the ARC CE
  server. The proxy is still delegated for use by the job.
  :jira:`1194`
  
- DAGMan ``VARS`` lines are now able to specify ``PREPEND`` or ``APPEND`` 
  to allow passed variables to be set at the beginning or end of a DAG
  job's submit description. Any ``VARS`` without these options will have behavior
  derived from ``DAGMAN_DEFAULT_APPEND_VARS`` configuration variable.
  Which defaults to PREPEND.
  :jira:`1080`

- A new knob, ``SCHEDD_SEND_RESCHEDULE`` has been added.  When set
  to false, the schedd never tries to send a reschedule command to the
  negotiator.  The default is true. Set this to false in the HTCondor-CE
  and other systems that have no negotiator.
  :jira:`1192`

- Added ``-nested`` and ``-not-nested`` options to *condor_gpu_discovery* and
  updated man page to document them and to expand the documentation of the
  ``-simulate`` argument.  Nested output is now the default for GPU discovery.
  Added examples of new *condor_startd* configuration that is possible when the ``-nested``
  option is used for discovery.
  :jira:`711`

- Using *condor_hold* to put jobs on hold now overrides other hold
  conditions. Jobs already held for other reasons will be updated (i.e.
  ``HoldReason`` and ``HoldReasonCode`` changed). The jobs will remain
  held with the updated hold reason until released with *condor_release*.
  The periodic release job policy expressions are now ignored for these
  jobs.
  :jira:`740`

Bugs Fixed:

- Fixed a bug where **arc** grid universe jobs would remain in idle
  status indefinitely when delegation of the job's X.509 proxy
  certificate failed.
  Now, the jobs go to held status.
  :jira:`1194`

- Fixed a problem when condor_submit -i would sometimes fail trying
  to start an interactive docker universe job
  :jira:`1210`

- Fixed the ClassAd shared library extension mechanism.  An earlier
  development series broke the ability for users to add custom
  ClassAd functions as documented in 
  :doc:`/classads/classad-mechanism.html#extending-classads-with-user-written-functions`.
  :jira:`1196`

Version 9.10.1
--------------

Release Notes:

.. HTCondor version 9.10.1 released on Month Date, 2022.

- HTCondor version 9.10.1 not yet released.

New Features:

- None.

Bugs Fixed:

- Fixed inflated values for job attribute ``ActivationSetupDuration`` if
  the job checkpoints.
  :jira:`1190`

Version 9.10.0
--------------

Release Notes:

- HTCondor version 9.10.0 released on July 14, 2022.

- This version includes all the updates from :ref:`lts-version-history-9014`.

- On macOS, updated to LibreSSL 2.8.3 and removed support for VOMS.
  :jira:`1129`

- On macOS, the Python bindings are now built against the version of
  Python 3 included in the Command Line Tools for Xcode package.
  Previously, they were built against Python 3.8 as distributed from
  the website python.org.
  :jira:`1154`

- The default value of configuration parameter ``USE_VOMS_ATTRIBUTES``
  has been changed to ``False``.
  :jira:`1161`

New Features:

- The remote administration capability in daemon ads sent to the
  **condor_collector** (configuration parameter
  ``SEC_ENABLE_REMOTE_ADMINISTRATION``) is now enabled be default.
  Client tools that issue ADMINISTRATOR-level commands now try to use
  this capability if it's available.
  :jira:`1122`

- For **arc** grid universe jobs, SciTokens can now be used for
  authentication with the ARC CE server.
  :jira:`1061`

- Preliminary support for ARM (aarch64) and Power PC (ppc64le) CPU architectures
  on Alma Linux 8 and equivalent platforms.
  :jira:`1150`

- Added support for running on Linux systems that ship with OpenSSL version 3.
  :jira:`1148`

- *condor_submit* now has support for submitting jobsets. Jobsets are still
  a technology preview and still not ready for general use.
  :jira:`1063`
  
- All regular expressions in configuration and in the ClassAd regexp function
  now use the pcre2 10.39 library. (http://www.pcre.org). We believe that this
  will break no existing regular expressions.
  :jira:`1087`

- If "singularity" is really the "apptainer" runtime, HTCondor now
  sets environment variables to be passed to the job appropriately, which
  prevents apptainer from displaying ugly warnings about how this won't
  work in the future.
  :jira:`1137`

- The *condor_schedd* now adds the ``ServerTime`` attribute to the job
  ads of a query only if the client (i.e. *condor_q*) requests it.
  :jira:`1125`

Bugs Fixed:

- Fixed the ``TransferInputStats`` nested attributes ``SizeBytesLastRun`` and
  ``SizeBytesTotal`` values from overflowing and becoming negative when transferring
  files greater than two gigabytes via plugin.
  :jira:`1103`
  
- Fixed a bug preventing ``preserve_relative_paths`` from working with
  lots (tens of thousands) of files.
  :jira:`993`

- Fixed several minor bugs in how the *condor_shadow* and
  *condor_starter* handle network disruptions and jobs that have no
  lease.
  :jira:`960`

- The ``condor-blahp`` RPM now requires the matching ``condor`` RPM version.
  :jira:`1074`

Version 9.9.1
-------------

Release Notes:

- HTCondor version 9.9.1 released on June 14, 2022.

New Features:

- None.

Bugs Fixed:

- Fixed bug introduced in 9.9.0 when forwarding slot ads from one
  *condor_collector* to another. As a result, the *condor_negotiator*
  was unable to match any jobs to the slots.
  :jira:`1157`

Version 9.9.0
-------------

Release Notes:

- HTCondor version 9.9.0 released on May 31, 2022.

- This version includes all the updates from :ref:`lts-version-history-9013`.

New Features:

- Daemons can optionally send a security capability when they advertise themselves
  to the *condor_collector*.
  Authorized administrator tools can retrieve this capability from the
  *condor_collector*, which allows them to send administrative commands
  to the daemons.
  This allows the authentication and authorization of administrators of a
  whole pool to be centralized at the *condor_collector*.
  :jira:`638`

- Elliptic-curve Diffie-Hellman (ECDH) Key Exchange is now used to generate
  session keys for network communication.
  :jira:`283`

- Added replay protection for authenticated network communication.
  :jira:`287`
  :jira:`1054`

- Improved notification between network peers when a cached security
  session is not recognized.
  :jira:`1057`

- Fix issue where DAGMan direct submission failed when using Kerberos.
  :jira:`1060`

- Added a Job Ad attribute called ``JobSubmitMethod`` to record what tool a user
  used to submit job(s) to HTCondor.
  :jira:`996`

- Singularity jobs can now pull images from docker style repositories.
  :jira:`1059`

- The ``OWNER`` authorization level has been removed. Commands that used to
  require this level now require ``ADMINISTRATOR`` authorization.
  :jira:`1023`

- Python bindings on Windows have been updated to Python 3.9. Bindings for
  Python 2.7 will no longer be available. If you are building HTCondor
  for Windows yourself, Visual Studio 2022 and Python 3.8, 3.9 and 3.10
  are now supported by the build.
  :jira:`1008`

- Job duration policy hold message now displays the time exceeded in 
  'dd+hh:mm:ss' format rather than just seconds.
  :jira:`1062`

- Improved the algorithm in the *condor_schedd* to speed up the scheduling of jobs
  when reusing claims.
  :jira:`1056`

- Changed the result returned by evaluating a nested ClassAd a
  with no attribute named ``missing`` to return undefined when evaluating
  ``a["missing"]``.  This matches the ``a.missing`` syntax.
  :jira:`1065`

- Added support for a global CM which only schedules fair-share between *condor_schedd* s,
  with each *condor_schedd* owning a local CM for fair-share between users.
  :jira:`1003`

- In the configuration for daemon logs, ``D_FULLDEBUG`` no longer modifies the verbosity
  of other message categories.  For instance ``D_FULLDEBUG D_SECURITY`` will now select
  debug messages and ``D_SECURITY:1`` messages.  In previous versions it would select debug
  messages and also modify ``D_SECURITY`` to select ``D_SECURITY:2`` messages.   The manual
  has been updated to explain the use of verbosity modifiers in :macro:`<SUBSYS>_DEBUG`.
  :jira:`1090`

Bugs Fixed:

- Fixed a bug in the dedicated scheduler when using partitionable slots that would
  cause the *condor_schedd* to assert.
  :jira:`1042`

- Fix a rare bug where the starter will fail to start a job, and the job will
  immediately transition back to the idle state to be run elsewhere.
  :jira:`1040`

Version 9.8.1
-------------

Release Notes:

- HTCondor version 9.8.1 released on April 25, 2022.

New Features:

- None.

Bugs Fixed:

- Fix problem that can cause HTCondor to not start up when the network
  configuration is complex.
  Long hostnames, multiple CCB addresses, having both IPv4 and IPv6 addresses,
  and long private network names all contribute to complexity.
  :jira:`1070`

Version 9.8.0
-------------

Release Notes:

- HTCondor version 9.8.0 released on April 21, 2022.

- This version includes all the updates from :ref:`lts-version-history-9012`.

New Features:

- Added the ability to do matchmaking and targeted resource binding of GPUs into dynamic
  slots while constraining on the properties of the GPUs.  This new behavior is enabled
  by using the ``-nested`` option of *condor_gpu_discovery*, along with the new ``require_gpus``
  keyword of *condor_submit*.  With this change HTCondor can now support heterogeneous GPUs
  in a single partitionable slot, and allow a job to require to be assigned with a specific
  GPU when creating a dynamic slot.
  :jira:`953`

- Added ClassAd functions ``countMatches`` and ``evalInEachContext``. These functions
  are used to support matchmaking of heterogeneous custom resources such as GPUs.
  :jira:`977`

- Added the Reverse GAHP, which allows *condor_remote_cluster* to work with
  remote clusters that don't allow SSH keys or require Multi-Factor
  Authentication for all SSH connections.
  :jira:`1007`

- If an administrator configures additional custom docker networks on a worker node
  and would like jobs to be able to opt into use them, the startd knob
  ``DOCKER_NETWORKS`` has been added to allow additional custom networks
  to be added to the *docker_network_type* submit command.
  :jira:`995`

- Added the ``-key`` command-line option to *condor_token_request*, which
  allows users to ask HTCondor to use a particular signing key when creating
  the IDTOKEN.  Added the corresponding configuration macro,
  :macro:`SEC_TOKEN_FETCH_ALLOWED_SIGNING_KEYS`, which defaults to the default key
  (``POOL``).
  :jira:`1024`

- Added basic tools for submitting and monitoring DAGMan workflows to our 
  new :doc:`/man-pages/htcondor` CLI tool.
  :jira:`929`

- The ClassAd ``sum``, ``avg``, ``min`` and ``max`` functions now promote boolean
  values in the list being operated on to integers rather than to error.
  :jira:`970`

Bugs Fixed:

- Fix for *condor_gpu_discovery* crash when run on Linux for Power (ppc64le) architecture.
  :jira:`967`

Version 9.7.1
-------------

Release Notes:

- HTCondor version 9.7.1 released on April 5, 2022.

New Features:

- None.

Bugs Fixed:

- Fixed bug introduced in HTCondor v9.7.0 where job may go on hold without
  setting a ``HoldReason`` and/or ``HoldReasonCode`` and ``HoldReasonSubCode``
  attributes in the job classad.  In particular, this could happen when file transfer
  using a file transfer plugin failed.
  :jira:`1035`

Version 9.7.0
-------------

Release Notes:

- HTCondor version 9.7.0 released on March 15, 2022.

- This version includes all the updates from :ref:`lts-version-history-9011`.

New Features:

- Added list type configuration for periodic job policy configuration.
  Added ``SYSTEM_PERIODIC_HOLD_NAMES``, ``SYSTEM_PERIODIC_RELEASE_NAMES``
  and ``SYSTEM_PERIODIC_REMOVE_NAMES`` which each define a list of configuration
  variables to be evaluated for periodic job policy.
  :jira:`905`

- Container universe now supports running singularity jobs where the 
  command executable is hardcoded in to the runfile.  We call this 
  running the container as the job.
  :jira:`966`

- In most situations, jobs in COMPLETED or REMOVED status will no longer
  transition to HELD status.
  Before, these jobs could transition to HELD status due to job policy
  expressions, the *condor_rm* tool, or errors encountered by the
  *condor_shadow* or *condor_starter*.
  Grid universe jobs may still transition to HELD status if the
  *condor_gridmanager* can not clean up job-related resources on remote
  systems.
  :jira:`873`

- Improved performance of the *condor_schedd* during negotiation.
  :jira:`961`
  
- For **arc** grid universe jobs, environment variables specified in
  the job ad are now included in the ADL job description given to the
  ARC CE REST service.
  Also, added new submit command ``arc_application``, which can be used
  to add additional elements under the ``<Application>`` element of
  the ADL job description given to the ARC CE REST service.
  :jira:`932`

- Reduce the size of the singularity test executable by not linking in
  libraries it doesn't need.
  :jira:`927`

- DAGMan now manages job submission by writing jobs directly to the
  *condor_schedd*, instead of forking a *condor_submit* process. This behavior
  is controlled by the ``DAGMAN_USE_DIRECT_SUBMIT`` configuration knob, which
  defaults to ``True``.
  :jira:`619`

- If a job specifies ``output_destination``, the output and error logs,
  if requested, will now be transferred to their respective requested
  names, instead of ``_condor_stdout`` or ``_condor_stderr``.
  :jira:`955`

- *condor_qedit* and the Python bindings no longer request that job ad
  changes be forwarded to an active *condor_shadow* or *condor_gridmanager*.
  If forwarding ad changes is desired (say to affect job policy evaluation),
  *condor_qedit* has a new **-forward** option.
  The Python methods *Schedd.edit()* and *Schedd.edit_multiple()* now
  have an optional *flags* argument of type *TransactionFlags*.
  :jira:`963`

- Added more statistics about file transfers in the job ClassAd.
  :jira:`822`

Bugs Fixed:

- When the blahp submits a job to HTCondor, it no longer requests
  email notification about job errors.
  :jira:`895`

- Fixed a very rare bug in the timing subsystem that would prevent
  any daemon from appearing in the collector, and periodic expressions
  to be run less frequently than they should.
  :jira:`934`

- The view server can now handle very long Accounting Group names
  :jira:`913`

- Fixed some bugs where ``allowed_execute_duration`` and
  ``allowed_job_duration`` would be evaluated at the wrong points in a
  job's lifetime.
  :jira:`922`

- Fixed several bugs in file transfer where unexpected failures by file
  transfer plugins would not get handled correctly, resulting in empty
  Hold Reason messages and meaningless Hold Reason Subcodes reported in the
  job's classad.
  :jira:`842`

Version 9.6.0
-------------

Release Notes:

-  HTCondor version 9.6.0 released on March 15, 2022.

New Features:

-  None.

Bugs Fixed:

-  *Security Items*: This release of HTCondor fixes security-related bugs
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0001 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0001>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0002 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0002>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003>`_.

   :jira:`724`
   :jira:`730`
   :jira:`985`

Version 9.5.4
-------------

Release Notes:

- HTCondor version 9.5.4 released on February 8, 2022.

New Features:

- Improved the ability of the Access Point to detect the disappearance
  of an Execution Point that is running a job.  Specifically, the ability
  of the *condor_shadow* to detect a problem with the *condor_starter*.
  :jira:`954`

Bugs Fixed:

- HTCondor no longer assumes that PID 1 is always visible.  Instead,
  it checks to see if ``/proc`` was mounted with the ``hidepid`` option
  of ``1`` or less, and only checks for PID 1 if it was.
  :jira:`944`

Version 9.5.3
-------------

Release Notes:

- HTCondor version 9.5.3 released on February 1, 2021.

New Features:

- Added new configuration option, :macro:`CCB_TIMEOUT`.  Added new
  configuration option, :macro:`CCB_REQUIRED_TO_START`, which if set causes
  HTCondor to exit if :macro:`CCB_ADDRESS` was set but HTCondor could
  not obtain one.  :macro:`CCB_REQUIRED_TO_START` is ignored if
  :macro:`USE_SHARED_PORT` is set, which is the default.
  :jira:`925`

Bugs Fixed:

- Fixed a bug that caused any daemon to crash when it was configured
  to report to more than one collector, and any of the collectors'
  names could not be resolved by DNS.
  :jira:`952`

- Fixed a bug introduced earlier in this series where in very 
  rare cases, a schedd would not appear in the collector when it
  started up, but would appear an hour later.
  :jira:`931`

Version 9.5.2
-------------

Release Notes:

- HTCondor version 9.5.2 released on January 25, 2021.

New Features:

- None.

Bugs Fixed:

- Fixed a bug where the *condor_shadow* could run indefinitely when it
  failed to contact the *condor_startd* in an attempt to kill the
  job. This problem could become visible to the user in several different ways,
  such as a job appearing to not go on hold when periodic_hold becomes true.
  :jira:`933`

- Fix problem where **condor_ssh_to_job** may fail to connect to a job
  running under an HTCondor tarball installation (glidein) built from an RPM
  based platform.
  :jira:`942`

- Fixed a bug in the file transfer mechanism where URL transfers caused 
  subsequent failures to report incorrect error messages.
  :jira:`915`

Version 9.5.1
-------------

Release Notes:

- HTCondor version 9.5.1 released on January 18, 2022.

New Features:

- None.

Bugs Fixed:

- HTCondor now properly creates directories when transferring a directory
  tree out of SPOOL while preserving relative paths.  This bug would manifest
  after a self-checkpointing job created a file in a new subdirectory of a
  directory in its checkpoint: when the job was rescheduled and had to
  download its checkpoint, it would go on hold.
  :jira:`923`

Version 9.5.0
-------------

Release Notes:

- HTCondor version 9.5.0 released on January 13, 2022.

- This version includes all the updates from :ref:`lts-version-history-909`.

New Features:

- Added new Container Universe that allows users to describe container
  images that can be run in Singularity or Docker or other container runtimes.
  :jira:`850`

- Docker universe jobs can now self-checkpoint by setting
  checkpoint_exit_code in submit files.
  :jira:`841`

- Docker universe now works with jobs that don't transfer any files.
  :jira:`867`

- The **blahp** is now included in the HTCondor Linux native packages.
  :jira:`838`

- The tool *bosco_cluster* is being renamed to *condor_remote_cluster*.
  The tool can still be used via the old name, but that will stop working
  in a future release.
  :jira:`733`

- **condor_adstash** can parse and push ClassAds from a file to
  Elasticsearch by using the ``--ad_file PATH`` option.
  :jira:`779`

Bugs Fixed:

- Fixed a bug where if the submit file set a checkpoint_exit_code, and the administrator
  enabled singularity support on the execute node, the job would go on hold at checkpoint time.
  :jira:`837`

Version 9.4.1
-------------

Release Notes:

- HTCondor version 9.4.1 released on December 21, 2021.

New Features:

- Added activation metrics (``ActivationDuration``,
  ``ActivationExecutionDuration``, ``ActivationSetupDuration``, and
  ``ActivationTeardownDuration``).
  :jira:`861`

Bugs Fixed:

- Fix a bug where the error number could be cleared before
  being reported when a file transfer plugin fails.
  :jira:`889`

Version 9.4.0
-------------

Release Notes:

- HTCondor version 9.4.0 released on December 2, 2021.

- This version includes all the updates from :ref:`lts-version-history-908`.

New Features:

- Submission and basic management (list, status, and removal) of :ref:`job_sets` added
  to the :ref:`htcondor_command` CLI tool.
  :jira:`793`

- A new configuration variable ``EXTENDED_SUBMIT_COMMANDS`` can now be used to
  extend the submit language by configuration in the *condor_schedd*.
  :jira:`802`

- In a HAD configuration, the negotiator is now more robust when trying
  to update to collectors that may have failed.  It will no longer block
  and timeout for an extended period of time should this happen.
  :jira:`816`

- SINGULARITY_EXTRA_ARGUMENTS can now be a ClassAd expression, so that the
  extra arguments can depend on the job.
  :jira:`570`

- The Environment command in a condor submit file can now contain the string
  $$(CondorScratchDir), which will get expanded to the value of the scratch
  directory on the execute node.  This is useful, for example, when transferring
  software packages to the job's scratch dir, when those packages need an environment
  variable pointing to the root of their install.
  :jira:`805`

- The :ref:`classad_eval` tool now supports evaluating ClassAd expressions in
  the context of a match.  To specify the target ad, use the new
  ``-target-file`` command-line option.  You may also specify the
  context ad with ``-my-file``, a synonym for ``-file``.  The `classad_eval`
  tool also now supports the ``-debug`` and ``-help`` flags.
  :jira:`707`

- Added a configuration parameter HISTORY_CONTAINS_JOB_ENVIRONMENT which defaults to true.
  When false, the job's environment attribute is not saved in the history file.  For
  some sites, this can substantially reduce the size of the history file, and allow
  the history to contain many more jobs before rotation.
  :jira:`497`

- Added an attribute to the job ClassAd ``LastRemoteWallClockTime``.  It holds
  the wall clock time of the most recent completed job execution.
  :jira:`751`

- ``JOB_TRANSFORM_*`` and ``SUBMIT_REQUIREMENT_*`` operations in the *condor_schedd*
  are now applied to late materialization job factories at submit time.
  :jira:`756`

- Added option ``--rgahp-nologin`` to **remote_gahp**, which removes the
  ``-l`` option normally given to ``bash`` when starting a remote **blahpd**
  or **condor_ft-gahp**.
  :jira:`734`

- Herefile support was added to configuration templates, and the template
  ``use FEATURE : AssignAccountingGroup`` was converted to from the old
  transform  syntax to the the native transform syntax which requires that support.
  :jira:`796`

- The GPU monitor will no longer run if ``use feature:GPUs`` is enabled
  but GPU discovery did not detect any GPUs.  This mechanism is available
  for other startd cron jobs; see :macro:`STARTD_CRON_<JobName>_CONDITION`.
  :jira:`667`

- Added a new feature where a user can export some of their jobs from the
  *condor_schedd* in the form of a job-queue file intended to be used by
  a new temporary *condor_schedd*.
  After the temporary *condor_schedd* runs the jobs, the results can be
  imported back to the original *condor_schedd*.
  This is experimental code that is not suitable for production use.
  :jira:`179`

- When running *remote_gahp* interactively to start a remote
  *condor_ftp-gahp* instance, the user no longer has to set a fake
  ``CONDOR_INHERIT`` environment variable.
  :jira:`819`

Bugs Fixed:

- Fixed a bug that prevented the *condor_procd* (and thus all of condor) from starting
  when running under QEMU emulation.  Condor can now build and run under QEMU ARM
  emulation with this fix.
  :jira:`761`

- Fixed several unlikely bugs when parsing the time strings in ClassAds
  :jira:`814`

- Fixed a bug when computing the identity of a job's X.509 credential that
  isn't a proxy.
  :jira:`800`

- Fixed a bug that prevented file transfer from working properly on Unix systems
  when the job created a file to be transferred back to the submit machine containing
  a backslash in it.
  :jira:`747`

- Fixed some bugs which could cause the counts of transferred files
  reported in the job ad to be inaccurate.
  :jira:`813`

Version 9.3.2
-------------

- HTCondor version 9.3.2 released on November 30, 2021.

New Features:

- Added new submit command ``allowed_execute_duration``, which limits how long
  a job can run -- not including file transfer -- expressed in seconds.
  If a job exceeds this limit, it is placed on hold.
  :jira:`820`

Bugs Fixed:

- A problem where HTCondor would not create a directory on the execute
  node before trying to transfer a file into it should no longer occur.  (This
  would cause the job which triggered this problem to go on hold.)  One
  way to trigger this problem was by setting ``preserve_relative_paths``
  and specifying the same directory in both ``transfer_input_files`` and
  ``transfer_checkpoint_files``.
  :jira:`809`

Version 9.3.1
-------------

Release Notes:

- HTCondor version 9.3.1 released on November 9, 2021.

New Features:

- Added new submit command ``allowed_job_duration``, which limits how long
  a job can run, expressed in seconds.
  If a job exceeds this limit, it is placed on hold.
  :jira:`794`

Bugs Fixed:

- None.


Version 9.3.0
-------------

Release Notes:

- HTCondor version 9.3.0 released on November 3, 2021.

- This version includes all the updates from :ref:`lts-version-history-907`.

- As we transition from identity based authentication and authorization
  (X.509 certificates) to capability based authorization (bearer tokens),
  we have removed Globus GSI support from this release.
  :jira:`697`

- Submission to ARC CE via the GridFTP interface (grid universe type
  **nordugrid**) is no longer supported.
  Submission to ARC CE's REST interface can be done using the **arc**
  type in the grid universe.
  :jira:`697`

New Features:

- HTCondor will now, if configured, put some common cloud-related attributes
  in the slot ads.  Check the manual :ref:`for details <CommonCloudAttributesConfiguration>`.
  :jira:`616`

- Revamped machine ad attribute ``OpSys*`` and configuration parameter
  ``OPSYS*`` values for macOS.
  The OS name is now ``macOS`` and the version number no longer ignores
  the initial ``10.`` or ``11.`` of the actual OS version.
  For example, for macOS 10.15.4, the value of machine attribute
  ``OpSysLongName`` is now ``"macOS 10.15"`` instead of ``"MacOSX 15.4"``.
  :jira:`627`

- Added an example template for a custom file transfer plugin, which can be
  used to build new plugins.
  :jira:`728`

- Added a new generic knob for setting the slot user for all slots.  Configure
  ''NOBODY_SLOT_USER`` for all slots, instead of configuring a ``SLOT<N>_USER`` for each slot.
  :jira:`720`

- Improved and simplified how HTCondor locates the blahp software.
  Configuration parameter ``GLITE_LOCATION`` has been replaced by
  ``BLAHPD_LOCATION``.
  :jira:`713`

- Added new attributes to the job ClassAd which records the number of files 
  transferred between the *condor_shadow* and *condor_starter* only during
  the last run of the job.
  :jira:`741`

- When declining to put a job on hold due to the temporary scratch
  directory disappearing, verify that the directory is expected to exist
  and require that the job not be local universe.
  :jira:`680`

Bugs Fixed:

- None.

Version 9.2.0
-------------

Release Notes:

- HTCondor version 9.2.0 released on September 23, 2021.

- This version includes all the updates from :ref:`lts-version-history-906`.

New Features:

- Added a ``SERVICE`` node type to *condor_dagman*: a special node which runs
  in parallel to a DAG for the duration of its workflow. This can be used to
  run tasks that monitor or report on a DAG workflow without directly
  impacting it.
  :jira:`437`

- Added new configuration parameter ``NEGOTIATOR_MIN_INTERVAL``, which
  sets the minimum amount of the time between the start of one
  negotiation cycle and the next.
  :jira:`606`

- The *condor_userprio* tool now accepts one or more username arguments and will report
  priority and usage for only those users
  :jira:`559`

- Added a new ``-yes`` command-line argument to the *condor_annex*, allowing
  it to request EC2 instances without manual user confirmation.
  :jira:`443`

Bugs Fixed:

- HTCondor no longer crashes on start-up if ``COLLECTOR_HOST`` is set to
  a string with a colon and a port number, but no host part.
  :jira:`602`

- Changed the default value of configuration parameter ``MAIL`` to
  */usr/bin/mail* on Linux.
  This location is valid on all of our supported Linux platforms, unlike
  the previous default value of */bin/mail*.
  :jira:`581`

- Removed unnecessary limit on history ad polling and fixed some
  configuration parameter checks in *condor_adstash*.
  :jira:`629`

Version 9.1.6
-------------

Release Notes:

- HTCondor version 9.1.6 limited release on September 14, 2021.

New Features:

- None.

Bugs Fixed:

- Fixed a bug that prevented Singularity jobs from running when the singularity
  binary emitted many warning messages to stderr.
  :jira:`698`

Version 9.1.5
-------------

Release Notes:

- HTCondor version 9.1.5 limited release on September 8, 2021.

New Features:

- The number of files transferred between the *condor_shadow* and
  *condor_starter* is now recorded in the job ad with the new attributes.
  :jira:`679`

Bugs Fixed:

- None.

Version 9.1.4
-------------

Release Notes:

- HTCondor version 9.1.4 limited release on August 31, 2021.

New Features:

- Jobs are no longer put on hold if a failure occurs due to the scratch
  execute directory unexpectedly disappearing. Instead, the jobs will
  return to idle status to be re-run.
  :jira:`664`

Bugs Fixed:

- Fixed a problem introduced in HTCondor version 9.1.3 where
  X.509 proxy delegation to older versions of HTCondor would fail.
  :jira:`674`

Version 9.1.3
-------------

Release Notes:

- HTCondor version 9.1.3 released on August 19, 2021.

- This version includes all the updates from :ref:`lts-version-history-905`.

- Globus GSI is no longer needed for X.509 proxy delegation

- GSI is no longer in the list of default authentication methods.
  To use GSI, you must enable it by setting one or more of the
  ``SEC_<access-level>_AUTHENTICATION_METHODS`` configuration parameters.
  :jira:`518`

New Features:

- The semantics of undefined user job policy expressions has changed.  A
  policy whose expression evaluates to undefined is now uniformly ignored,
  instead of either putting the job on hold or treated as false.
  :jira:`442`

- Added two new attributes to the job ClassAd, ``NumHolds`` and ``NumHoldsByReason``, 
  that are used to provide historical information about how often this
  job went on hold and why. Details on all job ClassAd attributes, including
  these two new attributes, can be found in section:
  :doc:`../classad-attributes/job-classad-attributes`
  :jira:`554`

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
  use Google Cloud Storage's "interoperability" API.  The HMAC access key ID
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

- When GSI authentication is configured or used, a warning is now printed
  to daemon logs and the stderr of tools.
  These warnings can be suppressed by setting configuration parameters
  ``WARN_ON_GSI_CONFIGURATION`` and ``WARN_ON_GSI_USAGE`` to ``False``.
  :jira:`517`

- Introduced a new command-line tool, ``htcondor`` 
  (see :doc:`man page <../man-pages/htcondor>`) for managing HTCondor jobs
  and resources. This tool also includes new capabilities for running
  HTCondor jobs on Slurm machines which are temporarily acquired
  to act as HTCondor execution points.
  :jira:`252`


Bugs Fixed:

- Fixed a bug where jobs cannot start on Linux if the execute directory is placed
  under /tmp or /var/tmp.  The problem is this breaks the default MOUNT_UNDER_SCRATCH
  option.  As a result, if the administrator located EXECUTE under tmp, HTCondor can
  no longer make a private /tmp or /var/tmp directory for the job.
  :jira:`484`


Version 9.1.2
-------------

Release Notes:

-  HTCondor version 9.1.2 released on July 29, 2021.

New Features:

-  None.

Bugs Fixed:

-  *Security Items*: This release of HTCondor fixes security-related bugs
   described at

   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003>`_.
   -  `http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004 <http://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004>`_.

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

- This version includes all the updates from :ref:`lts-version-history-901`.

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

