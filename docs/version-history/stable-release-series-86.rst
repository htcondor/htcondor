Stable Release Series 8.6
=========================

This is a stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
8.6.x releases. New features will be added in the 8.7.x development
series.

The details of each version are described below.

Version 8.6.13
--------------

Release Notes:

-  HTCondor version 8.6.13 released on October 31, 2018.

New Features:

-  Python bindings are now available on Debian 9, Ubuntu 16, and Ubuntu
   18. :ticket:`6751`
-  Grid-type cream jobs are now supported on all linux platforms.
   :ticket:`6799`

Bugs Fixed:

-  Fixed a bug in the Python classad module that caused the Python
   ``in`` operator to be case sensitive when used to see if a ClassAd
   contains a given attribute. :ticket:`6535`
-  Fixed a bug in the Python bindings that would leak one or more
   ClassAds each time the submit or submitMany methods of the schedd
   class were called without passing an explicit list to return the
   result ClassAds. :ticket:`6729`
-  Fixed a problem processing the whitelist of user modules for the
   Python bindings. :ticket:`6387`
-  Fixed a bug that caused output and error files with full path names
   to be transferred to the wrong directory on Windows. This would
   usually cause output file transfer to fail. :ticket:`6747`
-  Fixed a bug that could cause grid-type ``condor`` jobs under the grid
   universe to fail if the job's sandbox is transferred more than once.
   :ticket:`6791`
-  Fixed a bug where Singularity would not be usable if Docker was not
   installed. :ticket:`6772`
-  Fixed a bug where ``MOUNT_UNDER_SCRATCH`` would not be interpreted as
   an expression for Singularity jobs. :ticket:`6740`
-  Fixed a bug where Docker Universe jobs would not bind mount the
   scratch directory when the jobs did not transfer files. :ticket:`6776`
-  Fixed a bug where short-lived Docker jobs would report spuriously
   high usage. :ticket:`6796`
-  Fixed how job start timestamps are recorded in the job ad in the grid
   universe and the job router. :ticket:`6773`
-  Fixed a bug that resulted in core files on Windows being mostly empty
   when the files were made by the 64-bit Windows binaries. :ticket:`6801`
-  Fixed a bug in how jobs are sorted in priority order in the
   *condor_schedd* when any of these job attributes are set:
   ``PreJobPrio1``, ``PreJobPrio2``, ``PostJobPrio1``, and
   ``PostJobPrio2``. :ticket:`6800`
-  Fixed a bug where the negotiator would erroneously preempt some
   dynamic slots when ``ALLOW_PSLOT_PREEMPTION``
   :index:`ALLOW_PSLOT_PREEMPTION` is set. :ticket:`6793`
-  Updated the systemd unit file to start HTCondor daemons after NFS and
   the automounter was available. :ticket:`6794`
-  Fixed a bug which would cause the *condor_negotiator* to crash if
   the second argument of an ternary operator is omitted in a ``START``
   expression. ``(expression ?: value)`` :ticket:`6798`
-  Fixed a bug which would cause certain valid URLs not be recognized.
   This allows, for example, 's3' to be used as a custom file transfer
   plug-in. :ticket:`6722`
-  Fixed a bug in file transfer where jobs would go on hold if they
   created a domain socket, which the FileTransfer module would
   subsequently try (and fail) to send back to the submit machine.
   :ticket:`6744`
-  Prevented the combination of -forward and -since flags when calling
   condor_history, which is ambiguous as to how it should work.
   :ticket:`6417`

Version 8.6.12
--------------

Release Notes:

-  HTCondor version 8.6.12 released on August 1, 2018.

Known Issues:

-  Policies implemented by the startd may not function as desired while
   the machine is draining. Specifically, if the ``PREEMPT`` expression
   becomes true for a particular slot while a machine is draining, the
   corresponding job will not vacate the slot until draining completes.
   For example, this renders the policy macro
   ``HOLD_IF_MEMORY_EXCEEDED`` ineffective. This has been a problem
   since v8.6. :ticket:`6697`
-  Policies implemented by the startd may not function as desired while
   the startd is shutting down peacefully. Specifically, if the
   ``PREEMPT`` expression becomes true for a particular slot while the
   startd is shutting down peacefully, the corresponding job will never
   be vacated. For example, this renders renders the policy macro
   ``HOLD_IF_MEMORY_EXCEEDED`` ineffective. This has been a problem
   since v8.6. :ticket:`6701`

New Features:

-  Support for Debian 9, Ubuntu 16, and Ubuntu 18.

Bugs Fixed:

-  Fixed a memory leak when SSL authentication fails. :ticket:`6717`
-  Fixed a bug where a job transform with an invalid Requirements
   expression would not log an error, and would match all jobs as if
   Requirements had not been specified. :ticket:`6671`
-  Fixed a bug where *condor_qedit* would not allow attempts to change
   protected attributes even when the user had the right to change them.
   :ticket:`6674`
-  Fixed a bug that would prevent environment variables defined in a job
   submit file from appearing in jobs running in Singularity containers
   using Singularity version 2.4 and greater. :ticket:`6656`
-  Fixed a bug causing the shared port daemon to crash immediately when
   built and run on Fedora 28. :ticket:`6696`
-  Updated the systemd configuration to prevent the rare escape of a job
   from its cgroup. :ticket:`6708`
