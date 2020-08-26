Development Release Series 8.7
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 8.7.10
--------------

Release Notes:

-  HTCondor version 8.7.10 released on October 31, 2018.

New Features:

-  One can now submit an interactive Docker job. :ticket:`6710`
-  Added the ``SINGULARITY_EXTRA_ARGUMENTS`` configuration parameter.
   Administrators can now append arguments to the Singularity command
   line. :ticket:`6731`
-  The MUNGE security method is now supported on all Linux platforms.
   :ticket:`6713`
-  The grid universe can now be used to create and manage VM instances
   in Microsoft Azure, using the new grid type **azure**. :ticket:`6176`
-  Added single-node configuration package to facilitate using a
   personal HTCondor. :ticket:`6709`
-  Added a new file transfer plugin, multifile_curl_plugin which is
   able to transfer multiple files with only a single invocation of the
   plugin, preserving the TCP connection. It also takes a ClassAd as
   input data, which will allow us to pass in more complex input than
   the existing curl_plugin. :ticket:`6499`
-  Added two new policies, ``PREEMPT_IF_RUNTIME_EXCEEDS`` and
   ``HOLD_IF_RUNTIME_EXCEEDS``. The former is (intended to be) identical
   to the policy ``LIMIT_JOB_RUNTIMES``, except without ordering
   constraints with respect to other policy macros. (``ALWAYS_RUN_JOBS``
   must still come before any other policy macro, but unlike
   ``LIMIT_JOB_RUNTIMES``, ``PREEMPT_IF_RUNTIME_EXCEEDS`` may come after
   other policy macros.) Additionally, both of the new policies function
   while the machine is draining. :ticket:`6701`
-  *condor_submit* will no longer attempt to read submit commands from
   standard input when there is no submit file if a queue statement and
   at least one submit command is provided on the command line. :ticket:`6581`
-  If the first line of the job's executable starts with ``#!``
   *condor_submit* will now check that line for a Windows/DOS line
   ending, and if it finds one, it will not submit the job because such
   a script will not be able to start on Unix or Linux platforms. This
   check can be changed from an error to a warning by submitting with
   the **allow-crlf-script** option. :ticket:`6660`
-  Added support for spaces in remapped file paths in *condor_submit*.
   :ticket:`6642`
-  Improved error handling during SSL authentication. :ticket:`6720`
-  Improved throughput when submitting a large number of Condor-C jobs.
   Previously, Condor-C jobs could remain held for a long time in the
   remote *condor_schedd* 's queue while other jobs were being
   submitted. :ticket:`6716`
-  Updated default configuration parameters to improve performance for
   large pools and gives users a better experience. :ticket:`6768`
   :ticket:`6787`
-  Added new configuration parameter ``TRUST_LOCAL_UID_DOMAIN``. It
   works like ``TRUST_UID_DOMAIN``, but only applies when the
   *condor_shadow* and *condor_starter* are on the same machine.
   :ticket:`6785`
-  Added a new configuration parameter
   ``SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES``. It determines whether file
   transfer should default to YES, NO, or AUTO when when the submit file
   does not supply a value for ``should_transfer_files`` and file
   transfer is not forced on or off by some other parameter in the
   submit file. Prior to this addition, *condor_submit* would always
   default to AUTO. :ticket:`6784`
-  Added new statistics attributes about the lifetime of the
   *condor_starter* to the *condor_startd* Ad. This attributes are
   intended to aid in writing policy expressions that prevent a node
   from matching jobs when the node has frequently failed to start jobs.
   :ticket:`6698`
-  For grid-type ``boinc`` jobs, the following job ad attributes can be
   used to to set the BOINC job template parameters of the same name:
   ``rsc_fpops_est``, ``rsc_fpops_bound``, ``rsc_memory_bound``,
   ``rsc_disk_bound``, ``delay_bound``, and ``app_version_num``.
   :ticket:`6760`
-  Daemons now advertise ``DaemonLastReconfigTime`` in all of their ads.
   This is either the boot time of the time, or the last time
   *condor_reconfig* was run on that daemon. :ticket:`6758`

Bugs Fixed:

-  Fixed a bug where ``PREEMPT`` was not be evaluated if the machine was
   draining. This prevent the ``HOLD_IF`` series of policies from
   functioning properly in that situation. :ticket:`6697`
-  Fixed a bug that occurred when starting Docker Universe jobs that
   would cause the *condor_starter* to crash and the jobs to cycle
   between ``running`` and ``idle`` status. :ticket:`6725`
-  Fixed a bug that could cause a job to go into a rapid cycle between
   ``running`` and ``idle`` status if a policy expression evaluated to
   ``Undefined`` during input file transfer. :ticket:`6728`
-  Fixed bugs where small jobs would not match partitionable slots when
   Group Quotas were enabled. :ticket:`6714`
   :ticket:`6750`
-  Fixed a bug that prevented *condor_tail* ``-stderr`` from working.
   :ticket:`6755`
