      

Development Release Series 8.7
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 8.7.10
--------------

Release Notes:

-  HTCondor version 8.7.10 released on October 31, 2018.

New Features:

-  One can now submit an interactive Docker job. `(Ticket
   #6710). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6710>`__
-  Added the ``SINGULARITY_EXTRA_ARGUMENTS`` configuration parameter.
   Administrators can now append arguments to the Singularity command
   line. `(Ticket
   #6731). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6731>`__
-  The MUNGE security method is now supported on all Linux platforms.
   `(Ticket
   #6713). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6713>`__
-  The grid universe can now be used to create and manage VM instances
   in Microsoft Azure, using the new grid type **azure**. `(Ticket
   #6176). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6176>`__
-  Added single-node configuration package to facilitate using a
   personal HTCondor. `(Ticket
   #6709). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6709>`__
-  Added a new file transfer plugin, multifile\_curl\_plugin which is
   able to transfer multiple files with only a single invocation of the
   plugin, preserving the TCP connection. It also takes a ClassAd as
   input data, which will allow us to pass in more complex input than
   the existing curl\_plugin. `(Ticket
   #6499). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6499>`__
-  Added two new policies, ``PREEMPT_IF_RUNTIME_EXCEEDS`` and
   ``HOLD_IF_RUNTIME_EXCEEDS``. The former is (intended to be) identical
   to the policy ``LIMIT_JOB_RUNTIMES``, except without ordering
   constraints with respect to other policy macros. (``ALWAYS_RUN_JOBS``
   must still come before any other policy macro, but unlike
   ``LIMIT_JOB_RUNTIMES``, ``PREEMPT_IF_RUNTIME_EXCEEDS`` may come after
   other policy macros.) Additionally, both of the new policies function
   while the machine is draining. `(Ticket
   #6701). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6701>`__
-  *condor\_submit* will no longer attempt to read submit commands from
   standard input when there is no submit file if a queue statement and
   at least one submit command is provided on the command line. `(Ticket
   #6581). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6581>`__
-  If the first line of the job’s executable starts with ``#!``
   *condor\_submit* will now check that line for a Windows/DOS line
   ending, and if it finds one, it will not submit the job because such
   a script will not be able to start on Unix or Linux platforms. This
   check can be changed from an error to a warning by submitting with
   the **allow-crlf-script** option. `(Ticket
   #6660). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6660>`__
-  Added support for spaces in remapped file paths in *condor\_submit*.
   `(Ticket
   #6642). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6642>`__
-  Improved error handling during SSL authentication. `(Ticket
   #6720). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6720>`__
-  Improved throughput when submitting a large number of Condor-C jobs.
   Previously, Condor-C jobs could remain held for a long time in the
   remote *condor\_schedd*\ ’s queue while other jobs were being
   submitted. `(Ticket
   #6716). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6716>`__
-  Updated default configuration parameters to improve performance for
   large pools and gives users a better experience. `(Ticket
   #6768). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6768>`__
   `(Ticket
   #6787). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6787>`__
-  Added new configuration parameter ``TRUST_LOCAL_UID_DOMAIN``. It
   works like ``TRUST_UID_DOMAIN``, but only applies when the
   *condor\_shadow* and *condor\_starter* are on the same machine.
   `(Ticket
   #6785). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6785>`__
-  Added a new configuration parameter
   ``SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES``. It determines whether file
   transfer should default to YES, NO, or AUTO when when the submit file
   does not supply a value for ``should_transfer_files`` and file
   transfer is not forced on or off by some other parameter in the
   submit file. Prior to this addition, *condor\_submit* would always
   default to AUTO. `(Ticket
   #6784). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6784>`__
-  Added new statistics attributes about the lifetime of the
   *condor\_starter* to the *condor\_startd* Ad. This attributes are
   intended to aid in writing policy expressions that prevent a node
   from matching jobs when the node has frequently failed to start jobs.
   `(Ticket
   #6698). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6698>`__
-  For grid-type ``boinc`` jobs, the following job ad attributes can be
   used to to set the BOINC job template parameters of the same name:
   ``rsc_fpops_est``, ``rsc_fpops_bound``, ``rsc_memory_bound``,
   ``rsc_disk_bound``, ``delay_bound``, and ``app_version_num``.
   `(Ticket
   #6760). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6760>`__
-  Daemons now advertise ``DaemonLastReconfigTime`` in all of their ads.
   This is either the boot time of the time, or the last time
   *condor\_reconfig* was run on that daemon. `(Ticket
   #6758). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6758>`__

Bugs Fixed:

-  Fixed a bug where ``PREEMPT`` was not be evaluated if the machine was
   draining. This prevent the ``HOLD_IF`` series of policies from
   functioning properly in that situation. `(Ticket
   #6697). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6697>`__
-  Fixed a bug that occurred when starting Docker Universe jobs that
   would cause the *condor\_starter* to crash and the jobs to cycle
   between ``running`` and ``idle`` status. `(Ticket
   #6725). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6725>`__
-  Fixed a bug that could cause a job to go into a rapid cycle between
   ``running`` and ``idle`` status if a policy expression evaluated to
   ``Undefined`` during input file transfer. `(Ticket
   #6728). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6728>`__
-  Fixed bugs where small jobs would not match partitionable slots when
   Group Quotas were enabled. `(Ticket
   #6714). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6714>`__
   `(Ticket
   #6750). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6750>`__
-  Fixed a bug that prevented *condor\_tail* ``-stderr`` from working.
   `(Ticket
   #6755). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6755>`__
-  *condor\_who* now works properly on macOS. `(Ticket
   #6652). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6652>`__