-  Fix a bug in the *condor_qsub* wrapper that prevented users from
   requesting more than one CPU core. :ticket:`6693`
-  Fixed a bug where *condor_transfer_data* always wrote the job user
   log in the initialdir, even if the original submit file specified a
   different directory. :ticket:`6695`
-  Blank lines in a security or user map file no longer generate an
   error message. :ticket:`6672`
-  Fixed a bug that prevented the *condor_starter* from determining the
   status of a VM that has previously been shut down. :ticket:`6704`
-  Fixed a bug where the *condor_gridmanager* would consider grid-type
   gce jobs as completed when a query of the instances' status failed.
   :ticket:`6712`
-  Fixed a bug where using the warning keyword in a submit file would
   cause the subsequent queue statement to be reported as invalid.
   :ticket:`6677`
-  Fixed a bug in *condor_preen* where it did not clean up core dump
   files. It now erases all core files that exceed a certain size
   (defined by ``PREEN_COREFILE_MAX_SIZE``), a certain age (defined by
   ``PREEN_COREFILE_STALE_AGE``) or a maximum number of core files per
   process (defined by ``PREEN_COREFILES_PER_PROCESS``). :ticket:`6540`

Version 8.6.11
--------------

Release Notes:

-  HTCondor version 8.6.11 released on May 10, 2018.

New Features:

-  The MSI installer for Windows now appends the directory needed to use
   the HTCondor Python bindings libraries into the ``PYTHONPATH``
   environment variable. :ticket:`6607`
-  If the user sets the environment variable ``OMP_NUM_THREADS`` to some
   value in the submit file, trust the user, and do not overwrite this
   environment variable to the actual number of provisioned CPUs when
   the job runs. :ticket:`6606`

Bugs Fixed:

-  Fixed a bug where *condor_submit* **-i** would enter the wrong
   Singularity container. :ticket:`6595`
-  When using configuration parameter ``SINGULARITY_TARGET_DIR`` to
   mount the job scratch directory into the Singularity container,
   update the ``X509_USER_PROXY`` environment variable to point to the
   proxy file's location inside the container. :ticket:`6625`
-  Corrected a bug which could cause the shared port daemon to hang if
   it had been restarted, HTCondor had been configured with an allowable
   port range, and that port range had filled up. :ticket:`6596`
-  Fixed a bug that caused TCP port exhaustion when running a large
   number of instances of the *condor_chirp_client* program. :ticket:`6627`
-  *condor_submit* **-i** jobs now track their resource usage as normal
   jobs do. :ticket:`6590`
-  Fixed a bug that prevented HTCondor from running jobs if HTCondor was
   started within a Docker container, or more generally, with a root
   user id, but without ``CAP_SYSADMIN``. :ticket:`6603`
-  Fixed a bug that caused corruption of the XferStatsLog. :ticket:`6608`
-  Fixed bugs in *condor_q* where the **-global** option would
   sometimes truncate the job cluster id and the **-hold** option would
   truncate the hold reason. :ticket:`6634`
   :ticket:`6641`
-  Fixed a bug where ``STARTD_CRON_JOBLIST`` was not ignoring duplicate
   entries. :ticket:`6604`
-  Fixed a bug when running inside a docker container that would prevent
   the master from started unless ``DISCARD_SESSION_KEYRING_ON_STARTUP``
   was set to false. :ticket:`6602`
-  Fixed a bug specific to the HTCondor Python bindings on Windows,
   where the call htcondor.reload_config() would fail to see
   environment variable changes made by the Python program. :ticket:`6610`
-  DAGMan did not previously check the user log file (which it depends
   on for coordination with the *condor_schedd*) for corruption. Now,
   it checks to see if the user log file has been overwritten or
   deleted, and if so, exits immediately with an error. :ticket:`6579`
-  Fixed a bug in the ReadUserLog class where it failed to detect if a
   file file has been overwritten. :ticket:`6582`
-  Fixed a bug where *condor_submit* would not add needed file transfer
   plugins to the Requirements expression when should_transfer_files
   was ``IF_NEEDED``, which is the default. :ticket:`4692`
-  Fixed a bug where the configuration parameter
   ``STARTD_RECOMPUTE_DISK_FREE`` was not honored when creating a
   dynamic slot from a partitionable slot, which would sometimes result
   in the dynamic slot being provisioned with not enough disk space and
   then failing to match the job. :ticket:`6614`
-  Fixed a bug that caused the job ad attribute
   ``JobCurrentStartTransferOutputDate`` to be set incorrectly. :ticket:`6617`
-  Fixed a bug that could cause ``RemoteWallClockTime`` to have the
   wrong value in the history file. :ticket:`6626`
-  The *condor_schedd* now considers custom machine resources when
   selecting the next job to run on an idle claimed dynamic slot.
   :ticket:`6630`
-  The attribute ``SlotType`` is now set correctly in the slot ad when
   the *condor_schedd* is selecting the next job to run on a an idle
   claimed dynamic slot. :ticket:`6611`
-  Fixed a bug where *condor_submit* with the **-spool** or **-remote**
   option would fail when there were no input files to transfer.
   :ticket:`6655`
-  Fixed a bug that could cause the *condor_gridmanager* to falsely
   believe that grid-type boinc jobs were submitted to the BOINC server.
   :ticket:`6669`