-  *condor_who* now works properly on macOS. :ticket:`6652`
-  Fixed output of *condor_q* -global when printing in JSON, XML, or
   new ClassAd format. :ticket:`6761`
-  Fixed a bug that could cause *condor_wait* and the python bindings
   on Windows to repeat events when reading the job event log. :ticket:`6752`
-  Added missing Accounting, Credd, and Defrag AdTypes to the python
   bindings AdTypes enumeration. :ticket:`6737`
-  Fixed a bug that caused late materialization jobs to handle the
   ``getenv`` submit command incorrectly. :ticket:`6723`
-  Fixed an inefficiency in the SetAttribute remote procedure call that
   could sometimes result in noticeable performance reduction of the
   *condor_schedd*. Removing this inefficiency will allow a single
   *condor_schedd* to handle updates from a larger number of running
   jobs. :ticket:`6732`
-  The *condor_gangliad* can now publish accounting Ads as Ganglia
   metrics. :ticket:`6757`
-  *condor_ssh_to_job* is now configured to use the IPv4 loopback
   address. This avoids problems when IPv6 is present but not enabled.
   :ticket:`6711`
-  Fixed a bug where the ``JobSuccessExitCode`` was not set. :ticket:`6786`
-  Fixed a problem with the EC2 configuration file was present in the
   tarballs. :ticket:`6797`

Version 8.7.9
-------------

Release Notes:

-  HTCondor version 8.7.9 released on August 1, 2018.

Known Issues:

-  Amazon Web Services is deprecating support for the Node.js 4.3
   runtime, used by *condor_annex*, on July 31 (2018). If you ran the
   *condor_annex* setup command with a previous version, you must
   update your account to use the new runtime. Follow the link below for
   simple instructions. Accounts setup with this version of HTCondor
   will use the new runtime.
   `https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUpgradeTheAnnexRuntime <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUpgradeTheAnnexRuntime>`_
   :ticket:`6665`
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

-  The HTCondor Python bindings Submit class can now be initialized from
   an existing *condor_submit* file including the QUEUE statement.
   Python bindings Submit class also can now submit a job for each step
   of a Python iterator. :ticket:`6679`
-  VM universe jobs are now given time to shutdown after a power-off
   signal when they are evicted gracefully. :ticket:`6705`
-  The ``NETWORK_HOSTNAME`` configuration parameter can now be set to a
   fully-qualified hostname that's an alias of one of the machine's
   interfaces. :ticket:`6702`
-  Added a new tool, *condor_now*, which tries to run the specified job
   now. You specify two jobs that you own from the same
   *condor_schedd*: the now-job and the vacate-job. The latter is
   immediately vacated; after the vacated job terminates, if the
   *condor_schedd* still has the claim to the vacated job's slot (and
   it usually will), the *condor_schedd* will immediately start the
   now-job on that slot. The now-job must be idle and the vacate-job
   must be running. If you're a queue super-user, the jobs must have the
   same owner, but that owner doesn't have to be you. :ticket:`6659`
-  HTCondor now supports backfill while draining. You may now use the
   *condor_drain* command, or configure the *condor_defrag* daemon, to
   set a different ``START`` expression for the duration of the
   draining. See the definition of ``DEFRAG_DRAINING_START_EXPR`` (
   `Configuration Macros <../admin-manual/configuration-macros.html>`_)
   and the *condor_drain* manual (
   `condor_drain <../man-pages/condor_drain.html>`_) for details. See
   also the known issues above for information which may influence your
   choice of ``START`` expressions. :ticket:`6664`
-  Docker universe jobs now run with the supplemental group ids of the
   running user, not just the primary group. :ticket:`6658`
-  Added proxy delegation for vanilla universe jobs that define a X509
   proxy but do not use the file transfer mechanism. :ticket:`6587`
-  Added configuration parameters ``GAHP_SSL_CADIR`` and
   ``GAHP_SSL_CAFILE`` to specify trusted CAs when authenticating EC2
   and GCE servers. This used by be controlled by ``SOAP_SSL_CA_DIR``
   and ``SOAP_SSL_CAFILE``, which have been removed. :ticket:`6684`
-  HTCondor can now read the new credentials file format used by the
   Goggle Cloud Platform command-line tools. :ticket:`6657`

Bugs Fixed:

-  Fixed a bug where an ill-formed startd docker image cache file could
   cause the starter to crash starting docker universe jobs. :ticket:`6699`
-  Fixed a bug that would prevent environment variables defined in a job
   submit file from appearing in jobs running in Singularity containers
   using Singularity version 2.4 and greater. :ticket:`6656`
-  Fixed a problem where a *condor_vacate_job*, when passed the
   **-fast** flag, would leave the corresponding slot stuck in
   "Preempting/Vacating" state until the job lease expired. :ticket:`6663`
-  Fixed a problem where *condor_annex* 's setup routine, if no region
   had been specified on the command line, would write a configuration
   for a bogus region rather than the default one. :ticket:`6666`
-  The *condor_history_helper* program was removed. *condor_history*
   is now used by the *condor_schedd* to help with remote history
   queries. :ticket:`6247`

