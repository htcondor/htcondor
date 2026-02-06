Global Configuration Options
============================

:index:`General Options<single: Configuration; General Options>`

.. _general_config_options:

General Configuration File Entries
----------------------------------

This section describes settings which affect all parts of the HTCondor system.
Other system-wide settings can be found in `networking_config_options`_
and `shared_fs_config_options`_.

:macro-def:`SUBSYSTEM`
    Various configuration macros described below may include :macro:`<SUBSYS>` in the macro name.
    This allows for one general macro name to apply to specific subsystems via a common
    pattern. Just replace the :macro:`<SUBSYS>` part of the given macro with a valid HTCondor
    subsystem name to apply that macro. Note that some configuration macros with :macro:`<SUBSYS>`
    only work for select subsystems. List of HTCondor Subsystems:

    .. include:: ../subsystems.rst

:macro-def:`CONDOR_HOST`
    This macro is used to define the ``$(COLLECTOR_HOST)`` macro.
    Normally the *condor_collector* and *condor_negotiator* would run
    on the same machine. If for some reason they were not run on the
    same machine, ``$(CONDOR_HOST)`` would not be needed. Some of the
    host-based security macros use ``$(CONDOR_HOST)`` by default. See the
    :ref:`admin-manual/security:host-based security in htcondor` section on
    Setting up IP/host-based security in HTCondor for details.

:macro-def:`COLLECTOR_HOST`
    The host name of the machine where the *condor_collector* is
    running for your pool. Normally, it is defined relative to the
    ``$(CONDOR_HOST)`` macro. There is no default value for this macro;
    :macro:`COLLECTOR_HOST` must be defined for the pool to work properly.

    In addition to defining the host name, this setting can optionally
    be used to specify the network port of the *condor_collector*. The
    port is separated from the host name by a colon (':'). For example,

    .. code-block:: condor-config

        COLLECTOR_HOST = $(CONDOR_HOST):1234


    If no port is specified, the default port of 9618 is used. Using the
    default port is recommended for most sites. It is only changed if
    there is a conflict with another service listening on the same
    network port. For more information about specifying a non-standard
    port for the *condor_collector* daemon, see
    :ref:`admin-manual/networking:port usage in htcondor`.

    Multiple *condor_collector* daemons may be running simultaneously,
    if :macro:`COLLECTOR_HOST` is defined with a comma separated list of
    hosts. Multiple *condor_collector* daemons may run for the
    implementation of high availability; see :ref:`admin-manual/cm-configuration:High Availability of the Central Manager`
    for details. With more than one running, updates are sent to all.
    With more than one running, queries are sent to one of the
    *condor_collector* daemons, chosen at random.

:macro-def:`COLLECTOR_PORT`
    The default port used when contacting the *condor_collector* and
    the default port the *condor_collector* listens on if no port is
    specified. This variable is referenced if no port is given and there
    is no other means to find the *condor_collector* port. The default
    value is 9618.

:macro-def:`NEGOTIATOR_HOST`
    This configuration variable is no longer used. It previously defined
    the host name of the machine where the *condor_negotiator* is
    running. At present, the port where the *condor_negotiator* is
    listening is dynamically allocated.

:macro-def:`CONDOR_VIEW_HOST`
    A list of HTCondorView servers, separated by commas and/or spaces.
    Each HTCondorView server is denoted by the host name of the machine
    it is running on, optionally appended by a colon and the port
    number. This service is optional, and requires additional
    configuration to enable it. There is no default value for
    :macro:`CONDOR_VIEW_HOST`. If :macro:`CONDOR_VIEW_HOST` is not defined, no
    HTCondorView server is used. See
    :ref:`admin-manual/cm-configuration:configuring the
    htcondorview server` for more details.

:macro-def:`SCHEDD_HOST`
    The host name of the machine where the *condor_schedd* is running
    for your pool. This is the host that queues submitted jobs. If the
    host specifies :macro:`SCHEDD_NAME` or :macro:`MASTER_NAME`, that
    name must be included in the form name@hostname. In most condor
    installations, there is a *condor_schedd* running on each host from
    which jobs are submitted. The default value of :macro:`SCHEDD_HOST`
    is the current host with the optional name included. For most pools,
    this macro is not defined, nor does it need to be defined.

:macro-def:`RELEASE_DIR`
    The full path to the HTCondor release directory, which holds the
    ``bin``, ``etc``, ``lib``, and ``sbin`` directories. Other macros
    are defined relative to this one. There is no default value for
    :macro:`RELEASE_DIR`.

:macro-def:`BIN`
    This directory points to the HTCondor directory where user-level
    programs are installed. The default value is ``$(RELEASE_DIR)``/bin.

:macro-def:`LIB`
    This directory points to the HTCondor directory containing its
    libraries.  On Windows, libraries are located in :macro:`BIN`.

:macro-def:`LIBEXEC`
    This directory points to the HTCondor directory where support
    commands that HTCondor needs will be placed. Do not add this
    directory to a user or system-wide path.

:macro-def:`INCLUDE`
    This directory points to the HTCondor directory where header files
    reside. The default value is ``$(RELEASE_DIR)``/include. It can make
    inclusion of necessary header files for compilation of programs
    (such as those programs that use ``libcondorapi.a``) easier through
    the use of :tool:`condor_config_val`.

:macro-def:`SBIN`
    This directory points to the HTCondor directory where HTCondor's
    system binaries (such as the binaries for the HTCondor daemons) and
    administrative tools are installed. Whatever directory ``$(SBIN)``
    points to ought to be in the ``PATH`` of users acting as HTCondor
    administrators. The default value is ``$(BIN)`` in Windows and
    ``$(RELEASE_DIR)``/sbin on all other platforms.

:macro-def:`LOCAL_DIR`
    The location of the local HTCondor directory on each machine in your
    pool. The default value is ``$(RELEASE_DIR)`` on Windows and
    ``$(RELEASE_DIR)``/hosts/``$(HOSTNAME)`` on all other platforms.

    Another possibility is to use the condor user's home directory,
    which may be specified with ``$(TILDE)``. For example:

    .. code-block:: condor-config

        LOCAL_DIR = $(tilde)

:macro-def:`LOG`
    Used to specify the directory where each HTCondor daemon writes its
    log files. The names of the log files themselves are defined with
    other macros, which use the ``$(LOG)`` macro by default. The log
    directory also acts as the current working directory of the HTCondor
    daemons as the run, so if one of them should produce a core file for
    any reason, it would be placed in the directory defined by this
    macro. The default value is ``$(LOCAL_DIR)``/log.

    Do not stage other files in this directory; any files not created by
    HTCondor in this directory are subject to removal.

:macro-def:`RUN`
    A path and directory name to be used by the HTCondor init script to
    specify the directory where the :tool:`condor_master` should write its
    process ID (PID) file. The default if not defined is ``$(LOG)``.

:macro-def:`SPOOL`
    The spool directory is where certain files used by the
    *condor_schedd* are stored, such as the job queue file.  The
    spool also stores all input and output files for
    remotely-submitted jobs and all intermediate or checkpoint
    files.  Therefore,
    you will want to ensure that the spool directory is located on a
    partition with enough disk space. If a given machine is only set up
    to execute HTCondor jobs and not submit them, it would not need a
    spool directory (or this macro defined). The default value is
    ``$(LOCAL_DIR)``/spool. The *condor_schedd* will not function if
    :macro:`SPOOL` is not defined.

    Do not stage other files in this directory; any files not created by
    HTCondor in this directory are subject to removal.

:macro-def:`EXECUTE`
    This directory acts as a place to create the scratch directory of
    any HTCondor job that is executing on the local machine. The scratch
    directory is the destination of any input files that were specified
    for transfer. It also serves as the job's working directory if the
    job is using file transfer mode and no other working directory was
    specified. If a given machine is set up to only submit jobs and not
    execute them, it would not need an execute directory, and this macro
    need not be defined. The default value is ``$(LOCAL_DIR)``/execute.
    The *condor_startd* will not function if :macro:`EXECUTE` is undefined.
    To customize the execute directory independently for each batch
    slot, use :macro:`SLOT<N>_EXECUTE`.

    Do not stage other files in this directory; any files not created by
    HTCondor in this directory are subject to removal.

    Ideally, this directory should not be placed under /tmp or /var/tmp, if
    it is, HTCondor loses the ability to make private instances of /tmp and /var/tmp
    for jobs.

:macro-def:`ETC`
    This directory contains configuration and credential files used by
    the HTCondor daemons.
    The default value is ``$(LOCAL_DIR)``.
    For Linux package installations, the value ``/etc/condor`` is used.

:macro-def:`TMP_DIR`
    A directory path to a directory where temporary files are placed by
    various portions of the HTCondor system. The daemons and tools that
    use this directory are the *condor_gridmanager*,
    :tool:`condor_config_val` when using the **-rset** option, systems that
    use lock files when configuration variable
    :macro:`CREATE_LOCKS_ON_LOCAL_DISK` is ``True``, the Web
    Service API, and the *condor_credd* daemon. There is no default
    value.

    If both :macro:`TMP_DIR` and :macro:`TEMP_DIR` are defined, the value set for
    :macro:`TMP_DIR` is used and :macro:`TEMP_DIR` is ignored.

:macro-def:`TEMP_DIR`
    A directory path to a directory where temporary files are placed by
    various portions of the HTCondor system. The daemons and tools that
    use this directory are the *condor_gridmanager*,
    :tool:`condor_config_val` when using the **-rset** option, systems that
    use lock files when configuration variable
    :macro:`CREATE_LOCKS_ON_LOCAL_DISK` is ``True``, the Web
    Service API, and the *condor_credd* daemon. There is no default
    value.

    If both :macro:`TMP_DIR` and :macro:`TEMP_DIR` are defined, the value set for
    :macro:`TMP_DIR` is used and :macro:`TEMP_DIR` is ignored.

:macro-def:`SLOT<N>_EXECUTE`
    Specifies an execute directory for use by a specific batch slot.
    ``<N>`` represents the number of the batch slot, such as 1, 2, 3,
    etc. This execute directory serves the same purpose as
    :macro:`EXECUTE`, but it allows the configuration of the
    directory independently for each batch slot. Having slots each using
    a different partition would be useful, for example, in preventing
    one job from filling up the same disk that other jobs are trying to
    write to. If this parameter is undefined for a given batch slot, it
    will use :macro:`EXECUTE` as the default. Note that each slot will
    advertise :ad-attr:`TotalDisk` and :ad-attr:`Disk` for the partition containing
    its execute directory.

:macro-def:`LOCAL_CONFIG_FILE`
    Identifies the location of the local, machine-specific configuration
    file for each machine in the pool. The two most common choices would
    be putting this file in the ``$(LOCAL_DIR)``, or putting all local
    configuration files for the pool in a shared directory, each one
    named by host name. For example,

    .. code-block:: condor-config

        LOCAL_CONFIG_FILE = $(LOCAL_DIR)/condor_config.local


    or,

    .. code-block:: condor-config

        LOCAL_CONFIG_FILE = $(release_dir)/etc/$(hostname).local


    or, not using the release directory

    .. code-block:: condor-config

        LOCAL_CONFIG_FILE = /full/path/to/configs/$(hostname).local


    The value of :macro:`LOCAL_CONFIG_FILE` is treated as a list of files,
    not a single file. The items in the list are delimited by either
    commas or space characters. This allows the specification of
    multiple files as the local configuration file, each one processed
    in the order given (with parameters set in later files overriding
    values from previous files). This allows the use of one global
    configuration file for multiple platforms in the pool, defines a
    platform-specific configuration file for each platform, and uses a
    local configuration file for each machine. If the list of files is
    changed in one of the later read files, the new list replaces the
    old list, but any files that have already been processed remain
    processed, and are removed from the new list if they are present to
    prevent cycles. See
    :ref:`admin-manual/introduction-to-configuration:executing a program to produce configuration macros`
    for directions on using a program to generate the configuration
    macros that would otherwise reside in one or more files as described
    here. If :macro:`LOCAL_CONFIG_FILE` is not defined, no local
    configuration files are processed. For more information on this, see
    :ref:`admin-manual/introduction-to-configuration:configuring htcondor for multiple platforms`.

    If all files in a directory are local configuration files to be
    processed, then consider using :macro:`LOCAL_CONFIG_DIR`.

:macro-def:`REQUIRE_LOCAL_CONFIG_FILE`
    A boolean value that defaults to ``True``. When ``True``, HTCondor
    exits with an error, if any file listed in :macro:`LOCAL_CONFIG_FILE`
    cannot be read. A value of ``False`` allows local configuration
    files to be missing. This is most useful for sites that have both
    large numbers of machines in the pool and a local configuration file
    that uses the ``$(HOSTNAME)`` macro in its definition. Instead of
    having an empty file for every host in the pool, files can simply be
    omitted.

:macro-def:`LOCAL_CONFIG_DIR`
    A directory may be used as a container for local configuration
    files. The files found in the directory are sorted into
    lexicographical order by file name, and then each file is treated as
    though it was listed in :macro:`LOCAL_CONFIG_FILE`. :macro:`LOCAL_CONFIG_DIR`
    is processed before any files listed in :macro:`LOCAL_CONFIG_FILE`, and
    is checked again after processing the :macro:`LOCAL_CONFIG_FILE` list. It
    is a list of directories, and each directory is processed in the
    order it appears in the list. The process is not recursive, so any
    directories found inside the directory being processed are ignored.
    See also :macro:`LOCAL_CONFIG_DIR_EXCLUDE_REGEXP`.

:macro-def:`USER_CONFIG_FILE`
    The file name of a configuration file to be parsed after other local
    configuration files and before environment variables set
    configuration. Relevant only if HTCondor daemons are not run as root
    on Unix platforms or Local System on Windows platforms. The default
    is ``$(HOME)/.condor/user_config`` on Unix platforms. The default is
    %USERPROFILE%\\.condor\\user_config on Windows platforms. If a fully
    qualified path is given, that is used. If a fully qualified path is
    not given, then the Unix path ``$(HOME)/.condor/`` prefixes the file
    name given on Unix platforms, or the Windows path
    %USERPROFILE%\\.condor\\ prefixes the file name given on Windows
    platforms.

    The ability of a user to use this user-specified configuration file
    can be disabled by setting this variable to the empty string:

    .. code-block:: condor-config

        USER_CONFIG_FILE =