-  Fixed a bug that could cause the HOLD column to be missing from
   *condor_q* output when the **-global** option was used. :ticket:`6661`
-  Fixed a bug that caused the *condor_collector* to reject accounting
   ads when configuration parameter ``COLLECTOR_REQUIREMENTS`` is in
   use. :ticket:`6673`
-  Updated the systemd configuration to set the ``TasksMax`` and
   ``LimitNOFile`` to unlimited. Under some versions of systemd, the
   ``TasksMax`` defaults to 512, which is too small for a busy submit
   host. :ticket:`6645`
-  Reduced the ``RPATH`` in RPM builds to just the needed directories.
   Previously, the tarball ``RPATH`` was used. :ticket:`6662`
-  On the Windows platform, the HTCondor daemons will attempt a
   ``NETWORK`` login to impersonate a user if the ``INTERACTIVE`` login
   fails. :ticket:`6640`

Version 8.6.10
--------------

Release Notes:

-  HTCondor version 8.6.10 released on March 13, 2018.

New Features:

-  None.

Bugs Fixed:

-  Fixed a bug that caused *condor_preen* to crash before it finished
   cleaning the spool directory and leave a core file of its own in the
   log directory. This problem occurred on submit nodes that had running
   jobs when *condor_preen* was invoked. :ticket:`6521`
-  Improved the systemd configuration to clean up HTCondor processes on
   shutdown in the event that the *condor_master* fails to do so.
   :ticket:`6539`
-  HTCondor daemons will do fast shutdown whenever their parent process
   exits unexpectedly. :ticket:`6539`
-  Fixed a bug that would cause *condor_q* to crash if the hostname was
   longer than 64 bytes. :ticket:`6594`
-  Fixed a bug where if an administrator configured a Concurrency Limit
   whose name ended in a number, *condor_userprio* **-allusers** would
   show additional bogus user entries. :ticket:`6542`
-  Fixed a bug where the *condor_starter* would crash when talking to a
   shadow running a condor version older than 8.5 and match
   authentication was enabled. :ticket:`6520`
-  Fixed a bug in Python API *htcondor.Secman().ping()* method which
   would sometimes result in a RunTimeError exception. :ticket:`6562`
-  Fixed a bug where ``policy: want_hold_if`` would always evict
   standard universe jobs instead of putting them on hold. Instead, this
   policy now ignores standard universe jobs entirely. This means that
   the metaknobs ``policy: hold_if_memory_exceeded`` and
   ``policy: hold_if_cpus_exceeded`` will also ignore standard universe
   jobs entirely (instead of its previous bad behavior of of letting
   standard universe jobs use more than their requested memory until the
   first time they were evicted, whereafter each restart would be
   immediately evicted). :ticket:`6583`
-  The metaknob ``policy: hold_if_memory_exceeded`` and
   ``policy: preempt_if_memory_exceeded`` now ignore VM universe jobs.
   These jobs can't exceed their requested memory. :ticket:`6583`
-  Fixed a bug which mischaracterized the ``MemoryUsage`` of VM universe
   jobs. This should allow VM universe jobs to run when
   ``feature: Hold_If_Memory_Exceeded`` is enabled. :ticket:`6577`
-  Fixed a bug where the *condor_shadow* could accidentally kill itself
   by not checking if it was attempting to change immutable attributes.
   :ticket:`6557`
-  Fixed a bug that could cause the *condor_collector* to exit with an
   assertion error under certain (rare) conditions when it has no
   outgoing connectivity to the Internet. :ticket:`6511`
-  Fixed a bug that would cause any daemons interfacing with the CREDMON
   to retry indefinitely when polling for credentials. :ticket:`6523`
-  Fixed a bug that prevented grid-type batch jobs from being removed
   after an attempt to submit to the underlying batch system failed.
   :ticket:`6586`
-  Fixed a bug in Python plugin support for the *condor_collector* that
   would result in the *condor_collector* switching from writing from
   the CollectorLog to writing to the ToolLog after a reconfig. :ticket:`6588`
-  Fixed a bug in the $F() macro expansion in submit and configuration
   files that would cause a crash if the argument to the macro was a
   file literal rather than a variable name. :ticket:`6531`
-  Fixed a bug that allowed the *condor_schedd* to attempt to run jobs
   on a dynamic slot that requested more resources than the slot
   provided. :ticket:`6593`

Version 8.6.9
-------------

Release Notes:

-  HTCondor version 8.6.9 released on January 4, 2018.

New Features:

-  When a daemon crashes, more information about the cause is now
   written to its log file. :ticket:`6483`

Bugs Fixed:

-  Fixed a bug in the group quotas that would give too much surplus
   quota to some groups when ``ACCEPT_SURPLUS`` is on and
   ``NEGOTIATOR_ALLOW_QUOTA_OVERSUBSCRIPTION`` is true (the default)
   :ticket:`6514`
-  Fixed a bug in the Python bindings when doing queries that specify a
   projection with the "attr_list" argument. The bug could could
   potentially result in memory corruption of the Python interpreter
   process. :ticket:`6468`
-  Reduced the amount of time that *condor_preen* will block the
   *condor_schedd*. *condor_preen* now connects only when specifically
   needed, and automatically disconnects after
   ``PREEN_MAX_SCHEDD_CONNECTION_TIME`` seconds. :ticket:`6490`
