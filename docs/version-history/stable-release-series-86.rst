      

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
   18. `(Ticket
   #6751). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6751>`__
-  Grid-type cream jobs are now supported on all linux platforms.
   `(Ticket
   #6799). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6799>`__

Bugs Fixed:

-  Fixed a bug in the Python classad module that caused the Python
   ``in`` operator to be case sensitive when used to see if a ClassAd
   contains a given attribute. `(Ticket
   #6535). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6535>`__
-  Fixed a bug in the Python bindings that would leak one or more
   ClassAds each time the submit or submitMany methods of the schedd
   class were called without passing an explicit list to return the
   result ClassAds. `(Ticket
   #6729). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6729>`__
-  Fixed a problem processing the whitelist of user modules for the
   Python bindings. `(Ticket
   #6387). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6387>`__
-  Fixed a bug that caused output and error files with full path names
   to be transferred to the wrong directory on Windows. This would
   usually cause output file transfer to fail. `(Ticket
   #6747). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6747>`__
-  Fixed a bug that could cause grid-type ``condor`` jobs under the grid
   universe to fail if the job’s sandbox is transferred more than once.
   `(Ticket
   #6791). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6791>`__
-  Fixed a bug where Singularity would not be usable if Docker was not
   installed. `(Ticket
   #6772). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6772>`__
-  Fixed a bug where ``MOUNT_UNDER_SCRATCH`` would not be interpreted as
   an expression for Singularity jobs. `(Ticket
   #6740). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6740>`__
-  Fixed a bug where Docker Universe jobs would not bind mount the
   scratch directory when the jobs did not transfer files. `(Ticket
   #6776). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6776>`__
-  Fixed a bug where short-lived Docker jobs would report spuriously
   high usage. `(Ticket
   #6796). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6796>`__
-  Fixed how job start timestamps are recorded in the job ad in the grid
   universe and the job router. `(Ticket
   #6773). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6773>`__
-  Fixed a bug that resulted in core files on Windows being mostly empty
   when the files were made by the 64-bit Windows binaries. `(Ticket
   #6801). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6801>`__
-  Fixed a bug in how jobs are sorted in priority order in the
   *condor\_schedd* when any of these job attributes are set:
   ``PreJobPrio1``, ``PreJobPrio2``, ``PostJobPrio1``, and
   ``PostJobPrio2``. `(Ticket
   #6800). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6800>`__
-  Fixed a bug where the negotiator would erroneously preempt some
   dynamic slots when ``ALLOW_PSLOT_PREEMPTION`` is set. `(Ticket
   #6793). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6793>`__
-  Updated the systemd unit file to start HTCondor daemons after NFS and
   the automounter was available. `(Ticket
   #6794). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6794>`__
-  Fixed a bug which would cause the *condor\_negotiator* to crash if
   the second argument of an ternary operator is omitted in a ``START``
   expression. ``(expression ?: value)`` `(Ticket
   #6798). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6798>`__
-  Fixed a bug which would cause certain valid URLs not be recognized.
   This allows, for example, ‘s3’ to be used as a custom file transfer
   plug-in. `(Ticket
   #6722). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6722>`__
-  Fixed a bug in file transfer where jobs would go on hold if they
   created a domain socket, which the FileTransfer module would
   subsequently try (and fail) to send back to the submit machine.
   `(Ticket
   #6744). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6744>`__
-  Prevented the combination of -forward and -since flags when calling
   condor\_history, which is ambiguous as to how it should work.
   `(Ticket
   #6417). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6417>`__

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
   since v8.6. `(Ticket
   #6697). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6697>`__
-  Policies implemented by the startd may not function as desired while
   the startd is shutting down peacefully. Specifically, if the
   ``PREEMPT`` expression becomes true for a particular slot while the
   startd is shutting down peacefully, the corresponding job will never
   be vacated. For example, this renders renders the policy macro
   ``HOLD_IF_MEMORY_EXCEEDED`` ineffective. This has been a problem
   since v8.6. `(Ticket
   #6701). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6701>`__

New Features:

-  Support for Debian 9, Ubuntu 16, and Ubuntu 18.

Bugs Fixed:

-  Fixed a memory leak when SSL authentication fails. `(Ticket
   #6717). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6717>`__
-  Fixed a bug where a job transform with an invalid Requirements
   expression would not log an error, and would match all jobs as if
   Requirements had not been specified. `(Ticket
   #6671). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6671>`__
-  Fixed a bug where *condor\_qedit* would not allow attempts to change
   protected attributes even when the user had the right to change them.
   `(Ticket
   #6674). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6674>`__
-  Fixed a bug that would prevent environment variables defined in a job
   submit file from appearing in jobs running in Singularity containers
   using Singularity version 2.4 and greater. `(Ticket
   #6656). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6656>`__
-  Fixed a bug causing the shared port daemon to crash immediately when
   built and run on Fedora 28. `(Ticket
   #6696). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6696>`__
-  Updated the systemd configuration to prevent the rare escape of a job
   from its cgroup. `(Ticket
   #6708). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6708>`__
-  Fix a bug in the *condor\_qsub* wrapper that prevented users from
   requesting more than one CPU core. `(Ticket
   #6693). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6693>`__
-  Fixed a bug where *condor\_transfer\_data* always wrote the job user
   log in the initialdir, even if the original submit file specified a
   different directory. `(Ticket
   #6695). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6695>`__
-  Blank lines in a security or user map file no longer generate an
   error message. `(Ticket
   #6672). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6672>`__
-  Fixed a bug that prevented the *condor\_starter* from determining the
   status of a VM that has previously been shut down. `(Ticket
   #6704). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6704>`__
-  Fixed a bug where the *condor\_gridmanager* would consider grid-type
   gce jobs as completed when a query of the instances’ status failed.
   `(Ticket
   #6712). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6712>`__