Version 8.7.8
-------------

Release Notes:

-  HTCondor version 8.7.8 released on May 10, 2018.

New Features:

-  *condor_annex* may now be setup in multiple regions simultaneously.
   Use the **-aws-region** flag with **-setup** to add new regions. Use
   the **-aws-region** flag with other *condor_annex* commands to
   choose which region to operate in. You may change the default region
   by setting ``ANNEX_DEFAULT_AWS_REGION``. :ticket:`6632`
-  Added default AMIs for all four US regions to simplify using
   *condor_annex* in those regions. :ticket:`6633`
-  HTCondor will no longer mangle ``CUDA_VISIBLE_DEVICES`` or
   ``GPU_DEVICE_ORDINAL`` if those environment variables are set when it
   starts up. As a result, HTCondor will report GPU usage with the
   original device index (rather than starting over at 0). :ticket:`6584`
-  When reporting ``GPUsUsage``, HTCondor now also reports
   ``GPUsMemoryUsage``. This is like ``MemoryUsage``, except it is the
   peak amount of GPU memory used by the job. This feature only works
   for nVidia GPUs. :ticket:`6544`
-  Improved error messages when delegation of an X.509 proxy fails.
   :ticket:`6575`
-  *condor_q* will no longer limit the width of the output to 80
   columns when it outputs to a file or pipe. :ticket:`6643`
-  Submission of jobs via the Python bindings Submit class will now
   attempt to put all jobs submitted in a single transaction under the
   same ClusterId. :ticket:`6649`
-  Added support for *condor_schedd* query options in the Python
   bindings. :ticket:`6619`
-  Eliminated SOAP support. :ticket:`6648`

Bugs Fixed:

-  Fixed a problem where, when starting enough *condor_annex* instances
   simultaneously, some (approximately 1 in 100) instances would neither
   join the pool nor terminate themselves. :ticket:`6638`
-  When running in a HAD setup, there is a configuration parameter,
   ``COLLECTOR_HOST_FOR_NEGOTIATOR`` which tells the active negotiator
   which collector to prefer. Previously, this parameter had no default,
   so the negotiator might arbitrarily chose a far-away collector. Now
   this knob defaults to the local collector in a HAD setup. :ticket:`6616`
-  Fixed a bug when running in a configuration with more than one
   *condor_collector*, the *condor_negotiator* would only send the
   accounting ads to one of them. The result of this bug is that the
   *condor_userprio* tool would show now results about half of the time
   it was run. :ticket:`6615`
-  Fixed a bug where *condor_annex* would fail with a malformed
   authorization header when using AWS resources in a region other than
   ``us-east-1``. :ticket:`6629`
-  Fixed a bug that prevented Docker universe jobs with no executable
   listed in the submit file from running. :ticket:`6612`
-  Fixed a bug where the *condor_starter* would fail with an error
   after a docker job exits. :ticket:`6623`
-  Fixed a bug where *condor_userprio* would always show zero resources
   in use when NEGOTIATOR_CONSIDER_PREEMPTION=false was set. :ticket:`6621`
-  Fixed a bug where ".update.ad" was not being updated atomically.
   :ticket:`6591`
-  Fixed a bug that could cause a machine slot to become stuck in the
   Claimed/Busy state after a job completes. :ticket:`6597`

Version 8.7.7
-------------

Release Notes:

-  HTCondor version 8.7.7 released on March 13, 2018.

New Features:

-  *condor_ssh_to_job* now works with Docker Universe, the
   interactive shell is started inside the container. This assume that
   there is a shell executable inside the container, but not necessarily
   an sshd. :ticket:`6558`
-  Improved error messages in the job log for Docker universe jobs that
   do not start. :ticket:`6567`
-  Release a 32-bit *condor_shadow* for Enterprise Linux 7 platforms.
   :ticket:`6495`
-  HTCondor now reports, in the job ad and user log, which custom
   machine resources were assigned to the slot in which the job ran.
   :ticket:`6549`
-  HTCondor now reports ``CPUsUsage`` for each job. This attribute is
   like ``MemoryUsage`` and ``DiskUsage``, except it is the average
   number of CPUs used by the job. :ticket:`6477`
-  The ``use feature: GPUs`` metaknob now causes HTCondor to report
   ``GPUsUsage`` for each job. This is like ``CPUsUsage``, except it is
   the average number of GPUs used by the job. This feature only works
   for nVIDIA GPUs. :ticket:`6477`
-  Administrators may now, for each custom machine resource, define a
   custom resource monitor. Such a script reports the usage(s) of each
   instance of the corresponding machine resource since the last time it
   reported; HTCondor aggregates these reports between resource
   instances and over time to produce a ``<Resource>Usage`` attribute,
   which is like ``GPUsUsage``, except for the custom machine resource
   in question. :ticket:`6477`
-  The *condor_startd* now periodically writes a file to each job's
   sandbox named ".update.ad". This file is a copy of the slot's machine
   ad, but unlike ".machine.ad", it is regularly updated. Jobs may read
   this file to observe their own usage attributes. :ticket:`6477`