-  Fixed a bug on Windows that would often result in the job sandbox on
   the execute node not being deleted when the *condor_schedd*
   relinquished its claim on the slot before the *condor_starter* had
   exited. :ticket:`6497`
-  Fixed a bug where the *condor_master* stopped sending watchdog
   notifications to systemd after restarting itself. This resulted in
   systemd killing the *condor_master* shortly after the restart.
   :ticket:`6476`
-  Updated the systemd configuration to only restart HTCondor upon
   failure. Otherwise, systemd would restart HTCondor if *condor_off*
   requested the *condor_master* to exit. :ticket:`6503`
-  Fixed a bug with the use of the scheduler parameter
   ``MAX_JOBS_SUBMITTED``. If this limit was ever reached by a submit
   with more than one proc in the cluster, the limit would be reduced by
   the difference until the *condor_schedd* was restarted. :ticket:`6460`
-  Fixed a bug that caused very large RequestDisk requests to fail, and
   cause the Disk attribute in the machine ad to go negative. :ticket:`6467`
-  Fixed a bug with the ``RESERVED_DISK`` parameter that would not
   accept an argument larger than 2 Gigabytes. :ticket:`6472`
-  Improved validation of the lengths of messages in ``PASSWORD`` and
   ``SSL`` authentication methods. :ticket:`6493`
-  Fixed a problem where the VM universe would be taken offline on the
   execute node, if the qcow2 disk image was corrupt. The offending job
   is now put on hold with an appropriate hold message. :ticket:`6505`
-  Fixed a problem which would prevent Java universe jobs from working
   when using a relative path name to a jar file and submitting from
   Linux to Windows or vice versa. :ticket:`6474`
-  Fixed a bug on 32 bit Linux systems that caused the starter to crash
   on startup if cgroup limits were enabled. :ticket:`6501`
-  Fixed a bug in Startd Cron (see
   `Hooks <../misc-concepts/hooks.html>`_) where, in effect,
   ``SlotMergeConstraint`` was ignored. :ticket:`6488`
-  Fixed a bug when IPv6 is enabled which could cause the
   *condor_startd* to crash when spawning a starter. :ticket:`6462`
-  Fixed a bug in *condor_q* which could cause the DONE amount to be
   incorrect when multiple clusters shared a batch name. :ticket:`6469`
-  Fixed issue on newer versions of Linux where core files generated by
   a daemon were not usable by gdb. A side effect of this fix is that
   the configuration parameter ``CORE_FILE_NAME`` no longer has any
   effect on Linux. :ticket:`6482`
-  *condor_chirp* will now no longer abort when given a command that it
   cannot successfully execute, such as fetching a file that does not
   exist. :ticket:`6402`
-  Removed unneeded ``copy_to_spool`` statement from default interactive
   submit file. :ticket:`6315`

Version 8.6.8
-------------

Release Notes:

-  HTCondor version 8.6.8 released on November 14, 2017.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes a security-related
   bug described at
   `http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html>`_.
   :ticket:`6455`

Version 8.6.7
-------------

Release Notes:

-  HTCondor version 8.6.7 released on October 31, 2017.

New Features:

-  Added support for HTTPS transfers in the ``curl_plugin`` utility.
   :ticket:`6253`
-  Job attributes that are recognized by the *batch_gahp* but not by
   HTCondor can now be specified in the job ad without using a prefix of
   ``Remote_``. :ticket:`6422`

Bugs Fixed:

-  Fixed a bug that caused systems using cgroup memory limits to not
   properly reset the memory limit after the first use of a slot. The
   memory limit would get reused from the previous slot value. :ticket:`6414`
-  Added SELinux type enforcement rules to allow *condor_ssh_to_job*
   to function on Enterprise Linux 7. :ticket:`6362`
-  Asking systemd to stop condor now allows the HTCondor daemons to
   properly clean up, instead of simply immediately sending a SIGKILL.
   As a result, HTCondor daemons stopped via systemd will no longer
   continue to appear alive with *condor_status*. :ticket:`6096`
-  Fixed problems in Python bindings when using the Submit class to
   submit jobs specifying environment variables or file redirection.
   :ticket:`6420`
-  Change the default value of STARTD_RECOMPUTE_DISK_FREE to false,
   so that the Disk attribute is mostly correct for partitionable slots.
   :ticket:`6424`
-  Docker universe now sets the cgroup cpu-shares field to 100 times the
   number of requested cores, which matches vanilla universe. :ticket:`6423`
-  MOUNT_UNDER_SCRATCH when used in Docker universe can now be an
   expression, not just a literal string. This matches the way it works
   in vanilla universe. :ticket:`6401`
-  Fixed a bug that could cause the *condor_startd* to crash when
   spawning a *condor_starter* with mixed mode networking. :ticket:`6461`
-  Fixed a bug that caused the *condor_collector* on Windows to refuse
   connections whenever the number of open sockets was more than 820
   even though space was allocated for 1024 open sockets. :ticket:`6425`
-  Fixed a bug that caused the configuration variable
   ``DEFAULT_MASTER_SHUTDOWN_SCRIPT`` to be ignored on Windows when the
   *condor_master* was running as a service. :ticket:`6458`
-  *condor_status* will now print longer lines when its output is
   redirected into a pipe, rather than its input coming from one.
   :ticket:`6381`