-  Fixed output of *condor\_q* -global when printing in JSON, XML, or
   new ClassAd format. `(Ticket
   #6761). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6761>`__
-  Fixed a bug that could cause *condor\_wait* and the python bindings
   on Windows to repeat events when reading the job event log. `(Ticket
   #6752). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6752>`__
-  Added missing Accounting, Credd, and Defrag AdTypes to the python
   bindings AdTypes enumeration. `(Ticket
   #6737). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6737>`__
-  Fixed a bug that caused late materialization jobs to handle the
   ``getenv`` submit command incorrectly. `(Ticket
   #6723). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6723>`__
-  Fixed an inefficiency in the SetAttribute remote procedure call that
   could sometimes result in noticeable performance reduction of the
   *condor\_schedd*. Removing this inefficiency will allow a single
   *condor\_schedd* to handle updates from a larger number of running
   jobs. `(Ticket
   #6732). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6732>`__
-  The *condor\_gangliad* can now publish accounting Ads as Ganglia
   metrics. `(Ticket
   #6757). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6757>`__
-  *condor\_ssh\_to\_job* is now configured to use the IPv4 loopback
   address. This avoids problems when IPv6 is present but not enabled.
   `(Ticket
   #6711). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6711>`__
-  Fixed a bug where the ``JobSuccessExitCode`` was not set. `(Ticket
   #6786). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6786>`__
-  Fixed a problem with the EC2 configuration file was present in the
   tarballs. `(Ticket
   #6797). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6797>`__

Version 8.7.9
-------------

Release Notes:

-  HTCondor version 8.7.9 released on August 1, 2018.

Known Issues:

-  Amazon Web Services is deprecating support for the Node.js 4.3
   runtime, used by *condor\_annex*, on July 31 (2018). If you ran the
   *condor\_annex* setup command with a previous version, you must
   update your account to use the new runtime. Follow the link below for
   simple instructions. Accounts setup with this version of HTCondor
   will use the new runtime.
   `https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUpgradeTheAnnexRuntime <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUpgradeTheAnnexRuntime>`__
   `(Ticket
   #6665). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6665>`__
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

-  The HTCondor Python bindings Submit class can now be initialized from
   an existing *condor\_submit* file including the QUEUE statement.
   Python bindings Submit class also can now submit a job for each step
   of a Python iterator. `(Ticket
   #6679). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6679>`__
-  VM universe jobs are now given time to shutdown after a power-off
   signal when they are evicted gracefully. `(Ticket
   #6705). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6705>`__
-  The ``NETWORK_HOSTNAME`` configuration parameter can now be set to a
   fully-qualified hostname that’s an alias of one of the machine’s
   interfaces. `(Ticket
   #6702). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6702>`__
-  Added a new tool, *condor\_now*, which tries to run the specified job
   now. You specify two jobs that you own from the same
   *condor\_schedd*: the now-job and the vacate-job. The latter is
   immediately vacated; after the vacated job terminates, if the
   *condor\_schedd* still has the claim to the vacated job’s slot (and
   it usually will), the *condor\_schedd* will immediately start the
   now-job on that slot. The now-job must be idle and the vacate-job
   must be running. If you’re a queue super-user, the jobs must have the
   same owner, but that owner doesn’t have to be you. `(Ticket
   #6659). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6659>`__
-  HTCondor now supports backfill while draining. You may now use the
   *condor\_drain* command, or configure the *condor\_defrag* daemon, to
   set a different ``START`` expression for the duration of the
   draining. See the definition of ``DEFRAG_DRAINING_START_EXPR`` (
   `3.5.33 <ConfigurationMacros.html#x33-2290003.5.33>`__) and the
   *condor\_drain* manual ( `12 <Condordrain.html#x111-77600012>`__) for
   details. See also the known issues above for information which may
   influence your choice of ``START`` expressions. `(Ticket
   #6664). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6664>`__
-  Docker universe jobs now run with the supplemental group ids of the
   running user, not just the primary group. `(Ticket
   #6658). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6658>`__
-  Added proxy delegation for vanilla universe jobs that define a X509
   proxy but do not use the file transfer mechanism. `(Ticket
   #6587). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6587>`__
-  Added configuration parameters ``GAHP_SSL_CADIR`` and
   ``GAHP_SSL_CAFILE`` to specify trusted CAs when authenticating EC2
   and GCE servers. This used by be controlled by ``SOAP_SSL_CA_DIR``
   and ``SOAP_SSL_CAFILE``, which have been removed. `(Ticket
   #6684). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6684>`__
-  HTCondor can now read the new credentials file format used by the
   Goggle Cloud Platform command-line tools. `(Ticket
   #6657). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6657>`__

Bugs Fixed:

-  Fixed a bug where an ill-formed startd docker image cache file could
   cause the starter to crash starting docker universe jobs. `(Ticket
   #6699). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6699>`__
-  Fixed a bug that would prevent environment variables defined in a job
   submit file from appearing in jobs running in Singularity containers
   using Singularity version 2.4 and greater. `(Ticket
   #6656). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6656>`__
-  Fixed a problem where a *condor\_vacate\_job*, when passed the
   **-fast** flag, would leave the corresponding slot stuck in
   “Preempting/Vacating” state until the job lease expired. `(Ticket
   #6663). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6663>`__
-  Fixed a problem where *condor\_annex*\ ’s setup routine, if no region
   had been specified on the command line, would write a configuration
   for a bogus region rather than the default one. `(Ticket
   #6666). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6666>`__
-  The *condor\_history\_helper* program was removed. *condor\_history*
   is now used by the *condor\_schedd* to help with remote history
   queries. `(Ticket
   #6247). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6247>`__