-  Fixed a bug where using the warning keyword in a submit file would
   cause the subsequent queue statement to be reported as invalid.
   `(Ticket
   #6677). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6677>`__
-  Fixed a bug in *condor\_preen* where it did not clean up core dump
   files. It now erases all core files that exceed a certain size
   (defined by ``PREEN_COREFILE_MAX_SIZE``), a certain age (defined by
   ``PREEN_COREFILE_STALE_AGE``) or a maximum number of core files per
   process (defined by ``PREEN_COREFILES_PER_PROCESS``). `(Ticket
   #6540). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6540>`__

Version 8.6.11
--------------

Release Notes:

-  HTCondor version 8.6.11 released on May 10, 2018.

New Features:

-  The MSI installer for Windows now appends the directory needed to use
   the HTCondor Python bindings libraries into the ``PYTHONPATH``
   environment variable. `(Ticket
   #6607). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6607>`__
-  If the user sets the environment variable ``OMP_NUM_THREADS`` to some
   value in the submit file, trust the user, and do not overwrite this
   environment variable to the actual number of provisioned CPUs when
   the job runs. `(Ticket
   #6606). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6606>`__

Bugs Fixed:

-  Fixed a bug where *condor\_submit* **-i** would enter the wrong
   Singularity container. `(Ticket
   #6595). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6595>`__
-  When using configuration parameter ``SINGULARITY_TARGET_DIR`` to
   mount the job scratch directory into the Singularity container,
   update the ``X509_USER_PROXY`` environment variable to point to the
   proxy file’s location inside the container. `(Ticket
   #6625). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6625>`__
-  Corrected a bug which could cause the shared port daemon to hang if
   it had been restarted, HTCondor had been configured with an allowable
   port range, and that port range had filled up. `(Ticket
   #6596). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6596>`__
-  Fixed a bug that caused TCP port exhaustion when running a large
   number of instances of the *condor\_chirp\_client* program. `(Ticket
   #6627). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6627>`__
-  *condor\_submit* **-i** jobs now track their resource usage as normal
   jobs do. `(Ticket
   #6590). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6590>`__
-  Fixed a bug that prevented HTCondor from running jobs if HTCondor was
   started within a Docker container, or more generally, with a root
   user id, but without ``CAP_SYSADMIN``. `(Ticket
   #6603). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6603>`__
-  Fixed a bug that caused corruption of the XferStatsLog. `(Ticket
   #6608). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6608>`__
-  Fixed bugs in *condor\_q* where the **-global** option would
   sometimes truncate the job cluster id and the **-hold** option would
   truncate the hold reason. `(Ticket
   #6634). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6634>`__
   `(Ticket
   #6641). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6641>`__
-  Fixed a bug where ``STARTD_CRON_JOBLIST`` was not ignoring duplicate
   entries. `(Ticket
   #6604). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6604>`__
-  Fixed a bug when running inside a docker container that would prevent
   the master from started unless ``DISCARD_SESSION_KEYRING_ON_STARTUP``
   was set to false. `(Ticket
   #6602). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6602>`__
-  Fixed a bug specific to the HTCondor Python bindings on Windows,
   where the call htcondor.reload\_config() would fail to see
   environment variable changes made by the Python program. `(Ticket
   #6610). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6610>`__
-  DAGMan did not previously check the user log file (which it depends
   on for coordination with the *condor\_schedd*) for corruption. Now,
   it checks to see if the user log file has been overwritten or
   deleted, and if so, exits immediately with an error. `(Ticket
   #6579). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6579>`__
-  Fixed a bug in the ReadUserLog class where it failed to detect if a
   file file has been overwritten. `(Ticket
   #6582). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6582>`__
-  Fixed a bug where *condor\_submit* would not add needed file transfer
   plugins to the Requirements expression when should\_transfer\_files
   was ``IF_NEEDED``, which is the default. `(Ticket
   #4692). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=4692>`__
-  Fixed a bug where the configuration parameter
   ``STARTD_RECOMPUTE_DISK_FREE`` was not honored when creating a
   dynamic slot from a partitionable slot, which would sometimes result
   in the dynamic slot being provisioned with not enough disk space and
   then failing to match the job. `(Ticket
   #6614). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6614>`__
-  Fixed a bug that caused the job ad attribute
   ``JobCurrentStartTransferOutputDate`` to be set incorrectly. `(Ticket
   #6617). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6617>`__
-  Fixed a bug that could cause ``RemoteWallClockTime`` to have the
   wrong value in the history file. `(Ticket
   #6626). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6626>`__
-  The *condor\_schedd* now considers custom machine resources when
   selecting the next job to run on an idle claimed dynamic slot.
   `(Ticket
   #6630). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6630>`__
-  The attribute ``SlotType`` is now set correctly in the slot ad when
   the *condor\_schedd* is selecting the next job to run on a an idle
   claimed dynamic slot. `(Ticket
   #6611). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6611>`__
-  Fixed a bug where *condor\_submit* with the **-spool** or **-remote**
   option would fail when there were no input files to transfer.
   `(Ticket
   #6655). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6655>`__
-  Fixed a bug that could cause the *condor\_gridmanager* to falsely
   believe that grid-type boinc jobs were submitted to the BOINC server.
   `(Ticket
   #6669). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6669>`__
-  Fixed a bug that could cause the HOLD column to be missing from
   *condor\_q* output when the **-global** option was used. `(Ticket
   #6661). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6661>`__
-  Fixed a bug that caused the *condor\_collector* to reject accounting
   ads when configuration parameter ``COLLECTOR_REQUIREMENTS`` is in
   use. `(Ticket
   #6673). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6673>`__
-  Updated the systemd configuration to set the ``TasksMax`` and
   ``LimitNOFile`` to unlimited. Under some versions of systemd, the
   ``TasksMax`` defaults to 512, which is too small for a busy submit
   host. `(Ticket
   #6645). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6645>`__