-  Fixed a crash in *condor_transferer* when a connection can't be
   established with its peer. :ticket:`6412`
-  Fixed a bug that caused *condor_job_router_info* to crash if
   configuration parameter ``JOB_ROUTER_ENTRIES_REFRESH`` was set to a
   positive value. :ticket:`6444`
-  Fixed a bug in *condor_history* that caused it to print invalid XML
   or JSON syntax when reading from multiple history files. :ticket:`6437`
-  Fixed a bug in the *condor_schedd* which resulted in the
   ``IsNoopJob`` job attribute sometimes being ignored if the the value
   of this attribute was changed after the job was submitted. :ticket:`6396`
-  Fixed a bug that rarely caused slurm jobs to be held. When slurm
   reports memory utilization and it is a multiple of 1024k, Slurm uses
   the 'M' suffix. The parsing logic was extended to also interpret the
   'M', 'G', 'T', and 'P' suffixes for memory utilization. :ticket:`6431`
-  The condor-bosco RPM ensures the *rsync* is installed as required by
   the Bosco scripts. :ticket:`6439`
-  To avoid unnecessary transfers when ``copy_to_spool`` is set in the
   submit file, HTCondor no longer copies the executable to the local
   spool directory more than once for a cluster. :ticket:`6454`

Version 8.6.6
-------------

Release Notes:

-  HTCondor version 8.6.6 released on September 12, 2017.

New Features:

-  None.

Bugs Fixed:

-  Fixed a bug that might cause the *condor_schedd* or other daemons to
   crash when logging on Linux to the syslog facility, and the
   *condor_reconfig* command was run. :ticket:`6364`
-  Fixed a bug that prevented condor daemons from writing out a core
   file for debugging in the very unlikely event that one of them
   crashed. :ticket:`6365`
-  Fixed a bug where the negotiator would make matches where the daemons
   involved did not share an IP version, and thus could not talk to each
   other. :ticket:`6351`
-  HTCondor now works properly with systemd's watchdog feature on all
   flavors of Linux. Previously, the *condor_master* wouldn't send
   alive messages to systemd if systemd wasn't part of the Linux
   distribution's standard configuration. This would result in systemd
   killing the HTCondor daemons after a short period of time. :ticket:`6385`
-  Fixed handling of backslashes in string values in old ClassAds format
   in the Python bindings. :ticket:`6382`
-  Fixed a bug in how the CPU usage of Slurm jobs is interpreted.
   :ticket:`6380`
-  Fixed a bug that caused a machine claimed by a parallel universe job
   to stick in the Claimed/Idle state forever. This could only happen if
   the job was removed as it was in the process of claiming resources.
   :ticket:`6376`
-  Fixed a bug that caused a machine to stick in the Preempting/Vacating
   state after a job was removed when a ``JOB_EXIT_HOOK`` was defined.
   :ticket:`6383`
-  Added type enforcement rules for cgroups to HTCondor's SELinux
   profile. :ticket:`6168`
-  Fixed a bug where setting ``delegate_job_gsi_credentials_lifetime``
   to 0 in a submit description file was treated the same as not setting
   it at all. :ticket:`6375`
-  Fixed handling of octal escape sequences in ClassAd strings. :ticket:`6384`
-  Updated Boost external to version 1.64. :ticket:`6369`

Version 8.6.5
-------------

Release Notes:

-  HTCondor version 8.6.5 released on August 1, 2017.

New Features:

-  Added avx2 to the set of processor flags advertised by the
   *condor_startd*. :ticket:`6317`

Bugs Fixed:

-  Fixed a bug in socket clean-up that was causing a memory leak. This
   may have been particularly noticeable in the *condor_collector*.
   :ticket:`6342`
-  Fixed a bug that caused an infinite loop in the *condor_starter*
   when cgroups were enabled on systems (such as Debian) where the
   kernel has disabled the memory accounting controller. A job on such a
   system would go into the "R" state, but never actually start running.
   :ticket:`6347`
-  Fixed a bug where setting ``NETWORK_INTERFACE`` to an IPv6 address
   could cause HTCondor daemons to except. :ticket:`6339`
-  Fixed a bug where a cross protocol CCB connection would cause the
   *condor_shadow* or *condor_schedd* to except. :ticket:`6344`
-  Fixed a bug where the wildcard '\*' in ALLOW or DENY lists was being
   interpreted as only matching IPv4 addresses. It now properly matches
   any address family. :ticket:`6340`
-  Fixed a bug where reverse resolutions could return the string
   representation of the address in question instead of failing. This
   resulted in spurious warnings of the form "WARNING: forward
   resolution of 2001:630:10:f001::19a0 doesn't match
   2001:630:10:f001::19a0!" :ticket:`6338`
-  Fixed a bug which prevented using an ImDisk RAM disk as the execute
   directory on Windows. :ticket:`6324`
-  Fixed a bug where removal of a job could cause another job from the
   same user to also be removed. This was mostly likely to happen when
   the *condor_schedd* is under heavy load. :ticket:`6353`
-  Fixed a bug that cause parallel universe jobs not to start on pools
   with partitionable slots. :ticket:`6308`
-  Fixed a problem, introduced in HTCondor 8.6.4, where the
   *condor_collector* plugins where loaded but not used. :ticket:`6343`
-  Fixed a bug where "*condor_q* **-grid**" did not display the status
   column for any non-Globus job. :ticket:`6306`