Version 8.7.8
-------------

Release Notes:

-  HTCondor version 8.7.8 released on May 10, 2018.

New Features:

-  *condor\_annex* may now be setup in multiple regions simultaneously.
   Use the **-aws-region** flag with **-setup** to add new regions. Use
   the **-aws-region** flag with other *condor\_annex* commands to
   choose which region to operate in. You may change the default region
   by setting ``ANNEX_DEFAULT_AWS_REGION``. `(Ticket
   #6632). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6632>`__
-  Added default AMIs for all four US regions to simplify using
   *condor\_annex* in those regions. `(Ticket
   #6633). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6633>`__
-  HTCondor will no longer mangle ``CUDA_VISIBLE_DEVICES`` or
   ``GPU_DEVICE_ORDINAL`` if those environment variables are set when it
   starts up. As a result, HTCondor will report GPU usage with the
   original device index (rather than starting over at 0). `(Ticket
   #6584). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6584>`__
-  When reporting ``GPUsUsage``, HTCondor now also reports
   ``GPUsMemoryUsage``. This is like ``MemoryUsage``, except it is the
   peak amount of GPU memory used by the job. This feature only works
   for nVidia GPUs. `(Ticket
   #6544). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6544>`__
-  Improved error messages when delegation of an X.509 proxy fails.
   `(Ticket
   #6575). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6575>`__
-  *condor\_q* will no longer limit the width of the output to 80
   columns when it outputs to a file or pipe. `(Ticket
   #6643). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6643>`__
-  Submission of jobs via the Python bindings Submit class will now
   attempt to put all jobs submitted in a single transaction under the
   same ClusterId. `(Ticket
   #6649). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6649>`__
-  Added support for *condor\_schedd* query options in the Python
   bindings. `(Ticket
   #6619). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6619>`__
-  Eliminated SOAP support. `(Ticket
   #6648). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6648>`__

Bugs Fixed:

-  Fixed a problem where, when starting enough *condor\_annex* instances
   simultaneously, some (approximately 1 in 100) instances would neither
   join the pool nor terminate themselves. `(Ticket
   #6638). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6638>`__
-  When running in a HAD setup, there is a configuration parameter,
   ``COLLECTOR_HOST_FOR_NEGOTIATOR`` which tells the active negotiator
   which collector to prefer. Previously, this parameter had no default,
   so the negotiator might arbitrarily chose a far-away collector. Now
   this knob defaults to the local collector in a HAD setup. `(Ticket
   #6616). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6616>`__
-  Fixed a bug when running in a configuration with more than one
   *condor\_collector*, the *condor\_negotiator* would only send the
   accounting ads to one of them. The result of this bug is that the
   *condor\_userprio* tool would show now results about half of the time
   it was run. `(Ticket
   #6615). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6615>`__
-  Fixed a bug where *condor\_annex* would fail with a malformed
   authorization header when using AWS resources in a region other than
   ``us-east-1``. `(Ticket
   #6629). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6629>`__
-  Fixed a bug that prevented Docker universe jobs with no executable
   listed in the submit file from running. `(Ticket
   #6612). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6612>`__
-  Fixed a bug where the *condor\_starter* would fail with an error
   after a docker job exits. `(Ticket
   #6623). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6623>`__
-  Fixed a bug where *condor\_userprio* would always show zero resources
   in use when NEGOTIATOR\_CONSIDER\_PREEMPTION=false was set. `(Ticket
   #6621). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6621>`__
-  Fixed a bug where “.update.ad” was not being updated atomically.
   `(Ticket
   #6591). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6591>`__
-  Fixed a bug that could cause a machine slot to become stuck in the
   Claimed/Busy state after a job completes. `(Ticket
   #6597). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6597>`__

Version 8.7.7
-------------

Release Notes:

-  HTCondor version 8.7.7 released on March 13, 2018.

New Features:

-  *condor\_ssh\_to\_job* now works with Docker Universe, the
   interactive shell is started inside the container. This assume that
   there is a shell executable inside the container, but not necessarily
   an sshd. `(Ticket
   #6558). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6558>`__
-  Improved error messages in the job log for Docker universe jobs that
   do not start. `(Ticket
   #6567). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6567>`__
-  Release a 32-bit *condor\_shadow* for Enterprise Linux 7 platforms.
   `(Ticket
   #6495). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6495>`__
-  HTCondor now reports, in the job ad and user log, which custom
   machine resources were assigned to the slot in which the job ran.
   `(Ticket
   #6549). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6549>`__
-  HTCondor now reports ``CPUsUsage`` for each job. This attribute is
   like ``MemoryUsage`` and ``DiskUsage``, except it is the average
   number of CPUs used by the job. `(Ticket
   #6477). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6477>`__
-  The ``use feature: GPUs`` metaknob now causes HTCondor to report
   ``GPUsUsage`` for each job. This is like ``CPUsUsage``, except it is
   the average number of GPUs used by the job. This feature only works
   for nVIDIA GPUs. `(Ticket
   #6477). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6477>`__
-  Administrators may now, for each custom machine resource, define a
   custom resource monitor. Such a script reports the usage(s) of each
   instance of the corresponding machine resource since the last time it
   reported; HTCondor aggregates these reports between resource
   instances and over time to produce a ``<Resource>Usage`` attribute,
   which is like ``GPUsUsage``, except for the custom machine resource
   in question. `(Ticket
   #6477). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6477>`__
-  The *condor\_startd* now periodically writes a file to each job’s
   sandbox named “.update.ad”. This file is a copy of the slot’s machine
   ad, but unlike “.machine.ad”, it is regularly updated. Jobs may read
   this file to observe their own usage attributes. `(Ticket
   #6477). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6477>`__