:macro-def:`LOCAL_CONFIG_DIR_EXCLUDE_REGEXP`
    A regular expression that specifies file names to be ignored when
    looking for configuration files within the directories specified via
    :macro:`LOCAL_CONFIG_DIR`. The default expression ignores files with
    names beginning with a '.' or a '#', as well as files with names
    ending in '˜'. This avoids accidents that can be caused by treating
    temporary files created by text editors as configuration files.

:macro-def:`CONDOR_IDS`
    The User ID (UID) and Group ID (GID) pair that the HTCondor daemons
    should run as, if the daemons are spawned as root.
    :index:`CONDOR_IDS environment variable`\ :index:`CONDOR_IDS<single: CONDOR_IDS; environment variables>`
    This value can also be specified in the :macro:`CONDOR_IDS` environment
    variable. If the HTCondor daemons are not started as root, then
    neither this :macro:`CONDOR_IDS` configuration macro nor the
    :macro:`CONDOR_IDS` environment variable are used. The value is given by
    two integers, separated by a period. For example,
    CONDOR_IDS = 1234.1234. If this pair is not specified in either the
    configuration file or in the environment, and the HTCondor daemons
    are spawned as root, then HTCondor will search for a condor user on
    the system, and run as that user's UID and GID. See
    :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    on UIDs in HTCondor for more details.

:macro-def:`CONDOR_ADMIN`
    The email address that HTCondor will send mail to if something goes
    wrong in the pool. For example, if a daemon crashes, the
    :tool:`condor_master` can send an obituary to this address with the last
    few lines of that daemon's log file and a brief message that
    describes what signal or exit status that daemon exited with. The
    default value is root@\ ``$(FULL_HOSTNAME)``.

:macro-def:`<SUBSYS>_ADMIN_EMAIL`
    The email address that HTCondor
    will send mail to if something goes wrong with the named
    :macro:`<SUBSYS>`. Identical to :macro:`CONDOR_ADMIN`, but done on a per
    subsystem basis. There is no default value.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`CONDOR_SUPPORT_EMAIL`
    The email address to be included at the bottom of all email HTCondor
    sends out under the label "Email address of the local HTCondor
    administrator:". This is the address where HTCondor users at your
    site should send their questions about HTCondor and get technical
    support. If this setting is not defined, HTCondor will use the
    address specified in :macro:`CONDOR_ADMIN` (described above).

:macro-def:`EMAIL_SIGNATURE`
    Every e-mail sent by HTCondor includes a short signature line
    appended to the body. By default, this signature includes the URL to
    the global HTCondor project website. When set, this variable defines
    an alternative signature line to be used instead of the default.
    Note that the value can only be one line in length. This variable
    could be used to direct users to look at local web site with
    information specific to the installation of HTCondor.

:macro-def:`MAIL`
    The full path to a mail sending program that uses **-s** to specify
    a subject for the message. On all platforms, the default shipped
    with HTCondor should work. Only if you installed things in a
    non-standard location on your system would you need to change this
    setting. The default value is ``$(BIN)``/condor_mail.exe on Windows
    and ``/usr/bin/mail`` on all other platforms. The *condor_schedd*
    will not function unless :macro:`MAIL` is defined. For security reasons,
    non-Windows platforms should not use this setting and should use
    :macro:`SENDMAIL` instead.

:macro-def:`SENDMAIL`
    The full path to the *sendmail* executable. If defined, which it is
    by default on non-Windows platforms, *sendmail* is used instead of
    the mail program defined by :macro:`MAIL`.

:macro-def:`MAIL_FROM`
    The e-mail address that notification e-mails appear to come from.
    Contents is that of the ``From`` header. There is no default value;
    if undefined, the ``From`` header may be nonsensical.

:macro-def:`SMTP_SERVER`
    For Windows platforms only, the host name of the server through
    which to route notification e-mail. There is no default value; if
    undefined and the debug level is at ``FULLDEBUG``, an error message
    will be generated.

:macro-def:`RESERVED_SWAP`
    The amount of swap space in MiB to reserve for this machine.
    HTCondor will not start up more *condor_shadow* processes if the
    amount of free swap space on this machine falls below this level.
    The default value is 0, which disables this check. It is anticipated
    that this configuration variable will no longer be used in the near
    future. If :macro:`RESERVED_SWAP` is not set to 0, the value of
    :macro:`SHADOW_SIZE_ESTIMATE` is used.

:macro-def:`DISK`
    Tells HTCondor how much disk space (in kB) to advertise as being available
    for use by jobs. If :macro:`DISK` is not specified, HTCondor will advertise the
    amount of free space on your execute partition, minus :macro:`RESERVED_DISK`.

:macro-def:`RESERVED_DISK`
    Determines how much disk space (in MB) you want to reserve for your own
    machine. When HTCondor is reporting the amount of free disk space in
    a given partition on your machine, it will always subtract this
    amount. An example is the *condor_startd*, which advertises the
    amount of free space in the ``$(EXECUTE)`` directory. The default
    value of :macro:`RESERVED_DISK` is zero.

:macro-def:`LOCK`
    HTCondor needs to create lock files to synchronize access to various
    log files. Because of problems with network file systems and file
    locking over the years, we highly recommend that you put these lock
    files on a local partition on each machine. If you do not have your
    ``$(LOCAL_DIR)`` on a local partition, be sure to change this entry.

    Whatever user or group HTCondor is running as needs to have write
    access to this directory. If you are not running as root, this is
    whatever user you started up the :tool:`condor_master` as. If you are
    running as root, and there is a condor account, it is most likely
    condor.
    :index:`CONDOR_IDS environment variable`\ :index:`CONDOR_IDS<single: CONDOR_IDS; environment variables>`
    Otherwise, it is whatever you set in the :macro:`CONDOR_IDS` environment
    variable, or whatever you define in the :macro:`CONDOR_IDS` setting in
    the HTCondor config files. See
    :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    on UIDs in HTCondor for details.

    If no value for :macro:`LOCK` is provided, the value of :macro:`LOG` is used.

:macro-def:`HISTORY`
    Defines the location of the HTCondor history file, which stores
    information about all HTCondor jobs that have completed on a given
    machine. This macro is used by both the *condor_schedd* which
    appends the information and :tool:`condor_history`, the user-level
    program used to view the history file. This configuration macro is
    given the default value of ``$(SPOOL)/history`` in the default
    configuration. If not defined, no history file is kept.

:macro-def:`ENABLE_HISTORY_ROTATION`
    If this is defined to be true, then the history file will be
    rotated. If it is false, then it will not be rotated, and it will
    grow indefinitely, to the limits allowed by the operating system. If
    this is not defined, it is assumed to be true. The rotated files
    will be stored in the same directory as the history file.

:macro-def:`MAX_HISTORY_LOG`
    Defines the maximum size for the history file, in bytes. It defaults
    to 20MB. This parameter is only used if history file rotation is
    enabled.

:macro-def:`MAX_HISTORY_ROTATIONS`
    When history file rotation is turned on, this controls how many
    backup files there are. It default to 2, which means that there may
    be up to three history files (two backups, plus the history file
    that is being currently written to). When the history file is
    rotated, and this rotation would cause the number of backups to be
    too large, the oldest file is removed.

:macro-def:`HISTORY_CONTAINS_JOB_ENVIRONMENT`
    This parameter defaults to true.  When set to false, the job's
    environment attribute (which can be very large) is not written to
    the history file.  This may allow many more jobs to be kept in the
    history before rotation.

:macro-def:`HISTORY_HELPER_MAX_CONCURRENCY`
    Specifies the maximum number of concurrent remote :tool:`condor_history`
    queries allowed at a time; defaults to 50. When this maximum is
    exceeded, further queries will be queued in a non-blocking manner.
    Setting this option to 0 disables remote history access. A remote
    history access is defined as an invocation of :tool:`condor_history` that
    specifies a **-name** option to query a *condor_schedd* running on
    a remote machine.

:macro-def:`HISTORY_HELPER_MAX_HISTORY`
    Specifies the maximum number of ClassAds to parse on behalf of
    remote history clients. The default is 2,000,000,000. This allows the
    system administrator to indirectly manage the maximum amount of CPU
    time spent on each client. Setting this option to 0 disables remote
    history access.

:macro-def:`<SUBSYS>_DAEMON_HISTORY`
    A path representing a file for the daemon specified by :macro:`SUBSYSTEM`
    to periodically write ClassAd records into.

    .. note::

        Only the Schedd Daemon is capable of writing into a Daemon
        history file currently.

:macro-def:`MAX_DAEMON_HISTORY_LOG`
    An integer representing the maximum size in bytes the Daemon history
    can grow before rotating. Defaults to ``20 MB``.

:macro-def:`MAX_DAEMON_HISTORY_ROTATIONS`
    An integer representing the maximum number of old rotated Daemon history
    files to keep around at one time. Default is ``1``.

:macro-def:`MAX_JOB_QUEUE_LOG_ROTATIONS`
    The *condor_schedd* daemon periodically rotates the job queue
    database file, in order to save disk space. This option controls how
    many rotated files are saved. It defaults to 1, which means there
    may be up to two history files (the previous one, which was rotated
    out of use, and the current one that is being written to). When the
    job queue file is rotated, and this rotation would cause the number
    of backups to be larger than the maximum specified, the oldest file
    is removed.

:macro-def:`CLASSAD_LOG_STRICT_PARSING`
    A boolean value that defaults to ``True``. When ``True``, ClassAd
    log files will be read using a strict syntax checking for ClassAd
    expressions. ClassAd log files include the job queue log and the
    accountant log. When ``False``, ClassAd log files are read without
    strict expression syntax checking, which allows some legacy ClassAd
    log data to be read in a backward compatible manner. This
    configuration variable may no longer be supported in future
    releases, eventually requiring all ClassAd log files to pass strict
    ClassAd syntax checking.

:macro-def:`DEFAULT_DOMAIN_NAME`
    The value to be appended to a machine's host name, representing a
    domain name, which HTCondor then uses to form a fully qualified host
    name. This is required if there is no fully qualified host name in
    file ``/etc/hosts`` or in NIS. Set the value in the global
    configuration file, as HTCondor may depend on knowing this value in
    order to locate the local configuration file(s). The default value
    as given in the sample configuration file of the HTCondor download
    is bogus, and must be changed. If this variable is removed from the
    global configuration file, or if the definition is empty, then
    HTCondor attempts to discover the value.

:macro-def:`NO_DNS`
    A boolean value that defaults to ``False``. When ``True``, HTCondor
    constructs host names using the host's IP address together with the
    value defined for :macro:`DEFAULT_DOMAIN_NAME`.

:macro-def:`CM_IP_ADDR`
    If neither :macro:`COLLECTOR_HOST` nor ``COLLECTOR_IP_ADDR`` macros are
    defined, then this macro will be used to determine the IP address of
    the central manager (collector daemon). This macro is defined by an
    IP address.

:macro-def:`EMAIL_DOMAIN`
    By default, if a user does not specify ``notify_user`` in the submit
    description file, any email HTCondor sends about that job will go to
    "username@UID_DOMAIN". If your machines all share a common UID
    domain (so that you would set :macro:`UID_DOMAIN` to be the same across
    all machines in your pool), but email to user@UID_DOMAIN is not the
    right place for HTCondor to send email for your site, you can define
    the default domain to use for email. A common example would be to
    set :macro:`EMAIL_DOMAIN` to the fully qualified host name of each
    machine in your pool, so users submitting jobs from a specific
    machine would get email sent to user@machine.your.domain, instead of
    user@your.domain. You would do this by setting :macro:`EMAIL_DOMAIN` to
    ``$(FULL_HOSTNAME)``. In general, you should leave this setting
    commented out unless two things are true: 1) :macro:`UID_DOMAIN` is set
    to your domain, not ``$(FULL_HOSTNAME)``, and 2) email to
    user@UID_DOMAIN will not work.

:macro-def:`CREATE_CORE_FILES`
    Defines whether or not HTCondor daemons are to create a core file in
    the :macro:`LOG` directory if something really bad happens.
    It is used to set the resource limit for the size of a core
    file. If not defined, it leaves in place whatever limit was in
    effect when the HTCondor daemons (normally the :tool:`condor_master`)
    were started. This allows HTCondor to inherit the default system
    core file generation behavior at start up. For Unix operating
    systems, this behavior can be inherited from the parent shell, or
    specified in a shell script that starts HTCondor. If this parameter
    is set and ``True``, the limit is increased to the maximum. If it is
    set to ``False``, the limit is set at 0 (which means that no core
    files are created). Core files greatly help the HTCondor developers
    debug any problems you might be having. By using the parameter, you
    do not have to worry about tracking down where in your boot scripts
    you need to set the core limit before starting HTCondor. You set the
    parameter to whatever behavior you want HTCondor to enforce. This
    parameter defaults to undefined to allow the initial operating
    system default value to take precedence, and is commented out in the
    default configuration file.

:macro-def:`ABORT_ON_EXCEPTION`
    When HTCondor programs detect a fatal internal exception, they
    normally log an error message and exit. If you have turned on
    :macro:`CREATE_CORE_FILES`, in some cases you may also want to
    turn on :macro:`ABORT_ON_EXCEPTION` so that core files are generated
    when an exception occurs. Set the following to True if that is what
    you want.

:macro-def:`Q_QUERY_TIMEOUT`
    Defines the timeout (in seconds) that :tool:`condor_q` uses when trying
    to connect to the *condor_schedd*. Defaults to 20 seconds.

:macro-def:`DEAD_COLLECTOR_MAX_AVOIDANCE_TIME`
    Defines the interval of time (in seconds) between checks for a
    failed primary *condor_collector* daemon. If connections to the
    dead primary *condor_collector* take very little time to fail, new
    attempts to query the primary *condor_collector* may be more
    frequent than the specified maximum avoidance time. The default
    value equals one hour. This variable has relevance to flocked jobs,
    as it defines the maximum time they may be reporting to the primary
    *condor_collector* without the *condor_negotiator* noticing.

:macro-def:`PASSWD_CACHE_REFRESH`
    HTCondor can cause NIS servers to become overwhelmed by queries for
    uid and group information in large pools. In order to avoid this
    problem, HTCondor caches UID and group information internally. This
    integer value allows pool administrators to specify (in seconds) how
    long HTCondor should wait until refreshes a cache entry. The default
    is set to 72000 seconds, or 20 hours, plus a random number of
    seconds between 0 and 60 to avoid having lots of processes
    refreshing at the same time. This means that if a pool administrator
    updates the user or group database (for example, ``/etc/passwd`` or
    ``/etc/group``), it can take up to 6 minutes before HTCondor will
    have the updated information. This caching feature can be disabled
    by setting the refresh interval to 0. In addition, the cache can
    also be flushed explicitly by running the command
    :tool:`condor_reconfig`. This configuration variable has no effect on
    Windows.