-  Fixed bugs in the *condor_schedd* and *condor_negotiator* that
   could cause jobs to not be negotiated for when
   ``NEGOTIATOR_PREFETCH_REQUESTS`` is set to ``TRUE``. :ticket:`6336`
   :ticket:`6312`

Version 8.6.4
-------------

Release Notes:

-  HTCondor version 8.6.4 released on June 22, 2017.

New Features:

-  Python bindings are now available on MacOSX. :ticket:`6244`
-  Allow Python modules to be used as *condor_collector* plugin. This
   undocumented feature is to be used by expert developers only.
   :ticket:`6213`
   :ticket:`6295`

Bugs Fixed:

-  Fixed a bug with PASSWORD authentication that would sporadically
   cause it to fail to exchange keys, due to whether or not the first
   round-trip of communications blocked on reading from the network.
   :ticket:`6265`
-  Pslot preemption now properly handles machine custom resources, such
   as GPUs. :ticket:`6297`
-  Fixed a bug that prevented HTCondor from correctly setting virtual
   memory cgroup limits when soft physical memory limits were being
   used. :ticket:`6294`
-  Fixed a bug that prevented parallel universe jobs from running that
   used $$() expansion in submit files. :ticket:`6299`
-  Added a new knob, ``STARTD_RECOMPUTE_DISK_FREE``, which defaults to
   true, which tells the startd to periodically recompute and advertise
   free disk space. Admins can set this to false for partitionable slots
   whose execute directory is used by HTCondor alone. :ticket:`6301`
-  Fixed a bug that could cause *condor_submit* to fail to submit a job
   with a proxy file to a *condor_schedd* older than 8.5.8, due to the
   absence of an X.509 CA certificates directory. :ticket:`6258`
-  Restored a check in *condor_submit* about whether the job's X.509
   proxy has sufficient lifetime remaining. :ticket:`6283`
-  Fixed a bug in *condor_dagman* where the DAG status file showed an
   incorrect status code if submit attempts failed for the final node.
   :ticket:`6069`
-  Bosco now properly identifies CentOS 7 as a supported platform.
   :ticket:`6303`
-  Fixed a bug when Bosco is used to submit jobs to multiple remote
   clusters. When arguments to *remote_gahp* are provided in the
   GridResource attribute, jobs could be submitted to the wrong cluster.
   :ticket:`6277`
-  To speed up the installation process on Enterprise Linux 7, the
   SELinux profile is now reloaded only once, when setting the HTCondor
   daemons to run in permissive mode. :ticket:`6304`
-  Update the systemd configuration on Enterprise Linux 7 to start the
   *condor_master* after time synchronization is achieved. This
   prevents unnecessary daemon restarts due to sudden time shifts.
   :ticket:`6255`
-  The *condor_shadow* will now ignore updates of ``JobStartDate`` from
   the *condor_starter* since the *condor_schedd* already sets this
   attribute correctly and the *condor_starter* incorrectly tries to
   set it even if the job has already run once. A consequence of this
   fix is that the value of ``JobStartDate`` that the *condor_startd*
   uses for policy expressions will be different than the value that the
   *condor_schedd* uses. Resolving this problem will potentially break
   existing policy expressions in the *condor_startd*, so it will be be
   not be changed in the 8.6 series, but fixed in the 8.7 series.
   :ticket:`6280`
-  Fixed a bug where per-instance job attributes like ``RemoteHost``
   would show up in the history file for completed jobs. This bug
   occurred if a job happened to complete while the *condor_schedd* was
   in the process of a graceful shutdown. :ticket:`6251`
-  The *condor_convert_history* command is present again in this
   release. :ticket:`6282`
-  The parameter ``SETTABLE_ATTRS_ADMINISTRATOR`` is now correctly
   appears in *condor_config_val*. :ticket:`6286`

Version 8.6.3
-------------

Release Notes:

-  HTCondor version 8.6.3 released on May 9, 2017.

Bugs Fixed:

-  Fixed a bug that rarely corrupts the *condor_schedd* 's job queue
   log file when the input sandbox of a job with an X.509 proxy file is
   spooled. :ticket:`6240`
-  Fixed a memory leak in the Python bindings related to logging.
   :ticket:`6227`

Version 8.6.2
-------------

Release Notes:

-  HTCondor version 8.6.2 released on April 24, 2017.

New Features:

-  Added metaknobs for defining map files for use with the ClassAd
   usermap function in the *condor_schedd*, and a metaknob for
   automatically assigning an accounting group to a job based on a
   mapping of the owner name of the job. :ticket:`6179`
-  When the *condor_credd* is polling for credentials, the timeout is
   now configurable using ``CREDD_POLLING_TIMEOUT``.
-  The **reverse** option for *condor_q* was changed to
   **reverse-analyze**, and it now implies **better-analyze**. Formerly,
   the **reverse** option was ignored unless **-better-analyze** was
   also specified. :ticket:`6167`

Bugs Fixed:

-  Fixed a bug that could cause *condor_store_cred* to fail on Windows
   due to a case-sensitive check of the user's account name. :ticket:`6200`
-  Updated Open MPI helper script to catch and handle SIGTERM and to use
   bash explicitly. :ticket:`6194`
-  Docker Universe jobs now update the RemoteSysCpu attributes for job
   and in the job log. Previously, this field was always 0. :ticket:`6173`