-  A new option **-unmatchable** was added to *condor\_q* that causes
   *condor\_q* to show only jobs that will not match any of the
   available slots. `(Ticket
   #6529). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6529>`__
-  OpenMPI jobs launched in the parallel universe via ``openmpiscript``
   now work with shared file systems (again). `(Ticket
   #6556). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6556>`__
-  Allow a parallel universe job with parallel scheduling group to
   select a new parallel scheduling group when held and released.
   `(Ticket
   #6516). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6516>`__
-  Allow p-slot preemption to work with parallel universe. `(Ticket
   #6517). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6517>`__
-  Added the ability in *condor\_dagman* to specify submit files with
   spaces in their path names. Paths that include spaces must be wrapped
   in quotes (i.e. JOB A "/path to/job.sub"). `(Ticket
   #6389). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6389>`__
-  Added the ability in *condor\_submit* to specify executable, error
   and output files with spaces in their paths. Previously, adding
   whitespace to these fields would result in an error claiming certain
   attributes could only take exactly one argument. Now, whitespace is
   treated as part of the path. `(Ticket
   #6389). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6389>`__
-  An IPv6 address can now be specified in the configuration file either
   with or without square brackets in most cases. If specifying a port
   number in the same value, the square brackets are required. If using
   a wild card to specify a range of possible addresses, square brackets
   are not allowed. `(Ticket
   #5697). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5697>`__
-  Improved support for IPv6 link-local addresses, in particular using
   the correct scope id. Using a wild card or device name in
   ``NETWORK_INTERFACE`` now works properly when ``NO_DNS`` is set to
   ``True``. `(Ticket
   #6518). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6518>`__
-  Python bindings installed via pip on a system without a HTCondor
   install (i.e. without a ``condor_config`` present) will use a “null”
   config and print a warning. `(Ticket
   #6515). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6515>`__
-  The new configuration parameter ``NEGOTIATOR_JOB_CONSTRAINT`` defines
   an expression which constrains which job ads are considered for
   matchmaking by the *condor\_negotiator*. `(Ticket
   #6250). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6250>`__
-  The *condor\_startd* will now keep trying to delete a job sandbox
   until it succeeds. The retries are attempted with an exponential back
   off in frequency. `(Ticket
   #6500). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6500>`__
-  *condor\_q* will no longer batch jobs with different cluster ids
   together unless they have the same JobBatchName attribute or are in
   the same DAG. `(Ticket
   #6532). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6532>`__
-  *condor\_q* will now sort jobs by job id when the **-long** argument
   is used. `(Ticket
   #6287). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6287>`__
-  Improve the performance of reading and writing ClassAds to the
   network. The performance of reading ClassAds from UDP is particularly
   improved, up to 20% faster than previously. `(Ticket
   #6555). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6555>`__
   `(Ticket
   #6561). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6561>`__
-  Several minor performance improvements. `(Ticket
   #6550). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6550>`__
   `(Ticket
   #6551). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6551>`__
   `(Ticket
   #6565). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6565>`__
   `(Ticket
   #6566). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6566>`__
-  Removed configuration parameters ``ENABLE_ADDRESS_REWRITING`` and
   ``SHARED_PORT_ADDRESS_REWRITING``. `(Ticket
   #6525). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6525>`__
-  Removed the deprecated AvailStats attribute from the machine ad. This
   was being computing incorrectly, and apparently never used. `(Ticket
   #6526). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6526>`__
-  Added basic support for a "Credential Management" subsystem which
   will eventually be used to support interaction with OAuth services
   (like SciTokens, Box.com, Google Drive, DropBox, etc.). Still in
   preliminary phases and not really ready for public use. `(Ticket
   #6513). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6513>`__

Bugs Fixed:

-  Fixed a bug where Docker universe jobs that exited via a signal did
   not properly report the signal. `(Ticket
   #6538). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6538>`__
-  Fixed a bug where HTCondor would misreport the number of custom
   machine resources (GPUs) allocated to a job in certain cases.
   `(Ticket
   #6549). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6549>`__
-  IPv4 addresses are now ignored when resolving a hostname and
   ``ENABLE_IPV4`` is set to ``False``. `(Ticket
   #4881). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=4881>`__
-  Fixed a race condition in the *condor\_startd* that could result in
   skipping the code that makes sure that a job sandbox was deleted in
   the event that the *condor\_starter* did not delete it. `(Ticket
   #6524). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6524>`__
-  Fixed a bug in *condor\_q* when both the **-tot** and **-global**
   options were used, that would result in no output when querying a
   *condor\_schedd* running version 8.7 or later. `(Ticket
   #6494). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6494>`__
-  Fixed a bug that could prevent grid universe batch jobs from working
   properly on Debian and Ubuntu. `(Ticket
   #6560). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6560>`__

Version 8.7.6
-------------

Release Notes:

-  HTCondor version 8.7.6 released on January 4, 2018.

New Features:

-  Changed the default value of configuration parameter ``IS_OWNER`` to
   ``False``. The previous default value is now set as part of the
   ``use POLICY : Desktop`` configuration template. `(Ticket
   #6463). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6463>`__
-  You may now use SCHEDD and JOB instead of MY and TARGET in
   ``SUBMIT_REQUIREMENTS`` expressions. `(Ticket
   #4818). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=4818>`__
-  Added cmake build option ``WANT_PYTHON_WHEELS`` and make target
   ``pypi_staging`` to build the framework for Python wheels. This
   option and target are not enabled by default and are not likely to
   work outside of Linux environments with a single Python installation.
   `(Ticket
   #6486). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6486>`__
