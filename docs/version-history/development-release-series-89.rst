Development Release Series 8.9
==============================

This is the development release series of HTCondor. The details of each
version are described below.

Version 8.9.3
-------------

Release Notes:

-  HTCondor version 8.9.3 not yet released.

.. HTCondor version 8.9.3 released on Month Date, 2019.

New Features:

-  None.

Bugs Fixed:

-  None.

Version 8.9.2
-------------

Release Notes:

-  HTCondor version 8.9.2 released on June 4, 2019.

-  The default setting for ``CREDD_OAUTH_MODE`` is now ``true``.  This only
   affects people who were using the *condor_credd* to manage Kerberos credentials
   in the ``SEC_CREDENTIAL_DIRECTORY``.
   :ticket:`7046`

New Features:

-  The default file transfer plugin for HTTP/HTTPS will timeout transfers
   that make no progress as opposed to waiting indefinitely.  :ticket:`6971`

-  Added a new multifile transfer plugin for downloading files from Box.com user accounts. This
   supports URLs like "box://path/to/file" and using the plugin requires the admin configure the
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
   variables that control this; ``EVENT_LOG_FORMAT_OPTIONS`` controls the format of the global event log
   and ``DEFAULT_USERLOG_FORMAT_OPTIONS`` controls formatting of user log and DAGMan nodes logs.  These
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
   ``SSL_SKIP_HOST_CHECK`` to ``true``. :ticket:`7030`

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
   client saves the token to the file specified in ``SCITOKENS_FILE``, that token
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

-  Added a new config knob, ``DOCKER_EXTRA_ARGUMENTS``, to allow admins
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
   argv[0] to the string *condor_exe.exe*. This breaks jobs that look
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

   ::

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
   parameter ``SEC_USE_FAMILY_SESSION`` to ``False``. :ticket:`6788`

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
   ``_enter__`` and ``_exit__``). The ``JobEvent`` class now
   implements ``_str__`` and ``_repr__``. :ticket:`6814`

-  the *condor_hdfs* daemon which allowed the hdfs daemons to run under
   the *condor_master* has been removed from the contributed source.
   :ticket:`6809`

Bugs Fixed:

-  Fixed potential authentication failures between the *condor_schedd*
   and *condor_startd* when multiple *condor_startd* s are using the
   same shared port server. :ticket:`5604`