-  A new option **-unmatchable** was added to *condor_q* that causes
   *condor_q* to show only jobs that will not match any of the
   available slots. :ticket:`6529`
-  OpenMPI jobs launched in the parallel universe via ``openmpiscript``
   now work with shared file systems (again). :ticket:`6556`
-  Allow a parallel universe job with parallel scheduling group to
   select a new parallel scheduling group when held and released.
   :ticket:`6516`
-  Allow p-slot preemption to work with parallel universe. :ticket:`6517`
-  Added the ability in *condor_dagman* to specify submit files with
   spaces in their path names. Paths that include spaces must be wrapped
   in quotes (i.e. JOB A "/path to/job.sub"). :ticket:`6389`
-  Added the ability in *condor_submit* to specify executable, error
   and output files with spaces in their paths. Previously, adding
   whitespace to these fields would result in an error claiming certain
   attributes could only take exactly one argument. Now, whitespace is
   treated as part of the path. :ticket:`6389`
-  An IPv6 address can now be specified in the configuration file either
   with or without square brackets in most cases. If specifying a port
   number in the same value, the square brackets are required. If using
   a wild card to specify a range of possible addresses, square brackets
   are not allowed. :ticket:`5697`
-  Improved support for IPv6 link-local addresses, in particular using
   the correct scope id. Using a wild card or device name in
   ``NETWORK_INTERFACE`` now works properly when ``NO_DNS`` is set to
   ``True``. :ticket:`6518`
-  Python bindings installed via pip on a system without a HTCondor
   install (i.e. without a ``condor_config`` present) will use a "null"
   config and print a warning. :ticket:`6515`
-  The new configuration parameter ``NEGOTIATOR_JOB_CONSTRAINT`` defines
   an expression which constrains which job ads are considered for
   matchmaking by the *condor_negotiator*. :ticket:`6250`
-  The *condor_startd* will now keep trying to delete a job sandbox
   until it succeeds. The retries are attempted with an exponential back
   off in frequency. :ticket:`6500`
-  *condor_q* will no longer batch jobs with different cluster ids
   together unless they have the same JobBatchName attribute or are in
   the same DAG. :ticket:`6532`
-  *condor_q* will now sort jobs by job id when the **-long** argument
   is used. :ticket:`6287`
-  Improve the performance of reading and writing ClassAds to the
   network. The performance of reading ClassAds from UDP is particularly
   improved, up to 20% faster than previously. :ticket:`6555`
   :ticket:`6561`
-  Several minor performance improvements. :ticket:`6550`
   :ticket:`6551`
   :ticket:`6565`
   :ticket:`6566`
-  Removed configuration parameters ``ENABLE_ADDRESS_REWRITING`` and
   ``SHARED_PORT_ADDRESS_REWRITING``. :ticket:`6525`
-  Removed the deprecated AvailStats attribute from the machine ad. This
   was being computing incorrectly, and apparently never used. :ticket:`6526`
-  Added basic support for a "Credential Management" subsystem which
   will eventually be used to support interaction with OAuth services
   (like SciTokens, Box.com, Google Drive, DropBox, etc.). Still in
   preliminary phases and not really ready for public use. :ticket:`6513`

Bugs Fixed:

-  Fixed a bug where Docker universe jobs that exited via a signal did
   not properly report the signal. :ticket:`6538`
-  Fixed a bug where HTCondor would misreport the number of custom
   machine resources (GPUs) allocated to a job in certain cases.
   :ticket:`6549`
-  IPv4 addresses are now ignored when resolving a hostname and
   ``ENABLE_IPV4`` is set to ``False``. :ticket:`4881`
-  Fixed a race condition in the *condor_startd* that could result in
   skipping the code that makes sure that a job sandbox was deleted in
   the event that the *condor_starter* did not delete it. :ticket:`6524`
-  Fixed a bug in *condor_q* when both the **-tot** and **-global**
   options were used, that would result in no output when querying a
   *condor_schedd* running version 8.7 or later. :ticket:`6494`
-  Fixed a bug that could prevent grid universe batch jobs from working
   properly on Debian and Ubuntu. :ticket:`6560`

Version 8.7.6
-------------

Release Notes:

-  HTCondor version 8.7.6 released on January 4, 2018.

New Features:

-  Changed the default value of configuration parameter ``IS_OWNER`` to
   ``False``. The previous default value is now set as part of the
   ``use POLICY : Desktop`` configuration template. :ticket:`6463`
-  You may now use SCHEDD and JOB instead of MY and TARGET in
   ``SUBMIT_REQUIREMENTS`` expressions. :ticket:`4818`
-  Added cmake build option ``WANT_PYTHON_WHEELS`` and make target
   ``pypi_staging`` to build the framework for Python wheels. This
   option and target are not enabled by default and are not likely to
   work outside of Linux environments with a single Python installation.
   :ticket:`6486`
-  Added new job attributes BatchProject and BatchRuntime for grid-type
   batch jobs. They specify the project/allocation name and maximum
   runtime in seconds for the job that's submited to the underlying
   batch system. :ticket:`6451`