:macro-def:`SYSAPI_GET_LOADAVG`
    If set to False, then HTCondor will not attempt to compute the load
    average on the system, and instead will always report the system
    load average to be 0.0. Defaults to True.

:macro-def:`NETWORK_MAX_PENDING_CONNECTS`
    This specifies a limit to the maximum number of simultaneous network
    connection attempts. This is primarily relevant to *condor_schedd*,
    which may try to connect to large numbers of startds when claiming
    them. The negotiator may also connect to large numbers of startds
    when initiating security sessions used for sending MATCH messages.
    On Unix, the default for this parameter is eighty percent of the
    process file descriptor limit. On windows, the default is 1600.

:macro-def:`WANT_UDP_COMMAND_SOCKET`
    This setting, added in version 6.9.5, controls if HTCondor daemons
    should create a UDP command socket in addition to the TCP command
    socket (which is required). The default is ``True``, and modifying
    it requires restarting all HTCondor daemons, not just a
    :tool:`condor_reconfig` or SIGHUP.

    Normally, updates sent to the *condor_collector* use UDP, in
    addition to certain keep alive messages and other non-essential
    communication. However, in certain situations, it might be desirable
    to disable the UDP command port.

    Unfortunately, due to a limitation in how these command sockets are
    created, it is not possible to define this setting on a per-daemon
    basis, for example, by trying to set
    ``STARTD.WANT_UDP_COMMAND_SOCKET``. At least for now, this setting
    must be defined machine wide to function correctly.

    If this setting is set to true on a machine running a
    *condor_collector*, the pool should be configured to use TCP
    updates to that collector (see
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more information).

:macro-def:`ALLOW_SCRIPTS_TO_RUN_AS_EXECUTABLES`
    A boolean value that, when ``True``, permits scripts on Windows
    platforms to be used in place of the
    :subcom:`executable[and Windows scripts]` in a job
    submit description file, in place of a :tool:`condor_dagman` pre or post
    script, or in producing the configuration, for example. Allows a
    script to be used in any circumstance previously limited to a
    Windows executable or a batch file. The default value is ``True``.
    See :ref:`platform-specific/microsoft-windows:using windows scripts as job executables`
    for further description.

:macro-def:`OPEN_VERB_FOR_<EXT>_FILES`
    A string that defines a Windows verb for use in a root hive registry
    look up. <EXT> defines the file name extension, which represents a
    scripting language, also needed for the look up. See
    :ref:`platform-specific/microsoft-windows:using windows scripts as job executables`
    for a more complete description.

:macro-def:`ENABLE_CLASSAD_CACHING`
    A boolean value that controls the caching of ClassAds. Caching saves
    memory when an HTCondor process contains many ClassAds with the same
    expressions. The default value is ``True`` for all daemons other
    than the *condor_shadow*, *condor_starter*, and :tool:`condor_master`.
    A value of ``True`` enables caching.

:macro-def:`STRICT_CLASSAD_EVALUATION`
    A boolean value that controls how ClassAd expressions are evaluated.
    If set to ``True``, then New ClassAd evaluation semantics are used.
    This means that attribute references without a ``MY.`` or
    ``TARGET.`` prefix are only looked up in the local ClassAd. If set
    to the default value of ``False``, Old ClassAd evaluation semantics
    are used. See
    :ref:`classads/classad-mechanism:classads: old and new`
    for details.

:macro-def:`CLASSAD_USER_LIBS`
    A comma separated list of paths to shared libraries that contain
    additional ClassAd functions to be used during ClassAd evaluation.

:macro-def:`CLASSAD_USER_PYTHON_MODULES`
    A comma separated list of python modules to load, which are to be
    used during ClassAd evaluation. If module ``foo`` is in this list,
    then function ``bar`` can be invoked in ClassAds via the expression
    ``python_invoke("foo", "bar", ...)``. Any further arguments are
    converted from ClassAd expressions to python; the function return
    value is converted back to ClassAds. The python modules are loaded
    at configuration time, so any module-level statements are executed.
    Module writers can invoke ``classad.register`` at the module-level
    in order to use python functions directly.

    Functions executed by ClassAds should be non-blocking and have no
    side-effects; otherwise, unpredictable HTCondor behavior may occur.

:macro-def:`CLASSAD_USER_PYTHON_LIB`
    Specifies the path to the python libraries, which is needed when
    :macro:`CLASSAD_USER_PYTHON_MODULES` is set. Defaults to
    ``$(LIBEXEC)/libclassad_python_user.so``, and would rarely be
    changed from the default value.

:macro-def:`CONDOR_FSYNC`
    A boolean value that controls whether HTCondor calls fsync() when
    writing the user job and transaction logs. Setting this value to
    ``False`` will disable calls to fsync(), which can help performance
    for *condor_schedd* log writes at the cost of some durability of
    the log contents, should there be a power or hardware failure. The
    default value is ``True``.

:macro-def:`STATISTICS_TO_PUBLISH`
    A comma and/or space separated list that identifies which statistics
    collections are to place attributes in ClassAds. Additional
    information specifies a level of verbosity and other identification
    of which attributes to include and which to omit from ClassAds. The
    special value ``NONE`` disables all publishing, so no statistics
    will be published; no option is included. For other list items that
    define this variable, the syntax defines the two aspects by
    separating them with a colon. The first aspect defines a collection,
    which may specify which daemon is to publish the statistics, and the
    second aspect qualifies and refines the details of which attributes
    to publish for the collection, including a verbosity level. If the
    first aspect is ``ALL``, the option is applied to all collections.
    If the first aspect is ``DEFAULT``, the option is applied to all
    collections, with the intent that further list items will specify
    publishing that is to be different than the default. This first
    aspect may be ``SCHEDD`` or ``SCHEDULER`` to publish Statistics
    attributes in the ClassAd of the *condor_schedd*. It may be
    ``TRANSFER`` to publish file transfer statistics. It may be
    :macro:`STARTER` to publish Statistics attributes in the ClassAd of the
    *condor_starter*. Or, it may be ``DC`` or ``DAEMONCORE`` to publish
    DaemonCore statistics. One or more options are specified after the
    colon.

    +--------+---------------------------------------------------------+
    | Option | Description                                             |
    +========+=========================================================+
    | 0      | turns off the publishing of any statistics attributes   |
    +--------+---------------------------------------------------------+
    | 1      | the default level, where some statistics attributes are |
    |        | and others are omitted                                  |
    +--------+---------------------------------------------------------+
    | 2      | the verbose level, where all statistics attributes are  |
    |        | published                                               |
    +--------+---------------------------------------------------------+
    | 3      | the super verbose level, which is currently unused, but |
    |        | intended to be all statistics attributes published at   |
    |        | the verbose level plus extra information                |
    +--------+---------------------------------------------------------+
    | R      | include attributes from the most recent time interval;  |
    |        | the default                                             |
    +--------+---------------------------------------------------------+
    | !R     | omit attributes from the most recent time interval      |
    +--------+---------------------------------------------------------+
    | D      | include attributes for debugging                        |
    +--------+---------------------------------------------------------+
    | !D     | omit attributes for debugging; the default              |
    +--------+---------------------------------------------------------+
    | Z      | include attributes even if the attribute's value is 0   |
    +--------+---------------------------------------------------------+
    | !Z     | omit attributes when the attribute's value is 0         |
    +--------+---------------------------------------------------------+
    | L      | include attributes that represent the lifetime value;   |
    |        | the default                                             |
    +--------+---------------------------------------------------------+
    | !L     | omit attributes that represent the lifetime value       |
    +--------+---------------------------------------------------------+

    If this variable is not defined, then the default for each
    collection is used. If this variable is defined, and the definition
    does not specify each possible collection, then no statistics are
    published for those collections not defined. If an option specifies
    conflicting possibilities, such as ``R!R``, then the last one takes
    precedence and is applied.

    As an example, to cause a verbose setting of the publication of
    Statistics attributes only for the *condor_schedd*, and do not
    publish any other Statistics attributes:

    .. code-block:: condor-config

          STATISTICS_TO_PUBLISH = SCHEDD:2

    As a second example, to cause all collections other than those for
    ``DAEMONCORE`` to publish at a verbosity setting of ``1``, and omit
    lifetime values, where the ``DAEMONCORE`` includes all statistics at
    the verbose level:

    .. code-block:: condor-config

          STATISTICS_TO_PUBLISH = DEFAULT:1!L, DC:2RDZL

:macro-def:`STATISTICS_TO_PUBLISH_LIST`
    A comma and/or space separated list of statistics attribute names
    that should be published in updates to the *condor_collector*
    daemon, even though the verbosity specified in
    :macro:`STATISTICS_TO_PUBLISH` would not normally send them. This setting
    has the effect of redefining the verbosity level of the statistics
    attributes that it mentions, so that they will always match the
    current statistics publication level as specified in
    :macro:`STATISTICS_TO_PUBLISH`.

:macro-def:`STATISTICS_WINDOW_SECONDS`
    An integer value that controls the time window size, in seconds, for
    collecting windowed daemon statistics. These statistics are, by
    convention, those attributes with names that are of the form
    ``Recent<attrname>``. Any data contributing to a windowed statistic
    that is older than this number of seconds is dropped from the
    statistic. For example, if ``STATISTICS_WINDOW_SECONDS = 300``, then
    any jobs submitted more than 300 seconds ago are not counted in the
    windowed statistic :ad-attr:`RecentJobsSubmitted`. Defaults to 1200
    seconds, which is 20 minutes.

    The window is broken into smaller time pieces called quantum. The
    window advances one quantum at a time.

:macro-def:`STATISTICS_WINDOW_SECONDS_<collection>`
    The same as :macro:`STATISTICS_WINDOW_SECONDS`, but used to override the
    global setting for a particular statistic collection. Collection
    names currently implemented are ``DC`` or ``DAEMONCORE`` and
    ``SCHEDD`` or ``SCHEDULER``.

:macro-def:`STATISTICS_WINDOW_QUANTUM`
    For experts only, an integer value that controls the time
    quantization that form a time window, in seconds, for the data
    structures that maintain windowed statistics. Defaults to 240
    seconds, which is 6 minutes. This default is purposely set to be
    slightly smaller than the update rate to the *condor_collector*.
    Setting a smaller value than the default increases the memory
    requirement for the statistics. Graphing of statistics at the level
    of the quantum expects to see counts that appear like a saw tooth.

:macro-def:`STATISTICS_WINDOW_QUANTUM_<collection>`
    The same as :macro:`STATISTICS_WINDOW_QUANTUM`, but used to override the
    global setting for a particular statistic collection. Collection
    names currently implemented are ``DC`` or ``DAEMONCORE`` and
    ``SCHEDD`` or ``SCHEDULER``.

:macro-def:`TCP_KEEPALIVE_INTERVAL`
    The number of seconds specifying a keep alive interval to use for
    any HTCondor TCP connection. The default keep alive interval is 360
    (6 minutes); this value is chosen to minimize the likelihood that
    keep alive packets are sent, while still detecting dead TCP
    connections before job leases expire. A smaller value will consume
    more operating system and network resources, while a larger value
    may cause jobs to fail unnecessarily due to network disconnects.
    Most users will not need to tune this configuration variable. A
    value of 0 will use the operating system default, and a value of -1
    will disable HTCondor's use of a TCP keep alive.

:macro-def:`ENABLE_IPV4`
    A boolean with the additional special value of ``auto``. If true,
    HTCondor will use IPv4 if available, and fail otherwise. If false,
    HTCondor will not use IPv4. If ``auto``, which is the default,
    HTCondor will use IPv4 if it can find an interface with an IPv4
    address, and that address is (a) public or private, or (b) no
    interface's IPv6 address is public or private. If HTCondor finds
    more than one address of each protocol, only the most public address
    is considered for that protocol.

:macro-def:`ENABLE_IPV6`
    A boolean with the additional special value of ``auto``. If true,
    HTCondor will use IPv6 if available, and fail otherwise. If false,
    HTCondor will not use IPv6. If ``auto``, which is the default,
    HTCondor will use IPv6 if it can find an interface with an IPv6
    address, and that address is (a) public or private, or (b) no
    interface's IPv4 address is public or private. If HTCondor finds
    more than one address of each protocol, only the most public address
    is considered for that protocol.

:macro-def:`PREFER_IPV4`
    A boolean which will cause HTCondor to prefer IPv4 when it is able
    to choose. HTCondor will otherwise prefer IPv6. The default is
    ``True``.

:macro-def:`ADVERTISE_IPV4_FIRST`
    A string (treated as a boolean). If :macro:`ADVERTISE_IPV4_FIRST`
    evaluates to ``True``, HTCondor will advertise its IPv4 addresses
    before its IPv6 addresses; otherwise the IPv6 addresses will come
    first. Defaults to ``$(PREFER_IPV4)``.

:macro-def:`IGNORE_TARGET_PROTOCOL_PREFERENCE`
    A string (treated as a boolean). If
    :macro:`IGNORE_TARGET_PROTOCOL_PREFERENCE` evaluates to ``True``, the
    target's listed protocol preferences will be ignored; otherwise
    they will not. Defaults to ``$(PREFER_IPV4)``.

:macro-def:`IGNORE_DNS_PROTOCOL_PREFERENCE`
    A string (treated as a boolean). :macro:`IGNORE_DNS_PROTOCOL_PREFERENCE`
    evaluates to ``True``, the protocol order returned by the DNS will
    be ignored; otherwise it will not. Defaults to ``$(PREFER_IPV4)``.

:macro-def:`PREFER_OUTBOUND_IPV4`
    A string (treated as a boolean). :macro:`PREFER_OUTBOUND_IPV4` evaluates
    to ``True``, HTCondor will prefer IPv4; otherwise it will not.
    Defaults to ``$(PREFER_IPV4)``.

:macro-def:`<SUBSYS>_CLASSAD_USER_MAP_NAMES`
    A string defining a list of names for username-to-accounting group
    mappings for the specified daemon. Names must be separated by spaces
    or commas.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`CLASSAD_USER_MAPFILE_<name>`
    A string giving the name of a file to parse to initialize the map
    for the given username. Note that this macro is only used if
    :macro:`<SUBSYS>_CLASSAD_USER_MAP_NAMES` is defined for the relevant
    daemon.

    The format for the map file is the same as the format for
    :macro:`CLASSAD_USER_MAPDATA_<name>`, below.

