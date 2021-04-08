Stable Release Series 8.8
=========================

This is the stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
8.8.x releases. New features will be added in the 8.9.x development
series.

The details of each version are described below.

Version 8.8.13
--------------

Release Notes:

- HTCondor version 8.8.13 released on March 23, 2021.

New Features:

- Docker version 20.10.4 has a serious bug that prevents Docker Universe from 
  working.  HTCondor now detects this version of Docker, and sets 
  HasDocker = false in the slot ad, so Docker Universe jobs will not match on
  such machines.
  :jira:`310`

- condor_ssh_to_job into a container now properly maps carriage return and 
  newline.  The most common symptom of this problem was that the nano
  editor would not work properly. Also, the performance of transferring large
  amounts of data has been substantially improved.
  :jira:`311`

- The HA replication mechanism can now accept either SHA-2 or MD5 checksums.
  This is because support for MD5 checksums must be removed in the 9.0 release of HTCondor.
  The checksum that replication will send is controlled by a new configuration variable
  ``HAD_FIPS_MODE`` which defaults to 0 for compatibility with older versions
  of HTCondor.  For compatibility with the upcoming 9.0 release of HTCondor
  set ``HAD_FIPS_MODE`` to 1. Setting it to 1 will break compatibility with versions
  of HTCondor before this release.
  :jira:`130`

- Submission to NorduGrid ARC CE (grid universe type **nordugrid**) now works
  with newer ARC CE versions where the X.509 Distinguished Names (DNs) of
  job submitters are obscured in the LDAP information service.
  :jira:`281`

Bugs Fixed:

- Fixed a bug where ``condor_annex`` would crash when executing the ``-status``
  or ``status`` commands if built with sufficiently-modern compilers.
  :jira:`318`

- Fixed a bug where ``use feature: GPUsMonitor`` set the wrong path to the
  GPU monitor binary on Windows.
  :jira:`125`

- Fixed a bug where the ClassAd ``usermap`` function did not work as documented.
  When the third agument did not match an item in the mapped list, it should
  have returned the first item in the list, but it returned undefined instead.
  :jira:`144`

- Fixed a bug with pslot preemption and disks with more than 4 TB of space.
  :jira:`195`

- Fixed a bug where the counts of job reconnections can be off in the
  Schedd Restart Report.
  :jira:`190`

- Fixed a bug that in rare cases can crash the *condor_schedd* if a DAG
  is quickly released and then removed.
  :jira:`309`