-  HTCondor now respects ``ATTR_JOB_SUCCESS_EXIT_CODE`` when sending job
   notifications. :ticket:`6432`
-  Added some graph metrics (height, width, etc.) to DAGMan's metrics
   file output. :ticket:`6470`
-  Removed Quill from HTCondor codebase. :ticket:`6496`

Bugs Fixed:

-  HTCondor now reports all submit warnings, not just the first one.
   :ticket:`6446`
-  The job log will no longer contain empty submit warnings. :ticket:`6465`
-  DAGMan previously connected to *condor_schedd* every time it
   detected an update in its internal state. This is too aggressive for
   rapidly changing DAGs, so we've changed the connection to happen in
   time intervals defined by ``DAGMAN_QUEUE_UPDATE_INTERVAL``, by
   default once every five minutes. :ticket:`6464`
-  DAGMan now enforces the ``DAGMAN_MAX_JOB_HOLDS`` limit by the number
   of held jobs in a cluster at the same time. Previously it counted all
   holds over the lifetime of a cluster, even if only a small number of
   them are active at the same time. :ticket:`6492`
-  Fixed a bug where on rare occasions the ``ShadowLog`` would become
   owned by root. :ticket:`6485`
-  Fixed a bug where using *condor_qedit* to change any of the
   concurrency limits of a job would have no effect. :ticket:`6448`
-  When ``copy_to_spool`` is set to ``True``, *condor_submit* now
   attempts to transfer the job exectuable only once per job cluster,
   instead of once per job. :ticket:`6459`
-  Fixed a bug that could result in an incorrect total reported by
   condor_rm when the **-totals** option is used. :ticket:`6450`

Version 8.7.5
-------------

Release Notes:

-  HTCondor version 8.7.5 released on November 14, 2017.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes a security-related
   bug described at
   `http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html>`_.
   :ticket:`6455`

Version 8.7.4
-------------

Release Notes:

-  HTCondor version 8.7.4 released on October 31, 2017.

New Features:

-  Added support for late materialization into *condor_dagman*. DAGs
   that include late materialized jobs now work correctly in both normal
   and recovery conditions. :ticket:`6274`
-  We now produce run time statistics in *condor_dagman*, tracking how
   much time DAGMan spends idle, how much time it spends submitting jobs
   and processing log files. This information could be used to determine
   why a DAG is submitting jobs slowly and how to optimize it. These
   statistics currently get dumped into the .dagman.out file at the end
   of a DAGs execution. :ticket:`6411`
-  Added a new knob to *condor_dagman*, ``DAGMAN_AGGRESSIVE_SUBMIT``.
   When set to True, this tells DAGMan to ignore the interval time limit
   for submitting jobs (defined by ``DAGMAN_USER_LOG_SCAN_INTERVAL``)
   and to continuously submit jobs until no more are ready, or until it
   hits a different limit. :ticket:`6386`
-  Added *status* command to *condor_annex*. This command invokes
   *condor_status* to display information about annex instances that
   have reported to the collector. It also gathers information about
   annex instances from EC2 and forwards that data to *condor_status*
   to detect instances which the collector does not yet or any longer
   know about. The annex instance ads fabricated for this purpose are
   not real slot ads, so some options you may know from *condor_status*
   do not apply to the *status* command of *condor_annex*. See
   the :doc:`/cloud-computing/index` section for
   details. :ticket:`6321`
-  Added a "merge" mode to *condor_status*. When invoked with the
   [**-merge** *<file>*] option, ads will be read from *file*, which
   can be ``-`` to indicate standard in, and compared to the ads
   selected by the query specified as usual by the remainder of the
   command-line. Ads will be compared on the basis of the sort key
   (which you can change with [**-sort** *<key>*]). *condor_status*
   will print three tables based on that comparison: the first table
   will be generated from those ads whose key was in the query but not
   in *file*; the second table will be generated from those ads whose
   key was appeared in both the query and in *file*, and the third table
   will be generated from those ads whose key appeared only in *file*.
   :ticket:`6321`
-  Added *off* command to *condor_annex*. This command invokes
   *condor_off* *-annex* appropriately. :ticket:`6408`
-  Updated *condor_annex* *-check-setup* to check collector security as
   well as connectivity. :ticket:`6322`
-  Added submit warnings. See section `Policy Configuration for Execute
   Hosts and for Submit
   Hosts <../admin-manual/policy-configuration.html>`_. :ticket:`5971`
-  ``openmpiscript`` now uses *condor_chirp* to run Open MPI's execute
   daemons (orted) directly under the *condor_starter* (instead of
   using SSH). ``openmpiscript`` now also puts information about the
   number of CPUs in the hostfile given to ``mpirun`` and now includes
   an option for jobs that intend to use hybrid Open MPI+OpenMP.
   :ticket:`6403`
-  The High Availability *condor_replication* daemon now works on
   machines using mixed IPV6/IPV4 addressing or using the
   *condor_shared_port* daemon. :ticket:`6413`