:macro-def:`CLASSAD_USER_MAPDATA_<name>`
    A string containing data to be used to initialize the map for the
    given username. Note that this macro is only used if
    :macro:`<SUBSYS>_CLASSAD_USER_MAP_NAMES` is defined for the relevant
    daemon, and :macro:`CLASSAD_USER_MAPFILE_<name>` is not defined
    for the given name.

    The format for the map data is the same as the format
    for the security authentication map file (see
    :ref:`admin-manual/security:the authentication map file`
    for details).

    The first field must be \* (or a subset name - see below), the
    second field is a regex that we will match against the input, and
    the third field will be the output if the regex matches, the 3 and 4
    argument form of the ClassAd userMap() function (see
    :ref:`classads/classad-mechanism:ClassAd Syntax`) expect
    that the third field will be a comma separated list of values. For
    example:

    .. code-block:: text

        # file: groups.mapdata
        * John  chemistry,physics,glassblowing
        * Juan  physics,chemistry
        * Bob   security
        * Alice security,math

    Here is simple example showing how to configure :macro:`CLASSAD_USER_MAPDATA_<name>`
    for testing and experimentation.

    ::

        # configuration statements to create a simple userMap that
        # can be used by the Schedd as well as by tools like condor_q
        #
        SCHEDD_CLASSAD_USER_MAP_NAMES = Trust $(SCHEDD_CLASSAD_USER_MAP_NAMES)
        TOOL_CLASSAD_USER_MAP_NAMES = Trust $(TOOL_CLASSAD_USER_MAP_NAMES)
        CLASSAD_USER_MAPDATA_Trust @=end
          * Bob   User
          * Alice Admin
          * /.*/  Nobody
        @end
        #
        # test with
        #   condor_q -af:j 'Owner' 'userMap("Trust",Owner)'

    **Optional submaps:** If the first field of the mapfile contains
    something other than \*, then a submap is defined. To select a
    submap for lookup, the first argument for userMap() should be
    "mapname.submap". For example:

    .. code-block:: text

        # mapdata 'groups' with submaps
        *   Bob   security
        *   Alice security,math
        alt Alice math,hacking

:macro-def:`SIGN_S3_URLS`
    A boolean value that, when ``True``, tells HTCondor to convert ``s3://``
    URLs into pre-signed ``https://`` URLs.  This allows execute nodes to
    download from or upload to secure S3 buckets without access to the user's
    API tokens, which remain on the submit node at all times.  This value
    defaults to TRUE but can be disabled if the administrator has already
    provided an ``s3://`` plug-in.  This value must be set on both the submit
    node and on the execute node.

:index:`Daemon Logging Options<single: Configuration; Daemon Logging Options>`

.. _daemon_logging_config_options:

Daemon Logging Configuration File Entries
-----------------------------------------

These entries control how and where the HTCondor daemons write to log
files. Many of the entries in this section represents multiple macros.
There is one for each subsystem (listed in :macro:`SUBSYSTEM`).
The macro name for each substitutes :macro:`<SUBSYS>` with the name of the
subsystem corresponding to the daemon.

:macro-def:`<SUBSYS>_LOG`
    Defines the path and file name of the
    log file for a given subsystem. For example, ``$(STARTD_LOG)`` gives
    the location of the log file for the *condor_startd* daemon. The
    default value for most daemons is the daemon's name in camel case,
    concatenated with ``Log``. For example, the default log defined for
    the :tool:`condor_master` daemon is ``$(LOG)/MasterLog``. The default
    value for other subsystems is ``$(LOG)/<SUBSYS>LOG``. The special
    value ``SYSLOG`` causes the daemon to log via the syslog facility on
    Linux. If the log file cannot be written to, then the daemon will
    attempt to log this into a new file of the name
    ``$(LOG)/dprintf_failure.<SUBSYS>`` before the daemon exits.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`LOG_TO_SYSLOG`
    A boolean value that is ``False`` by default. When ``True``, all
    daemon logs are routed to the syslog facility on Linux.