-  Reduced the ``RPATH`` in RPM builds to just the needed directories.
   Previously, the tarball ``RPATH`` was used. `(Ticket
   #6662). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6662>`__
-  On the Windows platform, the HTCondor daemons will attempt a
   ``NETWORK`` login to impersonate a user if the ``INTERACTIVE`` login
   fails. `(Ticket
   #6640). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6640>`__

Version 8.6.10
--------------

Release Notes:

-  HTCondor version 8.6.10 released on March 13, 2018.

New Features:

-  None.

Bugs Fixed:

-  Fixed a bug that caused *condor\_preen* to crash before it finished
   cleaning the spool directory and leave a core file of its own in the
   log directory. This problem occurred on submit nodes that had running
   jobs when *condor\_preen* was invoked. `(Ticket
   #6521). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6521>`__
-  Improved the systemd configuration to clean up HTCondor processes on
   shutdown in the event that the *condor\_master* fails to do so.
   `(Ticket
   #6539). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6539>`__
-  HTCondor daemons will do fast shutdown whenever their parent process
   exits unexpectedly. `(Ticket
   #6539). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6539>`__
-  Fixed a bug that would cause *condor\_q* to crash if the hostname was
   longer than 64 bytes. `(Ticket
   #6594). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6594>`__
-  Fixed a bug where if an administrator configured a Concurrency Limit
   whose name ended in a number, *condor\_userprio* **-allusers** would
   show additional bogus user entries. `(Ticket
   #6542). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6542>`__
-  Fixed a bug where the *condor\_starter* would crash when talking to a
   shadow running a condor version older than 8.5 and match
   authentication was enabled. `(Ticket
   #6520). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6520>`__
-  Fixed a bug in Python API *htcondor.Secman().ping()* method which
   would sometimes result in a RunTimeError exception. `(Ticket
   #6562). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6562>`__
-  Fixed a bug where ``policy: want_hold_if`` would always evict
   standard universe jobs instead of putting them on hold. Instead, this
   policy now ignores standard universe jobs entirely. This means that
   the metaknobs ``policy: hold_if_memory_exceeded`` and
   ``policy: hold_if_cpus_exceeded`` will also ignore standard universe
   jobs entirely (instead of its previous bad behavior of of letting
   standard universe jobs use more than their requested memory until the
   first time they were evicted, whereafter each restart would be
   immediately evicted). `(Ticket
   #6583). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6583>`__
-  The metaknob ``policy: hold_if_memory_exceeded`` and
   ``policy: preempt_if_memory_exceeded`` now ignore VM universe jobs.
   These jobs can’t exceed their requested memory. `(Ticket
   #6583). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6583>`__
-  Fixed a bug which mischaracterized the ``MemoryUsage`` of VM universe
   jobs. This should allow VM universe jobs to run when
   ``feature: Hold_If_Memory_Exceeded`` is enabled. `(Ticket
   #6577). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6577>`__
-  Fixed a bug where the *condor\_shadow* could accidentally kill itself
   by not checking if it was attempting to change immutable attributes.
   `(Ticket
   #6557). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6557>`__
-  Fixed a bug that could cause the *condor\_collector* to exit with an
   assertion error under certain (rare) conditions when it has no
   outgoing connectivity to the Internet. `(Ticket
   #6511). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6511>`__
-  Fixed a bug that would cause any daemons interfacing with the CREDMON
   to retry indefinitely when polling for credentials. `(Ticket
   #6523). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6523>`__
-  Fixed a bug that prevented grid-type batch jobs from being removed
   after an attempt to submit to the underlying batch system failed.
   `(Ticket
   #6586). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6586>`__
-  Fixed a bug in Python plugin support for the *condor\_collector* that
   would result in the *condor\_collector* switching from writing from
   the CollectorLog to writing to the ToolLog after a reconfig. `(Ticket
   #6588). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6588>`__
-  Fixed a bug in the $F() macro expansion in submit and configuration
   files that would cause a crash if the argument to the macro was a
   file literal rather than a variable name. `(Ticket
   #6531). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6531>`__
-  Fixed a bug that allowed the *condor\_schedd* to attempt to run jobs
   on a dynamic slot that requested more resources than the slot
   provided. `(Ticket
   #6593). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6593>`__

Version 8.6.9
-------------

Release Notes:

-  HTCondor version 8.6.9 released on January 4, 2018.

New Features:

-  When a daemon crashes, more information about the cause is now
   written to its log file. `(Ticket
   #6483). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6483>`__

Bugs Fixed:

-  Fixed a bug in the group quotas that would give too much surplus
   quota to some groups when ``ACCEPT_SURPLUS`` is on and
   ``NEGOTIATOR_ALLOW_QUOTA_OVERSUBSCRIPTION`` is true (the default)
   `(Ticket
   #6514). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6514>`__
-  Fixed a bug in the Python bindings when doing queries that specify a
   projection with the “attr\_list” argument. The bug could could
   potentially result in memory corruption of the Python interpreter
   process. `(Ticket
   #6468). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6468>`__
-  Reduced the amount of time that *condor\_preen* will block the
   *condor\_schedd*. *condor\_preen* now connects only when specifically
   needed, and automatically disconnects after
   ``PREEN_MAX_SCHEDD_CONNECTION_TIME`` seconds. `(Ticket
   #6490). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6490>`__
-  Fixed a bug on Windows that would often result in the job sandbox on
   the execute node not being deleted when the *condor\_schedd*
   relinquished its claim on the slot before the *condor\_starter* had
   exited. `(Ticket
   #6497). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6497>`__
-  Fixed a bug where the *condor\_master* stopped sending watchdog
   notifications to systemd after restarting itself. This resulted in
   systemd killing the *condor\_master* shortly after the restart.
   `(Ticket
   #6476). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6476>`__
-  Updated the systemd configuration to only restart HTCondor upon
   failure. Otherwise, systemd would restart HTCondor if *condor\_off*
   requested the *condor\_master* to exit. `(Ticket
   #6503). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6503>`__