-  When Docker universe starts a job, it no longer uses the docker run
   command line to do so. Now, it first creates a container with docker
   create, then starts it with docker start. This allows HTCondor to
   better isolate errors at container creation time, but should not
   result in any user visible changes at run time. The ``StarterLog``
   will now always print the docker command line for the start and
   create, and not the run that it used to. :ticket:`6377`
-  When docker universe reports memory usage, it now reports the RSS
   (Resident Set Size) of the container, previously it reported RSS +
   page cache size :ticket:`6430`
-  Added support for both user and daemon authentication using the MUNGE
   service. :ticket:`6404`
-  Added a new **-macro** argument to *condor_config_val*. This
   argument causes *condor_config_val* to show the results of doing
   ``$()`` expansion of its arguments as if they were the result of a
   look up rather than the names of configuration variables to look up.
   :ticket:`6416`
-  CErequirements for the BLAHP can now be expressed in a simple form
   such as a string or nested ClassAd. :ticket:`6133`

Bugs Fixed:

-  Fixed a bug introduced in 8.7.0 where the job attributes
   RemoteUserCpu and RemoteSysCpu where never updated in the history
   file, or in condor_q output. The user log would show the correct
   values. :ticket:`6426`
-  The new behavior of the **-expand** command line argument of
   *condor_config_val* was breaking some scripts, so that
   functionality has been moved and **-expand** reverted to the pre
   8.7.2 behavior. :ticket:`6416`
-  Grid type boinc jobs are now considered running when they are
   reported as IN_PROGRESS. :ticket:`6405`

Version 8.7.3
-------------

Release Notes:

-  HTCondor version 8.7.3 released on September 12, 2017.

Known Issues:

-  Our current implementation of late materialization is incompatible
   with *condor_dagman* and will cause unexpected behavior, including
   failing without warning. This is a top-priority issue which aim to
   resolve in an upcoming release. :ticket:`6292`

New Features:

-  Changed *condor_top* tool to monitor the *condor_schedd* by
   default, to show more useful columns in the default view, to better
   format output when redirected or piped, and to optionally take input
   of two ClassAd files. :ticket:`6352`
-  Changed how ``auto`` works for ``ENABLE_IPV4`` and ``ENABLE_IPV6``.
   HTCondor now ignores addresses that are likely to be useless
   (loopback or link-local) unless no address is likely to be useful
   (private or public). :ticket:`6348`
-  Added support for Public Input Files in HTCondor jobs. This allows
   users to transfer input files over a publicly-available HTTP web
   service, which can benefit from caching proxies, load balancers, and
   other tools to improve file transfer performance. :ticket:`6356`
-  Added **-grid:ec2** to *condor_q* to avoid truncating AWS' new,
   longer, instance IDs. Replaced useless (given the instance ID)
   instance host name with the CMD column, to help distinguish EC2 jobs
   from each other. :ticket:`5478`
-  Added statistical output for job input files transferred from web
   servers using the curl_plugin tool. Statistics are stored in ClassAd
   format, saved by default to a transfer_history file in the local
   logs folder. :ticket:`6229`

Bugs Fixed:

-  Fixed some small memory leaks in the HTCondor daemons. :ticket:`6361`
-  Fixed a bug that would prevent dollar-dollar expansion from working
   correctly for parallel universe jobs running on partitionable slots.
   :ticket:`6370`

Version 8.7.2
-------------

Release Notes:

-  HTCondor version 8.7.2 released on June 22, 2017.

Known Issues:

-  Our current implementation of late materialization is incompatible
   with *condor_dagman* and will cause unexpected behavior, including
   failing without warning. This is a top-priority issue which aim to
   resolve in an upcoming release. :ticket:`6292`

New Features:

-  Improved the performance of the *condor_schedd* by setting the
   default for the knob ``SUBMIT_SKIP_FILECHECKS`` to true. This
   prevents the *condor_schedd* from checking the readability of all
   input files, and skips the creation of the output files on the submit
   side at submit time. Output files are now created either at transfer
   time, when file transfer is on, or by the job itself, if a shared
   filesystem is used. As a result of this change, it is possible that a
   job will run to completion, and only then is put on hold because the
   output file on the submit machine cannot be written. :ticket:`6220`
-  Changed *condor_submit* to not create empty stdout and stderr files
   before submitting jobs by default. This caused confusion for users,
   and slowed down the submission process. The older behavior, where
   *condor_submit* would fail if it could not create this files, is
   available when the parameter ``SUBMIT_SKIP_FILECHECKS`` is set to
   false. The default is now true. :ticket:`6220`
-  *condor_q* will now show expanded totals when querying a
   *condor_schedd* that is version 8.7.1 or later. The totals for the
   current user and for all users are provided by the *condor_schedd*.
   To get the old totals display set the configuration parameter
   ``CONDOR_Q_SHOW_OLD_SUMMARY`` to true. :ticket:`6254`
-  The *condor_annex* tool now logs to the user configuration
   directory. Added an audit log of *condor_annex* commands and their
   results. :ticket:`6267`