-  Docker universe detection is now more robust in the face of
   extraneous output to standard error on docker startup. This was
   preventing Condor from detecting that docker was properly working on
   hosts. :ticket:`6185`
-  Fixed a bug that prevented ``SUBMIT_REQUIREMENT`` and
   ``JOB_TRANSFORM`` expressions from referencing job attributes
   describing the job's X.509 proxy credential. :ticket:`6188`
-  The Linux kernel tuning script no longer adjusts some kernel
   parameters unless a *condor_schedd* will be started by the master.
   :ticket:`6208`
-  Fixed a bug that caused all but the first in a list of metaknobs to
   be ignored unless there were commas separating the list items. So
   ``use ROLE : Execute CentralManager`` would incorrectly add only the
   Execute role. Previously, ``use ROLE : Execute, CentralManager``
   would correctly add both roles. :ticket:`6171`
-  Worked around a problem with FORTRAN programs built with
   *condor_compile* and recent versions of gfortran (4.7.2 was OK,
   4.8.5 was not), where those executables would not write to standard
   out if started in the standard universe. Also, updated the
   checkpointing library to permit *condor_compile* to successfully
   link FORTRAN (and other) programs calling certain math functions and
   built against up-to-date versions of glibc. :ticket:`6026`
-  The default values for ``HAD_SOCKET_NAME`` and
   ``REPLICATION_SOCKET_NAME`` have changed to enable the documented
   configuration for using these services with shared port to work.
   :ticket:`6186`
-  Fixed a bug that caused *condor_dagman* to sometimes (rarely, but
   repeatably) crash when parsing DAGs containing splices. :ticket:`6170`
-  The configuration parameters that control when job policy expressions
   are evaluated now work as documented. Previously, the default value
   for ``PERIODIC_EXPR_INTERVAL`` was 300, not 60 as intended. Also, the
   parameters ``MAX_PERIODIC_EXPR_INTERVAL`` and
   ``PERIODIC_EXPR_TIMESLICE`` were ignored for grid universe jobs.
   :ticket:`6199`
-  Fixed a bug that could cause the Job Router to crash if the
   ``job_queue.log`` contained invalid or incomplete records. :ticket:`6195`
-  Fixed a bug that caused updates of the job attribute
   ``x509UserProxyExpiration`` to be ignored for job policy evaluation
   when the job was managed by the Job Router. :ticket:`6209`
-  Changed the default value of configuration parameters
   ``CREAM_GAHP_WORKER_THREADS`` to the value of
   ``GRIDMANAGER_MAX_PENDING_REQUESTS``. This should prevent a back-log
   of commands in the CREAM GAHP observed by some users. :ticket:`6071`
-  Fixed modification of ``PYTHONPATH`` environment variable that could
   fail in bash if *set -u* is enabled. :ticket:`6211`
-  *bosco_quickstart* no longer assumes that submitting to a Slurm
   cluster requires the PBS emulation module. :ticket:`6211`
-  Fixed a bug that caused *condor_submit* **-dump** to crash when the
   submit file had an attribute to enable the use of an x509 user proxy.
   :ticket:`6197`
-  Updated the supported platform list in the Bosco installer script to
   include Ubuntu 16 and Mac OSX 10.12. Also, dropped Ubuntu 12 and Mac
   OSX 10.6 through 10.9. :ticket:`6178`
-  Fixed a bug which in some obscure configurations caused a spurious
   PERMISSION DENIED error was printed in the StartLog when activating a
   claim. :ticket:`6172`.
-  Fixed a bug which forced the administrator to restart (rather than
   reconfigure) running daemons after adding an entry to a ``DENY_*``
   authorization list. :ticket:`6172`.

Version 8.6.1
-------------

Release Notes:

-  HTCondor version 8.6.1 released on March 2, 2017.

New Features:

-  *condor_q* now checks to see if authentication and security
   negotiation are enabled before attempting to request only the current
   users jobs from the *condor_schedd*. Prior to this change,
   configurations that disabled security or authentication would also
   need to set ``CONDOR_Q_ONLY_MY_JOBS`` to false. :ticket:`6125`
-  The CLAIMTOBE authentication method is now in the list of methods for
   READ access if no list of authentication methods for READ or DEFAULT
   is specified in the configuration. This change allows sites that use
   the default host based security model to use *condor_q* **-global**
   with the only-my-jobs feature without making changes to their
   security configuration. :ticket:`6125`
-  The collector now records the authentication method used to determine
   the authenticated identity. :ticket:`6122`

Bugs Fixed:

-  Update Docker interface to be able to retrieve usage information from
   running containers and to remove containers when certain errors
   occurred when using Docker version 1.13. :ticket:`6088`
-  In Docker universe, all writes to files in ``/tmp`` and ``/var/tmp``
   by default write inside the container. There is a limit on the file
   size within the container, and jobs that write a lot to ``/tmp`` may
   hit that. If a docker universe job now runs on a system with
   ``MOUNT_UNDER_SCRATCH`` defined, HTCondor now adds those mounts as
   volume mounts, so file writes do not go to the container, but to the
   host file system. :ticket:`6080`
-  Fixed a bug in *condor_status* **-format** and *condor_q*
   **-format** that caused the tools to truncate output to the width
   specified in the format specifier. The most likely manifestation of
   this bug was that punctuation after the format would not be printed
   when the format had an explicit width. :ticket:`6120`