-  Fixed a bug with the use of the scheduler parameter
   ``MAX_JOBS_SUBMITTED``. If this limit was ever reached by a submit
   with more than one proc in the cluster, the limit would be reduced by
   the difference until the *condor\_schedd* was restarted. `(Ticket
   #6460). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6460>`__
-  Fixed a bug that caused very large RequestDisk requests to fail, and
   cause the Disk attribute in the machine ad to go negative. `(Ticket
   #6467). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6467>`__
-  Fixed a bug with the ``RESERVED_DISK`` parameter that would not
   accept an argument larger than 2 Gigabytes. `(Ticket
   #6472). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6472>`__
-  Improved validation of the lengths of messages in ``PASSWORD`` and
   ``SSL`` authentication methods. `(Ticket
   #6493). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6493>`__
-  Fixed a problem where the VM universe would be taken offline on the
   execute node, if the qcow2 disk image was corrupt. The offending job
   is now put on hold with an appropriate hold message. `(Ticket
   #6505). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6505>`__
-  Fixed a problem which would prevent Java universe jobs from working
   when using a relative path name to a jar file and submitting from
   Linux to Windows or vice versa. `(Ticket
   #6474). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6474>`__
-  Fixed a bug on 32 bit Linux systems that caused the starter to crash
   on startup if cgroup limits were enabled. `(Ticket
   #6501). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6501>`__
-  Fixed a bug in Startd Cron (see
   `4.4.3 <Hooks.html#x51-4450004.4.3>`__) where, in effect,
   ``SlotMergeConstraint`` was ignored. `(Ticket
   #6488). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6488>`__
-  Fixed a bug when IPv6 is enabled which could cause the
   *condor\_startd* to crash when spawning a starter. `(Ticket
   #6462). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6462>`__
-  Fixed a bug in *condor\_q* which could cause the DONE amount to be
   incorrect when multiple clusters shared a batch name. `(Ticket
   #6469). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6469>`__
-  Fixed issue on newer versions of Linux where core files generated by
   a daemon were not usable by gdb. A side effect of this fix is that
   the configuration parameter ``CORE_FILE_NAME`` no longer has any
   effect on Linux. `(Ticket
   #6482). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6482>`__
-  *condor\_chirp* will now no longer abort when given a command that it
   cannot successfully execute, such as fetching a file that does not
   exist. `(Ticket
   #6402). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6402>`__
-  Removed unneeded ``copy_to_spool`` statement from default interactive
   submit file. `(Ticket
   #6315). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6315>`__

Version 8.6.8
-------------

Release Notes:

-  HTCondor version 8.6.8 released on November 14, 2017.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes a security-related
   bug described at
   `http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html>`__.
   `(Ticket
   #6455). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6455>`__

Version 8.6.7
-------------

Release Notes:

-  HTCondor version 8.6.7 released on October 31, 2017.

New Features:

-  Added support for HTTPS transfers in the ``curl_plugin`` utility.
   `(Ticket
   #6253). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6253>`__
-  Job attributes that are recognized by the *batch\_gahp* but not by
   HTCondor can now be specified in the job ad without using a prefix of
   ``Remote_``. `(Ticket
   #6422). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6422>`__

Bugs Fixed:

-  Fixed a bug that caused systems using cgroup memory limits to not
   properly reset the memory limit after the first use of a slot. The
   memory limit would get reused from the previous slot value. `(Ticket
   #6414). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6414>`__
-  Added SELinux type enforcement rules to allow *condor\_ssh\_to\_job*
   to function on Enterprise Linux 7. `(Ticket
   #6362). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6362>`__
-  Asking systemd to stop condor now allows the HTCondor daemons to
   properly clean up, instead of simply immediately sending a SIGKILL.
   As a result, HTCondor daemons stopped via systemd will no longer
   continue to appear alive with *condor\_status*. `(Ticket
   #6096). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6096>`__
-  Fixed problems in Python bindings when using the Submit class to
   submit jobs specifying environment variables or file redirection.
   `(Ticket
   #6420). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6420>`__
-  Change the default value of STARTD\_RECOMPUTE\_DISK\_FREE to false,
   so that the Disk attribute is mostly correct for partitionable slots.
   `(Ticket
   #6424). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6424>`__
-  Docker universe now sets the cgroup cpu-shares field to 100 times the
   number of requested cores, which matches vanilla universe. `(Ticket
   #6423). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6423>`__
-  MOUNT\_UNDER\_SCRATCH when used in Docker universe can now be an
   expression, not just a literal string. This matches the way it works
   in vanilla universe. `(Ticket
   #6401). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6401>`__
-  Fixed a bug that could cause the *condor\_startd* to crash when
   spawning a *condor\_starter* with mixed mode networking. `(Ticket
   #6461). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6461>`__
-  Fixed a bug that caused the *condor\_collector* on Windows to refuse
   connections whenever the number of open sockets was more than 820
   even though space was allocated for 1024 open sockets. `(Ticket
   #6425). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6425>`__
-  Fixed a bug that caused the configuration variable
   ``DEFAULT_MASTER_SHUTDOWN_SCRIPT`` to be ignored on Windows when the
   *condor\_master* was running as a service. `(Ticket
   #6458). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6458>`__
-  *condor\_status* will now print longer lines when its output is
   redirected into a pipe, rather than its input coming from one.
   `(Ticket
   #6381). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6381>`__
-  Fixed a crash in *condor\_transferer* when a connection can’t be
   established with its peer. `(Ticket
   #6412). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6412>`__
-  Fixed a bug that caused *condor\_job\_router\_info* to crash if
   configuration parameter ``JOB_ROUTER_ENTRIES_REFRESH`` was set to a
   positive value. `(Ticket
   #6444). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6444>`__
-  Fixed a bug in *condor\_history* that caused it to print invalid XML
   or JSON syntax when reading from multiple history files. `(Ticket
   #6437). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6437>`__