-  Added new job attributes BatchProject and BatchRuntime for grid-type
   batch jobs. They specify the project/allocation name and maximum
   runtime in seconds for the job that’s submited to the underlying
   batch system. `(Ticket
   #6451). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6451>`__
-  HTCondor now respects ``ATTR_JOB_SUCCESS_EXIT_CODE`` when sending job
   notifications. `(Ticket
   #6432). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6432>`__
-  Added some graph metrics (height, width, etc.) to DAGMan’s metrics
   file output. `(Ticket
   #6470). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6470>`__
-  Removed Quill from HTCondor codebase. `(Ticket
   #6496). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6496>`__

Bugs Fixed:

-  HTCondor now reports all submit warnings, not just the first one.
   `(Ticket
   #6446). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6446>`__
-  The job log will no longer contain empty submit warnings. `(Ticket
   #6465). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6465>`__
-  DAGMan previously connected to *condor\_schedd* every time it
   detected an update in its internal state. This is too aggressive for
   rapidly changing DAGs, so we’ve changed the connection to happen in
   time intervals defined by ``DAGMAN_QUEUE_UPDATE_INTERVAL``, by
   default once every five minutes. `(Ticket
   #6464). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6464>`__
-  DAGMan now enforces the ``DAGMAN_MAX_JOB_HOLDS`` limit by the number
   of held jobs in a cluster at the same time. Previously it counted all
   holds over the lifetime of a cluster, even if only a small number of
   them are active at the same time. `(Ticket
   #6492). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6492>`__
-  Fixed a bug where on rare occasions the ``ShadowLog`` would become
   owned by root. `(Ticket
   #6485). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6485>`__
-  Fixed a bug where using *condor\_qedit* to change any of the
   concurrency limits of a job would have no effect. `(Ticket
   #6448). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6448>`__
-  When ``copy_to_spool`` is set to ``True``, *condor\_submit* now
   attempts to transfer the job exectuable only once per job cluster,
   instead of once per job. `(Ticket
   #6459). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6459>`__
-  Fixed a bug that could result in an incorrect total reported by
   condor\_rm when the **-totals** option is used. `(Ticket
   #6450). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6450>`__

Version 8.7.5
-------------

Release Notes:

-  HTCondor version 8.7.5 released on November 14, 2017.

New Features:

-  None.

Bugs Fixed:

-  *Security Item*: This release of HTCondor fixes a security-related
   bug described at
   `http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html <http://htcondor.org/security/vulnerabilities/HTCONDOR-2017-0001.html>`__.
   `(Ticket
   #6455). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6455>`__

Version 8.7.4
-------------

Release Notes:

-  HTCondor version 8.7.4 released on October 31, 2017.

New Features:

-  Added support for late materialization into *condor\_dagman*. DAGs
   that include late materialized jobs now work correctly in both normal
   and recovery conditions. `(Ticket
   #6274). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6274>`__
-  We now produce run time statistics in *condor\_dagman*, tracking how
   much time DAGMan spends idle, how much time it spends submitting jobs
   and processing log files. This information could be used to determine
   why a DAG is submitting jobs slowly and how to optimize it. These
   statistics currently get dumped into the .dagman.out file at the end
   of a DAGs execution. `(Ticket
   #6411). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6411>`__
-  Added a new knob to *condor\_dagman*, ``DAGMAN_AGGRESSIVE_SUBMIT``.
   When set to True, this tells DAGMan to ignore the interval time limit
   for submitting jobs (defined by ``DAGMAN_USER_LOG_SCAN_INTERVAL``)
   and to continuously submit jobs until no more are ready, or until it
   hits a different limit. `(Ticket
   #6386). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6386>`__
-  Added *status* command to *condor\_annex*. This command invokes
   *condor\_status* to display information about annex instances that
   have reported to the collector. It also gathers information about
   annex instances from EC2 and forwards that data to *condor\_status*
   to detect instances which the collector does not yet or any longer
   know about. The annex instance ads fabricated for this purpose are
   not real slot ads, so some options you may know from *condor\_status*
   do not apply to the *status* command of *condor\_annex*. See
   section \ `6 <CloudComputing.html#x58-4970006>`__ for details.
   `(Ticket
   #6321). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6321>`__
-  Added a “merge” mode to *condor\_status*. When invoked with the
   [**-merge  **\ *<file>*] option, ads will be read from *file*, which
   can be ``-`` to indicate standard in, and compared to the ads
   selected by the query specified as usual by the remainder of the
   command-line. Ads will be compared on the basis of the sort key
   (which you can change with [**-sort  **\ *<key>*]). *condor\_status*
   will print three tables based on that comparison: the first table
   will be generated from those ads whose key was in the query but not
   in *file*; the second table will be generated from those ads whose
   key was appeared in both the query and in *file*, and the third table
   will be generated from those ads whose key appeared only in *file*.
   `(Ticket
   #6321). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6321>`__
-  Added *off* command to *condor\_annex*. This command invokes
   *condor\_off* *-annex* appropriately. `(Ticket
   #6408). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6408>`__
-  Updated *condor\_annex* *-check-setup* to check collector security as
   well as connectivity. `(Ticket
   #6322). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6322>`__
-  Added submit warnings. See section
   `3.7.2 <PolicyConfigurationforExecuteHostsandforSubmitHosts.html#x35-2670003.7.2>`__.
   `(Ticket
   #5971). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5971>`__