-  Changed *condor_off* so that the ``-annex`` flag implies the
   ``-master`` flag, since this is more likely to be the right thing.
   :ticket:`6266`
-  Added ``-status`` flag to *condor_annex*, which reports on instances
   which are running but not in the pool. :ticket:`6257`
-  If invoked with an annex name and duration (but not an instance or
   slot count), *condor_annex* will now adjust the duration of the
   named annex. :ticket:`6161`
-  Job input files which are downloaded from http:// web addresses now
   have mechanisms to recover from transfer failures. This should
   increase the reliability of using web-based input files, especially
   under slow and/or unstable network conditions. :ticket:`5886`
-  Reduced load on the *condor_collector* by optimizing queries
   performed when an HTCondor daemon needs to look up the address of
   another daemon. :ticket:`6223`
-  Reduced load on the *condor_collector* by optimizing queries
   performed when using condor_q with several different command-line
   options such as **-submitter** and **-global**. :ticket:`6222`
-  Added the *condor_top* tool, an automated version of the now-defunct
   *condor_top.pl* which uses the python bindings to monitor the status
   of daemons. :ticket:`6205`
-  Added a new option **-cron** to *condor_gpu_discovery* that allows
   it to be used directly as an executable of a *condor_startd* cron
   job. :ticket:`6012`
-  The configuration variable ``MAX_RUNNING_SCHEDULER_JOBS_PER_OWNER``
   was set to default to 100. It formerly had no default value. :ticket:`6260`
-  Added a parameter ``DEDICATED_SCHEDULER_USE_SERIAL_CLAIMS`` which
   defaults to false. When true, allows the dedicated schedule to use
   claimed/idle slots that the serial scheduler has claimed. :ticket:`6276`
-  The *condor_advertise* tool now assumes an update command if one is
   not specified on the command-line and attempts to determine exact
   command by inspecting the first ad to be advertised. :ticket:`6296`
-  Improved support for running several *condor_negotiator* s in a
   single pool. ``NEGOTIATOR_NAME`` now works like ``MASTER_NAME``.
   *condor_userprio* has a -name option to select a specific
   *condor_negotiator*. Accounting ads from multiple
   *condor_negotiator* s can co-exist in the *condor_collector*.
   :ticket:`5717`
-  Package EC2 Annex components in the condor-annex-ec2 sub RPM.
   :ticket:`6202`
-  Added configuration parameter ``ALTERNATE_JOB_SPOOL``, an expression
   evaluated against the job ad, which specifies an alternate spool
   directory to use for files related to that job. :ticket:`6221`

Bugs Fixed:

-  With an empty configuration file, HTCondor would behave as if
   ``ALLOW_ADMINISTRATOR`` were ``*``. Changed the default to
   ``$(CONDOR_HOST)``, which is much less insecure. :ticket:`6230`
-  Fixed a bug in the *condor_schedd* where it did not account for the
   initial state of late materialize jobs when calculating the running
   totals of jobs by state. This bug resulted in *condor_q* displaying
   incorrect totals when ``CONDOR_Q_SHOW_OLD_SUMMARY`` was set to false.
   :ticket:`6272`
-  Fixed a bug where the *condor_schedd* would incorrectly try to check
   the validity of output files and directories for late materialize
   jobs. The *condor_schedd* will now always skip file checks for late
   materialize jobs. :ticket:`6246`
-  Changed the output of the *condor_status* command so that the Load
   Average field now displays the load average of just the condor job
   running in that slot. Previously, load associated from outside of
   condor was proportionately distributed into the condor slots,
   resulting in much confusion. :ticket:`6225`
-  Illegal chars ('+', '.') are now prohibited in DAGMan node names.
   :ticket:`5966`
-  Improve audit log messages by including the connection ID and
   properly filtering out shadow and gridmanager modifications to the
   job queue log. :ticket:`6289`
-  *condor_root_switchboard* has been removed from the release, since
   PrivSep is no longer supported. :ticket:`6259`

Version 8.7.1
-------------

Release Notes:

-  HTCondor version 8.7.1 released on April 24, 2017.

New Features:

-  Previously, when the number of forked children processing Collector
   queries surpassed the maximum set by the configuration knob
   ``COLLECTOR_QUERY_WORKERS``, the Collector handled all new incoming
   queries in-processes (i.e. without forking). As processing a query
   and sending out the result to the network could take a long time, the
   result of servicing such queries in-process in the Collector is
   likely to drop a lot of updates. So now in v8.7.1, instead of
   servicing such queries in-process, they are queued up for servicing
   as soon as query worker child processes become available. The
   configuration knob ``COLLECTOR_QUERY_WORKERS_PENDING`` was
   introduced; see :doc:`/classad-attributes/collector-classad-attributes`.
   :ticket:`6192`
-  Default value for ``COLLECTOR_QUERY_WORKERS`` changed from 2 to 4.
   :ticket:`6192`