-  Fixed a bug in the *condor\_schedd* which resulted in the
   ``IsNoopJob`` job attribute sometimes being ignored if the the value
   of this attribute was changed after the job was submitted. `(Ticket
   #6396). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6396>`__
-  Fixed a bug that rarely caused slurm jobs to be held. When slurm
   reports memory utilization and it is a multiple of 1024k, Slurm uses
   the ’M’ suffix. The parsing logic was extended to also interpret the
   ’M’, ’G’, ’T’, and ’P’ suffixes for memory utilization. `(Ticket
   #6431). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6431>`__
-  The condor-bosco RPM ensures the *rsync* is installed as required by
   the Bosco scripts. `(Ticket
   #6439). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6439>`__
-  To avoid unnecessary transfers when ``copy_to_spool`` is set in the
   submit file, HTCondor no longer copies the executable to the local
   spool directory more than once for a cluster. `(Ticket
   #6454). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6454>`__

Version 8.6.6
-------------

Release Notes:

-  HTCondor version 8.6.6 released on September 12, 2017.

New Features:

-  None.

Bugs Fixed:

-  Fixed a bug that might cause the *condor\_schedd* or other daemons to
   crash when logging on Linux to the syslog facility, and the
   *condor\_reconfig* command was run. `(Ticket
   #6364). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6364>`__
-  Fixed a bug that prevented condor daemons from writing out a core
   file for debugging in the very unlikely event that one of them
   crashed. `(Ticket
   #6365). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6365>`__
-  Fixed a bug where the negotiator would make matches where the daemons
   involved did not share an IP version, and thus could not talk to each
   other. `(Ticket
   #6351). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6351>`__
-  HTCondor now works properly with systemd’s watchdog feature on all
   flavors of Linux. Previously, the *condor\_master* wouldn’t send
   alive messages to systemd if systemd wasn’t part of the Linux
   distribution’s standard configuration. This would result in systemd
   killing the HTCondor daemons after a short period of time. `(Ticket
   #6385). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6385>`__
-  Fixed handling of backslashes in string values in old ClassAds format
   in the Python bindings. `(Ticket
   #6382). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6382>`__
-  Fixed a bug in how the CPU usage of Slurm jobs is interpreted.
   `(Ticket
   #6380). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6380>`__
-  Fixed a bug that caused a machine claimed by a parallel universe job
   to stick in the Claimed/Idle state forever. This could only happen if
   the job was removed as it was in the process of claiming resources.
   `(Ticket
   #6376). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6376>`__
-  Fixed a bug that caused a machine to stick in the Preempting/Vacating
   state after a job was removed when a ``JOB_EXIT_HOOK`` was defined.
   `(Ticket
   #6383). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6383>`__
-  Added type enforcement rules for cgroups to HTCondor’s SELinux
   profile. `(Ticket
   #6168). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6168>`__
-  Fixed a bug where setting ``delegate_job_gsi_credentials_lifetime``
   to 0 in a submit description file was treated the same as not setting
   it at all. `(Ticket
   #6375). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6375>`__
-  Fixed handling of octal escape sequences in ClassAd strings. `(Ticket
   #6384). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6384>`__
-  Updated Boost external to version 1.64. `(Ticket
   #6369). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6369>`__

Version 8.6.5
-------------

Release Notes:

-  HTCondor version 8.6.5 released on August 1, 2017.

New Features:

-  Added avx2 to the set of processor flags advertised by the
   *condor\_startd*. `(Ticket
   #6317). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6317>`__

Bugs Fixed:

-  Fixed a bug in socket clean-up that was causing a memory leak. This
   may have been particularly noticeable in the *condor\_collector*.
   `(Ticket
   #6342). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6342>`__
-  Fixed a bug that caused an infinite loop in the *condor\_starter*
   when cgroups were enabled on systems (such as Debian) where the
   kernel has disabled the memory accounting controller. A job on such a
   system would go into the "R" state, but never actually start running.
   `(Ticket
   #6347). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6347>`__
-  Fixed a bug where setting ``NETWORK_INTERFACE`` to an IPv6 address
   could cause HTCondor daemons to except. `(Ticket
   #6339). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6339>`__
-  Fixed a bug where a cross protocol CCB connection would cause the
   *condor\_shadow* or *condor\_schedd* to except. `(Ticket
   #6344). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6344>`__
-  Fixed a bug where the wildcard ’\*’ in ALLOW or DENY lists was being
   interpreted as only matching IPv4 addresses. It now properly matches
   any address family. `(Ticket
   #6340). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6340>`__
-  Fixed a bug where reverse resolutions could return the string
   representation of the address in question instead of failing. This
   resulted in spurious warnings of the form "WARNING: forward
   resolution of 2001:630:10:f001::19a0 doesn’t match
   2001:630:10:f001::19a0!" `(Ticket
   #6338). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6338>`__
-  Fixed a bug which prevented using an ImDisk RAM disk as the execute
   directory on Windows. `(Ticket
   #6324). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6324>`__
-  Fixed a bug where removal of a job could cause another job from the
   same user to also be removed. This was mostly likely to happen when
   the *condor\_schedd* is under heavy load. `(Ticket
   #6353). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6353>`__
-  Fixed a bug that cause parallel universe jobs not to start on pools
   with partitionable slots. `(Ticket
   #6308). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6308>`__
-  Fixed a problem, introduced in HTCondor 8.6.4, where the
   *condor\_collector* plugins where loaded but not used. `(Ticket
   #6343). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6343>`__
-  Fixed a bug where "*condor\_q* **-grid**" did not display the status
   column for any non-Globus job. `(Ticket
   #6306). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6306>`__
-  Fixed bugs in the *condor\_schedd* and *condor\_negotiator* that
   could cause jobs to not be negotiated for when
   ``NEGOTIATOR_PREFETCH_REQUESTS`` is set to ``TRUE``. `(Ticket
   #6336). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6336>`__
   `(Ticket
   #6312). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6312>`__