-  ``openmpiscript`` now uses *condor\_chirp* to run Open MPI’s execute
   daemons (orted) directly under the *condor\_starter* (instead of
   using SSH). ``openmpiscript`` now also puts information about the
   number of CPUs in the hostfile given to ``mpirun`` and now includes
   an option for jobs that intend to use hybrid Open MPI+OpenMP.
   `(Ticket
   #6403). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6403>`__
-  The High Availability *condor\_replication* daemon now works on
   machines using mixed IPV6/IPV4 addressing or using the
   *condor\_shared\_port* daemon. `(Ticket
   #6413). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6413>`__
-  When Docker universe starts a job, it no longer uses the docker run
   command line to do so. Now, it first creates a container with docker
   create, then starts it with docker start. This allows HTCondor to
   better isolate errors at container creation time, but should not
   result in any user visible changes at run time. The ``StarterLog``
   will now always print the docker command line for the start and
   create, and not the run that it used to. `(Ticket
   #6377). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6377>`__
-  When docker universe reports memory usage, it now reports the RSS
   (Resident Set Size) of the container, previously it reported RSS +
   page cache size `(Ticket
   #6430). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6430>`__
-  Added support for both user and daemon authentication using the MUNGE
   service. `(Ticket
   #6404). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6404>`__
-  Added a new **-macro** argument to *condor\_config\_val*. This
   argument causes *condor\_config\_val* to show the results of doing
   ``$()`` expansion of its arguments as if they were the result of a
   look up rather than the names of configuration variables to look up.
   `(Ticket
   #6416). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6416>`__
-  CErequirements for the BLAHP can now be expressed in a simple form
   such as a string or nested ClassAd. `(Ticket
   #6133). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6133>`__

Bugs Fixed:

-  Fixed a bug introduced in 8.7.0 where the job attributes
   RemoteUserCpu and RemoteSysCpu where never updated in the history
   file, or in condor\_q output. The user log would show the correct
   values. `(Ticket
   #6426). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6426>`__
-  The new behavior of the **-expand** command line argument of
   *condor\_config\_val* was breaking some scripts, so that
   functionality has been moved and **-expand** reverted to the pre
   8.7.2 behavior. `(Ticket
   #6416). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6416>`__
-  Grid type boinc jobs are now considered running when they are
   reported as IN\_PROGRESS. `(Ticket
   #6405). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6405>`__

Version 8.7.3
-------------

Release Notes:

-  HTCondor version 8.7.3 released on September 12, 2017.

Known Issues:

-  Our current implementation of late materialization is incompatible
   with *condor\_dagman* and will cause unexpected behavior, including
   failing without warning. This is a top-priority issue which aim to
   resolve in an upcoming release. `(Ticket
   #6292). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6292>`__

New Features:

-  Changed *condor\_top* tool to monitor the *condor\_schedd* by
   default, to show more useful columns in the default view, to better
   format output when redirected or piped, and to optionally take input
   of two ClassAd files. `(Ticket
   #6352). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6352>`__
-  Changed how ``auto`` works for ``ENABLE_IPV4`` and ``ENABLE_IPV6``.
   HTCondor now ignores addresses that are likely to be useless
   (loopback or link-local) unless no address is likely to be useful
   (private or public). `(Ticket
   #6348). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6348>`__
-  Added support for Public Input Files in HTCondor jobs. This allows
   users to transfer input files over a publicly-available HTTP web
   service, which can benefit from caching proxies, load balancers, and
   other tools to improve file transfer performance. `(Ticket
   #6356). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6356>`__
-  Added **-grid:ec2** to *condor\_q* to avoid truncating AWS’ new,
   longer, instance IDs. Replaced useless (given the instance ID)
   instance host name with the CMD column, to help distinguish EC2 jobs
   from each other. `(Ticket
   #5478). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5478>`__
-  Added statistical output for job input files transferred from web
   servers using the curl\_plugin tool. Statistics are stored in ClassAd
   format, saved by default to a transfer\_history file in the local
   logs folder. `(Ticket
   #6229). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6229>`__

Bugs Fixed:

-  Fixed some small memory leaks in the HTCondor daemons. `(Ticket
   #6361). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6361>`__
-  Fixed a bug that would prevent dollar-dollar expansion from working
   correctly for parallel universe jobs running on partitionable slots.
   `(Ticket
   #6370). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6370>`__

Version 8.7.2
-------------

Release Notes:

-  HTCondor version 8.7.2 released on June 22, 2017.

Known Issues:

-  Our current implementation of late materialization is incompatible
   with *condor\_dagman* and will cause unexpected behavior, including
   failing without warning. This is a top-priority issue which aim to
   resolve in an upcoming release. `(Ticket
   #6292). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6292>`__

New Features:

-  Improved the performance of the *condor\_schedd* by setting the
   default for the knob ``SUBMIT_SKIP_FILECHECKS`` to true. This
   prevents the *condor\_schedd* from checking the readability of all
   input files, and skips the creation of the output files on the submit
   side at submit time. Output files are now created either at transfer
   time, when file transfer is on, or by the job itself, if a shared
   filesystem is used. As a result of this change, it is possible that a
   job will run to completion, and only then is put on hold because the
   output file on the submit machine cannot be written. `(Ticket
   #6220). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6220>`__
-  Changed *condor\_submit* to not create empty stdout and stderr files
   before submitting jobs by default. This caused confusion for users,
   and slowed down the submission process. The older behavior, where
   *condor\_submit* would fail if it could not create this files, is
   available when the parameter ``SUBMIT_SKIP_FILECHECKS`` is set to
   false. The default is now true. `(Ticket
   #6220). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6220>`__
