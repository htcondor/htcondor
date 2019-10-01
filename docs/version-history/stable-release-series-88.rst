Stable Release Series 8.8
=========================

This is the stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
8.8.x releases. New features will be added in the 8.9.x development
series.

The details of each version are described below.

Version 8.8.6
-------------

Release Notes:

-  HTCondor version 8.8.6 not yet released.

.. HTCondor version 8.8.6 released on Month Date, 2019.

New Features:

-  None.

Bugs Fixed:

-  Fixed a bug where ``COLLECTOR_REQUIREMENTS`` wrote too much to the log
   to be useful.  It now only writes warnings about rejected ads when
   the collector's debug level includes ``D_MACHINE``, and only includes
   the rejected ads themselves in the output at the ``D_MACHINE:2`` level.
   :ticket:`7264`

-  Fixed a bug where, for ``gce`` grid universe jobs, if the credentials
   file has credentials for more than one account, the wrong account's
   credentials are used for some requests.
   :ticket:`7218`

-  Fixed a bug where the classad function bool() would return the wrong
   value when passed a string.
   :ticket:`7253`

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

-  Dagman can now submit directly to the Schedd without using *condor_submit*
   This provides a workaround for slow submission rates for very large DAGs.
   This is controlled by a new configuration variable ``DAGMAN_USE_CONDOR_SUBMIT``
   which defaults to ``True``.  When it is ``False``, Dagman will contact the
   local Schedd directly to submit jobs. :ticket:`6974`

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

-  Fixed a bug that prevented *condor_submit* ``-i`` from working with
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
   container with the condor_master as PID 1.  HTCondor could start
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