Version 8.6.4
-------------

Release Notes:

-  HTCondor version 8.6.4 released on June 22, 2017.

New Features:

-  Python bindings are now available on MacOSX. `(Ticket
   #6244). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6244>`__
-  Allow Python modules to be used as *condor\_collector* plugin. This
   undocumented feature is to be used by expert developers only.
   `(Ticket
   #6213). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6213>`__
   `(Ticket
   #6295). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6295>`__

Bugs Fixed:

-  Fixed a bug with PASSWORD authentication that would sporadically
   cause it to fail to exchange keys, due to whether or not the first
   round-trip of communications blocked on reading from the network.
   `(Ticket
   #6265). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6265>`__
-  Pslot preemption now properly handles machine custom resources, such
   as GPUs. `(Ticket
   #6297). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6297>`__
-  Fixed a bug that prevented HTCondor from correctly setting virtual
   memory cgroup limits when soft physical memory limits were being
   used. `(Ticket
   #6294). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6294>`__
-  Fixed a bug that prevented parallel universe jobs from running that
   used $$() expansion in submit files. `(Ticket
   #6299). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6299>`__
-  Added a new knob, ``STARTD_RECOMPUTE_DISK_FREE``, which defaults to
   true, which tells the startd to periodically recompute and advertise
   free disk space. Admins can set this to false for partitionable slots
   whose execute directory is used by HTCondor alone. `(Ticket
   #6301). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6301>`__
-  Fixed a bug that could cause *condor\_submit* to fail to submit a job
   with a proxy file to a *condor\_schedd* older than 8.5.8, due to the
   absence of an X.509 CA certificates directory. `(Ticket
   #6258). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6258>`__
-  Restored a check in *condor\_submit* about whether the job’s X.509
   proxy has sufficient lifetime remaining. `(Ticket
   #6283). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6283>`__
-  Fixed a bug in *condor\_dagman* where the DAG status file showed an
   incorrect status code if submit attempts failed for the final node.
   `(Ticket
   #6069). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6069>`__
-  Bosco now properly identifies CentOS 7 as a supported platform.
   `(Ticket
   #6303). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6303>`__
-  Fixed a bug when Bosco is used to submit jobs to multiple remote
   clusters. When arguments to *remote\_gahp* are provided in the
   GridResource attribute, jobs could be submitted to the wrong cluster.
   `(Ticket
   #6277). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6277>`__
-  To speed up the installation process on Enterprise Linux 7, the
   SELinux profile is now reloaded only once, when setting the HTCondor
   daemons to run in permissive mode. `(Ticket
   #6304). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6304>`__
-  Update the systemd configuration on Enterprise Linux 7 to start the
   *condor\_master* after time synchronization is achieved. This
   prevents unnecessary daemon restarts due to sudden time shifts.
   `(Ticket
   #6255). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6255>`__
-  The *condor\_shadow* will now ignore updates of ``JobStartDate`` from
   the *condor\_starter* since the *condor\_schedd* already sets this
   attribute correctly and the *condor\_starter* incorrectly tries to
   set it even if the job has already run once. A consequence of this
   fix is that the value of ``JobStartDate`` that the *condor\_startd*
   uses for policy expressions will be different than the value that the
   *condor\_schedd* uses. Resolving this problem will potentially break
   existing policy expressions in the *condor\_startd*, so it will be be
   not be changed in the 8.6 series, but fixed in the 8.7 series.
   `(Ticket
   #6280). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6280>`__
-  Fixed a bug where per-instance job attributes like ``RemoteHost``
   would show up in the history file for completed jobs. This bug
   occurred if a job happened to complete while the *condor\_schedd* was
   in the process of a graceful shutdown. `(Ticket
   #6251). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6251>`__
-  The *condor\_convert\_history* command is present again in this
   release. `(Ticket
   #6282). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6282>`__
-  The parameter ``SETTABLE_ATTRS_ADMINISTRATOR`` is now correctly
   appears in *condor\_config\_val*. `(Ticket
   #6286). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6286>`__

Version 8.6.3
-------------

Release Notes:

-  HTCondor version 8.6.3 released on May 9, 2017.

Bugs Fixed:

-  Fixed a bug that rarely corrupts the *condor\_schedd*\ ’s job queue
   log file when the input sandbox of a job with an X.509 proxy file is
   spooled. `(Ticket
   #6240). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6240>`__
-  Fixed a memory leak in the Python bindings related to logging.
   `(Ticket
   #6227). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6227>`__

Version 8.6.2
-------------

Release Notes:

-  HTCondor version 8.6.2 released on April 24, 2017.

New Features:

-  Added metaknobs for defining map files for use with the ClassAd
   usermap function in the *condor\_schedd*, and a metaknob for
   automatically assigning an accounting group to a job based on a
   mapping of the owner name of the job. `(Ticket
   #6179). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6179>`__
-  When the *condor\_credd* is polling for credentials, the timeout is
   now configurable using ``CREDD_POLLING_TIMEOUT``.
-  The **reverse** option for *condor\_q* was changed to
   **reverse-analyze**, and it now implies **better-analyze**. Formerly,
   the **reverse** option was ignored unless **-better-analyze** was
   also specified. `(Ticket
   #6167). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6167>`__

Bugs Fixed:

-  Fixed a bug that could cause *condor\_store\_cred* to fail on Windows
   due to a case-sensitive check of the user’s account name. `(Ticket
   #6200). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6200>`__
-  Updated Open MPI helper script to catch and handle SIGTERM and to use
   bash explicitly. `(Ticket
   #6194). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6194>`__
-  Docker Universe jobs now update the RemoteSysCpu attributes for job
   and in the job log. Previously, this field was always 0. `(Ticket
   #6173). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6173>`__