:macro-def:`MAX_<SUBSYS>_LOG`
    Controls the maximum size in bytes or amount of time that a log will
    be allowed to grow. For any log not specified, the default is
    ``$(MAX_DEFAULT_LOG)``\ :index:`MAX_DEFAULT_LOG`, which
    currently defaults to 10 MiB in size. Values are specified with the
    same syntax as :macro:`MAX_DEFAULT_LOG`.

    Note that a log file for the :tool:`condor_procd` does not use this
    configuration variable definition. Its implementation is separate.
    See :macro:`MAX_PROCD_LOG`.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MAX_DEFAULT_LOG`
    Controls the maximum size in bytes or amount of time that any log
    not explicitly specified using :macro:`MAX_<SUBSYS>_LOG` will be
    allowed to grow. When it is time to rotate a log file, it will be
    saved to a file with an ISO timestamp suffix. The oldest rotated
    file receives the ending ``.old``. The ``.old`` files are overwritten
    each time the maximum number of rotated files (determined by the value of
    :macro:`MAX_NUM_<SUBSYS>_LOG`) is exceeded. The default value is 10 MiB
    in size. A value of 0 specifies that the file may grow without
    bounds. A single integer value is specified; without a suffix, it
    defaults to specifying a size in bytes. A suffix is case
    insensitive, except for ``Mb`` and ``Min``; these both start with
    the same letter, and the implementation attaches meaning to the
    letter case when only the first letter is present. Therefore, use
    the following suffixes to qualify the integer:
    ``Bytes`` for bytes
    ``Kb`` for KiB, 2\ :sup:`10` numbers of bytes
    ``Mb`` for MiB, 2\ :sup:`20` numbers of bytes
    ``Gb`` for GiB, 2\ :sup:`30` numbers of bytes
    ``Tb`` for TiB, 2\ :sup:`40` numbers of bytes
    ``Sec`` for seconds
    ``Min`` for minutes
    ``Hr`` for hours
    ``Day`` for days
    ``Wk`` for weeks

:macro-def:`MAX_NUM_<SUBSYS>_LOG`
    An integer that controls the maximum number of rotations a log file
    is allowed to perform before the oldest one will be rotated away.
    Thus, at most ``MAX_NUM_<SUBSYS>_LOG + 1`` log files of the same
    program coexist at a given time. The default value is 1.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`TRUNC_<SUBSYS>_LOG_ON_OPEN`
    If this macro is defined and set to ``True``, the affected log will
    be truncated and started from an empty file with each invocation of
    the program. Otherwise, new invocations of the program will append
    to the previous log file. By default this setting is ``False`` for
    all daemons.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_LOG_KEEP_OPEN`
    A boolean value that controls
    whether or not the log file is kept open between writes. When
    ``True``, the daemon will not open and close the log file between
    writes. Instead the daemon will hold the log file open until the log
    needs to be rotated. When ``False``, the daemon reverts to the
    previous behavior of opening and closing the log file between
    writes. When the ``$(<SUBSYS>_LOCK)`` macro is defined, setting
    ``$(<SUBSYS>_LOG_KEEP_OPEN)`` has no effect, as the daemon will
    unconditionally revert back to the open/close between writes
    behavior. On Windows platforms, the value defaults to ``True`` for
    all daemons. On Linux platforms, the value defaults to ``True`` for
    all daemons, except the *condor_shadow*, due to a global file
    descriptor limit.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_LOCK`
    This macro specifies the lock file used
    to synchronize append operations to the log file for this subsystem.
    It must be a separate file from the ``$(<SUBSYS>_LOG)`` file, since
    the ``$(<SUBSYS>_LOG)`` file may be rotated and you want to be able
    to synchronize access across log file rotations. A lock file is only
    required for log files which are accessed by more than one process.
    Currently, this includes only the :macro:`SHADOW` subsystem. This macro
    is defined relative to the ``$(LOCK)`` macro.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`JOB_QUEUE_LOG`
    A full path and file name, specifying the job queue log. The default
    value, when not defined is ``$(SPOOL)``/job_queue.log. This
    specification can be useful, if there is a solid state drive which
    is big enough to hold the frequently written to ``job_queue.log``,
    but not big enough to hold the whole contents of the spool
    directory.

:macro-def:`FILE_LOCK_VIA_MUTEX`
    This macro setting only works on Win32 - it is ignored on Unix. If
    set to be ``True``, then log locking is implemented via a kernel
    mutex instead of via file locking. On Win32, mutex access is FIFO,
    while obtaining a file lock is non-deterministic. Thus setting to
    ``True`` fixes problems on Win32 where processes (usually shadows)
    could starve waiting for a lock on a log file. Defaults to ``True``
    on Win32, and is always ``False`` on Unix.

:macro-def:`LOCK_DEBUG_LOG_TO_APPEND`
    A boolean value that defaults to ``False``. This variable controls
    whether a daemon's debug lock is used when appending to the log.
    When ``False``, the debug lock is only used when rotating the log
    file. This is more efficient, especially when many processes share
    the same log file. When ``True``, the debug lock is used when
    writing to the log, as well as when rotating the log file. This
    setting is ignored under Windows, and the behavior of Windows
    platforms is as though this variable were ``True``. Under Unix, the
    default value of ``False`` is appropriate when logging to file
    systems that support the POSIX semantics of ``O_APPEND``. On
    non-POSIX-compliant file systems, it is possible for the characters
    in log messages from multiple processes sharing the same log to be
    interleaved, unless locking is used. Since HTCondor does not support
    sharing of debug logs between processes running on different
    machines, many non-POSIX-compliant file systems will still avoid
    interleaved messages without requiring HTCondor to use a lock. Tests
    of AFS and NFS have not revealed any problems when appending to the
    log without locking.

:macro-def:`ENABLE_USERLOG_LOCKING`
    A boolean value that defaults to ``False`` on Unix platforms and
    ``True`` on Windows platforms. When ``True``, a user's job event log
    will be locked before being written to. If ``False``, HTCondor will
    not lock the file before writing.

:macro-def:`ENABLE_USERLOG_FSYNC`
    A boolean value that is ``True`` by default. When ``True``, writes
    to the user's job event log are sync-ed to disk before releasing the
    lock.

:macro-def:`CREATE_LOCKS_ON_LOCAL_DISK`
    A boolean value utilized only for Unix operating systems, that
    defaults to ``True``. This variable is only relevant if
    :macro:`ENABLE_USERLOG_LOCKING` is ``True``. When ``True``, lock files
    are written to a directory named ``condorLocks``, thereby using a
    local drive to avoid known problems with locking on NFS. The
    location of the ``condorLocks`` directory is determined by

    #. The value of :macro:`TEMP_DIR`, if defined.
    #. The value of :macro:`TMP_DIR`, if defined and :macro:`TEMP_DIR` is not
       defined.
    #. The default value of ``/tmp``, if neither :macro:`TEMP_DIR` nor
       :macro:`TMP_DIR` is defined.

:macro-def:`TOUCH_LOG_INTERVAL`
    The time interval in seconds between when daemons touch their log
    files. The change in last modification time for the log file is
    useful when a daemon restarts after failure or shut down. The last
    modification date is printed, and it provides an upper bound on the
    length of time that the daemon was not running. Defaults to 60
    seconds.

:macro-def:`LOGS_USE_TIMESTAMP`
    This macro controls how the current time is formatted at the start
    of each line in the daemon log files. When ``True``, the Unix time
    is printed (number of seconds since 00:00:00 UTC, January 1, 1970).
    When ``False`` (the default value), the time is printed like so:
    ``<Month>/<Day> <Hour>:<Minute>:<Second>`` in the local timezone.

:macro-def:`DEBUG_TIME_FORMAT`
    This string defines how to format the current time printed at the
    start of each line in the daemon log files. The value is a format
    string is passed to the C strftime() function, so see that manual
    page for platform-specific details. If not defined, the default
    value is

    .. code-block:: text

           "%m/%d/%y %H:%M:%S"

:macro-def:`<SUBSYS>_DEBUG`
    All of the HTCondor daemons can produce different levels of output depending
    on how much information is desired. The various levels of verbosity for a 
    given daemon are determined by this macro. Settings are a
    comma, vertical bar, or space-separated list of categories and options. Each
    category can be followed by a colon and a single digit indicating the verbosity
    for that category ``:1`` is assumed if there is no verbosity modifier.
    Permitted verbosity values are ``:1`` for
    normal, ``:2`` for extra messages, and ``:0`` to disable logging of that
    category of messages. The primary daemon log will always include category and verbosity
    ``D_ALWAYS:1``, unless ``D_ALWAYS:0`` is added to this list.  Category and option names are:

    ``D_ANY``
        This flag turns on all categories of messages Be
        warned: this will generate about a HUGE amount of output. To
        obtain a higher level of output than the default, consider using
        ``D_FULLDEBUG`` before using this option.

    ``D_ALL``
        This is equivalent to ``D_ANY D_PID D_FDS D_CAT`` Be
        warned: this will generate about a HUGE amount of output. To
        obtain a higher level of output than the default, consider using
        ``D_FULLDEBUG`` before using this option.

     ``D_FAILURE``
        This category is used for messages that indicate the daemon is unable
        to continue running. These message are "always" printed unless
        ``D_FAILURE:0`` is added to the list

     ``D_STATUS``
        This category is used for messages that indicate what task the
        daemon is currently doing or progress. Messages of this category will
        be always printed unless ``D_STATUS:0`` is added to the list

    ``D_ALWAYS``
        This category is used for messages that are "always" printed unless
        ``D_ALWAYS:0`` is configured.  These can be progress or status
        message, as well as failures that do not prevent the daemon from
        continuing to operate such as a failure to start a job.  At verbosity
        2 this category is equivalent to ``D_FULLDEBUG`` below.

    ``D_FULLDEBUG``
        This level provides verbose output of a general nature into the
        log files. Frequent log messages for very specific debugging
        purposes would be excluded. In those cases, the messages would
        be viewed by having that other flag and ``D_FULLDEBUG`` both
        listed in the configuration file.  This is equivalent to ``D_ALWAYS:2``

    ``D_DAEMONCORE``
        Provides log file entries specific to DaemonCore, such as timers
        the daemons have set and the commands that are registered. If
        ``D_DAEMONCORE:2`` is set, expect very verbose output.

    ``D_PRIV``
        This flag provides log messages about the privilege state
        switching that the daemons do. See
        :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
        on UIDs in HTCondor for details.

    ``D_COMMAND``
        With this flag set, any daemon that uses DaemonCore will print
        out a log message whenever a command comes in. The name and
        integer of the command, whether the command was sent via UDP or
        TCP, and where the command was sent from are all logged. Because
        the messages about the command used by *condor_kbdd* to
        communicate with the *condor_startd* whenever there is activity
        on the X server, and the command used for keep-alives are both
        only printed with ``D_FULLDEBUG`` enabled, it is best if this
        setting is used for all daemons.  When this flag is used
        with the ``-debug`` option of a tool, the commands that are
        sent by the tool will be included in the debug output.

    ``D_CRON``
        With this flag set, most messages about tasks defined in the
        :macro:`STARTD_CRON_JOBLIST`, :macro:`BENCHMARKS_JOBLIST` or
        :macro:`SCHEDD_CRON_JOBLIST` will be shown.  Note that prior to
        version 23.7 most of these messages were shown as ``D_FULLDEBUG`` messages.
        Some of the more frequent and detailed messages will only be
        shown when ``D_CRON:2`` is set.

    ``D_LOAD``
        The *condor_startd* keeps track of the load average on the
        machine where it is running. Both the general system load
        average, and the load average being generated by HTCondor's
        activity there are determined. With this flag set, the
        *condor_startd* will log a message with the current state of
        both of these load averages whenever it computes them. This flag
        only affects the *condor_startd*.

    ``D_KEYBOARD``
        With this flag set, the *condor_startd* will print out a log
        message with the current values for remote and local keyboard
        idle time. This flag affects only the *condor_startd*.

    ``D_JOB``
        When this flag is set, the *condor_startd* will send to its log
        file the contents of any job ClassAd that the *condor_schedd*
        sends to claim the *condor_startd* for its use. This flag
        affects only the *condor_startd*.

    ``D_MACHINE``
        When this flag is set, the *condor_startd* will send to its log
        file the contents of its resource ClassAd when the
        *condor_schedd* tries to claim the *condor_startd* for its
        use. This flag affects only the *condor_startd*.

    ``D_SYSCALLS``
        This flag is used to make the *condor_shadow* log remote
        syscall requests and return values. This can help track down
        problems a user is having with a particular job by providing the
        system calls the job is performing. If any are failing, the
        reason for the failure is given. The *condor_schedd* also uses
        this flag for the server portion of the queue management code.
        With ``D_SYSCALLS`` defined in :macro:`SCHEDD_DEBUG` there will be
        verbose logging of all queue management operations the
        *condor_schedd* performs.

    ``D_MATCH``
        When this flag is set, the *condor_negotiator* logs a message
        for every match.

    ``D_NETWORK``
        When this flag is set, all HTCondor daemons will log a message
        on every TCP accept, connect, and close, and on every UDP send
        and receive. This flag is not yet fully supported in the
        *condor_shadow*.

    ``D_HOSTNAME``
        When this flag is set, the HTCondor daemons and/or tools will
        print verbose messages explaining how they resolve host names,
        domain names, and IP addresses. This is useful for sites that
        are having trouble getting HTCondor to work because of problems
        with DNS, NIS or other host name resolving systems in use.

    ``D_SECURITY``
        This flag will enable debug messages pertaining to the setup of
        secure network communication, including messages for the
        negotiation of a socket authentication mechanism, the management
        of a session key cache. and messages about the authentication
        process itself. See
        :ref:`admin-manual/security:htcondor's security model`
        for more information about secure communication configuration.
        ``D_SECURITY:2`` logging is highly verbose and should be used only
        when actively debugging security configuration problems.

    ``D_PROCFAMILY``
        HTCondor often times needs to manage an entire family of
        processes, (that is, a process and all descendants of that
        process). This debug flag will turn on debugging output for the
        management of families of processes.

    ``D_ACCOUNTANT``
        When this flag is set, the *condor_negotiator* will output
        debug messages relating to the computation of user priorities
        (see :doc:`/admin-manual/cm-configuration`).

    ``D_PROTOCOL``
        Enable debug messages relating to the protocol for HTCondor's
        matchmaking and resource claiming framework.

    ``D_STATS``
        Enable debug messages relating to the TCP statistics for file
        transfers. Note that the shadow and starter, by default, log
        these statistics to special log files (see :macro:`SHADOW_STATS_LOG`
        and :macro:`STARTER_STATS_LOG`. Note that, as of version 8.5.6, 
        :macro:`C_GAHP_DEBUG` defaults to ``D_STATS``.

    ``D_PID``
        This flag is different from the other flags, because it is used
        to change the formatting of all log messages that are printed,
        as opposed to specifying what kinds of messages should be
        printed. If ``D_PID`` is set, HTCondor will always print out the
        process identifier (PID) of the process writing each line to the
        log file. This is especially helpful for HTCondor daemons that
        can fork multiple helper-processes (such as the *condor_schedd*
        or *condor_collector*) so the log file will clearly show which
        thread of execution is generating each log message.

    ``D_FDS``
        This flag is different from the other flags, because it is used
        to change the formatting of all log messages that are printed,
        as opposed to specifying what kinds of messages should be
        printed. If ``D_FDS`` is set, HTCondor will always print out the
        file descriptor that the open of the log file was allocated by
        the operating system. This can be helpful in debugging
        HTCondor's use of system file descriptors as it will generally
        track the number of file descriptors that HTCondor has open.
        Note the use of this flag is relatively expensive, so it 
        should only be enabled when you suspect there is a file
        descriptor leak.

    ``D_CAT`` or ``D_CATEGORY``
        This flag is different from the other flags, because it is used
        to change the formatting of all log messages that are printed,
        as opposed to specifying what kinds of messages should be
        printed. If ``D_CAT`` or ``D_CATEGORY`` is set, Condor will include the
        debugging level flags that were in effect for each line of
        output.  This may be used to filter log output by the level or
        tag it, for example, identifying all logging output at level
        ``D_SECURITY``, or ``D_ACCOUNTANT``.

    ``D_TIMESTAMP``
        This flag is different from the other flags, because it is used
        to change the formatting of all log messages that are printed,
        as opposed to specifying what kinds of messages should be
        printed. If ``D_TIMESTAMP`` is set, the time at the beginning of
        each line in the log file with be a number of seconds since the
        start of the Unix era. This form of timestamp can be more
        convenient for tools to process.

    ``D_SUB_SECOND``
        This flag is different from the other flags, because it is used
        to change the formatting of all log messages that are printed,
        as opposed to specifying what kinds of messages should be
        printed. If ``D_SUB_SECOND`` is set, the time at the beginning
        of each line in the log file will contain a fractional part to
        the seconds field that is accurate to the millisecond.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`ALL_DEBUG`
    Used to make all subsystems share a debug flag. Set the parameter
    :macro:`ALL_DEBUG` instead of changing all of the individual parameters.
    For example, to turn on all debugging in all subsystems, set
    ALL_DEBUG = D_ALL.

:macro-def:`TOOL_DEBUG`
    Uses the same values (debugging levels) as :macro:`<SUBSYS>_DEBUG` to
    describe the amount of debugging information sent to ``stderr`` for
    HTCondor tools.

Log files may optionally be specified per debug level as follows:

:macro-def:`<SUBSYS>_<LEVEL>_LOG`
    The name of a log file for
    messages at a specific debug level for a specific subsystem. <LEVEL>
    is defined by any debug level, but without the ``D_`` prefix. See
    :macro:`<SUBSYS>_DEBUG` for the list of debug levels.
    If the debug level is included in ``$(<SUBSYS>_DEBUG)``, then all
    messages of this debug level will be written both to the log file
    defined by :macro:`<SUBSYS>_LOG` and the log file defined by
    :macro:`<SUBSYS>_<LEVEL>_LOG`. As examples, ``SHADOW_SYSCALLS_LOG``
    specifies a log file for all remote system call debug messages, and
    :macro:`NEGOTIATOR_MATCH_LOG` specifies a log file that only captures
    *condor_negotiator* debug events occurring with matches.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MAX_<SUBSYS>_<LEVEL>_LOG`
    See :macro:`MAX_<SUBSYS>_LOG`.

:macro-def:`TRUNC_<SUBSYS>_<LEVEL>_LOG_ON_OPEN`
    See :macro:`TRUNC_<SUBSYS>_LOG_ON_OPEN`.

The following macros control where and what is written to the event log,
a file that receives job events, but across all users and user's jobs.

:macro-def:`EVENT_LOG`
    The full path and file name of the event log. There is no default
    value for this variable, so no event log will be written, if not
    defined.

:macro-def:`EVENT_LOG_MAX_SIZE`
    Controls the maximum length in bytes to which the event log will be
    allowed to grow. The log file will grow to the specified length,
    then be saved to a file with the suffix .old. The .old files are
    overwritten each time the log is saved. A value of 0 specifies that
    the file may grow without bounds (and disables rotation). The
    default is 1 MiB. For backwards compatibility, :macro:`MAX_EVENT_LOG`
    will be used if :macro:`EVENT_LOG_MAX_SIZE` is not defined. If
    :macro:`EVENT_LOG` is not defined, this parameter has no effect.

:macro-def:`MAX_EVENT_LOG`
    See :macro:`EVENT_LOG_MAX_SIZE`.

:macro-def:`EVENT_LOG_MAX_ROTATIONS`
    Controls the maximum number of rotations of the event log that will
    be stored. If this value is 1 (the default), the event log will be
    rotated to a ".old" file as described above. However, if this is
    greater than 1, then multiple rotation files will be stores, up to
    :macro:`EVENT_LOG_MAX_ROTATIONS` of them. These files will be named,
    instead of the ".old" suffix, ".1", ".2", with the ".1" being the
    most recent rotation. This is an integer parameter with a default
    value of 1. If :macro:`EVENT_LOG` is not defined, or if
    :macro:`EVENT_LOG_MAX_SIZE` has a value of 0 (which disables event log
    rotation), this parameter has no effect.

:macro-def:`EVENT_LOG_ROTATION_LOCK`
    Specifies the lock file that will be used to ensure that, when
    rotating files, the rotation is done by a single process. This is a
    string parameter; its default value is ``$(LOCK)/EventLogLock``. If
    an empty value is set, then the file that is used is the file path
    of the event log itself, with the string ``.lock`` appended. If
    :macro:`EVENT_LOG` is not defined, or if :macro:`EVENT_LOG_MAX_SIZE` has a
    value of 0 (which disables event log rotation), this configuration
    variable has no effect.

:macro-def:`EVENT_LOG_FSYNC`
    A boolean value that controls whether HTCondor will perform an
    fsync() after writing each event to the event log. When ``True``, an
    fsync() operation is performed after each event. This fsync()
    operation forces the operating system to synchronize the updates to
    the event log to the disk, but can negatively affect the performance
    of the system. Defaults to ``False``.

:macro-def:`EVENT_LOG_LOCKING`
    A boolean value that defaults to ``False`` on Unix platforms and
    ``True`` on Windows platforms. When ``True``, the event log (as
    specified by :macro:`EVENT_LOG`) will be locked before being written to.
    When ``False``, HTCondor does not lock the file before writing.

:macro-def:`EVENT_LOG_COUNT_EVENTS`
    A boolean value that is ``False`` by default. When ``True``, upon
    rotation of the user's job event log, a count of the number of job
    events is taken by scanning the log, such that the newly created,
    post-rotation user job event log will have this count in its header.
    This configuration variable is relevant when rotation of the user's
    job event log is enabled.

:macro-def:`EVENT_LOG_FORMAT_OPTIONS`
    A list of case-insensitive keywords that control formatting of the log events
    and of timestamps for the log specified by :macro:`EVENT_LOG`.  Use zero or one of the
    following formatting options:

    ``XML``
        Log events in XML format. This has the same effect :macro:`EVENT_LOG_USE_XML` below

    ``JSON``
        Log events in JSON format. This conflicts with :macro:`EVENT_LOG_USE_XML` below

    And zero or more of the following option flags:

    ``UTC``
        Log event timestamps as Universal Coordinated Time. The time value will be printed
        with a timezone value of Z to indicate that times are UTC.

    ``ISO_DATE``
        Log event timestamps in ISO 8601 format. This format includes a 4 digit year and is
        printed in a way that makes sorting by date easier.

    ``SUB_SECOND``
        Include fractional seconds in event timestamps.

    ``LEGACY``
        Set all time formatting flags to be compatible with older versions of HTCondor.

    All of the above options are case-insensitive, and can be preceded by a ! to invert their meaning,
    so configuring ``!UTC, !ISO_DATE, !SUB_SECOND`` gives the same result as configuring ``LEGACY``.

:macro-def:`EVENT_LOG_USE_XML`
    A boolean value that defaults to ``False``. When ``True``, events
    are logged in XML format. If :macro:`EVENT_LOG` is not defined, this
    parameter has no effect.

:macro-def:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS`
    A comma separated list of job ClassAd attributes, whose evaluated
    values form a new event, the ``JobAdInformationEvent``, given Event
    Number 028. This new event is placed in the event log in addition to
    each logged event. If :macro:`EVENT_LOG` is not defined, this
    configuration variable has no effect. This configuration variable is
    the same as the job ClassAd attribute :ad-attr:`JobAdInformationAttrs` (see
    :doc:`/classad-attributes/job-classad-attributes`), but it
    applies to the system Event Log rather than the user job log.

:macro-def:`DEFAULT_USERLOG_FORMAT_OPTIONS`
    A list of case-insensitive keywords that control formatting of the events
    and of timestamps for the log specified by a job's :ad-attr:`UserLog` or :ad-attr:`DAGManNodesLog`
    attributes. see :macro:`EVENT_LOG_FORMAT_OPTIONS` above for the permitted options.

:index:`DaemonCore Options<single: Configuration; DaemonCore Options>`

.. _daemoncore_config_options:

DaemonCore Configuration File Entries
-------------------------------------

Please read :ref:`admin-manual/installation-startup-shutdown-reconfiguration:DaemonCore` for
details on DaemonCore. There are certain configuration file settings
that DaemonCore uses which affect all HTCondor daemons.

:macro-def:`ALLOW_*`
    .. faux-definition::

:macro-def:`DENY_*`
    All macros that begin with either :macro:`ALLOW_*` or
    :macro:`DENY_*` are settings for HTCondor's security.
    See :ref:`admin-manual/security:authorization` on Setting
    up security in HTCondor for details on these macros and how to
    configure them.

:macro-def:`ENABLE_RUNTIME_CONFIG`
    The :tool:`condor_config_val` tool has an option **-rset** for
    dynamically setting run time configuration values, and which only
    affect the in-memory configuration variables. Because of the
    potential security implications of this feature, by default,
    HTCondor daemons will not honor these requests. To use this
    functionality, HTCondor administrators must specifically enable it
    by setting :macro:`ENABLE_RUNTIME_CONFIG` to ``True``, and specify what
    configuration variables can be changed using the ``SETTABLE_ATTRS...``
    family of configuration options. Defaults to ``False``.

:macro-def:`ENABLE_PERSISTENT_CONFIG`
    The :tool:`condor_config_val` tool has a **-set** option for dynamically
    setting persistent configuration values. These values override
    options in the normal HTCondor configuration files. Because of the
    potential security implications of this feature, by default,
    HTCondor daemons will not honor these requests. To use this
    functionality, HTCondor administrators must specifically enable it
    by setting :macro:`ENABLE_PERSISTENT_CONFIG` to ``True``, creating a
    directory where the HTCondor daemons will hold these
    dynamically-generated persistent configuration files (declared using
    :macro:`PERSISTENT_CONFIG_DIR`, described below) and specify what
    configuration variables can be changed using the ``SETTABLE_ATTRS...``
    family of configuration options. Defaults to ``False``.

:macro-def:`PERSISTENT_CONFIG_DIR`
    Directory where daemons should store dynamically-generated
    persistent configuration files (used to support
    :tool:`condor_config_val` **-set**) This directory should **only** be
    writable by root, or the user the HTCondor daemons are running as
    (if non-root). There is no default, administrators that wish to use
    this functionality must create this directory and define this
    setting. This directory must not be shared by multiple HTCondor
    installations, though it can be shared by all HTCondor daemons on
    the same host. Keep in mind that this directory should not be placed
    on an NFS mount where "root-squashing" is in effect, or else
    HTCondor daemons running as root will not be able to write to them.
    A directory (only writable by root) on the local file system is
    usually the best location for this directory.

:macro-def:`SETTABLE_ATTRS_<PERMISSION-LEVEL>`
    :index:`SETTABLE_ATTRS_CONFIG`
    All macros that begin with ``SETTABLE_ATTRS`` or
    ``<SUBSYS>.SETTABLE_ATTRS`` are settings used to restrict the
    configuration values that can be changed using the
    :tool:`condor_config_val` command.
    See :ref:`admin-manual/security:authorization` on Setting up
    Security in HTCondor for details on these macros and how to
    configure them. In particular,
    :ref:`admin-manual/security:authorization` contains details
    specific to these macros.

:macro-def:`SHUTDOWN_GRACEFUL_TIMEOUT`
    Determines how long HTCondor will allow daemons try their graceful
    shutdown methods before they do a hard shutdown. It is defined in
    terms of seconds. The default is 1800 (30 minutes).

:macro-def:`<SUBSYS>_ADDRESS_FILE`
    :index:`NEGOTIATOR_ADDRESS_FILE`
    :index:`COLLECTOR_ADDRESS_FILE` A complete path to a file that
    is to contain an IP address and port number for a daemon. Every
    HTCondor daemon that uses DaemonCore has a command port where
    commands are sent. The IP/port of the daemon is put in that daemon's
    ClassAd, so that other machines in the pool can query the
    *condor_collector* (which listens on a well-known port) to find the
    address of a given daemon on a given machine. When tools and daemons
    are all executing on the same single machine, communications do not
    require a query of the *condor_collector* daemon. Instead, they
    look in a file on the local disk to find the IP/port. This macro
    causes daemons to write the IP/port of their command socket to a
    specified file. In this way, local tools will continue to operate,
    even if the machine running the *condor_collector* crashes. Using
    this file will also generate slightly less network traffic in the
    pool, since tools including :tool:`condor_q` and :tool:`condor_rm` do not need
    to send any messages over the network to locate the *condor_schedd*
    daemon. This macro is not necessary for the *condor_collector*
    daemon, since its command socket is at a well-known port.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_SUPER_ADDRESS_FILE`
    :index:`SCHEDD_SUPER_ADDRESS_FILE`
    :index:`COLLECTOR_SUPER_ADDRESS_FILE` A complete path to a
    file that is to contain an IP address and port number for a command
    port that is serviced with priority for a daemon. Every HTCondor
    daemon that uses DaemonCore may have a higher priority command port
    where commands are sent. Any command that goes through
    :tool:`condor_sos`, and any command issued by the super user (root or
    local system) for a daemon on the local machine will have the
    command sent to this port. Default values are provided for the
    *condor_schedd* daemon at ``$(SPOOL)/.schedd_address.super`` and
    the *condor_collector* daemon at
    ``$(LOG)/.collector_address.super``. When not defined for other
    DaemonCore daemons, there will be no higher priority command port.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_DAEMON_AD_FILE`
    A complete path to a file
    that is to contain the ClassAd for a daemon. When the daemon sends a
    ClassAd describing itself to the *condor_collector*, it will also
    place a copy of the ClassAd in this file. Currently, this setting
    only works for the *condor_schedd*. :index:`<SUBSYS>_ATTRS`

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_ATTRS`
    Allows any DaemonCore daemon to advertise arbitrary expressions from
    the configuration file in its ClassAd. Give the list
    of entries from the configuration file you want in the given
    daemon's ClassAd. Frequently used to add attributes to machines so
    that the machines can discriminate between other machines in a job's
    **rank** and **requirements**.

    The macro is named by substituting :macro:`<SUBSYS>` with the appropriate
    subsystem string as defined by :macro:`SUBSYSTEM`.

    .. note::

        The *condor_kbdd* does not send ClassAds now, so this entry
        does not affect it. The *condor_startd*, *condor_schedd*,
        :tool:`condor_master`, and *condor_collector* do send ClassAds, so those
        would be valid subsystems to set this entry for.

    :macro:`SUBMIT_ATTRS` not part of the :macro:`<SUBSYS>_ATTRS`, it is
    documented in :macro:`SUBMIT_ATTRS`.

    Because of the different syntax of the configuration file and
    ClassAds, a little extra work is required to get a given entry into
    a ClassAd. In particular, ClassAds require quote marks (") around
    strings. Numeric values and boolean expressions can go in directly.
    For example, if the *condor_startd* is to advertise a string macro,
    a numeric macro, and a boolean expression, do something similar to:

    .. code-block:: condor-config

            STRING = This is a string
            NUMBER = 666
            BOOL1 = True
            BOOL2 = time() >= $(NUMBER) || $(BOOL1)
            MY_STRING = "$(STRING)"
            STARTD_ATTRS = MY_STRING, NUMBER, BOOL1, BOOL2

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`DAEMON_SHUTDOWN`
    Starting with HTCondor version 6.9.3, whenever a daemon is about to
    publish a ClassAd update to the *condor_collector*, it will
    evaluate this expression. If it evaluates to ``True``, the daemon
    will gracefully shut itself down, exit with the exit code 99, and
    will not be restarted by the :tool:`condor_master` (as if it sent itself
    a :tool:`condor_off` command). The expression is evaluated in the context
    of the ClassAd that is being sent to the *condor_collector*, so it
    can reference any attributes that can be seen with
    condor_status -long [-daemon_type] (for example,
    condor_status -long [-master] for the :tool:`condor_master`). Since each
    daemon's ClassAd will contain different attributes, administrators
    should define these shutdown expressions specific to each daemon,
    for example:

    .. code-block:: condor-config

            STARTD.DAEMON_SHUTDOWN = when to shutdown the startd
            MASTER.DAEMON_SHUTDOWN = when to shutdown the master


    Normally, these expressions would not be necessary, so if not
    defined, they default to FALSE.

    .. note::

        This functionality does not work in conjunction with
        HTCondor's high-availability support (see
        :ref:`admin-manual/cm-configuration:High Availability of the Central Manager`
        for more information). If you enable high-availability for a
        particular daemon, you should not define this expression.

:macro-def:`DAEMON_SHUTDOWN_FAST`
    Identical to :macro:`DAEMON_SHUTDOWN` (defined above), except the daemon
    will use the fast shutdown mode (as if it sent itself a
    :tool:`condor_off` command using the **-fast** option).

:macro-def:`USE_CLONE_TO_CREATE_PROCESSES`
    A boolean value that controls how an HTCondor daemon creates a new
    process on Linux platforms. If set to the default value of ``True``,
    the ``clone`` system call is used. Otherwise, the ``fork`` system
    call is used. ``clone`` provides scalability improvements for
    daemons using a large amount of memory, for example, a
    *condor_schedd* with a lot of jobs in the queue. Currently, the use
    of ``clone`` is available on Linux systems. If HTCondor detects that
    it is running under the *valgrind* analysis tools, this setting is
    ignored and treated as ``False``, to work around incompatibilities.

:macro-def:`MAX_TIME_SKIP`
    When an HTCondor daemon notices the system clock skip forwards or
    backwards more than the number of seconds specified by this
    parameter, it may take special action. For instance, the
    :tool:`condor_master` will restart HTCondor in the event of a clock skip.
    Defaults to a value of 1200, which in effect means that HTCondor
    will restart if the system clock jumps by more than 20 minutes.

:macro-def:`NOT_RESPONDING_TIMEOUT`
    When an HTCondor daemon's parent process is another HTCondor daemon,
    the child daemon will periodically send a short message to its
    parent stating that it is alive and well. If the parent does not
    hear from the child for a while, the parent assumes that the child
    is hung, kills the child, and restarts the child. This parameter
    controls how long the parent waits before killing the child. It is
    defined in terms of seconds and defaults to 3600 (1 hour). The child
    sends its alive and well messages at an interval of one third of
    this value.

:macro-def:`<SUBSYS>_NOT_RESPONDING_TIMEOUT`
    Identical to :macro:`NOT_RESPONDING_TIMEOUT`, but controls the timeout
    for a specific type of daemon. For example,
    ``SCHEDD_NOT_RESPONDING_TIMEOUT`` controls how long the
    *condor_schedd* 's parent daemon will wait without receiving an
    alive and well message from the *condor_schedd* before killing it.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`NOT_RESPONDING_WANT_CORE`
    A boolean value with a default value of ``False``. This parameter is
    for debugging purposes on Unix systems, and it controls the behavior
    of the parent process when the parent process determines that a
    child process is not responding. If :macro:`NOT_RESPONDING_WANT_CORE` is
    ``True``, the parent will send a SIGABRT instead of SIGKILL to the
    child process. If the child process is configured with the
    configuration variable :macro:`CREATE_CORE_FILES` enabled, the child
    process will then generate a core dump. See :macro:`NOT_RESPONDING_TIMEOUT`
    and :macro:`CREATE_CORE_FILES` for more details.

:macro-def:`LOCK_FILE_UPDATE_INTERVAL`
    An integer value representing seconds, controlling how often valid
    lock files should have their on disk timestamps updated. Updating
    the timestamps prevents administrative programs, such as *tmpwatch*,
    from deleting long lived lock files. If set to a value less than 60,
    the update time will be 60 seconds. The default value is 28800,
    which is 8 hours. This variable only takes effect at the start or
    restart of a daemon.

:macro-def:`SOCKET_LISTEN_BACKLOG`
    An integer value that defaults to 4096, which defines the backlog
    value for the listen() network call when a daemon creates a socket
    for incoming connections. It limits the number of new incoming
    network connections the operating system will accept for a daemon
    that the daemon has not yet serviced.

:macro-def:`MAX_ACCEPTS_PER_CYCLE`
    An integer value that defaults to 8. It is a rarely changed
    performance tuning parameter to limit the number of accepts of new,
    incoming, socket connect requests per DaemonCore event cycle. A
    value of zero or less means no limit. It has the most noticeable
    effect on the *condor_schedd*, and would be given a higher integer
    value for tuning purposes when there is a high number of jobs
    starting and exiting per second.

:macro-def:`MAX_TIMER_EVENTS_PER_CYCLE`
    An integer value that defaults to 3. It is a rarely changed
    performance tuning parameter to set the max number of internal
    timer events will be dispatched per DaemonCore event cycle.
    A value of zero means no limit, so that all timers that are due
    at the start of the event cycle should be dispatched.

:macro-def:`MAX_UDP_MSGS_PER_CYCLE`
    An integer value that defaults to 1. It is a rarely changed
    performance tuning parameter to set the number of incoming UDP
    messages a daemon will read per DaemonCore event cycle.
    A value of zero means no limit. It has the most noticeable
    effect on the *condor_schedd* and *condor_collector* daemons,
    which can receive a large number of UDP messages when under heavy
    load.

:macro-def:`MAX_REAPS_PER_CYCLE`
    An integer value that defaults to 0. It is a rarely changed
    performance tuning parameter that places a limit on the number of
    child process exits to process per DaemonCore event cycle. A value
    of zero or less means no limit.

:macro-def:`CORE_FILE_NAME`
    Defines the name of the core file created on Windows platforms.
    Defaults to ``core.$(SUBSYSTEM).WIN32``.

:macro-def:`PIPE_BUFFER_MAX`
    The maximum number of bytes read from a ``stdout`` or ``stdout``
    pipe. The default value is 10240. A rare example in which the value
    would need to increase from its default value is when a hook must
    output an entire ClassAd, and the ClassAd may be larger than the
    default.

:macro-def:`DETECTED_MEMORY`
    A read-only macro that cannot be set, but expands to the
    amount of detected memory on the system, regardless
    of any overrides via the :macro:`MEMORY` setting.

:index:`Networking Options<single: Configuration; Networking Options>`

.. _networking_config_options:

Networking Configuration Options
--------------------------------

More information about networking in HTCondor can be found in
:doc:`/admin-manual/networking`.

:macro-def:`BIND_ALL_INTERFACES[Networking]`
    For systems with multiple network interfaces, if this configuration
    setting is ``False``, HTCondor will only bind network sockets to the
    IP address specified with :macro:`NETWORK_INTERFACE` (described below).
    If set to ``True``, the default value, HTCondor will listen on all
    interfaces. However, currently HTCondor is still only able to
    advertise a single IP address, even if it is listening on multiple
    interfaces. By default, it will advertise the IP address of the
    network interface used to contact the collector, since this is the
    most likely to be accessible to other processes which query
    information from the same collector. More information about using
    this setting can be found in
    :ref:`admin-manual/networking:configuring htcondor for machines with multiple network interfaces`.

:macro-def:`CCB_ADDRESS[Networking]`
    This is the address of a *condor_collector* that will serve as this
    daemon's HTCondor Connection Broker (CCB). Multiple addresses may be
    listed (separated by commas and/or spaces) for redundancy. The CCB
    server must authorize this daemon at DAEMON level for this
    configuration to succeed. It is highly recommended to also configure
    :macro:`PRIVATE_NETWORK_NAME` if you configure :macro:`CCB_ADDRESS` so
    communications originating within the same private network do not
    need to go through CCB. For more information about CCB, see
    :ref:`admin-manual/networking:htcondor connection brokering (ccb)`.

:macro-def:`CCB_HEARTBEAT_INTERVAL[Networking]`
    This is the maximum number of seconds of silence on a daemon's
    connection to the CCB server after which it will ping the server to
    verify that the connection still works. The default is 5 minutes.
    This feature serves to both speed up detection of dead connections
    and to generate a guaranteed minimum frequency of activity to
    attempt to prevent the connection from being dropped. The special
    value 0 disables the heartbeat. The heartbeat is automatically
    disabled if the CCB server is older than HTCondor version 7.5.0.
    Having the heartbeat interval greater than the job ClassAd attribute
    :ad-attr:`JobLeaseDuration` may cause unnecessary job disconnects in pools
    with network issues.

:macro-def:`CCB_POLLING_INTERVAL[Networking]`
    In seconds, the smallest amount of time that could go by before CCB
    would begin another round of polling to check on already connected
    clients. While the value of this variable does not change, the
    actual interval used may be exceeded if the measured amount of time
    previously taken to poll to check on already connected clients
    exceeded the amount of time desired, as expressed with
    :macro:`CCB_POLLING_TIMESLICE`. The default value is 20 seconds.

:macro-def:`CCB_POLLING_MAX_INTERVAL[Networking]`
    In seconds, the interval of time after which polling to check on
    already connected clients must occur, independent of any other
    factors. The default value is 600 seconds.

:macro-def:`CCB_POLLING_TIMESLICE[Networking]`
    A floating point fraction representing the fractional amount of the
    total run time of CCB to set as a target for the maximum amount of
    CCB running time used on polling to check on already connected
    clients. The default value is 0.05.

:macro-def:`CCB_READ_BUFFER[Networking]`
    The size of the kernel TCP read buffer in bytes for all sockets used
    by CCB. The default value is 2 KiB.

:macro-def:`CCB_REQUIRED_TO_START[Networking]`
    If true, and :macro:`USE_SHARED_PORT` is false, and :macro:`CCB_ADDRESS`
    is set, but HTCondor fails to register with any broker, HTCondor will
    exit rather then continue to retry indefinitely.

:macro-def:`CCB_TIMEOUT[Networking]`
    The length, in seconds, that we wait for any CCB operation to complete.
    The default value is 300.

:macro-def:`CCB_WRITE_BUFFER[Networking]`
    The size of the kernel TCP write buffer in bytes for all sockets
    used by CCB. The default value is 2 KiB.

:macro-def:`CCB_SWEEP_INTERVAL[Networking]`
    The interval, in seconds, between times when the CCB server writes
    its information about open TCP connections to a file. Crash recovery
    is accomplished using the information. The default value is 1200
    seconds (20 minutes).

:macro-def:`CCB_RECONNECT_FILE[Networking]`
    The full path and file name of the file that the CCB server writes
    its information about open TCP connections to a file. Crash recovery
    is accomplished using the information. The default value is
    ``$(SPOOL)/<ip address>-<shared port ID or port number>.ccb_reconnect``.

:macro-def:`COLLECTOR_USES_SHARED_PORT[Networking]`
    A boolean value that specifies whether the *condor_collector* uses
    the *condor_shared_port* daemon. When true, the
    *condor_shared_port* will transparently proxy queries to the
    *condor_collector* so users do not need to be aware of the presence
    of the *condor_shared_port* when querying the collector and
    configuring other daemons. The default is ``True``

:macro-def:`SHARED_PORT_DEFAULT_ID[Networking]`
    When :macro:`COLLECTOR_USES_SHARED_PORT` is set to ``True``, this
    is the shared port ID used by the *condor_collector*. This defaults
    to ``collector`` and will not need to be changed by most sites.

:macro-def:`AUTO_INCLUDE_SHARED_PORT_IN_DAEMON_LIST[Networking]`
    A boolean value that specifies whether :macro:`SHARED_PORT`
    should be automatically inserted into :tool:`condor_master` 's
    :macro:`DAEMON_LIST` when :macro:`USE_SHARED_PORT` is
    ``True``. The default for this setting is ``True``.

:macro-def:`<SUBSYS>_MAX_FILE_DESCRIPTORS[Networking]`
    This setting is identical to :macro:`MAX_FILE_DESCRIPTORS`, but it only
    applies to a specific subsystem. If the subsystem-specific setting
    is unspecified, :macro:`MAX_FILE_DESCRIPTORS` is used. For the
    *condor_collector* daemon, the value defaults to 10240, and for the
    *condor_schedd* daemon, the value defaults to 4096. If the
    *condor_shared_port* daemon is in use, its value for this
    parameter should match the largest value set for the other daemons.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MAX_FILE_DESCRIPTORS[Networking]`
    Under Unix, this specifies the maximum number of file descriptors to
    allow the HTCondor daemon to use. File descriptors are a system
    resource used for open files and for network connections. HTCondor
    daemons that make many simultaneous network connections may require
    an increased number of file descriptors. For example, see
    :ref:`admin-manual/networking:htcondor connection brokering (ccb)`
    for information on file descriptor requirements of CCB. Changes to
    this configuration variable require a restart of HTCondor in order
    to take effect. Also note that only if HTCondor is running as root
    will it be able to increase the limit above the hard limit (on
    maximum open files) that it inherits.

:macro-def:`NETWORK_HOSTNAME[Networking]`
    The name HTCondor should use as the host name of the local machine,
    overriding the value returned by gethostname(). Among other things,
    the host name is used to identify daemons in an HTCondor pool, via
    the ``Machine`` and ``Name`` attributes of daemon ClassAds. This
    variable can be used when a machine has multiple network interfaces
    with different host names, to use a host name that is not the
    primary one. It should be set to a fully-qualified host name that
    will resolve to an IP address of the local machine.

:macro-def:`NETWORK_INTERFACE[Networking]`
    An IP address of the form ``192.0.2.123`` or the name of a
    network device, as in the example ``eth0``. The wild card character
    (``*``) may be used within either. For example, ``192.0.2.*`` would
    match a network interface with an IP address of ``192.0.2.123``
    or ``192.0.2.100``. The default value is ``*``, which matches
    all network interfaces.

    The effect of this variable depends on the value of
    :macro:`BIND_ALL_INTERFACES`. There are two cases:

    If :macro:`BIND_ALL_INTERFACES` is ``True`` (the default),
    :macro:`NETWORK_INTERFACE` controls what IP address will be advertised as
    the public address of the daemon. If multiple network interfaces
    match the value, the IP address that is chosen to be advertised will
    be the one associated with the first device (in system-defined
    order) that is in a public address space, or a private address
    space, or a loopback address, in that order of preference. If it is
    desired to advertise an IP address that is not associated with any
    local network interface, for example, when TCP forwarding is being
    used, then :macro:`TCP_FORWARDING_HOST` should be used instead of
    :macro:`NETWORK_INTERFACE`.

    If :macro:`BIND_ALL_INTERFACES` is ``False``, then :macro:`NETWORK_INTERFACE`
    specifies which IP address HTCondor should use for all incoming and
    outgoing communication. If more than one IP address matches the
    value, then the IP address that is chosen will be the one associated
    with the first device (in system-defined order) that is in a public
    address space, or a private address space, or a loopback address, in
    that order of preference.

    More information about configuring HTCondor on machines with
    multiple network interfaces can be found in
    :ref:`admin-manual/networking:configuring htcondor for machines with
    multiple network interfaces`.

:macro-def:`PRIVATE_NETWORK_NAME[Networking]`
    If two HTCondor daemons are trying to communicate with each other,
    and they both belong to the same private network, this setting will
    allow them to communicate directly using the private network
    interface, instead of having to use CCB or to go through a public IP
    address. Each private network should be assigned a unique network
    name. This string can have any form, but it must be unique for a
    particular private network. If another HTCondor daemon or tool is
    configured with the same :macro:`PRIVATE_NETWORK_NAME`, it will attempt
    to contact this daemon using its private network address. Even for
    sites using CCB, this is an important optimization, since it means
    that two daemons on the same network can communicate directly,
    without having to go through the broker. If CCB is enabled, and the
    :macro:`PRIVATE_NETWORK_NAME` is defined, the daemon's private address
    will be defined automatically. Otherwise, you can specify a
    particular private IP address to use by defining the
    :macro:`PRIVATE_NETWORK_INTERFACE` setting (described below). The default
    is ``$(FULL_HOSTNAME)``. After changing this setting and running
    :tool:`condor_reconfig`, it may take up to one *condor_collector* update
    interval before the change becomes visible.

:macro-def:`PRIVATE_NETWORK_INTERFACE[Networking]`
    For systems with multiple network interfaces, if this configuration
    setting and :macro:`PRIVATE_NETWORK_NAME` are both defined, HTCondor
    daemons will advertise some additional attributes in their ClassAds
    to help other HTCondor daemons and tools in the same private network
    to communicate directly.

    :macro:`PRIVATE_NETWORK_INTERFACE` defines what IP address of the form
    ``192.0.2.123`` or name of a network device (as in the example
    ``eth0``) a given multi-homed machine should use for the private
    network. The asterisk (\*) may be used as a wild card character
    within either the IP address or the device name. If another HTCondor
    daemon or tool is configured with the same :macro:`PRIVATE_NETWORK_NAME`,
    it will attempt to contact this daemon using the IP address
    specified here. The syntax for specifying an IP address is identical
    to :macro:`NETWORK_INTERFACE`. Sites using CCB only need to define the
    :macro:`PRIVATE_NETWORK_NAME`, and the :macro:`PRIVATE_NETWORK_INTERFACE` will
    be defined automatically. Unless CCB is enabled, there is no default
    value for this variable. After changing this variable and running
    :tool:`condor_reconfig`, it may take up to one *condor_collector* update
    interval before the change becomes visible.

:macro-def:`TCP_FORWARDING_HOST[Networking]`
    This specifies the host or IP address that should be used as the
    public address of this daemon. If a host name is specified, be aware
    that it will be resolved to an IP address by this daemon, not by the
    clients wishing to connect to it. It is the IP address that is
    advertised, not the host name. This setting is useful if HTCondor on
    this host may be reached through a NAT or firewall by connecting to
    an IP address that forwards connections to this host. It is assumed
    that the port number on the :macro:`TCP_FORWARDING_HOST` that forwards to
    this host is the same port number assigned to HTCondor on this host.
    This option could also be used when ssh port forwarding is being
    used. In this case, the incoming addresses of connections to this
    daemon will appear as though they are coming from the forwarding
    host rather than from the real remote host, so any authorization
    settings that rely on host addresses should be considered
    accordingly.

:macro-def:`HIGHPORT[Networking]`
    Specifies an upper limit of given port numbers for HTCondor to use,
    such that HTCondor is restricted to a range of port numbers. If this
    macro is not explicitly specified, then HTCondor will not restrict
    the port numbers that it uses. HTCondor will use system-assigned
    port numbers. For this macro to work, both :macro:`HIGHPORT` and
    :macro:`LOWPORT` (given below) must be defined.

:macro-def:`LOWPORT[Networking]`
    Specifies a lower limit of given port numbers for HTCondor to use,
    such that HTCondor is restricted to a range of port numbers. If this
    macro is not explicitly specified, then HTCondor will not restrict
    the port numbers that it uses. HTCondor will use system-assigned
    port numbers. For this macro to work, both :macro:`HIGHPORT` (given
    above) and :macro:`LOWPORT` must be defined.

:macro-def:`IN_LOWPORT[Networking]`
    An integer value that specifies a lower limit of given port numbers
    for HTCondor to use on incoming connections (ports for listening),
    such that HTCondor is restricted to a range of port numbers. This
    range implies the use of both :macro:`IN_LOWPORT` and :macro:`IN_HIGHPORT`. A
    range of port numbers less than 1024 may be used for daemons running
    as root. Do not specify :macro:`IN_LOWPORT` in combination with
    :macro:`IN_HIGHPORT` such that the range crosses the port 1024 boundary.
    Applies only to Unix machine configuration. Use of :macro:`IN_LOWPORT`
    and :macro:`IN_HIGHPORT` overrides any definition of :macro:`LOWPORT` and
    :macro:`HIGHPORT`.

:macro-def:`IN_HIGHPORT[Networking]`
    An integer value that specifies an upper limit of given port numbers
    for HTCondor to use on incoming connections (ports for listening),
    such that HTCondor is restricted to a range of port numbers. This
    range implies the use of both :macro:`IN_LOWPORT` and :macro:`IN_HIGHPORT`. A
    range of port numbers less than 1024 may be used for daemons running
    as root. Do not specify :macro:`IN_LOWPORT` in combination with
    :macro:`IN_HIGHPORT` such that the range crosses the port 1024 boundary.
    Applies only to Unix machine configuration. Use of :macro:`IN_LOWPORT`
    and :macro:`IN_HIGHPORT` overrides any definition of :macro:`LOWPORT` and
    :macro:`HIGHPORT`.

:macro-def:`OUT_LOWPORT[Networking]`
    An integer value that specifies a lower limit of given port numbers
    for HTCondor to use on outgoing connections, such that HTCondor is
    restricted to a range of port numbers. This range implies the use of
    both :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT`. A range of port numbers
    less than 1024 is inappropriate, as not all daemons and tools will
    be run as root. Applies only to Unix machine configuration. Use of
    :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT` overrides any definition of
    :macro:`LOWPORT` and :macro:`HIGHPORT`.

:macro-def:`OUT_HIGHPORT[Networking]`
    An integer value that specifies an upper limit of given port numbers
    for HTCondor to use on outgoing connections, such that HTCondor is
    restricted to a range of port numbers. This range implies the use of
    both :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT`. A range of port numbers
    less than 1024 is inappropriate, as not all daemons and tools will
    be run as root. Applies only to Unix machine configuration. Use of
    :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT` overrides any definition of
    :macro:`LOWPORT` and :macro:`HIGHPORT`.

:macro-def:`UPDATE_COLLECTOR_WITH_TCP`
    This boolean value controls whether TCP or UDP is used by daemons to
    send ClassAd updates to the *condor_collector*. Please read
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more details and a discussion of when this functionality is
    needed. When using TCP in large pools, it is also necessary to
    ensure that the *condor_collector* has a large enough file
    descriptor limit using :macro:`COLLECTOR_MAX_FILE_DESCRIPTORS`.
    The default value is ``True``.

:macro-def:`UPDATE_VIEW_COLLECTOR_WITH_TCP[Networking]`
    This boolean value controls whether TCP or UDP is used by the
    *condor_collector* to forward ClassAd updates to the
    *condor_collector* daemons specified by :macro:`CONDOR_VIEW_HOST`. Please read
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more details and a discussion of when this functionality is
    needed. The default value is ``False``.

:macro-def:`TCP_UPDATE_COLLECTORS[Networking]`
    The list of *condor_collector* daemons which will be updated with
    TCP instead of UDP when :macro:`UPDATE_COLLECTOR_WITH_TCP` or
    :macro:`UPDATE_VIEW_COLLECTOR_WITH_TCP` is ``False``. Please read
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more details and a discussion of when a site needs this
    functionality.

:macro-def:`<SUBSYS>_TIMEOUT_MULTIPLIER[Networking]`
    An integer value that
    defaults to 1. This value multiplies configured timeout values for
    all targeted subsystem communications, thereby increasing the time
    until a timeout occurs. This configuration variable is intended for
    use by developers for debugging purposes, where communication
    timeouts interfere.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`NONBLOCKING_COLLECTOR_UPDATE[Networking]`
    A boolean value that defaults to ``True``. When ``True``, the
    establishment of TCP connections to the *condor_collector* daemon
    for a security-enabled pool are done in a nonblocking manner.

:macro-def:`NEGOTIATOR_USE_NONBLOCKING_STARTD_CONTACT[Networking]`
    A boolean value that defaults to ``True``. When ``True``, the
    establishment of TCP connections from the *condor_negotiator*
    daemon to the *condor_startd* daemon for a security-enabled pool
    are done in a nonblocking manner.

:macro-def:`UDP_NETWORK_FRAGMENT_SIZE[Networking]`
    An integer value that defaults to 1000 and represents the maximum
    size in bytes of an outgoing UDP packet. If the outgoing message is
    larger than ``$(UDP_NETWORK_FRAGMENT_SIZE)``, then the message will
    be split (fragmented) into multiple packets no larger than
    ``$(UDP_NETWORK_FRAGMENT_SIZE)``. If the destination of the message
    is the loopback network interface, see :macro:`UDP_LOOPBACK_FRAGMENT_SIZE`
    below. For instance, the maximum payload size of a UDP packet over
    Ethernet is typically 1472 bytes, and thus if a UDP payload exceeds
    1472 bytes the IP network stack on either hosts or forwarding
    devices (such as network routers) will have to perform message
    fragmentation on transmission and reassembly on receipt.
    Experimentation has shown that such devices are more likely to
    simply drop a UDP message under high-traffic scenarios if the
    message requires reassembly. HTCondor avoids this situation via the
    capability to perform UDP fragmentation and reassembly on its own.

:macro-def:`UDP_LOOPBACK_FRAGMENT_SIZE[Networking]`
    An integer value that defaults to 60000 and represents the maximum
    size in bytes of an outgoing UDP packet that is being sent to the
    loopback network interface (e.g. 127.0.0.1). If the outgoing message
    is larger than ``$(UDP_LOOPBACK_FRAGMENT_SIZE)``, then the message
    will be split (fragmented) into multiple packets no larger than
    ``$(UDP_LOOPBACK_FRAGMENT_SIZE)``. If the destination of the message
    is not the loopback interface, see :macro:`UDP_NETWORK_FRAGMENT_SIZE`
    above.

:macro-def:`ALWAYS_REUSEADDR[Networking]`
    A boolean value that, when ``True``, tells HTCondor to set
    ``SO_REUSEADDR`` socket option, so that the schedd can run large
    numbers of very short jobs without exhausting the number of local
    ports needed for shadows. The default value is ``True``. (Note that
    this represents a change in behavior compared to versions of
    HTCondor older than 8.6.0, which did not include this configuration
    macro. To restore the previous behavior, set this value to
    ``False``.)

:index:`Shared Filesystem Options<single: Configuration; Shared Filesystem Options>`

.. _shared_fs_config_options:

Shared Filesystem Configuration Options
---------------------------------------

These macros control how HTCondor interacts with various shared and
network file systems.For information on submitting jobs under shared
file systems, see :ref:`users-manual/submitting-a-job:Submitting Jobs Using a Shared File System`.

:macro-def:`UID_DOMAIN[Filesystem]`
    The :macro:`UID_DOMAIN` macro is used to decide under which user to run
    jobs. If the ``$(UID_DOMAIN)`` on the submitting machine is
    different than the ``$(UID_DOMAIN)`` on the machine that runs a job,
    then HTCondor runs the job as the user nobody. For example, if the
    access point has a ``$(UID_DOMAIN)`` of flippy.cs.wisc.edu, and
    the machine where the job will execute has a ``$(UID_DOMAIN)`` of
    cs.wisc.edu, the job will run as user nobody, because the two
    ``$(UID_DOMAIN)``\ s are not the same. If the ``$(UID_DOMAIN)`` is
    the same on both the submit and execute machines, then HTCondor will
    run the job as the user that submitted the job.

    A further check attempts to assure that the submitting machine can
    not lie about its :macro:`UID_DOMAIN`. HTCondor compares the submit
    machine's claimed value for :macro:`UID_DOMAIN` to its fully qualified
    name. If the two do not end the same, then the access point is
    presumed to be lying about its :macro:`UID_DOMAIN`. In this case,
    HTCondor will run the job as user nobody. For example, a job
    submission to the HTCondor pool at the UW Madison from
    flippy.example.com, claiming a :macro:`UID_DOMAIN` of cs.wisc.edu,
    will run the job as the user nobody.

    Because of this verification, ``$(UID_DOMAIN)`` must be a real
    domain name. At the Computer Sciences department at the UW Madison,
    we set the ``$(UID_DOMAIN)`` to be cs.wisc.edu to indicate that
    whenever someone submits from a department machine, we will run the
    job as the user who submits it.

    Also see :macro:`SOFT_UID_DOMAIN` below for information about one more
    check that HTCondor performs before running a job as a given user.

    A few details:

    An administrator could set :macro:`UID_DOMAIN` to \*. This will match all
    domains, but it is a gaping security hole. It is not recommended.

    An administrator can also leave :macro:`UID_DOMAIN` undefined. This will
    force HTCondor to always run jobs as user nobody.
    If vanilla jobs are run as user nobody, then files
    that need to be accessed by the job will need to be marked as world
    readable/writable so the user nobody can access them.

    When HTCondor sends e-mail about a job, HTCondor sends the e-mail to
    ``user@$(UID_DOMAIN)``. If :macro:`UID_DOMAIN` is undefined, the e-mail
    is sent to ``user@submitmachinename``.

:macro-def:`TRUST_UID_DOMAIN[Filesystem]`
    As an added security precaution when HTCondor is about to spawn a
    job, it ensures that the :macro:`UID_DOMAIN` of a given access point is
    a substring of that machine's fully-qualified host name. However, at
    some sites, there may be multiple UID spaces that do not clearly
    correspond to Internet domain names. In these cases, administrators
    may wish to use names to describe the UID domains which are not
    substrings of the host names of the machines. For this to work,
    HTCondor must not do this regular security check. If the
    :macro:`TRUST_UID_DOMAIN` setting is defined to ``True``, HTCondor will
    not perform this test, and will trust whatever :macro:`UID_DOMAIN` is
    presented by the access point when trying to spawn a job, instead
    of making sure the access point's host name matches the
    :macro:`UID_DOMAIN`. When not defined, the default is ``False``, since it
    is more secure to perform this test.

:macro-def:`TRUST_LOCAL_UID_DOMAIN[Filesystem]`
    This parameter works like :macro:`TRUST_UID_DOMAIN`, but is only applied
    when the *condor_starter* and *condor_shadow* are on the same
    machine. If this parameter is set to ``True``, then the
    *condor_shadow* 's :macro:`UID_DOMAIN` doesn't have to be a substring
    its hostname. If this parameter is set to ``False``, then
    :macro:`UID_DOMAIN` controls whether this substring requirement is
    enforced by the *condor_starter*. The default is ``True``.

:macro-def:`SOFT_UID_DOMAIN[Filesystem]`
    A boolean variable that defaults to ``False`` when not defined. When
    HTCondor is about to run a job as a particular user (instead of as
    user nobody), it verifies that the UID given for the user is in the
    password file and actually matches the given user name. However,
    under installations that do not have every user in every machine's
    password file, this check will fail and the execution attempt will
    be aborted. To cause HTCondor not to do this check, set this
    configuration variable to ``True``. HTCondor will then run the job
    under the user's UID.

:macro-def:`SLOT<N>_USER[Filesystem]`
    The name of a user for HTCondor to use instead of user nobody, as
    part of a solution that plugs a security hole whereby a lurker
    process can prey on a subsequent job run as user name nobody.
    ``<N>`` is an integer associated with slots. On non Windows platforms
    you can use :macro:`NOBODY_SLOT_USER` instead of this configuration variable.
    On Windows, :macro:`SLOT<N>_USER` will only work if the credential of the specified
    user is stored on the execute machine using :tool:`condor_store_cred`.
    See :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    for more information.

:macro-def:`NOBODY_SLOT_USER[Filesystem]`
    The name of a user for HTCondor to use instead of user nobody when
    The :macro:`SLOT<N>_USER` for this slot is not configured.  Configure
    this to the value ``$(STARTER_SLOT_NAME)`` to use the name of the slot
    as the user name. This configuration macro is ignored on Windows,
    where the Starter will automatically create a unique temporary user for each slot as needed.
    See :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    for more information.

:macro-def:`STARTER_ALLOW_RUNAS_OWNER[Filesystem]`
    A boolean expression evaluated with the job ad as the target, that
    determines whether the job may run under the job owner's account
    (``True``) or whether it will run as :macro:`SLOT<N>_USER` or nobody
    (``False``). On Unix, this defaults to ``True``. On Windows, it
    defaults to ``False``. The job ClassAd may also contain the
    attribute ``RunAsOwner`` which is logically ANDed with the
    *condor_starter* daemon's boolean value. Under Unix, if the job
    does not specify it, this attribute defaults to ``True``. Under
    Windows, the attribute defaults to ``False``. In Unix, if the
    :ad-attr:`UidDomain` of the machine and job do not match, then there is no
    possibility to run the job as the owner anyway, so, in that case,
    this setting has no effect. See
    :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    for more information.

:macro-def:`DEDICATED_EXECUTE_ACCOUNT_REGEXP[Filesystem]`
    This is a regular expression (i.e. a string matching pattern) that
    matches the account name(s) that are dedicated to running condor
    jobs on the execute machine and which will never be used for more
    than one job at a time. The default matches no account name. If you
    have configured :macro:`SLOT<N>_USER` to be a different account for each
    HTCondor slot, and no non-condor processes will ever be run by these
    accounts, then this pattern should match the names of all
    :macro:`SLOT<N>_USER` accounts. Jobs run under a dedicated execute
    account are reliably tracked by HTCondor, whereas other jobs, may
    spawn processes that HTCondor fails to detect. Therefore, a
    dedicated execution account provides more reliable tracking of CPU
    usage by the job and it also guarantees that when the job exits, no
    "lurker" processes are left behind. When the job exits, condor will
    attempt to kill all processes owned by the dedicated execution
    account. Example:

    .. code-block:: condor-config

        SLOT1_USER = cndrusr1
        SLOT2_USER = cndrusr2
        STARTER_ALLOW_RUNAS_OWNER = False
        DEDICATED_EXECUTE_ACCOUNT_REGEXP = cndrusr[0-9]+

    You can tell if the starter is in fact treating the account as a
    dedicated account, because it will print a line such as the
    following in its log file:

    .. code-block:: text

        Tracking process family by login "cndrusr1"

:macro-def:`EXECUTE_LOGIN_IS_DEDICATED[Filesystem]`
    This configuration setting is deprecated because it cannot handle
    the case where some jobs run as dedicated accounts and some do not.
    Use :macro:`DEDICATED_EXECUTE_ACCOUNT_REGEXP` instead.

    A boolean value that defaults to ``False``. When ``True``, HTCondor
    knows that all jobs are being run by dedicated execution accounts
    (whether they are running as the job owner or as nobody or as
    :macro:`SLOT<N>_USER`). Therefore, when the job exits, all processes
    running under the same account will be killed.

:macro-def:`STARTER_SETS_HOME_ENV[Filesystem]`
    A boolean value that defaults to true.  When false, the HOME
    environment variable is not generally set, though some
    container runtimes might themselves set it.  When true, 
    HTCondor will set HOME to the home directory of the user
    on the EP system.

:macro-def:`STARTER_NESTED_SCRATCH[Filesystem]`
    A boolean value that defaults to true.  When false, the job's scratch
    directory hierarchy is created in the same way as it was previous
    to HTCondor 24.9.  That is, the job's scratch directory is a 
    direct subdirectory of :macro:`EXECUTE` named *dir_<starter_pid>*,
    and owned by the user.  When true, the scratch directory is 
    a subdirectory of that directory named scratch.  There are other
    subdirectories named "user", where user-owned HTCondor files
    will go, such as credentials, the .job.ad and other metadata.
    There is also an htcondor subdirectory, where files owned by
    the HTCondor system will go.  The idea is the scratch directory
    should not be polluted with system files, and only contain files
    the job expects to be there.

:macro-def:`FILESYSTEM_DOMAIN[Filesystem]`
    An arbitrary string that is used to decide if the two machines, a
    access point and an execute machine, share a file system. Although
    this configuration variable name contains the word "DOMAIN", its
    value is not required to be a domain name. It often is a domain
    name.

    Note that this implementation is not ideal: machines may share some
    file systems but not others. HTCondor currently has no way to
    express this automatically. A job can express the need to use a
    particular file system where machines advertise an additional
    ClassAd attribute and the job requires machines with the attribute,
    as described on the question within the
    `https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToAdminRecipes <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToAdminRecipes>`_
    page for how to run jobs on a subset of machines that have required
    software installed.

    Note that if you do not set ``$(FILESYSTEM_DOMAIN)``, the value
    defaults to the fully qualified host name of the local machine.
    Since each machine will have a different ``$(FILESYSTEM_DOMAIN)``,
    they will not be considered to have shared file systems.

:macro-def:`USE_NFS[Filesystem]`
    This configuration variable changes the semantics of Chirp
    file I/O when running in the vanilla, java or parallel universe. If
    this variable is set in those universes, Chirp will not send I/O
    requests over the network as requested, but perform them directly to
    the locally mounted file system.

:macro-def:`IGNORE_NFS_LOCK_ERRORS[Filesystem]`
    When set to ``True``, all errors related to file locking errors from
    NFS are ignored. Defaults to ``False``, not ignoring errors.