-  Introduced configuration macro
   ``COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO`` so that the
   collector prioritizes queries that are important for the operation of
   the pool (such as queries from the negotiator) ahead of servicing
   user invocations of *condor_status*. :ticket:`6192`
-  Introduced configuration macro ``COLLECTOR_QUERY_MAX_WORKTIME`` to
   define the maximum amount of time the collector may service a query
   from a client like condor_status. See
   :doc:`/classad-attributes/collector-classad-attributes` :ticket:`6192`
-  Added several new statistics on collector query performance into the
   Collector ClassAd, including ``ActiveQueryWorkers``,
   ``ActiveQueryWorkersPeak``, ``PendingQueries``,
   ``PendingQueriesPeak``, ``DroppedQueries``, and
   ``RecentDroppedQueries``. See
   :doc:`/classad-attributes/collector-classad-attributes` :ticket:`6192`
-  Further refinement and initial documentation of the HTCondor Annex.
   :ticket:`6147`
   :ticket:`6149`
   :ticket:`6150`
   :ticket:`6155`
   :ticket:`6157`
   :ticket:`6184`
   :ticket:`6196`
   :ticket:`6216`
   :ticket:`6218`
-  Docker universe jobs can now use condor_chirp command (if it is in
   the image). :ticket:`6162`
-  In the Job Router, when a candidate job matches multiple routes, the
   first route is now always selected. The old behavior of spreading
   jobs across all matching routes round-robin style can be enabled by
   setting the new configuration parameter
   ``JOB_ROUTER_ROUND_ROBIN_SELECTION`` to ``True``. :ticket:`6190`
-  The *condor_schedd* now keeps a count of jobs by state for each
   owner and submitter and will report them to *condor_q*. Condorq will
   display these totals unless the new configuration parameter
   ``CONDOR_Q_SHOW_OLD_SUMMARY`` is set to true. In 8.7.1 this parameter
   defaults to true. :ticket:`6160`
-  Milestone 1 for late materialization in the *condor_schedd* was
   completed. This milestone adds the undocumented option **-factory**
   to *condor_q* that can be used to submit a late materializing job
   cluster to the *condor_schedd*. The *condor_schedd* will refuse the
   submission unless the configuration parameter
   ``SCHEDD_ALLOW_LATE_MATERIALIZATION`` is set to true. :ticket:`6212`
-  Increased the default value for configuration parameter
   ``NEGOTIATOR_SOCKET_CACHE_SIZE`` to 500. :ticket:`6165`
-  Added new DaemonCore statistics UdpQueueDepth to measure the number
   of bytes in the UDP receive queue for daemons with a UDP command
   port. :ticket:`6183`
-  Improved speed of handling queries to the collector by caching the
   the configuration knob SHARED_PORT_ADDRESS_REWRITING. :ticket:`6187`
-  The *condor_collector* on Linux now handles some queries in process
   and some by forking a child process. This allows it to avoid the
   overhead of forking to handle queries that will take little time. The
   policy for deciding which queries to handle in process is controlled
   by a new configuration parameter ``HANDLE_QUERY_IN_PROC_POLICY``.
   :ticket:`6191`
-  Added **-limit** option to *condor_status* and changed the
   *condor_collector* to honor it. :ticket:`6198`
-  *condor_submit* was changed to use the same utility library that the
   submit python bindings use. This should help insure that submit via
   python bindings will give the same results as using *condor_submit*.
   :ticket:`6181`.

Bugs Fixed:

-  None.

Version 8.7.0
-------------

Release Notes:

-  HTCondor version 8.7.0 released on March 2, 2017.

New Features:

-  Optimized the code that reads reads ClassAds off the wire making the
   maximum possible update rate for the Collector about 1.7 times higher
   than it was before. :ticket:`6105`
   :ticket:`6130`
-  New statistics have been added to the Collector ad to show time spent
   handling queries. :ticket:`6123`
-  Changed the formatting of the printing of ClassAd expressions with
   parentheses. Now there is no space character after every open
   parenthesis, or before every close parenthesis This looks more
   natural, is somewhat faster for the condor to parse, and saves space.
   That is, an expression that used to print like

   .. code-block:: text

       ( ( ( foo ) ) )

   now will print like this

   .. code-block:: text

       (((foo)))

   :ticket:`6082`

-  Technology preview of the HTCondor Annex. The HTCondor Annex allows
   one to extend their HTCondor pool into the cloud.
   `https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUseCondorAnnexWithOnDemandInstances <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUseCondorAnnexWithOnDemandInstances>`_
   :ticket:`6121`
-  Added **-annex** option to *condor_status* and *condor_off*.
   Requires an argument; the request is constrained to match machines
   whose ``AnnexName`` ClassAd attribute matches the argument. :ticket:`6116`
   :ticket:`6117`
-  A refreshed X.509 proxy is now forwarded to the remote cluster in
   Bosco. :ticket:`5841`
-  Added several new statistics to the Negotiator ad, mainly detailing
   how time is spent in the negotiation cycle. :ticket:`6060`

Bugs Fixed:

-  Removed redundant updates to the job queue by the Job Router.
   :ticket:`6102`