-  Fixed a bug that caused spurious shared port-related error messages
   to appear in the ``dagman.out`` file (by adding the new
   ``DAGMAN_USE_SHARED_PORT`` configuration macro). :ticket:`6156`
-  Fixed a bug that caused VM universe jobs to fail if the **vm_disk**
   submit command contained spaces after a comma. :ticket:`6132`
-  Fixed a bug that can cause the Job Router and *condor_c-gahp* to
   crash if they fail to submit a job due to submit transforms or submit
   requirements. :ticket:`6152`
-  Fixed a bug that caused the Job Router to not route any jobs if the
   ``JOB_ROUTER_DEFAULTS`` configuration parameter value started with
   white space. :ticket:`6128`
-  Fixed several bugs in how the Job Router writes to job event logs.
   :ticket:`6092`
-  Removed Bosco's attempt to configure a default value for
   **grid_resource** in the submit description file, as
   *condor_submit* no longer supports this ability. Also, Bosco now
   works with Slurm clusters. :ticket:`6106`
-  Changed Bosco's configuration of the *condor_ft-gahp* to eliminate
   worrying error messages in the *condor_ft-gahp* 's log file.
   :ticket:`6107`
-  Fixed a bug that could cause a grid batch job submitted to PBS or
   Slurm to go on hold when the job's X.509 proxy is refreshed. :ticket:`6136`
-  Fixed a bug where the *condor_gridmanager* fails to put a job on
   hold due to the desired hold reason containing invalid characters.
   :ticket:`6142`
-  Improved the hold reason when submission of a grid-type batch job
   fails. :ticket:`3377`
-  Update helper scripts to work with current versions of Open MPI and
   MPICH2. :ticket:`6024`
-  Fixes a bug that could cause events for local universe jobs to not be
   written to the global event log. :ticket:`6100`
-  Fixed a bug on execute machines that enable PID namespaces that would
   generate a spurious error message in the daemon log when
   *condor_off* -fast was issued. :ticket:`6137`
-  Fixed a bug that could corrupt the job queue log file such that the
   *condor_schedd* cannot restart. The bug is mostly likely to occur if
   the disk becomes full. :ticket:`6153`
-  Incremented the ClassAd library version number, since the deprecated
   iostream interface has been removed. :ticket:`6050`
   :ticket:`6115`

Version 8.6.0
-------------

Release Notes:

-  HTCondor version 8.6.0 released on January 26, 2017.

New Features:

-  Added two new job ClassAd attributes, ``CumulativeRemoteSysCpu`` and
   ``CumulativeRemoteUserCpu``, which keep a running total of system and
   user CPU usage, respectively, across all job restarts. Also,
   immediately clear attributes ``RemoteSysCpu`` and ``RemoveUserCpu``
   on job start, instead of on first update. :ticket:`6022`
-  Added a new configuration knob, ``ALWAYS_REUSEADDR``, which defaults
   to ``True``. When ``True``, it tells HTCondor to set the
   ``SO_REUSEADDR`` socket option, so that the schedd can run large
   numbers of very short jobs without exhausting the number of local
   ports needed for shadows. :ticket:`6040`
-  Changed the default value of ``IGNORE_LEAF_OOM`` to ``True``.
   :ticket:`5775`

Bugs Fixed:

-  Fixed a bug causing unnecessarily slow updates from the
   *condor_startd*. If you depend on the old behavior, set
   ``UPDATE_SPREAD_TIME`` to 8. A value of 0 enables the fix. :ticket:`6062`
-  Fixed a race condition when running multiple concurrent jobs on the
   same claim. When the starter exits, it notifies the shadow, which
   tells the startd to kill the starter. Immediately after the shadows
   tells the startd, it fetches the next job, and tries to start it. If
   the starter hasn't completely exited yet (perhaps it needs to clean
   up a large sandbox), it will notice the shadow has closed the command
   socket, and the starter will go into disconnected mode, and get
   confused. This has been fixed. :ticket:`6049`
-  Fixed an infelicity with *condor_submit* -i and docker universe,
   where it would start an interactive shell without a container. Added
   error message expressing that this combination is not currently
   supported. :ticket:`6083`
-  When a job claimed by the Job Router is held or removed, it is no
   longer considered a failure of the job route chosen for that job.
   :ticket:`5968`
-  Fixed a bug in recovering a Google Compute Engine (GCE) job if the
   *condor_gridmanager* restarts during submission of the instance
   request. :ticket:`6078`
-  Fixed a bug that could cause re-installation of a remote cluster to
   fail in Bosco. :ticket:`6042`
-  Fixed a bug with handling the proxy files of grid-type batch jobs
   when the proxy's file name is a relative path. :ticket:`6053`
-  Fixed a bug that caused the *batch_gahp* to crash when a job's X.509
   proxy is refreshed and the *batch_gahp* is configured to not create
   a limited copy of the proxy. :ticket:`6051`
-  Fixed a bug in the virtual machine universe where ``RequestMemory``
   and ``RequestCPUs`` were not changing the resources assigned to the
   VM created by HTCondor. Now, ``VM_Memory`` defaults to
   ``RequestMemory``, and the number of CPUs defaults to
   ``RequestCPUs``. :ticket:`5998`