-  *condor\_q* will now show expanded totals when querying a
   *condor\_schedd* that is version 8.7.1 or later. The totals for the
   current user and for all users are provided by the *condor\_schedd*.
   To get the old totals display set the configuration parameter
   ``CONDOR_Q_SHOW_OLD_SUMMARY`` to true. `(Ticket
   #6254). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6254>`__
-  The *condor\_annex* tool now logs to the user configuration
   directory. Added an audit log of *condor\_annex* commands and their
   results. `(Ticket
   #6267). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6267>`__
-  Changed *condor\_off* so that the ``-annex`` flag implies the
   ``-master`` flag, since this is more likely to be the right thing.
   `(Ticket
   #6266). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6266>`__
-  Added ``-status`` flag to *condor\_annex*, which reports on instances
   which are running but not in the pool. `(Ticket
   #6257). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6257>`__
-  If invoked with an annex name and duration (but not an instance or
   slot count), *condor\_annex* will now adjust the duration of the
   named annex. `(Ticket
   #6161). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6161>`__
-  Job input files which are downloaded from http:// web addresses now
   have mechanisms to recover from transfer failures. This should
   increase the reliability of using web-based input files, especially
   under slow and/or unstable network conditions. `(Ticket
   #5886). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5886>`__
-  Reduced load on the *condor\_collector* by optimizing queries
   performed when an HTCondor daemon needs to look up the address of
   another daemon. `(Ticket
   #6223). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6223>`__
-  Reduced load on the *condor\_collector* by optimizing queries
   performed when using condor\_q with several different command-line
   options such as **-submitter** and **-global**. `(Ticket
   #6222). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6222>`__
-  Added the *condor\_top* tool, an automated version of the now-defunct
   *condor\_top.pl* which uses the python bindings to monitor the status
   of daemons. `(Ticket
   #6205). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6205>`__
-  Added a new option **-cron** to *condor\_gpu\_discovery* that allows
   it to be used directly as an executable of a *condor\_startd* cron
   job. `(Ticket
   #6012). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6012>`__
-  The configuration variable ``MAX_RUNNING_SCHEDULER_JOBS_PER_OWNER``
   was set to default to 100. It formerly had no default value. `(Ticket
   #6260). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6260>`__
-  Added a parameter ``DEDICATED_SCHEDULER_USE_SERIAL_CLAIMS`` which
   defaults to false. When true, allows the dedicated schedule to use
   claimed/idle slots that the serial scheduler has claimed. `(Ticket
   #6276). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6276>`__
-  The *condor\_advertise* tool now assumes an update command if one is
   not specified on the command-line and attempts to determine exact
   command by inspecting the first ad to be advertised. `(Ticket
   #6296). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6296>`__
-  Improved support for running several *condor\_negotiator*\ s in a
   single pool. ``NEGOTIATOR_NAME`` now works like ``MASTER_NAME``.
   *condor\_userprio* has a -name option to select a specific
   *condor\_negotiator*. Accounting ads from multiple
   *condor\_negotiator*\ s can co-exist in the *condor\_collector*.
   `(Ticket
   #5717). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5717>`__
-  Package EC2 Annex components in the condor-annex-ec2 sub RPM.
   `(Ticket
   #6202). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6202>`__
-  Added configuration parameter ``ALTERNATE_JOB_SPOOL``, an expression
   evaluated against the job ad, which specifies an alternate spool
   directory to use for files related to that job. `(Ticket
   #6221). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6221>`__

Bugs Fixed:

-  With an empty configuration file, HTCondor would behave as if
   ``ALLOW_ADMINISTRATOR`` were ``*``. Changed the default to
   ``$(CONDOR_HOST)``, which is much less insecure. `(Ticket
   #6230). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6230>`__
-  Fixed a bug in the *condor\_schedd* where it did not account for the
   initial state of late materialize jobs when calculating the running
   totals of jobs by state. This bug resulted in *condor\_q* displaying
   incorrect totals when ``CONDOR_Q_SHOW_OLD_SUMMARY`` was set to false.
   `(Ticket
   #6272). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6272>`__
-  Fixed a bug where the *condor\_schedd* would incorrectly try to check
   the validity of output files and directories for late materialize
   jobs. The *condor\_schedd* will now always skip file checks for late
   materialize jobs. `(Ticket
   #6246). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6246>`__
-  Changed the output of the *condor\_status* command so that the Load
   Average field now displays the load average of just the condor job
   running in that slot. Previously, load associated from outside of
   condor was proportionately distributed into the condor slots,
   resulting in much confusion. `(Ticket
   #6225). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6225>`__
-  Illegal chars (’+’, ’.’) are now prohibited in DAGMan node names.
   `(Ticket
   #5966). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5966>`__
-  Improve audit log messages by including the connection ID and
   properly filtering out shadow and gridmanager modifications to the
   job queue log. `(Ticket
   #6289). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6289>`__
-  *condor\_root\_switchboard* has been removed from the release, since
   PrivSep is no longer supported. `(Ticket
   #6259). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6259>`__

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
   introduced; see section\ |˜r |\ efparam:CollectorQueryWorkersPending.
   `(Ticket
   #6192). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6192>`__
-  Default value for ``COLLECTOR_QUERY_WORKERS`` changed from 2 to 4.
   `(Ticket
   #6192). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6192>`__
-  Introduced configuration macro
   ``COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO`` so that the
   collector prioritizes queries that are important for the operation of
   the pool (such as queries from the negotiator) ahead of servicing
   user invocations of *condor\_status*. `(Ticket
   #6192). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6192>`__
-  Introduced configuration macro ``COLLECTOR_QUERY_MAX_WORKTIME`` to
   define the maximum amount of time the collector may service a query
   from a client like condor\_status. See section\ |˜r
   |\ efparam:CollectorQueryMaxWorktime. `(Ticket
   #6192). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6192>`__