-  Docker universe detection is now more robust in the face of
   extraneous output to standard error on docker startup. This was
   preventing Condor from detecting that docker was properly working on
   hosts. `(Ticket
   #6185). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6185>`__
-  Fixed a bug that prevented ``SUBMIT_REQUIREMENT`` and
   ``JOB_TRANSFORM`` expressions from referencing job attributes
   describing the job’s X.509 proxy credential. `(Ticket
   #6188). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6188>`__
-  The Linux kernel tuning script no longer adjusts some kernel
   parameters unless a *condor\_schedd* will be started by the master.
   `(Ticket
   #6208). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6208>`__
-  Fixed a bug that caused all but the first in a list of metaknobs to
   be ignored unless there were commas separating the list items. So
   ``use ROLE : Execute CentralManager`` would incorrectly add only the
   Execute role. Previously, ``use ROLE : Execute, CentralManager``
   would correctly add both roles. `(Ticket
   #6171). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6171>`__
-  Worked around a problem with FORTRAN programs built with
   *condor\_compile* and recent versions of gfortran (4.7.2 was OK,
   4.8.5 was not), where those executables would not write to standard
   out if started in the standard universe. Also, updated the
   checkpointing library to permit *condor\_compile* to successfully
   link FORTRAN (and other) programs calling certain math functions and
   built against up-to-date versions of glibc. `(Ticket
   #6026). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6026>`__
-  The default values for ``HAD_SOCKET_NAME`` and
   ``REPLICATION_SOCKET_NAME`` have changed to enable the documented
   configuration for using these services with shared port to work.
   `(Ticket
   #6186). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6186>`__
-  Fixed a bug that caused *condor\_dagman* to sometimes (rarely, but
   repeatably) crash when parsing DAGs containing splices. `(Ticket
   #6170). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6170>`__
-  The configuration parameters that control when job policy expressions
   are evaluated now work as documented. Previously, the default value
   for ``PERIODIC_EXPR_INTERVAL`` was 300, not 60 as intended. Also, the
   parameters ``MAX_PERIODIC_EXPR_INTERVAL`` and
   ``PERIODIC_EXPR_TIMESLICE`` were ignored for grid universe jobs.
   `(Ticket
   #6199). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6199>`__
-  Fixed a bug that could cause the Job Router to crash if the
   ``job_queue.log`` contained invalid or incomplete records. `(Ticket
   #6195). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6195>`__
-  Fixed a bug that caused updates of the job attribute
   ``x509UserProxyExpiration`` to be ignored for job policy evaluation
   when the job was managed by the Job Router. `(Ticket
   #6209). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6209>`__
-  Changed the default value of configuration parameters
   ``CREAM_GAHP_WORKER_THREADS`` to the value of
   ``GRIDMANAGER_MAX_PENDING_REQUESTS``. This should prevent a back-log
   of commands in the CREAM GAHP observed by some users. `(Ticket
   #6071). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6071>`__
-  Fixed modification of ``PYTHONPATH`` environment variable that could
   fail in bash if *set -u* is enabled. `(Ticket
   #6211). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6211>`__
-  *bosco\_quickstart* no longer assumes that submitting to a Slurm
   cluster requires the PBS emulation module. `(Ticket
   #6211). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6211>`__
-  Fixed a bug that caused *condor\_submit* **-dump** to crash when the
   submit file had an attribute to enable the use of an x509 user proxy.
   `(Ticket
   #6197). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6197>`__
-  Updated the supported platform list in the Bosco installer script to
   include Ubuntu 16 and Mac OSX 10.12. Also, dropped Ubuntu 12 and Mac
   OSX 10.6 through 10.9. `(Ticket
   #6178). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6178>`__
-  Fixed a bug which in some obscure configurations caused a spurious
   PERMISSION DENIED error was printed in the StartLog when activating a
   claim. `(Ticket
   #6172). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6172>`__.
-  Fixed a bug which forced the administrator to restart (rather than
   reconfigure) running daemons after adding an entry to a ``DENY_*``
   authorization list. `(Ticket
   #6172). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6172>`__.

Version 8.6.1
-------------

Release Notes:

-  HTCondor version 8.6.1 released on March 2, 2017.

New Features:

-  *condor\_q* now checks to see if authentication and security
   negotiation are enabled before attempting to request only the current
   users jobs from the *condor\_schedd*. Prior to this change,
   configurations that disabled security or authentication would also
   need to set ``CONDOR_Q_ONLY_MY_JOBS`` to false. `(Ticket
   #6125). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6125>`__
-  The CLAIMTOBE authentication method is now in the list of methods for
   READ access if no list of authentication methods for READ or DEFAULT
   is specified in the configuration. This change allows sites that use
   the default host based security model to use *condor\_q* **-global**
   with the only-my-jobs feature without making changes to their
   security configuration. `(Ticket
   #6125). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6125>`__
-  The collector now records the authentication method used to determine
   the authenticated identity. `(Ticket
   #6122). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6122>`__

Bugs Fixed:

-  Update Docker interface to be able to retrieve usage information from
   running containers and to remove containers when certain errors
   occurred when using Docker version 1.13. `(Ticket
   #6088). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6088>`__
-  In Docker universe, all writes to files in ``/tmp`` and ``/var/tmp``
   by default write inside the container. There is a limit on the file
   size within the container, and jobs that write a lot to ``/tmp`` may
   hit that. If a docker universe job now runs on a system with
   ``MOUNT_UNDER_SCRATCH`` defined, HTCondor now adds those mounts as
   volume mounts, so file writes do not go to the container, but to the
   host file system. `(Ticket
   #6080). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6080>`__
-  Fixed a bug in *condor\_status* **-format** and *condor\_q*
   **-format** that caused the tools to truncate output to the width
   specified in the format specifier. The most likely manifestation of
   this bug was that punctuation after the format would not be printed
   when the format had an explicit width. `(Ticket
   #6120). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6120>`__