- Fixed a bug in DAGMan that prevented the use of the ``@`` symbol in the event
  log file path, where it was mistaken as an unresolved macro substitution.
  We now look for the ``@(`` character sequence to identify unresolved macros.
  :jira:`159`

- Fixed a bug where the Operating System and Version information were not
  detected on the Amazon Linux platform.
  :jira:`342`

Version 8.8.12
--------------

Release Notes:

- HTCondor version 8.8.12 released on November 23, 2020.

New Features:

- For compatibility with 8.9.9 (and eventually, the next stable series), add
  the family of version comparison functions to ClassAds.
  :jira:`36`

- For compatibility with 8.9 (and eventually, the next stable series), add
  the ``unresolved`` function to ClassAds.
  :jira:`66`

Bugs Fixed:

- Increased default Globus proxy key length to 2048 bits to align with NIST
  recommendations as of January 2015. The larger key size is required on
  modern Linuxes.
  :jira:`29`

- Fixed a bug in the *condor_job_router_info* that would build the umbrella 
  constraint value incorrectly when the tool was run as root.  This incorrect
  constraint would result in no jobs matching when the ``-match-jobs``
  ` or ``-route-jobs`` options were used.
  :jira:`38`

Version 8.8.11
--------------

Release Notes:

- HTCondor version 8.8.11 released on October 21, 2020.

New Features:

- None.

Bugs Fixed:

- Vanilla-universe jobs which set ``CheckpointExitCode`` (or otherwise make
  use of HTCondor's support for self-checkpointing) now report the total
  user and system CPU usage, not just the usage since the last checkpoint.
  :ticket:`4971`

- The Python bindings now define equality and inequality operators for
  ClassAd objects.
  :ticket:`7760`

- Fixed a bug in the *condor_job_router* that could cause a crash when a route
  was removed while jobs were still associated with it.
  :ticket:`7590`

- Fixed a bug with *condor_chirp* that could result in *condor_chirp* returning
  a non-zero exit code after a successful chirp command on Windows.
  :ticket:`7880`

- Using ``MACHINE_RESOURCE_NAMES`` will no longer cause crashes on Enterprise Linux 8.
  Additionally, the spurious warning about ``NAMES`` not being list as a
  resource has been eliminated.
  :ticket:`7755`

- Fixed the *condor_c-gahp* so that low-priority file transfer tasks don't
  block high-priority tasks such as querying the status of the remote jobs.
  :ticket:`7782`

- Fixed a rarely occurring bug that would cause the *condor_schedd* to crash,
  when trying to start a local universe job.
  :ticket:`7785`

- The GSI code now checks for a host alias before attempting to do a reverse
  DNS look-up.  This means that hosts with valid certificates no longer need
  a ``PTR`` record (although it must still be valid if it exists), if those hosts
  set the ``HOST_ALIAS`` configuration value appropriately
  (``$(FULL_HOSTNAME)``, usually).
  :ticket:`7788`

- Fixed a bug that can cause GSI authentication to fail with newer versions
  of OpenSSL.
  :ticket:`7332`

- Fixed a bug that could cause grid universe jobs of type ``batch`` to fail
  when the X.509 proxy was refreshed.
  :ticket:`7825`

- Fixed a bug where job attribute ``DelegateJobGSICredentialsLifetime``
  was ignored when a Condor-C job's refreshed proxy was forwarded to the
  remote *condor_schedd*.
  :ticket:`7856`

- Fixed a bug where worker nodes with very large (multi petabyte) scratch
  space could run jobs, but not reuse claims, causing lower utilization.
  :ticket:`7857`

- Attribute ``GridJobId`` is no longer removed from the job ad when the job
  enters ``Completed`` or ``Removed`` status.
  :ticket:`6159`

- When attempting to tell the *condor_startd* to stop a running job, the
  *condor_shadow* will now retry if a network failure occurs.
  :ticket:`7840`

- Fixed a bug where setting ``Notification = error`` in the submit file
  failed to send an email to the user when the job was held.
  :ticket:`7763`

- Fixed a bug in the ``-autoformat`` option when using lists and nested ads.
  :ticket:`7750`

- Improved the efficiency of process monitoring in macOS.
  :ticket:`7851`

- Re-enable VOMS support in the Debian and Ubuntu .deb packages.
  :ticket:`7875`

- Update the *bosco_quickstart* script to download tarballs via ``httpd``
  rather than ``ftp``.
  :ticket:`7821`

- Update the Debian and Ubuntu version tagging so that version numbers are
  unique and increasing between Debian and Ubuntu releases.
  :ticket:`7869`

- When HTCondor sends email about a failure to write to the ``STARTD_HISTORY``
  file, it now uses the correct name for the configuration parameter.
  :ticket:`7216`

- Improved the DaemonCore argument parser to look explicitly for ``-d`` or 
  ``-dynamic`` when using dynamic directories. All other arguments beginning
  with the letter *d* get passed on to the calling executable.
  :ticket:`7848`

- The D_SUB_SECOND debug format option will no longer produce timestamps
  with four digits (``1000``) in the milliseconds field.
  :ticket:`7685`

- Fixed the ``PreCmd`` and ``PostCmd`` job attributes to work correctly with
  absolute paths.
  :ticket:`7770`

Version 8.8.10
--------------

Release Notes:

- HTCondor version 8.8.10 released on August 6, 2020.

- Users can no longer use the *condor_qedit* command to disrupt the
  operations of the *condor_schedd*.
  :ticket:`7784`

- The ``SHARED_PORT_PORT`` setting is now honored. If you are using
  a non-standard port on machines other than the Central Manager, this
  bug fix will a require configuration change in order to specify
  the non-standard port.
  :ticket:`7697`

- On MacOSX, HTCondor now requires LibreSSL to function. MacOSX 10.13 and
  later are supported.

New Features:

- Added support for Ubuntu 20.04 (focal Fossa).
  :ticket:`7673`

- Added support for Amazon Linux 2.
  :ticket:`7430`

Bugs Fixed:

- Fixed some issues with the *condor_schedd* validating attribute values and actions from
  *condor_qedit*. Certain edits could cause the *condor_schedd* to enter an invalid state
  and in some cases would required editing of the job queue to restore the *condor_schedd*
  to operation. While no security exploits are known to be possible, mischievous
  users could potentially disrupt the operation of the *condor_schedd*. A more detailed
  description and workaround for these issues can be found in the ticket.
  :ticket:`7784`

- When the *condor_master* chooses the port to assign to the *condor_shared_port* daemon
  it will now ignore the ports specified in the ``COLLECTOR_LIST`` or ``COLLECTOR_HOST``
  configuration variables unless it is starting a primary collector.
  If it is not starting a primary collector (i.e. ``DAEMON_LIST`` does not have ``COLLECTOR``)
  it will use the port specified in ``SHARED_PORT_PORT`` or the default port, which is 9618.
  :ticket:`7697`

- The shared port daemon no longer blocks during socket hand-off.
  :ticket:`7502`

- The ``DiskUsage`` attribute should once again reflect the job's peak disk
  usage, rather than its current or terminal usage.
  :ticket:`7207`

- HTCondor daemons used to discard the private network name and address of
  daemons they were attempting to contact via the contactee's public
  address; however, if the contact had been pre-authorized, this would
  cause the contactee not to recognize the contacting daemon, and force it
  to reauthenticate.  The HTCondor daemons no longer discard the private
  network name and address; this will cause them to appear in the logs in
  places where they had not previously.
  :ticket:`7582`

- Allow ``SINGULARITY_EXTRA_ARGUMENTS`` to override the default -C option
  condor passes to singularity exec to allow administrators to tell
  condor not to contain certain resources.
  :ticket:`7719`

- *condor_gpu_discovery* no longer crashes if passed just the
  ``-dynamic`` flag.
  :ticket:`7639`

- *condor_gpu_discovery* now reports CoresPerCU for nVidia Volta and later GPUs.
  :ticket:`7704`

- Update *condor_gpu_discovery* to know how many CoresPerCU for nVidia Ampere
  GPUs.
  :ticket:`7711`

- Fix typographic error in ``condor.service`` file to wait for
  ``nfs-client.target``.
  :ticket:`7638`

- Increased ``TasksMax`` to ``4194303`` in HTCondor's
  systemd unit file so more than 32k shadows can run on a submit node.
  :ticket:`7650`

- For grid universe jobs of type ``batch``, stop using characters ``@``
  and ``#`` in temporary directory names.
  :ticket:`7730`

- When *condor_wait* is run without a limit on the number of jobs, it no
  longer exits if the number of active jobs goes to zero but there are more
  events in the log to read.  It now reads all existing events before deciding
  that there are no active jobs that need to be waited for.
  :ticket:`7653`

- In the python bindings the ``query`` methods on the ``Schedd`` and ``Collector``
  object now treat ``constraint=None`` having no constraint so all ads are returned
  rather than no ads.
  :ticket:`7727`

- Fixed a bug in the *condor_startd* on Windows that resulted in jobs failing to start with permission
  denied errors if ``ENCRYPT_EXECUTE_DIRECTORY`` was specified but the job did not have ``run_as_owner``
  enabled.
  :ticket:`7620`

- Fixed a bug that prevented the *condor_schedd* from effectively flocking
  to pools when resource request list prefetching is enabled, which is the
  default in HTCondor version 8.8.
  :ticket:`7754`

- The *sshd.sh* helper script no longer generates DSA keys when FIPS mode is enabled.
  :ticket:`7645`

- *condor_ssh_to_job* now works much better with Singularity. It allocates
  a pty and copies in the environment.
  :ticket:`7666`

Version 8.8.9
-------------

Release Notes:


-  HTCondor version 8.8.9 released on May 7, 2020.

New Features:

-  The attributes in a Partitionable slot that are produced by ``STARTD_PARTITIONABLE_SLOT_ATTRS``
   will contain evaluated values from the child slots rather than copies of the expressions
   from those slots.
   :ticket:`7521`

Bugs Fixed:

-  Fixed a bug whereby the ``MemoryUsage`` attribute in the job ClassAd for a Docker Universe job
   failed to report the maximum memory usage of the job, but instead
   reported either zero or the current memory usage.
   :ticket:`7527`

-  Fixed a bug that prevented the GPU from being re-assigned back to the Partitionable slot when a
   Dynamic slot containing a GPU was preempted.  This would result in the *condor_startd* aborting
   if the preempting job wanted a GPU and no free GPU was available.
   :ticket:`7591`

-  Fixed a bug that resulted in a segmentation fault when an iterator passed to the ``queue_with_itemdata``
   method on the ``Submit`` object raised a Python exception.
   :ticket:`7609`

-  Fixed a bug that caused ``SLOT_TYPE_<N>_<ATTR>`` overrides to be ignored when ``<ATTR>``
   was one of the standard policy configuration attributes like ``RANK``, ``PREEMPT``, ``KILL`` and
   ``SUSPEND``.  Only ``START`` and user defined attributes worked.
   :ticket:`7542`

-  Fixed a bug with accounting groups with quota where the quota was
   incorrectly calculated when jobs requested more than 1 CPU.  This
   bug was introduced in version 8.8.3.
   :ticket:`7602`

-  The *condor_annex* tool can again use Spot Fleets, after an unannounced
   API change by Amazon Web Services.
   :ticket:`7489`

-  Fixed a bug that prevented HTCondor from starting on Amazon AWS Fargate
   and other container based systems where HTCondor was started as root,
   but without the Linux capability CAP_SYS_RESOURCE.
   :ticket:`7470`

-  The *condor_collector* will no longer wait forever on an incoming command when
   only a few bytes of the command are sent and the socket is left open.
   Without this change, it is possible that a port scanner might hang the collector.
   :ticket:`7553`

-  Fixed a bug that prevented jobs with *stream_output* or *stream_error*
   to append to a file greater than 2Gb when running with a 32 bit shadow
   :ticket:`7547`

-  Fixed a bug where jobs that set `stream_output = true` would fail
   in a confusing way when the disk on the submit side is full.
   :ticket:`7596`

-  Fixed a bug that prevented *condor_ssh_to_job* from working when the
   job was in a container and there was a submit file argument.
   :ticket:`7506`

-  Fixed a bug where *condor_ssh_to_job* could fail for Docker Universe jobs if
   the HTCondor binaries are installed in a non-default location.
   :ticket:`7613`

-  Fixed a bug in *condor_gpu_discovery* and *condor_gpu_utilization* that could result in a crash on PowerPC processors.
   :ticket:`7605`

-  Fixed a bug that prevented ``POOL_HISTORY_MAX_STORAGE`` from begin honored on Windows.
   :ticket:`7438`

-  Increased the max directory depth from 20 to 128 when transferring files to
   avoid tripping a circuit breaker that limited the depth HTCondor was willing
   to traverse.
   :ticket:`7581`

-  Fixed a bug that caused the negotiator to crash when RequestCpus = 0
   and ``NEGOTIATOR_DEPTH_FIRST`` is set to ``True``.
   :ticket:`7583`

-  The *condor_wait* tool is again as efficient when waiting forever as when
   given a deadline on the command line.
   :ticket:`7458`

-  Fixed a problem where the Kerberos realm would not be set when there is no
   mapping from domain to realm and security debugging is not enabled.
   :ticket:`7492`

-  Fixed an issue where ``STARTD_NAME`` was ignored if the *condor_master* was
   started with the **-d** flag to enable dynamic directories.
   :ticket:`7585`

-  Fixed a bug that prevented ``$(KNOB:$(DEFAULT_VALUE))`` from being recognized by the configuration system
   and *condor_submit* as a macro with a default value that was also a macro.  As a result neither value would be substituted.
   :ticket:`7360`

-  Fixed a bug in the parsing of ``MAX_PROCD_LOG`` when a units value was used.  This bug could result in
   The *condor_procd* restricting itself to a very small log file size, which in turn could result in
   slow operation of the *condor_startd*
   :ticket:`7479`

-  Fixed a bug where *condor_qedit* would report incorrect counts of
   matching jobs when modifying multiple attributes.
   :ticket:`7520`

-  Fixed a bug with correctly marking and sweeping credentials on the execute
   machines when using Kerberos with ``SEC_CREDENTIAL_DIRECTORY`` defined.
   :ticket:`7558`

-  The *bosco_cluster* script now ensures that the ``glite/libexec`` directory
   is present on the remote host.
   :ticket:`7618`

-  ``openssh-server`` is now listed as an installation dependency so that
   *condor_ssh_to_job* works properly.
   :ticket:`7589`

-  On Debian and Ubuntu platforms, ``libglobus-gss-assist3`` is now listed
   as an installation dependency to ensure proper operation of HTCondor.
   :ticket:`7469`

-  The *condor_schedd* will now refuse to allow a job to be submitted when the
   submitting user is ``root`` or ``LOCAL_SYSTEM``.  Formerly, such jobs could
   be submitted, but would not run because of an ``Owner`` check in the *condor_shadow*.
   :ticket:`7441`

Version 8.8.8
-------------

Release Notes:

-  HTCondor version 8.8.8 released on April 6, 2020.

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

Version 8.8.7
-------------

Release Notes:

-  HTCondor version 8.8.7 released on December 26, 2019.

-  For *condor_annex* users: Amazon Web Services is deprecating support for
   the Node.js 8.10 runtime used by *condor_annex*.  If you ran the *condor_annex*
   setup command with a previous version of HTCondor, you should update your
   setup to use the new runtime.  `Instructions <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUpgradeTheAnnexRuntime>`_
   are available.
   :ticket:`7400`

New Features:

-  The *condor_job_router* now applies routes in the order specified by the
   configuration variable ``JOB_ROUTER_ROUTE_NAMES`` if it is defined.
   :ticket:`7284`

Bugs Fixed:

-  Fixed a bug that caused *condor_submit* to fail when the remote option
   was used and the remote *condor_schedd*  was using a map file.
   :ticket:`7353`

-  The *condor_wait* command will now function properly when reading a
   file on AFS that a process on another machine is writing.  This bug
   may have manifested as the machine running *condor_wait* not seeing
   writes to the log file.
   :ticket:`7373`

-  Fixed a packaging problem where the ``condor-bosco`` RPM
   (which is required by the ``condor-all`` RPM)
   could not installed on CentOS 8.
   :ticket:`7426`

-  Reverted an earlier change which prohibited certain characters in
   DAGMan node names. The period (.) character is now allowed again.
   We also added the ``DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS``
   configuration option, which, when sent to true, allow any characters
   (even illegal ones) to be allowed in node names.
   :ticket:`7403`

-  Fixed a bug in the Python bindings where the user could not turn on
   HTCondor daemons. We added ``DaemonsOn`` and ``DaemonOn`` to the
   ``DaemonCommands`` enumeration.
   :ticket:`7380`

-  Fixed a bug in the Python bindings that could result in a job submission
   failure with the report that there is no active transaction.
   :ticket:`7417`

-  Fixed a bug in the Python bindings that could result in intermingled messages if a multi-threaded Python program enabled
   the HTCondor debug log.
   :ticket:`7429`

-  The *condor_update_machine_ad* tool now respects the ``-pool`` and
   ``-name`` options.
   :ticket:`7378`

-  Fixed potential authentication failures between the *condor_schedd*
   and *condor_startd* when multiple *condor_startd* s are using the
   same shared port server. :ticket:`7391`

-  Fixed a bug where the *condor_negotiator* would refuse to match an
   IPv6-only *condor_startd* with a dual-stack *condor_schedd*.
   :ticket:`7397`

-  Fixed a bug that can cause the *condor_gridmanager* to exit and
   restart repeatedly if a Condor-C (i.e. grid-type *condor*) job's
   proxy file disappears.
   :ticket:`7409`

-  Fixed a bug that could cause the *condor_negotiator* to incorrectly
   count the number of jobs that will fit in a partitionable slot when
   ``NEGOTIATOR_DEPTH_FIRST`` is set to ``True``.
   The incorrect count was especially bad when ``SLOT_WEIGHT`` was set
   to a value other than the default of ``Cpus``.
   :ticket:`7422`

-  Python scripts included in the HTCondor release (e.g. *condor_top*)
   work again on systems that don't have *python2* in their ``PATH``.
   This was broken in HTCondor 8.8.6 and primarily affected macOS.
   :ticket:`7436`

Version 8.8.6
-------------

Release Notes:

- HTCondor version 8.8.6 released on November 13, 2019.

-  Initial support for Enterprise Linux 8 (CentOS 8).
   We recommend running HTCondor on systems with SELinux disabled.
   If SELinux is enabled, the audit log will contain many AVC messages
   in the audit log. Also, CREAM support is not present in this port.
   If there is demand, we may support CREAM in the future.
   :ticket:`7358`

-  The default encryption algorithm used by HTCondor was changed from
   `Triple-DES` to `Blowfish`.
   On a busy submit machine, many encrypted file transfers may consume
   significant CPU time.
   `Blowfish` is about six times faster and uses less memory than `Triple-DES`.
   :ticket:`7288`

-  The ClassAd builtin function regexMember has new semantics if
   any member of the list is undefined.  Previously, if any member
   of the list argument was undefined, it returned false.  Now, if
   any member of the list is undefined, it never returns false.  If any
   member of the list is undefined, and a defined member of the list matches,
   the function returns true.  Otherwise, it returns undefined.
   :ticket:`7243`

New Features:

-  Added a new argument to *condor_config_val*.  ``-summary`` reads the configuration
   files and prints out a summary of the values that differ from the defaults.
   :ticket:`7286`

- Updated the BOSCO find platform script to download the binary tarball
  via HTTPS instead of FTP.
  :ticket:`7362`

Bugs Fixed:

- Fixed a memory leak in the SSL authentication method.
  This memory leak could cause long running daemons, such as the
  *condor_collector* to grow in size without bound.
  :ticket:`7363`

-  Fixed a bug where submitting more than one job in a single cluster
   with the -spool option only actually submitted one job in the cluster.
   :ticket:`7282`

-  Fixed a bug where a misconfigured collector could forward ads to itself.
   The collector now recognizes more cases of this misconfiguration and
   properly ignores them.
   :ticket:`7229`

-  Fixed a bug where if the administrator configured a SLOT_WEIGHT that evaluated
   to less than 1.0, it would round down to zero, and the user would not
   get any matches.
   :ticket:`7313`

-  Fixed a bug where some tools (including *condor_submit*) would use the
   local daemon instead of failing if given a bogus hostname.
   :ticket:`7221`

-  Fixed a bug where ``COLLECTOR_REQUIREMENTS`` wrote too much to the log
   to be useful.  It now only writes warnings about rejected ads when
   the collector's debug level includes ``D_MACHINE``, and only includes
   the rejected ads themselves in the output at the ``D_MACHINE:2`` level.
   :ticket:`7264`

-  Fixed a bug where, for ``gce`` grid universe jobs, if the credentials
   file has credentials for more than one account, the wrong account's
   credentials are used for some requests.
   :ticket:`7218`

-  Fixed a bug where the ClassAd function bool() would return the wrong
   value when passed a string.
   :ticket:`7253`

-  Fixed a bug where *condor_preen* may mistakenly remove files from the
   the spool directory if the *condor_schedd* is heavily loaded or becomes unresponsive.
   :ticket:`7320`

-  Fixed a bug where *condor_preen* could render the *condor_schedd* unresponsive once a day
   for several minutes if there are a lot of job files spooled in the spool directory.
   :ticket:`7320`

-  Fixed a bug where *condor_submit* would fail when arguments were supplied
   but no submit file, and the arguments were sufficient that no submit file
   was needed.
   :ticket:`7249`

- Fixed a bug where the *condor_master* could crash upon reconfiguration if
  the configuration was changed to not use the *condor_shared_port* daemon.
  :ticket:`7335`

- Fixed a bug where using a custom print format with *condor_q* would not
  produce any output when doing aggregation.
  :ticket:`7290`

Version 8.8.5
-------------

Release Notes:

-  HTCondor version 8.8.5 released on September 5, 2019.

New Features:

-  Added configuration parameter ``MAX_UDP_MSGS_PER_CYCLE``, which
   controls how many UDP messages a daemon will read per DaemonCore
   event cycle. The default value of 1 maintains the behavior in previous
   versions of HTCondor.
   Setting a larger value can aid the ability of the *condor_schedd*
   and *condor_collector* daemons to handle heavy loads.
   :ticket:`7149`

-  Added configuration parameter ``MAX_TIMER_EVENTS_PER_CYCLE``, which
   controls how many internal timer events a daemon will dispatch per
   event cycle. The default value of 3 maintains the behavior in previous
   versions of HTCondor.
   Changing the value to zero (meaning no limit) could help
   the *condor_schedd* handle heavy loads.
   :ticket:`7195`

-  Updated *condor_gpu_discovery* to recognize nVidia Volta and Turing GPUs
   :ticket:`7197`

-  By default, HTCondor will no longer collect general usage information
   and forward it back to the HTCondor team.
   :ticket:`7219`

Bugs Fixed:

-  Fixed a bug that would sometimes result in the *condor_schedd* on Windows
   becoming slow to respond to commands after a period of time.  The slowness
   would persist until the *condor_schedd* was restarted.
   :ticket:`7143`

-  HTCondor daemons will no longer sit in a tight loop consuming the
   CPU when a network connection closes unexpectedly on Windows systems.
   :ticket:`7164`

-  Fixed a packaging error that caused the Java universe to be non-functional
   on Debian and Ubuntu systems.
   :ticket:`7209`

-  Fix a bug where singularity jobs with SINGULARITY_TARGET_DIR set
   would not have the job's environment properly set.
   :ticket:`7140`

-  Fixed a bug that caused incorrect values to be reported for the time
   taken to upload a job's files.
   :ticket:`7147`

-  HTCondor will now always use TCP to release slots claimed by the
   dedicated scheduler during shutdown.  This prevents some slots
   from staying in the Claimed/Idle state after a *condor_schedd* shutdown when
   running parallel jobs.
   :ticket:`7144`

-  Fixed a bug that caused the *condor_schedd* to not write a core file
   when it crashes on Linux.
   :ticket:`7163`

-  Fixed a bug in the *condor_schedd* that caused submit transforms to always
   reject submissions with more than one cluster id.  This bug was particularly
   easy to trigger by attempting to queue more than one submit object in
   a single transaction using the Python bindings.
   :ticket:`7036`

-  Fixed a bug that prevented new jobs from materializing when jobs changed
   to run state and a ``max_idle`` value was specified.
   :ticket:`7178`

-  Fixed a bug that caused *condor_chirp* to crash when the *getdir*
   command was used for an empty directory.
   :ticket:`7168`

-  Fixed a bug that caused GPU utilization to not be reported in the job
   ad when an encrypted execute directory is used.
   :ticket:`7169`

-  Integer values in ClassAds in HTCondor that are in hexadecimal or
   octal format are now rejected. Previously, they were read incorrectly.
   :ticket:`7127`

-  Fixed a bug in the *condor_dagman* parser which caused it to crash when
   certain commands were missing tokens.
   :ticket:`7196`

-  Fixed a bug in *condor_dagman* that caused it to fail when retrying a
   failed node with late materialization enabled.
   :ticket:`6946`

-  Minor change to the Python bindings to work around a bug in the third party
   collectd program on Linux that resulted in a crash trying to load the
   HTCondor Python module.
   :ticket:`7182`

-  Fixed a bug that could cause a daemon's log file to be created with the
   wrong owner. This would prevent the daemon from operating properly.
   :ticket:`7214`

-  Fixed a bug in *condor_submit* where it would require a match to a machine
   with GPUs when a job requested 0 GPUs.
   :ticket:`6938`

-  Fixed a bug in *condor_qedit* which was causing it to report an incorrect
   number of matching jobs.
   :ticket:`7119`

-  Fixed a bug where the annex-ec2 service would be disabled on Enterprise
   Linux systems when upgrading the HTCondor packages.
   :ticket:`7161`

-  Fixed an issue where *condor_ssh_to_job* would fail on Enterprise Linux
   systems when the administrator changed or deleted HTCondor's default
   configuration file.
   :ticket:`7116`

-  HTCondor will update its default configuration file by default on Enterprise
   Linux systems. Previously, if the administrator modified the default
   configuration file, the new file would appear as
   ``/etc/condor/condor_config.rpmnew``.
   :ticket:`7183`

Version 8.8.4
-------------

Release Notes:

-  HTCondor version 8.8.4 released on July 9, 2019.

Known Issues:

-  In the Python bindings, there are known issues with reference counting of
   ClassAds and ExprTrees. These problems are exacerbated by the more
   aggressive garbage collection in Python 3. See the ticket for more details.
   :ticket:`6721`

New Features:

-  The Python bindings are now available for Python 3 on Debian, Ubuntu, and
   Enterprise Linux 7. To use these bindings on Enterprise Linux 7 systems,
   the EPEL repositories are required to provide Python 3.6 and Boost 1.69.
   :ticket:`6327`

-  Added an optimization into DAGMan for graphs that use many-PARENT-many-CHILD
   statements. A new configuration variable ``DAGMAN_USE_JOIN_NODES`` can be
   used to automatically add an intermediate *join node* between the set of
   parent nodes and set of child nodes. When these sets are large, join nodes
   significantly improve *condor_dagman* memory footprint, parse time and
   submit speed. :ticket:`7108`

-  Dagman can now submit directly to the *condor_schedd*  without using *condor_submit*
   This provides a workaround for slow submission rates for very large DAGs.
   This is controlled by a new configuration variable ``DAGMAN_USE_CONDOR_SUBMIT``
   which defaults to ``True``.  When it is ``False``, Dagman will contact the
   local *condor_schedd*  directly to submit jobs. :ticket:`6974`

-  The HTCondor startd now advertises ``HasSelfCheckpointTransfers``, so that
   pools with 8.8.4 (and later) stable-series startds can run jobs submitted
   using a new feature in 8.9.3 (and later).
   :ticket:`7112`

Bugs Fixed:

-  Fixed a bug that caused editing a job ClassAd in the schedd via the
   Python bindings to be needlessly inefficient.
   :ticket:`7124`

-  Fixed a bug that could cause the *condor_schedd* to crash when a
   scheduler universe job is removed.
   :ticket:`7095`

-  If a user accidentally submits a parallel universe job with thousands
   of times more nodes than exist in the pool, the *condor_schedd* no longer
   gets stuck for hours sorting that out.
   :ticket:`7055`

-  Fixed a bug on the ARM architecture that caused the *condor_schedd*
   to crash when starting jobs and responding to *condor_history* queries.
   :ticket:`7102`

-  HTCondor properly starts up when the ``condor`` user is in LDAP.
   The *condor_master* creates ``/var/run/condor`` and ``/var/lock/condor``
   as needed at start up.
   :ticket:`7101`

-  The *condor_master* will no longer abort when the ``DAEMON_LIST`` does not contain
   ``MASTER``;  And when the ``DAEMON_LIST`` is empty, the *condor_master* will now
   start the ``SHARED_PORT`` daemon if shared port is enabled.
   :ticket:`7133`

-  Fixed a bug that prevented the inclusion of the last `OBITUARY_LOG_LENGTH`
   lines of the dead daemon's log in the obituary.  Increased the default
   `OBITUARY_LOG_LENGTH` from 20 to 200.
   :ticket:`7103`

-  Fixed a bug that could cause custom resources to fail to be released from a
   dynamic slot to partitionable slot correctly when there were multiple custom
   resources with the same identifier
   :ticket:`7104`

-  Fixed a bug that could result in job attributes ``CommittedTime`` and
   ``CommittedSlotTime`` reporting overly-large values.
   :ticket:`7083`

-  Improved the error messages generated when GSI authentication fails.
   :ticket:`7052`

-  Improved detection of failures writing to the job event logs.
   :ticket:`7008`

-  Updated the ``ChildCollector`` and ``CollectorNode`` configuration templates
   to set ``CCB_RECONNECT_FILE``.  This avoids a bug where each collector
   running behind the same shared port daemon uses the same reconnect file,
   corrupting it.  (This corruption will cause new connections to a daemon
   using CCB to fail if the collector has restarted since the daemon initially
   registered.)  If your configuration does not use the templates to run
   multiple collectors behind the same shared port daemon, you will need to
   update your configuration by hand.
   :ticket:`7134`

-  The *condor_q* tool now displays ``-nobatch`` mode by default when the ``-run``
   option is used.
   :ticket:`7068`

-  HTCondor EC2 components are now packaged for Debian and Ubuntu.
   :ticket:`7084`

-  Fixed a bug that could cause *condor_submit* to send invalid job
   ClassAds to the *condor_schedd* when the executable attribute was
   not the same for all jobs in that submission. :ticket:`6719`

-  Fixed a bug in the Standard Universe where ``SOFT_UID_DOMAIN`` did not
   work as expected.
   :ticket:`7075`

Version 8.8.3
-------------

Release Notes:

-  HTCondor version 8.8.3 released on May 28, 2019.

New Features:

-  The performance of HTCondor's File Transfer mechanism has improved when
   sending multiple files, especially in wide-area network settings.
   :ticket:`7000`

-  The HTCondor startd now deletes any orphaned Docker containers
   that have been left behind in the case of a starter crash, machine
   crash or docker restart
   :ticket:`7019`

-  If ``MAXJOBRETIREMENTTIME`` evaluates to ``-1``, it will truncate a job's
   retirement even during a peaceful shutdown.
   :ticket:`7034`

-  Unusually slow DNS queries now generate a warning in the daemon logs.
   :ticket:`6967`

-  Docker Universe now creates containers with a label named
   org.htcondorproject for 3rd party monitoring tools to classify
   and identify containers as managed by HTCondor.
   :ticket:`6965`

Bugs Fixed:

-  ``condor_off -peaceful`` will now work by default (and whenever
   ``MAXJOBRETIREMENTTIME`` is zero).
   :ticket:`7034`

-  Fixed a bug that caused the *condor_shadow* to not attempt to
   reconnect to the *condor_starter* after a network disconnection.
   This bug will also prevent reconnecting to some jobs after a
   restart of the *condor_schedd*.
   :ticket:`7033`

-  Fixed a bug that prevented ``condor_submit -i`` from working with
   a Singularity container environment for more than three minutes.
   :ticket:`7018`

-  Restored the old Python bindings for reading the job event log
   (``EventIterator`` and ``read_events()``) for Python 2.
   In HTCondor 8.8.2, they were mistakenly restored for Python 3 only.
   These bindings are marked as deprecated and will likely be
   removed permanently in the 8.9 series. Users should transition to the
   replacement bindings (``JobEventLog``)
   :ticket:`7039`

-  Included the Python binding libraries in the Debian and Ubuntu deb packages.
   :ticket:`7048`

-  Fixed a bug with *condor_ssh_to_job* did not remove subdirectories
   from the scratch directory on ssh exit.
   :ticket:`7010`

-  Fixed a bug that prevented HTCondor from being started inside a docker
   container with the *condor_master* as PID 1.  HTCondor could start
   if the master was launched from a script.
   :ticket:`7017`

-  Fixed a bug with singularity jobs where TMPDIR was set to the wrong
   value.  It is now set the the scratch directory inside the container.
   :ticket:`6991`

-  Fixed a bug when pid namespaces where enabled and vanilla checkpointing
   was also enabled that caused one copy of the pid namespace wrapper to wrap
   the job per each checkpoint restart.
   :ticket:`6986`

-  Fixed a bug where the memory usage reported for Docker Universe jobs
   in the job ClassAd and job event log could be underestimated.
   :ticket:`7049`

-  The job attributes ``NumJobStarts`` and ``JobRunCount`` are now
   updated properly for the grid universe and the job router.
   :ticket:`7016`

-  Fixed a bug that could cause reading ClassAds from a pipe to fail.
   :ticket:`7001`

-  Fixed a bug in *condor_q* that would result in the error "Two results with the same ID"
   when the ``-long`` and ``-attributes`` options were used, and the attributes list did
   not contain the ``ProcId`` attribute.
   :ticket:`6997`

-  Fixed a bug when GSI authentication fails, which could cause all other
   authentication methods to be skipped.
   :ticket:`7024`

-  Ensured that the HTCondor Annex boot-time configuration is done after the
   network is available.
   :ticket:`7045`

Version 8.8.2
-------------

Release Notes:

-  HTCondor version 8.8.2 released on April 11, 2019.

New Features:

-  Added a new parameter ``SINGULARITY_IS_SETUID``
   :index:`SINGULARITY_IS_SETUID`, which defaults to true. If
   false, allows *condor_ssh_to_job* to work when Singularity is
   configured to run without the setuid wrapper. :ticket:`6931`

-  The negotiator parameter ``NEGOTIATOR_DEPTH_FIRST``
   :index:`NEGOTIATOR_DEPTH_FIRST` has been added which, when
   using partitionable slots, fill each machine up with jobs before
   trying to use the next available machine. :ticket:`5884`

-  The Python bindings ``ClassAd`` module has a new printJson() method
   to serialize a ClassAd into a string in JSON format. :ticket:`6950`

Bugs Fixed:

-  Support for the *condor_ssh_to_job* command, when ssh'ing to a
   Singularity job, requires the nsenter command. Previous versions of
   HTCondor relied on features of nsenter not universally available.
   8.8.2 now works with all known versions of nsenter. :ticket:`6934`

-  Moved the execution of ``USER_JOB_WRAPPER``
   :index:`USER_JOB_WRAPPER` with Singularity jobs to be executed
   outside the container, not inside the container. :ticket:`6904`
-  Fixed a bug where *condor_ssh_to_job* would not work to a Docker
   universe job when file transfer was off. :ticket:`6945`

-  Included a patch from the development series that fixes problems that
   could crash *condor_annex* to crash. :ticket:`6980`

-  Fixed a bug that could cause the ``job_queue.log`` file to be
   corrupted when the *condor_schedd* compacts it. :ticket:`6929`

-  The *condor_userprio* command, when given the -negotiator and -l
   options used to emit the value of the concurrency limits in the one
   large ClassAd it printed. This was removed in 8.8.0, but has been
   restored in 8.8.2. :ticket:`6948`

-  In some situations, the GPU monitoring code could disagree with the
   GPU discovery code about the mapping between GPU device indices and
   actual devices. Both now use PCI bus IDs to establish the mapping.
   One consequence of this change is that we now prefer to use NVidia's
   management library, rather than the CUDA run-time library, when doing
   discovery. :ticket:`6903`
   :ticket:`6901`

-  Corrected documentation of ``CHIRP_DELAYED_UPDATED_PREFIX``; it is
   neither singular nor a prefix. Also resolved a problem where
   administrators had to specify each attribute in that list, rather
   than via prefixes or via wildcards. :ticket:`6958`

-  The Condormaster now waits until the *condor_procd* has exited
   before exiting itself. This change helps to prevent problems on
   Windows with using the Service Control Manager to restart the Condor
   service. :ticket:`6952`

-  Fixed a bug on Windows that could cause a delay of up to 2 minutes in
   responding to *condor_reconfig*, *condor_restart* or *condor_off*
   commands when using shared port. :ticket:`6960`

-  Fixed a bug that could cause the *condor_schedd* on Windows to to
   restart with the message "fd_set is full". This change reduces that
   maximum number of active connections that a *condor_collector* or
   *condor_schedd* on Windows will allow from 1023 to 1014. :ticket:`6957`

-  Fixed a bug where local universe jobs where unable to run
   *condor_submit* to their local schedd. :ticket:`6920`

-  Restored the old Python bindings for reading the job event log
   (``EventIterator`` and ``read_events()``). These bindings are marked
   as deprecated, are not available in Python 3, and will likely be
   removed permanently in the 8.9 series. Users should transition to the
   replacement bindings (``JobEventLog``) :ticket:`6939`

-  Fixed a bug that could cause entries in the job event log to be
   written with the wrong job id when a *condor_shadow* process is used
   to run multiple jobs. :ticket:`6919`

-  In some situations, the bytes sent and bytes received values in the
   termination event of the job event log could be reversed. This has
   been fixed. :ticket:`6914`

-  For grid universe jobs of type ``batch``, the job now receives a
   signal when the batch system wants it to exit, giving the job a
   chance to shut down gracefully. :ticket:`6915`

Version 8.8.1
-------------

Release Notes:

-  HTCondor version 8.8.1 released on February 19, 2019.

Known Issues:

-  GPU resource monitoring is no longer enabled by default after we
   received reports indicating excessive CPU usage. We believe we've
   fixed the problem, but would like to get updated reports from users
   who were previously affected. To enable (the patched) GPU resource
   monitoring, add 'use feature: GPUsMonitor' to the HTCondor
   configuration. Thank you.
   :ticket:`6857`

-  Discovered a bug in DAGMan where graph metrics reporting could
   sometimes send the *condor_dagman* process into an infinite loop. We
   worked around this by disabling graph metrics reporting by default,
   via the new ``DAGMAN_REPORT_GRAPH_METRICS``
   :index:`DAGMAN_REPORT_GRAPH_METRICS` configuration knob.
   :ticket:`6896`

New Features:

-  None.

Bugs Fixed:

-  Fixed a bug that caused *condor_gpu_discovery* to report the wrong
   value for DeviceMemory and possibly other attributes of the GPU when
   CUDA 10 was installed as the default run-time. Also fixed a bug that
   would sometimes cause the reported value of DeviceMemory to be
   limited to 4 Gigabytes. :ticket:`6883`

-  Fixed bug that prevented HTCondor on Windows from running jobs in the
   default configuration when started as a service. :ticket:`6853`

-  The Job Router no longer sets an incorrect ``User`` job attribute
   when routing a job between two *condor_schedd* s with different
   values for configuration parameter ``UID_DOMAIN``. :ticket:`6856`

-  Made Collector.locateAll() method more efficient in the Python
   bindings. :ticket:`6831`

-  Improved efficiency of negotiation code in the *condor_schedd*.
   :ticket:`6834`

-  The new ``minihtcondor`` package now starts HTCondor automatically at
   after installation. :ticket:`6888`

-  The *condor_master* now sends status updates to *systemd* every 10
   seconds. :ticket:`6888`

-  *condor_q* -autocluster data is now much more up-to-date. :ticket:`6833`

-  In order to work better with HTCondor 8.9.1 and later, remove support
   for remote submission to *condor_schedd* s older than version
   7.5.0. :ticket:`6844`

-  Fixed a bug that would cause DAGMan jobs to fail when using Kerberos
   Authentication on Debian or Ubuntu. :ticket:`6917`

-  Fixed a bug that caused execute nodes to ignore config knob
   ``CREDD_POLLING_TIMEOUT``\ :index:`CREDD_POLLING_TIMEOUT`.
   :ticket:`6887`

-  Python binding API method Schedd.submit() and submitMany() now edits
   job ``Requirements`` expression to consider the job ad's
   ``RequestCPUs`` and ``RequestGPUs`` attributes. :ticket:`6918`

Version 8.8.0
-------------

Release Notes:

-  HTCondor version 8.8.0 released on January 3, 2019.

New Features:

-  Provides a new package: ``minicondor`` on Red Hat based systems and
   ``minihtcondor`` on Debian and Ubuntu based systems. This
   mini-HTCondor package configures HTCondor to work on a single
   machine. :ticket:`6823`

-  Made the Python bindings' ``JobEvent`` API more Pythonic by handling
   optional event attributes as if the ``JobEvent`` object were a
   dictionary, instead. See section `Python
   Bindings <../apis/python-bindings.html>`_ for details. :ticket:`6820`

-  Added job ad attribute ``BlockReadKbytes`` and ``BlockWriteKybtes``
   which describe the number of kbytes read and written by the job to
   the sandbox directory. These are only defined on Linux machines with
   cgroup support enabled for vanilla jobs. :ticket:`6826`

-  The new ``IOWait`` attribute gives the I/O Wait time recorded by the
   cgroup controller. :ticket:`6830`

-  *condor_ssh_to_job* is now configured to be more secure. In
   particular, it will only use FIPS 140-2 approved algorithms. :ticket:`6822`

-  Added configuration parameter ``CRED_SUPER_USERS``, a list of users
   who are permitted to store credentials for any user when using the
   *condor_store_credd* command. Normally, users can only store
   credentials for themselves. :ticket:`6346`

-  For packaged HTCondor installations, the package version is now
   present in the HTCondor version string. :ticket:`6828`

Bugs Fixed:

-  Fixed a problem where a daemon would queue updates indefinitely when
   another daemon is offline. This is most noticeable as excess memory
   utilization when a *condor_schedd* is trying to flock to an offline
   HTCondor pool. :ticket:`6837`

-  Fixed a bug where invoking the Python bindings as root could change
   the effective uid of the calling process. :ticket:`6817`

-  Jobs in REMOVED status now properly leave the queue when evaluation
   of their ``LeaveJobInQueue`` attribute changes from ``True`` to
   ``False``. :ticket:`6808`

-  Fixed a rarely occurring bug where the *condor_schedd* would crash
   when jobs were submitted with a ``queue`` statement with multiple
   keys. The bug was introduced in the 8.7.10 release. :ticket:`6827`

-  Fixed a couple of bugs in the job event log reader code that were
   made visible by the new JobEventLog Python object. The remote error
   and job terminated event did not read all of the available
   information from the job log correctly. :ticket:`6816`
   :ticket:`6836`

-  On Debian and Ubuntu systems, the templates for
   *condor_ssh_to_job* and interactive submits are no longer
   installed in ``/etc/condor``. :ticket:`6770`