-  Added several new statistics on collector query performance into the
   Collector ClassAd, including ``ActiveQueryWorkers``,
   ``ActiveQueryWorkersPeak``, ``PendingQueries``,
   ``PendingQueriesPeak``, ``DroppedQueries``, and
   ``RecentDroppedQueries``. See section\ |˜r
   |\ efsec:Collector-ClassAd-Attributes. `(Ticket
   #6192). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6192>`__
-  Further refinement and initial documentation of the HTCondor Annex.
   `(Ticket
   #6147). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6147>`__
   `(Ticket
   #6149). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6149>`__
   `(Ticket
   #6150). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6150>`__
   `(Ticket
   #6155). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6155>`__
   `(Ticket
   #6157). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6157>`__
   `(Ticket
   #6184). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6184>`__
   `(Ticket
   #6196). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6196>`__
   `(Ticket
   #6216). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6216>`__
   `(Ticket
   #6218). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6218>`__
-  Docker universe jobs can now use condor\_chirp command (if it is in
   the image). `(Ticket
   #6162). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6162>`__
-  In the Job Router, when a candidate job matches multiple routes, the
   first route is now always selected. The old behavior of spreading
   jobs across all matching routes round-robin style can be enabled by
   setting the new configuration parameter
   ``JOB_ROUTER_ROUND_ROBIN_SELECTION`` to ``True``. `(Ticket
   #6190). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6190>`__
-  The *condor\_schedd* now keeps a count of jobs by state for each
   owner and submitter and will report them to *condor\_q*. Condorq will
   display these totals unless the new configuration parameter
   ``CONDOR_Q_SHOW_OLD_SUMMARY`` is set to true. In 8.7.1 this parameter
   defaults to true. `(Ticket
   #6160). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6160>`__
-  Milestone 1 for late materialization in the *condor\_schedd* was
   completed. This milestone adds the undocumented option **-factory**
   to *condor\_q* that can be used to submit a late materializing job
   cluster to the *condor\_schedd*. The *condor\_schedd* will refuse the
   submission unless the configuration parameter
   ``SCHEDD_ALLOW_LATE_MATERIALIZATION`` is set to true. `(Ticket
   #6212). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6212>`__
-  Increased the default value for configuration parameter
   ``NEGOTIATOR_SOCKET_CACHE_SIZE`` to 500. `(Ticket
   #6165). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6165>`__
-  Added new DaemonCore statistics UdpQueueDepth to measure the number
   of bytes in the UDP receive queue for daemons with a UDP command
   port. `(Ticket
   #6183). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6183>`__
-  Improved speed of handling queries to the collector by caching the
   the configuration knob SHARED\_PORT\_ADDRESS\_REWRITING. `(Ticket
   #6187). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6187>`__
-  The *condor\_collector* on Linux now handles some queries in process
   and some by forking a child process. This allows it to avoid the
   overhead of forking to handle queries that will take little time. The
   policy for deciding which queries to handle in process is controlled
   by a new configuration parameter ``HANDLE_QUERY_IN_PROC_POLICY``.
   `(Ticket
   #6191). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6191>`__
-  Added **-limit** option to *condor\_status* and changed the
   *condor\_collector* to honor it. `(Ticket
   #6198). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6198>`__
-  *condor\_submit* was changed to use the same utility library that the
   submit python bindings use. This should help insure that submit via
   python bindings will give the same results as using *condor\_submit*.
   `(Ticket
   #6181). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6181>`__.

Bugs Fixed:

-  None.

Version 8.7.0
-------------

Release Notes:

-  HTCondor version 8.7.0 released on March 2, 2017.

New Features:

-  Optimized the code that reads reads ClassAds off the wire making the
   maximum possible update rate for the Collector about 1.7 times higher
   than it was before. `(Ticket
   #6105). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6105>`__
   `(Ticket
   #6130). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6130>`__
-  New statistics have been added to the Collector ad to show time spent
   handling queries. `(Ticket
   #6123). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6123>`__
-  Changed the formatting of the printing of ClassAd expressions with
   parentheses. Now there is no space character after every open
   parenthesis, or before every close parenthesis This looks more
   natural, is somewhat faster for the condor to parse, and saves space.
   That is, an expression that used to print like

   ::

       ( ( ( foo ) ) )

   now will print like this

   ::

       (((foo)))

   `(Ticket
   #6082). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6082>`__

-  Technology preview of the HTCondor Annex. The HTCondor Annex allows
   one to extend their HTCondor pool into the cloud.
   `https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUseCondorAnnexWithOnDemandInstances <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToUseCondorAnnexWithOnDemandInstances>`__
   `(Ticket
   #6121). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6121>`__
-  Added **-annex** option to *condor\_status* and *condor\_off*.
   Requires an argument; the request is constrained to match machines
   whose ``AnnexName`` ClassAd attribute matches the argument. `(Ticket
   #6116). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6116>`__
   `(Ticket
   #6117). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6117>`__
-  A refreshed X.509 proxy is now forwarded to the remote cluster in
   Bosco. `(Ticket
   #5841). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=5841>`__
-  Added several new statistics to the Negotiator ad, mainly detailing
   how time is spent in the negotiation cycle. `(Ticket
   #6060). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6060>`__

Bugs Fixed:

-  Removed redundant updates to the job queue by the Job Router.
   `(Ticket
   #6102). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6102>`__

      

.. |˜r | image:: ref6x.png
.. |˜r | image:: ref7x.png
.. |˜r | image:: ref8x.png