-  Fixed a bug that caused spurious shared port-related error messages
   to appear in the ``dagman.out`` file (by adding the new
   ``DAGMAN_USE_SHARED_PORT`` configuration macro). `(Ticket
   #6156). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6156>`__
-  Fixed a bug that caused VM universe jobs to fail if the **vm\_disk**
   submit command contained spaces after a comma. `(Ticket
   #6132). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6132>`__
-  Fixed a bug that can cause the Job Router and *condor\_c-gahp* to
   crash if they fail to submit a job due to submit transforms or submit
   requirements. `(Ticket
   #6152). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6152>`__
-  Fixed a bug that caused the Job Router to not route any jobs if the
   ``JOB_ROUTER_DEFAULTS`` configuration parameter value started with
   white space. `(Ticket
   #6128). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6128>`__
-  Fixed several bugs in how the Job Router writes to job event logs.
   `(Ticket
   #6092). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6092>`__
-  Removed Bosco’s attempt to configure a default value for
   **grid\_resource** in the submit description file, as
   *condor\_submit* no longer supports this ability. Also, Bosco now
   works with Slurm clusters. `(Ticket
   #6106). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6106>`__
-  Changed Bosco’s configuration of the *condor\_ft-gahp* to eliminate
   worrying error messages in the *condor\_ft-gahp*\ ’s log file.
   `(Ticket
   #6107). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6107>`__
-  Fixed a bug that could cause a grid batch job submitted to PBS or
   Slurm to go on hold when the job’s X.509 proxy is refreshed. `(Ticket
   #6136). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6136>`__
-  Fixed a bug where the *condor\_gridmanager* fails to put a job on
   hold due to the desired hold reason containing invalid characters.
   `(Ticket
   #6142). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6142>`__
-  Improved the hold reason when submission of a grid-type batch job
   fails. `(Ticket
   #3377). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=3377>`__
-  Update helper scripts to work with current versions of Open MPI and
   MPICH2. `(Ticket
   #6024). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6024>`__
-  Fixes a bug that could cause events for local universe jobs to not be
   written to the global event log. `(Ticket
   #6100). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6100>`__
-  Fixed a bug on execute machines that enable PID namespaces that would
   generate a spurious error message in the daemon log when
   *condor\_off* -fast was issued. `(Ticket
   #6137). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6137>`__
-  Fixed a bug that could corrupt the job queue log file such that the
   *condor\_schedd* cannot restart. The bug is mostly likely to occur if
   the disk becomes full. `(Ticket
   #6153). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6153>`__
-  Incremented the ClassAd library version number, since the deprecated
   iostream interface has been removed. `(Ticket
   #6050). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6050>`__
   `(Ticket
   #6115). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6115>`__

Version 8.6.0
-------------

Release Notes:

-  HTCondor version 8.6.0 released on January 26, 2017.

New Features:

-  Added two new job ClassAd attributes, ``CumulativeRemoteSysCpu`` and
   ``CumulativeRemoteUserCpu``, which keep a running total of system and
   user CPU usage, respectively, across all job restarts. Also,
   immediately clear attributes ``RemoteSysCpu`` and ``RemoveUserCpu``
   on job start, instead of on first update. `(Ticket
   #6022). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6022>`__
-  Added a new configuration knob, ``ALWAYS_REUSEADDR``, which defaults
   to ``True``. When ``True``, it tells HTCondor to set the
   ``SO_REUSEADDR`` socket option, so that the schedd can run large
   numbers of very short jobs without exhausting the number of local
   ports needed for shadows. `(Ticket
   #6040). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6040>`__
-  Changed the default value of ``IGNORE_LEAF_OOM`` to ``True``.
   `(Ticket
   #5775). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5775>`__

Bugs Fixed:

-  Fixed a bug causing unnecessarily slow updates from the
   *condor\_startd*. If you depend on the old behavior, set
   ``UPDATE_SPREAD_TIME`` to 8. A value of 0 enables the fix. `(Ticket
   #6062). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6062>`__
-  Fixed a race condition when running multiple concurrent jobs on the
   same claim. When the starter exits, it notifies the shadow, which
   tells the startd to kill the starter. Immediately after the shadows
   tells the startd, it fetches the next job, and tries to start it. If
   the starter hasn’t completely exited yet (perhaps it needs to clean
   up a large sandbox), it will notice the shadow has closed the command
   socket, and the starter will go into disconnected mode, and get
   confused. This has been fixed. `(Ticket
   #6049). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6049>`__
-  Fixed an infelicity with *condor\_submit* -i and docker universe,
   where it would start an interactive shell without a container. Added
   error message expressing that this combination is not currently
   supported. `(Ticket
   #6083). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6083>`__
-  When a job claimed by the Job Router is held or removed, it is no
   longer considered a failure of the job route chosen for that job.
   `(Ticket
   #5968). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5968>`__
-  Fixed a bug in recovering a Google Compute Engine (GCE) job if the
   *condor\_gridmanager* restarts during submission of the instance
   request. `(Ticket
   #6078). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6078>`__
-  Fixed a bug that could cause re-installation of a remote cluster to
   fail in Bosco. `(Ticket
   #6042). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6042>`__
-  Fixed a bug with handling the proxy files of grid-type batch jobs
   when the proxy’s file name is a relative path. `(Ticket
   #6053). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6053>`__
-  Fixed a bug that caused the *batch\_gahp* to crash when a job’s X.509
   proxy is refreshed and the *batch\_gahp* is configured to not create
   a limited copy of the proxy. `(Ticket
   #6051). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6051>`__
-  Fixed a bug in the virtual machine universe where ``RequestMemory``
   and ``RequestCPUs`` were not changing the resources assigned to the
   VM created by HTCondor. Now, ``VM_Memory`` defaults to
   ``RequestMemory``, and the number of CPUs defaults to
   ``RequestCPUs``. `(Ticket
   #5998). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5998>`__

      
