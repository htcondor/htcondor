Configuration Macros
====================

:index:`configuration-macros<single: configuration-macros; HTCondor>`
:index:`configuration: macros`

The section contains a list of the individual configuration macros for
HTCondor. Before attempting to set up HTCondor configuration, you should
probably read the :doc:`/admin-manual/introduction-to-configuration` section
and possibly the 
:ref:`admin-manual/introduction-to-configuration:configuration templates`
section.

The settings that control the policy under which HTCondor will start,
suspend, resume, vacate or kill jobs are described in
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`,
not in this section.

HTCondor-wide Configuration File Entries
----------------------------------------

:index:`HTCondor-wide configuration variables<single: HTCondor-wide configuration variables; configuration>`

This section describes settings which affect all parts of the HTCondor
system. Other system-wide settings can be found in
:ref:`admin-manual/configuration-macros:network-related configuration file entries`
and :ref:`admin-manual/configuration-macros:shared file system configuration file macros`.

:macro-def:`SUBSYSTEM[Global]`
    Various configuration macros described below may include :macro:`<SUBSYS>` in the macro name.
    This allows for one general macro name to apply to specific subsystems via a common
    pattern. Just replace the :macro:`<SUBSYS>` part of the given macro with a valid HTCondor
    subsystem name to apply that macro. Note that some configuration macros with :macro:`<SUBSYS>`
    only work for select subsystems. List of HTCondor Subsystems:

    .. include:: subsystems.rst

:macro-def:`CONDOR_HOST[Global]`
    This macro is used to define the ``$(COLLECTOR_HOST)`` macro.
    Normally the *condor_collector* and *condor_negotiator* would run
    on the same machine. If for some reason they were not run on the
    same machine, ``$(CONDOR_HOST)`` would not be needed. Some of the
    host-based security macros use ``$(CONDOR_HOST)`` by default. See the
    :ref:`admin-manual/security:host-based security in htcondor` section on
    Setting up IP/host-based security in HTCondor for details.

:macro-def:`COLLECTOR_HOST[Global]`
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

:macro-def:`COLLECTOR_PORT[Global]`
    The default port used when contacting the *condor_collector* and
    the default port the *condor_collector* listens on if no port is
    specified. This variable is referenced if no port is given and there
    is no other means to find the *condor_collector* port. The default
    value is 9618.

:macro-def:`NEGOTIATOR_HOST[Global]`
    This configuration variable is no longer used. It previously defined
    the host name of the machine where the *condor_negotiator* is
    running. At present, the port where the *condor_negotiator* is
    listening is dynamically allocated.

:macro-def:`CONDOR_VIEW_HOST[Global]`
    A list of HTCondorView servers, separated by commas and/or spaces.
    Each HTCondorView server is denoted by the host name of the machine
    it is running on, optionally appended by a colon and the port
    number. This service is optional, and requires additional
    configuration to enable it. There is no default value for
    :macro:`CONDOR_VIEW_HOST`. If :macro:`CONDOR_VIEW_HOST` is not defined, no
    HTCondorView server is used. See
    :ref:`admin-manual/cm-configuration:configuring the
    htcondorview server` for more details.

:macro-def:`SCHEDD_HOST[Global]`
    The host name of the machine where the *condor_schedd* is running
    for your pool. This is the host that queues submitted jobs. If the
    host specifies :macro:`SCHEDD_NAME` or :macro:`MASTER_NAME`, that
    name must be included in the form name@hostname. In most condor
    installations, there is a *condor_schedd* running on each host from
    which jobs are submitted. The default value of :macro:`SCHEDD_HOST`
    is the current host with the optional name included. For most pools,
    this macro is not defined, nor does it need to be defined.

:macro-def:`RELEASE_DIR[Global]`
    The full path to the HTCondor release directory, which holds the
    ``bin``, ``etc``, ``lib``, and ``sbin`` directories. Other macros
    are defined relative to this one. There is no default value for
    :macro:`RELEASE_DIR`.

:macro-def:`BIN[Global]`
    This directory points to the HTCondor directory where user-level
    programs are installed. The default value is ``$(RELEASE_DIR)``/bin.

:macro-def:`LIB[Global]`
    This directory points to the HTCondor directory containing its
    libraries.  On Windows, libraries are located in :macro:`BIN`.

:macro-def:`LIBEXEC[Global]`
    This directory points to the HTCondor directory where support
    commands that HTCondor needs will be placed. Do not add this
    directory to a user or system-wide path.

:macro-def:`INCLUDE[Global]`
    This directory points to the HTCondor directory where header files
    reside. The default value is ``$(RELEASE_DIR)``/include. It can make
    inclusion of necessary header files for compilation of programs
    (such as those programs that use ``libcondorapi.a``) easier through
    the use of :tool:`condor_config_val`.

:macro-def:`SBIN[Global]`
    This directory points to the HTCondor directory where HTCondor's
    system binaries (such as the binaries for the HTCondor daemons) and
    administrative tools are installed. Whatever directory ``$(SBIN)``
    points to ought to be in the ``PATH`` of users acting as HTCondor
    administrators. The default value is ``$(BIN)`` in Windows and
    ``$(RELEASE_DIR)``/sbin on all other platforms.

:macro-def:`LOCAL_DIR[Global]`
    The location of the local HTCondor directory on each machine in your
    pool. The default value is ``$(RELEASE_DIR)`` on Windows and
    ``$(RELEASE_DIR)``/hosts/``$(HOSTNAME)`` on all other platforms.

    Another possibility is to use the condor user's home directory,
    which may be specified with ``$(TILDE)``. For example:

    .. code-block:: condor-config

        LOCAL_DIR = $(tilde)

:macro-def:`LOG[Global]`
    Used to specify the directory where each HTCondor daemon writes its
    log files. The names of the log files themselves are defined with
    other macros, which use the ``$(LOG)`` macro by default. The log
    directory also acts as the current working directory of the HTCondor
    daemons as the run, so if one of them should produce a core file for
    any reason, it would be placed in the directory defined by this
    macro. The default value is ``$(LOCAL_DIR)``/log.

    Do not stage other files in this directory; any files not created by
    HTCondor in this directory are subject to removal.

:macro-def:`RUN[Global]`
    A path and directory name to be used by the HTCondor init script to
    specify the directory where the :tool:`condor_master` should write its
    process ID (PID) file. The default if not defined is ``$(LOG)``.

:macro-def:`SPOOL[Global]`
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

:macro-def:`EXECUTE[Global]`
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

:macro-def:`TMP_DIR[Global]`
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

:macro-def:`TEMP_DIR[Global]`
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

:macro-def:`SLOT<N>_EXECUTE[Global]`
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

:macro-def:`LOCAL_CONFIG_FILE[Global]`
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

:macro-def:`REQUIRE_LOCAL_CONFIG_FILE[Global]`
    A boolean value that defaults to ``True``. When ``True``, HTCondor
    exits with an error, if any file listed in :macro:`LOCAL_CONFIG_FILE`
    cannot be read. A value of ``False`` allows local configuration
    files to be missing. This is most useful for sites that have both
    large numbers of machines in the pool and a local configuration file
    that uses the ``$(HOSTNAME)`` macro in its definition. Instead of
    having an empty file for every host in the pool, files can simply be
    omitted.

:macro-def:`LOCAL_CONFIG_DIR[Global]`
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

:macro-def:`USER_CONFIG_FILE[Global]`
    The file name of a configuration file to be parsed after other local
    configuration files and before environment variables set
    configuration. Relevant only if HTCondor daemons are not run as root
    on Unix platforms or Local System on Windows platforms. The default
    is ``$(HOME)/.condor/user_config`` on Unix platforms. The default is
    %USERPROFILE\\.condor\\user_config on Windows platforms. If a fully
    qualified path is given, that is used. If a fully qualified path is
    not given, then the Unix path ``$(HOME)/.condor/`` prefixes the file
    name given on Unix platforms, or the Windows path
    %USERPROFILE\\.condor\\ prefixes the file name given on Windows
    platforms.

    The ability of a user to use this user-specified configuration file
    can be disabled by setting this variable to the empty string:

    .. code-block:: condor-config

        USER_CONFIG_FILE =

:macro-def:`LOCAL_CONFIG_DIR_EXCLUDE_REGEXP[Global]`
    A regular expression that specifies file names to be ignored when
    looking for configuration files within the directories specified via
    :macro:`LOCAL_CONFIG_DIR`. The default expression ignores files with
    names beginning with a '.' or a '#', as well as files with names
    ending in '˜'. This avoids accidents that can be caused by treating
    temporary files created by text editors as configuration files.

:macro-def:`CONDOR_IDS[Global]`
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

:macro-def:`CONDOR_ADMIN[Global]`
    The email address that HTCondor will send mail to if something goes
    wrong in the pool. For example, if a daemon crashes, the
    :tool:`condor_master` can send an obituary to this address with the last
    few lines of that daemon's log file and a brief message that
    describes what signal or exit status that daemon exited with. The
    default value is root@\ ``$(FULL_HOSTNAME)``.

:macro-def:`<SUBSYS>_ADMIN_EMAIL[Global]`
    The email address that HTCondor
    will send mail to if something goes wrong with the named
    :macro:`<SUBSYS>`. Identical to :macro:`CONDOR_ADMIN`, but done on a per
    subsystem basis. There is no default value.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`CONDOR_SUPPORT_EMAIL[Global]`
    The email address to be included at the bottom of all email HTCondor
    sends out under the label "Email address of the local HTCondor
    administrator:". This is the address where HTCondor users at your
    site should send their questions about HTCondor and get technical
    support. If this setting is not defined, HTCondor will use the
    address specified in :macro:`CONDOR_ADMIN` (described above).

:macro-def:`EMAIL_SIGNATURE[Global]`
    Every e-mail sent by HTCondor includes a short signature line
    appended to the body. By default, this signature includes the URL to
    the global HTCondor project website. When set, this variable defines
    an alternative signature line to be used instead of the default.
    Note that the value can only be one line in length. This variable
    could be used to direct users to look at local web site with
    information specific to the installation of HTCondor.

:macro-def:`MAIL[Global]`
    The full path to a mail sending program that uses **-s** to specify
    a subject for the message. On all platforms, the default shipped
    with HTCondor should work. Only if you installed things in a
    non-standard location on your system would you need to change this
    setting. The default value is ``$(BIN)``/condor_mail.exe on Windows
    and ``/usr/bin/mail`` on all other platforms. The *condor_schedd*
    will not function unless :macro:`MAIL` is defined. For security reasons,
    non-Windows platforms should not use this setting and should use
    :macro:`SENDMAIL` instead.

:macro-def:`SENDMAIL[Global]`
    The full path to the *sendmail* executable. If defined, which it is
    by default on non-Windows platforms, *sendmail* is used instead of
    the mail program defined by :macro:`MAIL`.

:macro-def:`MAIL_FROM[Global]`
    The e-mail address that notification e-mails appear to come from.
    Contents is that of the ``From`` header. There is no default value;
    if undefined, the ``From`` header may be nonsensical.

:macro-def:`SMTP_SERVER[Global]`
    For Windows platforms only, the host name of the server through
    which to route notification e-mail. There is no default value; if
    undefined and the debug level is at ``FULLDEBUG``, an error message
    will be generated.

:macro-def:`RESERVED_SWAP[Global]`
    The amount of swap space in MiB to reserve for this machine.
    HTCondor will not start up more *condor_shadow* processes if the
    amount of free swap space on this machine falls below this level.
    The default value is 0, which disables this check. It is anticipated
    that this configuration variable will no longer be used in the near
    future. If :macro:`RESERVED_SWAP` is not set to 0, the value of
    :macro:`SHADOW_SIZE_ESTIMATE` is used.

:macro-def:`DISK[Global]`
    Tells HTCondor how much disk space (in kB) to advertise as being available
    for use by jobs. If :macro:`DISK` is not specified, HTCondor will advertise the
    amount of free space on your execute partition, minus :macro:`RESERVED_DISK`.

:macro-def:`RESERVED_DISK[Global]`
    Determines how much disk space (in MB) you want to reserve for your own
    machine. When HTCondor is reporting the amount of free disk space in
    a given partition on your machine, it will always subtract this
    amount. An example is the *condor_startd*, which advertises the
    amount of free space in the ``$(EXECUTE)`` directory. The default
    value of :macro:`RESERVED_DISK` is zero.

:macro-def:`LOCK[Global]`
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

:macro-def:`HISTORY[Global]`
    Defines the location of the HTCondor history file, which stores
    information about all HTCondor jobs that have completed on a given
    machine. This macro is used by both the *condor_schedd* which
    appends the information and :tool:`condor_history`, the user-level
    program used to view the history file. This configuration macro is
    given the default value of ``$(SPOOL)/history`` in the default
    configuration. If not defined, no history file is kept.

:macro-def:`ENABLE_HISTORY_ROTATION[Global]`
    If this is defined to be true, then the history file will be
    rotated. If it is false, then it will not be rotated, and it will
    grow indefinitely, to the limits allowed by the operating system. If
    this is not defined, it is assumed to be true. The rotated files
    will be stored in the same directory as the history file.

:macro-def:`MAX_HISTORY_LOG[Global]`
    Defines the maximum size for the history file, in bytes. It defaults
    to 20MB. This parameter is only used if history file rotation is
    enabled.

:macro-def:`MAX_HISTORY_ROTATIONS[Global]`
    When history file rotation is turned on, this controls how many
    backup files there are. It default to 2, which means that there may
    be up to three history files (two backups, plus the history file
    that is being currently written to). When the history file is
    rotated, and this rotation would cause the number of backups to be
    too large, the oldest file is removed.

:macro-def:`HISTORY_CONTAINS_JOB_ENVIRONMENT[Global]`
    This parameter defaults to true.  When set to false, the job's
    environment attribute (which can be very large) is not written to
    the history file.  This may allow many more jobs to be kept in the
    history before rotation.

:macro-def:`HISTORY_HELPER_MAX_CONCURRENCY[Global]`
    Specifies the maximum number of concurrent remote :tool:`condor_history`
    queries allowed at a time; defaults to 50. When this maximum is
    exceeded, further queries will be queued in a non-blocking manner.
    Setting this option to 0 disables remote history access. A remote
    history access is defined as an invocation of :tool:`condor_history` that
    specifies a **-name** option to query a *condor_schedd* running on
    a remote machine.

:macro-def:`HISTORY_HELPER_MAX_HISTORY[Global]`
    Specifies the maximum number of ClassAds to parse on behalf of
    remote history clients. The default is 10,000. This allows the
    system administrator to indirectly manage the maximum amount of CPU
    time spent on each client. Setting this option to 0 disables remote
    history access.

:macro-def:`MAX_JOB_QUEUE_LOG_ROTATIONS[Global]`
    The *condor_schedd* daemon periodically rotates the job queue
    database file, in order to save disk space. This option controls how
    many rotated files are saved. It defaults to 1, which means there
    may be up to two history files (the previous one, which was rotated
    out of use, and the current one that is being written to). When the
    job queue file is rotated, and this rotation would cause the number
    of backups to be larger than the maximum specified, the oldest file
    is removed.

:macro-def:`CLASSAD_LOG_STRICT_PARSING[Global]`
    A boolean value that defaults to ``True``. When ``True``, ClassAd
    log files will be read using a strict syntax checking for ClassAd
    expressions. ClassAd log files include the job queue log and the
    accountant log. When ``False``, ClassAd log files are read without
    strict expression syntax checking, which allows some legacy ClassAd
    log data to be read in a backward compatible manner. This
    configuration variable may no longer be supported in future
    releases, eventually requiring all ClassAd log files to pass strict
    ClassAd syntax checking.

:macro-def:`DEFAULT_DOMAIN_NAME[Global]`
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

:macro-def:`NO_DNS[Global]`
    A boolean value that defaults to ``False``. When ``True``, HTCondor
    constructs host names using the host's IP address together with the
    value defined for :macro:`DEFAULT_DOMAIN_NAME`.

:macro-def:`CM_IP_ADDR[Global]`
    If neither :macro:`COLLECTOR_HOST` nor ``COLLECTOR_IP_ADDR`` macros are
    defined, then this macro will be used to determine the IP address of
    the central manager (collector daemon). This macro is defined by an
    IP address.

:macro-def:`EMAIL_DOMAIN[Global]`
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

:macro-def:`CREATE_CORE_FILES[Global]`
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

:macro-def:`ABORT_ON_EXCEPTION[Global]`
    When HTCondor programs detect a fatal internal exception, they
    normally log an error message and exit. If you have turned on
    :macro:`CREATE_CORE_FILES`, in some cases you may also want to
    turn on :macro:`ABORT_ON_EXCEPTION` so that core files are generated
    when an exception occurs. Set the following to True if that is what
    you want.

:macro-def:`Q_QUERY_TIMEOUT[Global]`
    Defines the timeout (in seconds) that :tool:`condor_q` uses when trying
    to connect to the *condor_schedd*. Defaults to 20 seconds.

:macro-def:`DEAD_COLLECTOR_MAX_AVOIDANCE_TIME[Global]`
    Defines the interval of time (in seconds) between checks for a
    failed primary *condor_collector* daemon. If connections to the
    dead primary *condor_collector* take very little time to fail, new
    attempts to query the primary *condor_collector* may be more
    frequent than the specified maximum avoidance time. The default
    value equals one hour. This variable has relevance to flocked jobs,
    as it defines the maximum time they may be reporting to the primary
    *condor_collector* without the *condor_negotiator* noticing.

:macro-def:`PASSWD_CACHE_REFRESH[Global]`
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

:macro-def:`SYSAPI_GET_LOADAVG[Global]`
    If set to False, then HTCondor will not attempt to compute the load
    average on the system, and instead will always report the system
    load average to be 0.0. Defaults to True.

:macro-def:`NETWORK_MAX_PENDING_CONNECTS[Global]`
    This specifies a limit to the maximum number of simultaneous network
    connection attempts. This is primarily relevant to *condor_schedd*,
    which may try to connect to large numbers of startds when claiming
    them. The negotiator may also connect to large numbers of startds
    when initiating security sessions used for sending MATCH messages.
    On Unix, the default for this parameter is eighty percent of the
    process file descriptor limit. On windows, the default is 1600.

:macro-def:`WANT_UDP_COMMAND_SOCKET[Global]`
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

:macro-def:`ALLOW_SCRIPTS_TO_RUN_AS_EXECUTABLES[Global]`
    A boolean value that, when ``True``, permits scripts on Windows
    platforms to be used in place of the
    :subcom:`executable[and Windows scripts]` in a job
    submit description file, in place of a :tool:`condor_dagman` pre or post
    script, or in producing the configuration, for example. Allows a
    script to be used in any circumstance previously limited to a
    Windows executable or a batch file. The default value is ``True``.
    See :ref:`platform-specific/microsoft-windows:using windows scripts as job executables`
    for further description.

:macro-def:`OPEN_VERB_FOR_<EXT>_FILES[Global]`
    A string that defines a Windows verb for use in a root hive registry
    look up. <EXT> defines the file name extension, which represents a
    scripting language, also needed for the look up. See
    :ref:`platform-specific/microsoft-windows:using windows scripts as job executables`
    for a more complete description.

:macro-def:`ENABLE_CLASSAD_CACHING[Global]`
    A boolean value that controls the caching of ClassAds. Caching saves
    memory when an HTCondor process contains many ClassAds with the same
    expressions. The default value is ``True`` for all daemons other
    than the *condor_shadow*, *condor_starter*, and :tool:`condor_master`.
    A value of ``True`` enables caching.

:macro-def:`STRICT_CLASSAD_EVALUATION[Global]`
    A boolean value that controls how ClassAd expressions are evaluated.
    If set to ``True``, then New ClassAd evaluation semantics are used.
    This means that attribute references without a ``MY.`` or
    ``TARGET.`` prefix are only looked up in the local ClassAd. If set
    to the default value of ``False``, Old ClassAd evaluation semantics
    are used. See
    :ref:`classads/classad-mechanism:classads: old and new`
    for details.

:macro-def:`CLASSAD_USER_LIBS[Global]`
    A comma separated list of paths to shared libraries that contain
    additional ClassAd functions to be used during ClassAd evaluation.

:macro-def:`CLASSAD_USER_PYTHON_MODULES[Global]`
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

:macro-def:`CLASSAD_USER_PYTHON_LIB[Global]`
    Specifies the path to the python libraries, which is needed when
    :macro:`CLASSAD_USER_PYTHON_MODULES` is set. Defaults to
    ``$(LIBEXEC)/libclassad_python_user.so``, and would rarely be
    changed from the default value.

:macro-def:`CONDOR_FSYNC[Global]`
    A boolean value that controls whether HTCondor calls fsync() when
    writing the user job and transaction logs. Setting this value to
    ``False`` will disable calls to fsync(), which can help performance
    for *condor_schedd* log writes at the cost of some durability of
    the log contents, should there be a power or hardware failure. The
    default value is ``True``.

:macro-def:`STATISTICS_TO_PUBLISH[Global]`
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

:macro-def:`STATISTICS_TO_PUBLISH_LIST[Global]`
    A comma and/or space separated list of statistics attribute names
    that should be published in updates to the *condor_collector*
    daemon, even though the verbosity specified in
    :macro:`STATISTICS_TO_PUBLISH` would not normally send them. This setting
    has the effect of redefining the verbosity level of the statistics
    attributes that it mentions, so that they will always match the
    current statistics publication level as specified in
    :macro:`STATISTICS_TO_PUBLISH`.

:macro-def:`STATISTICS_WINDOW_SECONDS[Global]`
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

:macro-def:`STATISTICS_WINDOW_SECONDS_<collection>[Global]`
    The same as :macro:`STATISTICS_WINDOW_SECONDS`, but used to override the
    global setting for a particular statistic collection. Collection
    names currently implemented are ``DC`` or ``DAEMONCORE`` and
    ``SCHEDD`` or ``SCHEDULER``.

:macro-def:`STATISTICS_WINDOW_QUANTUM[Global]`
    For experts only, an integer value that controls the time
    quantization that form a time window, in seconds, for the data
    structures that maintain windowed statistics. Defaults to 240
    seconds, which is 6 minutes. This default is purposely set to be
    slightly smaller than the update rate to the *condor_collector*.
    Setting a smaller value than the default increases the memory
    requirement for the statistics. Graphing of statistics at the level
    of the quantum expects to see counts that appear like a saw tooth.

:macro-def:`STATISTICS_WINDOW_QUANTUM_<collection>[Global]`
    The same as :macro:`STATISTICS_WINDOW_QUANTUM`, but used to override the
    global setting for a particular statistic collection. Collection
    names currently implemented are ``DC`` or ``DAEMONCORE`` and
    ``SCHEDD`` or ``SCHEDULER``.

:macro-def:`TCP_KEEPALIVE_INTERVAL[Global]`
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

:macro-def:`ENABLE_IPV4[Global]`
    A boolean with the additional special value of ``auto``. If true,
    HTCondor will use IPv4 if available, and fail otherwise. If false,
    HTCondor will not use IPv4. If ``auto``, which is the default,
    HTCondor will use IPv4 if it can find an interface with an IPv4
    address, and that address is (a) public or private, or (b) no
    interface's IPv6 address is public or private. If HTCondor finds
    more than one address of each protocol, only the most public address
    is considered for that protocol.

:macro-def:`ENABLE_IPV6[Global]`
    A boolean with the additional special value of ``auto``. If true,
    HTCondor will use IPv6 if available, and fail otherwise. If false,
    HTCondor will not use IPv6. If ``auto``, which is the default,
    HTCondor will use IPv6 if it can find an interface with an IPv6
    address, and that address is (a) public or private, or (b) no
    interface's IPv4 address is public or private. If HTCondor finds
    more than one address of each protocol, only the most public address
    is considered for that protocol.

:macro-def:`PREFER_IPV4[Global]`
    A boolean which will cause HTCondor to prefer IPv4 when it is able
    to choose. HTCondor will otherwise prefer IPv6. The default is
    ``True``.

:macro-def:`ADVERTISE_IPV4_FIRST[Global]`
    A string (treated as a boolean). If :macro:`ADVERTISE_IPV4_FIRST`
    evaluates to ``True``, HTCondor will advertise its IPv4 addresses
    before its IPv6 addresses; otherwise the IPv6 addresses will come
    first. Defaults to ``$(PREFER_IPV4)``.

:macro-def:`IGNORE_TARGET_PROTOCOL_PREFERENCE[Global]`
    A string (treated as a boolean). If
    :macro:`IGNORE_TARGET_PROTOCOL_PREFERENCE` evaluates to ``True``, the
    target's listed protocol preferences will be ignored; otherwise
    they will not. Defaults to ``$(PREFER_IPV4)``.

:macro-def:`IGNORE_DNS_PROTOCOL_PREFERENCE[Global]`
    A string (treated as a boolean). :macro:`IGNORE_DNS_PROTOCOL_PREFERENCE`
    evaluates to ``True``, the protocol order returned by the DNS will
    be ignored; otherwise it will not. Defaults to ``$(PREFER_IPV4)``.

:macro-def:`PREFER_OUTBOUND_IPV4[Global]`
    A string (treated as a boolean). :macro:`PREFER_OUTBOUND_IPV4` evaluates
    to ``True``, HTCondor will prefer IPv4; otherwise it will not.
    Defaults to ``$(PREFER_IPV4)``.

:macro-def:`<SUBSYS>_CLASSAD_USER_MAP_NAMES[Global]`
    A string defining a list of names for username-to-accounting group
    mappings for the specified daemon. Names must be separated by spaces
    or commas.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`CLASSAD_USER_MAPFILE_<name>[Global]`
    A string giving the name of a file to parse to initialize the map
    for the given username. Note that this macro is only used if
    :macro:`<SUBSYS>_CLASSAD_USER_MAP_NAMES` is defined for the relevant
    daemon.

    The format for the map file is the same as the format for
    :macro:`CLASSAD_USER_MAPDATA_<name>`, below.

:macro-def:`CLASSAD_USER_MAPDATA_<name>[Global]`
    A string containing data to be used to initialize the map for the
    given username. Note that this macro is only used if
    :macro:`<SUBSYS>_CLASSAD_USER_MAP_NAMES` is defined for the relevant
    daemon, and :macro:`CLASSAD_USER_MAPFILE_<name>` is not defined
    for the given name.

    The format for the map data is the same as the format
    for the security unified map file (see
    :ref:`admin-manual/security:the unified map file for authentication`
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

:macro-def:`SIGN_S3_URLS[Global]`
    A boolean value that, when ``True``, tells HTCondor to convert ``s3://``
    URLs into pre-signed ``https://`` URLs.  This allows execute nodes to
    download from or upload to secure S3 buckets without access to the user's
    API tokens, which remain on the submit node at all times.  This value
    defaults to TRUE but can be disabled if the administrator has already
    provided an ``s3://`` plug-in.  This value must be set on both the submit
    node and on the execute node.

Daemon Logging Configuration File Entries
-----------------------------------------

:index:`daemon logging configuration variables<single: daemon logging configuration variables; configuration>`

These entries control how and where the HTCondor daemons write to log
files. Many of the entries in this section represents multiple macros.
There is one for each subsystem (listed in :macro:`SUBSYSTEM`).
The macro name for each substitutes :macro:`<SUBSYS>` with the name of the
subsystem corresponding to the daemon.

:macro-def:`<SUBSYS>_LOG[Global]`
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

:macro-def:`LOG_TO_SYSLOG[Global]`
    A boolean value that is ``False`` by default. When ``True``, all
    daemon logs are routed to the syslog facility on Linux.

:macro-def:`MAX_<SUBSYS>_LOG[Global]`
    Controls the maximum size in bytes or amount of time that a log will
    be allowed to grow. For any log not specified, the default is
    ``$(MAX_DEFAULT_LOG)``\ :index:`MAX_DEFAULT_LOG`, which
    currently defaults to 10 MiB in size. Values are specified with the
    same syntax as :macro:`MAX_DEFAULT_LOG`.

    Note that a log file for the :tool:`condor_procd` does not use this
    configuration variable definition. Its implementation is separate.
    See :macro:`MAX_PROCD_LOG`.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MAX_DEFAULT_LOG[Global]`
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

:macro-def:`MAX_NUM_<SUBSYS>_LOG[Global]`
    An integer that controls the maximum number of rotations a log file
    is allowed to perform before the oldest one will be rotated away.
    Thus, at most ``MAX_NUM_<SUBSYS>_LOG + 1`` log files of the same
    program coexist at a given time. The default value is 1.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`TRUNC_<SUBSYS>_LOG_ON_OPEN[Global]`
    If this macro is defined and set to ``True``, the affected log will
    be truncated and started from an empty file with each invocation of
    the program. Otherwise, new invocations of the program will append
    to the previous log file. By default this setting is ``False`` for
    all daemons.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_LOG_KEEP_OPEN[Global]`
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

:macro-def:`<SUBSYS>_LOCK[Global]`
    This macro specifies the lock file used
    to synchronize append operations to the log file for this subsystem.
    It must be a separate file from the ``$(<SUBSYS>_LOG)`` file, since
    the ``$(<SUBSYS>_LOG)`` file may be rotated and you want to be able
    to synchronize access across log file rotations. A lock file is only
    required for log files which are accessed by more than one process.
    Currently, this includes only the :macro:`SHADOW` subsystem. This macro
    is defined relative to the ``$(LOCK)`` macro.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`JOB_QUEUE_LOG[Global]`
    A full path and file name, specifying the job queue log. The default
    value, when not defined is ``$(SPOOL)``/job_queue.log. This
    specification can be useful, if there is a solid state drive which
    is big enough to hold the frequently written to ``job_queue.log``,
    but not big enough to hold the whole contents of the spool
    directory.

:macro-def:`FILE_LOCK_VIA_MUTEX[Global]`
    This macro setting only works on Win32 - it is ignored on Unix. If
    set to be ``True``, then log locking is implemented via a kernel
    mutex instead of via file locking. On Win32, mutex access is FIFO,
    while obtaining a file lock is non-deterministic. Thus setting to
    ``True`` fixes problems on Win32 where processes (usually shadows)
    could starve waiting for a lock on a log file. Defaults to ``True``
    on Win32, and is always ``False`` on Unix.

:macro-def:`LOCK_DEBUG_LOG_TO_APPEND[Global]`
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

:macro-def:`ENABLE_USERLOG_LOCKING[Global]`
    A boolean value that defaults to ``False`` on Unix platforms and
    ``True`` on Windows platforms. When ``True``, a user's job event log
    will be locked before being written to. If ``False``, HTCondor will
    not lock the file before writing.

:macro-def:`ENABLE_USERLOG_FSYNC[Global]`
    A boolean value that is ``True`` by default. When ``True``, writes
    to the user's job event log are sync-ed to disk before releasing the
    lock.

:macro-def:`USERLOG_FILE_CACHE_MAX[Global]`
    The integer number of job event log files that the *condor_schedd*
    will keep open for writing during an interval of time (specified by
    :macro:`USERLOG_FILE_CACHE_CLEAR_INTERVAL`). The default value is 0,
    causing no files to remain open; when 0, each job event log is
    opened, the event is written, and then the file is closed.
    Individual file descriptors are removed from this count when the
    *condor_schedd* detects that no jobs are currently using them.
    Opening a file is a relatively time consuming operation on a
    networked file system (NFS), and therefore, allowing a set of files
    to remain open can improve performance. The value of this variable
    needs to be set low enough such that the *condor_schedd* daemon
    process does not run out of file descriptors by leaving these job
    event log files open. The Linux operating system defaults to
    permitting 1024 assigned file descriptors per process; the
    *condor_schedd* will have one file descriptor per running job for
    the *condor_shadow*.

:macro-def:`USERLOG_FILE_CACHE_CLEAR_INTERVAL[Global]`
    The integer number of seconds that forms the time interval within
    which job event logs will be permitted to remain open when
    :macro:`USERLOG_FILE_CACHE_MAX` is greater than zero. The default is 60
    seconds. When the interval has passed, all job event logs that the
    *condor_schedd* has permitted to stay open will be closed, and the
    interval within which job event logs may remain open between writes
    of events begins anew. This time interval may be set to a longer
    duration if the administrator determines that the *condor_schedd*
    will not exceed the maximum number of file descriptors; a longer
    interval may yield higher performance due to fewer files being
    opened and closed.

:macro-def:`CREATE_LOCKS_ON_LOCAL_DISK[Global]`
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

:macro-def:`TOUCH_LOG_INTERVAL[Global]`
    The time interval in seconds between when daemons touch their log
    files. The change in last modification time for the log file is
    useful when a daemon restarts after failure or shut down. The last
    modification date is printed, and it provides an upper bound on the
    length of time that the daemon was not running. Defaults to 60
    seconds.

:macro-def:`LOGS_USE_TIMESTAMP[Global]`
    This macro controls how the current time is formatted at the start
    of each line in the daemon log files. When ``True``, the Unix time
    is printed (number of seconds since 00:00:00 UTC, January 1, 1970).
    When ``False`` (the default value), the time is printed like so:
    ``<Month>/<Day> <Hour>:<Minute>:<Second>`` in the local timezone.

:macro-def:`DEBUG_TIME_FORMAT[Global]`
    This string defines how to format the current time printed at the
    start of each line in the daemon log files. The value is a format
    string is passed to the C strftime() function, so see that manual
    page for platform-specific details. If not defined, the default
    value is

    .. code-block:: text

           "%m/%d/%y %H:%M:%S"

:macro-def:`<SUBSYS>_DEBUG[Global]`
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

:macro-def:`ALL_DEBUG[Global]`
    Used to make all subsystems share a debug flag. Set the parameter
    :macro:`ALL_DEBUG` instead of changing all of the individual parameters.
    For example, to turn on all debugging in all subsystems, set
    ALL_DEBUG = D_ALL.

:macro-def:`TOOL_DEBUG[Global]`
    Uses the same values (debugging levels) as :macro:`<SUBSYS>_DEBUG` to
    describe the amount of debugging information sent to ``stderr`` for
    HTCondor tools.

Log files may optionally be specified per debug level as follows:

:macro-def:`<SUBSYS>_<LEVEL>_LOG[Global]`
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

:macro-def:`MAX_<SUBSYS>_<LEVEL>_LOG[Global]`
    See :macro:`MAX_<SUBSYS>_LOG`.

:macro-def:`TRUNC_<SUBSYS>_<LEVEL>_LOG_ON_OPEN[Global]`
    See :macro:`TRUNC_<SUBSYS>_LOG_ON_OPEN`.

The following macros control where and what is written to the event log,
a file that receives job events, but across all users and user's jobs.

:macro-def:`EVENT_LOG[Global]`
    The full path and file name of the event log. There is no default
    value for this variable, so no event log will be written, if not
    defined.

:macro-def:`EVENT_LOG_MAX_SIZE[Global]`
    Controls the maximum length in bytes to which the event log will be
    allowed to grow. The log file will grow to the specified length,
    then be saved to a file with the suffix .old. The .old files are
    overwritten each time the log is saved. A value of 0 specifies that
    the file may grow without bounds (and disables rotation). The
    default is 1 MiB. For backwards compatibility, :macro:`MAX_EVENT_LOG`
    will be used if :macro:`EVENT_LOG_MAX_SIZE` is not defined. If
    :macro:`EVENT_LOG` is not defined, this parameter has no effect.

:macro-def:`MAX_EVENT_LOG[Global]`
    See :macro:`EVENT_LOG_MAX_SIZE`.

:macro-def:`EVENT_LOG_MAX_ROTATIONS[Global]`
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

:macro-def:`EVENT_LOG_ROTATION_LOCK[Global]`
    Specifies the lock file that will be used to ensure that, when
    rotating files, the rotation is done by a single process. This is a
    string parameter; its default value is ``$(LOCK)/EventLogLock``. If
    an empty value is set, then the file that is used is the file path
    of the event log itself, with the string ``.lock`` appended. If
    :macro:`EVENT_LOG` is not defined, or if :macro:`EVENT_LOG_MAX_SIZE` has a
    value of 0 (which disables event log rotation), this configuration
    variable has no effect.

:macro-def:`EVENT_LOG_FSYNC[Global]`
    A boolean value that controls whether HTCondor will perform an
    fsync() after writing each event to the event log. When ``True``, an
    fsync() operation is performed after each event. This fsync()
    operation forces the operating system to synchronize the updates to
    the event log to the disk, but can negatively affect the performance
    of the system. Defaults to ``False``.

:macro-def:`EVENT_LOG_LOCKING[Global]`
    A boolean value that defaults to ``False`` on Unix platforms and
    ``True`` on Windows platforms. When ``True``, the event log (as
    specified by :macro:`EVENT_LOG`) will be locked before being written to.
    When ``False``, HTCondor does not lock the file before writing.

:macro-def:`EVENT_LOG_COUNT_EVENTS[Global]`
    A boolean value that is ``False`` by default. When ``True``, upon
    rotation of the user's job event log, a count of the number of job
    events is taken by scanning the log, such that the newly created,
    post-rotation user job event log will have this count in its header.
    This configuration variable is relevant when rotation of the user's
    job event log is enabled.

:macro-def:`EVENT_LOG_FORMAT_OPTIONS[Global]`
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

:macro-def:`EVENT_LOG_USE_XML[Global]`
    A boolean value that defaults to ``False``. When ``True``, events
    are logged in XML format. If :macro:`EVENT_LOG` is not defined, this
    parameter has no effect.

:macro-def:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS[Global]`
    A comma separated list of job ClassAd attributes, whose evaluated
    values form a new event, the ``JobAdInformationEvent``, given Event
    Number 028. This new event is placed in the event log in addition to
    each logged event. If :macro:`EVENT_LOG` is not defined, this
    configuration variable has no effect. This configuration variable is
    the same as the job ClassAd attribute :ad-attr:`JobAdInformationAttrs` (see
    :doc:`/classad-attributes/job-classad-attributes`), but it
    applies to the system Event Log rather than the user job log.

:macro-def:`DEFAULT_USERLOG_FORMAT_OPTIONS[Global]`
    A list of case-insensitive keywords that control formatting of the events
    and of timestamps for the log specified by a job's :ad-attr:`UserLog` or :ad-attr:`DAGManNodesLog`
    attributes. see :macro:`EVENT_LOG_FORMAT_OPTIONS` above for the permitted options.

DaemonCore Configuration File Entries
-------------------------------------

:index:`DaemonCore configuration variables<single: DaemonCore configuration variables; configuration>`

Please read :ref:`admin-manual/installation-startup-shutdown-reconfiguration:DaemonCore` for
details on DaemonCore. There are certain configuration file settings
that DaemonCore uses which affect all HTCondor daemons.

:macro-def:`ALLOW_*[Global]` :macro-def:`DENY_*[Global]`
    All macros that begin with either :macro:`ALLOW_*` or
    :macro:`DENY_*` are settings for HTCondor's security.
    See :ref:`admin-manual/security:authorization` on Setting
    up security in HTCondor for details on these macros and how to
    configure them.

:macro-def:`ENABLE_RUNTIME_CONFIG[Global]`
    The :tool:`condor_config_val` tool has an option **-rset** for
    dynamically setting run time configuration values, and which only
    affect the in-memory configuration variables. Because of the
    potential security implications of this feature, by default,
    HTCondor daemons will not honor these requests. To use this
    functionality, HTCondor administrators must specifically enable it
    by setting :macro:`ENABLE_RUNTIME_CONFIG` to ``True``, and specify what
    configuration variables can be changed using the ``SETTABLE_ATTRS...``
    family of configuration options. Defaults to ``False``.

:macro-def:`ENABLE_PERSISTENT_CONFIG[Global]`
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

:macro-def:`PERSISTENT_CONFIG_DIR[Global]`
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

:macro-def:`SETTABLE_ATTRS_<PERMISSION-LEVEL>[Global]`:index:`SETTABLE_ATTRS_<PERMISSION-LEVEL>`
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

:macro-def:`SHUTDOWN_GRACEFUL_TIMEOUT[Global]`
    Determines how long HTCondor will allow daemons try their graceful
    shutdown methods before they do a hard shutdown. It is defined in
    terms of seconds. The default is 1800 (30 minutes).

:macro-def:`<SUBSYS>_ADDRESS_FILE[Global]`
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

:macro-def:`<SUBSYS>_SUPER_ADDRESS_FILE[Global]`
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

:macro-def:`<SUBSYS>_DAEMON_AD_FILE[Global]`
    A complete path to a file
    that is to contain the ClassAd for a daemon. When the daemon sends a
    ClassAd describing itself to the *condor_collector*, it will also
    place a copy of the ClassAd in this file. Currently, this setting
    only works for the *condor_schedd*. :index:`<SUBSYS>_ATTRS`

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_ATTRS[Global]`
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

:macro-def:`DAEMON_SHUTDOWN[Global]`
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

:macro-def:`DAEMON_SHUTDOWN_FAST[Global]`
    Identical to :macro:`DAEMON_SHUTDOWN` (defined above), except the daemon
    will use the fast shutdown mode (as if it sent itself a
    :tool:`condor_off` command using the **-fast** option).

:macro-def:`USE_CLONE_TO_CREATE_PROCESSES[Global]`
    A boolean value that controls how an HTCondor daemon creates a new
    process on Linux platforms. If set to the default value of ``True``,
    the ``clone`` system call is used. Otherwise, the ``fork`` system
    call is used. ``clone`` provides scalability improvements for
    daemons using a large amount of memory, for example, a
    *condor_schedd* with a lot of jobs in the queue. Currently, the use
    of ``clone`` is available on Linux systems. If HTCondor detects that
    it is running under the *valgrind* analysis tools, this setting is
    ignored and treated as ``False``, to work around incompatibilities.

:macro-def:`MAX_TIME_SKIP[Global]`
    When an HTCondor daemon notices the system clock skip forwards or
    backwards more than the number of seconds specified by this
    parameter, it may take special action. For instance, the
    :tool:`condor_master` will restart HTCondor in the event of a clock skip.
    Defaults to a value of 1200, which in effect means that HTCondor
    will restart if the system clock jumps by more than 20 minutes.

:macro-def:`NOT_RESPONDING_TIMEOUT[Global]`
    When an HTCondor daemon's parent process is another HTCondor daemon,
    the child daemon will periodically send a short message to its
    parent stating that it is alive and well. If the parent does not
    hear from the child for a while, the parent assumes that the child
    is hung, kills the child, and restarts the child. This parameter
    controls how long the parent waits before killing the child. It is
    defined in terms of seconds and defaults to 3600 (1 hour). The child
    sends its alive and well messages at an interval of one third of
    this value.

:macro-def:`<SUBSYS>_NOT_RESPONDING_TIMEOUT[Global]`
    Identical to :macro:`NOT_RESPONDING_TIMEOUT`, but controls the timeout
    for a specific type of daemon. For example,
    ``SCHEDD_NOT_RESPONDING_TIMEOUT`` controls how long the
    *condor_schedd* 's parent daemon will wait without receiving an
    alive and well message from the *condor_schedd* before killing it.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`NOT_RESPONDING_WANT_CORE[Global]`
    A boolean value with a default value of ``False``. This parameter is
    for debugging purposes on Unix systems, and it controls the behavior
    of the parent process when the parent process determines that a
    child process is not responding. If :macro:`NOT_RESPONDING_WANT_CORE` is
    ``True``, the parent will send a SIGABRT instead of SIGKILL to the
    child process. If the child process is configured with the
    configuration variable :macro:`CREATE_CORE_FILES` enabled, the child
    process will then generate a core dump. See :macro:`NOT_RESPONDING_TIMEOUT`
    and :macro:`CREATE_CORE_FILES` for more details.

:macro-def:`LOCK_FILE_UPDATE_INTERVAL[Global]`
    An integer value representing seconds, controlling how often valid
    lock files should have their on disk timestamps updated. Updating
    the timestamps prevents administrative programs, such as *tmpwatch*,
    from deleting long lived lock files. If set to a value less than 60,
    the update time will be 60 seconds. The default value is 28800,
    which is 8 hours. This variable only takes effect at the start or
    restart of a daemon.

:macro-def:`SOCKET_LISTEN_BACKLOG[Global]`
    An integer value that defaults to 4096, which defines the backlog
    value for the listen() network call when a daemon creates a socket
    for incoming connections. It limits the number of new incoming
    network connections the operating system will accept for a daemon
    that the daemon has not yet serviced.

:macro-def:`MAX_ACCEPTS_PER_CYCLE[Global]`
    An integer value that defaults to 8. It is a rarely changed
    performance tuning parameter to limit the number of accepts of new,
    incoming, socket connect requests per DaemonCore event cycle. A
    value of zero or less means no limit. It has the most noticeable
    effect on the *condor_schedd*, and would be given a higher integer
    value for tuning purposes when there is a high number of jobs
    starting and exiting per second.

:macro-def:`MAX_TIMER_EVENTS_PER_CYCLE[Global]`
    An integer value that defaults to 3. It is a rarely changed
    performance tuning parameter to set the max number of internal
    timer events will be dispatched per DaemonCore event cycle.
    A value of zero means no limit, so that all timers that are due
    at the start of the event cycle should be dispatched.

:macro-def:`MAX_UDP_MSGS_PER_CYCLE[Global]`
    An integer value that defaults to 1. It is a rarely changed
    performance tuning parameter to set the number of incoming UDP
    messages a daemon will read per DaemonCore event cycle.
    A value of zero means no limit. It has the most noticeable
    effect on the *condor_schedd* and *condor_collector* daemons,
    which can receive a large number of UDP messages when under heavy
    load.

:macro-def:`MAX_REAPS_PER_CYCLE[Global]`
    An integer value that defaults to 0. It is a rarely changed
    performance tuning parameter that places a limit on the number of
    child process exits to process per DaemonCore event cycle. A value
    of zero or less means no limit.

:macro-def:`CORE_FILE_NAME[Global]`
    Defines the name of the core file created on Windows platforms.
    Defaults to ``core.$(SUBSYSTEM).WIN32``.

:macro-def:`PIPE_BUFFER_MAX[Global]`
    The maximum number of bytes read from a ``stdout`` or ``stdout``
    pipe. The default value is 10240. A rare example in which the value
    would need to increase from its default value is when a hook must
    output an entire ClassAd, and the ClassAd may be larger than the
    default.

:macro-def:`DETECTED_MEMORY[Global]`
    A read-only macro that cannot be set, but expands to the
    amount of detected memory on the system, regardless
    of any overrides via the :macro:`MEMORY` setting.

Network-Related Configuration File Entries
------------------------------------------

:index:`network-related configuration variables<single: network-related configuration variables; configuration>`

More information about networking in HTCondor can be found in
:doc:`/admin-manual/networking`.

:macro-def:`BIND_ALL_INTERFACES[Network]`
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

:macro-def:`CCB_ADDRESS[Network]`
    This is the address of a *condor_collector* that will serve as this
    daemon's HTCondor Connection Broker (CCB). Multiple addresses may be
    listed (separated by commas and/or spaces) for redundancy. The CCB
    server must authorize this daemon at DAEMON level for this
    configuration to succeed. It is highly recommended to also configure
    :macro:`PRIVATE_NETWORK_NAME` if you configure :macro:`CCB_ADDRESS` so
    communications originating within the same private network do not
    need to go through CCB. For more information about CCB, see
    :ref:`admin-manual/networking:htcondor connection brokering (ccb)`.

:macro-def:`CCB_HEARTBEAT_INTERVAL[Network]`
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

:macro-def:`CCB_POLLING_INTERVAL[Network]`
    In seconds, the smallest amount of time that could go by before CCB
    would begin another round of polling to check on already connected
    clients. While the value of this variable does not change, the
    actual interval used may be exceeded if the measured amount of time
    previously taken to poll to check on already connected clients
    exceeded the amount of time desired, as expressed with
    :macro:`CCB_POLLING_TIMESLICE`. The default value is 20 seconds.

:macro-def:`CCB_POLLING_MAX_INTERVAL[Network]`
    In seconds, the interval of time after which polling to check on
    already connected clients must occur, independent of any other
    factors. The default value is 600 seconds.

:macro-def:`CCB_POLLING_TIMESLICE[Network]`
    A floating point fraction representing the fractional amount of the
    total run time of CCB to set as a target for the maximum amount of
    CCB running time used on polling to check on already connected
    clients. The default value is 0.05.

:macro-def:`CCB_READ_BUFFER[Network]`
    The size of the kernel TCP read buffer in bytes for all sockets used
    by CCB. The default value is 2 KiB.

:macro-def:`CCB_REQUIRED_TO_START[Network]`
    If true, and :macro:`USE_SHARED_PORT` is false, and :macro:`CCB_ADDRESS`
    is set, but HTCondor fails to register with any broker, HTCondor will
    exit rather then continue to retry indefinitely.

:macro-def:`CCB_TIMEOUT[Network]`
    The length, in seconds, that we wait for any CCB operation to complete.
    The default value is 300.

:macro-def:`CCB_WRITE_BUFFER[Network]`
    The size of the kernel TCP write buffer in bytes for all sockets
    used by CCB. The default value is 2 KiB.

:macro-def:`CCB_SWEEP_INTERVAL[Network]`
    The interval, in seconds, between times when the CCB server writes
    its information about open TCP connections to a file. Crash recovery
    is accomplished using the information. The default value is 1200
    seconds (20 minutes).

:macro-def:`CCB_RECONNECT_FILE[Network]`
    The full path and file name of the file that the CCB server writes
    its information about open TCP connections to a file. Crash recovery
    is accomplished using the information. The default value is
    ``$(SPOOL)/<ip address>-<shared port ID or port number>.ccb_reconnect``.

:macro-def:`COLLECTOR_USES_SHARED_PORT[Network]`
    A boolean value that specifies whether the *condor_collector* uses
    the *condor_shared_port* daemon. When true, the
    *condor_shared_port* will transparently proxy queries to the
    *condor_collector* so users do not need to be aware of the presence
    of the *condor_shared_port* when querying the collector and
    configuring other daemons. The default is ``True``

:macro-def:`SHARED_PORT_DEFAULT_ID[Network]`
    When :macro:`COLLECTOR_USES_SHARED_PORT` is set to ``True``, this
    is the shared port ID used by the *condor_collector*. This defaults
    to ``collector`` and will not need to be changed by most sites.

:macro-def:`AUTO_INCLUDE_SHARED_PORT_IN_DAEMON_LIST[Network]`
    A boolean value that specifies whether :macro:`SHARED_PORT`
    should be automatically inserted into :tool:`condor_master` 's
    :macro:`DAEMON_LIST` when :macro:`USE_SHARED_PORT` is
    ``True``. The default for this setting is ``True``.

:macro-def:`<SUBSYS>_MAX_FILE_DESCRIPTORS[Network]`
    This setting is identical to :macro:`MAX_FILE_DESCRIPTORS`, but it only
    applies to a specific subsystem. If the subsystem-specific setting
    is unspecified, :macro:`MAX_FILE_DESCRIPTORS` is used. For the
    *condor_collector* daemon, the value defaults to 10240, and for the
    *condor_schedd* daemon, the value defaults to 4096. If the
    *condor_shared_port* daemon is in use, its value for this
    parameter should match the largest value set for the other daemons.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MAX_FILE_DESCRIPTORS[Network]`
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

:macro-def:`NETWORK_HOSTNAME[Network]`
    The name HTCondor should use as the host name of the local machine,
    overriding the value returned by gethostname(). Among other things,
    the host name is used to identify daemons in an HTCondor pool, via
    the ``Machine`` and ``Name`` attributes of daemon ClassAds. This
    variable can be used when a machine has multiple network interfaces
    with different host names, to use a host name that is not the
    primary one. It should be set to a fully-qualified host name that
    will resolve to an IP address of the local machine.

:macro-def:`NETWORK_INTERFACE[Network]`
    An IP address of the form ``123.123.123.123`` or the name of a
    network device, as in the example ``eth0``. The wild card character
    (``*``) may be used within either. For example, ``123.123.*`` would
    match a network interface with an IP address of ``123.123.123.123``
    or ``123.123.100.100``. The default value is ``*``, which matches
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

:macro-def:`PRIVATE_NETWORK_NAME[Network]`
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

:macro-def:`PRIVATE_NETWORK_INTERFACE[Network]`
    For systems with multiple network interfaces, if this configuration
    setting and :macro:`PRIVATE_NETWORK_NAME` are both defined, HTCondor
    daemons will advertise some additional attributes in their ClassAds
    to help other HTCondor daemons and tools in the same private network
    to communicate directly.

    :macro:`PRIVATE_NETWORK_INTERFACE` defines what IP address of the form
    ``123.123.123.123`` or name of a network device (as in the example
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

:macro-def:`TCP_FORWARDING_HOST[Network]`
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

:macro-def:`HIGHPORT[Network]`
    Specifies an upper limit of given port numbers for HTCondor to use,
    such that HTCondor is restricted to a range of port numbers. If this
    macro is not explicitly specified, then HTCondor will not restrict
    the port numbers that it uses. HTCondor will use system-assigned
    port numbers. For this macro to work, both :macro:`HIGHPORT` and
    :macro:`LOWPORT` (given below) must be defined.

:macro-def:`LOWPORT[Network]`
    Specifies a lower limit of given port numbers for HTCondor to use,
    such that HTCondor is restricted to a range of port numbers. If this
    macro is not explicitly specified, then HTCondor will not restrict
    the port numbers that it uses. HTCondor will use system-assigned
    port numbers. For this macro to work, both :macro:`HIGHPORT` (given
    above) and :macro:`LOWPORT` must be defined.

:macro-def:`IN_LOWPORT[Network]`
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

:macro-def:`IN_HIGHPORT[Network]`
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

:macro-def:`OUT_LOWPORT[Network]`
    An integer value that specifies a lower limit of given port numbers
    for HTCondor to use on outgoing connections, such that HTCondor is
    restricted to a range of port numbers. This range implies the use of
    both :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT`. A range of port numbers
    less than 1024 is inappropriate, as not all daemons and tools will
    be run as root. Applies only to Unix machine configuration. Use of
    :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT` overrides any definition of
    :macro:`LOWPORT` and :macro:`HIGHPORT`.

:macro-def:`OUT_HIGHPORT[Network]`
    An integer value that specifies an upper limit of given port numbers
    for HTCondor to use on outgoing connections, such that HTCondor is
    restricted to a range of port numbers. This range implies the use of
    both :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT`. A range of port numbers
    less than 1024 is inappropriate, as not all daemons and tools will
    be run as root. Applies only to Unix machine configuration. Use of
    :macro:`OUT_LOWPORT` and :macro:`OUT_HIGHPORT` overrides any definition of
    :macro:`LOWPORT` and :macro:`HIGHPORT`.

:macro-def:`UPDATE_COLLECTOR_WITH_TCP[Network]`
    This boolean value controls whether TCP or UDP is used by daemons to
    send ClassAd updates to the *condor_collector*. Please read
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more details and a discussion of when this functionality is
    needed. When using TCP in large pools, it is also necessary to
    ensure that the *condor_collector* has a large enough file
    descriptor limit using :macro:`COLLECTOR_MAX_FILE_DESCRIPTORS`.
    The default value is ``True``.

:macro-def:`UPDATE_VIEW_COLLECTOR_WITH_TCP[Network]`
    This boolean value controls whether TCP or UDP is used by the
    *condor_collector* to forward ClassAd updates to the
    *condor_collector* daemons specified by :macro:`CONDOR_VIEW_HOST`. Please read
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more details and a discussion of when this functionality is
    needed. The default value is ``False``.

:macro-def:`TCP_UPDATE_COLLECTORS[Network]`
    The list of *condor_collector* daemons which will be updated with
    TCP instead of UDP when :macro:`UPDATE_COLLECTOR_WITH_TCP` or
    :macro:`UPDATE_VIEW_COLLECTOR_WITH_TCP` is ``False``. Please read
    :ref:`admin-manual/networking:using tcp to send updates to the *condor_collector*`
    for more details and a discussion of when a site needs this
    functionality.

:macro-def:`<SUBSYS>_TIMEOUT_MULTIPLIER[Network]`
    An integer value that
    defaults to 1. This value multiplies configured timeout values for
    all targeted subsystem communications, thereby increasing the time
    until a timeout occurs. This configuration variable is intended for
    use by developers for debugging purposes, where communication
    timeouts interfere.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`NONBLOCKING_COLLECTOR_UPDATE[Network]`
    A boolean value that defaults to ``True``. When ``True``, the
    establishment of TCP connections to the *condor_collector* daemon
    for a security-enabled pool are done in a nonblocking manner.

:macro-def:`NEGOTIATOR_USE_NONBLOCKING_STARTD_CONTACT[Network]`
    A boolean value that defaults to ``True``. When ``True``, the
    establishment of TCP connections from the *condor_negotiator*
    daemon to the *condor_startd* daemon for a security-enabled pool
    are done in a nonblocking manner.

:macro-def:`UDP_NETWORK_FRAGMENT_SIZE[Network]`
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

:macro-def:`UDP_LOOPBACK_FRAGMENT_SIZE[Network]`
    An integer value that defaults to 60000 and represents the maximum
    size in bytes of an outgoing UDP packet that is being sent to the
    loopback network interface (e.g. 127.0.0.1). If the outgoing message
    is larger than ``$(UDP_LOOPBACK_FRAGMENT_SIZE)``, then the message
    will be split (fragmented) into multiple packets no larger than
    ``$(UDP_LOOPBACK_FRAGMENT_SIZE)``. If the destination of the message
    is not the loopback interface, see :macro:`UDP_NETWORK_FRAGMENT_SIZE`
    above.

:macro-def:`ALWAYS_REUSEADDR[Network]`
    A boolean value that, when ``True``, tells HTCondor to set
    ``SO_REUSEADDR`` socket option, so that the schedd can run large
    numbers of very short jobs without exhausting the number of local
    ports needed for shadows. The default value is ``True``. (Note that
    this represents a change in behavior compared to versions of
    HTCondor older than 8.6.0, which did not include this configuration
    macro. To restore the previous behavior, set this value to
    ``False``.)

Shared File System Configuration File Macros
--------------------------------------------

:index:`shared file system configuration variables<single: shared file system configuration variables; configuration>`

These macros control how HTCondor interacts with various shared and
network file systems.For information on submitting jobs under shared
file systems, see :ref:`users-manual/submitting-a-job:Submitting Jobs Using a Shared File System`.

:macro-def:`UID_DOMAIN[FileSystem]`
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
    flippy.example.com, claiming a :macro:`UID_DOMAIN` of of cs.wisc.edu,
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

:macro-def:`TRUST_UID_DOMAIN[FileSystem]`
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

:macro-def:`TRUST_LOCAL_UID_DOMAIN[FileSystem]`
    This parameter works like :macro:`TRUST_UID_DOMAIN`, but is only applied
    when the *condor_starter* and *condor_shadow* are on the same
    machine. If this parameter is set to ``True``, then the
    *condor_shadow* 's :macro:`UID_DOMAIN` doesn't have to be a substring
    its hostname. If this parameter is set to ``False``, then
    :macro:`UID_DOMAIN` controls whether this substring requirement is
    enforced by the *condor_starter*. The default is ``True``.

:macro-def:`SOFT_UID_DOMAIN[FileSystem]`
    A boolean variable that defaults to ``False`` when not defined. When
    HTCondor is about to run a job as a particular user (instead of as
    user nobody), it verifies that the UID given for the user is in the
    password file and actually matches the given user name. However,
    under installations that do not have every user in every machine's
    password file, this check will fail and the execution attempt will
    be aborted. To cause HTCondor not to do this check, set this
    configuration variable to ``True``. HTCondor will then run the job
    under the user's UID.

:macro-def:`SLOT<N>_USER[FileSystem]`
    The name of a user for HTCondor to use instead of user nobody, as
    part of a solution that plugs a security hole whereby a lurker
    process can prey on a subsequent job run as user name nobody.
    ``<N>`` is an integer associated with slots. On non Windows platforms
    you can use :macro:`NOBODY_SLOT_USER` instead of this configuration variable.
    On Windows, :macro:`SLOT<N>_USER` will only work if the credential of the specified
    user is stored on the execute machine using :tool:`condor_store_cred`.
    See :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    for more information.

:macro-def:`NOBODY_SLOT_USER[FileSystem]`
    The name of a user for HTCondor to use instead of user nobody when
    The :macro:`SLOT<N>_USER` for this slot is not configured.  Configure
    this to the value ``$(STARTER_SLOT_NAME)`` to use the name of the slot
    as the user name. This configuration macro is ignored on Windows,
    where the Starter will automatically create a unique temporary user for each slot as needed.
    See :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    for more information.

:macro-def:`STARTER_ALLOW_RUNAS_OWNER[FileSystem]`
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

:macro-def:`DEDICATED_EXECUTE_ACCOUNT_REGEXP[FileSystem]`
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

:macro-def:`EXECUTE_LOGIN_IS_DEDICATED[FileSystem]`
    This configuration setting is deprecated because it cannot handle
    the case where some jobs run as dedicated accounts and some do not.
    Use :macro:`DEDICATED_EXECUTE_ACCOUNT_REGEXP` instead.

    A boolean value that defaults to ``False``. When ``True``, HTCondor
    knows that all jobs are being run by dedicated execution accounts
    (whether they are running as the job owner or as nobody or as
    :macro:`SLOT<N>_USER`). Therefore, when the job exits, all processes
    running under the same account will be killed.

:macro-def:`FILESYSTEM_DOMAIN[FileSystem]`
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

:macro-def:`USE_NFS[FileSystem]`
    This configuration variable changes the semantics of Chirp
    file I/O when running in the vanilla, java or parallel universe. If
    this variable is set in those universes, Chirp will not send I/O
    requests over the network as requested, but perform them directly to
    the locally mounted file system.

:macro-def:`IGNORE_NFS_LOCK_ERRORS[FileSystem]`
    When set to ``True``, all errors related to file locking errors from
    NFS are ignored. Defaults to ``False``, not ignoring errors.

condor_master Configuration File Macros
----------------------------------------

:index:`condor_master configuration variables<single: condor_master configuration variables; configuration>`

These macros control the :tool:`condor_master`.

:macro-def:`DAEMON_LIST[MASTER]`
    This macro determines what daemons the :tool:`condor_master` will start
    and keep its watchful eyes on. The list is a comma or space
    separated list of subsystem names (listed in
    :ref:`admin-manual/introduction-to-configuration:pre-defined macros`).
    For example,

    .. code-block:: condor-config

          DAEMON_LIST = MASTER, STARTD, SCHEDD

    .. note::

        The *condor_shared_port* daemon will be included in this list
        automatically when :macro:`USE_SHARED_PORT` is configured to ``True``.
        While adding ``SHARED_PORT`` to the :macro:`DAEMON_LIST` without setting
        :macro:`USE_SHARED_PORT` to ``True`` will start the *condor_shared_port*
        daemon, but it will not be used.  So there is generally no point
        in adding ``SHARED_PORT`` to the daemon list.

    .. note::

        On your central manager, your ``$(DAEMON_LIST)`` will be
        different from your regular pool, since it will include entries for
        the *condor_collector* and *condor_negotiator*.

:macro-def:`DC_DAEMON_LIST[MASTER]`
    A list delimited by commas and/or spaces that lists the daemons in
    :macro:`DAEMON_LIST` which use the HTCondor DaemonCore library. The
    :tool:`condor_master` must differentiate between daemons that use
    DaemonCore and those that do not, so it uses the appropriate
    inter-process communication mechanisms. This list currently includes
    all HTCondor daemons.

    As of HTCondor version 7.2.1, a daemon may be appended to the
    default :macro:`DC_DAEMON_LIST` value by placing the plus character (+)
    before the first entry in the :macro:`DC_DAEMON_LIST` definition. For
    example:

    .. code-block:: condor-config

          DC_DAEMON_LIST = +NEW_DAEMON

:macro-def:`<SUBSYS>[MASTER]`
    Once you have defined which subsystems you
    want the :tool:`condor_master` to start, you must provide it with the
    full path to each of these binaries. For example:

    .. code-block:: condor-config

            MASTER          = $(SBIN)/condor_master
            STARTD          = $(SBIN)/condor_startd
            SCHEDD          = $(SBIN)/condor_schedd


    These are most often defined relative to the ``$(SBIN)`` macro.

    The macro is named by substituting :macro:`<SUBSYS>` with the appropriate
    subsystem string as defined by :macro:`SUBSYSTEM`.

:macro-def:`<DaemonName>_ENVIRONMENT[MASTER]`
    ``<DaemonName>`` is the name of a daemon listed in :macro:`DAEMON_LIST`.
    Defines changes to the environment that the daemon is invoked with.
    It should use the same syntax for specifying the environment as the
    environment specification in a submit description file. For example,
    to redefine the ``TMP`` and ``CONDOR_CONFIG`` environment variables
    seen by the *condor_schedd*, place the following in the
    configuration:

    .. code-block:: condor-config

          SCHEDD_ENVIRONMENT = "TMP=/new/value CONDOR_CONFIG=/special/config"

    When the *condor_schedd* daemon is started by the :tool:`condor_master`,
    it would see the specified values of ``TMP`` and ``CONDOR_CONFIG``.

:macro-def:`<SUBSYS>_ARGS[MASTER]`
    This macro allows the specification of additional command line
    arguments for any process spawned by the :tool:`condor_master`. List the
    desired arguments using the same syntax as the arguments
    specification in a :tool:`condor_submit` submit file (see
    :doc:`/man-pages/condor_submit/`), with one
    exception: do not escape double-quotes when using the old-style
    syntax (this is for backward compatibility). Set the arguments for a
    specific daemon with this macro, and the macro will affect only that
    daemon. Define one of these for each daemon the :tool:`condor_master` is
    controlling. For example, set ``$(STARTD_ARGS)`` to specify any
    extra command line arguments to the *condor_startd*.

    The macro is named by substituting :macro:`<SUBSYS>` with the appropriate
    subsystem string as defined by :macro:`SUBSYSTEM`.

:macro-def:`<SUBSYS>_USERID[MASTER]`
    The account name that should be used
    to run the ``SUBSYS`` process spawned by the :tool:`condor_master`. When
    not defined, the process is spawned as the same user that is running
    :tool:`condor_master`. When defined, the real user id of the spawned
    process will be set to the specified account, so if this account is
    not root, the process will not have root privileges. The
    :tool:`condor_master` must be running as root in order to start processes
    as other users. Example configuration:

    .. code-block:: condor-config

        COLLECTOR_USERID = condor
        NEGOTIATOR_USERID = condor

    The above example runs the *condor_collector* and
    *condor_negotiator* as the condor user with no root privileges. If
    we specified some account other than the condor user, as set by the
    (:macro:`CONDOR_IDS`) configuration variable, then we would need to
    configure the log files for these daemons to be in a directory that
    they can write to. When using a security
    method in which the daemon credential is owned by root, it is also
    necessary to make a copy of the credential, make it be owned by the
    account the daemons are using, and configure the daemons to use that
    copy.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`PREEN[MASTER]`
    In addition to the daemons defined in ``$(DAEMON_LIST)``, the
    :tool:`condor_master` also starts up a special process, :tool:`condor_preen`
    to clean out junk files that have been left laying around by
    HTCondor. This macro determines where the :tool:`condor_master` finds the
    :tool:`condor_preen` binary. If this macro is set to nothing,
    :tool:`condor_preen` will not run.

:macro-def:`PREEN_ARGS[MASTER]`
    Controls how :tool:`condor_preen` behaves by allowing the specification
    of command-line arguments. This macro works as ``$(<SUBSYS>_ARGS)``
    does. The difference is that you must specify this macro for
    :tool:`condor_preen` if you want it to do anything. :tool:`condor_preen` takes
    action only because of command line arguments. **-m** means you want
    e-mail about files :tool:`condor_preen` finds that it thinks it should
    remove. **-r** means you want :tool:`condor_preen` to actually remove
    these files.

:macro-def:`PREEN_INTERVAL[MASTER]`
    This macro determines how often :tool:`condor_preen` should be started.
    It is defined in terms of seconds and defaults to 86400 (once a
    day).

:macro-def:`PUBLISH_OBITUARIES[MASTER]`
    When a daemon crashes, the :tool:`condor_master` can send e-mail to the
    address specified by ``$(CONDOR_ADMIN)`` with an obituary letting
    the administrator know that the daemon died, the cause of death
    (which signal or exit status it exited with), and (optionally) the
    last few entries from that daemon's log file. If you want
    obituaries, set this macro to ``True``.

:macro-def:`OBITUARY_LOG_LENGTH[MASTER]`
    This macro controls how many lines of the log file are part of
    obituaries. This macro has a default value of 20 lines.

:macro-def:`START_MASTER[MASTER]`
    If this setting is defined and set to ``False`` the :tool:`condor_master`
    will immediately exit upon startup. This appears strange, but
    perhaps you do not want HTCondor to run on certain machines in your
    pool, yet the boot scripts for your entire pool are handled by a
    centralized set of files - setting :macro:`START_MASTER` to ``False`` for
    those machines would allow this. Note that :macro:`START_MASTER` is an
    entry you would most likely find in a local configuration file, not
    a global configuration file. If not defined, :macro:`START_MASTER`
    defaults to ``True``.

:macro-def:`START_DAEMONS[MASTER]`
    This macro is similar to the ``$(START_MASTER)`` macro described
    above. However, the :tool:`condor_master` does not exit; it does not
    start any of the daemons listed in the ``$(DAEMON_LIST)``. The
    daemons may be started at a later time with a :tool:`condor_on` command.

:macro-def:`MASTER_UPDATE_INTERVAL[MASTER]`
    This macro determines how often the :tool:`condor_master` sends a ClassAd
    update to the *condor_collector*. It is defined in seconds and
    defaults to 300 (every 5 minutes).

:macro-def:`MASTER_CHECK_NEW_EXEC_INTERVAL[MASTER]`
    This macro controls how often the :tool:`condor_master` checks the
    timestamps of the running daemons. If any daemons have been
    modified, the master restarts them. It is defined in seconds and
    defaults to 300 (every 5 minutes).

:macro-def:`MASTER_NEW_BINARY_RESTART[MASTER]`
    Defines a mode of operation for the restart of the :tool:`condor_master`,
    when it notices that the :tool:`condor_master` binary has changed. Valid
    values are ``GRACEFUL``, ``PEACEFUL``, and ``NEVER``, with a default
    value of ``GRACEFUL``. On a ``GRACEFUL`` restart of the master,
    child processes are told to exit, but if they do not before a timer
    expires, then they are killed. On a ``PEACEFUL`` restart, child
    processes are told to exit, after which the :tool:`condor_master` waits
    until they do so.

:macro-def:`MASTER_NEW_BINARY_DELAY[MASTER]`
    Once the :tool:`condor_master` has discovered a new binary, this macro
    controls how long it waits before attempting to execute the new
    binary. This delay exists because the :tool:`condor_master` might notice
    a new binary while it is in the process of being copied, in which
    case trying to execute it yields unpredictable results. The entry is
    defined in seconds and defaults to 120 (2 minutes).

:macro-def:`SHUTDOWN_FAST_TIMEOUT[MASTER]`
    This macro determines the maximum amount of time daemons are given
    to perform their fast shutdown procedure before the :tool:`condor_master`
    kills them outright. It is defined in seconds and defaults to 300 (5
    minutes).

:macro-def:`DEFAULT_MASTER_SHUTDOWN_SCRIPT[MASTER]`
    A full path and file name of a program that the :tool:`condor_master` is
    to execute via the Unix execl() call, or the similar Win32 _execl()
    call, instead of the normal call to exit(). This allows the admin to
    specify a program to execute as root when the :tool:`condor_master`
    exits. Note that a successful call to the :tool:`condor_set_shutdown`
    program will override this setting; see the documentation for config
    knob :macro:`MASTER_SHUTDOWN_<Name>` below.

:macro-def:`MASTER_SHUTDOWN_<Name>[MASTER]`
    A full path and file name of a program that the :tool:`condor_master` is
    to execute via the Unix execl() call, or the similar Win32 _execl()
    call, instead of the normal call to exit(). Multiple programs to
    execute may be defined with multiple entries, each with a unique
    ``Name``. These macros have no effect on a :tool:`condor_master` unless
    :tool:`condor_set_shutdown` is run, or the `-exec` argument is used with
    :tool:`condor_off` or :tool:`condor_restart`. The ``Name`` specified as an
    argument to the :tool:`condor_set_shutdown` program or `-exec` arg must match the
    ``Name`` portion of one of these :macro:`MASTER_SHUTDOWN_<Name>` macros;
    if not, the :tool:`condor_master` will log an error and ignore the
    command. If a match is found, the :tool:`condor_master` will attempt to
    verify the program, and it will store the path and program name.
    When the :tool:`condor_master` shuts down (that is, just before it
    exits), the program is then executed as described above. The manual
    page for :doc:`/man-pages/condor_set_shutdown` contains details on the
    use of this program.

    NOTE: This program will be run with root privileges under Unix or
    administrator privileges under Windows. The administrator must
    ensure that this cannot be used in such a way as to violate system
    integrity.

:macro-def:`MASTER_BACKOFF_CONSTANT[MASTER]` and :macro-def:`MASTER_<name>_BACKOFF_CONSTANT[MASTER]`
    When a daemon crashes, :tool:`condor_master` uses an exponential back off
    delay before restarting it; see the discussion at the end of this
    section for a detailed discussion on how these parameters work
    together. These settings define the constant value of the expression
    used to determine how long to wait before starting the daemon again
    (and, effectively becomes the initial backoff time). It is an
    integer in units of seconds, and defaults to 9 seconds.

    ``$(MASTER_<name>_BACKOFF_CONSTANT)`` is the daemon-specific form of
    :macro:`MASTER_BACKOFF_CONSTANT`; if this daemon-specific macro is not
    defined for a specific daemon, the non-daemon-specific value will
    used.

:macro-def:`MASTER_BACKOFF_FACTOR[MASTER]` and :macro-def:`MASTER_<name>_BACKOFF_FACTOR[MASTER]`
    When a daemon crashes, :tool:`condor_master` uses an exponential back off
    delay before restarting it; see the discussion at the end of this
    section for a detailed discussion on how these parameters work
    together. This setting is the base of the exponent used to determine
    how long to wait before starting the daemon again. It defaults to 2
    seconds.

    ``$(MASTER_<name>_BACKOFF_FACTOR)`` is the daemon-specific form of
    :macro:`MASTER_BACKOFF_FACTOR`; if this daemon-specific macro is not
    defined for a specific daemon, the non-daemon-specific value will
    used.

:macro-def:`MASTER_BACKOFF_CEILING[MASTER]` and :macro-def:`MASTER_<name>_BACKOFF_CEILING[MASTER]`
    When a daemon crashes, :tool:`condor_master` uses an exponential back off
    delay before restarting it; see the discussion at the end of this
    section for a detailed discussion on how these parameters work
    together. This entry determines the maximum amount of time you want
    the master to wait between attempts to start a given daemon. (With
    2.0 as the ``$(MASTER_BACKOFF_FACTOR)``, 1 hour is obtained in 12
    restarts). It is defined in terms of seconds and defaults to 3600 (1
    hour).

    ``$(MASTER_<name>_BACKOFF_CEILING)`` is the daemon-specific form of
    :macro:`MASTER_BACKOFF_CEILING`; if this daemon-specific macro is not
    defined for a specific daemon, the non-daemon-specific value will
    used.

:macro-def:`MASTER_RECOVER_FACTOR[MASTER]` and :macro-def:`MASTER_<name>_RECOVER_FACTOR[MASTER]`
    A macro to set how long a daemon needs to run without crashing
    before it is considered recovered. Once a daemon has recovered, the
    number of restarts is reset, so the exponential back off returns to
    its initial state. The macro is defined in terms of seconds and
    defaults to 300 (5 minutes).

    ``$(MASTER_<name>_RECOVER_FACTOR)`` is the daemon-specific form of
    :macro:`MASTER_RECOVER_FACTOR`; if this daemon-specific macro is not
    defined for a specific daemon, the non-daemon-specific value will
    used.

When a daemon crashes, :tool:`condor_master` will restart the daemon after a
delay (a back off). The length of this delay is based on how many times
it has been restarted, and gets larger after each crashes. The equation
for calculating this backoff time is given by:

.. math::

    t = c + k^n

where t is the calculated time, c is the constant defined by
``$(MASTER_BACKOFF_CONSTANT)``, k is the "factor" defined by
``$(MASTER_BACKOFF_FACTOR)``, and n is the number of restarts already
attempted (0 for the first restart, 1 for the next, etc.).

With default values, after the first crash, the delay would be t = 9 +
2.0\ :sup:`0`, giving 10 seconds (remember, n = 0). If the daemon keeps
crashing, the delay increases.

For example, take the ``$(MASTER_BACKOFF_FACTOR)`` (which defaults to
2.0) to the power the number of times the daemon has restarted, and add
``$(MASTER_BACKOFF_CONSTANT)`` (which defaults to 9). Thus:

1\ :sup:`st` crash: n = 0, so: t = 9 + 2\ :sup:`0` = 9 + 1 = 10 seconds

2\ :sup:`nd` crash: n = 1, so: t = 9 + 2\ :sup:`1` = 9 + 2 = 11 seconds

3\ :sup:`rd` crash: n = 2, so: t = 9 + 2\ :sup:`2` = 9 + 4 = 13 seconds

...

6\ :sup:`th` crash: n = 5, so: t = 9 + 2\ :sup:`5` = 9 + 32 = 41 seconds

...

9\ :sup:`th` crash: n = 8, so: t = 9 + 2\ :sup:`8` = 9 + 256 =
265 seconds

And, after the 13 crashes, it would be:

13\ :sup:`th` crash: n = 12, so: t = 9 + 2\ :sup:`12` = 9 + 4096 =
4105 seconds

This is bigger than the ``$(MASTER_BACKOFF_CEILING)``, which defaults to
3600, so the daemon would really be restarted after only 3600 seconds,
not 4105. The :tool:`condor_master` tries again every hour (since the numbers
would get larger and would always be capped by the ceiling). Eventually,
imagine that daemon finally started and did not crash. This might happen
if, for example, an administrator reinstalled an accidentally deleted
binary after receiving e-mail about the daemon crashing. If it stayed
alive for ``$(MASTER_RECOVER_FACTOR)`` seconds (defaults to 5 minutes),
the count of how many restarts this daemon has performed is reset to 0.

The moral of the example is that the defaults work quite well, and you
probably will not want to change them for any reason.

:macro-def:`MASTER_NAME[MASTER]`
    Defines a unique name given for a :tool:`condor_master` daemon on a
    machine. For a :tool:`condor_master` running as root, it defaults to the
    fully qualified host name. When not running as root, it defaults to
    the user that instantiates the :tool:`condor_master`, concatenated with
    an at symbol (@), concatenated with the fully qualified host name.
    If more than one :tool:`condor_master` is running on the same host, then
    the :macro:`MASTER_NAME` for each :tool:`condor_master` must be defined to
    uniquely identify the separate daemons.

    A defined :macro:`MASTER_NAME` is presumed to be of the form
    identifying-string@full.host.name. If the string does not include an
    @ sign, HTCondor appends one, followed by the fully qualified host
    name of the local machine. The identifying-string portion may
    contain any alphanumeric ASCII characters or punctuation marks,
    except the @ sign. We recommend that the string does not contain the
    : (colon) character, since that might cause problems with certain
    tools. Previous to HTCondor 7.1.1, when the string included an @
    sign, HTCondor replaced whatever followed the @ sign with the fully
    qualified host name of the local machine. HTCondor does not modify
    any portion of the string, if it contains an @ sign. This is useful
    for remote job submissions under the high availability of the job
    queue.

    If the :macro:`MASTER_NAME` setting is used, and the :tool:`condor_master` is
    configured to spawn a *condor_schedd*, the name defined with
    :macro:`MASTER_NAME` takes precedence over the :macro:`SCHEDD_NAME`
    setting. Since HTCondor makes the assumption that there is only
    one instance of the *condor_startd* running on a machine, the
    :macro:`MASTER_NAME` is not automatically propagated to the *condor_startd*.
    However, in situations where multiple *condor_startd* daemons are
    running on the same host, the :macro:`STARTD_NAME` should be set to
    uniquely identify the *condor_startd* daemons.

    If an HTCondor daemon (master, schedd or startd) has been given a
    unique name, all HTCondor tools that need to contact that daemon can
    be told what name to use via the **-name** command-line option.

:macro-def:`MASTER_ATTRS[MASTER]`
    This macro is described in :macro:`<SUBSYS>_ATTRS`.

:macro-def:`MASTER_DEBUG[MASTER]`
    This macro is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`MASTER_ADDRESS_FILE[MASTER]`
    This macro is described in :macro:`<SUBSYS>_ADDRESS_FILE`.

:macro-def:`ALLOW_ADMIN_COMMANDS[MASTER]`
    If set to NO for a given host, this macro disables administrative
    commands, such as :tool:`condor_restart`, :tool:`condor_on`, and
    :tool:`condor_off`, to that host.

:macro-def:`MASTER_INSTANCE_LOCK[MASTER]`
    Defines the name of a file for the :tool:`condor_master` daemon to lock
    in order to prevent multiple :tool:`condor_master` s from starting. This
    is useful when using shared file systems like NFS which do not
    technically support locking in the case where the lock files reside
    on a local disk. If this macro is not defined, the default file name
    will be ``$(LOCK)/InstanceLock``. ``$(LOCK)`` can instead be defined
    to specify the location of all lock files, not just the
    :tool:`condor_master` 's ``InstanceLock``. If ``$(LOCK)`` is undefined,
    then the master log itself is locked.

:macro-def:`ADD_WINDOWS_FIREWALL_EXCEPTION[MASTER]`
    When set to ``False``, the :tool:`condor_master` will not automatically
    add HTCondor to the Windows Firewall list of trusted applications.
    Such trusted applications can accept incoming connections without
    interference from the firewall. This only affects machines running
    Windows XP SP2 or higher. The default is ``True``.

:macro-def:`WINDOWS_FIREWALL_FAILURE_RETRY[MASTER]`
    An integer value (default value is 2) that represents the number of
    times the :tool:`condor_master` will retry to add firewall exceptions.
    When a Windows machine boots up, HTCondor starts up by default as
    well. Under certain conditions, the :tool:`condor_master` may have
    difficulty adding exceptions to the Windows Firewall because of a
    delay in other services starting up. Examples of services that may
    possibly be slow are the SharedAccess service, the Netman service,
    or the Workstation service. This configuration variable allows
    administrators to set the number of times (once every 5 seconds)
    that the :tool:`condor_master` will retry to add firewall exceptions. A
    value of 0 means that HTCondor will retry indefinitely.

:macro-def:`USE_PROCESS_GROUPS[MASTER]`
    A boolean value that defaults to ``True``. When ``False``, HTCondor
    daemons on Unix machines will not create new sessions or process
    groups. HTCondor uses processes groups to help it track the
    descendants of processes it creates. This can cause problems when
    HTCondor is run under another job execution system.

:macro-def:`DISCARD_SESSION_KEYRING_ON_STARTUP[MASTER]`
    A boolean value that defaults to ``True``. When ``True``, the
    :tool:`condor_master` daemon will replace the kernel session keyring it
    was invoked with with a new keyring named ``htcondor``. Various
    Linux system services, such as OpenAFS and eCryptFS, use the kernel
    session keyring to hold passwords and authentication tokens. By
    replacing the keyring on start up, the :tool:`condor_master` ensures
    these keys cannot be unintentionally obtained by user jobs.

:macro-def:`ENABLE_KERNEL_TUNING[MASTER]`
    Relevant only to Linux platforms, a boolean value that defaults to
    ``True``. When ``True``, the :tool:`condor_master` daemon invokes the
    kernel tuning script specified by configuration variable
    :macro:`LINUX_KERNEL_TUNING_SCRIPT` once as root when the
    :tool:`condor_master` daemon starts up.

:macro-def:`KERNEL_TUNING_LOG[MASTER]`
    A string value that defaults to ``$(LOG)/KernelTuningLog``. If the
    kernel tuning script runs, its output will be logged to this file.

:macro-def:`LINUX_KERNEL_TUNING_SCRIPT[MASTER]`
    A string value that defaults to ``$(LIBEXEC)/linux_kernel_tuning``.
    This is the script that the :tool:`condor_master` runs to tune the kernel
    when :macro:`ENABLE_KERNEL_TUNING` is ``True``.

condor_startd Configuration File Macros
---------------------------------------

:index:`condor_startd configuration variables<single: condor_startd configuration variables; configuration>`

.. note::

    If you are running HTCondor on a multi-CPU machine, be sure to
    also read :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy
    configuration` which describes how to set up and configure HTCondor on
    multi-core machines.

These settings control general operation of the *condor_startd*.
Examples using these configuration macros, as well as further
explanation is found in the :doc:`/admin-manual/ep-policy-configuration`
section.

:macro-def:`START[STARTD]`
    A boolean expression that, when ``True``, indicates that the machine
    is willing to start running an HTCondor job. :macro:`START` is considered
    when the *condor_negotiator* daemon is considering evicting the job
    to replace it with one that will generate a better rank for the
    *condor_startd* daemon, or a user with a higher priority.

:macro-def:`DEFAULT_DRAINING_START_EXPR[STARTD]`
    An alternate :macro:`START` expression to use while draining when the
    drain command is sent without a ``-start`` argument.  When this
    configuration parameter is not set and the drain command does not specify
    a ``-start`` argument, :macro:`START` will have the value ``undefined``
    and ``Requirements`` will be ``false`` while draining. This will prevent new
    jobs from matching.  To allow evictable jobs to match while draining,
    set this to an expression that matches only those jobs.

:macro-def:`SUSPEND[STARTD]`
    A boolean expression that, when ``True``, causes HTCondor to suspend
    running an HTCondor job. The machine may still be claimed, but the
    job makes no further progress, and HTCondor does not generate a load
    on the machine.

:macro-def:`PREEMPT[STARTD]`
    A boolean expression that, when ``True``, causes HTCondor to stop a
    currently running job once :macro:`MAXJOBRETIREMENTTIME` has expired.
    This expression is not evaluated if :macro:`WANT_SUSPEND` is ``True``.
    The default value is ``False``, such that preemption is disabled.

:macro-def:`WANT_HOLD[STARTD]`
    A boolean expression that defaults to ``False``. When ``True`` and
    the value of :macro:`PREEMPT` becomes ``True`` and :macro:`WANT_SUSPEND` is
    ``False`` and :macro:`MAXJOBRETIREMENTTIME` has expired, the job is put
    on hold for the reason (optionally) specified by the variables
    :macro:`WANT_HOLD_REASON` and :macro:`WANT_HOLD_SUBCODE`. As usual, the job
    owner may specify
    :subcom:`periodic_release[and WANT_HOLD]`
    and/or :subcom:`periodic_remove[and WANT_HOLD]`
    expressions to react to specific hold states automatically. The
    attribute :ad-attr:`HoldReasonCode` in the job ClassAd is set to the value
    21 when :macro:`WANT_HOLD` is responsible for putting the job on hold.

    Here is an example policy that puts jobs on hold that use too much
    virtual memory:

    .. code-block:: condor-config

        VIRTUAL_MEMORY_AVAILABLE_MB = (VirtualMemory*0.9)
        MEMORY_EXCEEDED = ImageSize/1024 > $(VIRTUAL_MEMORY_AVAILABLE_MB)
        PREEMPT = ($(PREEMPT)) || ($(MEMORY_EXCEEDED))
        WANT_SUSPEND = ($(WANT_SUSPEND)) && ($(MEMORY_EXCEEDED)) =!= TRUE
        WANT_HOLD = ($(MEMORY_EXCEEDED))
        WANT_HOLD_REASON = \
           ifThenElse( $(MEMORY_EXCEEDED), \
                       "Your job used too much virtual memory.", \
                       undefined )

:macro-def:`WANT_HOLD_REASON[STARTD]`
    An expression that defines a string utilized to set the job ClassAd
    attribute :ad-attr:`HoldReason` when a job is put on hold due to
    :macro:`WANT_HOLD`. If not defined or if the expression evaluates to
    ``Undefined``, a default hold reason is provided.

:macro-def:`WANT_HOLD_SUBCODE[STARTD]`
    An expression that defines an integer value utilized to set the job
    ClassAd attribute :ad-attr:`HoldReasonSubCode` when a job is put on hold
    due to :macro:`WANT_HOLD`. If not defined or if the expression evaluates
    to ``Undefined``, the value is set to 0. Note that
    :ad-attr:`HoldReasonCode` is always set to 21.

:macro-def:`CONTINUE[STARTD]`
    A boolean expression that, when ``True``, causes HTCondor to
    continue the execution of a suspended job.

:macro-def:`KILL[STARTD]`
    A boolean expression that, when ``True``, causes HTCondor to
    immediately stop the execution of a vacating job, without delay. The
    job is hard-killed, so any attempt by the job to clean
    up will be aborted. This expression should normally be ``False``.
    When desired, it may be used to abort the graceful shutdown of a job
    earlier than the limit imposed by :macro:`MachineMaxVacateTime`.

:macro-def:`PERIODIC_CHECKPOINT[STARTD]`
    A boolean expression that, when ``True``, causes HTCondor to
    initiate a checkpoint of the currently running job.  This setting
    applies to vm universe jobs that have set
    :subcom:`vm_checkpoint[and PERIODIC_CHECKPOINT]`
    to ``True`` in the submit description file.

:macro-def:`RANK[STARTD]`
    A floating point value that HTCondor uses to compare potential jobs.
    A larger value for a specific job ranks that job above others with
    lower values for :macro:`RANK`.

:macro-def:`MaxJobRetirementTime[START]`
    An expression evaluated in the context of the slot ad and the Job
    ad that should evaluate to a number of seconds.  This is the
    number of seconds after a running job has been requested to
    be preempted or evicted, that it is allowed to remain
    running in the Preempting/Retiring state.  It can Be
    thought of as a "minimum guaranteed runtime".

:macro-def:`ADVERTISE_PSLOT_ROLLUP_INFORMATION[STARTD]`
    A boolean value that defaults to ``True``, causing the
    *condor_startd* to advertise ClassAd attributes that may be used in
    partitionable slot preemption. The attributes are

    -  :ad-attr:`ChildAccountingGroup`
    -  :ad-attr:`ChildActivity`
    -  ``ChildCPUs``
    -  :ad-attr:`ChildCurrentRank`
    -  :ad-attr:`ChildEnteredCurrentState`
    -  :ad-attr:`ChildMemory`
    -  :ad-attr:`ChildName`
    -  :ad-attr:`ChildRemoteOwner`
    -  :ad-attr:`ChildRemoteUser`
    -  :ad-attr:`ChildRetirementTimeRemaining`
    -  :ad-attr:`ChildState`
    -  :ad-attr:`PslotRollupInformation`

:macro-def:`STARTD_PARTITIONABLE_SLOT_ATTRS[STARTD]`
    A list of additional from the above default attributes from dynamic
    slots that will be rolled up into a list attribute in their parent
    partitionable slot, prefixed with the name Child.

:macro-def:`WANT_SUSPEND[STARTD]`
    A boolean expression that, when ``True``, tells HTCondor to evaluate
    the :macro:`SUSPEND` expression to decide whether to suspend a running
    job. When ``True``, the :macro:`PREEMPT` expression is not evaluated.
    When not explicitly set, the *condor_startd* exits with an error.
    When explicitly set, but the evaluated value is anything other than
    ``True``, the value is utilized as if it were ``False``.

:macro-def:`WANT_VACATE[STARTD]`
    A boolean expression that, when ``True``, defines that a preempted
    HTCondor job is to be vacated, instead of killed. This means the job
    will be soft-killed and given time to clean up. The amount of time
    given depends on :macro:`MachineMaxVacateTime` and :macro:`KILL`.
    The default value is ``True``.

:macro-def:`IS_OWNER[STARTD]`
    A boolean expression that determines when a machine ad should enter
    the :ad-attr:`Owner` state. While in the :ad-attr:`Owner` state, the machine ad
    will not be matched to any jobs. The default value is ``False``
    (never enter :ad-attr:`Owner` state). Job ClassAd attributes should not be
    used in defining :macro:`IS_OWNER`, as they would be ``Undefined``.

:macro-def:`STARTD_HISTORY[STARTD]`
    A file name where the *condor_startd* daemon will maintain a job
    history file in an analogous way to that of the history file defined
    by the configuration variable :macro:`HISTORY`. It will be rotated in the
    same way, and the same parameters that apply to the :macro:`HISTORY` file
    rotation apply to the *condor_startd* daemon history as well. This
    can be read with the :tool:`condor_history` command by passing the name
    of the file to the -file option of :tool:`condor_history`.

    .. code-block:: console

        $ condor_history -file `condor_config_val LOG`/startd_history

:macro-def:`STARTER[STARTD]`
    This macro holds the full path to the *condor_starter* binary that
    the *condor_startd* should spawn. It is normally defined relative
    to ``$(SBIN)``.

:macro-def:`KILLING_TIMEOUT[STARTD]`
    The amount of time in seconds that the *condor_startd* should wait
    after sending a fast shutdown request to *condor_starter* before
    forcibly killing the job and *condor_starter*. The default value is
    30 seconds.

:macro-def:`POLLING_INTERVAL[STARTD]`
    When a *condor_startd* enters the claimed state, this macro
    determines how often the state of the machine is polled to check the
    need to suspend, resume, vacate or kill the job. It is defined in
    terms of seconds and defaults to 5.

:macro-def:`UPDATE_INTERVAL[STARTD]`
    Determines how often the *condor_startd* should send a ClassAd
    update to the *condor_collector*. The *condor_startd* also sends
    update on any state or activity change, or if the value of its
    :macro:`START` expression changes. See
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    on *condor_startd* states, *condor_startd* Activities, and
    *condor_startd* :macro:`START` expression for details on states,
    activities, and the :macro:`START` expression. This macro is defined in
    terms of seconds and defaults to 300 (5 minutes).

:macro-def:`UPDATE_OFFSET[STARTD]`
    An integer value representing the number of seconds of delay that
    the *condor_startd* should wait before sending its initial update.
    The default is 0. The time of all other periodic updates sent after
    this initial update is determined by ``$(UPDATE_INTERVAL)``. Thus,
    the first update will be sent after ``$(UPDATE_OFFSET)`` seconds,
    and the second update will be sent after ``$(UPDATE_OFFSET)`` +
    ``$(UPDATE_INTERVAL)``. This is useful when used in conjunction with
    the ``$RANDOM_INTEGER()`` macro for large pools, to spread out the
    updates sent by a large number of *condor_startd* daemons when all
    of the machines are started at the same time.
    The example configuration

    .. code-block:: condor-config

          startd.UPDATE_INTERVAL = 300
          startd.UPDATE_OFFSET   = $RANDOM_INTEGER(0,300)


    causes the initial update to occur at a random number of seconds
    falling between 0 and 300, with all further updates occurring at
    fixed 300 second intervals following the initial update.

:macro-def:`MachineMaxVacateTime[STARTD]`
    An integer expression representing the number of seconds the machine
    is willing to wait for a job that has been soft-killed to gracefully
    shut down. The default value is 600 seconds (10 minutes). This
    expression is evaluated when the job starts running. The job may
    adjust the wait time by setting :ad-attr:`JobMaxVacateTime`. If the job's
    setting is less than the machine's, the job's specification is used.
    If the job's setting is larger than the machine's, the result
    depends on whether the job has any excess retirement time. If the
    job has more retirement time left than the machine's maximum vacate
    time setting, then retirement time will be converted into vacating
    time, up to the amount of :ad-attr:`JobMaxVacateTime`. The :macro:`KILL`
    expression may be used to abort the graceful shutdown of the job
    at any time. At the time when the job is preempted, the
    :macro:`WANT_VACATE` expression may be used to skip the graceful
    shutdown of the job.

:macro-def:`MAXJOBRETIREMENTTIME[STARTD]`
    When the *condor_startd* wants to evict a job, a job which has run
    for less than the number of seconds specified by this expression
    will not be hard-killed. The *condor_startd* will wait for the job
    to finish or to exceed this amount of time, whichever comes sooner.
    Time spent in suspension does not count against the job. The default
    value of 0 (when the configuration variable is not present) means
    that the job gets no retirement time. If the job vacating policy
    grants the job X seconds of vacating time, a preempted job will be
    soft-killed X seconds before the end of its retirement time, so that
    hard-killing of the job will not happen until the end of the
    retirement time if the job does not finish shutting down before
    then. Note that in peaceful shutdown mode of the *condor_startd*,
    retirement time is treated as though infinite. In graceful shutdown
    mode, the job will not be preempted until the configured retirement
    time expires or :macro:`SHUTDOWN_GRACEFUL_TIMEOUT` expires. In fast shutdown
    mode, retirement time is ignored. See :macro:`MAXJOBRETIREMENTTIME` in
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    for further explanation.

    By default the *condor_negotiator* will not match jobs to a slot
    with retirement time remaining. This behavior is controlled by
    :macro:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION`.

    There is no default value for this configuration variable.

:macro-def:`CLAIM_WORKLIFE[STARTD]`
    This expression specifies the number of seconds after which a claim
    will stop accepting additional jobs. The default is 1200, which is
    20 minutes. Once the *condor_negotiator* gives a *condor_schedd* a
    claim to a slot, the *condor_schedd* will keep running jobs on that
    slot as long as it has more jobs with matching requirements, and
    :macro:`CLAIM_WORKLIFE` has not expired, and it is not preempted. Once
    :macro:`CLAIM_WORKLIFE` expires, any existing job may continue to run as
    usual, but once it finishes or is preempted, the claim is closed.
    When :macro:`CLAIM_WORKLIFE` is -1, this is treated as an infinite claim
    work life, so claims may be held indefinitely (as long as they are
    not preempted and the user does not run out of jobs, of course). A
    value of 0 has the effect of not allowing more than one job to run
    per claim, since it immediately expires after the first job starts
    running.

:macro-def:`MAX_CLAIM_ALIVES_MISSED[STARTD]`
    The *condor_schedd* sends periodic updates to each *condor_startd*
    as a keep alive (see the description of :macro:`ALIVE_INTERVAL`
    If the *condor_startd* does not receive any keep alive messages, it
    assumes that something has gone wrong with the *condor_schedd* and
    that the resource is not being effectively used. Once this happens,
    the *condor_startd* considers the claim to have timed out, it
    releases the claim, and starts advertising itself as available for
    other jobs. Because these keep alive messages are sent via UDP, they
    are sometimes dropped by the network. Therefore, the
    *condor_startd* has some tolerance for missed keep alive messages,
    so that in case a few keep alives are lost, the *condor_startd*
    will not immediately release the claim. This setting controls how
    many keep alive messages can be missed before the *condor_startd*
    considers the claim no longer valid. The default is 6.

:macro-def:`MATCH_TIMEOUT[STARTD]`
    The amount of time a startd will stay in Matched state without
    getting a claim request before reverting back to Unclaimed state.
    Defaults to 120 seconds.

:macro-def:`STARTD_HAS_BAD_UTMP[STARTD]`
    When the *condor_startd* is computing the idle time of all the
    users of the machine (both local and remote), it checks the ``utmp``
    file to find all the currently active ttys, and only checks access
    time of the devices associated with active logins. Unfortunately, on
    some systems, ``utmp`` is unreliable, and the *condor_startd* might
    miss keyboard activity by doing this. So, if your ``utmp`` is
    unreliable, set this macro to ``True`` and the *condor_startd* will
    check the access time on all tty and pty devices.

:macro-def:`CONSOLE_DEVICES[STARTD]`
    This macro allows the *condor_startd* to monitor console (keyboard
    and mouse) activity by checking the access times on special files in
    ``/dev``. Activity on these files shows up as :ad-attr:`ConsoleIdle` time
    in the *condor_startd* 's ClassAd. Give a comma-separated list of
    the names of devices considered the console, without the ``/dev/``
    portion of the path name. The defaults vary from platform to
    platform, and are usually correct.

    One possible exception to this is on Linux, where we use "mouse" as
    one of the entries. Most Linux installations put in a soft link from
    ``/dev/mouse`` that points to the appropriate device (for example,
    ``/dev/psaux`` for a PS/2 bus mouse, or ``/dev/tty00`` for a serial
    mouse connected to com1). However, if your installation does not
    have this soft link, you will either need to put it in (you will be
    glad you did), or change this macro to point to the right device.

    Unfortunately, modern versions of Linux do not update the access
    time of device files for USB devices. Thus, these files cannot be be
    used to determine when the console is in use. Instead, use the
    *condor_kbdd* daemon, which gets this information by connecting to
    the X server.

:macro-def:`KBDD_BUMP_CHECK_SIZE[STARTD]`
    The number of pixels that the mouse can move in the X and/or Y
    direction, while still being considered a bump, and not keyboard
    activity. If the movement is greater than this bump size then the
    move is not a transient one, and it will register as activity. The
    default is 16, and units are pixels. Setting the value to 0
    effectively disables bump testing.

:macro-def:`KBDD_BUMP_CHECK_AFTER_IDLE_TIME[STARTD]`
    The number of seconds of keyboard idle time that will pass before
    bump testing begins. The default is 15 minutes.

:macro-def:`STARTD_JOB_ATTRS[STARTD]`
    When the machine is claimed by a remote user, the *condor_startd*
    can also advertise arbitrary attributes from the job ClassAd in the
    machine ClassAd. List the attribute names to be advertised.

    .. note::

        Since these are already ClassAd expressions, do not do anything
        unusual with strings. By default, the job ClassAd attributes
        JobUniverse, NiceUser, ExecutableSize and ImageSize are advertised
        into the machine ClassAd.

:macro-def:`STARTD_LATCH_EXPRS[STARTD]`
    Each time a slot is created, activated, or when periodic STARTD policy
    is evaluated HTCondor will evaluate expressions whose names are listed
    in this configuration variable.  If the evaluated value can be converted
    to an integer, and the value of the integer changes, the time of the change
    will be published.

    This macro should be a list of the names of configuration variables that contain
    an expression to be evaluated, the name of the configuration variable will be
    treated as the base name of attributes published for the macro. Thus expressions listed
    behave like :macro:`STARTD_ATTRS` with the additional behavior the most recent evaluated
    value will be advertised as ``<name>Value`` and the time the value changed will be
    advertised as ``<name>Time``.  Entries in this list can also be the names of standard
    slot attributes like ``NumDynamicSlots``, in which case the change time will be advertised
    but the evaluated value will not be advertised, since that would be redundant.

    It is not an error when the result of evaluation is undefined, in that case the STARTD
    will remember the time that the value became undefined but not advertise the time. If
    the evaluated value becomes defined again, the time that it changed from undefined to
    the new value will again be advertised.

    Example:

    .. code-block:: condor-config

          STARTD_LATCH_EXPRS = HalfFull NumDynamicSlots
          HalfFull = Cpus < (TotalSlotCPUs/2) || Memory < (TotalSlotMemory/2)

    For the configuration fragment above, the STARTD will advertise ``HalfFull`` as an expression,
    along with the last evaluated value of that expression as ``HalfFullValue``, and the time
    it changed to that value as ``HalfFullTime``.  It will also advertise the time that the number
    of dynamic slots changed to its current value as ``NumDynamicSlotsTime``. It will not advertise
    a ``NumDynamicSlotsValue`` because the ``<name>Value`` attribute is only advertised if ``<name>``
    is an expression in the configuration that is not simple literal value.


:macro-def:`STARTD_ATTRS[STARTD]`
    This macro is described in :macro:`<SUBSYS>_ATTRS`.

:macro-def:`SLOT<N>_STARTD_ATTRS[STARTD]`
    Like the above, but only applies to the numbered slot.

:macro-def:`STARTD_DEBUG[STARTD]`
    This macro (and other settings related to debug logging in the
    *condor_startd*) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`STARTD_ADDRESS_FILE[STARTD]`
    This macro is described in :macro:`<SUBSYS>_ADDRESS_FILE`

:macro-def:`ENABLE_STARTD_DAEMON_AD[STARTD]`
    Enable a daemon ad for the *condor_startd* that is separate from the slot ads used for matchmaking
    and running jobs.  Allowed values are True, False, and Auto.
    When the value is True, the *condor_startd* will advertise ``Slot`` ads describing the slot state
    and ``StartDaemon`` ads describing the overall state of the daemon.
    When the value is False, the *condor_startd* will advertise only ``Machine`` ads.
    When the value is Auto, the *condor_startd* will advertise ``Slot`` and ``StartDaemon`` ads to
    collectors that are HTCondor version 23.2 or later, and ``Machine`` ads to older collectors.
    The default value is Auto.

:macro-def:`STARTD_SHOULD_WRITE_CLAIM_ID_FILE[STARTD]`
    The *condor_startd* can be configured to write out the ``ClaimId``
    for the next available claim on all slots to separate files. This
    boolean attribute controls whether the *condor_startd* should write
    these files. The default value is ``True``.

:macro-def:`STARTD_CLAIM_ID_FILE[STARTD]`
    This macro controls what file names are used if the above
    :macro:`STARTD_SHOULD_WRITE_CLAIM_ID_FILE` is true. By default, HTCondor
    will write the ClaimId into a file in the
    ``$(LOG)``\ :index:`LOG` directory called
    ``.startd_claim_id.slotX``, where X is the value of :ad-attr:`SlotID`, the
    integer that identifies a given slot on the system, or 1 on a
    single-slot machine. If you define your own value for this setting,
    you should provide a full path, and HTCondor will automatically
    append the .slotX portion of the file name.

:macro-def:`STARTD_PRINT_ADS_ON_SHUTDOWN[STARTD]`
    The *condor_startd* can be configured to write out the slot ads into
    the daemon's log file as it is shutting down.  This is a boolean and the
    default value is ``False``.

:macro-def:`STARTD_PRINT_ADS_FILTER[STARTD]`
    When :macro:`STARTD_PRINT_ADS_ON_SHUTDOWN` above is set to ``True``, this
    macro can list which specific types of ads will get written to the log.
    The possible values are ``static```, ``partitionable``, and ``dynamic``.
    The list is comma separated and the default is to print all three types of
    ads.

:macro-def:`NUM_CPUS[STARTD]`
    An integer value, which can be used to lie to the *condor_startd*
    daemon about how many CPUs a machine has. When set, it overrides the
    value determined with HTCondor's automatic computation of the number
    of CPUs in the machine. Lying in this way can allow multiple
    HTCondor jobs to run on a single-CPU machine, by having that machine
    treated like a multi-core machine with multiple CPUs, which could
    have different HTCondor jobs running on each one. Or, a multi-core
    machine may advertise more slots than it has CPUs. However, lying in
    this manner will hurt the performance of the jobs, since now
    multiple jobs will run on the same CPU, and the jobs will compete
    with each other. The option is only meant for people who
    specifically want this behavior and know what they are doing. It is
    disabled by default.

    The default value is
    ``$(DETECTED_CPUS_LIMIT)``\ :index:`DETECTED_CPUS_LIMIT`.

    The *condor_startd* only takes note of the value of this
    configuration variable on start up, therefore it cannot be changed
    with a simple reconfigure. To change this, restart the
    *condor_startd* daemon for the change to take effect. The command
    will be

    .. code-block:: console

          $ condor_restart -startd

:macro-def:`MAX_NUM_CPUS[STARTD]`
    An integer value used as a ceiling for the number of CPUs detected
    by HTCondor on a machine. This value is ignored if :macro:`NUM_CPUS` is
    set. If set to zero, there is no ceiling. If not defined, the
    default value is zero, and thus there is no ceiling.

    Note that this setting cannot be changed with a simple reconfigure,
    either by sending a SIGHUP or by using the :tool:`condor_reconfig`
    command. To change this, restart the *condor_startd* daemon for the
    change to take effect. The command will be

    .. code-block:: console

          $ condor_restart -startd

:macro-def:`COUNT_HYPERTHREAD_CPUS[STARTD]`
    This configuration variable controls how HTCondor sees
    hyper-threaded processors. When set to the default value of
    ``True``, it includes virtual CPUs in the default value of
    ``DETECTED_CPUS``. On dedicated cluster nodes, counting virtual CPUs
    can sometimes improve total throughput at the expense of individual
    job speed. However, counting them on desktop workstations can
    interfere with interactive job performance.

:macro-def:`MEMORY[STARTD]`
    Normally, HTCondor will automatically detect the amount of physical
    memory available on your machine. Define :macro:`MEMORY` to tell HTCondor
    how much physical memory (in MB) your machine has, overriding the
    value HTCondor computes automatically. The actual amount of memory
    detected by HTCondor is always available in the pre-defined
    configuration macro :macro:`DETECTED_MEMORY`.

:macro-def:`RESERVED_MEMORY[STARTD]`
    How much memory (in MB) would you like reserved from HTCondor? By default,
    HTCondor considers all the physical memory of your machine as
    available to be used by HTCondor jobs. If :macro:`RESERVED_MEMORY` is
    defined, HTCondor subtracts it from the amount of memory it
    advertises as available.

:macro-def:`STARTD_NAME[STARTD]`
    Used to give an alternative value to the ``Name`` attribute in the
    *condor_startd* 's ClassAd. This esoteric configuration macro
    might be used in the situation where there are two *condor_startd*
    daemons running on one machine, and each reports to the same
    *condor_collector*. Different names will distinguish the two
    daemons. See the description of :macro:`MASTER_NAME`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`RUNBENCHMARKS[STARTD]`
    A boolean expression that specifies whether to run benchmarks. When
    the machine is in the Unclaimed state and this expression evaluates
    to ``True``, benchmarks will be run. If :macro:`RUNBENCHMARKS` is
    specified and set to anything other than ``False``, additional
    benchmarks will be run once, when the *condor_startd* starts. To
    disable start up benchmarks, set ``RunBenchmarks`` to ``False``.

:macro-def:`DedicatedScheduler[STARTD]`
    A string that identifies the dedicated scheduler this machine is
    managed by.
    :ref:`admin-manual/ap-policy-configuration:dedicated scheduling`
    details the use of a dedicated scheduler.

:macro-def:`STARTD_NOCLAIM_SHUTDOWN[STARTD]`
    The number of seconds to run without receiving a claim before
    shutting HTCondor down on this machine. Defaults to unset, which
    means to never shut down. This is primarily intended to facilitate
    glidein; use in other situations is not recommended.

:macro-def:`STARTD_PUBLISH_WINREG[STARTD]`
    A string containing a semicolon-separated list of Windows registry
    key names. For each registry key, the contents of the registry key
    are published in the machine ClassAd. All attribute names are
    prefixed with ``WINREG_``. The remainder of the attribute name is
    formed in one of two ways. The first way explicitly specifies the
    name within the list with the syntax

    .. code-block:: condor-config

          STARTD_PUBLISH_WINREG = AttrName1 = KeyName1; AttrName2 = KeyName2

    The second way of forming the attribute name derives the attribute
    names from the key names in the list. The derivation uses the last
    three path elements in the key name and changes each illegal
    character to an underscore character. Illegal characters are
    essentially any non-alphanumeric character. In addition, the percent
    character (%) is replaced by the string ``Percent``, and the string
    ``/sec`` is replaced by the string ``_Per_Sec``.

    HTCondor expects that the hive identifier, which is the first
    element in the full path given by a key name, will be the valid
    abbreviation. Here is a list of abbreviations:

    - ``HKLM`` is the abbreviation for ``HKEY_LOCAL_MACHINE``
    - ``HKCR`` is the abbreviation for ``HKEY_CLASSES_ROOT``
    - ``HKCU`` is the abbreviation for ``HKEY_CURRENT_USER``
    - ``HKPD`` is the abbreviation for ``HKEY_PERFORMANCE_DATA``
    - ``HKCC`` is the abbreviation for ``HKEY_CURRENT_CONFIG``
    - ``HKU`` is the abbreviation for ``HKEY_USERS``

    The ``HKPD`` key names are unusual, as they are not shown in
    *regedit*. Their values are periodically updated at the interval
    defined by :macro:`UPDATE_INTERVAL`. The others are not updated until
    :tool:`condor_reconfig` is issued.

    Here is a complete example of the configuration variable definition,

    .. code-block:: condor-config

            STARTD_PUBLISH_WINREG = HKLM\Software\Perl\BinDir; \
             BATFile_RunAs_Command = HKCR\batFile\shell\RunAs\command; \
             HKPD\Memory\Available MBytes; \
             BytesAvail = HKPD\Memory\Available Bytes; \
             HKPD\Terminal Services\Total Sessions; \
             HKPD\Processor\% Idle Time; \
             HKPD\System\Processes

    which generates the following portion of a machine ClassAd:

    .. code-block:: condor-classad

          WINREG_Software_Perl_BinDir = "C:\Perl\bin\perl.exe"
          WINREG_BATFile_RunAs_Command = "%SystemRoot%\System32\cmd.exe /C \"%1\" %*"
          WINREG_Memory_Available_MBytes = 5331
          WINREG_BytesAvail = 5590536192.000000
          WINREG_Terminal_Services_Total_Sessions = 2
          WINREG_Processor_Percent_Idle_Time = 72.350384
          WINREG_System_Processes = 166

:macro-def:`MOUNT_UNDER_SCRATCH[STARTD]`
    A ClassAd expression, which when evaluated in the context of the job
    and machine ClassAds, evaluates to a string that contains a comma separated list
    of directories. For each directory in the list, HTCondor creates a
    directory in the job's temporary scratch directory with that name,
    and makes it available at the given name using bind mounts. This is
    available on Linux systems which provide bind mounts and per-process
    tree mount tables, such as Red Hat Enterprise Linux 5. A bind mount
    is like a symbolic link, but is not globally visible to all
    processes. It is only visible to the job and the job's child
    processes. As an example:

    .. code-block:: condor-config

          MOUNT_UNDER_SCRATCH = ifThenElse(TARGET.UtsnameSysname =?= "Linux", "/tmp,/var/tmp", "")

    If the job is running on a Linux system, it will see the usual
    ``/tmp`` and ``/var/tmp`` directories, but when accessing files via
    these paths, the system will redirect the access. The resultant
    files will actually end up in directories named ``tmp`` or
    ``var/tmp`` under the job's temporary scratch directory. This is
    useful, because the job's scratch directory will be cleaned up after
    the job completes, two concurrent jobs will not interfere with each
    other, and because jobs will not be able to fill up the real
    ``/tmp`` directory. Another use case might be for home directories,
    which some jobs might want to write to, but that should be cleaned
    up after each job run. The default value is ``"/tmp,/var/tmp"``.

    If the job's execute directory is encrypted, ``/tmp`` and
    ``/var/tmp`` are automatically added to :macro:`MOUNT_UNDER_SCRATCH` when
    the job is run (they will not show up if :macro:`MOUNT_UNDER_SCRATCH` is
    examined with :tool:`condor_config_val`).

    .. note::

        The MOUNT_UNDER_SCRATCH mounts do not take place until the
        PreCmd of the job, if any, completes.
        (See :ref:`classad-attributes/job-classad-attributes:job classad attributes`
        for information on PreCmd.)

    Also note that, if :macro:`MOUNT_UNDER_SCRATCH` is defined, it must
    either be a ClassAd string (with double-quotes) or an expression
    that evaluates to a string.

    For Docker Universe jobs, any directories that are mounted under
    scratch are also volume mounted on the same paths inside the
    container. That is, any reads or writes to files in those
    directories goes to the host filesystem under the scratch directory.
    This is useful if a container has limited space to grow a filesystem.

:macro-def:`MOUNT_PRIVATE_DEV_SHM[STARTD]`
    This boolean value, which defaults to ``True`` tells the *condor_starter*
    to make /dev/shm on Linux private to each job.  When private, the
    starter removes any files from the private /dev/shm at job exit time.

The following macros control if the *condor_startd* daemon should create a
custom filesystem for the job's scratch directory. This allows HTCondor to
prevent the job from using more scratch space than provisioned.

:macro-def:`STARTD_ENFORCE_DISK_LIMITS[STARTD]`
    This boolean value, which is only evaluated on Linux systems, tells
    the *condor_startd* whether to make an ephemeral filesystem for the
    scratch execute directory for jobs.  The default is ``False``. This
    should only be set to true on HTCondor installations that have root
    privilege. When ``true``, you must set :macro:`LVM_VOLUME_GROUP_NAME`
    and :macro:`LVM_THINPOOL_NAME`, or alternatively set :macro:`LVM_BACKING_FILE`.
    If ``true`` and required pre-made LVM components are not defined
    then HTCondor will default to using the :macro:`LVM_BACKING_FILE`.

.. note::
    :macro:`LVM_THINPOOL_NAME` only needs to be set if Startd disk enforcement
    is using thin provisioning for logical volumes. This behavior is dictated
    by :macro:`LVM_USE_THIN_PROVISIONING`.

:macro-def:`LVM_USE_THIN_PROVISIONING[STARTD]`
    A boolean value that defaults to ``True``. When ``True`` HTCondor will create
    thin provisioned logical volumes from a backing thin pool logical volume for
    ephemeral execute directories. If ``False`` then HTCondor will create linear
    logical volumes for ephemeral execute directories.

:macro-def:`LVM_THINPOOL_NAME[STARTD]`
    A string value that represents an external pre-made Linux LVM thin-pool
    type logical volume to be used as a backing pool for ephemeral execute
    directories. This setting only matters when :macro:`STARTD_ENFORCE_DISK_LIMITS`
    is ``True``, and HTCondor has root privilege. This option does not have
    a default value.

:macro-def:`LVM_VOLUME_GROUP_NAME[STARTD]`
    A string value that represents an external pre-made Linux LVM volume
    group to be used to create logical volumes for ephemeral execute
    directories. This setting only matters when :macro:`STARTD_ENFORCE_DISK_LIMITS`
    is True, and HTCondor has root privilege. This option does not have a
    default value.

:macro-def:`LVM_BACKING_FILE[STARTD]`
    A string valued parameter that defaults to ``$(SPOOL)/startd_disk.img``.
    If a rootly HTCondor does not have pre-made Linux LVM components configured,
    a single large file will be used as the backing store for ephemeral file
    systems for execute directories. This parameter should be set to the path of
    a large, pre-created file to hold the blocks these filesystems are created from.

:macro-def:`LVM_BACKING_FILE_SIZE_MB[STARTD]`
    An integer value that represents the size in Megabytes to allocate for
    the ephemeral backing file described by :macro:`LVM_BACKING_FILE`. This
    option default to 10240 (10GB).

:macro-def:`LVM_THIN_LV_EXTRA_SIZE_MB[STARTD]`
    An integer value that represents size in Megabytes to be added onto the size
    of a thinly provisioned logical volume for an ephemeral execute directory.
    This option only applies when :macro:`LVM_USE_THIN_PROVISIONING` is ``True``.
    This extra space over will over provision the backing thin pool while providing
    a buffer to better catch over use of disk before a job gets ``ENOSPC`` errors.
    The default value is 2000 (2GB).

:macro-def:`LVM_HIDE_MOUNT[STARTD]`
    A boolean value that defaults to ``false``.  When LVM ephemeral
    filesystems are enabled (as described above), if this knob is
    set to ``true``, the mount will only be visible to the job and the
    starter.  Any process in any other process tree will not be able
    to see the mount.  Setting this to true breaks Docker universe.

The following macros control if the *condor_startd* daemon should
perform backfill computations whenever resources would otherwise be
idle. See :ref:`admin-manual/ep-policy-configuration:configuring
htcondor for running backfill jobs` for details.

:macro-def:`ENABLE_BACKFILL[STARTD]`
    A boolean value that, when ``True``, indicates that the machine is
    willing to perform backfill computations when it would otherwise be
    idle. This is not a policy expression that is evaluated, it is a
    simple ``True`` or ``False``. This setting controls if any of the
    other backfill-related expressions should be evaluated. The default
    is ``False``.

:macro-def:`BACKFILL_SYSTEM[STARTD]`
    A string that defines what backfill system to use for spawning and
    managing backfill computations. Currently, the only supported value
    for this is ``"BOINC"``, which stands for the Berkeley Open
    Infrastructure for Network Computing. See
    `http://boinc.berkeley.edu <http://boinc.berkeley.edu>`_ for more
    information about BOINC. There is no default value, administrators
    must define this.

:macro-def:`START_BACKFILL[STARTD]`
    A boolean expression that is evaluated whenever an HTCondor resource
    is in the Unclaimed/Idle state and the :macro:`ENABLE_BACKFILL`
    expression is ``True``. If :macro:`START_BACKFILL` evaluates to ``True``,
    the machine will enter the Backfill state and attempt to spawn a
    backfill computation. This expression is analogous to the
    :macro:`START` expression that controls when an HTCondor
    resource is available to run normal HTCondor jobs. The default value
    is ``False`` (which means do not spawn a backfill job even if the
    machine is idle and :macro:`ENABLE_BACKFILL` expression is ``True``). For
    more information about policy expressions and the Backfill state,
    see :doc:`/admin-manual/ep-policy-configuration`, especially the
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    section.

:macro-def:`EVICT_BACKFILL[STARTD]`
    A boolean expression that is evaluated whenever an HTCondor resource
    is in the Backfill state which, when ``True``, indicates the machine
    should immediately kill the currently running backfill computation
    and return to the Owner state. This expression is a way for
    administrators to define a policy where interactive users on a
    machine will cause backfill jobs to be removed. The default value is
    ``False``. For more information about policy expressions and the
    Backfill state, see :doc:`/admin-manual/ep-policy-configuration`, especially the
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    section.

:macro-def:`BOINC_Arguments[STARTD]` :macro-def:`BOINC_Environment[STARTD]` :macro-def:`BOINC_Error[STARTD]` :macro-def:`BOINC_Executable[STARTD]` :macro-def:`BOINC_InitialDir[STARTD]` :macro-def:`BOINC_Output[STARTD]` :macro-def:`BOINC_Owner[STARTD]` :macro-def:`BOINC_Universe[STARTD]`
     These relate to the BOINC backfill system.

The following macros only apply to the *condor_startd* daemon when it
is running on a multi-core machine. See the
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
section for details.

:macro-def:`STARTD_RESOURCE_PREFIX[STARTD]`
    A string which specifies what prefix to give the unique HTCondor
    resources that are advertised on multi-core machines. Previously,
    HTCondor used the term virtual machine to describe these resources,
    so the default value for this setting was ``vm``. However, to avoid
    confusion with other kinds of virtual machines, such as the ones
    created using tools like VMware or Xen, the old virtual machine
    terminology has been changed, and has become the term slot.
    Therefore, the default value of this prefix is now ``slot``. If
    sites want to continue using ``vm``, or prefer something other
    ``slot``, this setting enables sites to define what string the
    *condor_startd* will use to name the individual resources on a
    multi-core machine.

:macro-def:`SLOTS_CONNECTED_TO_CONSOLE[STARTD]`
    An integer which indicates how many of the machine slots the
    *condor_startd* is representing should be "connected" to the
    console. This allows the *condor_startd* to notice console
    activity. Defaults to 0.  :macro:`use POLICY:DESKTOP` sets
    this to a very large number so that all slots will be connected.

:macro-def:`SLOTS_CONNECTED_TO_KEYBOARD[STARTD]`
    An integer which indicates how many of the machine slots the
    *condor_startd* is representing should be "connected" to the
    keyboard (for remote tty activity, as well as console activity).
    Defaults to 0.  :macro:`use POLICY:DESKTOP` sets
    this to a very large number so that all slots will be connected.

:macro-def:`DISCONNECTED_KEYBOARD_IDLE_BOOST[STARTD]`
    If there are slots not connected to either the keyboard or the
    console, the corresponding idle time reported will be the time since
    the *condor_startd* was spawned, plus the value of this macro. It
    defaults to 1200 seconds (20 minutes). We do this because if the
    slot is configured not to care about keyboard activity, we want it
    to be available to HTCondor jobs as soon as the *condor_startd*
    starts up, instead of having to wait for 15 minutes or more (which
    is the default time a machine must be idle before HTCondor will
    start a job). If you do not want this boost, set the value to 0. If
    you change your START expression to require more than 15 minutes
    before a job starts, but you still want jobs to start right away on
    some of your multi-core nodes, increase this macro's value.

:macro-def:`STARTD_SLOT_ATTRS[STARTD]`
    The list of ClassAd attribute names that should be shared across all
    slots on the same machine. This setting was formerly know as
    :macro:`STARTD_VM_ATTRS` For each attribute in the list, the attribute's value is
    taken from each slot's machine ClassAd and placed into the machine
    ClassAd of all the other slots within the machine. For example, if
    the configuration file for a 2-slot machine contains

    .. code-block:: condor-config

                STARTD_SLOT_ATTRS = State, Activity, EnteredCurrentActivity

    then the machine ClassAd for both slots will contain attributes that
    will be of the form:

    .. code-block:: condor-classad

             slot1_State = "Claimed"
             slot1_Activity = "Busy"
             slot1_EnteredCurrentActivity = 1075249233
             slot2_State = "Unclaimed"
             slot2_Activity = "Idle"
             slot2_EnteredCurrentActivity = 1075240035

The following settings control the number of slots reported for a given
multi-core host, and what attributes each one has. They are only needed
if you do not want to have a multi-core machine report to HTCondor with
a separate slot for each CPU, with all shared system resources evenly
divided among them. Please read
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
for details on how to properly configure these settings to suit your
needs.

.. note::

    You cannot
    change the number or definition of the different slot types with a reconfig. If
    you change anything related to slot provisioning,
    you must restart the *condor_startd* for the change to
    take effect (for example, using ``condor_restart -startd``).

.. note::

    Prior to version 6.9.3, any settings that included the term
    ``slot`` used to use virtual machine or ``vm``. If searching for
    information about one of these older settings, search for the
    corresponding attribute names using ``slot``, instead.

:macro-def:`MAX_SLOT_TYPES[STARTD]`
    The maximum number of different slot types. Note: this is the
    maximum number of different types, not of actual slots. Defaults to
    10. (You should only need to change this setting if you define more
    than 10 separate slot types, which would be pretty rare.)

:macro-def:`SLOT_TYPE_<N>[STARTD]`
    This setting defines a given slot type, by specifying what part of
    each shared system resource (like RAM, swap space, etc) this kind of
    slot gets. This setting has no effect unless you also define
    :macro:`NUM_SLOTS_TYPE_<N>`. N can be any integer from 1 to the value of
    ``$(MAX_SLOT_TYPES)``, such as ``SLOT_TYPE_1``. The format of this
    entry can be somewhat complex, so please refer to
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    for details on the different possibilities.

:macro-def:`SLOT_TYPE_<N>_PARTITIONABLE[STARTD]`
    A boolean variable that defaults to ``False``. When ``True``, this
    slot permits dynamic provisioning, as specified in
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`.

:macro-def:`CLAIM_PARTITIONABLE_LEFTOVERS[STARTD]`
    A boolean variable that defaults to ``True``. When ``True`` within
    the configuration for both the *condor_schedd* and the
    *condor_startd*, and the *condor_schedd* claims a partitionable
    slot, the *condor_startd* returns the slot's ClassAd and a claim id
    for leftover resources. In doing so, the *condor_schedd* can claim
    multiple dynamic slots without waiting for a negotiation cycle.

:macro-def:`ENABLE_CLAIMABLE_PARTITIONABLE_SLOTS[STARTD]`
    A boolean variable that defaults to ``False``.
    When set to ``True`` in the configuration of both the
    *condor_startd* and the *condor_schedd*, and the *condor_schedd*
    claims a partitionable slot, the partitionable slot's :ad-attr:`State` will
    change to ``Claimed`` in addition to the creation of a ``Claimed``
    dynamic slot.
    While the slot is ``Claimed``, no other *condor_schedd* is able
    to create new dynamic slots to run jobs.

:macro-def:`MAX_PARTITIONABLE_SLOT_CLAIM_TIME[STARTD]`
    An integer that indicates the maximum amount of time that a
    partitionable slot can be in the ``Claimed`` state before
    returning to the Unclaimed state, expressed in seconds.
    The default value is 3600.

:macro-def:`MACHINE_RESOURCE_NAMES[STARTD]`
    A comma and/or space separated list of resource names that represent
    custom resources specific to a machine. These resources are further
    intended to be statically divided or partitioned, and these resource
    names identify the configuration variables that define the
    partitioning. If used, custom resources without names in the list
    are ignored.

:macro-def:`STARTD_DETECT_GPUS[STARTD]`
    The arguments passed to :tool:`condor_gpu_discovery` to detect GPUs when
    the configuration does not have a GPUs resource explicity configured
    via ``MACHINE_RESOURCE_GPUS`` or  ``MACHINE_RESOURCE_INVENTORY_GPUS``.
    Use of the configuration template ``use FEATURE : GPUs`` will set
    ``MACHINE_RESOURCE_INVENTORY_GPUS`` and that will cause this configuration variable
    to be ignored.
    If the value of this configuration variable is set to ``false`` or ``0``
    or empty then automatic GPU discovery will be disabled, but a GPUs resource
    will still be defined if the configuration has ``MACHINE_RESOURCE_GPUS`` or
    ``MACHINE_RESOURCE_INVENTORY_GPUS`` or the configuration template ``use FEATURE : GPUs``.
    The default value is ``-properties $(GPU_DISCOVERY_EXTRA)``

:macro-def:`GPU_DISCOVERY_EXTRA[STARTD]`
    A string valued parameter that defaults to ``-extra``.  Used by
    :macro:`use feature:GPUs` and the default value of
    :macro:`STARTD_DETECT_GPUS` to allow you to pass additional
    command line arguments to the the :tool:`condor_gpu_discovery` tool.

:macro-def:`MACHINE_RESOURCE_<name>[STARTD]`
    An integer that specifies the quantity of or list of identifiers for
    the customized local machine resource available for an SMP machine.
    The portion of this configuration variable's name identified with
    ``<name>`` will be used to label quantities of the resource
    allocated to a slot. If a quantity is specified, the resource is
    presumed to be fungible and slots will be allocated a quantity of
    the resource but specific instances will not be identified. If a
    list of identifiers is specified the quantity is the number of
    identifiers and slots will be allocated both a quantity of the
    resource and assigned specific resource identifiers.

:macro-def:`OFFLINE_MACHINE_RESOURCE_<name>[STARTD]`
    A comma and/or space separated list of resource identifiers for any
    customized local machine resources that are currently offline, and
    therefore should not be allocated to a slot. The identifiers
    specified here must match those specified by value of configuration
    variables :macro:`MACHINE_RESOURCE_<name>` or
    :macro:`MACHINE_RESOURCE_INVENTORY_<name>`, or the identifiers
    will be ignored. The ``<name>`` identifies the type of resource, as
    specified by the value of configuration variable
    :macro:`MACHINE_RESOURCE_NAMES`. This configuration variable is used to
    have resources that are detected and reported to exist by HTCondor,
    but not assigned to slots. A restart of the *condor_startd* is
    required for changes to resources assigned to slots to take effect.
    If this variable is changed and :tool:`condor_reconfig` command is sent
    to the Startd, the list of Offline resources will be updated, and
    the count of resources of that type will be updated,
    but newly offline resources will still be assigned to slots.
    If an offline resource is assigned to a Partitionable slot, it will
    never be assigned to a new dynamic slot but it will not be removed from
    the ``Assigned<name>`` attribute of an existing dynamic slot.

:macro-def:`MACHINE_RESOURCE_INVENTORY_<name>[STARTD]`
    Specifies a command line that is executed upon start up of the
    *condor_startd* daemon. The script is expected to output an
    attribute definition of the form

    .. code-block:: condor-classad

          Detected<xxx>=y

    or of the form

    .. code-block:: condor-classad

          Detected<xxx>="y, z, a, ..."

    where ``<xxx>`` is the name of a resource that exists on the
    machine, and ``y`` is the quantity of the resource or
    ``"y, z, a, ..."`` is a comma and/or space separated list of
    identifiers of the resource that exist on the machine. This
    attribute is added to the machine ClassAd, such that these resources
    may be statically divided or partitioned. A script may be a
    convenient way to specify a calculated or detected quantity of the
    resource, instead of specifying a fixed quantity or list of the
    resource in the configuration when set by
    :macro:`MACHINE_RESOURCE_<name>`.

    The script may also output an attribute of the form

    .. code-block:: condor-classad

        Offline<xxx>="y, z"

    where ``<xxx>`` is the name of the resource, and ``"y, z"`` is a comma and/or
    space separated list of resource identifiers that are also in the ``Detected<xxx>`` list.
    This attribute is added to the machine ClassAd, and resources ``y`` and ``z`` will not be
    assigned to any slot and will not be included in the count of resources of this type.
    This will override the configuration variable ``OFFLINE_MACHINE_RESOURCE_<xxx>``
    on startup. But ``OFFLINE_MACHINE_RESOURCE_<xxx>`` can still be used to take additional resources offline
    without restarting.

:macro-def:`ENVIRONMENT_FOR_Assigned<name>[STARTD]`
    A space separated list of environment variables to set for the job.
    Each environment variable will be set to the list of assigned
    resources defined by the slot ClassAd attribute ``Assigned<name>``.
    Each environment variable name may be followed by an equals sign and
    a Perl style regular expression that defines how to modify each
    resource ID before using it as the value of the environment
    variable. As a special case for CUDA GPUs, if the environment
    variable name is ``CUDA_VISIBLE_DEVICES``, then the correct Perl
    style regular expression is applied automatically.

    For example, with the configuration

    .. code-block:: condor-config

          ENVIRONMENT_FOR_AssignedGPUs = VISIBLE_GPUS=/^/gpuid:/

    and with the machine ClassAd attribute
    ``AssignedGPUs = "CUDA1, CUDA2"``, the job's environment will
    contain

    .. code-block:: condor-config

          VISIBLE_GPUS = gpuid:CUDA1, gpuid:CUDA2

:macro-def:`ENVIRONMENT_VALUE_FOR_UnAssigned<name>[STARTD]`
    Defines the value to set for environment variables specified in by
    configuration variable :macro:`ENVIRONMENT_FOR_Assigned<name>` when there
    is no machine ClassAd attribute ``Assigned<name>`` for the slot.
    This configuration variable exists to deal with the situation where
    jobs will use a resource that they have not been assigned because
    there is no explicit assignment. The CUDA runtime library (for GPUs)
    has this problem.

    For example, where configuration is

    .. code-block:: condor-config

          ENVIRONMENT_FOR_AssignedGPUs = VISIBLE_GPUS
          ENVIRONMENT_VALUE_FOR_UnAssignedGPUs = none

    and there is no machine ClassAd attribute ``AssignedGPUs``, the
    job's environment will contain

    .. code-block:: condor-config

          VISIBLE_GPUS = none

:macro-def:`MUST_MODIFY_REQUEST_EXPRS[STARTD]`
    A boolean value that defaults to ``False``. When ``False``,
    configuration variables whose names begin with
    ``MODIFY_REQUEST_EXPR`` are only applied if the job claim still
    matches the partitionable slot after modification. If ``True``, the
    modifications always take place, and if the modifications cause the
    claim to no longer match, then the *condor_startd* will simply
    refuse the claim.

:macro-def:`MODIFY_REQUEST_EXPR_REQUESTMEMORY[STARTD]`
    An integer expression used by the *condor_startd* daemon to modify
    the evaluated value of the :ad-attr:`RequestMemory` job ClassAd attribute,
    before it used to provision a dynamic slot. The default value is
    given by

    .. code-block:: text

          quantize(RequestMemory,{128})


:macro-def:`MODIFY_REQUEST_EXPR_REQUESTDISK[STARTD]`
    An integer expression used by the *condor_startd* daemon to modify
    the evaluated value of the :ad-attr:`RequestDisk` job ClassAd attribute,
    before it used to provision a dynamic slot. The default value is
    given by

    .. code-block:: text

          quantize(RequestDisk,{1024})


:macro-def:`MODIFY_REQUEST_EXPR_REQUESTCPUS[STARTD]`
    An integer expression used by the *condor_startd* daemon to modify
    the evaluated value of the :ad-attr:`RequestCpus` job ClassAd attribute,
    before it used to provision a dynamic slot. The default value is
    given by

    .. code-block:: text

          quantize(RequestCpus,{1})


:macro-def:`NUM_SLOTS_TYPE_<N>[STARTD]`
    This macro controls how many of a given slot type are actually
    reported to HTCondor. There is no default.

:macro-def:`NUM_SLOTS[STARTD]`
    An integer value representing the number of slots reported when the
    multi-core machine is being evenly divided, and the slot type
    settings described above are not being used. The default is one slot
    for each CPU. This setting can be used to reserve some CPUs on a
    multi-core machine, which would not be reported to the HTCondor
    pool. This value cannot be used to make HTCondor advertise more
    slots than there are CPUs on the machine. To do that, use
    :macro:`NUM_CPUS`.

The following variables set consumption policies for partitionable
slots.
The :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
section details consumption policies.

:macro-def:`CONSUMPTION_POLICY[STARTD]`
    A boolean value that defaults to ``False``. When ``True``,
    consumption policies are enabled for partitionable slots within the
    *condor_startd* daemon. Any definition of the form
    ``SLOT_TYPE_<N>_CONSUMPTION_POLICY`` overrides this global
    definition for the given slot type.

:macro-def:`CONSUMPTION_<Resource>[STARTD]`
    An expression that specifies a consumption policy for a particular
    resource within a partitionable slot. To support a consumption
    policy, each resource advertised by the slot must have such a policy
    configured. Custom resources may be specified, substituting the
    resource name for ``<Resource>``. Any definition of the form
    ``SLOT_TYPE_<N>_CONSUMPTION_<Resource>`` overrides this global
    definition for the given slot type. CPUs, memory, and disk resources
    are always advertised by *condor_startd*, and have the default
    values:

    .. code-block:: condor-config

        CONSUMPTION_CPUS = quantize(target.RequestCpus,{1})
        CONSUMPTION_MEMORY = quantize(target.RequestMemory,{128})
        CONSUMPTION_DISK = quantize(target.RequestDisk,{1024})

    Custom resources have no default consumption policy.

:macro-def:`SLOT_WEIGHT[STARTD]`
    An expression that specifies a slot's weight, used as a multiplier
    the *condor_negotiator* daemon during matchmaking to assess user
    usage of a slot, which affects user priority. Defaults to :ad-attr:`Cpus`.

    In the case of slots with consumption policies, the cost of each
    match is is assessed as the difference in the slot weight expression
    before and after the resources consumed by the match are deducted
    from the slot. Only Memory, Cpus and Disk are valid attributes for
    this parameter.

:macro-def:`NUM_CLAIMS[STARTD]`
    Specifies the number of claims a partitionable slot will advertise
    for use by the *condor_negotiator* daemon. In the case of slots
    with a defined consumption policy, the *condor_negotiator* may
    match more than one job to the slot in a single negotiation cycle.
    For partitionable slots with a consumption policy, :macro:`NUM_CLAIMS`
    defaults to the number of CPUs owned by the slot. Otherwise, it
    defaults to 1.

The following configuration variables support java universe jobs.

:macro-def:`JAVA[STARTD]`
    The full path to the Java interpreter (the Java Virtual Machine).

:macro-def:`JAVA_CLASSPATH_ARGUMENT[STARTD]`
    The command line argument to the Java interpreter (the Java Virtual
    Machine) that specifies the Java Classpath. Classpath is a
    Java-specific term that denotes the list of locations (``.jar``
    files and/or directories) where the Java interpreter can look for
    the Java class files that a Java program requires.

:macro-def:`JAVA_CLASSPATH_SEPARATOR[STARTD]`
    The single character used to delimit constructed entries in the
    Classpath for the given operating system and Java Virtual Machine.
    If not defined, the operating system is queried for its default
    Classpath separator.

:macro-def:`JAVA_CLASSPATH_DEFAULT[STARTD]`
    A list of path names to ``.jar`` files to be added to the Java
    Classpath by default. The comma and/or space character delimits list
    entries.

:macro-def:`JAVA_EXTRA_ARGUMENTS[STARTD]`
    A list of additional arguments to be passed to the Java executable.

The following configuration variables control .NET version
advertisement.

:macro-def:`STARTD_PUBLISH_DOTNET[STARTD]`
    A boolean value that controls the advertising of the .NET framework
    on Windows platforms. When ``True``, the *condor_startd* will
    advertise all installed versions of the .NET framework within the
    :ad-attr:`DotNetVersions` attribute in the *condor_startd* machine
    ClassAd. The default value is ``True``. Set the value to ``false``
    to turn off .NET version advertising.

:macro-def:`DOT_NET_VERSIONS[STARTD]`
    A string expression that administrators can use to override the way
    that .NET versions are advertised. If the administrator wishes to
    advertise .NET installations, but wishes to do so in a format
    different than what the *condor_startd* publishes in its ClassAds,
    setting a string in this expression will result in the
    *condor_startd* publishing the string when :macro:`STARTD_PUBLISH_DOTNET`
    is ``True``. No value is set by default.

These macros control the power management capabilities of the
*condor_startd* to optionally put the machine in to a low power state
and wake it up later. 
See (:ref:`admin-manual/ep-policy-configuration:power management`). for more details

:macro-def:`HIBERNATE_CHECK_INTERVAL[STARTD]`
    An integer number of seconds that determines how often the
    *condor_startd* checks to see if the machine is ready to enter a
    low power state. The default value is 0, which disables the check.
    If not 0, the :macro:`HIBERNATE` expression is evaluated within the
    context of each slot at the given interval. If used, a value 300 (5
    minutes) is recommended.

    As a special case, the interval is ignored when the machine has just
    returned from a low power state, excluding ``"SHUTDOWN"``. In order
    to avoid machines from volleying between a running state and a low
    power state, an hour of uptime is enforced after a machine has been
    woken. After the hour has passed, regular checks resume.

:macro-def:`HIBERNATE[STARTD]`
    A string expression that represents lower power state. When this
    state name evaluates to a valid state other than ``"NONE"``, causes
    HTCondor to put the machine into the specified low power state. The
    following names are supported (and are not case sensitive):

    -  ``"NONE"``, ``"0"``: No-op; do not enter a low power state
    -  ``"S1"``, ``"1"``, ``"STANDBY"``, ``"SLEEP"``: On Windows, this
       is Sleep (standby)
    -  ``"S2"``, ``"2"``: On Windows, this is Sleep (standby)
    -  ``"S3"``, ``"3"``, ``"RAM"``, ``"MEM"``, ``"SUSPEND"``: On
       Windows, this is Sleep (standby)
    -  ``"S4"``, ``"4"``, ``"DISK"``, ``"HIBERNATE"``: Hibernate
    -  ``"S5"``, ``"5"``, ``"SHUTDOWN"``, ``"OFF"``: Shutdown (soft-off)

    The :macro:`HIBERNATE` expression is written in terms of the S-states as
    defined in the Advanced Configuration and Power Interface (ACPI)
    specification. The S-states take the form S<n>, where <n> is an
    integer in the range 0 to 5, inclusive. The number that results from
    evaluating the expression determines which S-state to enter. The
    notation was adopted because it appears to be the standard naming
    scheme for power states on several popular operating systems,
    including various flavors of Windows and Linux distributions. The
    other strings, such as ``"RAM"`` and ``"DISK"``, are provided for
    ease of configuration.

    Since this expression is evaluated in the context of each slot on
    the machine, any one slot has veto power over the other slots. If
    the evaluation of :macro:`HIBERNATE` in one slot evaluates to ``"NONE"``
    or ``"0"``, then the machine will not be placed into a low power
    state. On the other hand, if all slots evaluate to a non-zero value,
    but differ in value, then the largest value is used as the
    representative power state.

    Strings that do not match any in the table above are treated as
    ``"NONE"``.

:macro-def:`UNHIBERNATE[STARTD]`
    A boolean expression that specifies when an offline machine should
    be woken up. The default value is
    ``MachineLastMatchTime =!= UNDEFINED``. This expression does not do
    anything, unless there is an instance of *condor_rooster* running,
    or another program that evaluates the :ad-attr:`Unhibernate` expression of
    offline machine ClassAds. In addition, the collecting of offline
    machine ClassAds must be enabled for this expression to work. The
    variable :macro:`COLLECTOR_PERSISTENT_AD_LOG` explains this. The special
    attribute :ad-attr:`MachineLastMatchTime` is updated in the ClassAds of
    offline machines when a job would have been matched to the machine
    if it had been online. For multi-slot machines, the offline ClassAd
    for slot1 will also contain the attributes
    ``slot<X>_MachineLastMatchTime``, where ``X`` is replaced by the
    slot id of the other slots that would have been matched while
    offline. This allows the slot1 :macro:`UNHIBERNATE`
    expression to refer to all of the slots
    on the machine, in case that is necessary. By default,
    *condor_rooster* will wake up a machine if any slot on the machine
    has its :macro:`UNHIBERNATE` expression evaluate to ``True``.

:macro-def:`HIBERNATION_PLUGIN[STARTD]`
    A string which specifies the path and executable name of the
    hibernation plug-in that the *condor_startd* should use in the
    detection of low power states and switching to the low power states.
    The default value is ``$(LIBEXEC)/power_state``. A default
    executable in that location which meets these specifications is
    shipped with HTCondor.

    The *condor_startd* initially invokes this plug-in with both the
    value defined for :macro:`HIBERNATION_PLUGIN_ARGS` and the argument *ad*,
    and expects the plug-in to output a ClassAd to its standard output
    stream. The *condor_startd* will use this ClassAd to determine what
    low power setting to use on further invocations of the plug-in. To
    that end, the ClassAd must contain the attribute
    ``HibernationSupportedStates``, a comma separated list of low power
    modes that are available. The recognized mode strings are the same
    as those in the table for the configuration variable :macro:`HIBERNATE`.
    The optional attribute ``HibernationMethod`` specifies a string
    which describes the mechanism used by the plug-in. The default Linux
    plug-in shipped with HTCondor will produce one of the strings NONE,
    /sys, /proc, or pm-utils. The optional attribute
    ``HibernationRawMask`` is an integer which represents the bit mask
    of the modes detected.

    Subsequent *condor_startd* invocations of the plug-in have command
    line arguments defined by :macro:`HIBERNATION_PLUGIN_ARGS` plus the
    argument **set** *<power-mode>*, where *<power-mode>* is one of
    the supported states as given in the attribute
    ``HibernationSupportedStates``.

:macro-def:`HIBERNATION_PLUGIN_ARGS[STARTD]`
    Command line arguments appended to the command that invokes the
    plug-in. The additional argument *ad* is appended when the
    *condor_startd* initially invokes the plug-in.

:macro-def:`HIBERNATION_OVERRIDE_WOL[STARTD]`
    A boolean value that defaults to ``False``. When ``True``, it causes
    the *condor_startd* daemon's detection of the whether or not the
    network interface handles WOL packets to be ignored. When ``False``,
    hibernation is disabled if the network interface does not use WOL
    packets to wake from hibernation. Therefore, when ``True``
    hibernation can be enabled despite the fact that WOL packets are not
    used to wake machines.

:macro-def:`LINUX_HIBERNATION_METHOD[STARTD]`
    A string that can be used to override the default search used by
    HTCondor on Linux platforms to detect the hibernation method to use.
    This is used by the default hibernation plug-in executable that is
    shipped with HTCondor. The default behavior orders its search with:

    #. Detect and use the *pm-utils* command line tools. The
       corresponding string is defined with "pm-utils".
    #. Detect and use the directory in the virtual file system
       ``/sys/power``. The corresponding string is defined with "/sys".
    #. Detect and use the directory in the virtual file system
       ``/proc/ACPI``. The corresponding string is defined with "/proc".

    To override this ordered search behavior, and force the use of one
    particular method, set :macro:`LINUX_HIBERNATION_METHOD` to one of the
    defined strings.

:macro-def:`OFFLINE_EXPIRE_ADS_AFTER[STARTD]`
    An integer number of seconds specifying the lifetime of the
    persistent machine ClassAd representing a hibernating machine.
    Defaults to the largest 32-bit integer.

:macro-def:`DOCKER[STARTD]`
    Defines the path and executable name of the Docker CLI. The default
    value is /usr/bin/docker. Remember that the condor user must also be
    in the docker group for Docker Universe to work. See the Docker
    universe manual section for more details
    (:ref:`admin-manual/ep-policy-configuration:docker universe`).
    An example of the configuration for running the
    Docker CLI:

    .. code-block:: condor-config

          DOCKER = /usr/bin/docker


:macro-def:`DOCKER_VOLUMES[STARTD]`
    A list of directories on the host execute machine that might be volume
    mounted within the container. See the Docker Universe section for
    full details
    (:ref:`admin-manual/ep-policy-configuration:docker universe`).

:macro-def:`DOCKER_MOUNT_VOLUMES[STARTD]`
    A list of volumes, defined in the macro above, that will unconditionally
    be mounted inside the docker container.

:macro-def:`DOCKER_VOLUME_DIR_xxx_MOUNT_IF[STARTD]`
    This is a class ad expression, evaluated in the context of the job ad and the
    machine ad. Only when it evaluted to TRUE, is the volume named xxx mounted.

:macro-def:`DOCKER_IMAGE_CACHE_SIZE[STARTD]`
    The number of most recently used Docker images that will be kept on
    the local machine. The default value is 8.

:macro-def:`DOCKER_DROP_ALL_CAPABILITIES[STARTD]`
    A class ad expression, which defaults to true. Evaluated in the
    context of the job ad and the machine ad, when true, runs the Docker
    container with the command line option -drop-all-capabilities.
    Admins should be very careful with this setting, and only allow
    trusted users to run with full Linux capabilities within the
    container.

:macro-def:`DOCKER_PERFORM_TEST[STARTD]`
    When the *condor_startd* starts up, it runs a simple Docker
    container to verify that Docker completely works.  If 
    DOCKER_PERFORM_TEST is false, this test is skipped.

:macro-def:`DOCKER_RUN_UNDER_INIT[STARTD]`
    A boolean value which defaults to true, which tells the worker
    node to run Docker universe jobs with the --init option.
    
:macro-def:`DOCKER_EXTRA_ARGUMENTS[STARTD]`
    Any additional command line options the administrator wants to be
    added to the Docker container create command line can be set with
    this parameter. Note that the admin should be careful setting this,
    it is intended for newer Docker options that HTCondor doesn't support
    directly.  Arbitrary Docker options may break Docker universe, for example
    don't pass the --rm flag in DOCKER_EXTRA_ARGUMENTS, because then
    HTCondor cannot get the final exit status from a Docker job.

:macro-def:`DOCKER_NETWORKS[STARTD]`
    An optional, comma-separated list of admin-defined networks that a job
    may request with the ``docker_network_type`` submit file command.
    Advertised into the slot attribute DockerNetworks.

:macro-def:`DOCKER_SHM_SIZE[STARTD]`
    An optional knob that can be configured to adapt the ``--shm-size`` Docker
    container create argument. Allowed values are integers in bytes.
    If not set, ``--shm-size`` will not be specified by HTCondor and Docker's
    default is used.
    This is used to configure the size of the container's ``/dev/shm`` size adapting
    to the job's requested memory.

:macro-def:`DOCKER_CACHE_ADVERTISE_INTERVAL[STARTD]`
    The *condor_startd* periodically advertises how much disk
    space the docker daemon is using to store images into the
    slot attribute DockerCachedImageSize.  This knob, which 
    defaults to 1200 (seconds), controls how often the start
    polls the docker daemon for this information.

:macro-def:`DOCKER_LOG_DRIVER_NONE[STARTD]`
    When this knob is true (the default), condor passes the command line
    option --log-driver none to the docker container it creates.  This
    prevents the docker daemon from duplicating the job's stdout and saving
    it in a docker-specific place on disk to be viewed with the docker logs
    command, saving space on disk for jobs with large stdout.

:macro-def:`OPENMPI_INSTALL_PATH[STARTD]`
    The location of the Open MPI installation on the local machine.
    Referenced by ``examples/openmpiscript``, which is used for running
    Open MPI jobs in the parallel universe. The Open MPI bin and lib
    directories should exist under this path. The default value is
    ``/usr/lib64/openmpi``.

:macro-def:`OPENMPI_EXCLUDE_NETWORK_INTERFACES[STARTD]`
    A comma-delimited list of network interfaces that Open MPI should
    not use for MPI communications. Referenced by
    ``examples/openmpiscript``, which is used for running Open MPI jobs
    in the parallel universe.

    The list should contain any interfaces that your job could
    potentially see from any execute machine. The list may contain
    undefined interfaces without generating errors. Open MPI should
    exclusively use low latency/high speed networks it finds (e.g.
    InfiniBand) regardless of this setting. The default value is
    ``docker0``,\ ``virbr0``.

condor_schedd Configuration File Entries
-----------------------------------------

:index:`condor_schedd configuration variables<single: condor_schedd configuration variables; configuration>`

These macros control the *condor_schedd*.

:macro-def:`SHADOW[SCHEDD]`
    This macro determines the full path of the *condor_shadow* binary
    that the *condor_schedd* spawns. It is normally defined in terms of
    ``$(SBIN)``.

:macro-def:`START_LOCAL_UNIVERSE[SCHEDD]`
    A boolean value that defaults to ``TotalLocalJobsRunning < 200``.
    The *condor_schedd* uses this macro to determine whether to start a
    **local** universe job. At intervals determined by
    :macro:`SCHEDD_INTERVAL`, the *condor_schedd* daemon evaluates this
    macro for each idle **local** universe job that it has. For each
    job, if the :macro:`START_LOCAL_UNIVERSE` macro is ``True``, then the
    job's ``Requirements`` :index:`Requirements` expression is
    evaluated. If both conditions are met, then the job is allowed to
    begin execution.

    The following example only allows 10 **local** universe jobs to
    execute concurrently. The attribute :ad-attr:`TotalLocalJobsRunning` is
    supplied by *condor_schedd* 's ClassAd:

    .. code-block:: condor-config

            START_LOCAL_UNIVERSE = TotalLocalJobsRunning < 10


:macro-def:`STARTER_LOCAL[SCHEDD]`
    The complete path and executable name of the *condor_starter* to
    run for **local** universe jobs. This variable's value is defined in
    the initial configuration provided with HTCondor as

    .. code-block:: condor-config

          STARTER_LOCAL = $(SBIN)/condor_starter


    This variable would only be modified or hand added into the
    configuration for a pool to be upgraded from one running a version
    of HTCondor that existed before the **local** universe to one that
    includes the **local** universe, but without utilizing the newer,
    provided configuration files.

:macro-def:`LOCAL_UNIV_EXECUTE[SCHEDD]`
    A string value specifying the execute location for local universe
    jobs. Each running local universe job will receive a uniquely named
    subdirectory within this directory. If not specified, it defaults to
    ``$(SPOOL)/local_univ_execute``.

:macro-def:`USE_CGROUPS_FOR_LOCAL_UNIVERSE[SCHEDD]`
    A boolean value that defaults to true.  When true, local universe
    jobs on Linux are put into their own cgroup, for monitoring and
    cleanup.

:macro-def:`START_SCHEDULER_UNIVERSE[SCHEDD]`
    A boolean value that defaults to
    ``TotalSchedulerJobsRunning < 500``. The *condor_schedd* uses this
    macro to determine whether to start a **scheduler** universe job. At
    intervals determined by :macro:`SCHEDD_INTERVAL`, the *condor_schedd*
    daemon evaluates this macro for each idle **scheduler** universe job
    that it has. For each job, if the :macro:`START_SCHEDULER_UNIVERSE` macro
    is ``True``, then the job's ``Requirements``
    :index:`Requirements` expression is evaluated. If both
    conditions are met, then the job is allowed to begin execution.

    The following example only allows 10 **scheduler** universe jobs to
    execute concurrently. The attribute :ad-attr:`TotalSchedulerJobsRunning` is
    supplied by *condor_schedd* 's ClassAd:

    .. code-block:: condor-config

            START_SCHEDULER_UNIVERSE = TotalSchedulerJobsRunning < 10

:macro-def:`START_VANILLA_UNIVERSE[SCHEDD]`
    A boolean expression that defaults to nothing.
    When this macro is defined the *condor_schedd* uses it
    to determine whether to start a **vanilla** universe job.
    The *condor_schedd* uses the expression
    when matching a job with a slot in addition to the ``Requirements``
    expression of the job and the slot ClassAds.  The expression can
    refer to job attributes by using the prefix ``JOB``, slot attributes
    by using the prefix ``SLOT``, and job owner attributes by using the prefix ``OWNER``.

    The following example prevents jobs owned by a user from starting when
    that user has more than 25 held jobs

    .. code-block:: condor-config

            START_VANILLA_UNIVERSE = OWNER.JobsHeld <= 25


:macro-def:`SCHEDD_USES_STARTD_FOR_LOCAL_UNIVERSE[SCHEDD]`
    A boolean value that defaults to false. When true, the
    *condor_schedd* will spawn a special startd process to run local
    universe jobs. This allows local universe jobs to run with both a
    condor_shadow and a condor_starter, which means that file transfer
    will work with local universe jobs.

:macro-def:`MAX_JOBS_RUNNING[SCHEDD]`
    An integer representing a limit on the number of *condor_shadow*
    processes spawned by a given *condor_schedd* daemon, for all job
    universes except grid, scheduler, and local universe. Limiting the
    number of running scheduler and local universe jobs can be done
    using :macro:`START_LOCAL_UNIVERSE` and :macro:`START_SCHEDULER_UNIVERSE`. The
    actual number of allowed *condor_shadow* daemons may be reduced, if
    the amount of memory defined by :macro:`RESERVED_SWAP` limits the number
    of *condor_shadow* daemons. A value for :macro:`MAX_JOBS_RUNNING` that
    is less than or equal to 0 prevents any new job from starting.
    Changing this setting to be below the current number of jobs that
    are running will cause running jobs to be aborted until the number
    running is within the limit.

    Like all integer configuration variables, :macro:`MAX_JOBS_RUNNING` may
    be a ClassAd expression that evaluates to an integer, and which
    refers to constants either directly or via macro substitution. The
    default value is an expression that depends on the total amount of
    memory and the operating system. The default expression requires
    1MByte of RAM per running job on the access point. In some
    environments and configurations, this is overly generous and can be
    cut by as much as 50%. On Windows platforms, the number of running
    jobs is capped at 2000. A 64-bit version of Windows is recommended
    in order to raise the value above the default. Under Unix, the
    maximum default is now 10,000. To scale higher, we recommend that
    the system ephemeral port range is extended such that there are at
    least 2.1 ports per running job.

    Here are example configurations:

    .. code-block:: condor-config

        ## Example 1:
        MAX_JOBS_RUNNING = 10000

        ## Example 2:
        ## This is more complicated, but it produces the same limit as the default.
        ## First define some expressions to use in our calculation.
        ## Assume we can use up to 80% of memory and estimate shadow private data
        ## size of 800k.
        MAX_SHADOWS_MEM = ceiling($(DETECTED_MEMORY)*0.8*1024/800)
        ## Assume we can use ~21,000 ephemeral ports (avg ~2.1 per shadow).
        ## Under Linux, the range is set in /proc/sys/net/ipv4/ip_local_port_range.
        MAX_SHADOWS_PORTS = 10000
        ## Under windows, things are much less scalable, currently.
        ## Note that this can probably be safely increased a bit under 64-bit windows.
        MAX_SHADOWS_OPSYS = ifThenElse(regexp("WIN.*","$(OPSYS)"),2000,100000)
        ## Now build up the expression for MAX_JOBS_RUNNING.  This is complicated
        ## due to lack of a min() function.
        MAX_JOBS_RUNNING = $(MAX_SHADOWS_MEM)
        MAX_JOBS_RUNNING = \
          ifThenElse( $(MAX_SHADOWS_PORTS) < $(MAX_JOBS_RUNNING), \
                      $(MAX_SHADOWS_PORTS), \
                      $(MAX_JOBS_RUNNING) )
        MAX_JOBS_RUNNING = \
          ifThenElse( $(MAX_SHADOWS_OPSYS) < $(MAX_JOBS_RUNNING), \
                      $(MAX_SHADOWS_OPSYS), \
                      $(MAX_JOBS_RUNNING) )

:macro-def:`MAX_JOBS_SUBMITTED[SCHEDD]`
    This integer value limits the number of jobs permitted in a
    *condor_schedd* daemon's queue. Submission of a new cluster of jobs
    fails, if the total number of jobs would exceed this limit. The
    default value for this variable is the largest positive integer
    value.

:macro-def:`MAX_JOBS_PER_OWNER[SCHEDD]`
    This integer value limits the number of jobs any given owner (user)
    is permitted to have within a *condor_schedd* daemon's queue. A job
    submission fails if it would cause this limit on the number of jobs
    to be exceeded. The default value is 100000.

    This configuration variable may be most useful in conjunction with
    :macro:`MAX_JOBS_SUBMITTED`, to ensure that no one user can dominate the
    queue.

:macro-def:`ALLOW_SUBMIT_FROM_KNOWN_USERS_ONLY[SCHEDD]`
    This boolean value determines if a User record will be created automatically
    when an unknown user submits a job.
    When true, only daemons or users that have a User record in the *condor_schedd*
    can submit jobs. When false, a User record will be added during job submission
    for users that have never submitted a job before. The default value is false
    which is consistent with the behavior of the *condor_schedd* before User records
    were added.

:macro-def:`MAX_RUNNING_SCHEDULER_JOBS_PER_OWNER[SCHEDD]`
    This integer value limits the number of scheduler universe jobs that
    any given owner (user) can have running at one time. This limit will
    affect the number of running Dagman jobs, but not the number of
    nodes within a DAG. The default value is 200

:macro-def:`MAX_JOBS_PER_SUBMISSION[SCHEDD]`
    This integer value limits the number of jobs any single submission
    is permitted to add to a *condor_schedd* daemon's queue. The whole
    submission fails if the number of jobs would exceed this limit. The
    default value is 20000.

    This configuration variable may be useful for catching user error,
    and for protecting a busy *condor_schedd* daemon from the
    excessively lengthy interruption required to accept a very large
    number of jobs at one time.

:macro-def:`MAX_SHADOW_EXCEPTIONS[SCHEDD]`
    This macro controls the maximum number of times that
    *condor_shadow* processes can have a fatal error (exception) before
    the *condor_schedd* will relinquish the match associated with the
    dying shadow. Defaults to 2.

:macro-def:`MAX_PENDING_STARTD_CONTACTS[SCHEDD]`
    An integer value that limits the number of simultaneous connection
    attempts by the *condor_schedd* when it is requesting claims from
    one or more *condor_startd* daemons. The intention is to protect
    the *condor_schedd* from being overloaded by authentication
    operations. The default value is 0. The special value 0 indicates no
    limit.

:macro-def:`CURB_MATCHMAKING[SCHEDD]`
    A ClassAd expression evaluated by the *condor_schedd* in the
    context of the *condor_schedd* daemon's own ClassAd. While this
    expression evaluates to ``True``, the *condor_schedd* will refrain
    from requesting more resources from a *condor_negotiator*. Defaults
    to ``RecentDaemonCoreDutyCycle > 0.98``.

:macro-def:`MAX_CONCURRENT_DOWNLOADS[SCHEDD]`
    This specifies the maximum number of simultaneous transfers of
    output files from execute machines to the access point. The limit
    applies to all jobs submitted from the same *condor_schedd*. The
    default is 100. A setting of 0 means unlimited transfers. This limit
    currently does not apply to grid universe jobs,
    and it also does not apply to streaming output files. When the
    limit is reached, additional transfers will queue up and wait before
    proceeding.

:macro-def:`MAX_CONCURRENT_UPLOADS[SCHEDD]`
    This specifies the maximum number of simultaneous transfers of input
    files from the access point to execute machines. The limit applies
    to all jobs submitted from the same *condor_schedd*. The default is
    100. A setting of 0 means unlimited transfers. This limit currently
    does not apply to grid universe jobs. When
    the limit is reached, additional transfers will queue up and wait
    before proceeding.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE[SCHEDD]`
    This configures throttling of file transfers based on the disk load
    generated by file transfers. The maximum number of concurrent file
    transfers is specified by :macro:`MAX_CONCURRENT_UPLOADS` and
    :macro:`MAX_CONCURRENT_DOWNLOADS`. Throttling will dynamically
    reduce the level of concurrency further to attempt to prevent disk
    load from exceeding the specified level. Disk load is computed as
    the average number of file transfer processes conducting read/write
    operations at the same time. The throttle may be specified as a
    single floating point number or as a range. Syntax for the range is
    the smaller number followed by 1 or more spaces or tabs, the string
    ``"to"``, 1 or more spaces or tabs, and then the larger number.
    Example:

    .. code-block:: condor-config

          FILE_TRANSFER_DISK_LOAD_THROTTLE = 5 to 6.5

    If only a single number is provided, this serves as the upper limit,
    and the lower limit is set to 90% of the upper limit. When the disk
    load is above the upper limit, no new transfers will be started.
    When between the lower and upper limits, new transfers will only be
    started to replace ones that finish. The default value is 2.0.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE_WAIT_BETWEEN_INCREMENTS[SCHEDD]`
    This rarely configured variable sets the waiting period between
    increments to the concurrency level set by
    :macro:`FILE_TRANSFER_DISK_LOAD_THROTTLE`. The default is 1 minute. A
    value that is too short risks starting too many transfers before
    their effect on the disk load becomes apparent.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE_SHORT_HORIZON[SCHEDD]`
    This rarely configured variable specifies the string name of the
    short monitoring time span to use for throttling. The named time
    span must exist in :macro:`TRANSFER_IO_REPORT_TIMESPANS`. The
    default is ``1m``, which is 1 minute.

:macro-def:`FILE_TRANSFER_DISK_LOAD_THROTTLE_LONG_HORIZON[SCHEDD]`
    This rarely configured variable specifies the string name of the
    long monitoring time span to use for throttling. The named time span
    must exist in :macro:`TRANSFER_IO_REPORT_TIMESPANS`. The default is
    ``5m``, which is 5 minutes.

:macro-def:`TRANSFER_QUEUE_USER_EXPR[SCHEDD]`
    This rarely configured expression specifies the user name to be used
    for scheduling purposes in the file transfer queue. The scheduler
    attempts to give equal weight to each user when there are multiple
    jobs waiting to transfer files within the limits set by
    :macro:`MAX_CONCURRENT_UPLOADS` and/or :macro:`MAX_CONCURRENT_DOWNLOADS`.
    When choosing a new job to allow to transfer, the first job belonging
    to the transfer queue user who has least number of active transfers
    will be selected. In case of a tie, the user who has least recently
    been given an opportunity to start a transfer will be selected. By
    default, a transfer queue user is identified as the job owner. A
    different user name may be specified by configuring :macro:`TRANSFER_QUEUE_USER_EXPR`
    to a string expression that is evaluated in the context of the job ad.
    For example, if this expression were set to a name that is the same
    for all jobs, file transfers would be scheduled in
    first-in-first-out order rather than equal share order. Note that
    the string produced by this expression is used as a prefix in the
    ClassAd attributes for per-user file transfer I/O statistics that
    are published in the *condor_schedd* ClassAd.

:macro-def:`MAX_TRANSFER_INPUT_MB[SCHEDD]`
    This integer expression specifies the maximum allowed total size in
    MiB of the input files that are transferred for a job. This
    expression does not apply to grid universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value ``-1`` indicates
    no limit. The default value is -1. The job may override the system
    setting by specifying its own limit using the :ad-attr:`MaxTransferInputMB`
    attribute. If the observed size of all input files at submit time is
    larger than the limit, the job will be immediately placed on hold
    with a :ad-attr:`HoldReasonCode` value of 32. If the job passes this
    initial test, but the size of the input files increases or the limit
    decreases so that the limit is violated, the job will be placed on
    hold at the time when the file transfer is attempted.

:macro-def:`MAX_TRANSFER_OUTPUT_MB[SCHEDD]`
    This integer expression specifies the maximum allowed total size in
    MiB of the output files that are transferred for a job. This
    expression does not apply to grid universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value ``-1`` indicates
    no limit. The default value is -1. The job may override the system
    setting by specifying its own limit using the
    :ad-attr:`MaxTransferOutputMB` attribute. If the total size of the job's
    output files to be transferred is larger than the limit, the job
    will be placed on hold with a :ad-attr:`HoldReasonCode` value of 33. The
    output will be transferred up to the point when the limit is hit, so
    some files may be fully transferred, some partially, and some not at
    all.

:macro-def:`MAX_TRANSFER_QUEUE_AGE[SCHEDD]`
    The number of seconds after which an aged and queued transfer may be
    dequeued from the transfer queue, as it is presumably hung. Defaults
    to 7200 seconds, which is 120 minutes.

:macro-def:`TRANSFER_IO_REPORT_INTERVAL[SCHEDD]`
    The sampling interval in seconds for collecting I/O statistics for
    file transfer. The default is 10 seconds. To provide sufficient
    resolution, the sampling interval should be small compared to the
    smallest time span that is configured in
    :macro:`TRANSFER_IO_REPORT_TIMESPANS`. The shorter the sampling interval,
    the more overhead of data collection, which may slow down the
    *condor_schedd*. See :doc:`/classad-attributes/scheduler-classad-attributes`
    for a description of the published attributes.

:macro-def:`TRANSFER_IO_REPORT_TIMESPANS[SCHEDD]`
    A string that specifies a list of time spans over which I/O
    statistics are reported, using exponential moving averages (like the
    1m, 5m, and 15m load averages in Unix). Each entry in the list
    consists of a label followed by a colon followed by the number of
    seconds over which the named time span should extend. The default is
    1m:60 5m:300 1h:3600 1d:86400. To provide sufficient resolution, the
    smallest reported time span should be large compared to the sampling
    interval, which is configured by :macro:`TRANSFER_IO_REPORT_INTERVAL`.
    See :doc:`/classad-attributes/scheduler-classad-attributes` for a
    description of the published attributes.

:macro-def:`SCHEDD_QUERY_WORKERS[SCHEDD]`
    This specifies the maximum number of concurrent sub-processes that
    the *condor_schedd* will spawn to handle queries. The setting is
    ignored in Windows. In Unix, the default is 8. If the limit is
    reached, the next query will be handled in the *condor_schedd* 's
    main process.

:macro-def:`CONDOR_Q_USE_V3_PROTOCOL[SCHEDD]`
    A boolean value that, when ``True``, causes the *condor_schedd* to
    use an algorithm that responds to :tool:`condor_q` requests by not
    forking itself to handle each request. It instead handles the
    requests in a non-blocking way. The default value is ``True``.

:macro-def:`CONDOR_Q_DASH_BATCH_IS_DEFAULT[SCHEDD]`
    A boolean value that, when ``True``, causes :tool:`condor_q` to print the
    **-batch** output unless the **-nobatch** option is used or the
    other arguments to :tool:`condor_q` are incompatible with batch mode. For
    instance **-long** is incompatible with **-batch**. The default
    value is ``True``.

:macro-def:`CONDOR_Q_ONLY_MY_JOBS[SCHEDD]`
    A boolean value that, when ``True``, causes :tool:`condor_q` to request
    that only the current user's jobs be queried unless the current user
    is a queue superuser. It also causes the *condor_schedd* to honor
    that request. The default value is ``True``. A value of ``False`` in
    either :tool:`condor_q` or the *condor_schedd* will result in the old
    behavior of querying all jobs.

:macro-def:`CONDOR_Q_SHOW_OLD_SUMMARY[SCHEDD]`
    A boolean value that, when ``True``, causes :tool:`condor_q` to show the
    old single line summary totals. When ``False`` :tool:`condor_q` will show
    the new multi-line summary totals.

:macro-def:`SCHEDD_MIN_INTERVAL[SCHEDD]`
    This macro determines the minimum interval for both how often the
    *condor_schedd* sends a ClassAd update to the *condor_collector*
    and how often the *condor_schedd* daemon evaluates jobs. It is
    defined in terms of seconds and defaults to 5 seconds.

:macro-def:`SCHEDD_INTERVAL[SCHEDD]`
    This macro determines the maximum interval for both how often the
    *condor_schedd* sends a ClassAd update to the *condor_collector*
    and how often the *condor_schedd* daemon evaluates jobs. It is
    defined in terms of seconds and defaults to 300 (every 5 minutes).

:macro-def:`ABSENT_SUBMITTER_LIFETIME[SCHEDD]`
    This macro determines the maximum time that the *condor_schedd*
    will remember a submitter after the last job for that submitter
    leaves the queue. It is defined in terms of seconds and defaults to
    1 week.

:macro-def:`ABSENT_SUBMITTER_UPDATE_RATE[SCHEDD]`
    This macro can be used to set the maximum rate at which the
    *condor_schedd* sends updates to the *condor_collector* for
    submitters that have no jobs in the queue. It is defined in terms of
    seconds and defaults to 300 (every 5 minutes).

:macro-def:`WINDOWED_STAT_WIDTH[SCHEDD]`
    The number of seconds that forms a time window within which
    performance statistics of the *condor_schedd* daemon are
    calculated. Defaults to 300 seconds.

:macro-def:`SCHEDD_INTERVAL_TIMESLICE[SCHEDD]`
    The bookkeeping done by the *condor_schedd* takes more time when
    there are large numbers of jobs in the job queue. However, when it
    is not too expensive to do this bookkeeping, it is best to keep the
    collector up to date with the latest state of the job queue.
    Therefore, this macro is used to adjust the bookkeeping interval so
    that it is done more frequently when the cost of doing so is
    relatively small, and less frequently when the cost is high. The
    default is 0.05, which means the schedd will adapt its bookkeeping
    interval to consume no more than 5% of the total time available to
    the schedd. The lower bound is configured by :macro:`SCHEDD_MIN_INTERVAL`
    (default 5 seconds), and the upper bound is configured by
    :macro:`SCHEDD_INTERVAL` (default 300 seconds).

:macro-def:`JOB_START_COUNT[SCHEDD]`
    This macro works together with the :macro:`JOB_START_DELAY` macro
    to throttle job starts. The default and minimum values for this
    integer configuration variable are both 1.

:macro-def:`JOB_START_DELAY[SCHEDD]`
    This integer-valued macro works together with the :macro:`JOB_START_COUNT`
    macro to throttle job starts. The *condor_schedd* daemon starts
    ``$(JOB_START_COUNT)`` jobs at a time, then delays for
    ``$(JOB_START_DELAY)`` seconds before starting the next set of jobs.
    This delay prevents a sudden, large load on resources required by
    the jobs during their start up phase. The resulting job start rate
    averages as fast as (``$(JOB_START_COUNT)``/``$(JOB_START_DELAY)``)
    jobs/second. This setting is defined in terms of seconds and
    defaults to 0, which means jobs will be started as fast as possible.
    If you wish to throttle the rate of specific types of jobs, you can
    use the job attribute :ad-attr:`NextJobStartDelay`.

:macro-def:`MAX_NEXT_JOB_START_DELAY[SCHEDD]`
    An integer number of seconds representing the maximum allowed value
    of the job ClassAd attribute :ad-attr:`NextJobStartDelay`. It defaults to
    600, which is 10 minutes.

:macro-def:`JOB_STOP_COUNT[SCHEDD]`
    An integer value representing the number of jobs operated on at one
    time by the *condor_schedd* daemon, when throttling the rate at
    which jobs are stopped via :tool:`condor_rm`, :tool:`condor_hold`, or
    :tool:`condor_vacate_job`. The default and minimum values are both 1.
    This variable is ignored for grid and scheduler universe jobs.

:macro-def:`JOB_STOP_DELAY[SCHEDD]`
    An integer value representing the number of seconds delay utilized
    by the *condor_schedd* daemon, when throttling the rate at which
    jobs are stopped via :tool:`condor_rm`, :tool:`condor_hold`, or
    :tool:`condor_vacate_job`. The *condor_schedd* daemon stops
    ``$(JOB_STOP_COUNT)`` jobs at a time, then delays for
    ``$(JOB_STOP_DELAY)`` seconds before stopping the next set of jobs.
    This delay prevents a sudden, large load on resources required by
    the jobs when they are terminating. The resulting job stop rate
    averages as fast as ``JOB_STOP_COUNT/JOB_STOP_DELAY`` jobs per
    second. This configuration variable is also used during the graceful
    shutdown of the *condor_schedd* daemon. During graceful shutdown,
    this macro determines the wait time in between requesting each
    *condor_shadow* daemon to gracefully shut down. The default value
    is 0, which means jobs will be stopped as fast as possible. This
    variable is ignored for grid and scheduler universe jobs.

:macro-def:`JOB_IS_FINISHED_COUNT[SCHEDD]`
    An integer value representing the number of jobs that the
    *condor_schedd* will let permanently leave the job queue each time
    that it examines the jobs that are ready to do so. The default value
    is 1.

:macro-def:`JOB_IS_FINISHED_INTERVAL[SCHEDD]`
    The *condor_schedd* maintains a list of jobs that are ready to
    permanently leave the job queue, for example, when they have
    completed or been removed. This integer-valued macro specifies a
    delay in seconds between instances of taking jobs permanently out of
    the queue. The default value is 0, which tells the *condor_schedd*
    to not impose any delay.

:macro-def:`ALIVE_INTERVAL[SCHEDD]`
    An initial value for an integer number of seconds defining how often
    the *condor_schedd* sends a UDP keep alive message to any
    *condor_startd* it has claimed. When the *condor_schedd* claims a
    *condor_startd*, the *condor_schedd* tells the *condor_startd*
    how often it is going to send these messages. The utilized interval
    for sending keep alive messages is the smallest of the two values
    :macro:`ALIVE_INTERVAL` and the expression ``JobLeaseDuration/3``, formed
    with the job ClassAd attribute :ad-attr:`JobLeaseDuration`. The value of
    the interval is further constrained by the floor value of 10
    seconds. If the *condor_startd* does not receive any of these keep
    alive messages during a certain period of time (defined via
    :macro:`MAX_CLAIM_ALIVES_MISSED`) the  *condor_startd* releases the claim,
    and the *condor_schedd* no longer pays for the resource (in terms of
    user priority in the system). The macro is defined in terms of seconds
    and defaults to 300, which is 5 minutes.

:macro-def:`STARTD_SENDS_ALIVES[SCHEDD]`
    Note: This setting is deprecated, and may go away in a future
    version of HTCondor. This setting is mainly useful when running
    mixing very old *condor_schedd* daemons with newer pools. A boolean
    value that defaults to ``True``, causing keep alive messages to be
    sent from the *condor_startd* to the *condor_schedd* by TCP during
    a claim. When ``False``, the *condor_schedd* daemon sends keep
    alive signals to the *condor_startd*, reversing the direction.
    This variable is only used by the *condor_schedd* daemon.

:macro-def:`REQUEST_CLAIM_TIMEOUT[SCHEDD]`
    This macro sets the time (in seconds) that the *condor_schedd* will
    wait for a claim to be granted by the *condor_startd*. The default
    is 30 minutes. This is only likely to matter if
    :macro:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION` is ``True``, and
    the *condor_startd* has an existing claim, and it takes a long time
    for the existing claim to be preempted due to
    ``MaxJobRetirementTime``. Once a request times out, the
    *condor_schedd* will simply begin the process of finding a machine
    for the job all over again.

    Normally, it is not a good idea to set this to be very small, where
    a small value is a few minutes. Doing so can lead to failure to
    preempt, because the preempting job will spend a significant
    fraction of its time waiting to be re-matched. During that time, it
    would miss out on any opportunity to run if the job it is trying to
    preempt gets out of the way.

:macro-def:`SHADOW_SIZE_ESTIMATE[SCHEDD]`
    The estimated private virtual memory size of each *condor_shadow*
    process in KiB. This value is only used if :macro:`RESERVED_SWAP` is
    non-zero. The default value is 800.

:macro-def:`SHADOW_RENICE_INCREMENT[SCHEDD]`
    When the *condor_schedd* spawns a new *condor_shadow*, it can do
    so with a nice-level. A nice-level is a Unix mechanism that allows
    users to assign their own processes a lower priority so that the
    processes run with less priority than other tasks on the machine.
    The value can be any integer between 0 and 19, with a value of 19
    being the lowest priority. It defaults to 0.

:macro-def:`SCHED_UNIV_RENICE_INCREMENT[SCHEDD]`
    Analogous to :macro:`JOB_RENICE_INCREMENT` and
    :macro:`SHADOW_RENICE_INCREMENT`, scheduler universe jobs can be given a
    nice-level. The value can be any integer between 0 and 19, with a
    value of 19 being the lowest priority. It defaults to 0.

:macro-def:`QUEUE_CLEAN_INTERVAL[SCHEDD]`
    The *condor_schedd* maintains the job queue on a given machine. It
    does so in a persistent way such that if the *condor_schedd*
    crashes, it can recover a valid state of the job queue. The
    mechanism it uses is a transaction-based log file (the
    ``job_queue.log`` file, not the ``SchedLog`` file). This file
    contains an initial state of the job queue, and a series of
    transactions that were performed on the queue (such as new jobs
    submitted or jobs completing). Periodically, the
    *condor_schedd* will go through this log, truncate all the
    transactions and create a new file with containing only the new
    initial state of the log. This is a somewhat expensive operation,
    but it speeds up when the *condor_schedd* restarts since there are
    fewer transactions it has to play to figure out what state the job
    queue is really in. This macro determines how often the
    *condor_schedd* should rework this queue to cleaning it up. It is
    defined in terms of seconds and defaults to 86400 (once a day).

:macro-def:`WALL_CLOCK_CKPT_INTERVAL[SCHEDD]`
    The job queue contains a counter for each job's "wall clock" run
    time, i.e., how long each job has executed so far. This counter is
    displayed by :tool:`condor_q`. The counter is updated when the job is
    evicted or when the job completes. When the *condor_schedd*
    crashes, the run time for jobs that are currently running will not
    be added to the counter (and so, the run time counter may become
    smaller than the CPU time counter). The *condor_schedd* saves run
    time "checkpoints" periodically for running jobs so if the
    *condor_schedd* crashes, only run time since the last checkpoint is
    lost. This macro controls how often the *condor_schedd* saves run
    time checkpoints. It is defined in terms of seconds and defaults to
    3600 (one hour). A value of 0 will disable wall clock checkpoints.

:macro-def:`QUEUE_ALL_USERS_TRUSTED[SCHEDD]`
    Defaults to False. If set to True, then unauthenticated users are
    allowed to write to the queue, and also we always trust whatever the
    :ad-attr:`Owner` value is set to be by the client in the job ad. This was
    added so users can continue to use the SOAP web-services interface
    over HTTP (w/o authenticating) to submit jobs in a secure,
    controlled environment - for instance, in a portal setting.

:macro-def:`QUEUE_SUPER_USERS[SCHEDD]`
    A comma and/or space separated list of user names on a given machine
    that are given super-user access to the job queue, meaning that they
    can modify or delete the job ClassAds of other users. These should be
    of form ``USER@DOMAIN``; if the domain is not present in the username,
    HTCondor will assume the default :macro:`UID_DOMAIN`. When not on
    this list, users can only modify or delete their own ClassAds from
    the job queue. Whatever user name corresponds with the UID that
    HTCondor is running as - usually user condor - will automatically be
    included in this list, because that is needed for HTCondor's proper
    functioning. See
    :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    on UIDs in HTCondor for more details on this. By default, the Unix user root
    and the Windows user administrator are given the ability to remove
    other user's jobs, in addition to user condor. In addition to a
    single user, Unix user groups may be specified by using a special
    syntax defined for this configuration variable; the syntax is the
    percent character (``%``) followed by the user group name. All
    members of the user group are given super-user access.

:macro-def:`QUEUE_SUPER_USER_MAY_IMPERSONATE[SCHEDD]`
    A regular expression that matches the operating system user names
    (that is, job owners in the form ``USER``) that the queue super user
    may impersonate when managing jobs.  This allows the admin to limit
    the operating system users a super user can launch jobs as.
    When not set, the default behavior is to allow impersonation of any
    user who has had a job in the queue during the life of the
    *condor_schedd*. For proper functioning of the *condor_shadow*,
    the *condor_gridmanager*, and the *condor_job_router*, this
    expression, if set, must match the owner names of all jobs that
    these daemons will manage. Note that a regular expression that
    matches only part of the user name is still considered a match. If
    acceptance of partial matches is not desired, the regular expression
    should begin with ^ and end with $.

:macro-def:`SYSTEM_JOB_MACHINE_ATTRS[SCHEDD]`
    This macro specifies a space and/or comma separated list of machine
    attributes that should be recorded in the job ClassAd. The default
    attributes are :ad-attr:`Cpus` and :ad-attr:`SlotWeight`. When there are multiple
    run attempts, history of machine attributes from previous run
    attempts may be kept. The number of run attempts to store is
    specified by the configuration variable
    :macro:`SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH`. A machine
    attribute named ``X`` will be inserted into the job ClassAd as an
    attribute named ``MachineAttrX0``. The previous value of this
    attribute will be named ``MachineAttrX1``, the previous to that will
    be named ``MachineAttrX2``, and so on, up to the specified history
    length. A history of length 1 means that only ``MachineAttrX0`` will
    be recorded. Additional attributes to record may be specified on a
    per-job basis by using the
    :subcom:`job_machine_attrs[and SYSTEM_JOB_MACHINE_ATTRS]`
    submit file command. The value recorded in the job ClassAd is the
    evaluation of the machine attribute in the context of the job
    ClassAd when the *condor_schedd* daemon initiates the start up of
    the job. If the evaluation results in an ``Undefined`` or ``Error``
    result, the value recorded in the job ClassAd will be ``Undefined``
    or ``Error`` respectively.

:macro-def:`SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH[SCHEDD]`
    The integer number of run attempts to store in the job ClassAd when
    recording the values of machine attributes listed in
    :macro:`SYSTEM_JOB_MACHINE_ATTRS`. The default is 1. The
    history length may also be extended on a per-job basis by using the
    submit file command
    :subcom:`job_machine_attrs_history_length[and SYSTEM_JOB_MACHINE_ATTRS_HISTORY_LENGTH]`
    The larger of the system and per-job history lengths will be used. A
    history length of 0 disables recording of machine attributes.

:macro-def:`SCHEDD_LOCK[SCHEDD]`
    This macro specifies what lock file should be used for access to the
    ``SchedLog`` file. It must be a separate file from the ``SchedLog``,
    since the ``SchedLog`` may be rotated and synchronization across log
    file rotations is desired. This macro is defined relative to the
    ``$(LOCK)`` macro.

:macro-def:`SCHEDD_NAME[SCHEDD]`
    Used to give an alternative value to the ``Name`` attribute in the
    *condor_schedd* 's ClassAd.

    See the description of :macro:`MASTER_NAME`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`SCHEDD_ATTRS[SCHEDD]`
    This macro is described in :macro:`<SUBSYS>_ATTRS`.

:macro-def:`SCHEDD_DEBUG[SCHEDD]`
    This macro (and other settings related to debug logging in the
    *condor_schedd*) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`SCHEDD_ADDRESS_FILE[SCHEDD]`
    This macro is described in :macro:`<SUBSYS>_ADDRESS_FILE`.

:macro-def:`SCHEDD_EXECUTE[SCHEDD]`
    A directory to use as a temporary sandbox for local universe jobs.
    Defaults to ``$(SPOOL)``/execute.

:macro-def:`FLOCK_TO[SCHEDD]`
    This defines a comma separate list of central manager
    machines this schedd should flock to.  The default value
    is empty.  For flocking to work, each of these central
    managers should also define :macro:`FLOCK_FROM` with the
    name of this schedd in that list.  This paramaeter
    explicilty sets :macro:`FLOCK_NEGOTIATOR_HOSTS` and 
    :macro:`FLOCK_COLLECTOR_HOSTS` so that you usually
    just need to set :macro:`FLOCK_TO` and no others to make
    flocking work.

:macro-def:`FLOCK_NEGOTIATOR_HOSTS[SCHEDD]`
    Defines a comma and/or space separated list of *condor_negotiator*
    host names for pools in which the *condor_schedd* should attempt to
    run jobs. If not set, the *condor_schedd* will query the
    *condor_collector* daemons for the addresses of the
    *condor_negotiator* daemons. If set, then the *condor_negotiator*
    daemons must be specified in order, corresponding to the list set by
    :macro:`FLOCK_COLLECTOR_HOSTS`. In the typical case, where each pool has
    the *condor_collector* and *condor_negotiator* running on the same
    machine, ``$(FLOCK_NEGOTIATOR_HOSTS)`` should have the same
    definition as ``$(FLOCK_COLLECTOR_HOSTS)``. This configuration value
    is also typically used as a macro for adding the
    *condor_negotiator* to the relevant authorization lists.

:macro-def:`FLOCK_COLLECTOR_HOSTS[SCHEDD]`
    This macro defines a list of collector host names (not including the
    local ``$(COLLECTOR_HOST)`` machine) for pools in which the
    *condor_schedd* should attempt to run jobs. Hosts in the list
    should be in order of preference. The *condor_schedd* will only
    send a request to a central manager in the list if the local pool
    and pools earlier in the list are not satisfying all the job
    requests. :macro:`ALLOW_NEGOTIATOR_SCHEDD`
    must also be configured to allow negotiators from all of the pools to
    contact the *condor_schedd* at the ``NEGOTIATOR`` authorization level.
    Similarly, the central managers of the remote pools must be
    configured to allow this *condor_schedd* to join the pool (this
    requires ``ADVERTISE_SCHEDD`` authorization level, which defaults to
    ``WRITE``).

:macro-def:`FLOCK_INCREMENT[SCHEDD]`
    This integer value controls how quickly flocking to various pools
    will occur. It defaults to 1, meaning that pools will be considered
    for flocking slowly. The first *condor_collector* daemon listed in
    :macro:`FLOCK_COLLECTOR_HOSTS` will be considered for flocking, and
    then the second, and so on. A larger value increases the number of
    *condor_collector* daemons to be considered for flocking. For example,
    a value of 2 will partition the :macro:`FLOCK_COLLECTOR_HOSTS`
    into sets of 2 *condor_collector* daemons, and each set will be
    considered for flocking.

:macro-def:`MIN_FLOCK_LEVEL[SCHEDD]`
    This integer value specifies a number of remote pools that the
    *condor_schedd* should always flock to.
    It defaults to 0, meaning that none of the pools listed in
    :macro:`FLOCK_COLLECTOR_HOSTS` will be considered for flocking when
    there are no idle jobs in need of match-making.
    Setting a larger value N means the *condor_schedd* will always
    flock to (i.e. look for matches in) the first N pools listed in
    :macro:`FLOCK_COLLECTOR_HOSTS`.

:macro-def:`NEGOTIATE_ALL_JOBS_IN_CLUSTER[SCHEDD]`
    If this macro is set to False (the default), when the
    *condor_schedd* fails to start an idle job, it will not try to
    start any other idle jobs in the same cluster during that
    negotiation cycle. This makes negotiation much more efficient for
    large job clusters. However, in some cases other jobs in the cluster
    can be started even though an earlier job can't. For example, the
    jobs' requirements may differ, because of different disk space,
    memory, or operating system requirements. Or, machines may be
    willing to run only some jobs in the cluster, because their
    requirements reference the jobs' virtual memory size or other
    attribute. Setting this macro to True will force the
    *condor_schedd* to try to start all idle jobs in each negotiation
    cycle. This will make negotiation cycles last longer, but it will
    ensure that all jobs that can be started will be started.

:macro-def:`PERIODIC_EXPR_INTERVAL[SCHEDD]`
    This macro determines the minimum period, in seconds, between
    evaluation of periodic job control expressions, such as
    periodic_hold, periodic_release, and periodic_remove, given by
    the user in an HTCondor submit file. By default, this value is 60
    seconds. A value of 0 prevents the *condor_schedd* from performing
    the periodic evaluations.

:macro-def:`MAX_PERIODIC_EXPR_INTERVAL[SCHEDD]`
    This macro determines the maximum period, in seconds, between
    evaluation of periodic job control expressions, such as
    periodic_hold, periodic_release, and periodic_remove, given by
    the user in an HTCondor submit file. By default, this value is 1200
    seconds. If HTCondor is behind on processing events, the actual
    period between evaluations may be higher than specified.

:macro-def:`PERIODIC_EXPR_TIMESLICE[SCHEDD]`
    This macro is used to adapt the frequency with which the
    *condor_schedd* evaluates periodic job control expressions. When
    the job queue is very large, the cost of evaluating all of the
    ClassAds is high, so in order for the *condor_schedd* to continue
    to perform well, it makes sense to evaluate these expressions less
    frequently. The default time slice is 0.01, so the *condor_schedd*
    will set the interval between evaluations so that it spends only 1%
    of its time in this activity. The lower bound for the interval is
    configured by :macro:`PERIODIC_EXPR_INTERVAL` (default 60 seconds)
    and the upper bound is configured with :macro:`MAX_PERIODIC_EXPR_INTERVAL`
    (default 1200 seconds).

:macro-def:`SYSTEM_PERIODIC_HOLD_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    job that is not in the ``HELD``, ``COMPLETED``, or ``REMOVED``
    state. Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_HOLD_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    not case-sensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_HOLD` expression will be evaluated. If any
    of these expression evaluates to ``True`` the job will be held.  See also
    :macro:`SYSTEM_PERIODIC_HOLD`
    There is no default value.

:macro-def:`SYSTEM_PERIODIC_HOLD[SCHED]` and :macro-def:`SYSTEM_PERIODIC_HOLD_<Name>[SCHEDD]`
    This expression behaves identically to the job expression
    ``periodic_hold``, but it is evaluated for every job in the queue.
    It defaults to ``False``. When ``True``, it causes the job to stop
    running and go on hold. Here is an example that puts jobs on hold if
    they have been restarted too many times, have an unreasonably large
    virtual memory :ad-attr:`ImageSize`, or have unreasonably large disk usage
    for an invented environment. 

    .. code-block:: condor-config

        if version > 9.5
           # use hold names if the version supports it
           SYSTEM_PERIODIC_HOLD_NAMES = Mem Disk
           SYSTEM_PERIODIC_HOLD_Mem = ImageSize > 3000000
           SYSTEM_PERIODIC_HOLD_Disk = JobStatus == 2 && DiskUsage > 10000000
           SYSTEM_PERIODIC_HOLD = JobStatus == 1 && JobRunCount > 10
        else
           SYSTEM_PERIODIC_HOLD = \
          (JobStatus == 1 || JobStatus == 2) && \
          (JobRunCount > 10 || ImageSize > 3000000 || DiskUsage > 10000000)
        endif

:macro-def:`SYSTEM_PERIODIC_HOLD_REASON[SCHEDD]` and :macro-def:`SYSTEM_PERIODIC_HOLD_<Name>_REASON[SCHEDD]`
    This string expression is evaluated when the job is placed on hold
    due to :macro:`SYSTEM_PERIODIC_HOLD` or :macro:`SYSTEM_PERIODIC_HOLD_<Name>` evaluating to ``True``. If it
    evaluates to a non-empty string, this value is used to set the job
    attribute :ad-attr:`HoldReason`. Otherwise, a default description is used.

:macro-def:`SYSTEM_PERIODIC_HOLD_SUBCODE[SCHEDD]` and :macro-def:`SYSTEM_PERIODIC_HOLD_<Name>_SUBCODE[SCHEDD]`
    This integer expression is evaluated when the job is placed on hold
    due to :macro:`SYSTEM_PERIODIC_HOLD` or :macro:`SYSTEM_PERIODIC_HOLD_<Name>` evaluating to ``True``. If it
    evaluates to a valid integer, this value is used to set the job
    attribute :ad-attr:`HoldReasonSubCode`. Otherwise, a default of 0 is used.
    The attribute :ad-attr:`HoldReasonCode` is set to 26, which indicates that
    the job went on hold due to a system job policy expression.

:macro-def:`SYSTEM_PERIODIC_RELEASE_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    job that is in the ``HELD`` state (jobs with a :ad-attr:`HoldReasonCode`
    value of ``1`` are ignored). Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_RELEASE_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    not case-sensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_RELEASE` expression will be evaluated. If any
    of these expressions evaluates to ``True`` the job will be released.  See also
    :macro:`SYSTEM_PERIODIC_RELEASE`
    There is no default value.

:macro-def:`SYSTEM_PERIODIC_RELEASE[SCHEDD]` and :macro-def:`SYSTEM_PERIODIC_RELEASE_<Name>[SCHEDD]`
    This expression behaves identically to a job's definition of a
    :subcom:`periodic_release[and SYSTEM_PERIODIC_RELEASE]`
    expression in a submit description file, but it is evaluated for
    every job in the queue. It defaults to ``False``. When ``True``, it
    causes a Held job to return to the Idle state. Here is an example
    that releases jobs from hold if they have tried to run less than 20
    times, have most recently been on hold for over 20 minutes, and have
    gone on hold due to ``Connection timed out`` when trying to execute
    the job, because the file system containing the job's executable is
    temporarily unavailable.

    .. code-block:: condor-config

        SYSTEM_PERIODIC_RELEASE = \
          (JobRunCount < 20 && (time() - EnteredCurrentStatus) > 1200 ) &&  \
            (HoldReasonCode == 6 && HoldReasonSubCode == 110)

:macro-def:`SYSTEM_PERIODIC_REMOVE_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    job in the queue. Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_REMOVE_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    not case-sensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_REMOVE` expression will be evaluated. If any
    of these expressions evaluates to ``True`` the job will be removed from the queue.  See also
    :macro:`SYSTEM_PERIODIC_REMOVE`
    There is no default value.

:macro-def:`SYSTEM_PERIODIC_REMOVE[SCHEDD]` and :macro-def:`SYSTEM_PERIODIC_REMOVE_<Name>[SCHEDD]`
    This expression behaves identically to the job expression
    ``periodic_remove``, but it is evaluated for every job in the queue.
    As it is in the configuration file, it is easy for an administrator
    to set a remove policy that applies to all jobs. It defaults to
    ``False``. When ``True``, it causes the job to be removed from the
    queue. Here is an example that removes jobs which have been on hold
    for 30 days:

    .. code-block:: condor-config

        SYSTEM_PERIODIC_REMOVE = \
          (JobStatus == 5 && time() - EnteredCurrentStatus > 3600*24*30)

:macro-def:`SYSTEM_PERIODIC_VACATE_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain an expression that will be periodically evaluated for each
    running job in the queue. Each name in the list will be used in the name of
    configuration variable ``SYSTEM_PERIODIC_VACATE_<Name>``. The named expressions
    are evaluated in the order in which names appear in this list. Names are
    case-insensitive. After all of the named expressions are evaluated,
    the nameless :macro:`SYSTEM_PERIODIC_VACATE` expression will be evaluated. If any
    of these expressions evaluates to ``True`` the job will be evicted
    from the machine it is running on, and returned to the queue as Idle.  See also
    :macro:`SYSTEM_PERIODIC_VACATE` There is no default value.

:macro-def:`SYSTEM_PERIODIC_VACATE[SCHEDD]` and :macro-def:`SYSTEM_PERIODIC_VACATE_<Name>[SCHEDD]`
    This expression behaves identically to the job expression
    ``periodic_vacate``, but it is evaluated for every running job in the queue.
    As it is in the configuration file, it is easy for an administrator
    to set a vacate policy that applies to all jobs. It defaults to
    ``False``. When ``True``, it causes the job to be evicted from the
    machine it is running on.

:macro-def:`SYSTEM_ON_VACATE_COOL_DOWN[SCHEDD]`
    This expression is evaluated whenever an execution attempt for a
    job is interrupted (i.e. the job does not exit of its own accord).
    If it evaluates to a positive integer, then the job is put into a
    cool-down state for that number of seconds. During this time, the
    job will not be run again.

:macro-def:`SCHEDD_ASSUME_NEGOTIATOR_GONE[SCHEDD]`
    This macro determines the period, in seconds, that the
    *condor_schedd* will wait for the *condor_negotiator* to initiate
    a negotiation cycle before the schedd will simply try to claim any
    local *condor_startd*. This allows for a machine that is acting as
    both a submit and execute node to run jobs locally if it cannot
    communicate with the central manager. The default value, if not
    specified, is 2,000,000 seconds (effectively never).  If this
    feature is desired, we recommend setting it to some small multiple
    of the negotiation cycle, say, 1200 seconds, or 20 minutes.

.. _GRACEFULLY_REMOVE_JOBS:

:macro-def:`GRACEFULLY_REMOVE_JOBS[SCHEDD]`
    A boolean value defaulting to ``True``.  If ``True``, jobs will be
    given a chance to shut down cleanly when removed.  In the vanilla
    universe, this means that the job will be sent the signal set in
    its ``SoftKillSig`` attribute, or ``SIGTERM`` if undefined; if the
    job hasn't exited after its max vacate time, it will be hard-killed
    (sent ``SIGKILL``).  Signals are different on Windows, and other
    details differ between universes.

    The submit command :subcom:`want_graceful_removal[and GRACEFULLY_REMOVE_JOBS]`
    overrides this configuration variable.

    See :macro:`MachineMaxVacateTime` for details on
    how HTCondor computes the job's max vacate time.

:macro-def:`SCHEDD_ROUND_ATTR_<xxxx>[SCHEDD]`
    This is used to round off attributes in the job ClassAd so that
    similar jobs may be grouped together for negotiation purposes. There
    are two cases. One is that a percentage such as 25% is specified. In
    this case, the value of the attribute named <xxxx>\\ in the job
    ClassAd will be rounded up to the next multiple of the specified
    percentage of the values order of magnitude. For example, a setting
    of 25% will cause a value near 100 to be rounded up to the next
    multiple of 25 and a value near 1000 will be rounded up to the next
    multiple of 250. The other case is that an integer, such as 4, is
    specified instead of a percentage. In this case, the job attribute
    is rounded up to the specified number of decimal places. Replace
    <xxxx> with the name of the attribute to round, and set this macro
    equal to the number of decimal places to round up. For example, to
    round the value of job ClassAd attribute ``foo`` up to the nearest
    100, set

    .. code-block:: condor-config

                SCHEDD_ROUND_ATTR_foo = 2

    When the schedd rounds up an attribute value, it will save the raw
    (un-rounded) actual value in an attribute with the same name
    appended with "_RAW". So in the above example, the raw value will
    be stored in attribute ``foo_RAW`` in the job ClassAd. The following
    are set by default:

    .. code-block:: condor-config

                SCHEDD_ROUND_ATTR_ResidentSetSize = 25%
                SCHEDD_ROUND_ATTR_ProportionalSetSizeKb = 25%
                SCHEDD_ROUND_ATTR_ImageSize = 25%
                SCHEDD_ROUND_ATTR_ExecutableSize = 25%
                SCHEDD_ROUND_ATTR_DiskUsage = 25%
                SCHEDD_ROUND_ATTR_NumCkpts = 4

    Thus, an ImageSize near 100MB will be rounded up to the next
    multiple of 25MB. If your batch slots have less memory or disk than
    the rounded values, it may be necessary to reduce the amount of
    rounding, because the job requirements will not be met.

:macro-def:`SCHEDD_BACKUP_SPOOL[SCHEDD]`
    A boolean value that, when ``True``, causes the *condor_schedd* to
    make a backup of the job queue as it starts. When ``True``, the
    *condor_schedd* creates a host-specific backup of the current spool
    file to the spool directory. This backup file will be overwritten
    each time the *condor_schedd* starts. Defaults to ``False``.

:macro-def:`SCHEDD_PREEMPTION_REQUIREMENTS[SCHEDD]`
    This boolean expression is utilized only for machines allocated by a
    dedicated scheduler. When ``True``, a machine becomes a candidate
    for job preemption. This configuration variable has no default; when
    not defined, preemption will never be considered.

:macro-def:`SCHEDD_PREEMPTION_RANK[SCHEDD]`
    This floating point value is utilized only for machines allocated by
    a dedicated scheduler. It is evaluated in context of a job ClassAd,
    and it represents a machine's preference for running a job. This
    configuration variable has no default; when not defined, preemption
    will never be considered.

:macro-def:`ParallelSchedulingGroup[SCHEDD]`
    For parallel jobs which must be assigned within a group of machines
    (and not cross group boundaries), this configuration variable is a
    string which identifies a group of which this machine is a member.
    Each machine within a group sets this configuration variable with a
    string that identifies the group.

:macro-def:`PER_JOB_HISTORY_DIR[SCHEDD]`
    If set to a directory writable by the HTCondor user, when a job
    leaves the *condor_schedd* 's queue, a copy of the job's ClassAd
    will be written in that directory. The files are named ``history``,
    with the job's cluster and process number appended. For example, job
    35.2 will result in a file named ``history.35.2``. HTCondor does not
    rotate or delete the files, so without an external entity to clean
    the directory, it can grow very large. This option defaults to being
    unset. When not set, no files are written.

:macro-def:`DEDICATED_SCHEDULER_USE_FIFO[SCHEDD]`
    When this parameter is set to true (the default), parallel universe
    jobs will be scheduled in a first-in, first-out manner. When set to
    false, parallel jobs are scheduled using a best-fit algorithm. Using
    the best-fit algorithm is not recommended, as it can cause
    starvation.

:macro-def:`DEDICATED_SCHEDULER_WAIT_FOR_SPOOLER[SCHEDD]`
    A boolean value that when ``True``, causes the dedicated scheduler
    to schedule parallel universe jobs in a very strict first-in,
    first-out manner. When the default value of ``False``, parallel jobs
    that are being remotely submitted to a scheduler and are on hold,
    waiting for spooled input files to arrive at the scheduler, will not
    block jobs that arrived later, but whose input files have finished
    spooling. When ``True``, jobs with larger cluster IDs, but that are
    in the Idle state will not be scheduled to run until all earlier
    jobs have finished spooling in their input files and have been
    scheduled.

:macro-def:`SCHEDD_SEND_VACATE_VIA_TCP[SCHEDD]`
    A boolean value that defaults to ``True``. When ``True``, the
    *condor_schedd* daemon sends vacate signals via TCP, instead of the
    default UDP.

:macro-def:`SCHEDD_CLUSTER_INITIAL_VALUE[SCHEDD]`
    An integer that specifies the initial cluster number value to use
    within a job id when a job is first submitted. If the job cluster
    number reaches the value set by :macro:`SCHEDD_CLUSTER_MAXIMUM_VALUE` and
    wraps, it will be re-set to the value given by this variable. The
    default value is 1.

:macro-def:`SCHEDD_CLUSTER_INCREMENT_VALUE[SCHEDD]`
    A positive integer that defaults to 1, representing a stride used
    for the assignment of cluster numbers within a job id. When a job is
    submitted, the job will be assigned a job id. The cluster number of
    the job id will be equal to the previous cluster number used plus
    the value of this variable.

:macro-def:`SCHEDD_CLUSTER_MAXIMUM_VALUE[SCHEDD]`
    An integer that specifies an upper bound on assigned job cluster id
    values. For value M, the maximum job cluster id assigned to any job
    will be M - 1. When the maximum id is reached, cluster ids will
    continue assignment using :macro:`SCHEDD_CLUSTER_INITIAL_VALUE`. The
    default value of this variable is zero, which represents the
    behavior of having no maximum cluster id value.

    Note that HTCondor does not check for nor take responsibility for
    duplicate cluster ids for queued jobs. If
    :macro:`SCHEDD_CLUSTER_MAXIMUM_VALUE` is set to a non-zero value, the
    system administrator is responsible for ensuring that older jobs do
    not stay in the queue long enough for cluster ids of new jobs to
    wrap around and reuse the same id. With a low enough value, it is
    possible for jobs to be erroneously assigned duplicate cluster ids,
    which will result in a corrupt job queue.

:macro-def:`SCHEDD_JOB_QUEUE_LOG_FLUSH_DELAY[SCHEDD]`
    An integer which specifies an upper bound in seconds on how long it
    takes for changes to the job ClassAd to be visible to the HTCondor
    Job Router. The default is 5 seconds.

:macro-def:`ROTATE_HISTORY_DAILY[SCHEDD]`
    A boolean value that defaults to ``False``. When ``True``, the
    history file will be rotated daily, in addition to the rotations
    that occur due to the definition of :macro:`MAX_HISTORY_LOG` that rotate
    due to size.

:macro-def:`ROTATE_HISTORY_MONTHLY[SCHEDD]`
    A boolean value that defaults to ``False``. When ``True``, the
    history file will be rotated monthly, in addition to the rotations
    that occur due to the definition of :macro:`MAX_HISTORY_LOG` that rotate
    due to size.

:macro-def:`SCHEDD_COLLECT_STATS_FOR_<Name>[SCHEDD]`
    A boolean expression that when ``True`` creates a set of
    *condor_schedd* ClassAd attributes of statistics collected for a
    particular set. These attributes are named using the prefix of
    ``<Name>``. The set includes each entity for which this expression
    is ``True``. As an example, assume that *condor_schedd* statistics
    attributes are to be created for only user Einstein's jobs. Defining

    .. code-block:: condor-config

          SCHEDD_COLLECT_STATS_FOR_Einstein = (Owner=="einstein")

    causes the creation of the set of statistics attributes with names
    such as ``EinsteinJobsCompleted`` and ``EinsteinJobsCoredumped``.

:macro-def:`SCHEDD_COLLECT_STATS_BY_<Name>[SCHEDD]`
    Defines a string expression. The evaluated string is used in the
    naming of a set of *condor_schedd* statistics ClassAd attributes.
    The naming begins with ``<Name>``, an underscore character, and the
    evaluated string. Each character not permitted in an attribute name
    will be converted to the underscore character. For example,

    .. code-block:: condor-config

          SCHEDD_COLLECT_STATS_BY_Host = splitSlotName(RemoteHost)[1]

    a set of statistics attributes will be created and kept. If the
    string expression were to evaluate to ``"storm.04.cs.wisc.edu"``,
    the names of two of these attributes will be
    ``Host_storm_04_cs_wisc_edu_JobsCompleted`` and
    ``Host_storm_04_cs_wisc_edu_JobsCoredumped``.

:macro-def:`SCHEDD_EXPIRE_STATS_BY_<Name>[SCHEDD]`
    The number of seconds after which the *condor_schedd* daemon will
    stop collecting and discard the statistics for a subset identified
    by ``<Name>``, if no event has occurred to cause any counter or
    statistic for the subset to be updated. If this variable is not
    defined for a particular ``<Name>``, then the default value will be
    ``60*60*24*7``, which is one week's time.

:macro-def:`SIGNIFICANT_ATTRIBUTES[SCHEDD]`
    A comma and/or space separated list of job ClassAd attributes that
    are to be added to the list of attributes for determining the sets
    of jobs considered as a unit (an auto cluster) in negotiation, when
    auto clustering is enabled. When defined, this list replaces the
    list that the *condor_negotiator* would define based upon machine
    ClassAds.

:macro-def:`ADD_SIGNIFICANT_ATTRIBUTES[SCHEDD]`
    A comma and/or space separated list of job ClassAd attributes that
    will always be added to the list of attributes that the
    *condor_negotiator* defines based upon machine ClassAds, for
    determining the sets of jobs considered as a unit (an auto cluster)
    in negotiation, when auto clustering is enabled.

:macro-def:`REMOVE_SIGNIFICANT_ATTRIBUTES[SCHEDD]`
    A comma and/or space separated list of job ClassAd attributes that
    are removed from the list of attributes that the
    *condor_negotiator* defines based upon machine ClassAds, for
    determining the sets of jobs considered as a unit (an auto cluster)
    in negotiation, when auto clustering is enabled.

:macro-def:`SCHEDD_SEND_RESCHEDULE[SCHEDD]`
    A boolean value which defaults to true.  Set to false for 
    schedds like those in the HTCondor-CE that have no negotiator
    associated with them, in order to reduce spurious error messages
    in the SchedLog file.

:macro-def:`SCHEDD_AUDIT_LOG[SCHEDD]`
    The path and file name of the *condor_schedd* log that records
    user-initiated commands that modify the job queue. If not defined,
    there will be no *condor_schedd* audit log.

:macro-def:`MAX_SCHEDD_AUDIT_LOG[SCHEDD]`
    Controls the maximum amount of time that a log will be allowed to
    grow. When it is time to rotate a log file, it will be saved to a
    file with an ISO timestamp suffix. The oldest rotated file receives
    the file name suffix ``.old``. The ``.old`` files are overwritten
    each time the maximum number of rotated files (determined by the
    value of :macro:`MAX_NUM_SCHEDD_AUDIT_LOG`) is exceeded. A value of 0
    specifies that the file may grow without bounds. The following
    suffixes may be used to qualify the integer:

        ``Sec`` for seconds
        ``Min`` for minutes
        ``Hr`` for hours
        ``Day`` for days
        ``Wk`` for weeks

:macro-def:`MAX_NUM_SCHEDD_AUDIT_LOG[SCHEDD]`
    The integer that controls the maximum number of rotations that the
    *condor_schedd* audit log is allowed to perform, before the oldest
    one will be rotated away. The default value is 1.

:macro-def:`SCHEDD_USE_SLOT_WEIGHT[SCHEDD]`
    A boolean that defaults to ``False``. When ``True``, the
    *condor_schedd* does use configuration variable :macro:`SLOT_WEIGHT` to
    weight running and idle job counts in the submitter ClassAd.

:macro-def:`EXTENDED_SUBMIT_COMMANDS[SCHEDD]`
    A long form ClassAd that defines extended submit commands and their associated
    job ad attributes for a specific Schedd.  :tool:`condor_submit` will query the
    destination schedd for this ClassAd and use it to modify the internal
    table of submit commands before interpreting the submit file.

    Each entry in this ClassAd will define a new submit command, the value will
    indicate the required data type to the submit file parser with the data type
    given by example from the value according to this list of types

    -  *string-list* - a quoted string containing a comma. e.g. ``"a,b"``. *string-list* values
       are converted to canonical form.
    -  *filename* - a quoted string beginning with the word file.  e.g. ``"filename"``. *filename* values
       are converted to fully qualified file paths using the same rules as other submit filenames.
    -  *string* - a quoted string that does not match the above special rules. e.g. ``"string"``. *string* values
       can be provided quoted or unquoted in the submit file.  Unquoted values will have leading and trailing
       whitespace removed.
    -  *unsigned-integer* - any non-negative integer e.g. ``0``. *unsigned-integer* values are evaluated as expressions
       and submit will fail if the result does not convert to an unsigned integer.  A simple integer value
       will be stored in the job.
    -  *integer* - any negative integer e.g. ``-1``.  *integer* values are evaluated as expressions and submit
       will fail if the result does not convert to an integer.  A simple integer value will be stored in the job.
    -  *boolean* - any boolean value e.g. ``true``. *boolean* values are evaluated as expressions and submit will
       fail if the result does not convert to ``true`` or ``false``.
    -  *expression* - any expression or floating point number that is not one of the above. e.g. ``a+b``. *expression*
       values will be parsed as a classad expression and stored in the job.
    -  *error* - the literal ``error`` will tell submit to generate an error when the command is used. 
       this provides a way for admins to disable existing submit commands.
    -  *undefined* - the literal ``undefined`` will be treated by :tool:`condor_submit` as if that
       attribute is not in this ad. This is intended to aid composibility of this ad across multiple
       configuration files.

    The following example will add four new submit commands and disable the use of the
    the ``accounting_group_user`` submit command.

    .. code-block:: condor-config

          EXTENDED_SUBMIT_COMMANDS @=end
             LongJob = true
             Project = "string"
             FavoriteFruit = "a,b"
             SomeFile = "filename"
             accounting_group_user = error
          @end

:macro-def:`EXTENDED_SUBMIT_HELPFILE[SCHEDD]`
    A URL or file path to text describing how the *condor_schedd* extends the submit schema. Use this to document
    for users the extended submit commands defined by the configuration variable :macro:`EXTENDED_SUBMIT_COMMANDS`.
    :tool:`condor_submit` will display this URL or the text of this file when the user uses the ``-capabilities`` option.

:macro-def:`SUBMIT_TEMPLATE_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain a set of submit commands.  Each name in the list will be used in the name of
    the configuration variable :macro:`SUBMIT_TEMPLATE_<Name>`.
    Names are not case-sensitive. There is no default value.  Submit templates are
    used by :tool:`condor_submit` when parsing submit files, so administrators or users can
    add submit templates to the configuration of :tool:`condor_submit` to customize the
    schema or to simplify the creation of submit files.

:macro-def:`SUBMIT_TEMPLATE_<Name>[SCHEDD]`
    A single submit template containing one or more submit commands.
    The template can be invoked with or without arguments.  The template
    can refer arguments by number using the ``$(<N>)`` where ``<N>`` is
    a value from 0 thru 9.  ``$(0)`` expands to all of the arguments,
    ``$(1)`` to the first argument, ``$(2)`` to the second argument, and so on.
    The argument number can be followed by ``?`` to test if the argument
    was specified, or by ``+`` to expand to that argument and all subsequent
    arguments.  Thus ``$(0)`` and ``$(1+)`` will expand to the same thing.

    For example:

    .. code-block:: condor-config

          SUBMIT_TEMPLATE_NAMES = $(SUBMIT_TEMPLATE_NAMES) Slurm
          SUBMIT_TEMPLATE_Slurm @=tpl
             if ! $(1?)
                error : Template:Slurm requires at least 1 argument - Slurm(project, [queue [, resource_args...])
             endif
             universe = Grid
             grid_resource = batch slurm $(3)
             batch_project = $(1)
             batch_queue = $(2:Default)
          @tpl

    This could be used in a submit file in this way:

    .. code-block:: condor-submit

          use template : Slurm(Blue Book)


:macro-def:`JOB_TRANSFORM_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    contain a set of rules governing the transformation of jobs during
    submission. Each name in the list will be used in the name of
    configuration variable :macro:`JOB_TRANSFORM_<Name>`. Transforms are
    applied in the order in which names appear in this list. Names are
    not case-sensitive. There is no default value.

:macro-def:`JOB_TRANSFORM_<Name>[SCHEDD]`
    A single job transform specified as a set of transform rules.
    The syntax for these rules is specified in :ref:`classads/transforms:ClassAd Transforms`
    The transform rules are applied to jobs that match
    the transform's ``REQUIREMENTS`` expression as they are submitted.
    ``<Name>`` corresponds to a name listed in :macro:`JOB_TRANSFORM_NAMES`.
    Names are not case-sensitive. There is no default value.
    For jobs submitted as late materialization factories, the factory Cluster ad is transformed
    at submit time.  When job ads are later materialized, attribute values set by the transform
    will override values set by the job factory for those attributes.

:macro-def:`SUBMIT_REQUIREMENT_NAMES[SCHEDD]`
    A comma and/or space separated list of unique names, where each is
    used in the formation of a configuration variable name that will
    represent an expression evaluated to decide whether or not to reject
    a job submission. Each name in the list will be used in the name of
    configuration variable :macro:`SUBMIT_REQUIREMENT_<Name>`. There is no
    default value.

:macro-def:`SUBMIT_REQUIREMENT_<Name>[SCHEDD]`
    A boolean expression evaluated in the context of the
    *condor_schedd* daemon ClassAd, which is the ``SCHEDD.`` or ``MY.``
    name space and the job ClassAd, which is the ``JOB.`` or ``TARGET.``
    name space. When ``False``, it causes the *condor_schedd* to reject
    the submission of the job or cluster of jobs. ``<Name>`` corresponds
    to a name listed in :macro:`SUBMIT_REQUIREMENT_NAMES`. There is no
    default value.

:macro-def:`SUBMIT_REQUIREMENT_<Name>_REASON[SCHEDD]`
    An expression that evaluates to a string, to be printed for the job
    submitter when :macro:`SUBMIT_REQUIREMENT_<Name>` evaluates to ``False``
    and the *condor_schedd* rejects the job. There is no default value.

:macro-def:`SCHEDD_RESTART_REPORT[SCHEDD]`
    The complete path to a file that will be written with report
    information. The report is written when the *condor_schedd* starts.
    It contains statistics about its attempts to reconnect to the
    *condor_startd* daemons for all jobs that were previously running.
    The file is updated periodically as reconnect attempts succeed or
    fail. Once all attempts have completed, a copy of the report is
    emailed to address specified by :macro:`CONDOR_ADMIN`. The default value
    is ``$(LOG)/ScheddRestartReport``. If a blank value is set, then no
    report is written or emailed.

:macro-def:`JOB_SPOOL_PERMISSIONS[SCHEDD]`
    Control the permissions on the job's spool directory. Defaults to
    ``user`` which sets permissions to 0700. Possible values are
    ``user``, ``group``, and ``world``. If set to ``group``, then the
    directory is group-accessible, with permissions set to 0750. If set
    to ``world``, then the directory is created with permissions set to
    0755.

:macro-def:`CHOWN_JOB_SPOOL_FILES[SCHEDD]`
    Prior to HTCondor 8.5.0 on unix, the condor_schedd would chown job
    files in the SPOOL directory between the condor account and the
    account of the job submitter. Now, these job files are always owned
    by the job submitter by default. To restore the older behavior, set
    this parameter to ``True``. The default value is ``False``.

:macro-def:`IMMUTABLE_JOB_ATTRS[SCHEDD]`
    A comma and/or space separated list of attributes provided by the
    administrator that cannot be changed, once they have committed
    values. No attributes are in this list by default.

:macro-def:`SYSTEM_IMMUTABLE_JOB_ATTRS[SCHEDD]`
    A predefined comma and/or space separated list of attributes that
    cannot be changed, once they have committed values. The hard-coded
    value is: :ad-attr:`Owner` :ad-attr:`ClusterId` :ad-attr:`ProcId` :ad-attr:`MyType`
    :ad-attr:`TargetType`.

:macro-def:`PROTECTED_JOB_ATTRS[SCHEDD]`
    A comma and/or space separated list of attributes provided by the
    administrator that can only be altered by the queue super-user, once
    they have committed values. No attributes are in this list by
    default.

:macro-def:`SYSTEM_PROTECTED_JOB_ATTRS[SCHEDD]`
    A predefined comma and/or space separated list of attributes that
    can only be altered by the queue super-user, once they have
    committed values. The hard-code value is empty.

:macro-def:`ALTERNATE_JOB_SPOOL[SCHEDD]`
    A ClassAd expression evaluated in the context of the job ad. If the
    result is a string, the value is used an an alternate spool
    directory under which the job's files will be stored. This alternate
    directory must already exist and have the same file ownership and
    permissions as the main :macro:`SPOOL` directory. Care must be taken that
    the value won't change during the lifetime of each job.

:macro-def:`<OAuth2Service>_CLIENT_ID[SCHEDD]`
    The client ID string for an OAuth2 service named ``<OAuth2Service>``.
    The client ID is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_CLIENT_SECRET_FILE[SCHEDD]`
    The path to the file containing the client secret string
    for an OAuth2 service named ``<OAuth2Service>``.
    The client secret is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_RETURN_URL_SUFFIX[SCHEDD]`
    The path (``https://<hostname>/<path>``)
    that an OAuth2 service named ``<OAuth2Service>``
    should be directed when returning
    after a user permits the submit host access
    to their account.
    Most often, this should be set to name of the OAuth2 service
    (e.g. ``box``, ``gdrive``, ``onedrive``, etc.).
    The derived return URL is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_AUTHORIZATION_URL[SCHEDD]`
    The URL that the companion OAuth2 credmon WSGI application
    should redirect a user to
    in order to request access for a user's credentials
    for the OAuth2 service named ``<OAuth2Service>``.
    This URL should be found in the service's API documentation.
    The authorization URL is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`<OAuth2Service>_TOKEN_URL[SCHEDD]`
    The URL that the *condor_credmon_oauth* should use
    in order to refresh a user's tokens
    for the OAuth2 service named ``<OAuth2Service>``.
    This URL should be found in the service's API documentation.
    The token URL is passed on to the *condor_credmon_oauth*
    when a job requests OAuth2 credentials
    for a configured OAuth2 service.

:macro-def:`CHECKPOINT_DESTINATION_MAPFILE[SCHEDD]`
    The location on disk of the file which maps from checkpoint destinations
    to how invoke the corresponding clean-up plug-in.  Defaults to
    ``$(ETC)/checkpoint-destination-mapfile``.

:macro-def:`SCHEDD_CHECKPOINT_CLEANUP_TIMEOUT[SCHEDD]`
    There's only so long that the *condor_schedd* is willing to let clean-up
    for a single job (including all of its checkpoints) take.  This macro
    defines that duration (as an integer number of seconds).

:macro-def:`DEFAULT_NUM_EXTRA_CHECKPOINTS[SCHEDD]`
    By default, how many "extra" checkpoints should HTCondor store for a
    self-checkpoint job using third-party storage.

:macro-def:`USE_JOBSETS[SCHEDD]`
    Boolean to enable the use of job sets with the `htcondor jobset` command.
    Defaults to false.

:macro-def:`ENABLE_HTTP_PUBLIC_FILES[SCHEDD]`
    A boolean that defaults to false.  When true, the schedd will
    use an external http server to transfer public input file.

:macro-def:`HTTP_PUBLIC_FILES_ADDRESS[SCHEDD]`
    The full web address (hostname + port) where your web server is serving files (default:
    127.0.0.1:80)

:macro-def:`HTTP_PUBLIC_FILES_ROOT_DIR[SCHEDD]`
    Absolute path to the local directory where the web service is serving files from.

:macro-def:`HTTP_PUBLIC_FILES_USER[SCHEDD]`
   User security level used to write links to the directory specified by
   HTTP_PUBLIC_FILES_ROOT_DIR. There are three valid options for
   this knob:  **<user>**, **<condor>** or **<%username%>**

condor_shadow Configuration File Entries
-----------------------------------------

:index:`condor_shadow configuration variables<single: condor_shadow configuration variables; configuration>`

These settings affect the *condor_shadow*.

:macro-def:`SHADOW_LOCK[SHADOW]`
    This macro specifies the lock file to be used for access to the
    ``ShadowLog`` file. It must be a separate file from the
    ``ShadowLog``, since the ``ShadowLog`` may be rotated and you want
    to synchronize access across log file rotations. This macro is
    defined relative to the ``$(LOCK)`` macro.

:macro-def:`SHADOW_DEBUG[SHADOW]`
    This macro (and other settings related to debug logging in the
    shadow) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`SHADOW_QUEUE_UPDATE_INTERVAL[SHADOW]`
    The amount of time (in seconds) between ClassAd updates that the
    *condor_shadow* daemon sends to the *condor_schedd* daemon.
    Defaults to 900 (15 minutes).

:macro-def:`SHADOW_LAZY_QUEUE_UPDATE[SHADOW]`
    This boolean macro specifies if the *condor_shadow* should
    immediately update the job queue for certain attributes (at this
    time, it only effects the :ad-attr:`NumJobStarts` and :ad-attr:`NumJobReconnects`
    counters) or if it should wait and only update the job queue on the
    next periodic update. There is a trade-off between performance and
    the semantics of these attributes, which is why the behavior is
    controlled by a configuration macro. If the *condor_shadow* do not
    use a lazy update, and immediately ensures the changes to the job
    attributes are written to the job queue on disk, the semantics for
    the attributes are very solid (there's only a tiny chance that the
    counters will be out of sync with reality), but this introduces a
    potentially large performance and scalability problem for a busy
    *condor_schedd*. If the *condor_shadow* uses a lazy update, there
    is no additional cost to the *condor_schedd*, but it means that
    :tool:`condor_q` will not immediately see the changes to the job
    attributes, and if the *condor_shadow* happens to crash or be
    killed during that time, the attributes are never incremented. Given
    that the most obvious usage of these counter attributes is for the
    periodic user policy expressions (which are evaluated directly by
    the *condor_shadow* using its own copy of the job's ClassAd, which
    is immediately updated in either case), and since the additional
    cost for aggressive updates to a busy *condor_schedd* could
    potentially cause major problems, the default is ``True`` to do
    lazy, periodic updates.

:macro-def:`SHADOW_WORKLIFE[SHADOW]`
    The integer number of seconds after which the *condor_shadow* will
    exit when the current job finishes, instead of fetching a new job to
    manage. Having the *condor_shadow* continue managing jobs helps
    reduce overhead and can allow the *condor_schedd* to achieve higher
    job completion rates. The default is 3600, one hour. The value 0
    causes *condor_shadow* to exit after running a single job.

:macro-def:`SHADOW_JOB_CLEANUP_RETRY_DELAY[SHADOW]`
    This integer specifies the number of seconds to wait between tries
    to commit the final update to the job ClassAd in the
    *condor_schedd* 's job queue. The default is 30.

:macro-def:`SHADOW_MAX_JOB_CLEANUP_RETRIES[SHADOW]`
    This integer specifies the number of times to try committing the
    final update to the job ClassAd in the *condor_schedd* 's job
    queue. The default is 5.

:macro-def:`SHADOW_CHECKPROXY_INTERVAL[SHADOW]`
    The number of seconds between tests to see if the job proxy has been
    updated or should be refreshed. The default is 600 seconds (10
    minutes). This variable's value should be small in comparison to the
    refresh interval required to keep delegated credentials from
    expiring (configured via :macro:`DELEGATE_JOB_GSI_CREDENTIALS_REFRESH`
    and :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`). If this
    variable's value is too small, proxy updates could happen very
    frequently, potentially creating a lot of load on the submit
    machine.

:macro-def:`SHADOW_RUN_UNKNOWN_USER_JOBS[SHADOW]`
    A boolean that defaults to ``False``. When ``True``, it allows the
    *condor_shadow* daemon to run jobs as user nobody when remotely
    submitted and from users not in the local password file.

:macro-def:`SHADOW_STATS_LOG[SHADOW]`
    The full path and file name of a file that stores TCP statistics for
    shadow file transfers. (Note that the shadow logs TCP statistics to
    this file by default. Adding ``D_STATS`` to the :macro:`SHADOW_DEBUG`
    value will cause TCP statistics to be logged to the normal shadow
    log file (``$(SHADOW_LOG)``).) If not defined, :macro:`SHADOW_STATS_LOG`
    defaults to ``$(LOG)/XferStatsLog``. Setting :macro:`SHADOW_STATS_LOG` to
    ``/dev/null`` disables logging of shadow TCP file transfer
    statistics.

:macro-def:`MAX_SHADOW_STATS_LOG[SHADOW]`
    Controls the maximum size in bytes or amount of time that the shadow
    TCP statistics log will be allowed to grow. If not defined,
    :macro:`MAX_SHADOW_STATS_LOG` defaults to ``$(MAX_DEFAULT_LOG)``, which
    currently defaults to 10 MiB in size. Values are specified with the
    same syntax as :macro:`MAX_DEFAULT_LOG`.

:macro-def:`ALLOW_TRANSFER_REMAP_TO_MKDIR[SHADOW]`
    A boolean value that when ``True`` allows the *condor_shadow* to
    create directories in a transfer output remap path when the directory
    does not exist already. The *condor_shadow* can not create directories
    if the remap is an absolute path or if the remap tries to write to
    a directory specified within ``LIMIT_DIRECTORY_ACCESS``.

:macro-def:`JOB_EPOCH_HISTORY[SHADOW]`
    A full path and filename of a file where the *condor_shadow* will
    write to a per run job history file in an analogous way to that of
    the history file defined by the configuration variable :macro:`HISTORY`.
    It will be rotated in the same way, and has similar parameters that
    apply to the :macro:`HISTORY` file rotation apply to the *condor_shadow*
    daemon epoch history as well. This can be read with the :tool:`condor_history`
    command using the -epochs option. The default value is ``$(SPOOL)/epoch_history``.

    .. code-block:: console

        $ condor_history -epochs

:macro-def:`MAX_EPOCH_HISTORY_LOG[SHADOW]`
    Defines the maximum size for the epoch history file, in bytes. It
    defaults to 20MB.

:macro-def:`MAX_EPOCH_HISTORY_ROTATIONS[SHADOW]`
    Controls the maximum number of backup epoch history files to be kept.
    It defaults to 2, which means that there may be up to three epoch history
    files (two backups, plus the epoch history file that is being currently
    written to). When the epoch history file is rotated, and this rotation
    would cause the number of backups to be too large, the oldest file is removed.

:macro-def:`JOB_EPOCH_HISTORY_DIR[SHADOW]`
    A full path to an existing directory that the *condor_shadow* will write
    the jobs current job ad to a per job run history file with the name
    ``job.runs.X.Y.ads``. Where ``X`` is the jobs cluster id and ``Y`` is
    the jobs process id. For example, job 35.2 would write a job ad for each run
    to the file ``job.runs.35.2.ads``. These files can be read through :tool:`condor_history`
    when ran with the -epochs and -directory options.

    .. code-block:: console

        $ condor_history -epochs -directory

    HTCondor does not automatically  delete these files, so unchecked the
    directory can grow very large. Either an external entity needs to clean
    up or :tool:`condor_history` can use the -epochs options optional ``:d``
    extension to read and delete the files.

    .. code-block:: console

        $ condor_history -epochs:d -directory

condor_starter Configuration File Entries
------------------------------------------

:index:`condor_starter configuration variables<single: condor_starter configuration variables; configuration>`

These settings affect the *condor_starter*.

:macro-def:`DISABLE_SETUID[STARTER]`
    HTCondor can prevent jobs from running setuid executables
    on Linux by setting the no-new-privileges flag.  This can be
    enabled (i.e. to disallow setuid binaries) by setting :macro:`DISABLE_SETUID`
    to true.

:macro-def:`JOB_RENICE_INCREMENT[STARTER]`
    When the *condor_starter* spawns an HTCondor job, it can do so with
    a nice-level. A nice-level is a Unix mechanism that allows users to
    assign their own processes a lower priority, such that these
    processes do not interfere with interactive use of the machine. For
    machines with lots of real memory and swap space, such that the only
    scarce resource is CPU time, use this macro in conjunction with a
    policy that allows HTCondor to always start jobs on the machines.
    HTCondor jobs would always run, but interactive response on the
    machines would never suffer. A user most likely will not notice
    HTCondor is running jobs. See
    :doc:`/admin-manual/ep-policy-configuration` for more details on setting up a
    policy for starting and stopping jobs on a given machine.

    The ClassAd expression is evaluated in the context of the job ad to
    an integer value, which is set by the *condor_starter* daemon for
    each job just before the job runs. The range of allowable values are
    integers in the range of 0 to 19 (inclusive), with a value of 19
    being the lowest priority. If the integer value is outside this
    range, then on a Unix machine, a value greater than 19 is
    auto-decreased to 19; a value less than 0 is treated as 0. For
    values outside this range, a Windows machine ignores the value and
    uses the default instead. The default value is 0, on Unix, and the
    idle priority class on a Windows machine.

:macro-def:`STARTER_LOCAL_LOGGING[STARTER]`
    This macro determines whether the starter should do local logging to
    its own log file, or send debug information back to the
    *condor_shadow* where it will end up in the ShadowLog. It defaults
    to ``True``.

:macro-def:`STARTER_LOG_NAME_APPEND[STARTER]`
    A fixed value that sets the file name extension of the local log
    file used by the *condor_starter* daemon. Permitted values are
    ``true``, ``false``, ``slot``, ``cluster`` and ``jobid``. A value of
    ``false`` will suppress the use of a file extension. A value of
    ``true`` gives the default behavior of using the slot name, unless
    there is only a single slot. A value of ``slot`` uses the slot name.
    A value of ``cluster`` uses the job's :ad-attr:`ClusterId` ClassAd
    attribute. A value of ``jobid`` uses the job's :ad-attr:`ClusterId` and
    :ad-attr:`ProcId` ClassAd
    attributes. If ``cluster`` or ``jobid`` are specified, the resulting
    log files will persist until deleted by the user, so these two
    options should only be used to assist in debugging, not as permanent
    options.

:macro-def:`STARTER_DEBUG[STARTER]`
    This setting (and other settings related to debug logging in the
    starter) is described above in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`STARTER_NUM_THREADS_ENV_VARS[STARTER]`
    A string containing a list of job environment variables to set equal to
    the number of cores allocated into the slot.  Many commonly used computing
    libraries and programs will look at the value of environment
    variables, such as ``OMP_NUM_THREADS``, to control how many CPU cores to use.  
    Defaults to
    CUBACORES, GOMAXPROCS, JULIA_NUM_THREADS, MKL_NUM_THREADS,
    NUMEXPR_NUM_THREADS, OMP_NUM_THREADS, OMP_THREAD_LIMIT,
    OPENBLAS_NUM_THREADS, PYTHON_CPU_COUNT, ROOT_MAX_THREADS, TF_LOOP_PARALLEL_ITERATIONS,
    TF_NUM_THREADS.

:macro-def:`STARTER_FILE_XFER_STALL_TIMEOUT`
    This value defaults to 3600 (seconds).  It controlls the amount of
    time a file transfer can stall before the starter evicts the job.
    A stall can happen when the sandbox is on an NFS server that it down,
    or the network has broken.

:macro-def:`STARTER_UPDATE_INTERVAL[STARTER]`
    An integer value representing the number of seconds between ClassAd
    updates that the *condor_starter* daemon sends to the
    *condor_shadow* and *condor_startd* daemons. Defaults to 300 (5
    minutes).

:macro-def:`STARTER_INITIAL_UPDATE_INTERVAL[STARTER]`
    An integer value representing the number of seconds before the
    first ClassAd update from the *condor_starter* to the *condor_shadow*
    and *condor_startd*.  Defaults to 2 seconds.  On extremely
    large systems which frequently launch all starters at the same time,
    setting this to a random delay may help spread out starter updates
    over time.

:macro-def:`STARTER_UPDATE_INTERVAL_TIMESLICE[STARTER]`
    A floating point value, specifying the highest fraction of time that
    the *condor_starter* daemon should spend collecting monitoring
    information about the job, such as disk usage. The default value is
    0.1. If monitoring, such as checking disk usage takes a long time,
    the *condor_starter* will monitor less frequently than specified by
    :macro:`STARTER_UPDATE_INTERVAL`.

:macro-def:`STARTER_UPDATE_INTERVAL_MAX[STARTER]`
    An integer value representing an upper bound on the number of 
    seconds between updates controlled by :macro:`STARTER_UPDATE_INTERVAL` and
    :macro:`STARTER_UPDATE_INTERVAL_TIMESLICE`.  It is recommended to leave this parameter
    at its default value, which is calculated 
    as :macro:`STARTER_UPDATE_INTERVAL` * ( 1 / :macro:`STARTER_UPDATE_INTERVAL_TIMESLICE` )

:macro-def:`USER_JOB_WRAPPER[STARTER]`
    The full path and file name of an executable or script. If
    specified, HTCondor never directly executes a job, but instead
    invokes this executable, allowing an administrator to specify the
    executable (wrapper script) that will handle the execution of all
    user jobs. The command-line arguments passed to this program will
    include the full path to the actual user job which should be
    executed, followed by all the command-line parameters to pass to the
    user job. This wrapper script must ultimately replace its image with
    the user job; thus, it must exec() the user job, not fork() it.

    For Bourne type shells (*sh*, *bash*, *ksh*), the last line should
    be:

    .. code-block:: bash

        exec "$@"

    For the C type shells (*csh*, *tcsh*), the last line should be:

    .. code-block:: csh

        exec $*:q

    On Windows, the end should look like:

    .. code-block:: bat

        REM set some environment variables
        set LICENSE_SERVER=192.168.1.202:5012
        set MY_PARAMS=2

        REM Run the actual job now
        %*

    This syntax is precise, to correctly handle program arguments which
    contain white space characters.

    For Windows machines, the wrapper will either be a batch script with
    a file extension of ``.bat`` or ``.cmd``, or an executable with a
    file extension of ``.exe`` or ``.com``.

    If the wrapper script encounters an error as it runs, and it is
    unable to run the user job, it is important that the wrapper script
    indicate this to the HTCondor system so that HTCondor does not
    assign the exit code of the wrapper script to the job. To do this,
    the wrapper script should write a useful error message to the file
    named in the environment variable ``_CONDOR_WRAPPER_ERROR_FILE``,
    and then the wrapper script should exit with a non-zero value. If
    this file is created by the wrapper script, HTCondor assumes that
    the wrapper script has failed, and HTCondor will place the job back
    in the queue marking it as Idle, such that the job will again be
    run. The *condor_starter* will also copy the contents of this error
    file to the *condor_starter* log, so the administrator can debug
    the problem.

    When a wrapper script is in use, the executable of a job submission
    may be specified by a relative path, as long as the submit
    description file also contains:

    .. code-block:: condor-submit

                +PreserveRelativeExecutable = True

    For example,

    .. code-block:: condor-submit

                # Let this executable be resolved by user's path in the wrapper
                cmd = sleep
                +PreserveRelativeExecutable = True

    Without this extra attribute:

    .. code-block:: condor-submit

                # A typical fully-qualified executable path
                cmd = /bin/sleep

:macro-def:`CGROUP_MEMORY_LIMIT_POLICY[STARTER]`
    A string with possible values of ``hard``, ``custom`` and ``none``.
    The default value is ``hard``. If set to ``hard``, when the job tries
    to use more memory than the slot size, it will be put on hold with
    an appropriate message.  Also, the cgroup soft limit will set to
    90% of the hard limit to encourage the kernel to lower 
    cacheable memory the job is using.  If set to ``none``, no limit will be enforced, 
    but the memory usage of the job will be accurately measured by a cgroup.
    When set to custom, the additional knob CGROUP_HARD_MEMORY_LIMIT_EXPR
    must be set, which is a classad expression evaluated
    in the context of the machine and the job, respectively, to determine the hard limits.

:macro-def:`CGROUP_HARD_MEMORY_LIMIT_EXPR`
    See above.

:macro-def:`CGROUP_LOW_MEMORY_LIMIT`
    A classad expression, evaluated in the context of the slot and job ad.
    When it evaluated to a number, that number is written to the job's
    cgroup memory.low limit.  This is only implemented on Linux systems
    where HTCondor controls the jobs' cgroups.  When the job exceeds this 
    limit, the kernel will aggressively evict read-only pages (often disk cache)
    from the job's use.  For example, an admin could set this to 
    Memory * 0.5, in order to prevent the system from using otherwise available
    memory for caching on behalf of the job.

:macro-def:`CGROUP_IGNORE_CACHE_MEMORY[STARTER]`
    A boolean value which defaults to true.  When true, cached memory pages
    (like the disk cache) do not count to the job's reported memory usage.

:macro-def:`DISABLE_SWAP_FOR_JOB[STARTER]`
    A boolean that defaults to false.  When true, and cgroups are in effect, the
    *condor_starter* will set the memws to the same value as the hard memory limit.
    This will prevent the job from using any swap space.  If it needs more memory than
    the hard limit, it will be put on hold.  When false, the job is allowed to use any
    swap space configured by the operating system.

:macro-def:`STARTER_HIDE_GPU_DEVICES[STARTER]`
    A Linux-specific boolean that defaults to true.  When true, if started as root,
    HTCondor will use the "devices" cgroup to prevent the job from accessing
    any NVidia GPUs not assigned to it by HTCondor.  The device files will still exist
    in ``/dev``, but any attempt to access them will fail, regardless of their file
    permissions.  The ``nvidia-smi`` command will not report them as being available.
    Setting this macro to false returns to the previous functionality (of allowing jobs
    to access NVidia GPUs not assigned to them).
   
:macro-def:`USE_VISIBLE_DESKTOP[STARTER]`
    This boolean variable is only meaningful on Windows machines. If
    ``True``, HTCondor will allow the job to create windows on the
    desktop of the execute machine and interact with the job. This is
    particularly useful for debugging why an application will not run
    under HTCondor. If ``False``, HTCondor uses the default behavior of
    creating a new, non-visible desktop to run the job on. See
    the :doc:`/platform-specific/microsoft-windows` section for details
    on how HTCondor interacts with the desktop.

:macro-def:`STARTER_JOB_ENVIRONMENT[STARTER]`
    This macro sets the default environment inherited by jobs. The
    syntax is the same as the syntax for environment settings in the job
    submit file (see :doc:`/man-pages/condor_submit`). If the same
    environment variable is assigned by this macro and by the user in
    the submit file, the user's setting takes precedence.

:macro-def:`JOB_INHERITS_STARTER_ENVIRONMENT[STARTER]`
    A matchlist or boolean value that defaults to ``False``. When set to 
    a matchlist it causes jobs to inherit all environment variables from the
    *condor_starter* that are selected by the match list and not already defined
    in the job ClassAd or by the :macro:`STARTER_JOB_ENVIRONMENT` configuration variable.

    A matchlist is a comma, semicolon or space separated list of environment variable names
    and name patterns that match or reject names.
    Matchlist members are matched case-insensitively to each name
    in the environment and those that match are imported. Matchlist members can contain ``*`` as wildcard
    character which matches anything at that position.  Members can have two ``*`` characters if one of them
    is at the end. Members can be prefixed with ``!``
    to force a matching environment variable to not be imported.  The order of members in the Matchlist
    has no effect on the result.  For backward compatibility a single value of ``True`` behaves as if the value
    was set to ``*``.  Prior to HTCondor version 10.1.0 all values other than ``True`` are treated as ``False``.

:macro-def:`NAMED_CHROOT[STARTER]`
    A comma and/or space separated list of full paths to one or more
    directories, under which the *condor_starter* may run a chroot-ed
    job. This allows HTCondor to invoke chroot() before launching a job,
    if the job requests such by defining the job ClassAd attribute
    :ad-attr:`RequestedChroot` with a directory that matches one in this list.
    There is no default value for this variable.

:macro-def:`STARTER_UPLOAD_TIMEOUT[STARTER]`
    An integer value that specifies the network communication timeout to
    use when transferring files back to the access point. The default
    value is set by the *condor_shadow* daemon to 300. Increase this
    value if the disk on the access point cannot keep up with large
    bursts of activity, such as many jobs all completing at the same
    time.

:macro-def:`ASSIGN_CPU_AFFINITY[STARTER]`
    A boolean expression that defaults to ``False``. When it evaluates
    to ``True``, each job under this *condor_startd* is confined to
    using only as many cores as the configured number of slots. When
    using partitionable slots, each job will be bound to as many cores
    as requested by specifying **request_cpus**. When ``True``, this
    configuration variable overrides any specification of
    :macro:`ENFORCE_CPU_AFFINITY`. The expression is evaluated in the context
    of the Job ClassAd.

:macro-def:`ENFORCE_CPU_AFFINITY[STARTER]`
    This configuration variable is replaced by :macro:`ASSIGN_CPU_AFFINITY`.
    Do not enable this configuration variable unless using glidein or
    another unusual setup.

    A boolean value that defaults to ``False``. When ``False``, the CPU
    affinity of processes in a job is not enforced. When ``True``, the
    processes in an HTCondor job maintain their affinity to a CPU. This
    means that this job will only run on that particular CPU, even if
    other CPU cores are idle.

    If ``True`` and :macro:`SLOT<N>_CPU_AFFINITY` is not set, the CPU that
    the job is locked to is the same as ``SlotID - 1``. Note that slots
    are numbered beginning with the value 1, while CPU cores are
    numbered beginning with the value 0.

    When ``True``, more fine grained affinities may be specified with
    :macro:`SLOT<N>_CPU_AFFINITY`.

:macro-def:`SLOT<N>_CPU_AFFINITY[STARTER]`
    This configuration variable is replaced by :macro:`ASSIGN_CPU_AFFINITY`.
    Do not enable this configuration variable unless using glidein or
    another unusual setup.

    A comma separated list of cores to which an HTCondor job running on
    a specific slot given by the value of ``<N>`` show affinity. Note
    that slots are numbered beginning with the value 1, while CPU cores
    are numbered beginning with the value 0. This affinity list only
    takes effect when ``ENFORCE_CPU_AFFINITY = True``.

:macro-def:`ENABLE_URL_TRANSFERS[STARTER]`
    A boolean value that when ``True`` causes the *condor_starter* for
    a job to invoke all plug-ins defined by :macro:`FILETRANSFER_PLUGINS` to
    determine their capabilities for handling protocols to be used in
    file transfer specified with a URL. When ``False``, a URL transfer
    specified in a job's submit description file will cause an error
    issued by :tool:`condor_submit`. The default value is ``True``.

:macro-def:`FILETRANSFER_PLUGINS[STARTER]`
    A comma separated list of full and absolute path and executable
    names for plug-ins that will accomplish the task of doing file
    transfer when a job requests the transfer of an input file by
    specifying a URL. See
    :ref:`admin-manual/file-and-cred-transfer:Custom File Transfer Plugins`
    for a description of the functionality required of a plug-in.

:macro-def:`<PLUGIN>_TEST_URL[STARTER]`
    This configuration takes a URL to be tested against the specified
    ``<PLUGIN>``. If this test fails, then that plugin is removed from
    the *condor_starter* classad attribute :ad-attr:`HasFileTransferPluginMethods`.
    This attribute determines what plugin capabilities the *condor_starter*
    can utilize.

:macro-def:`RUN_FILETRANSFER_PLUGINS_WITH_ROOT[STARTER]`
    A boolean value that affects only Unix platforms and defaults to
    ``False``, causing file transfer plug-ins invoked for a job to run
    with both the real and the effective UID set to user that the job
    runs as. The user that the job runs as may be the job owner, nobody,
    or the slot user. The group is set to primary group of the user that
    the job runs as, and all supplemental groups are dropped. The
    default gives the behavior exhibited prior to the existence of this
    configuration variable. When set to ``True``, file transfer plug-ins
    are invoked with a real UID of 0 (root), provided the HTCondor
    daemons also run as root. The effective UID is set to the user that
    the job runs as.

    This configuration variable can permit plug-ins to do privileged
    operations, such as access a credential protected by file system
    permissions. The default value is recommended unless privileged
    operations are required.

:macro-def:`MAX_FILE_TRANSFER_PLUGIN_LIFETIME[STARTER]`:
    An integer number of seconds (defaulting to twenty hours) after which
    the starter will kill a file transfer plug-in for taking too long.
    Currently, this causes the job to go on hold with ``ETIME`` (62) as
    the hold reason subcode.

:macro-def:`ENABLE_CHIRP[STARTER]`
    A boolean value that defaults to ``True``. An administrator would
    set the value to ``False`` to disable Chirp remote file access from
    execute machines.

:macro-def:`ENABLE_CHIRP_UPDATES[STARTER]`
    A boolean value that defaults to ``True``. If :macro:`ENABLE_CHIRP`
    is ``True``, and :macro:`ENABLE_CHIRP_UPDATES` is ``False``, then the
    user job can only read job attributes from the submit side; it
    cannot change them or write to the job event log. If :macro:`ENABLE_CHIRP`
    is ``False``, the setting of this variable does not matter, as no
    Chirp updates are allowed in that case.

:macro-def:`ENABLE_CHIRP_IO[STARTER]`
    A boolean value that defaults to ``True``. If ``False``, the file
    I/O :tool:`condor_chirp` commands are prohibited.

:macro-def:`ENABLE_CHIRP_DELAYED[STARTER]`
    A boolean value that defaults to ``True``. If ``False``, the
    :tool:`condor_chirp` commands **get_job_attr_delayed** and
    **set_job_attr_delayed** are prohibited.

:macro-def:`CHIRP_DELAYED_UPDATE_PREFIX[STARTER]`
    This is a string-valued and case-insensitive parameter with the
    default value of ``"Chirp*"``. The string is a list separated by
    spaces and/or commas. Each attribute passed to the either of the
    :tool:`condor_chirp` commands **set_job_attr_delayed** or
    **get_job_attr_delayed** must match against at least one element
    in the list. An attribute which does not match any list element
    fails. A list element may contain a wildcard character
    (``"Chirp*"``), which marks where any number of characters matches.
    Thus, the default is to allow reads from and writes to only
    attributes which start with ``"Chirp"``.

    Because this parameter must be set to the same value on both the
    submit and execute nodes, it is advised that this parameter not be
    changed from its built-in default.

:macro-def:`CHIRP_DELAYED_UPDATE_MAX_ATTRS[STARTER]`
    This integer-valued parameter, which defaults to 100, represents the
    maximum number of pending delayed chirp updates buffered by the
    *condor_starter*. If the number of unique attributes updated by the
    :tool:`condor_chirp` command **set_job_attr_delayed** exceeds this
    parameter, it is possible for these updates to be ignored.

:macro-def:`CONDOR_SSH_TO_JOB_FAKE_PASSWD_ENTRY[STARTER]`
    A boolean valued parameter which defaults to true.  When true,
    it sets the environment variable LD_PRELOAD to point to the
    htcondor-provided libgetpwnam.so for the sshd run by 
    :tool:`condor_ssh_to_job`.  This results in the shell being
    set to /bin/sh and the home directory to the scratch directory
    for processes launched by :tool:`condor_ssh_to_job`.

:macro-def:`USE_PSS[STARTER]`
    A boolean value, that when ``True`` causes the *condor_starter* to
    measure the PSS (Proportional Set Size) of each HTCondor job. The
    default value is ``False``. When running many short lived jobs,
    performance problems in the :tool:`condor_procd` have been observed, and
    a setting of ``False`` may relieve these problems.

:macro-def:`MEMORY_USAGE_METRIC[STARTER]`
    A ClassAd expression that produces an initial value for the job
    ClassAd attribute :ad-attr:`MemoryUsage` in jobs that are not vm universe.

:macro-def:`MEMORY_USAGE_METRIC_VM[STARTER]`
    A ClassAd expression that produces an initial value for the job
    ClassAd attribute :ad-attr:`MemoryUsage` in vm universe jobs.

:macro-def:`STARTER_RLIMIT_AS[STARTER]`
    An integer ClassAd expression, expressed in MiB, evaluated by the
    *condor_starter* to set the ``RLIMIT_AS`` parameter of the
    setrlimit() system call. This limits the virtual memory size of each
    process in the user job. The expression is evaluated in the context
    of both the machine and job ClassAds, where the machine ClassAd is
    the ``MY.`` ClassAd, and the job ClassAd is the ``TARGET.`` ClassAd.
    There is no default value for this variable. Since values larger
    than 2047 have no real meaning on 32-bit platforms, values larger
    than 2047 result in no limit set on 32-bit platforms.

:macro-def:`USE_PID_NAMESPACES[STARTER]`
    A boolean value that, when ``True``, enables the use of per job PID
    namespaces for HTCondor jobs run on Linux kernels. Defaults to
    ``False``.

:macro-def:`PER_JOB_NAMESPACES[STARTER]`
    A boolean value that defaults to ``True``. Relevant only for Linux
    platforms using file system namespaces. A value of
    ``False`` ensures that there will be no private mount points,
    because auto mounts done by *autofs* would use the wrong name for
    private file system mounts. A ``True`` value is useful when private
    file system mounts are permitted and *autofs* (for NFS) is not used.

:macro-def:`DYNAMIC_RUN_ACCOUNT_LOCAL_GROUP[STARTER]`
    For Windows platforms, a value that sets the local group to a group
    other than the default ``Users`` for the ``condor-slot<X>`` run
    account. Do not place the local group name within quotation marks.

:macro-def:`JOB_EXECDIR_PERMISSIONS[STARTER]`
    Control the permissions on the job's scratch directory. Defaults to
    ``user`` which sets permissions to 0700. Possible values are
    ``user``, ``group``, and ``world``. If set to ``group``, then the
    directory is group-accessible, with permissions set to 0750. If set
    to ``world``, then the directory is created with permissions set to
    0755.

:macro-def:`CONDOR_SSHD[STARTER]`
    A string value defaulting to /usr/sbin/sshd which is used by
    the example parallel universe scripts to find a working sshd.

:macro-def:`CONDOR_SSH_KEYGEN[STARTER]`
    A string value defaulting to /usr/bin/ssh_keygen which is used by
    the example parallel universe scripts to find a working ssh_keygen
    program.

:macro-def:`STARTER_STATS_LOG[STARTER]`
    The full path and file name of a file that stores TCP statistics for
    starter file transfers. (Note that the starter logs TCP statistics
    to this file by default. Adding ``D_STATS`` to the :macro:`STARTER_DEBUG`
    value will cause TCP statistics to be logged to the normal starter
    log file (``$(STARTER_LOG)``).) If not defined,
    :macro:`STARTER_STATS_LOG` defaults to ``$(LOG)/XferStatsLog``. Setting
    :macro:`STARTER_STATS_LOG` to ``/dev/null`` disables logging of starter
    TCP file transfer statistics.

:macro-def:`MAX_STARTER_STATS_LOG[STARTER]`
    Controls the maximum size in bytes or amount of time that the
    starter TCP statistics log will be allowed to grow. If not defined,
    :macro:`MAX_STARTER_STATS_LOG` defaults to ``$(MAX_DEFAULT_LOG)``, which
    currently defaults to 10 MiB in size. Values are specified with the
    same syntax as :macro:`MAX_DEFAULT_LOG`.

:macro-def:`SINGULARITY[STARTER]`
    The path to the Singularity binary. The default value is
    ``/usr/bin/singularity``.

:macro-def:`SINGULARITY_JOB[STARTER]`
    A boolean value specifying whether this startd should run jobs under
    Singularity.  This can be an expression evaluted in the context of the slot
    ad and the job ad, where the slot ad is the "MY.", and the job ad is the
    "TARGET.". The default value is ``False``.

:macro-def:`SINGULARITY_IMAGE_EXPR[STARTER]`
    The path to the Singularity container image file.  This can be an
    expression evaluted in the context of the slot ad and the job ad, where the
    slot ad is the "MY.", and the job ad is the "TARGET.".  The default value
    is ``"SingularityImage"``.

:macro-def:`SINGULARITY_TARGET_DIR[STARTER]`
    A directory within the Singularity image to which
    ``$_CONDOR_SCRATCH_DIR`` on the host should be mapped. The default
    value is ``""``.

:macro-def:`SINGULARITY_BIND_EXPR[STARTER]`
    A string value containing a list of bind mount specifications to be passed
    to Singularity.  This can be an expression evaluted in the context of the
    slot ad and the job ad, where the slot ad is the "MY.", and the job ad is
    the "TARGET.". The default value is ``"SingularityBind"``.

:macro-def:`SINGULARITY_IGNORE_MISSING_BIND_TARGET[STARTER]`
    A boolean value defaulting to false.  If true, and the singularity
    image is a directory, and the target of a bind mount doesn't exist in
    the target, then skip this bind mount.

:macro-def:`SINGULARITY_USE_PID_NAMESPACES[STARTER]`
    Controls if jobs using Singularity should run in a private PID namespace, with a default value of ``Auto``.
    If set to ``Auto``, then PID namespaces will be used if it is possible to do so, else not used.
    If set to ``True``, then a PID namespaces must be used; if the installed Singularity cannot
    activate PID namespaces (perhaps due to insufficient permissions), then the slot
    attribute :ad-attr:`HasSingularity` will be set to False so that jobs needing Singularity will match.
    If set to ``False``, then PID namespaces must not be used.

:macro-def:`SINGULARITY_EXTRA_ARGUMENTS[STARTER]`
    A string value or classad expression containing a list of extra arguments to be appended
    to the Singularity command line. This can be an expression evaluted in the context of the
    slot ad and the job ad, where the slot ad is the "MY.", and the job ad is the "TARGET.".
:macro-def:`SINGULARITY_RUN_TEST_BEFORE_JOB[STARTER]`
    A boolean value which defaults to true.  When true, before running a singularity
    or apptainer contained job, the HTCondor starter will run apptainer test your_image.
    Only if that succeeds will HTCondor then run your job proper.

:macro-def:`SINGULARITY_VERBOSITY[STARTER]`
    A string value that defaults to -q.  This string is placed immediately after the
    singularity or apptainer command, intended to control debugging verbosity, but
    could be used for any global option for all singularity or apptainer commands.
    Debugging singularity or apptainer problems may be aided by setting this to -v
    or -d.

:macro-def:`USE_DEFAULT_CONTAINER[STARTER]`
    A boolean value or classad expression evaluating to boolean in the context of the Slot
    ad (the MY.) and the job ad (the TARGET.).  When true, a vanilla universe job that
    does not request a container will be put into the container image specified by
    the parameter :macro:`DEFAULT_CONTAINER_IMAGE`

:macro-def:`DEFAULT_CONTAINER_IMAGE[STARTER]`
    A string value that when :macro:`USE_DEFAULT_CONTAINER` is true, contains the container
    image to use, either starting with docker:, ending in .sif for a sif file, or otherwise
    an exploded directory for singularity/apptainer to run.

condor_submit Configuration File Entries
-----------------------------------------

:index:`condor_submit configuration variables<single: condor_submit configuration variables; configuration>`

:macro-def:`DEFAULT_UNIVERSE[SUBMIT]`
    The universe under which a job is executed may be specified in the
    submit description file. If it is not specified in the submit
    description file, then this variable specifies the universe (when
    defined). If the universe is not specified in the submit description
    file, and if this variable is not defined, then the default universe
    for a job will be the vanilla universe.

:macro-def:`JOB_DEFAULT_NOTIFICATION[SUBMIT]`
    The default that sets email notification for jobs. This variable
    defaults to ``NEVER``, such that HTCondor will not send email about
    events for jobs. Possible values are ``NEVER``, ``ERROR``,
    ``ALWAYS``, or ``COMPLETE``. If ``ALWAYS``, the owner will be
    notified whenever the
    job completes. If ``COMPLETE``, the owner will be notified when the
    job terminates. If ``ERROR``, the owner will only be notified if the
    job terminates abnormally, or if the job is placed on hold because
    of a failure, and not by user request. If ``NEVER``, the owner will
    not receive email.

:macro-def:`JOB_DEFAULT_LEASE_DURATION[SUBMIT]`
    The default value for the
    :subcom:`job_lease_duration[and JOB_DEFAULT_LEASE_DURATION]`
    submit command when the submit file does not specify a value. The
    default value is 2400, which is 40 minutes.

:macro-def:`JOB_DEFAULT_REQUESTMEMORY[SUBMIT]`
    The amount of memory in MiB to acquire for a job, if the job does
    not specify how much it needs using the
    :subcom:`request_memory[and JOB_DEFAULT_REQUESTMEMORY]`
    submit command. If this variable is not defined, then the default is
    defined by the expression

    .. code-block:: text

          ifThenElse(MemoryUsage =!= UNDEFINED,MemoryUsage,(ImageSize+1023)/1024)

:macro-def:`JOB_DEFAULT_REQUESTDISK[SUBMIT]`
    The amount of disk in KiB to acquire for a job, if the job does not
    specify how much it needs using the
    :subcom:`request_disk[and JOB_DEFAULT_REQUESTDISK]`
    submit command. If the job defines the value, then that value takes
    precedence. If not set, then then the default is defined as
    :ad-attr:`DiskUsage`.

:macro-def:`JOB_DEFAULT_REQUESTCPUS[SUBMIT]`
    The number of CPUs to acquire for a job, if the job does not specify
    how many it needs using the :subcom:`request_cpus[and JOB_DEFAULT_REQUESTCPUS]`
    submit command. If the job defines the value, then that value takes
    precedence. If not set, then then the default is 1.

:macro-def:`DEFAULT_JOB_MAX_RETRIES[SUBMIT]`
    The default value for the maximum number of job retries, if the
    :tool:`condor_submit` retry feature is used. (Note that this value is
    only relevant if either :subcom:`retry_until[and DEFAULT_JOB_MAX_RETRIES]`
    or :subcom:`success_exit_code[and DEFAULT_JOB_MAX_RETRIES]`
    is defined in the submit file, and :subcom:`max_retries[and DEFAULT_JOB_MAX_RETRIES]`
    is not.) (See the :doc:`/man-pages/condor_submit` man page.) The default value
    if not defined is 2.

If you want :tool:`condor_submit` to automatically append an expression to
the ``Requirements`` expression or ``Rank`` expression of jobs at your
site use the following macros:

:macro-def:`APPEND_REQ_VANILLA[SUBMIT]`
    Expression to be appended to vanilla job requirements.

:macro-def:`APPEND_REQUIREMENTS[SUBMIT]`
    Expression to be appended to any type of universe jobs. However, if
    :macro:`APPEND_REQ_VANILLA` is defined, then
    ignore the :macro:`APPEND_REQUIREMENTS` for that universe.

:macro-def:`APPEND_RANK[SUBMIT]`
    Expression to be appended to job rank.
    :macro:`APPEND_RANK_VANILLA` will override this setting if defined.

:macro-def:`APPEND_RANK_VANILLA[SUBMIT]`
    Expression to append to vanilla job rank.

In addition, you may provide default ``Rank`` expressions if your users
do not specify their own with:

:macro-def:`DEFAULT_RANK[SUBMIT]`
    Default rank expression for any job that does not specify its own
    rank expression in the submit description file. There is no default
    value, such that when undefined, the value used will be 0.0.

:macro-def:`DEFAULT_RANK_VANILLA[SUBMIT]`
    Default rank for vanilla universe jobs. There is no default value,
    such that when undefined, the value used will be 0.0. When both
    :macro:`DEFAULT_RANK` and :macro:`DEFAULT_RANK_VANILLA` are defined, the value
    for :macro:`DEFAULT_RANK_VANILLA` is used for vanilla universe jobs.

:macro-def:`SUBMIT_GENERATE_CUSTOM_RESOURCE_REQUIREMENTS[SUBMIT]`
    If ``True``, :tool:`condor_submit` will treat any attribute in the job
    ClassAd that begins with ``Request`` as a request for a custom resource
    and will ad a clause to the Requirements expression ensuring that
    on slots that have that resource will match the job.
    The default value is ``True``.

:macro-def:`SUBMIT_GENERATE_CONDOR_C_REQUIREMENTS[SUBMIT]`
    If ``True``, :tool:`condor_submit` will add clauses to the job's
    Requirements expression for **condor** grid universe jobs like it
    does for vanilla universe jobs.
    The default value is ``True``.

:macro-def:`SUBMIT_SKIP_FILECHECKS[SUBMIT]`
    If ``True``, :tool:`condor_submit` behaves as if the **-disable**
    command-line option is used. This tells :tool:`condor_submit` to disable
    file permission checks when submitting a job for read permissions on
    all input files, such as those defined by commands
    :subcom:`input[and SUBMIT_SKIP_FILECHECKS]`
    :subcom:`transfer_input_files[and SUBMIT_SKIP_FILECHECKS]`
    as well as write permission to output files, such as a log file
    defined by :subcom:`log[and SUBMIT_SKIP_FILECHECKS]`
    files defined with :subcom:`output[and SUBMIT_SKIP_FILECHECKS]`
    :subcom:`transfer_output_files[and SUBMIT_SKIP_FILECHECKS]`
    This can significantly decrease the amount of time required to
    submit a large group of jobs.  The default value is ``True``.

:macro-def:`WARN_ON_UNUSED_SUBMIT_FILE_MACROS[SUBMIT]`
    A boolean variable that defaults to ``True``. When ``True``,
    :tool:`condor_submit` performs checks on the job's submit description
    file contents for commands that define a macro, but do not use the
    macro within the file. A warning is issued, but job submission
    continues. A definition of a new macro occurs when the lhs of a
    command is not a known submit command. This check may help spot
    spelling errors of known submit commands.

:macro-def:`SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES[SUBMIT]`
    Provides a default value for the submit command
    :subcom:`should_transfer_files[and SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES]`
    if the submit file does not supply a value and when the value is not
    forced by some other command in the submit file, such as the
    universe. Valid values are YES, TRUE, ALWAYS, NO, FALSE, NEVER and
    IF_NEEDED. If the value is not one of these, then IF_NEEDED will
    be used.

:macro-def:`SUBMIT_REQUEST_MISSING_UNITS`
    If set to the string ``error``, it is an error to submit a job with a 
    :subcom:`request_memory` or :subcom:`request_disk` with a unitless
    value.  If set to ``warn``, a warning is printed to the screen, but
    submit continues. Default value is unset (neither warn or error).
    :jira:`1837`
    
:macro-def:`SUBMIT_SEND_RESCHEDULE[SUBMIT]`
    A boolean expression that when False, prevents :tool:`condor_submit` from
    automatically sending a :tool:`condor_reschedule` command as it
    completes. The :tool:`condor_reschedule` command causes the
    *condor_schedd* daemon to start searching for machines with which
    to match the submitted jobs. When True, this step always occurs. In
    the case that the machine where the job(s) are submitted is managing
    a huge number of jobs (thousands or tens of thousands), this step
    would hurt performance in such a way that it became an obstacle to
    scalability. The default value is True.

:macro-def:`SUBMIT_ATTRS[SUBMIT]`
    A comma-separated and/or space-separated list of ClassAd attribute
    names for which the attribute and value will be inserted into all
    the job ClassAds that :tool:`condor_submit` creates. In this way, it is
    like the "+" syntax in a submit description file. Attributes defined
    in the submit description file with "+" will override attributes
    defined in the configuration file with :macro:`SUBMIT_ATTRS`. Note that
    adding an attribute to a job's ClassAd will not function as a method
    for specifying default values of submit description file commands
    forgotten in a job's submit description file. The command in the
    submit description file results in actions by :tool:`condor_submit`,
    while the use of :macro:`SUBMIT_ATTRS` adds a job ClassAd attribute at a
    later point in time.

:macro-def:`SUBMIT_ALLOW_GETENV[SUBMIT]`
    A boolean attribute which defaults to true. If set to false, the
    submit command "getenv = true" is an error.  Any restricted
    form of "getenv = some_env_var_name" is still allowed. 

:macro-def:`LOG_ON_NFS_IS_ERROR[SUBMIT]`
    A boolean value that controls whether :tool:`condor_submit` prohibits job
    submit description files with job event log files on NFS. If
    :macro:`LOG_ON_NFS_IS_ERROR` is set to ``True``, such submit files will
    be rejected. If :macro:`LOG_ON_NFS_IS_ERROR` is set to ``False``, the job
    will be submitted. If not defined, :macro:`LOG_ON_NFS_IS_ERROR` defaults
    to ``False``.

:macro-def:`SUBMIT_MAX_PROCS_IN_CLUSTER[SUBMIT]`
    An integer value that limits the maximum number of jobs that would
    be assigned within a single cluster. Job submissions that would
    exceed the defined value fail, issuing an error message, and with no
    jobs submitted. The default value is 0, which does not limit the
    number of jobs assigned a single cluster number.

:macro-def:`ENABLE_DEPRECATION_WARNINGS[SUBMIT]`
    A boolean value that defaults to ``False``. When ``True``,
    :tool:`condor_submit` issues warnings when a job requests features that
    are no longer supported.

:macro-def:`INTERACTIVE_SUBMIT_FILE[SUBMIT]`
    The path and file name of a submit description file that
    :tool:`condor_submit` will use in the specification of an interactive
    job. The default is ``$(RELEASE_DIR)``/libexec/interactive.sub when
    not defined.

:macro-def:`CRED_MIN_TIME_LEFT[SUBMIT]`
    When a job uses an X509 user proxy, condor_submit will refuse to
    submit a job whose x509 expiration time is less than this many
    seconds in the future. The default is to only refuse jobs whose
    expiration time has already passed.

:macro-def:`CONTAINER_SHARED_FS[SUBMIT]`
    This is a list of strings that name directories which are shared
    on the execute machines and may contain container images under them.
    The default value is /cvmfs.  When a container universe job lists
    a *condor_image* that is under one of these directories, HTCondor
    knows not to try to transfer the file to the worker node.

condor_preen Configuration File Entries
----------------------------------------

:index:`condor_preen configuration variables<single: condor_preen configuration variables; configuration>`

These macros affect :tool:`condor_preen`.

:macro-def:`PREEN_ADMIN[PREEN]`
    This macro sets the e-mail address where :tool:`condor_preen` will send
    e-mail (if it is configured to send email at all; see the entry for
    :macro:`PREEN`). Defaults to ``$(CONDOR_ADMIN)``.

:macro-def:`VALID_SPOOL_FILES[PREEN]`
    A comma or space separated list of files that :tool:`condor_preen`
    considers valid files to find in the ``$(SPOOL)`` directory, such
    that :tool:`condor_preen` will not remove these files. There is no
    default value. :tool:`condor_preen` will add to the list files and
    directories that are normally present in the ``$(SPOOL)`` directory.
    A single asterisk (\*) wild card character is permitted in each file
    item within the list.

:macro-def:`SYSTEM_VALID_SPOOL_FILES[PREEN]`
    A comma or space separated list of files that :tool:`condor_preen`
    considers valid files to find in the ``$(SPOOL)`` directory. The
    default value is all files known by HTCondor to be valid. This
    variable exists such that it can be queried; it should not be
    changed. :tool:`condor_preen` use it to initialize the list files and
    directories that are normally present in the ``$(SPOOL)`` directory.
    A single asterisk (\*) wild card character is permitted in each file
    item within the list.

:macro-def:`INVALID_LOG_FILES[PREEN]`
    This macro contains a (comma or space separated) list of files that
    :tool:`condor_preen` considers invalid files to find in the ``$(LOG)``
    directory. There is no default value.

:macro-def:`MAX_CHECKPOINT_CLEANUP_PROCS[PREEN]`
    If a checkpoint clean-up plug-in fails when the *condor_schedd*
    (indirectly) invokes it after a job exits the queue, the next run of
    :tool:`condor_preen` will retry it.  :tool:`condor_preen` assumes that the clean-up
    process is relatively light-weight and starts more than one if more than
    one job failed to clean up.  This macro limits the number of simultaneous
    clean-up processes.

:macro-def:`CHECKPOINT_CLEANUP_TIMEOUT[PREEN]`
    A checkpoint clean-up plug-in is invoked once per file in the checkpoint,
    and must therefore do its job relatively quickly.  This macro defines
    (as an integer number of seconds)
    how long HTCondor will wait for a checkpoint clean-up plug-in to exit
    before it declares that it's stuck and kills it.

:macro-def:`PREEN_CHECKPOINT_CLEANUP_TIMEOUT[PREEN]`
    In addition to the per-file time-out :macro:`CHECKPOINT_CLEANUP_TIMEOUT`,
    there's only so long that :tool:`condor_preen` is willing to let clean-up for
    a single job (including all of its checkpoints) take.  This macro
    defines that duration (as an integer number of seconds).

condor_collector Configuration File Entries
--------------------------------------------

:index:`condor_collector configuration variables<single: condor_collector configuration variables; configuration>`

These macros affect the *condor_collector*.

:macro-def:`CLASSAD_LIFETIME[COLLECTOR]`
    The default maximum age in seconds for ClassAds collected by the
    *condor_collector*. ClassAds older than the maximum age are
    discarded by the *condor_collector* as stale.

    If present, the ClassAd attribute ``ClassAdLifetime`` specifies the
    ClassAd's lifetime in seconds. If ``ClassAdLifetime`` is not present
    in the ClassAd, the *condor_collector* will use the value of
    ``$(CLASSAD_LIFETIME)``. This variable is defined in terms of
    seconds, and it defaults to 900 seconds (15 minutes).

    To ensure that the *condor_collector* does not miss any ClassAds,
    the frequency at which all other subsystems that report using an
    update interval must be tuned. The configuration variables that set
    these subsystems are

    -  :macro:`UPDATE_INTERVAL` (for the *condor_startd* daemon)
    -  :macro:`NEGOTIATOR_UPDATE_INTERVAL`
    -  :macro:`SCHEDD_INTERVAL`
    -  :macro:`MASTER_UPDATE_INTERVAL`
    -  :macro:`DEFRAG_UPDATE_INTERVAL`
    -  :macro:`HAD_UPDATE_INTERVAL`

:macro-def:`COLLECTOR_REQUIREMENTS[COLLECTOR]`
    A boolean expression that filters out unwanted ClassAd updates. The
    expression is evaluated for ClassAd updates that have passed through
    enabled security authorization checks. The default behavior when
    this expression is not defined is to allow all ClassAd updates to
    take place. If ``False``, a ClassAd update will be rejected.

    Stronger security mechanisms are the better way to authorize or deny
    updates to the *condor_collector*. This configuration variable
    exists to help those that use host-based security, and do not trust
    all processes that run on the hosts in the pool. This configuration
    variable may be used to throw out ClassAds that should not be
    allowed. For example, for *condor_startd* daemons that run on a
    fixed port, configure this expression to ensure that only machine
    ClassAds advertising the expected fixed port are accepted. As a
    convenience, before evaluating the expression, some basic sanity
    checks are performed on the ClassAd to ensure that all of the
    ClassAd attributes used by HTCondor to contain IP:port information
    are consistent. To validate this information, the attribute to check
    is ``TARGET.MyAddress``.

    Please note that _all_ ClassAd updates are filtered.  Unless your
    requirements are the same for all daemons, including the collector
    itself, you'll want to use the :ad-attr:`MyType` attribute to limit your
    filter(s).

:macro-def:`CLIENT_TIMEOUT[COLLECTOR]`
    Network timeout that the *condor_collector* uses when talking to
    any daemons or tools that are sending it a ClassAd update. It is
    defined in seconds and defaults to 30.

:macro-def:`QUERY_TIMEOUT[COLLECTOR]`
    Network timeout when talking to anyone doing a query. It is defined
    in seconds and defaults to 60.

:macro-def:`COLLECTOR_NAME[COLLECTOR]`
    This macro is used to specify a short description of your pool. It
    should be about 20 characters long. For example, the name of the
    UW-Madison Computer Science HTCondor Pool is ``"UW-Madison CS"``.
    While this macro might seem similar to :macro:`MASTER_NAME` or
    :macro:`SCHEDD_NAME`, it is unrelated. Those settings are used to
    uniquely identify (and locate) a specific set of HTCondor daemons,
    if there are more than one running on the same machine. The
    :macro:`COLLECTOR_NAME` setting is just used as a human-readable string
    to describe the pool.

:macro-def:`COLLECTOR_UPDATE_INTERVAL[COLLECTOR]`
    This variable is defined in seconds and defaults to 900 (every 15
    minutes). It controls the frequency of the periodic updates sent to
    a central *condor_collector*.

:macro-def:`COLLECTOR_SOCKET_BUFSIZE[COLLECTOR]`
    This specifies the buffer size, in bytes, reserved for
    *condor_collector* network UDP sockets. The default is 10240000, or
    a ten megabyte buffer. This is a healthy size, even for a large
    pool. The larger this value, the less likely the *condor_collector*
    will have stale information about the pool due to dropping update
    packets. If your pool is small or your central manager has very
    little RAM, considering setting this parameter to a lower value
    (perhaps 256000 or 128000).

    .. note::

        For some Linux distributions, it may be necessary to raise the
        OS's system-wide limit for network buffer sizes. The parameter that
        controls this limit is /proc/sys/net/core/rmem_max. You can see the
        values that the *condor_collector* actually uses by enabling
        D_FULLDEBUG for the collector and looking at the log line that
        looks like this:

    Reset OS socket buffer size to 2048k (UDP), 255k (TCP).

    For changes to this parameter to take effect, *condor_collector*
    must be restarted.

:macro-def:`COLLECTOR_TCP_SOCKET_BUFSIZE[COLLECTOR]`
    This specifies the TCP buffer size, in bytes, reserved for
    *condor_collector* network sockets. The default is 131072, or a 128
    kilobyte buffer. This is a healthy size, even for a large pool. The
    larger this value, the less likely the *condor_collector* will have
    stale information about the pool due to dropping update packets. If
    your pool is small or your central manager has very little RAM,
    considering setting this parameter to a lower value (perhaps 65536
    or 32768).

    .. note::

        See the note for :macro:`COLLECTOR_SOCKET_BUFSIZE`.

:macro-def:`KEEP_POOL_HISTORY[COLLECTOR]`
    This boolean macro is used to decide if the collector will write out
    statistical information about the pool to history files. The default
    is ``False``. The location, size, and frequency of history logging
    is controlled by the other macros.

:macro-def:`POOL_HISTORY_DIR[COLLECTOR]`
    This macro sets the name of the directory where the history files
    reside (if history logging is enabled). The default is the :macro:`SPOOL`
    directory.

:macro-def:`POOL_HISTORY_MAX_STORAGE[COLLECTOR]`
    This macro sets the maximum combined size of the history files. When
    the size of the history files is close to this limit, the oldest
    information will be discarded. Thus, the larger this parameter's
    value is, the larger the time range for which history will be
    available. The default value is 10000000 (10 MB).

:macro-def:`POOL_HISTORY_SAMPLING_INTERVAL[COLLECTOR]`
    This macro sets the interval, in seconds, between samples for
    history logging purposes. When a sample is taken, the collector goes
    through the information it holds, and summarizes it. The information
    is written to the history file once for each 4 samples. The default
    (and recommended) value is 60 seconds. Setting this macro's value
    too low will increase the load on the collector, while setting it to
    high will produce less precise statistical information.

:macro-def:`FLOCK_FROM[COLLECTOR]`
    The macros contains a comma separate list of schedd names that
    should be allowed to flock to this central manager.  Defaults
    to an empty list.

:macro-def:`COLLECTOR_DAEMON_STATS[COLLECTOR]`
    A boolean value that controls whether or not the *condor_collector*
    daemon keeps update statistics on incoming updates. The default
    value is ``True``. If enabled, the *condor_collector* will insert
    several attributes into the ClassAds that it stores and sends.
    ClassAds without the ``UpdateSequenceNumber`` and
    ``DaemonStartTime`` attributes will not be counted, and will not
    have attributes inserted (all modern HTCondor daemons which publish
    ClassAds publish these attributes).
    :index:`UpdatesTotal<single: UpdatesTotal; ClassAd attribute added by the condor_collector>`
    :index:`UpdatesSequenced<single: UpdatesSequenced; ClassAd attribute added by the condor_collector>`
    :index:`UpdatesLost<single: UpdatesLost; ClassAd attribute added by the condor_collector>`

    The attributes inserted are ``UpdatesTotal``, :ad-attr:`UpdatesSequenced`,
    and ``UpdatesLost``. ``UpdatesTotal`` is the total number of updates
    (of this ClassAd type) the *condor_collector* has received from
    this host. :ad-attr:`UpdatesSequenced` is the number of updates that the
    *condor_collector* could have as lost. In particular, for the first
    update from a daemon, it is impossible to tell if any previous ones
    have been lost or not. ``UpdatesLost`` is the number of updates that
    the *condor_collector* has detected as being lost. See
    :doc:`/classad-attributes/classad-attributes-added-by-collector` for
    more information on the added attributes.

:macro-def:`COLLECTOR_STATS_SWEEP[COLLECTOR]`
    This value specifies the number of seconds between sweeps of the
    *condor_collector* 's per-daemon update statistics. Records for
    daemons which have not reported in this amount of time are purged in
    order to save memory. The default is two days. It is unlikely that
    you would ever need to adjust this.
    :index:`UpdatesHistory<single: UpdatesHistory; ClassAd attribute added by the condor_collector>`

:macro-def:`COLLECTOR_DAEMON_HISTORY_SIZE[COLLECTOR]`
    This variable controls the size of the published update history that
    the *condor_collector* inserts into the ClassAds it stores and
    sends. The default value is 128, which means that history is stored
    and published for the latest 128 updates. This variable's value is
    ignored, if :macro:`COLLECTOR_DAEMON_STATS` is not enabled.

    If the value is a non-zero one, the *condor_collector* will insert
    attribute :ad-attr:`UpdatesHistory` into the ClassAd (similar to
    ``UpdatesTotal``). AttrUpdatesHistory is a hexadecimal string which
    represents a bitmap of the last :macro:`COLLECTOR_DAEMON_HISTORY_SIZE`
    updates. The most significant bit (MSB) of the bitmap represents
    the most recent update, and the least significant bit (LSB) represents
    the least recent. A value of zero means that the update was not lost,
    and a value of 1 indicates that the update was detected as lost.

    For example, if the last update was not lost, the previous was lost,
    and the previous two not, the bitmap would be 0100, and the matching
    hex digit would be ``"4"``. Note that the MSB can never be marked as
    lost because its loss can only be detected by a non-lost update (a
    gap is found in the sequence numbers). Thus,
    ``UpdatesHistory = "0x40"`` would be the history for the last 8
    updates. If the next updates are all successful, the values
    published, after each update, would be: 0x20, 0x10, 0x08, 0x04,
    0x02, 0x01, 0x00.

    See
    :ref:`classad-attributes/classad-attributes-added-by-collector:classad attributes added by the *condor_collector*`
    for more information on the added attribute.

:macro-def:`COLLECTOR_CLASS_HISTORY_SIZE[COLLECTOR]`
    This variable controls the size of the published update history that
    the *condor_collector* inserts into the *condor_collector*
    ClassAds it produces. The default value is zero.

    If this variable has a non-zero value, the *condor_collector* will
    insert ``UpdatesClassHistory`` into the *condor_collector* ClassAd
    (similar to :ad-attr:`UpdatesHistory`). These are added per class of
    ClassAd, however. The classes refer to the type of ClassAds.
    Additionally, there is a Total class created, which represents the
    history of all ClassAds that this *condor_collector* receives.

    Note that the *condor_collector* always publishes Lost, Total and
    Sequenced counts for all ClassAd classes. This is similar to the
    statistics gathered if :macro:`COLLECTOR_DAEMON_STATS` is enabled.

:macro-def:`COLLECTOR_QUERY_WORKERS[COLLECTOR]`
    This macro sets the maximum number of child worker processes that
    the *condor_collector* can have, and defaults to a value of 4 on
    Linux and MacOS platforms. When receiving a large query request, the
    *condor_collector* may fork() a new process to handle the query,
    freeing the main process to handle other requests. Each forked child
    process will consume memory, potentially up to 50% or more of the
    memory consumed by the parent collector process. To limit the amount
    of memory consumed on the central manager to handle incoming
    queries, the default value for this macro is 4. When the number of
    outstanding worker processes reaches the maximum specified by this
    macro, any additional incoming query requests will be queued and
    serviced after an existing child worker completes. Note that on
    Windows platforms, this macro has a value of zero and cannot be
    changed.

:macro-def:`COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO[COLLECTOR]`
    This macro defines the number of :macro:`COLLECTOR_QUERY_WORKERS`
    slots will be held in reserve to only service high priority query
    requests. Currently, high priority queries are defined as those
    coming from the *condor_negotiator* during the course of matchmaking,
    or via a "condor_sos condor_status" command. The idea here is the critical
    operation of matchmaking machines to jobs will take precedence over
    user condor_status invocations. Defaults to a value of 1. The
    maximum allowable value for this macro is equal to
    :macro:`COLLECTOR_QUERY_WORKERS` minus 1.

:macro-def:`COLLECTOR_QUERY_WORKERS_PENDING[COLLECTOR]`
    This macro sets the maximum of collector pending query requests that
    can be queued waiting for child workers to exit. Queries that would
    exceed this maximum are immediately aborted. When a forked child
    worker exits, a pending query will be pulled from the queue for
    service. Note the collector will confirm that the client has not
    closed the TCP socket (because it was tired of waiting) before going
    through all the work of actually forking a child and starting to
    service the query. Defaults to a value of 50.

:macro-def:`COLLECTOR_QUERY_MAX_WORKTIME[COLLECTOR]`
    This macro defines the maximum amount of time in seconds that a
    query has to complete before it is aborted. Queries that wait in the
    pending queue longer than this period of time will be aborted before
    forking. Queries that have already forked will also abort after the
    worktime has expired - this protects against clients on a very slow
    network connection. If set to 0, then there is no timeout. The
    default is 0.

:macro-def:`HANDLE_QUERY_IN_PROC_POLICY[COLLECTOR]`
    This variable sets the policy for which queries the
    *condor_collector* should handle in process rather than by forking
    a worker. It should be set to one of the following values

    -  ``always`` Handle all queries in process
    -  ``never`` Handle all queries using fork workers
    -  ``small_table`` Handle only queries of small tables in process
    -  ``small_query`` Handle only small queries in process
    -  ``small_table_and_query`` Handle only small queries on small
       tables in process
    -  ``small_table_or_query`` Handle small queries or small tables in
       process

    A small table is any table of ClassAds in the collector other than
    Master,Startd,Generic and Any ads. A small query is a locate query,
    or any query with both a projection and a result limit that is
    smaller than 10. The default value is ``small_table_or_query``.

:macro-def:`COLLECTOR_DEBUG[COLLECTOR]`
    This macro (and other macros related to debug logging in the
    *condor_collector* is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`CONDOR_VIEW_CLASSAD_TYPES[COLLECTOR]`
    Provides the ClassAd types that will be forwarded to the
    :macro:`CONDOR_VIEW_HOST`. The ClassAd types can be found with
    :tool:`condor_status` **-any**. The default forwarding behavior of the
    *condor_collector* is equivalent to

    .. code-block:: condor-config

          CONDOR_VIEW_CLASSAD_TYPES=Machine,Submitter

    There is no default value for this variable.

:macro-def:`COLLECTOR_FORWARD_FILTERING[COLLECTOR]`
    When this boolean variable is set to ``True``, Machine and Submitter
    ad updates are not forwarded to the :macro:`CONDOR_VIEW_HOST` if certain
    attributes are unchanged from the previous update of the ad. The
    default is ``False``, meaning all updates are forwarded.

:macro-def:`COLLECTOR_FORWARD_WATCH_LIST[COLLECTOR]`
    When :macro:`COLLECTOR_FORWARD_FILTERING` is set to ``True``, this
    variable provides the list of attributes that controls whether a
    Machine or Submitter ad update is forwarded to the
    :macro:`CONDOR_VIEW_HOST`. If all attributes in this list are unchanged
    from the previous update, then the new update is not forwarded. The
    default value is ``State,Cpus,Memory,IdleJobs``.

:macro-def:`COLLECTOR_FORWARD_INTERVAL[COLLECTOR]`
    When :macro:`COLLECTOR_FORWARD_FILTERING` is set to ``True``, this
    variable limits how long forwarding of updates for a given ad can be
    filtered before an update must be forwarded. The default is one
    third of :macro:`CLASSAD_LIFETIME`.

:macro-def:`COLLECTOR_FORWARD_CLAIMED_PRIVATE_ADS[COLLECTOR]`
    When this boolean variable is set to ``False``, the *condor_collector*
    will not forward the private portion of Machine ads to the
    :macro:`CONDOR_VIEW_HOST` if the ad's :ad-attr:`State` is ``Claimed``.
    The default value is ``$(NEGOTIATOR_CONSIDER_PREEMPTION)``.

:macro-def:`COLLECTOR_FORWARD_PROJECTION[COLLECTOR]`
    An expression that evaluates to a string in the context of an update. The string is treated as a list
    of attributes to forward.  If the string has no attributes, it is ignored. The intended use is to
    restrict the list of attributes forwarded for claimed Machine ads.
    When ``$(NEGOTIATOR_CONSIDER_PREEMPTION)`` is false, the negotiator needs only a few attributes from
    Machine ads that are in the ``Claimed`` state. A Suggested use might be

    .. code-block:: condor-config

          if ! $(NEGOTIATOR_CONSIDER_PREEMPTION)
             COLLECTOR_FORWARD_PROJECTION = IfThenElse(State is "Claimed", "$(FORWARD_CLAIMED_ATTRS)", "")
             # forward only the few attributes needed by the Negotiator and a few more needed by condor_status
             FORWARD_CLAIMED_ATTRS = Name MyType MyAddress StartdIpAddr Machine Requirements \
                State Activity AccountingGroup Owner RemoteUser SlotWeight ConcurrencyLimits \
                Arch OpSys Memory Cpus CondorLoadAvg EnteredCurrentActivity
          endif

    There is no default value for this variable.


The following macros control where, when, and for how long HTCondor
persistently stores absent ClassAds. See
section :ref:`admin-manual/cm-configuration:absent classads` for more details.

:macro-def:`ABSENT_REQUIREMENTS[COLLECTOR]`
    A boolean expression evaluated by the *condor_collector* when a
    machine ClassAd would otherwise expire. If ``True``, the ClassAd
    instead becomes absent. If not defined, the implementation will
    behave as if ``False``, and no absent ClassAds will be stored.

:macro-def:`ABSENT_EXPIRE_ADS_AFTER[COLLECTOR]`
    The integer number of seconds after which the *condor_collector*
    forgets about an absent ClassAd. If 0, the ClassAds persist forever.
    Defaults to 30 days.

:macro-def:`COLLECTOR_PERSISTENT_AD_LOG[COLLECTOR]`
    The full path and file name of a file that stores machine ClassAds
    for every hibernating or absent machine. This forms a persistent
    storage of these ClassAds, in case the *condor_collector* daemon
    crashes.

    To avoid :tool:`condor_preen` removing this log, place it in a directory
    other than the directory defined by ``$(SPOOL)``. Alternatively, if
    this log file is to go in the directory defined by ``$(SPOOL)``, add
    the file to the list given by :macro:`VALID_SPOOL_FILES`.

:macro-def:`EXPIRE_INVALIDATED_ADS[COLLECTOR]`
    A boolean value that defaults to ``False``. When ``True``, causes
    all invalidated ClassAds to be treated as if they expired. This
    permits invalidated ClassAds to be marked absent, as defined in
    :ref:`admin-manual/cm-configuration:absent classads`.

condor_negotiator Configuration File Entries
---------------------------------------------

:index:`condor_negotiator configuration variables<single: condor_negotiator
configuration variables; configuration>`

These macros affect the *condor_negotiator*.

:macro-def:`NEGOTIATOR[NEGOTIATOR]`
    The full path to the *condor_negotiator* binary.

:macro-def:`NEGOTIATOR_NAME[NEGOTIATOR]`
    Used to give an alternative value to the ``Name`` attribute in the
    *condor_negotiator* 's ClassAd and the ``NegotiatorName``
    attribute of its accounting ClassAds. This configuration macro is
    useful in the situation where there are two *condor_negotiator*
    daemons running on one machine, and both report to the same
    *condor_collector*. Different names will distinguish the two
    daemons.

    See the description of :macro:`MASTER_NAME`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`NEGOTIATOR_INTERVAL[NEGOTIATOR]`
    Sets the maximum time the *condor_negotiator* will wait before
    starting a new negotiation cycle, counting from the start of the
    previous cycle.
    It is defined in seconds and defaults to 60 (1 minute).

:macro-def:`NEGOTIATOR_MIN_INTERVAL[NEGOTIATOR]`
    Sets the minimum time the *condor_negotiator* will wait before
    starting a new negotiation cycle, counting from the start of the
    previous cycle.
    It is defined in seconds and defaults to 5.

:macro-def:`NEGOTIATOR_UPDATE_INTERVAL[NEGOTIATOR]`
    This macro determines how often the *condor_negotiator* daemon
    sends a ClassAd update to the *condor_collector*. It is defined in
    seconds and defaults to 300 (every 5 minutes).

:macro-def:`NEGOTIATOR_CYCLE_DELAY[NEGOTIATOR]`
    An integer value that represents the minimum number of seconds that
    must pass before a new negotiation cycle may start. The default
    value is 20. :macro:`NEGOTIATOR_CYCLE_DELAY` is intended only for use by
    HTCondor experts.

:macro-def:`NEGOTIATOR_TIMEOUT[NEGOTIATOR]`
    Sets the timeout that the negotiator uses on its network connections
    to the *condor_schedd* and *condor_startd* s. It is defined in
    seconds and defaults to 30.

:macro-def:`NEGOTIATION_CYCLE_STATS_LENGTH[NEGOTIATOR]`
    Specifies how many recent negotiation cycles should be included in
    the history that is published in the *condor_negotiator* 's ad.
    The default is 3 and the maximum allowed value is 100. Setting this
    value to 0 disables publication of negotiation cycle statistics. The
    statistics about recent cycles are stored in several attributes per
    cycle. Each of these attribute names will have a number appended to
    it to indicate how long ago the cycle happened, for example:
    ``LastNegotiationCycleDuration0``,
    ``LastNegotiationCycleDuration1``,
    ``LastNegotiationCycleDuration2``, .... The attribute numbered 0
    applies to the most recent negotiation cycle. The attribute numbered
    1 applies to the next most recent negotiation cycle, and so on. See
    :doc:`/classad-attributes/negotiator-classad-attributes` for a list of
    attributes that are published.

:macro-def:`NEGOTIATOR_NUM_THREADS[NEGOTIATOR]`
    An integer that specifies the number of threads the negotiator should
    use when trying to match a job to slots.  The default is 1.  For
    sites with large number of slots, where the negotiator is running
    on a large machine, setting this to a larger value may result in
    faster negotiation times.  Setting this to more than the number
    of cores will result in slow downs.  An administrator setting this
    should also consider what other processes on the machine may need
    cores, such as the collector, and all of its forked children,
    the condor_master, and any helper programs or scripts running there.

:macro-def:`PRIORITY_HALFLIFE[NEGOTIATOR]`
    This macro defines the half-life of the user priorities. See
    :ref:`users-manual/job-scheduling:user priority` on
    User Priorities for details. It is defined in seconds and defaults
    to 86400 (1 day).

:macro-def:`DEFAULT_PRIO_FACTOR[NEGOTIATOR]`
    Sets the priority factor for local users as they first submit jobs,
    as described in :doc:`/admin-manual/cm-configuration`.
    Defaults to 1000.

:macro-def:`NICE_USER_PRIO_FACTOR[NEGOTIATOR]`
    Sets the priority factor for nice users, as described in
    :doc:`/admin-manual/cm-configuration`.
    Defaults to 10000000000.

:macro-def:`NICE_USER_ACCOUNTING_GROUP_NAME[NEGOTIATOR]`
    Sets the name used for the nice-user accounting group by :tool:`condor_submit`.
    Defaults to nice-user.

:macro-def:`REMOTE_PRIO_FACTOR[NEGOTIATOR]`
    Defines the priority factor for remote users, which are those users
    who who do not belong to the local domain. See
    :doc:`/admin-manual/cm-configuration` for details.
    Defaults to 10000000.

:macro-def:`ACCOUNTANT_DATABASE_FILE[NEGOTIATOR]`
    Defines the full path of the accountant database log file.
    The default value is ``$(SPOOL)/Accountantnew.log``

:macro-def:`ACCOUNTANT_LOCAL_DOMAIN[NEGOTIATOR]`
    Describes the local UID domain. This variable is used to decide if a
    user is local or remote. A user is considered to be in the local
    domain if their UID domain matches the value of this variable.
    Usually, this variable is set to the local UID domain. If not
    defined, all users are considered local.

:macro-def:`MAX_ACCOUNTANT_DATABASE_SIZE[NEGOTIATOR]`
    This macro defines the maximum size (in bytes) that the accountant
    database log file can reach before it is truncated (which re-writes
    the file in a more compact format). If, after truncating, the file
    is larger than one half the maximum size specified with this macro,
    the maximum size will be automatically expanded. The default is 1
    megabyte (1000000).

:macro-def:`NEGOTIATOR_DISCOUNT_SUSPENDED_RESOURCES[NEGOTIATOR]`
    This macro tells the negotiator to not count resources that are
    suspended when calculating the number of resources a user is using.
    Defaults to false, that is, a user is still charged for a resource
    even when that resource has suspended the job.

:macro-def:`NEGOTIATOR_SOCKET_CACHE_SIZE[NEGOTIATOR]`
    This macro defines the maximum number of sockets that the
    *condor_negotiator* keeps in its open socket cache. Caching open
    sockets makes the negotiation protocol more efficient by eliminating
    the need for socket connection establishment for each negotiation
    cycle. The default is currently 500. To be effective, this parameter
    should be set to a value greater than the number of
    *condor_schedd* s submitting jobs to the negotiator at any time.
    If you lower this number, you must run :tool:`condor_restart` and not
    just :tool:`condor_reconfig` for the change to take effect.

:macro-def:`NEGOTIATOR_INFORM_STARTD[NEGOTIATOR]`
    Boolean setting that controls if the *condor_negotiator* should
    inform the *condor_startd* when it has been matched with a job. The
    default is ``False``. When this is set to the default value of
    ``False``, the *condor_startd* will never enter the Matched state,
    and will go directly from Unclaimed to Claimed. Because this
    notification is done via UDP, if a pool is configured so that the
    execute hosts do not create UDP command sockets (see the
    :macro:`WANT_UDP_COMMAND_SOCKET` setting for details), the
    *condor_negotiator* should be configured not to attempt to contact
    these *condor_startd* daemons by using the default value.

:macro-def:`NEGOTIATOR_PRE_JOB_RANK[NEGOTIATOR]`
    Resources that match a request are first sorted by this expression.
    If there are any ties in the rank of the top choice, the top
    resources are sorted by the user-supplied rank in the job ClassAd,
    then by :macro:`NEGOTIATOR_POST_JOB_RANK`, then by :macro:`PREEMPTION_RANK`
    (if the match would cause preemption and there are still any ties in
    the top choice). MY refers to attributes of the machine ClassAd and
    TARGET refers to the job ClassAd. The purpose of the pre job rank is
    to allow the pool administrator to override any other rankings, in
    order to optimize overall throughput. For example, it is commonly
    used to minimize preemption, even if the job rank prefers a machine
    that is busy. If explicitly set to be undefined, this expression has
    no effect on the ranking of matches. The default value prefers to
    match multi-core jobs to dynamic slots in a best fit manner:

    .. code-block:: condor-config

          NEGOTIATOR_PRE_JOB_RANK = (10000000 * My.Rank) + \
           (1000000 * (RemoteOwner =?= UNDEFINED)) - (100000 * Cpus) - Memory

:macro-def:`NEGOTIATOR_POST_JOB_RANK[NEGOTIATOR]`
    Resources that match a request are first sorted by
    :macro:`NEGOTIATOR_PRE_JOB_RANK`. If there are any ties in the rank of
    the top choice, the top resources are sorted by the user-supplied
    rank in the job ClassAd, then by :macro:`NEGOTIATOR_POST_JOB_RANK`, then
    by :macro:`PREEMPTION_RANK` (if the match would cause preemption and
    there are still any ties in the top choice). ``MY.`` refers to
    attributes of the machine ClassAd and ``TARGET.`` refers to the job
    ClassAd. The purpose of the post job rank is to allow the pool
    administrator to choose between machines that the job ranks equally.
    The default value is

    .. code-block:: condor-config

          NEGOTIATOR_POST_JOB_RANK = \
           (RemoteOwner =?= UNDEFINED) * \
           (ifThenElse(isUndefined(KFlops), 1000, Kflops) - \
           SlotID - 1.0e10*(Offline=?=True))

:macro-def:`PREEMPTION_REQUIREMENTS[NEGOTIATOR]`
    When considering user priorities, the negotiator will not preempt a
    job running on a given machine unless this expression evaluates to
    ``True``, and the owner of the idle job has a better priority than
    the owner of the running job. The :macro:`PREEMPTION_REQUIREMENTS`
    expression is evaluated within the context of the candidate machine
    ClassAd and the candidate idle job ClassAd; thus the MY scope prefix
    refers to the machine ClassAd, and the TARGET scope prefix refers to
    the ClassAd of the idle (candidate) job. There is no direct access
    to the currently running job, but attributes of the currently
    running job that need to be accessed in :macro:`PREEMPTION_REQUIREMENTS`
    can be placed in the machine ClassAd using :macro:`STARTD_JOB_ATTRS`.
    If not explicitly set in the HTCondor configuration file, the default
    value for this expression is ``False``. :macro:`PREEMPTION_REQUIREMENTS`
    should include the term ``(SubmitterGroup =?= RemoteGroup)``, if a
    preemption policy that respects group quotas is desired. Note that
    this variable does not influence other potential causes of preemption,
    such as the :macro:`RANK` of the *condor_startd*, or :macro:`PREEMPT` expressions. See
    :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
    for a general discussion of limiting preemption.

:macro-def:`PREEMPTION_REQUIREMENTS_STABLE[NEGOTIATOR]`
    A boolean value that defaults to ``True``, implying that all
    attributes utilized to define the :macro:`PREEMPTION_REQUIREMENTS`
    variable will not change within a negotiation period time interval.
    If utilized attributes will change during the negotiation period
    time interval, then set this variable to ``False``.

:macro-def:`PREEMPTION_RANK[NEGOTIATOR]`
    Resources that match a request are first sorted by
    :macro:`NEGOTIATOR_PRE_JOB_RANK`. If there are any ties in the rank of
    the top choice, the top resources are sorted by the user-supplied
    rank in the job ClassAd, then by :macro:`NEGOTIATOR_POST_JOB_RANK`, then
    by :macro:`PREEMPTION_RANK` (if the match would cause preemption and
    there are still any ties in the top choice). MY refers to attributes
    of the machine ClassAd and TARGET refers to the job ClassAd. This
    expression is used to rank machines that the job and the other
    negotiation expressions rank the same. For example, if the job has
    no preference, it is usually preferable to preempt a job with a
    small :ad-attr:`ImageSize` instead of a job with a large :ad-attr:`ImageSize`. The
    default value first considers the user's priority and chooses the
    user with the worst priority. Then, among the running jobs of that
    user, it chooses the job with the least accumulated run time:

    .. code-block:: condor-config

          PREEMPTION_RANK = (RemoteUserPrio * 1000000) - \
           ifThenElse(isUndefined(TotalJobRunTime), 0, TotalJobRunTime)

:macro-def:`PREEMPTION_RANK_STABLE[NEGOTIATOR]`
    A boolean value that defaults to ``True``, implying that all
    attributes utilized to define the :macro:`PREEMPTION_RANK` variable will
    not change within a negotiation period time interval. If utilized
    attributes will change during the negotiation period time interval,
    then set this variable to ``False``.

:macro-def:`NEGOTIATOR_SLOT_CONSTRAINT[NEGOTIATOR]`
    An expression which constrains which machine ClassAds are fetched
    from the *condor_collector* by the *condor_negotiator* during a
    negotiation cycle.

:macro-def:`NEGOTIATOR_SUBMITTER_CONSTRAINT[NEGOTIATOR]`
    An expression which constrains which submitter ClassAds are fetched
    from the *condor_collector* by the *condor_negotiator* during a
    negotiation cycle.
    The *condor_negotiator* will ignore the jobs of submitters whose
    submitter ads don't match this constraint.

:macro-def:`NEGOTIATOR_JOB_CONSTRAINT[NEGOTIATOR]`
    An expression which constrains which job ClassAds are considered for
    matchmaking by the *condor_negotiator*. This parameter is read by
    the *condor_negotiator* and sent to the *condor_schedd* for
    evaluation. *condor_schedd* s older than version 8.7.7 will ignore
    this expression and so will continue to send all jobs to the
    *condor_negotiator*.

:macro-def:`NEGOTIATOR_TRIM_SHUTDOWN_THRESHOLD[NEGOTIATOR]`
    This setting is not likely to be customized, except perhaps within a
    glidein setting. An integer expression that evaluates to a value
    within the context of the *condor_negotiator* ClassAd, with a
    default value of 0. When this expression evaluates to an integer X
    greater than 0, the *condor_negotiator* will not make matches to
    machines that contain the ClassAd attribute ``DaemonShutdown`` which
    evaluates to ``True``, when that shut down time is X seconds into
    the future. The idea here is a mechanism to prevent matching with
    machines that are quite close to shutting down, since the match
    would likely be a waste of time.

:macro-def:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT[NEGOTIATOR]`  or :macro-def:`GROUP_DYNAMIC_MACH_CONSTRAINT[NEGOTIATOR]`
    This optional expression specifies which machine ClassAds should be
    counted when computing the size of the pool. It applies both for
    group quota allocation and when there are no groups. The default is
    to count all machine ClassAds. When extra slots exist for special
    purposes, as, for example, suspension slots or file transfer slots,
    this expression can be used to inform the *condor_negotiator* that
    only normal slots should be counted when computing how big each
    group's share of the pool should be.

    The name :macro:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT` replaces
    ``GROUP_DYNAMIC_MACH_CONSTRAINT`` as of HTCondor version 7.7.3.
    Using the older name causes a warning to be logged, although the
    behavior is unchanged.

:macro-def:`NEGOTIATOR_DEBUG[NEGOTIATOR]`
    This macro (and other settings related to debug logging in the
    negotiator) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER[NEGOTIATOR]`
    The maximum number of seconds the *condor_negotiator* will spend
    with each individual submitter during one negotiation cycle. Once
    this time limit has been reached, the *condor_negotiator* will skip
    over requests from this submitter until the next negotiation cycle.
    It defaults to 60 seconds.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_SCHEDD[NEGOTIATOR]`
    The maximum number of seconds the *condor_negotiator* will spend
    with each individual *condor_schedd* during one negotiation cycle.
    Once this time limit has been reached, the *condor_negotiator* will
    skip over requests from this *condor_schedd* until the next
    negotiation cycle. It defaults to 120 seconds.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_CYCLE[NEGOTIATOR]`
    The maximum number of seconds the *condor_negotiator* will spend in
    total across all submitters during one negotiation cycle. Once this
    time limit has been reached, the *condor_negotiator* will skip over
    requests from all submitters until the next negotiation cycle. It
    defaults to 1200 seconds.

:macro-def:`NEGOTIATOR_MAX_TIME_PER_PIESPIN[NEGOTIATOR]`
    The maximum number of seconds the *condor_negotiator* will spend
    with a submitter in one pie spin. A negotiation cycle is composed of
    at least one pie spin, possibly more, depending on whether there are
    still machines left over after computing fair shares and negotiating
    with each submitter. By limiting the maximum length of a pie spin or
    the maximum time per submitter per negotiation cycle, the
    *condor_negotiator* is protected against spending a long time
    talking to one submitter, for example someone with a very slow
    *condor_schedd* daemon. But, this can result in unfair allocation
    of machines or some machines not being allocated at all. See
    :doc:`/admin-manual/cm-configuration`
    for a description of a pie slice. It defaults to 120 seconds.

:macro-def:`NEGOTIATOR_DEPTH_FIRST[NEGOTIATOR]`
    A boolean value which defaults to false. When partitionable slots
    are enabled, and this parameter is true, the negotiator tries to
    pack as many jobs as possible on each machine before moving on to
    the next machine.

:macro-def:`USE_RESOURCE_REQUEST_COUNTS[NEGOTIATOR]`
    A boolean value that defaults to ``True``. When ``True``, the
    latency of negotiation will be reduced when there are many jobs next
    to each other in the queue with the same auto cluster, and many
    matches are being made. When ``True``, the *condor_schedd* tells
    the *condor_negotiator* to send X matches at a time, where X equals
    number of consecutive jobs in the queue within the same auto
    cluster.

:macro-def:`NEGOTIATOR_RESOURCE_REQUEST_LIST_SIZE[NEGOTIATOR]`
    An integer tuning parameter used by the *condor_negotiator* to
    control the number of resource requests fetched from a
    *condor_schedd* per network round-trip. With higher values, the
    latency of negotiation can be significantly be reduced when
    negotiating with a *condor_schedd* running HTCondor version 8.3.0
    or more recent, especially over a wide-area network. Setting this
    value too high, however, could cause the *condor_schedd* to
    unnecessarily block on network I/O. The default value is 200. If
    :macro:`USE_RESOURCE_REQUEST_COUNTS` is set to ``False``, then this
    variable will be unconditionally set to a value of 1.

:macro-def:`NEGOTIATOR_MATCH_EXPRS[NEGOTIATOR]`
    A comma-separated list of macro names that are inserted as ClassAd
    attributes into matched job ClassAds. The attribute name in the
    ClassAd will be given the prefix ``NegotiatorMatchExpr``, if the
    macro name does not already begin with that. Example:

    .. code-block:: condor-config

          NegotiatorName = "My Negotiator"
          NEGOTIATOR_MATCH_EXPRS = NegotiatorName

    As a result of the above configuration, jobs that are matched by
    this *condor_negotiator* will contain the following attribute when
    they are sent to the *condor_startd*:

    .. code-block:: condor-config

          NegotiatorMatchExprNegotiatorName = "My Negotiator"

    The expressions inserted by the *condor_negotiator* may be useful
    in *condor_startd* policy expressions, when the *condor_startd*
    belongs to multiple HTCondor pools.

:macro-def:`NEGOTIATOR_MATCHLIST_CACHING[NEGOTIATOR]`
    A boolean value that defaults to ``True``. When ``True``, it enables
    an optimization in the *condor_negotiator* that works with auto
    clustering. In determining the sorted list of machines that a job
    might use, the job goes to the first machine off the top of the
    list. If :macro:`NEGOTIATOR_MATCHLIST_CACHING` is ``True``, and if the
    next job is part of the same auto cluster, meaning that it is a very
    similar job, the *condor_negotiator* will reuse the previous list
    of machines, instead of recreating the list from scratch.

:macro-def:`NEGOTIATOR_CONSIDER_PREEMPTION[NEGOTIATOR]`
    For expert users only. A boolean value that defaults to ``True``.
    When ``False``, it can cause the *condor_negotiator* to run faster
    and also have better spinning pie accuracy. Only set this to
    ``False`` if :macro:`PREEMPTION_REQUIREMENTS` is ``False``, and if all
    *condor_startd* rank expressions are ``False``.

:macro-def:`NEGOTIATOR_CONSIDER_EARLY_PREEMPTION[NEGOTIATOR]`
    A boolean value that when ``False`` (the default), prevents the
    *condor_negotiator* from matching jobs to claimed slots that cannot
    immediately be preempted due to :macro:`MAXJOBRETIREMENTTIME`.

:macro-def:`ALLOW_PSLOT_PREEMPTION[NEGOTIATOR]`
    A boolean value that defaults to ``False``. When set to ``True`` for
    the *condor_negotiator*, it enables a new matchmaking mode in which
    one or more dynamic slots can be preempted in order to make enough
    resources available in their parent partitionable slot for a job to
    successfully match to the partitionable slot.

:macro-def:`STARTD_AD_REEVAL_EXPR[NEGOTIATOR]`
    A boolean value evaluated in the context of each machine ClassAd
    within a negotiation cycle that determines whether the ClassAd from
    the *condor_collector* is to replace the stashed ClassAd utilized
    during the previous negotiation cycle. When ``True``, the ClassAd
    from the *condor_collector* does replace the stashed one. When not
    defined, the default value is to replace the stashed ClassAd if the
    stashed ClassAd's sequence number is older than its potential
    replacement.

:macro-def:`NEGOTIATOR_UPDATE_AFTER_CYCLE[NEGOTIATOR]`
    A boolean value that defaults to ``False``. When ``True``, it will
    force the *condor_negotiator* daemon to publish an update to the
    *condor_collector* at the end of every negotiation cycle. This is
    useful if monitoring statistics for the previous negotiation cycle.

:macro-def:`NEGOTIATOR_READ_CONFIG_BEFORE_CYCLE[NEGOTIATOR]`
    A boolean value that defaults to ``False``. When ``True``, the
    *condor_negotiator* will re-read the configuration prior to
    beginning each negotiation cycle. Note that this operation will
    update configured behaviors such as concurrency limits, but not data
    structures constructed during a full reconfiguration, such as the
    group quota hierarchy. A full reconfiguration, for example as
    accomplished with :tool:`condor_reconfig`, remains the best way to
    guarantee that all *condor_negotiator* configuration is completely
    updated.

:macro-def:`<NAME>_LIMIT[NEGOTIATOR]`
    An integer value that defines the amount of resources available for
    jobs which declare that they use some consumable resource as
    described in :ref:`admin-manual/cm-configuration:concurrency
    limits`. ``<Name>`` is a string invented to uniquely describe the resource.

:macro-def:`CONCURRENCY_LIMIT_DEFAULT[NEGOTIATOR]`
    An integer value that describes the number of resources available
    for any resources that are not explicitly named defined with the
    configuration variable :macro:`<NAME>_LIMIT`. If not defined, no limits
    are set for resources not explicitly identified using
    :macro:`<NAME>_LIMIT`.

:macro-def:`CONCURRENCY_LIMIT_DEFAULT_<NAME>[NEGOTIATOR]`
    If set, this defines a default concurrency limit for all resources
    that start with ``<NAME>.``

The following configuration macros affect negotiation for group users.

:macro-def:`GROUP_NAMES[NEGOTIATOR]`
    A comma-separated list of the recognized group names, case
    insensitive. If undefined (the default), group support is disabled.
    Group names must not conflict with any user names. That is, if there
    is a physics group, there may not be a physics user. Any group that
    is defined here must also have a quota, or the group will be
    ignored. Example:

    .. code-block:: condor-config

            GROUP_NAMES = group_physics, group_chemistry


:macro-def:`GROUP_QUOTA_<groupname>[NEGOTIATOR]`
    A floating point value to represent a static quota specifying an
    integral number of machines for the hierarchical group identified by
    ``<groupname>``. It is meaningless to specify a non integer value,
    since only integral numbers of machines can be allocated. Example:

    .. code-block:: condor-config

            GROUP_QUOTA_group_physics = 20
            GROUP_QUOTA_group_chemistry = 10


    When both static and dynamic quotas are defined for a specific
    group, the static quota is used and the dynamic quota is ignored.

:macro-def:`GROUP_QUOTA_DYNAMIC_<groupname>[NEGOTIATOR]`
    A floating point value in the range 0.0 to 1.0, inclusive,
    representing a fraction of a pool's machines (slots) set as a
    dynamic quota for the hierarchical group identified by
    ``<groupname>``. For example, the following specifies that a quota
    of 25% of the total machines are reserved for members of the
    group_biology group.

    .. code-block:: condor-config

           GROUP_QUOTA_DYNAMIC_group_biology = 0.25


    The group name must be specified in the :macro:`GROUP_NAMES` list.

    This section has not yet been completed

:macro-def:`GROUP_PRIO_FACTOR_<groupname>[NEGOTIATOR]`
    A floating point value greater than or equal to 1.0 to specify the
    default user priority factor for <groupname>. The group name must
    also be specified in the :macro:`GROUP_NAMES` list.
    :macro:`GROUP_PRIO_FACTOR_<groupname>` is evaluated when the negotiator
    first negotiates for the user as a member of the group. All members
    of the group inherit the default priority factor when no other value
    is present. For example, the following setting specifies that all
    members of the group named group_physics inherit a default user
    priority factor of 2.0:

    .. code-block:: condor-config

            GROUP_PRIO_FACTOR_group_physics = 2.0


:macro-def:`GROUP_AUTOREGROUP[NEGOTIATOR]`
    A boolean value (defaults to ``False``) that when ``True``, causes
    users who submitted to a specific group to also negotiate a second
    time with the ``<none>`` group, to be considered with the
    independent job submitters. This allows group submitted jobs to be
    matched with idle machines even if the group is over its quota. The
    user name that is used for accounting and prioritization purposes is
    still the group user as specified by :ad-attr:`AccountingGroup` in the job
    ClassAd.

:macro-def:`GROUP_AUTOREGROUP_<groupname>[NEGOTIATOR]`
    This is the same as :macro:`GROUP_AUTOREGROUP`, but it is settable on a
    per-group basis. If no value is specified for a given group, the
    default behavior is determined by :macro:`GROUP_AUTOREGROUP`, which in
    turn defaults to ``False``.

:macro-def:`GROUP_ACCEPT_SURPLUS[NEGOTIATOR]`
    A boolean value that, when ``True``, specifies that groups should be
    allowed to use more than their configured quota when there is not
    enough demand from other groups to use all of the available
    machines. The default value is ``False``.

:macro-def:`GROUP_ACCEPT_SURPLUS_<groupname>[NEGOTIATOR]`
    A boolean value applied as a group-specific version of
    :macro:`GROUP_ACCEPT_SURPLUS`. When not specified, the value of
    :macro:`GROUP_ACCEPT_SURPLUS` applies to the named group.

:macro-def:`GROUP_QUOTA_ROUND_ROBIN_RATE[NEGOTIATOR]`
    The maximum sum of weighted slots that should be handed out to an
    individual submitter in each iteration within a negotiation cycle.
    If slot weights are not being used by the *condor_negotiator*, as
    specified by ``NEGOTIATOR_USE_SLOT_WEIGHTS = False``, then this
    value is just the (unweighted) number of slots. The default value is
    a very big number, effectively infinite. Setting the value to a
    number smaller than the size of the pool can help avoid starvation.
    An example of the starvation problem is when there are a subset of
    machines in a pool with large memory, and there are multiple job
    submitters who desire all of these machines. Normally, HTCondor will
    decide how much of the full pool each person should get, and then
    attempt to hand out that number of resources to each person. Since
    the big memory machines are only a subset of pool, it may happen
    that they are all given to the first person contacted, and the
    remainder requiring large memory machines get nothing. Setting
    :macro:`GROUP_QUOTA_ROUND_ROBIN_RATE` to a value that is small compared
    to the size of subsets of machines will reduce starvation at the
    cost of possibly slowing down the rate at which resources are
    allocated.

:macro-def:`GROUP_QUOTA_MAX_ALLOCATION_ROUNDS[NEGOTIATOR]`
    An integer that specifies the maximum number of times within one
    negotiation cycle the *condor_negotiator* will calculate how many
    slots each group deserves and attempt to allocate them. The default
    value is 3. The reason it may take more than one round is that some
    groups may not have jobs that match some of the available machines,
    so some of the slots that were withheld for those groups may not get
    allocated in any given round.

:macro-def:`NEGOTIATOR_USE_SLOT_WEIGHTS[NEGOTIATOR]`
    A boolean value with a default of ``True``. When ``True``, the
    *condor_negotiator* pays attention to the machine ClassAd attribute
    :ad-attr:`SlotWeight`. When ``False``, each slot effectively has a weight
    of 1.

:macro-def:`NEGOTIATOR_USE_WEIGHTED_DEMAND[NEGOTIATOR]`
    A boolean value that defaults to ``True``. When ``False``, the
    behavior is the same as for HTCondor versions prior to 7.9.6. If
    ``True``, when the *condor_schedd* advertises ``IdleJobs`` in the
    submitter ClassAd, which represents the number of idle jobs in the
    queue for that submitter, it will also advertise the total number of
    requested cores across all idle jobs from that submitter,
    :ad-attr:`WeightedIdleJobs`. If partitionable slots are being used, and if
    hierarchical group quotas are used, and if any hierarchical group
    quotas set :macro:`GROUP_ACCEPT_SURPLUS` to ``True``, and if
    configuration variable :ad-attr:`SlotWeight` :index:`SlotWeight` is
    set to the number of cores, then setting this configuration variable
    to ``True`` allows the amount of surplus allocated to each group to
    be calculated correctly.

:macro-def:`GROUP_SORT_EXPR[NEGOTIATOR]`
    A floating point ClassAd expression that controls the order in which
    the *condor_negotiator* considers groups when allocating resources.
    The smallest magnitude positive value goes first. The default value
    is set such that group ``<none>`` always goes last when considering
    group quotas, and groups are considered in starvation order (the
    group using the smallest fraction of its resource quota is
    considered first).

:macro-def:`NEGOTIATOR_ALLOW_QUOTA_OVERSUBSCRIPTION[NEGOTIATOR]`
    A boolean value that defaults to ``True``. When ``True``, the
    behavior of resource allocation when considering groups is more like
    it was in the 7.4 stable series of HTCondor. In implementation, when
    ``True``, the static quotas of subgroups will not be scaled when the
    sum of these static quotas of subgroups sums to more than the
    group's static quota. This behavior is desirable when using static
    quotas, unless the sum of subgroup quotas is considerably less than
    the group's quota, as scaling is currently based on the number of
    machines available, not assigned quotas (for static quotas).

condor_procd Configuration File Macros
---------------------------------------

:macro-def:`USE_PROCD[PROCD]`
    This boolean variable determines whether the :tool:`condor_procd` will be
    used for managing process families. If the :tool:`condor_procd` is not
    used, each daemon will run the process family tracking logic on its
    own. Use of the :tool:`condor_procd` results in improved scalability
    because only one instance of this logic is required. The
    :tool:`condor_procd` is required when using group ID-based process
    tracking (see :ref:`admin-manual/ep-policy-configuration:group
    id-based process tracking`.
    In this case, the :macro:`USE_PROCD` setting will be ignored and a
    :tool:`condor_procd` will always be used. By default, the
    :tool:`condor_master` will start a :tool:`condor_procd` that all other daemons
    that need process family tracking will use. A daemon that uses the
    :tool:`condor_procd` will start a :tool:`condor_procd` for use by itself and
    all of its child daemons.

:macro-def:`PROCD_MAX_SNAPSHOT_INTERVAL[PROCD]`
    This setting determines the maximum time that the :tool:`condor_procd`
    will wait between probes of the system for information about the
    process families it is tracking.

:macro-def:`PROCD_LOG[PROCD]`
    Specifies a log file for the :tool:`condor_procd` to use. Note that by
    design, the :tool:`condor_procd` does not include most of the other logic
    that is shared amongst the various HTCondor daemons. This means that
    the :tool:`condor_procd` does not include the normal HTCondor logging
    subsystem, and thus multiple debug levels are not supported.
    :macro:`PROCD_LOG` defaults to ``$(LOG)/ProcLog``. Note that enabling
    ``D_PROCFAMILY`` in the debug level for any other daemon will cause
    it to log all interactions with the :tool:`condor_procd`.

:macro-def:`MAX_PROCD_LOG[PROCD]`
    Controls the maximum length in bytes to which the :tool:`condor_procd`
    log will be allowed to grow. The log file will grow to the specified
    length, then be saved to a file with the suffix ``.old``. The
    ``.old`` file is overwritten each time the log is saved, thus the
    maximum space devoted to logging will be twice the maximum length of
    this log file. A value of 0 specifies that the file may grow without
    bounds. The default is 10 MiB.

:macro-def:`PROCD_ADDRESS[PROCD]`
    This specifies the address that the :tool:`condor_procd` will use to
    receive requests from other HTCondor daemons. On Unix, this should
    point to a file system location that can be used for a named pipe.
    On Windows, named pipes are also used but they do not exist in the
    file system. The default setting is $(RUN)/procd_pipe on Unix
    and \\\\.\\pipe\\procd_pipe on Windows.

:macro-def:`USE_GID_PROCESS_TRACKING[PROCD]`
    A boolean value that defaults to ``False``. When ``True``, a job's
    initial process is assigned a dedicated GID which is further used by
    the :tool:`condor_procd` to reliably track all processes associated with
    a job. When ``True``, values for :macro:`MIN_TRACKING_GID` and
    :macro:`MAX_TRACKING_GID` must also be set, or HTCondor will abort,
    logging an error message. See :ref:`admin-manual/ep-policy-configuration:group
    id-based process tracking` for a detailed description.

:macro-def:`MIN_TRACKING_GID[PROCD]`
    An integer value, that together with :macro:`MAX_TRACKING_GID` specify a
    range of GIDs to be assigned on a per slot basis for use by the
    :tool:`condor_procd` in tracking processes associated with a job. See
    :ref:`admin-manual/ep-policy-configuration:group id-based
    process tracking` for a detailed description.

:macro-def:`MAX_TRACKING_GID[PROCD]`
    An integer value, that together with :macro:`MIN_TRACKING_GID` specify a
    range of GIDs to be assigned on a per slot basis for use by the
    :tool:`condor_procd` in tracking processes associated with a job. See
    :ref:`admin-manual/ep-policy-configuration:group id-based process
    tracking` for a detailed description.

:macro-def:`BASE_CGROUP[PROCD]`
    The path to the directory used as the virtual file system for the
    implementation of Linux kernel cgroups. This variable defaults to
    the string ``htcondor``, and is only used on Linux systems. To
    disable cgroup tracking, define this to an empty string. See
    :ref:`admin-manual/ep-policy-configuration:cgroup-based process
    tracking` for a description of cgroup-based process tracking.
    An administrator can configure distinct cgroup roots for 
    different slot types within the same startd by prefixing
    the *BASE_CGROUP* macro with the slot type. e.g. setting
    SLOT_TYPE_1.BASE_CGROUP = hiprio_cgroup and SLOT_TYPE_2.BASE_CGROUP = low_prio

:macro-def:`CREATE_CGROUP_WITHOUT_ROOT[PROCD]`
    Defaults to false.  When true, on a Linux cgroup v2 system, a
    condor system without root privilege (such as a glidein)
    will attempt to create cgroups for jobs.  The condor_master
    must have been started under a writeable cgroup for this to work.

condor_credd Configuration File Macros
---------------------------------------

:index:`condor_credd daemon`
:index:`condor_credd configuration variables<single: condor_credd configuration variables; configuration>`

These macros affect the *condor_credd* and its credmon plugin.

:macro-def:`CREDD_HOST[CREDD]`
    The host name of the machine running the *condor_credd* daemon.

:macro-def:`CREDD_POLLING_TIMEOUT[CREDD]`
    An integer value representing the number of seconds that the
    *condor_credd*, *condor_starter*, and *condor_schedd* daemons
    will wait for valid credentials to be produced by a credential
    monitor (CREDMON) service. The default value is 20.

:macro-def:`CREDD_CACHE_LOCALLY[CREDD]`
    A boolean value that defaults to ``False``. When ``True``, the first
    successful password fetch operation to the *condor_credd* daemon
    causes the password to be stashed in a local, secure password store.
    Subsequent uses of that password do not require communication with
    the *condor_credd* daemon.

:macro-def:`CRED_SUPER_USERS[CREDD]`
    A comma and/or space separated list of user names on a given machine
    that are permitted to store credentials for any user when using the
    :tool:`condor_store_cred` command. When not on this list, users can only
    store their own credentials. Entries in this list can contain a
    single '\*' wildcard character, which matches any sequence of
    characters.

:macro-def:`SKIP_WINDOWS_LOGON_NETWORK[CREDD]`
    A boolean value that defaults to ``False``. When ``True``, Windows
    authentication skips trying authentication with the
    ``LOGON_NETWORK`` method first, and attempts authentication with
    ``LOGON_INTERACTIVE`` method. This can be useful if many
    authentication failures are noticed, potentially leading to users
    getting locked out.

:macro-def:`CREDMON_KRB[CREDD]`
    The path to the credmon daemon process when using the Kerberos 
    credentials type.  The default is /usr/sbin/condor_credmon_krb

:macro-def:`CREDMON_OAUTH[CREDD]`
    The path to the credmon daemon process when using the OAuth2
    credentials type.  The default is /usr/sbin/condor_credmon_oauth.

:macro-def:`CREDMON_OAUTH_TOKEN_MINIMUM[CREDD]`
    The minimum time in seconds that OAuth2 tokens should have remaining
    on them when they are generated.  The default is 40 minutes.
    This is currently implemented only in the vault credmon, not the
    default oauth credmon.

:macro-def:`CREDMON_OAUTH_TOKEN_REFRESH[CREDD]`
    The time in seconds between renewing OAuth2 tokens.  The default is
    half of :macro:`CREDMON_OAUTH_TOKEN_MINIMUM`.  This is currently implemented
    only in the vault credmon, not the default oauth credmon.

:macro-def:`LOCAL_CREDMON_TOKEN_VERSION[CREDD]`
    A string valued macro that defines what the local issuer should put into
    the "ver" field of the token.  Defaults to ``scitoken:2.0``.

:macro-def:`SEC_CREDENTIAL_DIRECTORY[CREDD]`
    A string valued macro that defines a path directory where
    the credmon looks for credential files.

:macro-def:`SEC_CREDENTIAL_MONITOR[CREDD]`
    A string valued macro that defines a path to the credential monitor
    executable.

:macro-def:`SEC_CREDENTIAL_GETTOKEN_OPTS[CREDD]` configuration option to
    pass additional command line options to gettoken.  Mostly
    used for vault, where this should be set to "-a vault_name".

condor_gridmanager Configuration File Entries
----------------------------------------------

:index:`condor_gridmanager configuration variables<single: condor_gridmanager configuration variables; configuration>`

These macros affect the *condor_gridmanager*.

:macro-def:`GRIDMANAGER_LOG[GRIDMANAGER]`
    Defines the path and file name for the log of the
    *condor_gridmanager*. The owner of the file is the condor user.

:macro-def:`GRIDMANAGER_CHECKPROXY_INTERVAL[GRIDMANAGER]`
    The number of seconds between checks for an updated X509 proxy
    credential. The default is 10 minutes (600 seconds).

:macro-def:`GRIDMANAGER_PROXY_REFRESH_TIME[GRIDMANAGER]`
    For remote schedulers that allow for X.509 proxy refresh,
    the *condor_gridmanager* will not forward a
    refreshed proxy until the lifetime left for the proxy on the remote
    machine falls below this value. The value is in seconds and the
    default is 21600 (6 hours).

:macro-def:`GRIDMANAGER_MINIMUM_PROXY_TIME[GRIDMANAGER]`
    The minimum number of seconds before expiration of the X509 proxy
    credential for the gridmanager to continue operation. If seconds
    until expiration is less than this number, the gridmanager will
    shutdown and wait for a refreshed proxy credential. The default is 3
    minutes (180 seconds).

:macro-def:`HOLD_JOB_IF_CREDENTIAL_EXPIRES[GRIDMANAGER]`
    True or False. Defaults to True. If True, and for grid universe jobs
    only, HTCondor-G will place a job on hold
    :macro:`GRIDMANAGER_MINIMUM_PROXY_TIME` seconds before the proxy expires.
    If False, the job will stay in the last known state, and HTCondor-G
    will periodically check to see if the job's proxy has been
    refreshed, at which point management of the job will resume.

:macro-def:`GRIDMANAGER_SELECTION_EXPR[GRIDMANAGER]`
    By default, the gridmanager operates on a per-:ad-attr:`Owner` basis.  That
    is, the *condor_schedd* starts a distinct *condor_gridmanager* for each
    grid universe job with a distinct :ad-attr:`Owner`.  For additional isolation
    and/or scalability, you may set this macro to a ClassAd expression.
    It will be evaluated against each grid universe job, and jobs with
    the same evaluated result will go to the same gridmanager.  For instance,
    if you want to isolate job going to different remote sites from each
    other, the following expression works:

    .. code-block:: condor-config

          GRIDMANAGER_SELECTION_EXPR = GridResource

:macro-def:`GRIDMANAGER_LOG_APPEND_SELECTION_EXPR[GRIDMANAGER]`
    A boolean value that defaults to ``False``. When ``True``, the
    evaluated value of :macro:`GRIDMANAGER_SELECTION_EXPR` (if set) is
    appended to the value of :macro:`GRIDMANAGER_LOG` for each
    *condor_gridmanager* instance.
    The value is sanitized to remove characters that have special
    meaning to the shell.
    This allows each *condor_gridmanager* instance that runs concurrently
    to write to a separate daemon log.

:macro-def:`GRIDMANAGER_CONTACT_SCHEDD_DELAY[GRIDMANAGER]`
    The minimum number of seconds between connections to the
    *condor_schedd*. The default is 5 seconds.

:macro-def:`GRIDMANAGER_JOB_PROBE_INTERVAL[GRIDMANAGER]`
    The number of seconds between active probes for the status of a
    submitted job. The default is 1 minute (60 seconds). Intervals
    specific to grid types can be set by appending the name of the grid
    type to the configuration variable name, as the example

    .. code-block:: condor-config

          GRIDMANAGER_JOB_PROBE_INTERVAL_ARC = 300


:macro-def:`GRIDMANAGER_JOB_PROBE_RATE[GRIDMANAGER]`
    The maximum number of job status probes per second that will be
    issued to a given remote resource. The time between status probes
    for individual jobs may be lengthened beyond
    :macro:`GRIDMANAGER_JOB_PROBE_INTERVAL` to enforce this rate.
    The default is 5 probes per second. Rates specific to grid types can
    be set by appending the name of the grid type to the configuration
    variable name, as the example

    .. code-block:: condor-config

          GRIDMANAGER_JOB_PROBE_RATE_ARC = 15


:macro-def:`GRIDMANAGER_RESOURCE_PROBE_INTERVAL[GRIDMANAGER]`
    When a resource appears to be down, how often (in seconds) the
    *condor_gridmanager* should ping it to test if it is up again.
    The default is 5 minutes (300 seconds).

:macro-def:`GRIDMANAGER_EMPTY_RESOURCE_DELAY[GRIDMANAGER]`
    The number of seconds that the *condor_gridmanager* retains
    information about a grid resource, once the *condor_gridmanager*
    has no active jobs on that resource. An active job is a grid
    universe job that is in the queue, for which :ad-attr:`JobStatus` is
    anything other than Held. Defaults to 300 seconds.

:macro-def:`GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE[GRIDMANAGER]`
    An integer value that limits the number of jobs that a
    *condor_gridmanager* daemon will submit to a resource. A
    comma-separated list of pairs that follows this integer limit will
    specify limits for specific remote resources. Each pair is a host
    name and the job limit for that host. Consider the example:

    .. code-block:: condor-config

          GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE = 200, foo.edu, 50, bar.com, 100


    In this example, all resources have a job limit of 200, except
    foo.edu, which has a limit of 50, and bar.com, which has a limit of
    100.

    Limits specific to grid types can be set by appending the name of
    the grid type to the configuration variable name, as the example

    .. code-block:: condor-config

          GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE_PBS = 300


    In this example, the job limit for all PBS resources is 300.
    Defaults to 1000.

:macro-def:`GAHP_DEBUG_HIDE_SENSITIVE_DATA[GRIDMANAGER]`
    A boolean value that determines when sensitive data such as security
    keys and passwords are hidden, when communication to or from a GAHP
    server is written to a daemon log. The default is ``True``, hiding
    sensitive data.

:macro-def:`GRIDMANAGER_GAHP_CALL_TIMEOUT[GRIDMANAGER]`
    The number of seconds after which a pending GAHP command should time
    out. The default is 5 minutes (300 seconds).

:macro-def:`GRIDMANAGER_GAHP_RESPONSE_TIMEOUT[GRIDMANAGER]`
    The *condor_gridmanager* will assume a GAHP is hung if this many
    seconds pass without a response. The default is 20.

:macro-def:`GRIDMANAGER_MAX_PENDING_REQUESTS[GRIDMANAGER]`
    The maximum number of GAHP commands that can be pending at any time.
    The default is 50.

:macro-def:`GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT[GRIDMANAGER]`
    The number of times to retry a command that failed due to a timeout
    or a failed connection. The default is 3.

:macro-def:`EC2_RESOURCE_TIMEOUT[GRIDMANAGER]`
    The number of seconds after which if an EC2 grid universe job fails
    to ping the EC2 service, the job will be put on hold. Defaults to
    -1, which implements an infinite length, such that a failure to ping
    the service will never put the job on hold.

:macro-def:`EC2_GAHP_RATE_LIMIT[GRIDMANAGER]`
    The minimum interval, in whole milliseconds, between requests to the
    same EC2 service with the same credentials. Defaults to 100.

:macro-def:`BATCH_GAHP_CHECK_STATUS_ATTEMPTS[GRIDMANAGER]`
    The number of times a failed status command issued to the
    *blahpd* should be retried. These retries allow the
    *condor_gridmanager* to tolerate short-lived failures of the
    underlying batch system. The default value is 5.

:macro-def:`C_GAHP_LOG[GRIDMANAGER]`
    The complete path and file name of the HTCondor GAHP server's log.
    The default value is ``/tmp/CGAHPLog.$(USERNAME)``.

:macro-def:`MAX_C_GAHP_LOG[GRIDMANAGER]`
    The maximum size of the :macro:`C_GAHP_LOG`.

:macro-def:`C_GAHP_WORKER_THREAD_LOG[GRIDMANAGER]`
    The complete path and file name of the HTCondor GAHP worker process'
    log. The default value is ``/temp/CGAHPWorkerLog.$(USERNAME)``.

:macro-def:`C_GAHP_CONTACT_SCHEDD_DELAY[GRIDMANAGER]`
    The number of seconds that the *condor_C-gahp* daemon waits between
    consecutive connections to the remote *condor_schedd* in order to
    send batched sets of commands to be executed on that remote
    *condor_schedd* daemon. The default value is 5.

:macro-def:`C_GAHP_MAX_FILE_REQUESTS[GRIDMANAGER]`
    Limits the number of file transfer commands of each type (input,
    output, proxy refresh) that are performed before other (potentially
    higher-priority) commands are read and performed.
    The default value is 10.

:macro-def:`BLAHPD_LOCATION[GRIDMANAGER]`
    The complete path to the directory containing the *blahp* software,
    which is required for grid-type batch jobs.
    The default value is ``$(RELEASE_DIR)``.

:macro-def:`GAHP_SSL_CADIR[GRIDMANAGER]`
    The path to a directory that may contain the certificates (each in
    its own file) for multiple trusted CAs to be used by GAHP servers
    when authenticating with remote services.

:macro-def:`GAHP_SSL_CAFILE[GRIDMANAGER]`
    The path and file name of a file containing one or more trusted CA's
    certificates to be used by GAHP servers when authenticating with
    remote services.

:macro-def:`CONDOR_GAHP[GRIDMANAGER]`
    The complete path and file name of the HTCondor GAHP executable. The
    default value is ``$(SBIN)``/condor_c-gahp.

:macro-def:`EC2_GAHP[GRIDMANAGER]`
    The complete path and file name of the EC2 GAHP executable. The
    default value is ``$(SBIN)``/ec2_gahp.

:macro-def:`BATCH_GAHP[GRIDMANAGER]`
    The complete path and file name of the batch GAHP executable, to be
    used for Slurm, PBS, LSF, SGE, and similar batch systems. The default
    location is ``$(BIN)``/blahpd.

:macro-def:`ARC_GAHP[GRIDMANAGER]`
    The complete path and file name of the ARC GAHP executable.
    The default value is ``$(SBIN)``/arc_gahp.

:macro-def:`ARC_GAHP_COMMAND_LIMIT[GRIDMANAGER]`
    On systems where libcurl uses NSS for security, start a new
    *arc_gahp* process when the existing one has handled the given
    number of commands.
    The default is 1000.

:macro-def:`ARC_GAHP_USE_THREADS[GRIDMANAGER]`
    Controls whether the *arc_gahp* should run multiple HTTPS requests
    in parallel in different threads.
    The default is ``False``.

:macro-def:`GCE_GAHP[GRIDMANAGER]`
    The complete path and file name of the GCE GAHP executable. The
    default value is ``$(SBIN)``/gce_gahp.

:macro-def:`AZURE_GAHP[GRIDMANAGER]`
    The complete path and file name of the Azure GAHP executable. The
    default value is ``$(SBIN)``/AzureGAHPServer.py on Windows and
    ``$(SBIN)``/AzureGAHPServer on other platforms.

:macro-def:`BOINC_GAHP[GRIDMANAGER]`
    The complete path and file name of the BOINC GAHP executable. The
    default value is ``$(SBIN)``/boinc_gahp.

condor_job_router Configuration File Entries
----------------------------------------------

:index:`condor_job_router configuration variables<single: condor_job_router configuration variables; configuration>`

These macros affect the *condor_job_router* daemon.


:macro-def:`JOB_ROUTER_ROUTE_NAMES[JOB ROUTER]`
    An ordered list of the names of enabled routes.  In version 8.9.7 or later,
    routes whose names are listed here should each have a :macro:`JOB_ROUTER_ROUTE_<NAME>`
    configuration variable that specifies the route.

    Routes will be matched to jobs in the order their names are declared in this list.  Routes not
    declared in this list will be disabled.  

    If routes are specified in the deprecated `JOB_ROUTER_ENTRIES`, `JOB_ROUTER_ENTRIES_FILE`
    and `JOB_ROUTER_ENTRIES_CMD` configuration variables, then :macro:`JOB_ROUTER_ROUTE_NAMES` is optional.
    if it is empty, the order in which routes are considered will be the order in
    which their names hash.

:macro-def:`JOB_ROUTER_ROUTE_<NAME>[JOB ROUTER]`
    Specification of a single route in transform syntax.  ``<NAME>`` should be one of the
    route names specified in :macro:`JOB_ROUTER_ROUTE_NAMES`. The transform syntax is specified
    in the :ref:`classads/transforms:ClassAd Transforms` section of this manual.

:macro-def:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES[JOB ROUTER]`
    An ordered list of the names of transforms that should be applied when a job is being
    routed before the route transform is applied.  Each transform name listed here should
    have a corresponding :macro:`JOB_ROUTER_TRANSFORM_<NAME>` configuration variable.

:macro-def:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES[JOB ROUTER]`
    An ordered list of the names of transforms that should be applied when a job is being
    routed after the route transform is applied.  Each transform name listed here should
    have a corresponding :macro:`JOB_ROUTER_TRANSFORM_<NAME>` configuration variable.

:macro-def:`JOB_ROUTER_TRANSFORM_<NAME>[JOB ROUTER]`
    Specification of a single pre-route or post-route transform.  ``<NAME>`` should be one of the
    route names specified in :macro:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES` or in :macro:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES`.
    The transform syntax is specified in the :ref:`classads/transforms:ClassAd Transforms` section of this manual.

:macro-def:`JOB_ROUTER_USE_DEPRECATED_ROUTER_ENTRIES[JOB ROUTER]`
    A boolean value that defaults to ``False``. When ``True``, the
    deprecated parameters :macro:`JOB_ROUTER_DEFAULTS` and
    :macro:`JOB_ROUTER_ENTRIES` can be used to define routes in the
    job routing table.

:macro-def:`JOB_ROUTER_DEFAULTS[JOB ROUTER]`
    .. warning::
        This macro is deprecated and will be removed for V24 of HTCondor.
        The actual removal of this configuration macro will occur during the
        lifetime of the HTCondor V23 feature series.

    Deprecated, use :macro:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES` instead.
    Defined by a single ClassAd in New ClassAd syntax, used to provide
    default values for routes in the *condor_job_router* daemon's
    routing table that are specified by the also deprecated ``JOB_ROUTER_ENTRIES*``.
    The enclosing square brackets are optional.

:macro-def:`JOB_ROUTER_ENTRIES[JOB ROUTER]`
    .. warning::
        This macro is deprecated and will be removed for V24 of HTCondor.
        The actual removal of this configuration macro will occur during the
        lifetime of the HTCondor V23 feature series.

    Deprecated, use :macro:`JOB_ROUTER_ROUTE_<NAME>` instead.
    Specification of the job routing table. It is a list of ClassAds, in
    New ClassAd syntax, where each individual ClassAd is surrounded by
    square brackets, and the ClassAds are separated from each other by
    spaces. Each ClassAd describes one entry in the routing table, and
    each describes a site that jobs may be routed to.

    A :tool:`condor_reconfig` command causes the *condor_job_router* daemon
    to rebuild the routing table. Routes are distinguished by a routing
    table entry's ClassAd attribute ``Name``. Therefore, a ``Name``
    change in an existing route has the potential to cause the
    inaccurate reporting of routes.

    Instead of setting job routes using this configuration variable,
    they may be read from an external source using the
    :macro:`JOB_ROUTER_ENTRIES_FILE` or be dynamically generated by an
    external program via the :macro:`JOB_ROUTER_ENTRIES_CMD` configuration
    variable.

    Routes specified by any of these 3 configuration variables are merged
    with the :macro:`JOB_ROUTER_DEFAULTS` before being used.

:macro-def:`JOB_ROUTER_ENTRIES_FILE[JOB ROUTER]`
    .. warning::
        This macro is deprecated and will be removed for V24 of HTCondor.
        The actual removal of this configuration macro will occur during the
        lifetime of the HTCondor V23 feature series.

    Deprecated, use :macro:`JOB_ROUTER_ROUTE_<NAME>` instead.
    A path and file name of a file that contains the ClassAds, in New
    ClassAd syntax, describing the routing table. The specified file is
    periodically reread to check for new information. This occurs every
    ``$(JOB_ROUTER_ENTRIES_REFRESH)`` seconds.

:macro-def:`JOB_ROUTER_ENTRIES_CMD[JOB ROUTER]`
    .. warning::
        This macro is deprecated and will be removed for V24 of HTCondor.
        The actual removal of this configuration macro will occur during the
        lifetime of the HTCondor V23 feature series.

    Deprecated, use :macro:`JOB_ROUTER_ROUTE_<NAME>` instead.
    Specifies the command line of an external program to run. The output
    of the program defines or updates the routing table, and the output
    must be given in New ClassAd syntax. The specified command is
    periodically rerun to regenerate or update the routing table. This
    occurs every ``$(JOB_ROUTER_ENTRIES_REFRESH)`` seconds. Specify the
    full path and file name of the executable within this command line,
    as no assumptions may be made about the current working directory
    upon command invocation. To enter spaces in any command-line
    arguments or in the command name itself, surround the right hand
    side of this definition with double quotes, and use single quotes
    around individual arguments that contain spaces. This is the same as
    when dealing with spaces within job arguments in an HTCondor submit
    description file.

:macro-def:`JOB_ROUTER_ENTRIES_REFRESH[JOB ROUTER]`
    The number of seconds between updates to the routing table described
    by :macro:`JOB_ROUTER_ENTRIES_FILE` or :macro:`JOB_ROUTER_ENTRIES_CMD`. The
    default value is 0, meaning no periodic updates occur. With the
    default value of 0, the routing table can be modified when a
    :tool:`condor_reconfig` command is invoked or when the
    *condor_job_router* daemon restarts.

:macro-def:`JOB_ROUTER_LOCK[JOB ROUTER]`
    This specifies the name of a lock file that is used to ensure that
    multiple instances of condor_job_router never run with the same
    :macro:`JOB_ROUTER_NAME`. Multiple instances running with the same name
    could lead to mismanagement of routed jobs. The default value is
    $(LOCK)/$(JOB_ROUTER_NAME)Lock.

:macro-def:`JOB_ROUTER_SOURCE_JOB_CONSTRAINT[JOB ROUTER]`
    Specifies a global ``Requirements`` expression that must be true for
    all newly routed jobs, in addition to any ``Requirements`` specified
    within a routing table entry. In addition to the configurable
    constraints, the *condor_job_router* also has some hard-coded
    constraints. It avoids recursively routing jobs by requiring that
    the job's attribute ``RoutedBy`` does not match :macro:`JOB_ROUTER_NAME`.
    When not running as root, it also avoids routing jobs belonging to other users.

:macro-def:`JOB_ROUTER_MAX_JOBS[JOB ROUTER]`
    An integer value representing the maximum number of jobs that may be
    routed, summed over all routes. The default value is -1, which means
    an unlimited number of jobs may be routed.

:macro-def:`JOB_ROUTER_DEFAULT_MAX_JOBS_PER_ROUTE[JOB ROUTER]`
    An integer value representing the maximum number of jobs that may be
    routed to a single route when the route does not specify a ``MaxJobs``
    value. The default value is 100.

:macro-def:`JOB_ROUTER_DEFAULT_MAX_IDLE_JOBS_PER_ROUTE[JOB ROUTER]`
    An integer value representing the maximum number of jobs in a single
    route that may be in the idle state.  When the number of jobs routed
    to that site exceeds this number, no more jobs will be routed to it. 
    A route may specify ``MaxIdleJobs`` to override this number.
    The default value is 50.

:macro-def:`MAX_JOB_MIRROR_UPDATE_LAG[JOB ROUTER]`
    An integer value that administrators will rarely consider changing,
    representing the maximum number of seconds the *condor_job_router*
    daemon waits, before it decides that routed copies have gone awry,
    due to the failure of events to appear in the *condor_schedd* 's
    job queue log file. The default value is 600. As the
    *condor_job_router* daemon uses the *condor_schedd* 's job queue
    log file entries for synchronization of routed copies, when an
    expected log file event fails to appear after this wait period, the
    *condor_job_router* daemon acts presuming the expected event will
    never occur.

:macro-def:`JOB_ROUTER_POLLING_PERIOD[JOB ROUTER]`
    An integer value representing the number of seconds between cycles
    in the *condor_job_router* daemon's task loop. The default is 10
    seconds. A small value makes the *condor_job_router* daemon quick
    to see new candidate jobs for routing. A large value makes the
    *condor_job_router* daemon generate less overhead at the cost of
    being slower to see new candidates for routing. For very large job
    queues where a few minutes of routing latency is no problem,
    increasing this value to a few hundred seconds would be reasonable.

:macro-def:`JOB_ROUTER_NAME[JOB ROUTER]`
    A unique identifier utilized to name multiple instances of the
    *condor_job_router* daemon on the same machine. Each instance must
    have a different name, or all but the first to start up will refuse
    to run. The default is ``"jobrouter"``.

    Changing this value when routed jobs already exist is not currently
    gracefully handled. However, it can be done if one also uses
    :tool:`condor_qedit` to change the value of ``ManagedManager`` and
    ``RoutedBy`` from the old name to the new name. The following
    commands may be helpful:

    .. code-block:: console

        $ condor_qedit -constraint \
        'RoutedToJobId =!= undefined && ManagedManager == "insert_old_name"' \
          ManagedManager '"insert_new_name"'
        $ condor_qedit -constraint \
        'RoutedBy == "insert_old_name"' RoutedBy '"insert_new_name"'

:macro-def:`JOB_ROUTER_RELEASE_ON_HOLD[JOB ROUTER]`
    A boolean value that defaults to ``True``. It controls how the
    *condor_job_router* handles the routed copy when it goes on hold.
    When ``True``, the *condor_job_router* leaves the original job
    ClassAd in the same state as when claimed. When ``False``, the
    *condor_job_router* does not attempt to reset the original job
    ClassAd to a pre-claimed state upon yielding control of the job.

:macro-def:`JOB_ROUTER_SCHEDD1_SPOOL[JOB ROUTER]`
    DEPRECATED.  Please use
    :macro:`JOB_ROUTER_SCHEDD1_JOB_QUEUE_LOG` instead.
    The path to the spool directory for the *condor_schedd* serving as
    the source of jobs for routing. If not specified, this defaults to
    ``$(SPOOL)``. If specified, this parameter must point to the spool
    directory of the *condor_schedd* identified by
    :macro:`JOB_ROUTER_SCHEDD1_NAME`.

:macro-def:`JOB_ROUTER_SCHEDD1_JOB_QUEUE_LOG[JOB ROUTER]`
    The path to the job_queue.log file for the *condor_schedd*
    serving as the source of jobs for routing.  If specified,
    this must point to the job_queue.log file of the *condor_schedd*
    identified by :macro:`JOB_ROUTER_SCHEDD1_NAME`.

:macro-def:`JOB_ROUTER_SCHEDD2_SPOOL[JOB ROUTER]`
    DEPRECATED.  Please use
    :macro:`JOB_ROUTER_SCHEDD2_JOB_QUEUE_LOG` instead.
    The path to the spool directory for the *condor_schedd* to which
    the routed copy of the jobs are submitted. If not specified, this
    defaults to ``$(SPOOL)``. If specified, this parameter must point to
    the spool directory of the *condor_schedd* identified by
    :macro:`JOB_ROUTER_SCHEDD2_NAME`. Note that when *condor_job_router* is
    running as root and is submitting routed jobs to a different
    *condor_schedd* than the source *condor_schedd*, it is required
    that *condor_job_router* have permission to impersonate the job
    owners of the routed jobs. It is therefore usually necessary to
    configure :macro:`QUEUE_SUPER_USER_MAY_IMPERSONATE` in the configuration
    of the target *condor_schedd*.

:macro-def:`JOB_ROUTER_SCHEDD2_JOB_QUEUE_LOG[JOB ROUTER]`
    The path to the job_queue.log file for the *condor_schedd*
    serving as the destination of jobs for routing.  If specified,
    this must point to the job_queue.log file of the *condor_schedd*
    identified by :macro:`JOB_ROUTER_SCHEDD2_NAME`.

:macro-def:`JOB_ROUTER_SCHEDD1_NAME[JOB ROUTER]`
    The advertised daemon name of the *condor_schedd* serving as the
    source of jobs for routing. If not specified, this defaults to the
    local *condor_schedd*. If specified, this parameter must name the
    same *condor_schedd* whose spool is configured in
    :macro:`JOB_ROUTER_SCHEDD1_SPOOL`. If the named *condor_schedd* is not
    advertised in the local pool, :macro:`JOB_ROUTER_SCHEDD1_POOL` will
    also need to be set.

:macro-def:`JOB_ROUTER_SCHEDD2_NAME[JOB ROUTER]`
    The advertised daemon name of the *condor_schedd* to which the
    routed copy of the jobs are submitted. If not specified, this
    defaults to the local *condor_schedd*. If specified, this parameter
    must name the same *condor_schedd* whose spool is configured in
    :macro:`JOB_ROUTER_SCHEDD2_SPOOL`. If the named *condor_schedd* is not
    advertised in the local pool, :macro:`JOB_ROUTER_SCHEDD2_POOL` will
    also need to be set. Note that when *condor_job_router* is running
    as root and is submitting routed jobs to a different *condor_schedd*
    than the source *condor_schedd*, it is required that *condor_job_router*
    have permission to impersonate the job owners of the routed jobs. It
    is therefore usually necessary to configure
    :macro:`QUEUE_SUPER_USER_MAY_IMPERSONATE` in the configuration
    of the target *condor_schedd*.

:macro-def:`JOB_ROUTER_SCHEDD1_POOL[JOB ROUTER]`
    The Condor pool (*condor_collector* address) of the
    *condor_schedd* serving as the source of jobs for routing. If not
    specified, defaults to the local pool.

:macro-def:`JOB_ROUTER_SCHEDD2_POOL[JOB ROUTER]`
    The Condor pool (*condor_collector* address) of the
    *condor_schedd* to which the routed copy of the jobs are submitted.
    If not specified, defaults to the local pool.

:macro-def:`JOB_ROUTER_ROUND_ROBIN_SELECTION[JOB ROUTER]`
    A boolean value that controls which route is chosen for a candidate
    job that matches multiple routes. When set to ``False``, the
    default, the first matching route is always selected. When set to
    ``True``, the Job Router attempts to distribute jobs across all
    matching routes, round robin style.

:macro-def:`JOB_ROUTER_CREATE_IDTOKEN_NAMES[JOB ROUTER]`
    An list of the names of IDTOKENs that the JobRouter should create and refresh.
    IDTOKENS whose names are listed here should each have a :macro:`JOB_ROUTER_CREATE_IDTOKEN_<NAME>`
    configuration variable that specifies the filename, ownership and properties of the IDTOKEN.

:macro-def:`JOB_ROUTER_IDTOKEN_REFRESH[JOB ROUTER]`
    An integer value of seconds that controls the rate at which the JobRouter will refresh
    the IDTOKENS listed by the :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES` configuration variable.

:macro-def:`JOB_ROUTER_CREATE_IDTOKEN_<NAME>[JOB ROUTER]`
    Specification of a single IDTOKEN that will be created an refreshed by the JobRouter.
    ``<NAME>`` should be one of the IDTOKEN names specified in :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES`.
    The filename, ownership and properties of the IDTOKEN are defined by the following attributes.
    Each attribute value must be a classad expression that evaluates to a string, except ``lifetime``
    which must evaluate to an integer.

    :macro-def:`kid[JOB ROUTER]`
         The ID of the token signing key to use, equivalent to the ``-key`` argument of :tool:`condor_token_create`
         and the ``kid`` attribute of :tool:`condor_token_list`.  Defaults to "POOL"

    :macro-def:`sub[JOB ROUTER]`
         The subject or user identity, equivalent to the ``-identity`` argument of :tool:`condor_token_create`
         and the ``sub`` attribute of :tool:`condor_token_list`. Defaults the token name.

    :macro-def:`scope[JOB ROUTER]`
         List of allowed authorizations, equivalent to the ``-authz`` argument of :tool:`condor_token_create`
         and the ``scope`` attribute of :tool:`condor_token_list`. 

    :macro-def:`lifetime[JOB ROUTER]`
         Time in seconds that the IDTOKEN is valid after creation, equivalent to the ``-lifetime`` argument of :tool:`condor_token_create`.
         The ``exp`` attribute of :tool:`condor_token_list` is the creation time of the token plus this value.

    :macro-def:`file[JOB ROUTER]`
         The filename of the IDTOKEN file, equivalent to the ``-token`` argument of :tool:`condor_token_create`.
         Defaults to the token name.

    :macro-def:`dir[JOB ROUTER]`
         The directory that the IDTOKEN file will be created and refreshed into. Defaults to ``$(SEC_TOKEN_DIRECTORY)``.

    :macro-def:`owner[JOB ROUTER]`
         If specified, the IDTOKEN file will be owned by this user.  If not specified, the IDTOKEN file will be owned
         by the owner of *condor_job_router* process.  This attribute is optional if the *condor_job_router* is running as an ordinary user
         but required if it is running as a Windows service or as the ``root`` or ``condor`` user.  The owner specified here
         should be the same as the :ad-attr:`Owner` attribute of the jobs that this IDTOKEN is intended to be sent to.

:macro-def:`JOB_ROUTER_SEND_ROUTE_IDTOKENS[JOB ROUTER]`
    List of the names of the IDTOKENS to add to the input file transfer list of each routed job. This list should be one or
    more of the IDTOKEN names specified by the :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES`.
    If the route has a ``SendIDTokens`` definition, this configuration variable is not used for that route.

condor_lease_manager Configuration File Entries
-------------------------------------------------

:index:`condor_lease_manager configuration variables<single: condor_lease_manager configuration variables; configuration>`

These macros affect the *condor_lease_manager*.

The *condor_lease_manager* expects to use the syntax

.. code-block:: text

     <subsystem name>.<parameter name>

in configuration. This allows multiple instances of the
*condor_lease_manager* to be easily configured using the syntax

.. code-block:: text

     <subsystem name>.<local name>.<parameter name>

:macro-def:`LeaseManager.GETADS_INTERVAL[LEASE MANAGER]`
    An integer value, given in seconds, that controls the frequency with
    which the *condor_lease_manager* pulls relevant resource ClassAds
    from the *condor_collector*. The default value is 60 seconds, with
    a minimum value of 2 seconds.

:macro-def:`LeaseManager.UPDATE_INTERVAL[LEASE MANAGER]`
    An integer value, given in seconds, that controls the frequency with
    which the *condor_lease_manager* sends its ClassAds to the
    *condor_collector*. The default value is 60 seconds, with a minimum
    value of 5 seconds.

:macro-def:`LeaseManager.PRUNE_INTERVAL[LEASE MANAGER]`
    An integer value, given in seconds, that controls the frequency with
    which the *condor_lease_manager* prunes its leases. This involves
    checking all leases to see if they have expired. The default value
    is 60 seconds, with no minimum value.

:macro-def:`LeaseManager.DEBUG_ADS[LEASE MANAGER]`
    A boolean value that defaults to ``False``. When ``True``, it
    enables extra debugging information about the resource ClassAds that
    it retrieves from the *condor_collector* and about the search
    ClassAds that it sends to the *condor_collector*.

:macro-def:`LeaseManager.MAX_LEASE_DURATION[LEASE MANAGER]`
    An integer value representing seconds which determines the maximum
    duration of a lease. This can be used to provide a hard limit on
    lease durations. Normally, the *condor_lease_manager* honors the
    ``MaxLeaseDuration`` attribute from the resource ClassAd. If this
    configuration variable is defined, it limits the effective maximum
    duration for all resources to this value. The default value is 1800
    seconds.

    Note that leases can be renewed, and thus can be extended beyond
    this limit. To provide a limit on the total duration of a lease, use
    ``LeaseManager.MAX_TOTAL_LEASE_DURATION``.

:macro-def:`LeaseManager.MAX_TOTAL_LEASE_DURATION[LEASE MANAGER]`
    An integer value representing seconds used to limit the total
    duration of leases, over all its renewals. The default value is 3600
    seconds.

:macro-def:`LeaseManager.DEFAULT_MAX_LEASE_DURATION[LEASE MANAGER]`
    The *condor_lease_manager* uses the ``MaxLeaseDuration`` attribute
    from the resource ClassAd to limit the lease duration. If this
    attribute is not present in a resource ClassAd, then this
    configuration variable is used instead. This integer value is given
    in units of seconds, with a default value of 60 seconds.

:macro-def:`LeaseManager.CLASSAD_LOG[LEASE MANAGER]`
    This variable defines a full path and file name to the location
    where the *condor_lease_manager* keeps persistent state
    information. This variable has no default value.

:macro-def:`LeaseManager.QUERY_ADTYPE[LEASE MANAGER]`
    This parameter controls the type of the query in the ClassAd sent to
    the *condor_collector*, which will control the types of ClassAds
    returned by the *condor_collector*. This parameter must be a valid
    ClassAd type name, with a default value of ``"Any"``.

:macro-def:`LeaseManager.QUERY_CONSTRAINTS[LEASE MANAGER]`
    A ClassAd expression that controls the constraint in the query sent
    to the *condor_collector*. It is used to further constrain the
    types of ClassAds from the *condor_collector*. There is no default
    value, resulting in no constraints being placed on query.

.. _DAGMan Configuration:

DAGMan Configuration File Entries
---------------------------------

These macros affect the operation of DAGMan and DAGMan jobs within
HTCondor.

.. note::

    Many of these configuration variables are appropriate to set on a
    per DAG basis. For more information see :ref:`Per DAG Config`.

.. warning::

    Configuration settings do not get applied to running DAGMan workflows
    when executing :tool:`condor_reconfig`.

:index:`General<single: DAGMan Configuration Sections; General>`

General
'''''''

:macro-def:`DAGMAN_CONFIG_FILE[DAGMan]`
    The path to the configuration file to be used by :tool:`condor_dagman`.
    This option is set by :tool:`condor_submit_dag` automatically and should not be
    set explicitly by the user. Defaults to an empty string.

:macro-def:`DAGMAN_USE_STRICT[DAGMan]`
    An integer defining the level of strictness :tool:`condor_dagman` will
    apply when turning warnings into fatal errors, as follows:

    -  0: no warnings become errors
    -  1: severe warnings become errors
    -  2: medium-severity warnings become errors
    -  3: almost all warnings become errors

    Using a strictness value greater than 0 may help find problems with
    a DAG that may otherwise escape notice. The default value if not
    defined is 1.

:macro-def:`DAGMAN_STARTUP_CYCLE_DETECT[DAGMan]`
    A boolean value that defaults to ``False``. When ``True``, causes
    :tool:`condor_dagman` to check for cycles in the DAG before submitting
    DAG node jobs, in addition to its run time cycle detection.

.. note::

    The startup process for DAGMan is much slower for large DAGs when set
    to ``True``.

:macro-def:`DAGMAN_ABORT_DUPLICATES[DAGMan]`
    A boolean that defaults to ``True``. When ``True``, upon startup DAGMan
    will check to see if a previous DAGMan process for a specific DAG is
    still running and prevent the new DAG instance from executing. This
    check is done by checking for a DAGMan lock file and verifying whether
    or not the recorded PID's associated process is still running.

.. note::

    This value should rarely be changed, especially by users, since have
    multiple DAGMan processes executing the same DAG in the same directory
    can lead to strange behavior and issues.

:macro-def:`DAGMAN_USE_SHARED_PORT[DAGMan]`
    A boolean that defaults to ``False``. When ``True``, :tool:`condor_dagman`
    will attempt to connect to the shared port daemon.

.. note::

    This value should never be changed since it was added to prevent spurious
    shared port related error messages from appearing the DAGMan debug log.

:macro-def:`DAGMAN_USE_DIRECT_SUBMIT[DAGMan]`
    A boolean value that defaults to ``True``. When ``True``, :tool:`condor_dagman`
    will open a direct connection to the local *condor_schedd* to submit jobs rather
    than spawning the :tool:`condor_submit` process.

:macro-def:`DAGMAN_PRODUCE_JOB_CREDENTIALS[DAGMan]`
    A boolean value that defaults to ``True``. When ``True``, :tool:`condor_dagman`
    will attempt to produce needed credentials for jobs at submit time when using
    direct submission.

:macro-def:`DAGMAN_USE_JOIN_NODES[DAGMan]`
    A boolean value that defaults to ``True``. When ``True``, :tool:`condor_dagman`
    will create special *join nodes* for :dag-cmd:`PARENT/CHILD` relationships between
    multiple parent nodes and multiple child nodes.

.. note::

    This should never be changed since it reduces the number of dependencies in the
    graph resulting in a significant improvement to the memory footprint, parse time,
    and submit speed of :tool:`condor_dagman` (Especially for large DAGs).

:macro-def:`DAGMAN_PUT_FAILED_JOBS_ON_HOLD[DAGMan]`
    A boolean value that defaults to ``False``. When ``True``, DAGMan will automatically
    retry a node with its job submitted on hold if any of the nodes jobs fail. Script
    failures do not cause this behavior. The job is only put on hold if the node has no
    more declared :dag-cmd:`RETRY` attempts.

:macro-def:`DAGMAN_DEFAULT_APPEND_VARS[DAGMan]`
    A boolean value that defaults to ``False``. When ``True``, variables
    parsed in the DAG file :dag-cmd:`VARS` line will be appended to the given Job
    submit description file unless :dag-cmd:`VARS` specifies *PREPEND* or *APPEND*.
    When ``False``, the parsed variables will be prepended unless specified.

:macro-def:`DAGMAN_MANAGER_JOB_APPEND_GETENV[DAGMan]`
    A comma separated list of variable names to add to the DAGMan ``*.condor.sub``
    file's :subcom:`getenv` option. This will in turn add any found matching environment
    variables to the DAGMan proper jobs :ad-attr:`Environment`. Setting this value to
    ``True`` will result in ``getenv = true``. The Base ``*.condor.sub`` values for
    :subcom:`getenv` are the following:

    +---------------+--------------------+--------------------+--------------------+
    |               |        PATH        |        HOME        |        USER        |
    |               +--------------------+--------------------+--------------------+
    | General Shell |         TZ         |        LANG        |       LC_ALL       |
    |               +--------------------+--------------------+--------------------+
    |               |     PYTHONPATH     |        PERL*       |                    |
    +---------------+--------------------+--------------------+--------------------+
    |    HTCondor   |   CONDOR_CONFIG    |       CONDOR_*     |                    |
    +---------------+--------------------+--------------------+--------------------+
    |    Scitoken   |    BEARER_TOKEN    | BEAERER_TOKEN_FILE |   XDG_RUNTIME_DIR  |
    +---------------+--------------------+--------------------+--------------------+
    |     Misc.     |     PEGASUS_*      |                    |                    |
    +---------------+--------------------+--------------------+--------------------+

:macro-def:`DAGMAN_NODE_RECORD_INFO[DAGMan]`
    A string that when set to ``RETRY`` will cause DAGMan to insert a nodes current
    retry attempt number into the nodes job ad as the attribute :ad-attr:`DAGManNodeRetry`
    at submission time. This knob is not set by default.

:macro-def:`DAGMAN_RECORD_MACHINE_ATTRS[DAGMan]`
    A comma separated list of machine attributes that DAGMan will insert into a node jobs
    submit description for :subcom:`job_ad_information_attrs` and :subcom:`job_machine_attrs`.
    This will result in the listed machine attributes to be injected into the nodes produced
    job ads and userlog. This knob is not set by default.

:macro-def:`DAGMAN_REPORT_GRAPH_METRICS`
    A boolean that defaults to ``False``. When ``True``, DAGMan will write additional
    information regarding graph metrics to ``*.metrics`` file. The included graph metrics
    are as follows:

    - Graph Height
    - Graph Width
    - Number of edges (dependencies)
    - Number of vertices (nodes)


:index:`Throttling<single: DAGMan Configuration Sections; Throttling>`

Throttling
''''''''''

:macro-def:`DAGMAN_MAX_JOBS_IDLE[DAGMan]`
    An integer value that controls the maximum number of idle procs
    allowed within the DAG before :tool:`condor_dagman` temporarily stops
    submitting jobs. :tool:`condor_dagman` will resume submitting jobs once
    the number of idle procs falls below the specified limit.
    :macro:`DAGMAN_MAX_JOBS_IDLE` currently counts each individual proc
    within a cluster as a job, which is inconsistent with
    :macro:`DAGMAN_MAX_JOBS_SUBMITTED`. Note that submit description files
    that queue multiple procs can cause the :macro:`DAGMAN_MAX_JOBS_IDLE`
    limit to be exceeded. If a submit description file contains
    ``queue 5000`` and :macro:`DAGMAN_MAX_JOBS_IDLE` is set to 250, this will
    result in 5000 procs being submitted to the *condor_schedd*, not
    250; in this case, no further jobs will then be submitted by
    :tool:`condor_dagman` until the number of idle procs falls below 250. The
    default value is 1000. To disable this limit, set the value to 0.
    This configuration option can be overridden by the
    :tool:`condor_submit_dag` **-maxidle** command-line argument (see
    :doc:`/man-pages/condor_submit_dag`).

:macro-def:`DAGMAN_MAX_JOBS_SUBMITTED[DAGMan]`
    An integer value that controls the maximum number of node jobs
    (clusters) within the DAG that will be submitted to HTCondor at one
    time. A single invocation of :tool:`condor_submit` by :tool:`condor_dagman`
    counts as one job, even if the submit file produces a multi-proc
    cluster. The default value is 0 (unlimited). This configuration
    option can be overridden by the :tool:`condor_submit_dag`
    **-maxjobs** command-line argument (see :doc:`/man-pages/condor_submit_dag`).

:macro-def:`DAGMAN_MAX_PRE_SCRIPTS[DAGMan]`
    An integer defining the maximum number of PRE scripts that any given
    :tool:`condor_dagman` will run at the same time. The value 0 allows any
    number of PRE scripts to run. The default value if not defined is
    20. Note that the :macro:`DAGMAN_MAX_PRE_SCRIPTS` value can be overridden
    by the :tool:`condor_submit_dag` **-maxpre** command line option.

:macro-def:`DAGMAN_MAX_POST_SCRIPTS[DAGMan]`
    An integer defining the maximum number of POST scripts that any
    given :tool:`condor_dagman` will run at the same time. The value 0 allows
    any number of POST scripts to run. The default value if not defined
    is 20. Note that the :macro:`DAGMAN_MAX_POST_SCRIPTS` value can be
    overridden by the :tool:`condor_submit_dag` **-maxpost** command line
    option.

:macro-def:`DAGMAN_MAX_HOLD_SCRIPTS[DAGMan]`
    An integer defining the maximum number of HOLD scripts that any
    given :tool:`condor_dagman` will run at the same time. The default value
    0 allows any number of HOLD scripts to run.

:macro-def:`DAGMAN_REMOVE_JOBS_AFTER_LIMIT_CHANGE[DAGMan]`
    A boolean that determines if after changing some of these throttle limits,
    :tool:`condor_dagman` should forceably remove jobs to meet the new limit.
    Defaults to ``False``.

:index:`Node Semantics<single: DAGMan Configuration Sections; Node Semantics>`

Priority, node semantics
''''''''''''''''''''''''

:macro-def:`DAGMAN_DEFAULT_PRIORITY[DAGMan]`
    An integer value defining the minimum priority of node jobs running
    under this :tool:`condor_dagman` job. Defaults to 0.

:macro-def:`DAGMAN_SUBMIT_DEPTH_FIRST[DAGMan]`
    A boolean value that controls whether to submit ready DAG node jobs
    in (more-or-less) depth first order, as opposed to breadth-first
    order. Setting :macro:`DAGMAN_SUBMIT_DEPTH_FIRST` to ``True`` does not
    override dependencies defined in the DAG. Rather, it causes newly
    ready nodes to be added to the head, rather than the tail, of the
    ready node list. If there are no PRE scripts in the DAG, this will
    cause the ready nodes to be submitted depth-first. If there are PRE
    scripts, the order will not be strictly depth-first, but it will
    tend to favor depth rather than breadth in executing the DAG. If
    :macro:`DAGMAN_SUBMIT_DEPTH_FIRST` is set to ``True``, consider also
    setting :macro:`DAGMAN_RETRY_SUBMIT_FIRST` and
    :macro:`DAGMAN_RETRY_NODE_FIRST` to ``True``. If not defined,
    :macro:`DAGMAN_SUBMIT_DEPTH_FIRST` defaults to ``False``.

:macro-def:`DAGMAN_ALWAYS_RUN_POST[DAGMan]`
    A boolean value defining whether :tool:`condor_dagman` will ignore the
    return value of a PRE script when deciding whether to run a POST
    script. The default is ``False``, which means that the failure of a
    PRE script causes the POST script to not be executed. Changing this
    to ``True`` will restore the previous behavior of :tool:`condor_dagman`,
    which is that a POST script is always executed, even if the PRE
    script fails.

:index:`Job Management<single: DAGMan Configuration Sections; Job Management>`

Node job submission/removal
'''''''''''''''''''''''''''

:macro-def:`DAGMAN_USER_LOG_SCAN_INTERVAL[NEGOTIATOR]`
    An integer value representing the number of seconds that
    :tool:`condor_dagman` waits between checking the workflow log file for
    status updates. Setting this value lower than the default increases
    the CPU time :tool:`condor_dagman` spends checking files, perhaps
    fruitlessly, but increases responsiveness to nodes completing or
    failing. The legal range of values is 1 to INT_MAX. If not defined,
    it defaults to 5 seconds. This default may be automatically decreased
    if :macro:`DAGMAN_MAX_JOBS_IDLE` is set to a small value. If so,
    this will be noted in the ``dagman.out`` file.

:macro-def:`DAGMAN_MAX_SUBMITS_PER_INTERVAL[DAGMan]`
    An integer that controls how many individual jobs :tool:`condor_dagman`
    will submit in a row before servicing other requests (such as a
    :tool:`condor_rm`). The legal range of values is 1 to 1000. If defined
    with a value less than 1, the value 1 will be used. If defined with
    a value greater than 1000, the value 1000 will be used. If not
    defined, it defaults to 100. This default may be automatically
    decreased if :macro:`DAGMAN_MAX_JOBS_IDLE` is set to a small value. If so,
    this will be noted in the ``dagman.out`` file.

    **Note: The maximum rate at which DAGMan can submit jobs is
    DAGMAN_MAX_SUBMITS_PER_INTERVAL / DAGMAN_USER_LOG_SCAN_INTERVAL.**

:macro-def:`DAGMAN_MAX_SUBMIT_ATTEMPTS[DAGMan]`
    An integer that controls how many times in a row :tool:`condor_dagman`
    will attempt to execute :tool:`condor_submit` for a given job before
    giving up. Note that consecutive attempts use an exponential
    backoff, starting with 1 second. The legal range of values is 1 to
    16. If defined with a value less than 1, the value 1 will be used.
    If defined with a value greater than 16, the value 16 will be used.
    Note that a value of 16 would result in :tool:`condor_dagman` trying for
    approximately 36 hours before giving up. If not defined, it defaults
    to 6 (approximately two minutes before giving up).

:macro-def:`DAGMAN_MAX_JOB_HOLDS[DAGMan]`
    An integer value defining the maximum number of times a node job is
    allowed to go on hold. As a job goes on hold this number of times,
    it is removed from the queue. For example, if the value is 2, as the
    job goes on hold for the second time, it will be removed. At this
    time, this feature is not fully compatible with node jobs that have
    more than one ``ProcID``. The number of holds of each process in the
    cluster count towards the total, rather than counting individually.
    So, this setting should take that possibility into account, possibly
    using a larger value. A value of 0 allows a job to go on hold any
    number of times. The default value if not defined is 100.

:macro-def:`DAGMAN_HOLD_CLAIM_TIME[DAGMan]`
    An integer defining the number of seconds that :tool:`condor_dagman` will
    cause a hold on a claim after a job is finished, using the job
    ClassAd attribute :ad-attr:`KeepClaimIdle`. The default value is 20. A
    value of 0 causes :tool:`condor_dagman` not to set the job ClassAd
    attribute.

:macro-def:`DAGMAN_SUBMIT_DELAY[DAGMan]`
    An integer that controls the number of seconds that :tool:`condor_dagman`
    will sleep before submitting consecutive jobs. It can be increased
    to help reduce the load on the *condor_schedd* daemon. The legal
    range of values is any non negative integer. If defined with a value
    less than 0, the value 0 will be used.

:macro-def:`DAGMAN_PROHIBIT_MULTI_JOBS[DAGMan]`
    A boolean value that controls whether :tool:`condor_dagman` prohibits
    node job submit description files that queue multiple job procs
    other than parallel universe. If a DAG references such a submit
    file, the DAG will abort during the initialization process. If not
    defined, :macro:`DAGMAN_PROHIBIT_MULTI_JOBS` defaults to ``False``.

:macro-def:`DAGMAN_GENERATE_SUBDAG_SUBMITS[DAGMan]`
    A boolean value specifying whether :tool:`condor_dagman` itself should
    create the ``.condor.sub`` files for nested DAGs. If set to
    ``False``, nested DAGs will fail unless the ``.condor.sub`` files
    are generated manually by running :tool:`condor_submit_dag`
    *-no_submit* on each nested DAG, or the *-do_recurse* flag is
    passed to :tool:`condor_submit_dag` for the top-level DAG. DAG nodes
    specified with the ``SUBDAG EXTERNAL`` keyword or with submit
    description file names ending in ``.condor.sub`` are considered
    nested DAGs. The default value if not defined is ``True``.

:macro-def:`DAGMAN_REMOVE_NODE_JOBS[DAGMan]`
    A boolean value that controls whether :tool:`condor_dagman` removes its
    node jobs itself when it is removed (in addition to the
    *condor_schedd* removing them). Note that setting
    :macro:`DAGMAN_REMOVE_NODE_JOBS` to ``True`` is the safer option (setting
    it to ``False`` means that there is some chance of ending up with
    "orphan" node jobs). Setting :macro:`DAGMAN_REMOVE_NODE_JOBS` to
    ``False`` is a performance optimization (decreasing the load on the
    *condor_schedd* when a :tool:`condor_dagman` job is removed). Note that
    even if :macro:`DAGMAN_REMOVE_NODE_JOBS` is set to ``False``,
    :tool:`condor_dagman` will remove its node jobs in some cases, such as a
    DAG abort triggered by an *ABORT-DAG-ON* command. Defaults to
    ``True``.

:macro-def:`DAGMAN_MUNGE_NODE_NAMES[DAGMan]`
    A boolean value that controls whether :tool:`condor_dagman` automatically
    renames nodes when running multiple DAGs. The renaming is done to
    avoid possible name conflicts. If this value is set to ``True``, all
    node names have the DAG number followed by the period character (.)
    prepended to them. For example, the first DAG specified on the
    :tool:`condor_submit_dag` command line is considered DAG number 0, the
    second is DAG number 1, etc. So if DAG number 2 has a node named B,
    that node will internally be renamed to 2.B. If not defined,
    :macro:`DAGMAN_MUNGE_NODE_NAMES` defaults to ``True``. **Note: users
    should rarely change this setting.**

:macro-def:`DAGMAN_SUPPRESS_JOB_LOGS[DAGMan]`
    A boolean value specifying whether events should be written to a log
    file specified in a node job's submit description file. The default
    value is ``False``, such that events are written to a log file
    specified by a node job.

:macro-def:`DAGMAN_SUPPRESS_NOTIFICATION[DAGMan]`
    A boolean value that controls whether jobs submitted by :tool:`condor_dagman`
    can send email notifications. If **True** then no submitted jobs will
    send email notifications. This is equivalent to setting :subcom:`notification`
    to ``Never``. Defaults to **False**.

:macro-def:`DAGMAN_CONDOR_SUBMIT_EXE[DAGMan]`
    The executable that :tool:`condor_dagman` will use to submit HTCondor
    jobs. If not defined, :tool:`condor_dagman` looks for :tool:`condor_submit` in
    the path. **Note: users should rarely change this setting.**

:macro-def:`DAGMAN_CONDOR_RM_EXE[DAGMan]`
    The executable that :tool:`condor_dagman` will use to remove HTCondor
    jobs. If not defined, :tool:`condor_dagman` looks for :tool:`condor_rm` in the
    path. **Note: users should rarely change this setting.**

:macro-def:`DAGMAN_ABORT_ON_SCARY_SUBMIT[DAGMan]`
    A boolean value that controls whether to abort a DAG upon detection
    of a scary submit event. An example of a scary submit event is one
    in which the HTCondor ID does not match the expected value. Note
    that in all HTCondor versions prior to 6.9.3, :tool:`condor_dagman` did
    not abort a DAG upon detection of a scary submit event. This
    behavior is what now happens if :macro:`DAGMAN_ABORT_ON_SCARY_SUBMIT` is
    set to ``False``. If not defined, :macro:`DAGMAN_ABORT_ON_SCARY_SUBMIT`
    defaults to ``True``. **Note: users should rarely change this
    setting.**

:index:`Rescue and Retry<single: DAGMan Configuration Sections; Rescue and Retry>`

Rescue/retry
''''''''''''

:macro-def:`DAGMAN_AUTO_RESCUE[DAGMan]`
    A boolean value that controls whether :tool:`condor_dagman` automatically
    runs Rescue DAGs. If :macro:`DAGMAN_AUTO_RESCUE` is ``True`` and the DAG
    input file ``my.dag`` is submitted, and if a Rescue DAG such as the
    examples ``my.dag.rescue001`` or ``my.dag.rescue002`` exists, then
    the largest magnitude Rescue DAG will be run. If not defined,
    :macro:`DAGMAN_AUTO_RESCUE` defaults to ``True``.

:macro-def:`DAGMAN_MAX_RESCUE_NUM[DAGMan]`
    An integer value that controls the maximum Rescue DAG number that
    will be written, in the case that ``DAGMAN_OLD_RESCUE`` is
    ``False``, or run if :macro:`DAGMAN_AUTO_RESCUE` is ``True``. The maximum
    legal value is 999; the minimum value is 0, which prevents a Rescue
    DAG from being written at all, or automatically run. If not defined,
    :macro:`DAGMAN_MAX_RESCUE_NUM` defaults to 100.

:macro-def:`DAGMAN_RESET_RETRIES_UPON_RESCUE[DAGMan]`
    A boolean value that controls whether node retries are reset in a
    Rescue DAG. If this value is ``False``, the number of node retries
    written in a Rescue DAG is decreased, if any retries were used in
    the original run of the DAG; otherwise, the original number of
    retries is allowed when running the Rescue DAG. If not defined,
    :macro:`DAGMAN_RESET_RETRIES_UPON_RESCUE` defaults to ``True``.

:macro-def:`DAGMAN_WRITE_PARTIAL_RESCUE[DAGMan]`
    A boolean value that controls whether :tool:`condor_dagman` writes a
    partial or a full DAG file as a Rescue DAG. If not defined,
    :macro:`DAGMAN_WRITE_PARTIAL_RESCUE` defaults to ``True``. **Note: users
    should rarely change this setting.**

    .. warning::

        :macro:`DAGMAN_WRITE_PARTIAL_RESCUE[Deprecation Warning]` is deprecated
        as the writing of full Rescue DAG's is deprecated. This is slated to be
        removed during the lifetime of the HTCondor V24 feature series.

:macro-def:`DAGMAN_RETRY_SUBMIT_FIRST[DAGMan]`
    A boolean value that controls whether a failed submit is retried
    first (before any other submits) or last (after all other ready jobs
    are submitted). If this value is set to ``True``, when a job submit
    fails, the job is placed at the head of the queue of ready jobs, so
    that it will be submitted again before any other jobs are submitted.
    This had been the behavior of :tool:`condor_dagman`. If this value is set
    to ``False``, when a job submit fails, the job is placed at the tail
    of the queue of ready jobs. If not defined, it defaults to ``True``.

:macro-def:`DAGMAN_RETRY_NODE_FIRST[DAGMan]`
    A boolean value that controls whether a failed node with retries is
    retried first (before any other ready nodes) or last (after all
    other ready nodes). If this value is set to ``True``, when a node
    with retries fails after the submit succeeded, the node is placed at
    the head of the queue of ready nodes, so that it will be tried again
    before any other jobs are submitted. If this value is set to
    ``False``, when a node with retries fails, the node is placed at the
    tail of the queue of ready nodes. This had been the behavior of
    :tool:`condor_dagman`. If not defined, it defaults to ``False``.

:index:`Nodes Log File<single: DAGMan Configuration Sections; Nodes Log File>`

Log files
'''''''''

:macro-def:`DAGMAN_DEFAULT_NODE_LOG[DAGMan]`
    The default name of a file to be used as a job event log by all node
    jobs of a DAG.

    This configuration variable uses a special syntax in which **@**
    instead of **$** indicates an evaluation of special variables.
    Normal HTCondor configuration macros may be used with the normal
    **$** syntax.

    Special variables to be used only in defining this configuration
    variable:

    -  ``@(DAG_DIR)``: The directory in which the primary DAG input file
       resides. If more than one DAG input file is specified to
       :tool:`condor_submit_dag`, the primary DAG input file is the leftmost
       one on the command line.
    -  ``@(DAG_FILE)``: The name of the primary DAG input file. It does
       not include the path.
    -  ``@(CLUSTER)``: The :ad-attr:`ClusterId` attribute of the
       :tool:`condor_dagman` job.
    -  ``@(OWNER)``: The user name of the user who submitted the DAG.
    -  ``@(NODE_NAME)``: For SUBDAGs, this is the node name of the
       SUBDAG in the upper level DAG; for a top-level DAG, it is the
       string ``"undef"``.

    If not defined, ``@(DAG_DIR)/@(DAG_FILE).nodes.log`` is the default
    value.

    Notes:

    -  Using ``$(LOG)`` in defining a value for
       :macro:`DAGMAN_DEFAULT_NODE_LOG` will not have the expected effect,
       because ``$(LOG)`` is defined as ``"."`` for :tool:`condor_dagman`. To
       place the default log file into the log directory, write the
       expression relative to a known directory, such as
       ``$(LOCAL_DIR)/log`` (see examples below).
    -  A default log file placed in the spool directory will need extra
       configuration to prevent :tool:`condor_preen` from removing it; modify
       :macro:`VALID_SPOOL_FILES`. Removal of the default log file during a
       run will cause severe problems.
    -  **The value defined for DAGMAN_DEFAULT_NODE_LOG must ensure that the
       file is unique for each DAG.** Therefore, the value should always
       include ``@(DAG_FILE)``. For example,

       .. code-block:: condor-config

             DAGMAN_DEFAULT_NODE_LOG = $(LOCAL_DIR)/log/@(DAG_FILE).nodes.log


       is okay, but

       .. code-block:: condor-config

             DAGMAN_DEFAULT_NODE_LOG = $(LOCAL_DIR)/log/dag.nodes.log


       will cause failure when more than one DAG is run at the same time
       on a given access point.

:macro-def:`DAGMAN_LOG_ON_NFS_IS_ERROR[DAGMan]`
    A boolean value that controls whether :tool:`condor_dagman` prohibits a
    DAG workflow log from being on an NFS file system. This value is
    ignored if :macro:`CREATE_LOCKS_ON_LOCAL_DISK` and
    :macro:`ENABLE_USERLOG_LOCKING` are both ``True``. If a DAG uses such a
    workflow log file file and :macro:`DAGMAN_LOG_ON_NFS_IS_ERROR` is
    ``True`` (and not ignored), the DAG will abort during the
    initialization process. If not defined,
    :macro:`DAGMAN_LOG_ON_NFS_IS_ERROR` defaults to ``False``.

:macro-def:`DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS[DAGMan]`
    Allows any characters to be used in DAGMan node names, even
    characters that are considered illegal because they are used internally
    as separators. Turning this feature on could lead to instability when
    using splices or munged node names. The default value is ``False``.

:macro-def:`DAGMAN_ALLOW_EVENTS[DAGMan]`
    An integer that controls which bad events are considered fatal
    errors by :tool:`condor_dagman`. This macro replaces and expands upon the
    functionality of the ``DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION``
    macro. If :macro:`DAGMAN_ALLOW_EVENTS` is set, it overrides the setting
    of ``DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION``. **Note: users should
    rarely change this setting.**

    The :macro:`DAGMAN_ALLOW_EVENTS` value is a logical bitwise OR of the
    following values:

        -  0 = allow no bad events
        -  1 = allow all bad events, except the event ``"job re-run after terminated event"``
        -  2 = allow terminated/aborted event combination
        -  4 = allow a ``"job re-run after terminated event"`` bug
        -  8 = allow garbage or orphan events
        - 16 = allow an execute or terminate event before job's submit event
        - 32 = allow two terminated events per job, as sometimes seen with grid jobs
        - 64 = allow duplicated events in general

    The default value is 114, which allows terminated/aborted event
    combination, allows an execute and/or terminated event before job's
    submit event, allows double terminated events, and allows general
    duplicate events.

    As examples, a value of 6 instructs :tool:`condor_dagman` to allow both
    the terminated/aborted event combination and the
    ``"job re-run after terminated event"`` bug. A value of 0 means that
    any bad event will be considered a fatal error.

    A value of 5 will never abort the DAG because of a bad event. But
    this value should almost never be used, because the
    ``"job re-run after terminated event"`` bug breaks the semantics of
    the DAG.

:index:`Debugging Log<single: DAGMan Configuration Sections; Debugging Log>`

Debug output
''''''''''''

:macro-def:`DAGMAN_DEBUG[DAGMan]`
    This variable is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`DAGMAN_VERBOSITY[DAGMan]`
    An integer value defining the verbosity of output to the
    ``dagman.out`` file, as follows (each level includes all output from
    lower debug levels):

    -  level = 0; never produce output, except for usage info
    -  level = 1; very quiet, output severe errors
    -  level = 2; output errors and warnings
    -  level = 3; normal output
    -  level = 4; internal debugging output
    -  level = 5; internal debugging output; outer loop debugging
    -  level = 6; internal debugging output; inner loop debugging
    -  level = 7; internal debugging output; rarely used

    The default value if not defined is 3.

:macro-def:`DAGMAN_DEBUG_CACHE_ENABLE[DAGMan]`
    A boolean value that determines if log line caching for the
    ``dagman.out`` file should be enabled in the :tool:`condor_dagman`
    process to increase performance (potentially by orders of magnitude)
    when writing the ``dagman.out`` file to an NFS server. Currently,
    this cache is only utilized in Recovery Mode. If not defined, it
    defaults to ``False``.

:macro-def:`DAGMAN_DEBUG_CACHE_SIZE[DAGMan]`
    An integer value representing the number of bytes of log lines to be
    stored in the log line cache. When the cache surpasses this number,
    the entries are written out in one call to the logging subsystem. A
    value of zero is not recommended since each log line would surpass
    the cache size and be emitted in addition to bracketing log lines
    explaining that the flushing was happening. The legal range of
    values is 0 to INT_MAX. If defined with a value less than 0, the
    value 0 will be used. If not defined, it defaults to 5 Megabytes.

:macro-def:`DAGMAN_PENDING_REPORT_INTERVAL[DAGMan]`
    An integer value representing the number of seconds that controls
    how often :tool:`condor_dagman` will print a report of pending nodes to
    the ``dagman.out`` file. The report will only be printed if
    :tool:`condor_dagman` has been waiting at least
    :macro:`DAGMAN_PENDING_REPORT_INTERVAL` seconds without seeing any node
    job events, in order to avoid cluttering the ``dagman.out`` file.
    This feature is mainly intended to help diagnose :tool:`condor_dagman`
    processes that are stuck waiting indefinitely for a job to finish.
    If not defined, :macro:`DAGMAN_PENDING_REPORT_INTERVAL` defaults to 600
    seconds (10 minutes).

:macro-def:`DAGMAN_CHECK_QUEUE_INTERVAL[DAGMan]`
    An integer value representing the number of seconds DAGMan will wait
    pending on nodes to make progress before querying the local *condor_schedd*
    queue to verify that the jobs the DAG is pending on are in said queue.
    If jobs are missing, DAGMan will write a rescue DAG and abort. When set
    to a value equal to or less than 0 DAGMan will not query the *condor_schedd*.
    Default value is 28800 seconds (8 Hours).


:macro-def:`MAX_DAGMAN_LOG[DAGMan]`
    This variable is described in :macro:`MAX_<SUBSYS>_LOG`. If not defined,
    :macro:`MAX_DAGMAN_LOG` defaults to 0 (unlimited size).

:index:`HTCondor Attributes<single: DAGMan Configuration Sections; HTCondor Attributes>`

HTCondor attributes
'''''''''''''''''''

:macro-def:`DAGMAN_COPY_TO_SPOOL[DAGMan]`
    A boolean value that when ``True`` copies the :tool:`condor_dagman`
    binary to the spool directory when a DAG is submitted. Setting this
    variable to ``True`` allows long-running DAGs to survive a DAGMan
    version upgrade. For running large numbers of small DAGs, leave this
    variable unset or set it to ``False``. The default value if not
    defined is ``False``. **Note: users should rarely change this
    setting.**

:macro-def:`DAGMAN_INSERT_SUB_FILE[DAGMan]`
    A file name of a file containing submit description file commands to
    be inserted into the ``.condor.sub`` file created by
    :tool:`condor_submit_dag`. The specified file is inserted into the
    ``.condor.sub`` file before the
    :subcom:`queue[and DAGMAN_INSERT_SUB_FILE]` command and before
    any commands specified with the **-append** :tool:`condor_submit_dag`
    command line option. Note that the :macro:`DAGMAN_INSERT_SUB_FILE` value
    can be overridden by the :tool:`condor_submit_dag`
    **-insert_sub_file** command line option.

:macro-def:`DAGMAN_ON_EXIT_REMOVE[DAGMan]`
    Defines the ``OnExitRemove`` ClassAd expression placed into the
    :tool:`condor_dagman` submit description file by :tool:`condor_submit_dag`.
    The default expression is designed to ensure that :tool:`condor_dagman`
    is automatically re-queued by the *condor_schedd* daemon if it
    exits abnormally or is killed (for example, during a reboot). If
    this results in :tool:`condor_dagman` staying in the queue when it should
    exit, consider changing to a less restrictive expression, as in the
    example

    .. code-block:: condor-classad-expr

          (ExitBySignal == false || ExitSignal =!= 9)

    If not defined, :macro:`DAGMAN_ON_EXIT_REMOVE` defaults to the expression

    .. code-block:: condor-classad-expr

          ( ExitSignal =?= 11 || (ExitCode =!= UNDEFINED && ExitCode >=0 && ExitCode <= 2))

:macro-def:`DAGMAN_INHERIT_ATTRS`
    A list of job ClassAd attributes from the root DAGMan job's ClassAd to be
    passed down to all managed jobs including SubDAGs. Empty by default.

:macro-def:`DAGMAN_INHERIT_ATTRS_PREFIX`
    A string to be prefixed to the job ClassAd attributes described in
    :macro:`DAGMAN_INHERIT_ATTRS`. Empty by default.

Configuration File Entries Relating to Security
-----------------------------------------------

:index:`security configuration variables<single: security configuration variables; configuration>`

These macros affect the secure operation of HTCondor. Many of these
macros are described in the :doc:`/admin-manual/security` section.

:macro-def:`SEC_DEFAULT_AUTHENTICATION[SECURITY]` :macro-def:`SEC_*_AUTHENTICATION[SECURITY]`
    Whether authentication is required for a specified permission level.
    Acceptable values are ``REQUIRED``, ``PREFERRED``, ``OPTIONAL``, and
    ``NEVER``.  For example, setting ``SEC_READ_AUTHENTICATION = REQUIRED``
    indicates that any command requiring ``READ`` authorization will fail
    unless authentication is performed.  The special value,
    ``SEC_DEFAULT_AUTHENTICATION``, controls the default setting if no
    others are specified.

:macro-def:`SEC_DEFAULT_ENCRYPTION[SECURITY]` :macro-def:`SEC_*_ENCRYPTION[SECURITY]`
    Whether encryption is required for a specified permission level.
    Encryption prevents another entity on the same network from understanding
    the contents of the transfer between client and server.
    Acceptable values are ``REQUIRED``, ``PREFERRED``, ``OPTIONAL``, and
    ``NEVER``.  For example, setting ``SEC_WRITE_ENCRYPTION = REQUIRED``
    indicates that any command requiring ``WRITE`` authorization will fail
    unless the channel is encrypted.  The special value,
    ``SEC_DEFAULT_ENCRYPTION``, controls the default setting if no
    others are specified.

:macro-def:`SEC_DEFAULT_INTEGRITY[SECURITY]` :macro-def:`SEC_*_INTEGRITY[SECURITY]`
    Whether integrity-checking is required for a specified permission level.
    Integrity checking allows the client and server to detect changes
    (malicious or otherwise)  to the contents of the transfer.
    Acceptable values are ``REQUIRED``, ``PREFERRED``, ``OPTIONAL``, and
    ``NEVER``.  For example, setting ``SEC_WRITE_INTEGRITY = REQUIRED``
    indicates that any command requiring ``WRITE`` authorization will fail
    unless the channel is integrity-checked.  The special value,
    ``SEC_DEFAULT_INTEGRITY``, controls the default setting if no
    others are specified.

    As a special exception, file transfers are not integrity checked unless
    they are also encrypted.

:macro-def:`SEC_DEFAULT_NEGOTIATION[SECURITY]` :macro-def:`SEC_*_NEGOTIATION[SECURITY]`
    Whether the client and server should negotiate security parameters (such
    as encryption, integrity, and authentication) for a given authorization
    level.  For example, setting ``SEC_DEFAULT_NEGOTIATION = REQUIRED`` will
    require a security negotiation for all permission levels by default.
    There is very little penalty for security negotiation and it is strongly
    suggested to leave this as the default (``REQUIRED``) at all times.

:macro-def:`SEC_DEFAULT_AUTHENTICATION_METHODS[SECURITY]` :macro-def:`SEC_*_AUTHENTICATION_METHODS[SECURITY]`
    An ordered list of allowed authentication methods for a given authorization
    level.  This set of configuration variables controls both the ordering and
    the allowed methods.  Currently allowed values are
    ``SSL``, ``KERBEROS``, ``PASSWORD``, ``FS`` (non-Windows), ``FS_REMOTE``
    (non-Windows), ``NTSSPI``, ``MUNGE``, ``CLAIMTOBE``, ``IDTOKENS``,
    ``SCITOKENS``,  and ``ANONYMOUS``.
    See the :doc:`/admin-manual/security` section for a discussion of the
    relative merits of each method; some, such as ``CLAIMTOBE`` provide effectively
    no security at all.  The default authentication methods are
    ``NTSSPI,FS,IDTOKENS,KERBEROS,SSL``.

    These methods are tried in order until one succeeds or they all fail; for
    this reason, we do not recommend changing the default method list.

    The special value, ``SEC_DEFAULT_AUTHENTICATION_METHODS``, controls the
    default setting if no others are specified.

:macro-def:`SEC_DEFAULT_CRYPTO_METHODS[SECURITY]` :macro-def:`SEC_*_CRYPTO_METHODS[SECURITY]`
    An ordered list of allowed cryptographic algorithms to use for
    encrypting a network session at a specified authorization level.
    The server will select the first entry in its list that both
    server and client allow.
    Possible values are ``AES``, ``3DES``, and ``BLOWFISH``.
    The special parameter name ``SEC_DEFAULT_CRYPTO_METHODS`` controls the
    default setting if no others are specified.
    There is little benefit in varying
    the setting per authorization level; it is recommended to leave these
    settings untouched.

:macro-def:`HOST_ALIAS[SECURITY]`
    Specifies the fully qualified host name that clients authenticating
    this daemon with SSL should expect the daemon's certificate to
    match. The alias is advertised to the *condor_collector* as part of
    the address of the daemon. When this is not set, clients validate
    the daemon's certificate host name by matching it against DNS A
    records for the host they are connected to. See :macro:`SSL_SKIP_HOST_CHECK`
    for ways to disable this validation step.

:macro-def:`USE_COLLECTOR_HOST_CNAME[SECURITY]`
    A boolean value that determines what hostname a client should
    expect when validating the collector's certificate during SSL
    authentication.
    When set to ``True``, the hostname given to the client is used.
    When set to ``False``, if the given hostname is a DNS CNAME, the
    client resolves it to a DNS A record and uses that hostname.
    The default value is ``True``.

:macro-def:`DELEGATE_JOB_GSI_CREDENTIALS[SECURITY]`
    A boolean value that defaults to ``True`` for HTCondor version
    6.7.19 and more recent versions. When ``True``, a job's X.509
    credentials are delegated, instead of being copied. This results in
    a more secure communication when not encrypted.

:macro-def:`DELEGATE_FULL_JOB_GSI_CREDENTIALS[SECURITY]`
    A boolean value that controls whether HTCondor will delegate a full
    or limited X.509 proxy. The default value of ``False`` indicates
    the limited X.509 proxy.

:macro-def:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME[SECURITY]`
    An integer value that specifies the maximum number of seconds for
    which delegated proxies should be valid. The default value is one
    day. A value of 0 indicates that the delegated proxy should be valid
    for as long as allowed by the credential used to create the proxy.
    The job may override this configuration setting by using the
    :subcom:`delegate_job_GSI_credentials_lifetime[and DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME]`
    submit file command. This configuration variable currently only
    applies to proxies delegated for non-grid jobs and HTCondor-C jobs.
    This variable has no effect if
    :macro:`DELEGATE_JOB_GSI_CREDENTIALS`
    :index:`DELEGATE_JOB_GSI_CREDENTIALS` is ``False``.

:macro-def:`DELEGATE_JOB_GSI_CREDENTIALS_REFRESH[SECURITY]`
    A floating point number between 0 and 1 that indicates the fraction
    of a proxy's lifetime at which point delegated credentials with a
    limited lifetime should be renewed. The renewal is attempted
    periodically at or near the specified fraction of the lifetime of
    the delegated credential. The default value is 0.25. This setting
    has no effect if :macro:`DELEGATE_JOB_GSI_CREDENTIALS` is ``False``
    or if :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME` is 0. For
    non-grid jobs, the precise timing of the proxy refresh depends on
    :macro:`SHADOW_CHECKPROXY_INTERVAL`. To ensure that the
    delegated proxy remains valid, the interval for checking the proxy
    should be, at most, half of the interval for refreshing it.

:macro-def:`USE_VOMS_ATTRIBUTES[SECURITY]`
    A boolean value that controls whether HTCondor will attempt to extract
    and verify VOMS attributes from X.509 credentials.
    The default is ``False``.

:macro-def:`AUTH_SSL_USE_VOMS_IDENTITY[SECURITY]`
    A boolean value that controls whether VOMS attributes are included
    in the peer's authenticated identity during SSL authentication.
    This is used with the unified map file to determine the peer's
    HTCondor identity.
    If :macro:`USE_VOMS_ATTRIBUTES` is ``False``, then this parameter
    is treated as ``False``.
    The default is ``True``.

:macro-def:`SEC_<access-level>_SESSION_DURATION[SECURITY]`
    The amount of time in seconds before a communication session
    expires. A session is a record of necessary information to do
    communication between a client and daemon, and is protected by a
    shared secret key. The session expires to reduce the window of
    opportunity where the key may be compromised by attack. A short
    session duration increases the frequency with which daemons have to
    re-authenticate with each other, which may impact performance.

    If the client and server are configured with different durations,
    the shorter of the two will be used. The default for daemons is
    86400 seconds (1 day) and the default for command-line tools is 60
    seconds. The shorter default for command-line tools is intended to
    prevent daemons from accumulating a large number of communication
    sessions from the short-lived tools that contact them over time. A
    large number of security sessions consumes a large amount of memory.
    It is therefore important when changing this configuration setting
    to preserve the small session duration for command-line tools.

    One example of how to safely change the session duration is to
    explicitly set a short duration for tools and :tool:`condor_submit` and a
    longer duration for everything else:

    .. code-block:: condor-config

        SEC_DEFAULT_SESSION_DURATION = 50000
        TOOL.SEC_DEFAULT_SESSION_DURATION = 60
        SUBMIT.SEC_DEFAULT_SESSION_DURATION = 60

    Another example of how to safely change the session duration is to
    explicitly set the session duration for a specific daemon:

    .. code-block:: condor-config

        COLLECTOR.SEC_DEFAULT_SESSION_DURATION = 50000

    :index:`SEC_DEFAULT_SESSION_LEASE`

:macro-def:`SEC_<access-level>_SESSION_LEASE[SECURITY]`
    The maximum number of seconds an unused security session will be
    kept in a daemon's session cache before being removed to save
    memory. The default is 3600. If the server and client have different
    configurations, the smaller one will be used.

:macro-def:`SEC_INVALIDATE_SESSIONS_VIA_TCP[SECURITY]`
    Use TCP (if True) or UDP (if False) for responding to attempts to
    use an invalid security session. This happens, for example, if a
    daemon restarts and receives incoming commands from other daemons
    that are still using a previously established security session. The
    default is True.

:macro-def:`FS_REMOTE_DIR[SECURITY]`
    The location of a file visible to both server and client in Remote
    File System authentication. The default when not defined is the
    directory ``/shared/scratch/tmp``.

:macro-def:`DISABLE_EXECUTE_DIRECTORY_ENCRYPTION[SECURITY]`
    A boolean value that when set to ``True`` disables the ability for
    encryption of job execute directories on the specified host. Defaults
    to ``False``.

:macro-def:`ENCRYPT_EXECUTE_DIRECTORY[SECURITY]`
    A boolean value that, when ``True``, causes the execute directory for
    all jobs on Linux or Windows platforms to be encrypted. Defaults to
    ``False``. Enabling this functionality requires that the HTCondor
    service is run as user root on Linux platforms, or as a system service
    on Windows platforms. On Linux platforms, the encryption method is *cryptsetup*
    and is only available when using :macro:`STARTD_ENFORCE_DISK_LIMITS`.
    On Windows platforms, the encryption method is the EFS (Encrypted File System)
    feature of NTFS.

    .. note::
        Even if ``False``, the user can require encryption of the execute directory on a per-job
        basis by setting :subcom:`encrypt_execute_directory[and ENCRYPT_EXECUTE_DIRECTORY]` to ``True``
        in the job submit description file. Unless :macro:`DISABLE_EXECUTE_DIRECTORY_ENCRYPTION`
        is ``True``.

:macro-def:`SEC_TCP_SESSION_TIMEOUT[SECURITY]`
    The length of time in seconds until the timeout on individual
    network operations when establishing a UDP security session via TCP.
    The default value is 20 seconds. Scalability issues with a large
    pool would be the only basis for a change from the default value.

:macro-def:`SEC_TCP_SESSION_DEADLINE[SECURITY]`
    An integer representing the total length of time in seconds until
    giving up when establishing a security session. Whereas
    :macro:`SEC_TCP_SESSION_TIMEOUT` specifies the timeout for individual
    blocking operations (connect, read, write), this setting specifies
    the total time across all operations, including non-blocking operations
    that have little cost other than holding open the socket. The default
    value is 120 seconds. The intention of this setting is to avoid waiting
    for hours for a response in the rare event that the other side freezes
    up and the socket remains in a connected state. This problem has been
    observed in some types of operating system crashes.

:macro-def:`SEC_DEFAULT_AUTHENTICATION_TIMEOUT[SECURITY]`
    The length of time in seconds that HTCondor should attempt
    authenticating network connections before giving up. The default
    imposes no time limit, so the attempt never gives up. Like other
    security settings, the portion of the configuration variable name,
    ``DEFAULT``, may be replaced by a different access level to specify
    the timeout to use for different types of commands, for example
    ``SEC_CLIENT_AUTHENTICATION_TIMEOUT``.

:macro-def:`SEC_PASSWORD_FILE[SECURITY]`
    For Unix machines, the path and file name of the file containing the
    pool password for password authentication.

:macro-def:`SEC_PASSWORD_DIRECTORY[SECURITY]`
    The path to the directory containing signing key files
    for token authentication.  Defaults to ``/etc/condor/passwords.d`` on
    Unix and to ``$(RELEASE_DIR)\tokens.sk`` on Windows.

:macro-def:`TRUST_DOMAIN[SECURITY]`
    An arbitrary string used by the IDTOKENS authentication method; it defaults
    to :macro:`UID_DOMAIN`.  When HTCondor creates an IDTOKEN, it sets the
    issuer (``iss``) field to this value. When an HTCondor client attempts to
    authenticate using the IDTOKENS method, it only presents an IDTOKEN to
    the server if the server's reported issuer matches the token's.

    Note that the issuer (``iss``) field is for the _server_. Each IDTOKEN also
    contains a subject (``sub``) field, which identifies the user. IDTOKENS
    generated by `condor_token_fetch` will always be of the form
    ``user@UID_DOMAIN``.

    If you have configured the same signing key on two different machines,
    and want tokens issued by one machine to be accepted by the other (e.g.
    an access point and a central manager), those two machines must have
    the same value for this setting.

:macro-def:`SEC_TOKEN_FETCH_ALLOWED_SIGNING_KEYS[SECURITY]`
    A comma or space -separated list of signing key names that can be used
    to create a token if requested by :tool:`condor_token_fetch`.  Defaults
    to ``POOL``.

:macro-def:`SEC_TOKEN_ISSUER_KEY[SECURITY]`
    The default signing key name to use to create a token if requested
    by :tool:`condor_token_fetch`. Defaults to ``POOL``.

:macro-def:`SEC_TOKEN_POOL_SIGNING_KEY_FILE[SECURITY]`
    The path and filename for the file containing the default signing key
    for token authentication.  Defaults to ``/etc/condor/passwords.d/POOL`` on Unix
    and to ``$(RELEASE_DIR)\tokens.sk\POOL`` on Windows.

:macro-def:`SEC_TOKEN_SYSTEM_DIRECTORY[SECURITY]`
    For Unix machines, the path to the directory containing tokens for
    daemon-to-daemon authentication with the token method.  Defaults to
    ``/etc/condor/tokens.d``.

:macro-def:`SEC_TOKEN_DIRECTORY[SECURITY]`
    For Unix machines, the path to the directory containing tokens for
    user authentication with the token method.  Defaults to ``~/.condor/tokens.d``.

:macro-def:`SEC_TOKEN_REVOCATION_EXPR[SECURITY]`
    A ClassAd expression evaluated against tokens during authentication;
    if :macro:`SEC_TOKEN_REVOCATION_EXPR` is set and evaluates to true, then the
    token is revoked and the authentication attempt is denied.

:macro-def:`SEC_TOKEN_REQUEST_LIMITS[SECURITY]`
    If set, this is a comma-separated list of authorization levels that limit
    the authorizations a token request can receive.  For example, if
    :macro:`SEC_TOKEN_REQUEST_LIMITS` is set to ``READ, WRITE``, then a token
    cannot be issued with the authorization ``DAEMON`` even if this would
    otherwise be permissible.

:macro-def:`AUTH_SSL_SERVER_CAFILE[SECURITY]`
    The path and file name of a file containing one or more trusted CA's
    certificates for the server side of a communication authenticating
    with SSL.
    This file is used in addition to the default CA file configured
    for OpenSSL.

:macro-def:`AUTH_SSL_CLIENT_CAFILE[SECURITY]`
    The path and file name of a file containing one or more trusted CA's
    certificates for the client side of a communication authenticating
    with SSL.
    This file is used in addition to the default CA file configured
    for OpenSSL.

:macro-def:`AUTH_SSL_SERVER_CADIR[SECURITY]`
    The path to a directory containing the certificates (each in
    its own file) for multiple trusted CAs for the server side of a
    communication authenticating with SSL.
    This directory is used in addition to the default CA directory
    configured for OpenSSL.

:macro-def:`AUTH_SSL_CLIENT_CADIR[SECURITY]`
    The path to a directory containing the certificates (each in
    its own file) for multiple trusted CAs for the client side of a
    communication authenticating with SSL.
    This directory is used in addition to the default CA directory
    configured for OpenSSL.

:macro-def:`AUTH_SSL_SERVER_USE_DEFAULT_CAS[SECURITY]`
    A boolean value that controls whether the default trusted CA file
    and directory configured for OpenSSL should be used by the server
    during SSL authentication.
    The default value is ``True``.

:macro-def:`AUTH_SSL_CLIENT_USE_DEFAULT_CAS[SECURITY]`
    A boolean value that controls whether the default trusted CA file
    and directory configured for OpenSSL should be used by the client
    during SSL authentication.
    The default value is ``True``.

:macro-def:`AUTH_SSL_SERVER_CERTFILE[SECURITY]`
    A comma-separated list of filenames to search for a public certificate
    to be used for the server side of SSL authentication.
    The first file that contains a valid credential (in combination with
    :macro:`AUTH_SSL_SERVER_KEYFILE`)  will be used.

:macro-def:`AUTH_SSL_CLIENT_CERTFILE[SECURITY]`
    The path and file name of the file containing the public certificate
    for the client side of a communication authenticating with SSL.  If
    no client certificate is provided, then the client may authenticate
    as the user ``anonymous@ssl``.

:macro-def:`AUTH_SSL_SERVER_KEYFILE[SECURITY]`
    A comma-separated list of filenames to search for a private key
    to be used for the server side of SSL authentication.
    The first file that contains a valid credential (in combination with
    :macro:`AUTH_SSL_SERVER_CERTFILE`) will be used.

:macro-def:`AUTH_SSL_CLIENT_KEYFILE[SECURITY]`
    The path and file name of the file containing the private key for
    the client side of a communication authenticating with SSL.

:macro-def:`AUTH_SSL_REQUIRE_CLIENT_CERTIFICATE[SECURITY]`
    A boolean value that controls whether the client side of a
    communication authenticating with SSL must have a credential.
    If set to ``True`` and the client doesn't have a credential, then
    the SSL authentication will fail and other authentication methods
    will be tried.
    The default is ``False``.

:macro-def:`AUTH_SSL_ALLOW_CLIENT_PROXY[SECURITY]`
    A boolean value that controls whether a daemon will accept an
    X.509 proxy certificate from a client during SSL authentication.
    The default is ``False``.

:macro-def:`AUTH_SSL_USE_CLIENT_PROXY_ENV_VAR[SECURITY]`
    A boolean value that controls whether a client checks environment
    variable `X509_USER_PROXY` for the location the X.509 credential
    to use for SSL authentication with a daemon.
    If this parameter is ``True`` and `X509_USER_PROXY` is set, then
    that file is used instead of the files specified by
    `AUTH_SSL_CLIENT_CERTFILE` and `AUTH_SSL_CLIENT_KEYFILE`.
    The default is ``False``.

:macro-def:`SSL_SKIP_HOST_CHECK[SECURITY]`
    A boolean variable that controls whether a host check is performed
    by the client during an SSL authentication of a Condor daemon. This
    check requires the daemon's host name to match either the "distinguished
    name" or a subject alternate name embedded in the server's host certificate
    When the  default value of ``False`` is set, the check is not skipped.
    When ``True``, this check is skipped, and hosts will not be rejected due
    to a mismatch of certificate and host name.

:macro-def:`COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE[SECURITY]`
    A boolean variable that controls whether the *condor_collector*
    should generate its own CA and host certificate at startup.
    When ``True``, if the SSL certificate file given by
    :macro:`AUTH_SSL_SERVER_CERTFILE` doesn't exist, the *condor_collector*
    will generate its own CA, then use that CA to generate an SSL host
    certificate. The certificate and key files are written to the
    locations given by :macro:`AUTH_SSL_SERVER_CERTFILE` and
    :macro:`AUTH_SSL_SERVER_KEYFILE`, respectively.
    The locations of the CA files are controlled by
    :macro:`TRUST_DOMAIN_CAFILE` and :macro:`TRUST_DOMAIN_CAKEY`.
    The default value is ``True`` on unix platforms and ``False`` on Windows.

:macro-def:`TRUST_DOMAIN_CAFILE[SECURITY]`
    A path specifying the location of the CA the *condor_collector*
    will automatically generate if needed when
    :macro:`COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE` is ``True``.
    This CA will be used to generate a host certificate and key
    if one isn't provided in :macro:`AUTH_SSL_SERVER_KEYFILE` :index:`AUTH_SSL_SERVER_KEYFILE`.
    On Linux, this defaults to ``/etc/condor/trust_domain_ca.pem``.

:macro-def:`TRUST_DOMAIN_CAKEY[SECURITY]`
    A path specifying the location of the private key for the CA generated at
    :macro:`TRUST_DOMAIN_CAFILE`.  On Linux, this defaults ``/etc/condor/trust_domain_ca_privkey.pem``.

:macro-def:`BOOTSTRAP_SSL_SERVER_TRUST[SECURITY]`
    A boolean variable controlling whether tools and daemons automatically trust
    the SSL host certificate presented on first authentication.  When the
    default of ``false`` is set, daemons only trust host certificates from known
    CAs and tools may prompt the user for confirmation if the
    certificate is not trusted (see :macro:`BOOTSTRAP_SSL_SERVER_TRUST_PROMPT_USER`).
    After the first authentication, the method and certificate are persisted to a
    ``known_hosts`` file; subsequent authentications will succeed only if the certificate
    is unchanged from the one in the ``known_hosts`` file.

:macro-def:`BOOTSTRAP_SSL_SERVER_TRUST_PROMPT_USER[SECURITY]`
    A boolean variable that controls if tools will prompt the user about
    whether to trust an SSL host certificate from an unknown CA.
    The default value is ``True``.

:macro-def:`SEC_SYSTEM_KNOWN_HOSTS[SECURITY]`
    The location of the ``known_hosts`` file for daemon authentication.  This defaults
    to ``/etc/condor/known_hosts`` on Linux.  Tools will always save their ``known_hosts``
    file inside ``$HOME/.condor``.

:macro-def:`CERTIFICATE_MAPFILE[SECURITY]`
    A path and file name of the unified map file.

:macro-def:`CERTIFICATE_MAPFILE_ASSUME_HASH_KEYS[SECURITY]`
    For HTCondor version 8.5.8 and later. When this is true, the second
    field of the :macro:`CERTIFICATE_MAPFILE` is not interpreted as a regular
    expression unless it begins and ends with the slash / character.

:macro-def:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION[SECURITY]`
    This is a special authentication mechanism designed to minimize
    overhead in the *condor_schedd* when communicating with the execute
    machine. When this is enabled, the *condor_negotiator* sends the *condor_schedd*
    a secret key generated by the *condor_startd*.  This key is used
    to establish a strong security session between the execute and
    submit daemons without going through the usual security negotiation
    protocol. This is especially important when operating at large scale
    over high latency networks (for example, on a pool with one
    *condor_schedd* daemon and thousands of *condor_startd* daemons on
    a network with a 0.1 second round trip time).

    The default value is ``True``. To have any effect, it must be
    ``True`` in the configuration of both the execute side
    (*condor_startd*) as well as the submit side (*condor_schedd*).
    When ``True``, all other security negotiation between the submit and
    execute daemons is bypassed. All inter-daemon communication between
    the submit and execute side will use the *condor_startd* daemon's
    settings for ``SEC_DAEMON_ENCRYPTION`` and ``SEC_DAEMON_INTEGRITY``;
    the configuration of these values in the *condor_schedd*,
    *condor_shadow*, and *condor_starter* are ignored.

    Important: for this mechanism to be secure, integrity
    and encryption, should be enabled in the startd configuration. Also,
    some form of strong mutual authentication (e.g. SSL) should be
    enabled between all daemons and the central manager.  Otherwise, the shared
    secret which is exchanged in matchmaking cannot be safely encrypted
    when transmitted over the network.

    The *condor_schedd* and *condor_shadow* will be authenticated as
    ``submit-side@matchsession`` when they talk to the *condor_startd* and
    *condor_starter*. The *condor_startd* and *condor_starter* will
    be authenticated as ``execute-side@matchsession`` when they talk to the
    *condor_schedd* and *condor_shadow*. These identities is
    automatically added to the DAEMON, READ, and CLIENT authorization
    levels in these daemons when needed.

    This same mechanism is also used to allow the *condor_negotiator* to authenticate
    with the *condor_schedd*.  The submitter ads contain a unique security key;
    any entity that can obtain the key from the collector (by default, anyone
    with ``NEGOTIATOR`` permission) is authorized to perform negotiation with
    the *condor_schedd*.  This implies, when :macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION`
    is enabled, the HTCondor administrator does not need to explicitly setup
    authentication from the negotiator to the submit host.

:macro-def:`SEC_USE_FAMILY_SESSION[SECURITY]`
    The "family" session is a special security session that's shared
    between an HTCondor daemon and all of its descendant daemons. It
    allows a family of daemons to communicate securely without an
    expensive authentication negotiation on each network connection. It
    bypasses the security authorization settings. The default value is
    ``True``.

:macro-def:`SEC_ENABLE_REMOTE_ADMINISTRATION[SECURITY]`
    A boolean parameter that controls whether daemons should include a
    secret administration key when they advertise themselves to the
    **condor_collector**.
    Anyone with this key is authorized to send ADMINISTRATOR-level
    commands to the daemon.
    The **condor_collector** will only provide this key to clients who
    are authorized at the ADMINISTRATOR level to the **condor_collector**.
    The default value is ``True``.

    When this parameter is enabled for all daemons, control of who is
    allowed to administer the pool can be consolidated in the
    **condor_collector** and its security configuration.

:macro-def:`KERBEROS_MAP_FILE[SECURITY]`
    A path to a file that contains ' = ' seperated keys and values,
    one per line.  The key is the kerberos realm, and the value
    is the HTCondor uid domain.

:macro-def:`KERBEROS_SERVER_KEYTAB[SECURITY]`
    The path and file name of the keytab file that holds the necessary
    Kerberos principals. If not defined, this variable's value is set by
    the installed Kerberos; it is ``/etc/v5srvtab`` on most systems.

:macro-def:`KERBEROS_SERVER_PRINCIPAL[SECURITY]`
    An exact Kerberos principal to use. The default value is
    ``$(KERBEROS_SERVER_SERVICE)/<hostname>@<realm>``, where
    :macro:`KERBEROS_SERVER_SERVICE` defaults to ``host``. When
    both :macro:`KERBEROS_SERVER_PRINCIPAL` and :macro:`KERBEROS_SERVER_SERVICE`
    are defined, this value takes precedence.

:macro-def:`KERBEROS_SERVER_USER[SECURITY]`
    The user name that the Kerberos server principal will map to after
    authentication. The default value is condor.

:macro-def:`KERBEROS_SERVER_SERVICE[SECURITY]`
    A string representing the Kerberos service name. This string is
    suffixed with a slash character (/) and the host name in order to
    form the Kerberos server principal. This value defaults to ``host``.
    When both :macro:`KERBEROS_SERVER_PRINCIPAL` and :macro:`KERBEROS_SERVER_SERVICE`
    are defined, the value of :macro:`KERBEROS_SERVER_PRINCIPAL` takes
    precedence.

:macro-def:`KERBEROS_CLIENT_KEYTAB[SECURITY]`
    The path and file name of the keytab file for the client in Kerberos
    authentication. This variable has no default value.

:macro-def:`SCITOKENS_FILE[SECURITY]`
    The path and file name of a file containing a SciToken for use by
    the client during the SCITOKENS authentication methods.  This variable
    has no default value.  If left unset, HTCondor will use the bearer
    token discovery protocol defined by the WLCG (https://zenodo.org/record/3937438)
    to find one.

:macro-def:`SEC_CREDENTIAL_SWEEP_DELAY[SECURITY]`
    The number of seconds to wait before cleaning up unused credentials.
    Defaults to 3,600 seconds (1 hour).

:macro-def:`SEC_CREDENTIAL_DIRECTORY_KRB[SECURITY]`
    The directory that users' Kerberos credentials should be stored in.
    This variable has no default value.

:macro-def:`SEC_CREDENTIAL_DIRECTORY_OAUTH[SECURITY]`
    The directory that users' OAuth2 credentials should be stored in.
    This directory must be owned by root:condor with the setgid flag enabled.

:macro-def:`SEC_CREDENTIAL_PRODUCER[SECURITY]`
    A script for :tool:`condor_submit` to execute to produce credentials while
    using the Kerberos type of credentials.  No parameters are passed,
    and credentials most be sent to stdout.

:macro-def:`SEC_CREDENTIAL_STORER[SECURITY]`
    A script for :tool:`condor_submit` to execute to produce credentials while
    using the OAuth2 type of credentials.  The oauth services specified
    in the ``use_auth_services`` line in the submit file are passed as
    parameters to the script, and the script should use
    ``condor_store_cred`` to store credentials for each service.
    Additional modifiers to each service may be passed: &handle=,
    &scopes=, or &audience=.  The handle should be appended after
    an underscore to the service name used with ``condor_store_cred``,
    the comma-separated list of scopes should be passed to the command
    with the -S option, and the audience should be passed to it with the
    -A option.

:macro-def:`SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES[SECURITY]`
    A boolean value that defaults to False.  Set to True to 
    allow EGI CheckIn tokens to be used to authenticate via the SCITOKENS
    authentication method.

:macro-def:`SEC_SCITOKENS_FOREIGN_TOKEN_ISSUERS[SECURITY]`
    When the :macro:`SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES` is True,
    this parameter is a list of URLs that determine which token types
    will be accepted under these relaxed checks. It's a list of issuer URLs that
    defaults to the EGI CheckIn issuer.  These parameters should be used with
    caution, as they disable some security checks.

:macro-def:`SEC_SCITOKENS_PLUGIN_NAMES[SECURITY]`
    If the special value ``PLUGIN:*`` is given in the scitokens map file, 
    then this configuration parameter is consulted to determine the names of the
    plugins to run.

:macro-def:`SEC_SCITOKENS_PLUGIN_<name>_COMMAND[SECURITY]`
    For each plugin above with <name>, this parameter gives the executable and optional
    command line arguments needed to invoke the plugin.

:macro-def:`SEC_SCITOKENS_PLUGIN_<name>_MAPPING[SECURITY]`
    For each plugin above with <name>, this parameter specifies the mapped
    identity if the plugin accepts the token.

:macro-def:`LEGACY_ALLOW_SEMANTICS[SECURITY]`
    A boolean parameter that defaults to ``False``.
    In HTCondor 8.8 and prior, if `ALLOW_DAEMON` or `DENY_DAEMON` wasn't
    set in the configuration files, then the value of `ALLOW_WRITE` or
    `DENY_DAEMON` (respectively) was used for these parameters.
    Setting `LEGACY_ALLOW_SEMANTICS` to ``True`` enables this old behavior.
    This is a potential security concern, so this setting should only be
    used to ease the upgrade of an existing pool from 8.8 or prior to
    9.0 or later.


Configuration File Entries Relating to Virtual Machines
-------------------------------------------------------

:index:`virtual machine configuration variables<single: virtual machine configuration variables; configuration>`

These macros affect how HTCondor runs **vm** universe jobs on a matched
machine within the pool. They specify items related to the
*condor_vm-gahp*.

:macro-def:`VM_GAHP_SERVER[Virtual Machines]`
    The complete path and file name of the *condor_vm-gahp*. The
    default value is ``$(SBIN)``/condor_vm-gahp.

:macro-def:`VM_GAHP_LOG[Virtual Machines]`
    The complete path and file name of the *condor_vm-gahp* log. If not
    specified on a Unix platform, the *condor_starter* log will be used
    for *condor_vm-gahp* log items. There is no default value for this
    required configuration variable on Windows platforms.

:macro-def:`MAX_VM_GAHP_LOG[Virtual Machines]`
    Controls the maximum length (in bytes) to which the
    *condor_vm-gahp* log will be allowed to grow.

:macro-def:`VM_TYPE[Virtual Machines]`
    Specifies the type of supported virtual machine software. It will be
    the value ``kvm`` or ``xen``. There is no default value for this
    required configuration variable.

:macro-def:`VM_MEMORY[Virtual Machines]`
    An integer specifying the maximum amount of memory in MiB to be
    shared among the VM universe jobs run on this machine.

:macro-def:`VM_MAX_NUMBER[Virtual Machines]`
    An integer limit on the number of executing virtual machines. When
    not defined, the default value is the same :macro:`NUM_CPUS`. When it
    evaluates to ``Undefined``, as is the case when not defined with a
    numeric value, no meaningful limit is imposed.

:macro-def:`VM_STATUS_INTERVAL[Virtual Machines]`
    An integer number of seconds that defaults to 60, representing the
    interval between job status checks by the *condor_starter* to see
    if the job has finished. A minimum value of 30 seconds is enforced.

:macro-def:`VM_GAHP_REQ_TIMEOUT[Virtual Machines]`
    An integer number of seconds that defaults to 300 (five minutes),
    representing the amount of time HTCondor will wait for a command
    issued from the *condor_starter* to the *condor_vm-gahp* to be
    completed. When a command times out, an error is reported to the
    *condor_startd*.

:macro-def:`VM_RECHECK_INTERVAL[Virtual Machines]`
    An integer number of seconds that defaults to 600 (ten minutes),
    representing the amount of time the *condor_startd* waits after a
    virtual machine error as reported by the *condor_starter*, and
    before checking a final time on the status of the virtual machine.
    If the check fails, HTCondor disables starting any new vm universe
    jobs by removing the :ad-attr:`VM_Type` attribute from the machine ClassAd.

:macro-def:`VM_SOFT_SUSPEND[Virtual Machines]`
    A boolean value that defaults to ``False``, causing HTCondor to free
    the memory of a vm universe job when the job is suspended. When
    ``True``, the memory is not freed.

:macro-def:`VM_NETWORKING[Virtual Machines]`
    A boolean variable describing if networking is supported. When not
    defined, the default value is ``False``.

:macro-def:`VM_NETWORKING_TYPE[Virtual Machines]`
    A string describing the type of networking, required and relevant
    only when :macro:`VM_NETWORKING` is ``True``. Defined strings are

    .. code-block:: text

            bridge
            nat
            nat, bridge


:macro-def:`VM_NETWORKING_DEFAULT_TYPE[Virtual Machines]`
    Where multiple networking types are given in :macro:`VM_NETWORKING_TYPE`,
    this optional configuration variable identifies which to use.
    Therefore, for

    .. code-block:: condor-config

          VM_NETWORKING_TYPE = nat, bridge


    this variable may be defined as either ``nat`` or ``bridge``. Where
    multiple networking types are given in :macro:`VM_NETWORKING_TYPE`, and
    this variable is not defined, a default of ``nat`` is used.

:macro-def:`VM_NETWORKING_BRIDGE_INTERFACE[Virtual Machines]`
    For Xen and KVM only, a required string if bridge networking is to
    be enabled. It specifies the networking interface that vm universe
    jobs will use.

:macro-def:`LIBVIRT_XML_SCRIPT[Virtual Machines]`
    For Xen and KVM only, a path and executable specifying a program.
    When the *condor_vm-gahp* is ready to start a Xen or KVM **vm**
    universe job, it will invoke this program to generate the XML
    description of the virtual machine, which it then provides to the
    virtualization software. The job ClassAd will be provided to this
    program via standard input. This program should print the XML to
    standard output. If this configuration variable is not set, the
    *condor_vm-gahp* will generate the XML itself. The provided script
    in ``$(LIBEXEC)``/libvirt_simple_script.awk will generate the same
    XML that the *condor_vm-gahp* would.

:macro-def:`LIBVIRT_XML_SCRIPT_ARGS[Virtual Machines]`
    For Xen and KVM only, the command-line arguments to be given to the
    program specified by :macro:`LIBVIRT_XML_SCRIPT`.

The following configuration variables are specific to the Xen virtual
machine software.

:macro-def:`XEN_BOOTLOADER[Virtual Machines]`
    A required full path and executable for the Xen bootloader, if the
    kernel image includes a disk image.

Configuration File Entries Relating to High Availability
--------------------------------------------------------

:index:`high availability configuration variables<single: high availability configuration variables; configuration>`

These macros affect the high availability operation of HTCondor.

:macro-def:`MASTER_HA_LIST[High Availability]`
    Similar to :macro:`DAEMON_LIST`, this macro defines a list of daemons
    that the :tool:`condor_master` starts and keeps its watchful eyes on.
    However, the :macro:`MASTER_HA_LIST` daemons are run in a High
    Availability mode. The list is a comma or space separated list of
    subsystem names (as listed in
    :ref:`admin-manual/introduction-to-configuration:pre-defined macros`).
    For example,

    .. code-block:: condor-config

                MASTER_HA_LIST = SCHEDD


    The High Availability feature allows for several :tool:`condor_master`
    daemons (most likely on separate machines) to work together to
    insure that a particular service stays available. These
    :tool:`condor_master` daemons ensure that one and only one of them will
    have the listed daemons running.

    To use this feature, the lock URL must be set with :macro:`HA_LOCK_URL`.

    Currently, only file URLs are supported (those with ``file:...``). The
    default value for :macro:`MASTER_HA_LIST` is the empty string, which
    disables the feature.

:macro-def:`HA_LOCK_URL[High Availability]`
    This macro specifies the URL that the :tool:`condor_master` processes use
    to synchronize for the High Availability service. Currently, only
    file URLs are supported; for example, ``file:/share/spool``. Note
    that this URL must be identical for all :tool:`condor_master` processes
    sharing this resource. For *condor_schedd* sharing, we recommend
    setting up :macro:`SPOOL` on an NFS share and having all High
    Availability *condor_schedd* processes sharing it, and setting the
    :macro:`HA_LOCK_URL` to point at this directory as well. For example:

    .. code-block:: condor-config

                MASTER_HA_LIST = SCHEDD
                SPOOL = /share/spool
                HA_LOCK_URL = file:/share/spool
                VALID_SPOOL_FILES = SCHEDD.lock

    A separate lock is created for each High Availability daemon.

    There is no default value for :macro:`HA_LOCK_URL`.

    Lock files are in the form <SUBSYS>.lock. :tool:`condor_preen` is not
    currently aware of the lock files and will delete them if they are
    placed in the :macro:`SPOOL` directory, so be sure to add <SUBSYS>.lock
    to :macro:`VALID_SPOOL_FILES` for each High Availability daemon.

:macro-def:`HA_<SUBSYS>_LOCK_URL[High Availability]`
    This macro controls the High Availability lock URL for a specific
    subsystem as specified in the configuration variable name, and it
    overrides the system-wide lock URL specified by :macro:`HA_LOCK_URL`. If
    not defined for each subsystem, :macro:`HA_<SUBSYS>_LOCK_URL` is ignored,
    and the value of :macro:`HA_LOCK_URL` is used.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`HA_LOCK_HOLD_TIME[High Availability]`
    This macro specifies the number of seconds that the :tool:`condor_master`
    will hold the lock for each High Availability daemon. Upon gaining
    the shared lock, the :tool:`condor_master` will hold the lock for this
    number of seconds. Additionally, the :tool:`condor_master` will
    periodically renew each lock as long as the :tool:`condor_master` and the
    daemon are running. When the daemon dies, or the :tool:`condor_master`
    exists, the :tool:`condor_master` will immediately release the lock(s) it
    holds.

    :macro:`HA_LOCK_HOLD_TIME` defaults to 3600 seconds (one hour).

:macro-def:`HA_<SUBSYS>_LOCK_HOLD_TIME[High Availability]`
    This macro controls the High Availability lock hold time for a
    specific subsystem as specified in the configuration variable name,
    and it overrides the system wide poll period specified by
    :macro:`HA_LOCK_HOLD_TIME`. If not defined for each subsystem,
    :macro:`HA_<SUBSYS>_LOCK_HOLD_TIME` is ignored, and the value of
    :macro:`HA_LOCK_HOLD_TIME` is used.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`HA_POLL_PERIOD[High Availability]`
    This macro specifies how often the :tool:`condor_master` polls the High
    Availability locks to see if any locks are either stale (meaning not
    updated for :macro:`HA_LOCK_HOLD_TIME` seconds), or have been released by
    the owning :tool:`condor_master`. Additionally, the :tool:`condor_master`
    renews any locks that it holds during these polls.

    :macro:`HA_POLL_PERIOD` defaults to 300 seconds (five minutes).

:macro-def:`HA_<SUBSYS>_POLL_PERIOD[High Availability]`
    This macro controls the High Availability poll period for a specific
    subsystem as specified in the configuration variable name, and it
    overrides the system wide poll period specified by
    :macro:`HA_POLL_PERIOD`. If not defined for each subsystem,
    :macro:`HA_<SUBSYS>_POLL_PERIOD` is ignored, and the value of
    :macro:`HA_POLL_PERIOD` is used.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MASTER_<SUBSYS>_CONTROLLER[High Availability]`
    Used only in HA configurations involving the *condor_had*.

    The :tool:`condor_master` has the concept of a controlling and controlled
    daemon, typically with the *condor_had* daemon serving as the
    controlling process. In this case, all :tool:`condor_on` and
    :tool:`condor_off` commands directed at controlled daemons are given to
    the controlling daemon, which then handles the command, and, when
    required, sends appropriate commands to the :tool:`condor_master` to do
    the actual work. This allows the controlling daemon to know the
    state of the controlled daemon.

    As of 6.7.14, this configuration variable must be specified for all
    configurations using *condor_had*. To configure the
    *condor_negotiator* controlled by *condor_had*:

    .. code-block:: condor-config

        MASTER_NEGOTIATOR_CONTROLLER = HAD

    The macro is named by substituting :macro:`<SUBSYS>` with the appropriate
    subsystem string as defined by :macro:`SUBSYSTEM`.

:macro-def:`HAD_LIST[High Availability]`
    A comma-separated list of all *condor_had* daemons in the form
    ``IP:port`` or ``hostname:port``. Each central manager machine that
    runs the *condor_had* daemon should appear in this list. If
    :macro:`HAD_USE_PRIMARY` is set to ``True``, then the first machine in
    this list is the primary central manager, and all others in the list
    are backups.

    All central manager machines must be configured with an identical
    :macro:`HAD_LIST`. The machine addresses are identical to the addresses
    defined in :macro:`COLLECTOR_HOST`.

:macro-def:`HAD_USE_PRIMARY[High Availability]`
    Boolean value to determine if the first machine in the :macro:`HAD_LIST`
    configuration variable is a primary central manager. Defaults to
    ``False``.

:macro-def:`HAD_CONTROLLEE[High Availability]`
    This variable is used to specify the name of the daemon which the
    *condor_had* daemon controls. This name should match the daemon
    name in the :tool:`condor_master` daemon's :macro:`DAEMON_LIST` definition.
    The default value is ``NEGOTIATOR``.

:macro-def:`HAD_CONNECTION_TIMEOUT[High Availability]`
    The time (in seconds) that the *condor_had* daemon waits before
    giving up on the establishment of a TCP connection. The failure of
    the communication connection is the detection mechanism for the
    failure of a central manager machine. For a LAN, a recommended value
    is 2 seconds. The use of authentication (by HTCondor) increases the
    connection time. The default value is 5 seconds. If this value is
    set too low, *condor_had* daemons will incorrectly assume the
    failure of other machines.

:macro-def:`HAD_ARGS[High Availability]`
    Command line arguments passed by the :tool:`condor_master` daemon as it
    invokes the *condor_had* daemon. To make high availability work,
    the *condor_had* daemon requires the port number it is to use. This
    argument is of the form

    .. code-block:: text

           -p $(HAD_PORT_NUMBER)


    where ``HAD_PORT_NUMBER`` is a helper configuration variable defined
    with the desired port number. Note that this port number must be the
    same value here as used in :macro:`HAD_LIST`. There is no default value.

:macro-def:`HAD[High Availability]`
    The path to the *condor_had* executable. Normally it is defined
    relative to ``$(SBIN)``. This configuration variable has no default
    value.

:macro-def:`MAX_HAD_LOG[High Availability]`
    Controls the maximum length in bytes to which the *condor_had*
    daemon log will be allowed to grow. It will grow to the specified
    length, then be saved to a file with the suffix ``.old``. The
    ``.old`` file is overwritten each time the log is saved, thus the
    maximum space devoted to logging is twice the maximum length of this
    log file. A value of 0 specifies that this file may grow without
    bounds. The default is 1 MiB.

:macro-def:`HAD_DEBUG[High Availability]`
    Logging level for the *condor_had* daemon. See :macro:`<SUBSYS>_DEBUG`
    for values.

:macro-def:`HAD_LOG[High Availability]`
    Full path and file name of the log file. The default value is
    ``$(LOG)``/HADLog.

:macro-def:`HAD_FIPS_MODE[High Availability]`
    Controls what type of checksum will be sent along with files that are replicated.
    Set it to 0 for MD5 checksums and to 1 for SHA-2 checksums.
    Prior to versions 8.8.13 and 8.9.12 only MD5 checksums are supported. In the 10.0 and
    later release of HTCondor, MD5 support will be removed and only SHA-2 will be
    supported.  This configuration variable is intended to provide a transition
    between the 8.8 and 9.0 releases.  Once all machines in your pool involved in HAD replication
    have been upgraded to 9.0 or later, you should set the value of this configuration
    variable to 1. Default value is 0 in HTCondor versions before 9.12 and 1 in version 9.12 and later.

:macro-def:`REPLICATION_LIST[High Availability]`
    A comma-separated list of all *condor_replication* daemons in the
    form ``IP:port`` or ``hostname:port``. Each central manager machine
    that runs the *condor_had* daemon should appear in this list. All
    potential central manager machines must be configured with an
    identical :macro:`REPLICATION_LIST`.

:macro-def:`STATE_FILE[High Availability]`
    A full path and file name of the file protected by the replication
    mechanism. When not defined, the default path and file used is

    .. code-block:: console

          $(SPOOL)/Accountantnew.log


:macro-def:`REPLICATION_INTERVAL[High Availability]`
    Sets how often the *condor_replication* daemon initiates its tasks
    of replicating the ``$(STATE_FILE)``. It is defined in seconds and
    defaults to 300 (5 minutes).

:macro-def:`MAX_TRANSFER_LIFETIME[High Availability]`
    A timeout period within which the process that transfers the state
    file must complete its transfer. The recommended value is
    ``2 * average size of state file / network rate``. It is defined in
    seconds and defaults to 300 (5 minutes).

:macro-def:`HAD_UPDATE_INTERVAL[High Availability]`
    Like :macro:`UPDATE_INTERVAL`, determines how often the *condor_had* is
    to send a ClassAd update to the *condor_collector*. Updates are
    also sent at each and every change in state. It is defined in
    seconds and defaults to 300 (5 minutes).

:macro-def:`HAD_USE_REPLICATION[High Availability]`
    A boolean value that defaults to ``False``. When ``True``, the use
    of *condor_replication* daemons is enabled.

:macro-def:`REPLICATION_ARGS[High Availability]`
    Command line arguments passed by the :tool:`condor_master` daemon as it
    invokes the *condor_replication* daemon. To make high availability
    work, the *condor_replication* daemon requires the port number it
    is to use. This argument is of the form

    .. code-block:: text

          -p $(REPLICATION_PORT_NUMBER)


    where ``REPLICATION_PORT_NUMBER`` is a helper configuration variable
    defined with the desired port number. Note that this port number
    must be the same value as used in :macro:`REPLICATION_LIST`. There is no
    default value.

:macro-def:`REPLICATION[High Availability]`
    The full path and file name of the *condor_replication* executable.
    It is normally defined relative to ``$(SBIN)``. There is no default
    value.

:macro-def:`MAX_REPLICATION_LOG[High Availability]`
    Controls the maximum length in bytes to which the
    *condor_replication* daemon log will be allowed to grow. It will
    grow to the specified length, then be saved to a file with the
    suffix ``.old``. The ``.old`` file is overwritten each time the log
    is saved, thus the maximum space devoted to logging is twice the
    maximum length of this log file. A value of 0 specifies that this
    file may grow without bounds. The default is 1 MiB.

:macro-def:`REPLICATION_DEBUG[High Availability]`
    Logging level for the *condor_replication* daemon. See
    :macro:`<SUBSYS>_DEBUG` for values.

:macro-def:`REPLICATION_LOG[High Availability]`
    Full path and file name to the log file. The default value is
    ``$(LOG)``/ReplicationLog.

:macro-def:`TRANSFERER[High Availability]`
    The full path and file name of the *condor_transferer* executable.
    The default value is ``$(LIBEXEC)``/condor_transferer.

:macro-def:`TRANSFERER_LOG[High Availability]`
    Full path and file name to the log file. The default value is
    ``$(LOG)``/TransfererLog.

:macro-def:`TRANSFERER_DEBUG[High Availability]`
    Logging level for the *condor_transferer* daemon. See
    :macro:`<SUBSYS>_DEBUG` for values.

:macro-def:`MAX_TRANSFERER_LOG[High Availability]`
    Controls the maximum length in bytes to which the
    *condor_transferer* daemon log will be allowed to grow. A value of
    0 specifies that this file may grow without bounds. The default is 1
    MiB.

Configuration File Entries Relating to condor_ssh_to_job
-----------------------------------------------------------

:index:`condor_ssh_to_job configuration variables<single: condor_ssh_to_job configuration variables; configuration>`

These macros affect how HTCondor deals with :tool:`condor_ssh_to_job`, a
tool that allows users to interactively debug jobs. With these
configuration variables, the administrator can control who can use the
tool, and how the *ssh* programs are invoked. The manual page for
:tool:`condor_ssh_to_job` is at :doc:`/man-pages/condor_ssh_to_job`.

:macro-def:`ENABLE_SSH_TO_JOB[SSH_TO_JOB]`
    A boolean expression read by the *condor_starter*, that when
    ``True`` allows the owner of the job or a queue super user on the
    *condor_schedd* where the job was submitted to connect to the job
    via *ssh*. The expression may refer to attributes of both the job
    and the machine ClassAds. The job ClassAd attributes may be
    referenced by using the prefix ``TARGET.``, and the machine ClassAd
    attributes may be referenced by using the prefix ``MY.``. When
    ``False``, it prevents :tool:`condor_ssh_to_job` from starting an *ssh*
    session. The default value is ``True``.

:macro-def:`SCHEDD_ENABLE_SSH_TO_JOB[SSH_TO_JOB]`
    A boolean expression read by the *condor_schedd*, that when
    ``True`` allows the owner of the job or a queue super user to
    connect to the job via *ssh* if the execute machine also allows
    :tool:`condor_ssh_to_job` access (see :macro:`ENABLE_SSH_TO_JOB`). The
    expression may refer to attributes of only the job ClassAd. When
    ``False``, it prevents :tool:`condor_ssh_to_job` from starting an *ssh*
    session for all jobs managed by the *condor_schedd*. The default
    value is ``True``.

:macro-def:`SSH_TO_JOB_<SSH-CLIENT>_CMD[SSH_TO_JOB]`
    A string read by the :tool:`condor_ssh_to_job` tool. It specifies the
    command and arguments to use when invoking the program specified by
    ``<SSH-CLIENT>``. Values substituted for the placeholder
    ``<SSH-CLIENT>`` may be SSH, SFTP, SCP, or any other *ssh* client
    capable of using a command as a proxy for the connection to *sshd*.
    The entire command plus arguments string is enclosed in double quote
    marks. Individual arguments may be quoted with single quotes, using
    the same syntax as for arguments in a :tool:`condor_submit` file. The
    following substitutions are made within the arguments:

        %h: is substituted by the remote host
        %i: is substituted by the ssh key
        %k: is substituted by the known hosts file
        %u: is substituted by the remote user
        %x: is substituted by a proxy command suitable for use with the
        *OpenSSH* ProxyCommand option
        %%: is substituted by the percent mark character

    | The default string is:
    | ``"ssh -oUser=%u -oIdentityFile=%i -oStrictHostKeyChecking=yes -oUserKnownHostsFile=%k -oGlobalKnownHostsFile=%k -oProxyCommand=%x %h"``

    When the ``<SSH-CLIENT>`` is *scp*, %h is omitted.

:macro-def:`SSH_TO_JOB_SSHD[SSH_TO_JOB]`
    The path and executable name of the *ssh* daemon. The value is read
    by the *condor_starter*. The default value is ``/usr/sbin/sshd``.

:macro-def:`SSH_TO_JOB_SSHD_ARGS[SSH_TO_JOB]`
    A string, read by the *condor_starter* that specifies the
    command-line arguments to be passed to the *sshd* to handle an
    incoming ssh connection on its ``stdin`` or ``stdout`` streams in
    inetd mode. Enclose the entire arguments string in double quote
    marks. Individual arguments may be quoted with single quotes, using
    the same syntax as for arguments in an HTCondor submit description
    file. Within the arguments, the characters %f are replaced by the
    path to the *sshd* configuration file the characters %% are replaced
    by a single percent character. The default value is the string
    "-i -e -f %f".

:macro-def:`SSH_TO_JOB_SSHD_CONFIG_TEMPLATE[SSH_TO_JOB]`
    A string, read by the *condor_starter* that specifies the path and
    file name of an *sshd* configuration template file. The template is
    turned into an *sshd* configuration file by replacing macros within
    the template that specify such things as the paths to key files. The
    macro replacement is done by the script
    ``$(LIBEXEC)/condor_ssh_to_job_sshd_setup``. The default value is
    ``$(LIB)/condor_ssh_to_job_sshd_config_template``.

:macro-def:`SSH_TO_JOB_SSH_KEYGEN[SSH_TO_JOB]`
    A string, read by the *condor_starter* that specifies the path to
    *ssh_keygen*, the program used to create ssh keys.

:macro-def:`SSH_TO_JOB_SSH_KEYGEN_ARGS[SSH_TO_JOB]`
    A string, read by the *condor_starter* that specifies the
    command-line arguments to be passed to the *ssh_keygen* to generate
    an ssh key. Enclose the entire arguments string in double quotes.
    Individual arguments may be quoted with single quotes, using the
    same syntax as for arguments in an HTCondor submit description file.
    Within the arguments, the characters %f are replaced by the path to
    the key file to be generated, and the characters %% are replaced by
    a single percent character. The default value is the string
    "-N '' -C '' -q -f %f -t rsa". If the user specifies additional
    arguments with the command condor_ssh_to_job -keygen-options,
    then those arguments are placed after the arguments specified by the
    value of :macro:`SSH_TO_JOB_SSH_KEYGEN_ARGS`.

condor_rooster Configuration File Macros
-----------------------------------------

:index:`condor_rooster configuration variables<single: condor_rooster configuration variables; configuration>`

*condor_rooster* is an optional daemon that may be added to the
:tool:`condor_master` daemon's :macro:`DAEMON_LIST`. It is responsible for waking
up hibernating machines when their :macro:`UNHIBERNATE`
:index:`UNHIBERNATE` expression becomes ``True``. In the typical
case, a pool runs a single instance of *condor_rooster* on the central
manager. However, if the network topology requires that Wake On LAN
packets be sent to specific machines from different locations,
*condor_rooster* can be run on any machine(s) that can read from the
pool's *condor_collector* daemon.

For *condor_rooster* to wake up hibernating machines, the collecting of
offline machine ClassAds must be enabled. See variable
:macro:`COLLECTOR_PERSISTENT_AD_LOG` for details on how to do this.

:macro-def:`ROOSTER_INTERVAL[ROOSTER]`
    The integer number of seconds between checks for offline machines
    that should be woken. The default value is 300.

:macro-def:`ROOSTER_MAX_UNHIBERNATE[ROOSTER]`
    An integer specifying the maximum number of machines to wake up per
    cycle. The default value of 0 means no limit.

:macro-def:`ROOSTER_UNHIBERNATE[ROOSTER]`
    A boolean expression that specifies which machines should be woken
    up. The default expression is ``Offline && Unhibernate``. If network
    topology or other considerations demand that some machines in a pool
    be woken up by one instance of *condor_rooster*, while others be
    woken up by a different instance, :macro:`ROOSTER_UNHIBERNATE` may
    be set locally such that it is different for the two instances of
    *condor_rooster*. In this way, the different instances will only
    try to wake up their respective subset of the pool.

:macro-def:`ROOSTER_UNHIBERNATE_RANK[ROOSTER]`
    A ClassAd expression specifying which machines should be woken up
    first in a given cycle. Higher ranked machines are woken first. If
    the number of machines to be woken up is limited by
    :macro:`ROOSTER_MAX_UNHIBERNATE`, the rank may be used for determining
    which machines are woken before reaching the limit.

:macro-def:`ROOSTER_WAKEUP_CMD[ROOSTER]`
    A string representing the command line invoked by *condor_rooster*
    that is to wake up a machine. The command and any arguments should
    be enclosed in double quote marks, the same as
    :subcom:`arguments` syntax in
    an HTCondor submit description file. The default value is
    "$(BIN)/condor_power -d -i". The command is expected to read from
    its standard input a ClassAd representing the offline machine.

condor_shared_port Configuration File Macros
----------------------------------------------

:index:`condor_shared_port configuration variables<single: condor_shared_port configuration variables; configuration>`

These configuration variables affect the *condor_shared_port* daemon.
For general discussion of the *condor_shared_port* daemon,
see :ref:`admin-manual/networking:reducing port usage with the
*condor_shared_port* daemon`.

:macro-def:`SHARED_PORT`
    The path to the binary of the shared_port daemon.

:macro-def:`USE_SHARED_PORT[SHARED PORT]`
    A boolean value that specifies whether HTCondor daemons should rely
    on the *condor_shared_port* daemon for receiving incoming
    connections. Under Unix, write access to the location defined by
    :macro:`DAEMON_SOCKET_DIR` is required for this to take effect.
    The default is ``True``.

:macro-def:`SHARED_PORT_PORT[SHARED PORT]`
    The default TCP port used by the *condor_shared_port* daemon. If
    :macro:`COLLECTOR_USES_SHARED_PORT` is the default value of ``True``, and
    the :tool:`condor_master` launches a *condor_collector* daemon, then the
    *condor_shared_port* daemon will ignore this value and use the TCP
    port assigned to the *condor_collector* via the :macro:`COLLECTOR_HOST`
    configuration variable.

    The default value is ``$(COLLECTOR_PORT)``, which defaults to 9618.
    Note that this causes all HTCondor hosts to use TCP port 9618 by
    default, differing from previous behavior. The previous behavior has
    only the *condor_collector* host using a fixed port. To restore
    this previous behavior, set :macro:`SHARED_PORT_PORT` to ``0``, which
    will cause the *condor_shared_port* daemon to use a randomly
    selected port in the range :macro:`LOWPORT` - :macro:`HIGHPORT`, as defined in
    :ref:`admin-manual/networking:port usage in htcondor`.

:macro-def:`SHARED_PORT_DAEMON_AD_FILE[SHARED PORT]`
    This specifies the full path and name of a file used to publish the
    address of *condor_shared_port*. This file is read by the other
    daemons that have ``USE_SHARED_PORT=True`` and which are therefore
    sharing the same port. The default typically does not need to be
    changed.

:macro-def:`SHARED_PORT_MAX_WORKERS[SHARED PORT]`
    An integer that specifies the maximum number of sub-processes
    created by *condor_shared_port* while servicing requests to
    connect to the daemons that are sharing the port. The default is 50.

:macro-def:`DAEMON_SOCKET_DIR[SHARED PORT]`
    This specifies the directory where Unix versions of HTCondor daemons
    will create named sockets so that incoming connections can be
    forwarded to them by *condor_shared_port*. If this directory does
    not exist, it will be created. The maximum length of named socket
    paths plus names is restricted by the operating system, so using a
    path that is longer than 90 characters may cause failures.

    Write access to this directory grants permission to receive
    connections through the shared port. By default, the directory is
    created to be owned by HTCondor and is made to be only writable by
    HTCondor. One possible reason to broaden access to this directory is
    if execute nodes are accessed via CCB and the submit node is behind
    a firewall with only one open port, which is the port assigned to
    *condor_shared_port*. In this case, commands that interact with
    the execute node, such as :tool:`condor_ssh_to_job`, will not be able
    to operate unless run by a user with write access to
    :macro:`DAEMON_SOCKET_DIR`. In this case, one could grant tmp-like
    permissions to this directory so that all users can receive CCB
    connections back through the firewall. But, consider the wisdom of
    having a firewall in the first place, if it will be circumvented in
    this way.

    On Linux platforms, daemons use abstract named sockets instead of
    normal named sockets. Abstract sockets are not not tied to a file in
    the file system. The :tool:`condor_master` picks a random prefix for
    abstract socket names and shares it privately with the other
    daemons. When searching for the recipient of an incoming connection,
    *condor_shared_port* will check for both an abstract socket and a
    named socket in the directory indicated by this variable. The named
    socket allows command-line tools such as :tool:`condor_ssh_to_job` to
    use *condor_shared_port* as described.

    On Linux platforms, setting :macro:`SHARED_PORT_AUDIT_LOG` causes HTCondor
    to log the following information about each connection made through the
    :macro:`DAEMON_SOCKET_DIR`: the source address, the socket file name, and
    the target process's PID, UID, GID, executable path, and command
    line. An administrator may use this logged information to deter
    abuse.

    The default value is ``auto``, causing the use of the directory
    ``$(LOCK)/daemon_sock``. On Unix platforms other than Linux, if that
    path is longer than the 90 characters maximum, then the
    :tool:`condor_master` will instead create a directory under ``/tmp`` with
    a name that looks like ``/tmp/condor_shared_port_<XXXXXX>``, where
    ``<XXXXXX>`` is replaced with random characters. The
    :tool:`condor_master` then tells the other daemons the exact name of the
    directory it created, and they use it.

    If a different value is set for :macro:`DAEMON_SOCKET_DIR`, then that
    directory is used, without regard for the length of the path name.
    Ensure that the length is not longer than 90 characters.

:macro-def:`SHARED_PORT_ARGS[SHARED PORT]`
    Like all daemons started by the :tool:`condor_master` daemon, the command
    line arguments to the invocation of the *condor_shared_port*
    daemon can be customized. The arguments can be used to specify a
    non-default port number for the *condor_shared_port* daemon as in
    this example, which specifies port 4080:

    .. code-block:: condor-config

          SHARED_PORT_ARGS = -p 4080

    It is recommended to use configuration variable :macro:`SHARED_PORT_PORT`
    to set a non-default port number, instead of using this
    configuration variable.

:macro-def:`SHARED_PORT_AUDIT_LOG[SHARED PORT]`
    On Linux platforms, the path and file name of the
    *condor_shared_port* log that records connections made via the
    :macro:`DAEMON_SOCKET_DIR`. If not defined, there will be no
    *condor_shared_port* audit log.

:macro-def:`MAX_SHARED_PORT_AUDIT_LOG[SHARED PORT]`
    On Linux platforms, controls the maximum amount of time that the
    *condor_shared_port* audit log will be allowed to grow. When it is
    time to rotate a log file, the log file will be saved to a file
    named with an ISO timestamp suffix. The oldest rotated file receives
    the file name suffix ``.old``. The ``.old`` files are overwritten
    each time the maximum number of rotated files (determined by the
    value of :macro:`MAX_NUM_SHARED_PORT_AUDIT_LOG`) is exceeded. A value of
    0 specifies that the file may grow without bounds. The following
    suffixes may be used to qualify the integer:

        ``Sec`` for seconds
        ``Min`` for minutes
        ``Hr`` for hours
        ``Day`` for days
        ``Wk`` for weeks

:macro-def:`MAX_NUM_SHARED_PORT_AUDIT_LOG[SHARED PORT]`
    On Linux platforms, the integer that controls the maximum number of
    rotations that the *condor_shared_port* audit log is allowed to
    perform, before the oldest one will be rotated away. The default
    value is 1.

Configuration File Entries Relating to Job Hooks
------------------------------------------------

:index:`job hook configuration variables<single: job hook configuration variables; configuration>`
:index:`Job Router`

These macros control the various hooks that interact with HTCondor.
Currently, there are two independent sets of hooks. One is a set of
fetch work hooks, some of which are invoked by the *condor_startd* to
optionally fetch work, and some are invoked by the *condor_starter*.
See :ref:`admin-manual/ep-policy-configuration:job hooks that fetch work` for more
details. The other set replace functionality of the
*condor_job_router* daemon. Documentation for the
*condor_job_router* daemon is in
:doc:`/grid-computing/job-router`.

:macro-def:`SLOT<N>_JOB_HOOK_KEYWORD[HOOKS]`
    For the fetch work hooks, the keyword used to define which set of
    hooks a particular compute slot should invoke. The value of <N> is
    replaced by the slot identification number. For example, on slot 1,
    the variable name will be called ``SLOT1_JOB_HOOK_KEYWORD``. There
    is no default keyword. Sites that wish to use these job hooks must
    explicitly define the keyword and the corresponding hook paths.

:macro-def:`STARTD_JOB_HOOK_KEYWORD[HOOKS]`
    For the fetch work hooks, the keyword used to define which set of
    hooks a particular *condor_startd* should invoke. This setting is
    only used if a slot-specific keyword is not defined for a given
    compute slot. There is no default keyword. Sites that wish to use
    job hooks must explicitly define the keyword and the corresponding
    hook paths.

:macro-def:`STARTER_DEFAULT_JOB_HOOK_KEYWORD[HOOKS]`
    A string valued parameter that defaults to empty.  If the job
    does not define a hook, or defines an invalid one, this
    can be used to force a default hook for the job.

:macro-def:`STARTER_JOB_HOOK_KEYWORD[HOOKS]`
    This can be defined to force the *condor_starter* to always use a 
    given keyword for its own hooks, regardless of the value in the 
    job ClassAd for the :ad-attr:`HookKeyword` attribute.¬

:macro-def:`<Keyword>_HOOK_FETCH_WORK[HOOKS]`
    For the fetch work hooks, the full path to the program to invoke
    whenever the *condor_startd* wants to fetch work. ``<Keyword>`` is
    the hook keyword defined to distinguish between sets of hooks. There
    is no default.

:macro-def:`<Keyword>_HOOK_REPLY_FETCH[HOOKS]`
    For the fetch work hooks, the full path to the program to invoke
    when the hook defined by :macro:`<Keyword>_HOOK_FETCH_WORK` returns data
    and then the *condor_startd* decides if it is going to accept the
    fetched job or not. ``<Keyword>`` is the hook keyword defined to
    distinguish between sets of hooks.

:macro-def:`<Keyword>_HOOK_REPLY_CLAIM[HOOKS]`
    For the fetch work hooks, the full path to the program to invoke
    whenever the *condor_startd* finishes fetching a job and decides
    what to do with it. ``<Keyword>`` is the hook keyword defined to
    distinguish between sets of hooks. There is no default.

:macro-def:`<Keyword>_HOOK_PREPARE_JOB[HOOKS]`
    For the fetch work hooks, the full path to the program invoked by
    the *condor_starter* before it runs the job. ``<Keyword>`` is the
    hook keyword defined to distinguish between sets of hooks.

:macro-def:`<Keyword>_HOOK_PREPARE_JOB_ARGS[HOOKS]`
    The arguments for the *condor_starter* to use when invoking the
    prepare job hook specified by ``<Keyword>``.

:macro-def:`<Keyword>_HOOK_UPDATE_JOB_INFO[HOOKS]`
    This configuration variable is used by both fetch work hooks and by
    *condor_job_router* hooks.

    For the fetch work hooks, the full path to the program invoked by
    the *condor_starter* periodically as the job runs, allowing the
    *condor_starter* to present an updated and augmented job ClassAd to
    the program. See :ref:`admin-manual/ep-policy-configuration:job hooks that fetch work` for
    the list of additional attributes included. When the job is first invoked,
    the *condor_starter* will invoke the program after
    ``$(STARTER_INITIAL_UPDATE_INTERVAL)`` seconds. Thereafter, the
    *condor_starter* will invoke the program every
    ``$(STARTER_UPDATE_INTERVAL)`` seconds. ``<Keyword>`` is the hook
    keyword defined to distinguish between sets of hooks.

    As a Job Router hook, the full path to the program invoked when the
    Job Router polls the status of routed jobs at intervals set by
    :macro:`JOB_ROUTER_POLLING_PERIOD`. ``<Keyword>`` is the hook keyword
    defined by :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the hooks.

:macro-def:`<Keyword>_HOOK_EVICT_CLAIM[HOOKS]`
    For the fetch work hooks, the full path to the program to invoke
    whenever the *condor_startd* needs to evict a fetched claim.
    ``<Keyword>`` is the hook keyword defined to distinguish between
    sets of hooks. There is no default.

:macro-def:`<Keyword>_HOOK_JOB_EXIT[HOOKS]`
    For the fetch work hooks, the full path to the program invoked by
    the *condor_starter* whenever a job exits, either on its own or
    when being evicted from an execution slot. ``<Keyword>`` is the hook
    keyword defined to distinguish between sets of hooks.

:macro-def:`<Keyword>_HOOK_JOB_EXIT_TIMEOUT[HOOKS]`
    For the fetch work hooks, the number of seconds the
    *condor_starter* will wait for the hook defined by
    :macro:`<Keyword>_HOOK_JOB_EXIT` hook to exit, before continuing with job
    clean up. Defaults to 30 seconds. ``<Keyword>`` is the hook keyword
    defined to distinguish between sets of hooks.

:macro-def:`FetchWorkDelay[HOOKS]`
    An expression that defines the number of seconds that the
    *condor_startd* should wait after an invocation of
    :macro:`<Keyword>_HOOK_FETCH_WORK` completes before the hook
    should be invoked again. The expression is evaluated in the context
    of the slot ClassAd, and the ClassAd of the currently running job
    (if any). The expression must evaluate to an integer. If not
    defined, the *condor_startd* will wait 300 seconds (five minutes)
    between attempts to fetch work. For more information about this
    expression, see :ref:`admin-manual/ep-policy-configuration:job hooks that fetch work`.

:macro-def:`JOB_ROUTER_HOOK_KEYWORD[HOOKS]`
    For the Job Router hooks, the keyword used to define the set of
    hooks the *condor_job_router* is to invoke to replace
    functionality of routing translation. There is no default keyword.
    Use of these hooks requires the explicit definition of the keyword
    and the corresponding hook paths.

:macro-def:`<Keyword>_HOOK_TRANSLATE_JOB[HOOKS]`
    A Job Router hook, the full path to the program invoked when the Job
    Router has determined that a job meets the definition for a route.
    This hook is responsible for doing the transformation of the job.
    ``<Keyword>`` is the hook keyword defined by
    :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the hooks.

:macro-def:`<Keyword>_HOOK_JOB_FINALIZE[HOOKS]`
    A Job Router hook, the full path to the program invoked when the Job
    Router has determined that the job completed. ``<Keyword>`` is the
    hook keyword defined by :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the
    hooks.

:macro-def:`<Keyword>_HOOK_JOB_CLEANUP[HOOKS]`
    A Job Router hook, the full path to the program invoked when the Job
    Router finishes managing the job. ``<Keyword>`` is the hook keyword
    defined by :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the hooks.

Configuration File Entries Relating to Daemon ClassAd Hooks: Startd Cron and Schedd Cron
----------------------------------------------------------------------------------------

:index:`daemon ClassAd hook configuration variables<single: daemon ClassAd hook configuration variables; configuration>`

The following macros describe the daemon ClassAd hooks which run
startd cron and schedd cron.  These run executables or scripts
directly from the *condor_startd* and *condor_schedd*
daemons.  The output is merged into the 
ClassAd generated by the respective daemon.  The mechanism is described
in :ref:`admin-manual/ep-policy-configuration:Startd Cron`.

These macros all include ``CRON`` because the default mode for a daemon 
ClassAd hook is to run periodically.  A specific daemon ClassAd 
hook is called a ``JOB``.

To define a job:

* Add a ``JobName`` to :macro:`STARTD_CRON_JOBLIST`.  (If
  you want to define a benchmark, or a daemon ClassAd hook in the schedd,
  use ``BENCHMARK`` or ``SCHEDD`` in the macro name instead.)  A
  ``JobName`` identifies a specific job and must be unique.  In the rest of
  this section, where ``<JobName>`` appears in a macro name, it means to
  replace ``<JobName>`` with one of the names :macro:`STARTD_CRON_JOBLIST`.

* You must set :macro:`STARTD_CRON_<JobName>_EXECUTABLE`,
  and you'll probably want to set :macro:`STARTD_CRON_<JobName>_ARGS` as well.
  These macros tell HTCondor how to actually run the job.

* You must also decide when your job will run.  By default, a job runs
  every :macro:`STARTD_CRON_<JobName>_PERIOD` seconds after the daemon
  starts up.  You may set :macro:`STARTD_CRON_<JobName>_MODE` to change
  to this to continuously (``WaitForExit``); on start-up (``OneShot``)
  and optionally, when the daemon is reconfigured; or as a benchmark
  (``OnDemand``).  If you do not select ``OneShot``, you must set
  :macro:`STARTD_CRON_<JobName>_PERIOD`.

All the other job-specific macros are optional, of which
:macro:`STARTD_CRON_<JobName>_ENV` and :macro:`STARTD_CRON_<JobName>_CWD`
are probably the most common.

:macro-def:`STARTD_CRON_AUTOPUBLISH[HOOKS]`
    Optional setting that determines if the *condor_startd* should
    automatically publish a new update to the *condor_collector* after
    any of the jobs produce output. Beware that enabling this setting
    can greatly increase the network traffic in an HTCondor pool,
    especially when many modules are executed, or if the period in which
    they run is short. There are three possible (case insensitive)
    values for this variable:

     ``Never``
        This default value causes the *condor_startd* to not
        automatically publish updates based on any jobs. Instead,
        updates rely on the usual behavior for sending updates, which is
        periodic, based on the :macro:`UPDATE_INTERVAL` configuration
        variable, or whenever a given slot changes state.
     ``Always``
        Causes the *condor_startd* to always send a new update to the
        *condor_collector* whenever any job exits.
     ``If_Changed``
        Causes the *condor_startd* to only send a new update to the
        *condor_collector* if the output produced by a given job is
        different than the previous output of the same job. The only
        exception is the ``LastUpdate`` attribute, which is
        automatically set for all jobs to be the timestamp when the job
        last ran. It is ignored when :macro:`STARTD_CRON_AUTOPUBLISH` is set
        to ``If_Changed``.

:macro-def:`STARTD_CRON_CONFIG_VAL[HOOKS]`, :macro-def:`SCHEDD_CRON_CONFIG_VAL[HOOKS]`, and :macro-def:`BENCHMARKS_CONFIG_VAL[HOOKS]`
    This configuration variable can be used to specify the path and
    executable name of the :tool:`condor_config_val` program which the jobs
    (hooks) should use to get configuration information from the daemon.
    If defined, an environment variable by the same name with the same
    value will be passed to all jobs.

:macro-def:`STARTD_CRON_JOBLIST[HOOKS]`, :macro-def:`SCHEDD_CRON_JOBLIST[HOOKS]`, and :macro-def:`BENCHMARKS_JOBLIST[HOOKS]`
    These configuration variables are defined by a comma and/or white
    space separated list of job names to run. Each is the logical name
    of a job. This name must be unique; no two jobs may have the same
    name. The *condor_startd* reads this configuration variable on startup
    and on reconfig.  The *condor_schedd* reads this variable and other 
    ``SCHEDD_CRON_*`` variables only on startup.

:macro-def:`STARTD_CRON_MAX_JOB_LOAD[HOOKS]`, :macro-def:`SCHEDD_CRON_MAX_JOB_LOAD[HOOKS]`, and :macro-def:`BENCHMARKS_MAX_JOB_LOAD[HOOKS]`
    A floating point value representing a threshold for CPU load, such
    that if starting another job would cause the sum of assumed loads
    for all running jobs to exceed this value, no further jobs will be
    started. The default value for ``STARTD_CRON`` or a ``SCHEDD_CRON``
    hook managers is 0.1. This implies that a maximum of 10 jobs (using
    their default, assumed load) could be concurrently running. The
    default value for the ``BENCHMARKS`` hook manager is 1.0. This
    implies that only 1 ``BENCHMARKS`` job (at the default, assumed
    load) may be running.

:macro-def:`STARTD_CRON_LOG_NON_ZERO_EXIT[HOOKS]` and :macro-def:`SCHEDD_CRON_LOG_NON_ZERO_EXIT[HOOKS]`
    If true, each time a cron job returns a non-zero exit code, the
    corresponding daemon will log the cron job's exit code and output.  There
    is no default value, so no logging will occur by default.

:macro-def:`STARTD_CRON_<JobName>_ARGS[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_ARGS[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_ARGS[HOOKS]`
    The command line arguments to pass to the job as it is invoked. The
    first argument will be ``<JobName>``.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_CONDITION[HOOKS]`
    A ClassAd expression evaluated each time the job might otherwise
    be started.  If this macro is set, but the expression does not evaluate to
    ``True``, the job will not be started.  The expression is evaluated in
    a context similar to a slot ad, but without any slot-specific attributes.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`.

:macro-def:`STARTD_CRON_<JobName>_CWD[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_CWD[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_CWD[HOOKS]`
    The working directory in which to start the job.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_ENV[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_ENV[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_ENV[HOOKS]`
    The environment string to pass to the job. The syntax is the same as
    that of :macro:`<DaemonName>_ENVIRONMENT` as defined at
    :ref:`admin-manual/configuration-macros:condor_master configuration file macros`.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_EXECUTABLE[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_EXECUTABLE[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_EXECUTABLE[HOOKS]`
    The full path and executable to run for this job. Note that multiple
    jobs may specify the same executable, although the jobs need to have
    different logical names.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_JOB_LOAD[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_JOB_LOAD[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_JOB_LOAD[HOOKS]`
    A floating point value that represents the assumed and therefore
    expected CPU load that a job induces on the system. This job load is
    then used to limit the total number of jobs that run concurrently,
    by not starting new jobs if the assumed total load from all jobs is
    over a set threshold. The default value for each individual
    ``STARTD_CRON`` or a ``SCHEDD_CRON`` job is 0.01. The default value
    for each individual ``BENCHMARKS`` job is 1.0.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_KILL[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_KILL[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_KILL[HOOKS]`
    A boolean value applicable only for jobs with a ``MODE`` of anything
    other than ``WaitForExit``. The default value is ``False``.

    This variable controls the behavior of the daemon hook manager when
    it detects that an instance of the job's executable is still running
    as it is time to invoke the job again. If ``True``, the daemon hook
    manager will kill the currently running job and then invoke an new
    instance of the job. If ``False``, the existing job invocation is
    allowed to continue running.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_METRICS[HOOKS]`
    A space or comma -separated list. Each element in the list is a
    metric type, either ``SUM`` or ``PEAK``; a colon; and a metric name.

    An attribute preceded by ``SUM`` is a metric which accumulates over
    time. The canonical example is seconds of CPU usage.

    An attribute preceded by ``PEAK`` is a metric which instead records
    the largest value reported over the period of use. The canonical
    example is megabytes of memory usage.

    A job with :macro:`STARTD_CRON_<JobName>_METRICS` set is a custom machine
    resource monitor (CMRM), and its output is handled differently than
    a normal job's. A CMRM should output one ad per custom machine
    resource instance and use ``SlotMergeConstraint``\ s (see
    :ref:`admin-manual/ep-policy-configuration:Startd Cron`) to specify the instance to
    which it applies.

    The ad corresponding to each custom machine resource instance should
    have an attribute for each metric named in the configuration. For
    SUM metrics, the attribute should be ``Uptime<MetricName>Seconds``;
    for PEAK metrics, the attribute should be
    ``Uptime<MetricName>PeakUsage``.

    Each value should be the value of the metric since the last time the
    job reported. The reported value may therefore go up or down;
    HTCondor will record either the sum or the peak value, as
    appropriate, for the duration of the job running in a slot assigned
    resources of the corresponding type.

    For example, if your custom resources are SQUIDs, and you detected
    four of them, your monitor might output the following:

    .. code-block:: text

        SlotMergeConstraint = StringListMember( "SQUID0", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 5.0
        UptimeSQUIDsMemoryPeakUsage = 50
        - SQUIDsReport0
        SlotMergeConstraint = StringListMember( "SQUID1", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 1.0
        UptimeSQUIDsMemoryPeakUsage = 10
        - SQUIDsReport1
        SlotMergeConstraint = StringListMember( "SQUID2", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 9.0
        UptimeSQUIDsMemoryPeakUsage = 90
        - SQUIDsReport2
        SlotMergeConstraint = StringListMember( "SQUID3", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 4.0
        UptimeSQUIDsMemoryPeakUsage = 40
        - SQUIDsReport3

    The names ('SQUIDsReport0') may be anything, but must be consistent
    from report to report and the ClassAd for each report must have a
    distinct name.

    You might specify the monitor in the example above as follows:

    .. code-block:: condor-config

        MACHINE_RESOURCE_INVENTORY_SQUIDs = /usr/local/bin/cmr-squid-discovery

        STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) SQUIDs_MONITOR
        STARTD_CRON_SQUIDs_MONITOR_MODE = Periodic
        STARTD_CRON_SQUIDs_MONITOR_PERIOD = 10
        STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE = /usr/local/bin/cmr-squid-monitor
        STARTD_CRON_SQUIDs_MONITOR_METRICS = SUM:SQUIDs, PEAK:SQUIDsMemory

:macro-def:`STARTD_CRON_<JobName>_MODE[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_MODE[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_MODE[HOOKS]`
    A string that specifies a mode within which the job operates. Legal
    values are

    -  ``Periodic``, which is the default.
    -  ``WaitForExit``
    -  ``OneShot``
    -  ``OnDemand``

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

    The default ``Periodic`` mode is used for most jobs. In this mode,
    the job is expected to be started by the *condor_startd* daemon,
    gather and publish its data, and then exit.

    In ``WaitForExit`` mode the *condor_startd* daemon interprets the
    period as defined by :macro:`STARTD_CRON_<JobName>_PERIOD` differently.
    In this case, it refers to the amount of time to wait after the job
    exits before restarting it. With a value of 1, the job is kept
    running nearly continuously. In general, ``WaitForExit`` mode is for
    jobs that produce a periodic stream of updated data, but it can be
    used for other purposes, as well. The output data from the job is
    accumulated into a temporary ClassAd until the job exits or until it
    writes a line starting with dash (-) character. At that point, the
    temporary ClassAd replaces the active ClassAd for the job. The
    active ClassAd for the job is merged into the appropriate slot
    ClassAds whenever the slot ClassAds are published.

    The ``OneShot`` mode is used for jobs that are run once at the start
    of the daemon. If the ``reconfig_rerun`` option is specified, the
    job will be run again after any reconfiguration.

    The ``OnDemand`` mode is used only by the ``BENCHMARKS`` mechanism.
    All benchmark jobs must be be ``OnDemand`` jobs. Any other jobs
    specified as ``OnDemand`` will never run. Additional future features
    may allow for other ``OnDemand`` job uses.

:macro-def:`STARTD_CRON_<JobName>_PERIOD[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_PERIOD[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_PERIOD[HOOKS]`
    The period specifies time intervals at which the job should be run.
    For periodic jobs, this is the time interval that passes between
    starting the execution of the job. The value may be specified in
    seconds, minutes, or hours. Specify this time by appending the
    character ``s``, ``m``, or ``h`` to the value. As an example, 5m
    starts the execution of the job every five minutes. If no character
    is appended to the value, seconds are used as a default. In
    ``WaitForExit`` mode, the value has a different meaning: the period
    specifies the length of time after the job ceases execution and
    before it is restarted. The minimum valid value of the period is 1
    second.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_PREFIX[HOOKS]`, :macro-def:`SCHEDD_CRON_<JobName>_PREFIX[HOOKS]`, and :macro-def:`BENCHMARKS_<JobName>_PREFIX[HOOKS]`
    Specifies a string which is prepended by HTCondor to all attribute
    names that the job generates. The use of prefixes avoids the
    conflicts that would be caused by attributes of the same name
    generated and utilized by different jobs. For example, if a module
    prefix is ``xyz_``, and an individual attribute is named ``abc``, then the
    resulting attribute name will be ``xyz_abc``. Due to restrictions on
    ClassAd names, a prefix is only permitted to contain alpha-numeric
    characters and the underscore character.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_RECONFIG[HOOKS]` and :macro-def:`SCHEDD_CRON_<JobName>_RECONFIG[HOOKS]`
    A boolean value that when ``True``, causes the daemon to send an HUP
    signal to the job when the daemon is reconfigured. The job is
    expected to reread its configuration at that time.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST` or
    ``SCHEDD_CRON_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_RECONFIG_RERUN[HOOKS]` and :macro-def:`SCHEDD_CRON_<JobName>_RECONFIG_RERUN[HOOKS]`
    A boolean value that when ``True``, causes the daemon ClassAd hook
    mechanism to re-run the specified job when the daemon is
    reconfigured via :tool:`condor_reconfig`. The default value is ``False``.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST` or
    ``SCHEDD_CRON_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_SLOTS[HOOKS]` and :macro-def:`BENCHMARKS_<JobName>_SLOTS[HOOKS]`
    Only the slots specified in this comma-separated list may
    incorporate the output of the job specified by ``<JobName>``. If the
    list is not specified, any slot may. Whether or not a specific slot
    actually incorporates the output depends on the output; see
    :ref:`admin-manual/ep-policy-configuration:Startd Cron`.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST` or
    ``BENCHMARKS_JOBLIST``.


Configuration File Entries Only for Windows Platforms
-----------------------------------------------------

:index:`Windows platform configuration variables<single: Windows platform configuration variables; configuration>`

These macros are utilized only on Windows platforms.

:macro-def:`WINDOWS_RMDIR[WINDOWS]`
    The complete path and executable name of the HTCondor version of the
    built-in *rmdir* program. The HTCondor version will not fail when
    the directory contains files that have ACLs that deny the SYSTEM
    process delete access. If not defined, the built-in Windows *rmdir*
    program is invoked, and a value defined for :macro:`WINDOWS_RMDIR_OPTIONS`
    is ignored.

:macro-def:`WINDOWS_RMDIR_OPTIONS[WINDOWS]`
    Command line options to be specified when configuration variable
    :macro:`WINDOWS_RMDIR` is defined. Defaults to **/S** **/C** when
    configuration variable :macro:`WINDOWS_RMDIR` is defined and its
    definition contains the string ``"condor_rmdir.exe"``.

condor_defrag Configuration File Macros
----------------------------------------

:index:`condor_defrag configuration variables<single: condor_defrag configuration variables; configuration>`

These configuration variables affect the *condor_defrag* daemon. A
general discussion of *condor_defrag* may be found in
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`.

:macro-def:`DEFRAG_NAME[DEFRAG]`
    Used to give an prefix value to the ``Name`` attribute in the
    *condor_defrag* daemon's ClassAd. Defaults to the name given in the :macro:`DAEMON_LIST`.
    This esoteric configuration macro
    might be used in the situation where there are two *condor_defrag*
    daemons running on one machine, and each reports to the same
    *condor_collector*. Different names will distinguish the two
    daemons. See the description of :macro:`MASTER_NAME` in
    :ref:`admin-manual/configuration-macros:condor_master configuration file macros`
    for defaults and composition of valid HTCondor daemon names.

:macro-def:`DEFRAG_DRAINING_MACHINES_PER_HOUR[DEFRAG]`
    A floating point number that specifies how many machines should be
    drained per hour. The default is 0, so no draining will happen
    unless this setting is changed. Each *condor_startd* is considered
    to be one machine. The actual number of machines drained per hour
    may be less than this if draining is halted by one of the other
    defragmentation policy controls. The granularity in timing of
    draining initiation is controlled by :macro:`DEFRAG_INTERVAL`.
    The lowest rate of draining that is supported is one machine per
    day or one machine per :macro:`DEFRAG_INTERVAL`, whichever is
    lower. A fractional number of machines contributing to the value of
    :macro:`DEFRAG_DRAINING_MACHINES_PER_HOUR` is rounded to the nearest
    whole number of machines on a per day basis.

:macro-def:`DEFRAG_DRAINING_START_EXPR[DEFRAG]`
    A ClassAd expression that replaces the machine's :macro:`START`
    expression while it's draining. Slots which
    accepted a job after the machine began draining set the machine ad
    attribute :ad-attr:`AcceptedWhileDraining` to ``true``. When the last job
    which was not accepted while draining exits, all other jobs are
    immediately evicted with a ``MaxJobRetirementTime`` of 0; job vacate
    times are still respected. While the jobs which were accepted while
    draining are vacating, the :macro:`START` expression is ``false``. Using
    ``$(START)`` in this expression is usually a mistake: it will be
    replaced by the defrag daemon's :macro:`START` expression, not the value
    of the target machine's :macro:`START` expression (and especially not the
    value of its :macro:`START` expression at the time draining begins).

:macro-def:`DEFRAG_REQUIREMENTS[DEFRAG]`
    An expression that narrows the selection of which machines to drain.
    By default *condor_defrag* will drain all machines that are drainable.
    A machine, meaning a *condor_startd*, is matched if any of its
    partitionable slots match this expression. Machines are automatically excluded if
    they cannot be drained, are already draining, or if they match
    :macro:`DEFRAG_WHOLE_MACHINE_EXPR`.

    The *condor_defrag* daemon will always add the following requirements to :macro:`DEFRAG_REQUIREMENTS`

    .. code-block:: condor-classad-expr

          PartitionableSlot && Offline =!= true && Draining =!= true

:macro-def:`DEFRAG_CANCEL_REQUIREMENTS[DEFRAG]`
    An expression that is periodically evaluated against machines that are draining.
    When this expression evaluates to ``True``, draining will be cancelled.
    This defaults to 
    ``$(DEFRAG_WHOLE_MACHINE_EXPR)``\ :index:`DEFRAG_WHOLE_MACHINE_EXPR`.
    This could be used to drain partial rather than whole machines.
    Beginning with version 8.9.11, only machines that have no ``DrainReason``
    or a value of ``"Defrag"`` for ``DrainReason``
    will be checked to see if draining should be cancelled.  Beginning with
    10.7.0 The Defrag daemon will also check for its own name in the ``DrainReason``.

:macro-def:`DEFRAG_RANK[DEFRAG]`
    An expression that specifies which machines are more desirable to
    drain. The expression should evaluate to a number for each candidate
    machine to be drained. If the number of machines to be drained is
    less than the number of candidates, the machines with higher rank
    will be chosen. The rank of a machine, meaning a *condor_startd*,
    is the rank of its highest ranked slot. The default rank is
    ``-ExpectedMachineGracefulDrainingBadput``.

:macro-def:`DEFRAG_WHOLE_MACHINE_EXPR[DEFRAG]`
    An expression that specifies which machines are already operating as
    whole machines. The default is

    .. code-block:: condor-classad-expr

          Cpus == TotalSlotCpus

    A machine is matched if any Partitionable slot on the machine matches this
    expression and the machine is not draining or was drained by *condor_defrag*.
    Each *condor_startd* is considered to be one machine.
    Whole machines are excluded when selecting machines to drain. They
    are also counted against :macro:`DEFRAG_MAX_WHOLE_MACHINES`.

:macro-def:`DEFRAG_MAX_WHOLE_MACHINES[DEFRAG]`
    An integer that specifies the maximum number of whole machines. When
    the number of whole machines is greater than or equal to this, no
    new machines will be selected for draining. Each *condor_startd* is
    counted as one machine. The special value -1 indicates that there is
    no limit. The default is -1.

:macro-def:`DEFRAG_MAX_CONCURRENT_DRAINING[DEFRAG]`
    An integer that specifies the maximum number of draining machines.
    When the number of machines that are draining is greater than or
    equal to this, no new machines will be selected for draining. Each
    draining *condor_startd* is counted as one machine. The special
    value -1 indicates that there is no limit. The default is -1.

:macro-def:`DEFRAG_INTERVAL[DEFRAG]`
    An integer that specifies the number of seconds between evaluations
    of the defragmentation policy. In each cycle, the state of the pool
    is observed and machines are drained, if specified by the policy.
    The default is 600 seconds. Very small intervals could create
    excessive load on the *condor_collector*.

:macro-def:`DEFRAG_UPDATE_INTERVAL[DEFRAG]`
    An integer that specifies the number of seconds between times that
    the *condor_defrag* daemon sends updates to the collector. (See
    :ref:`classad-attributes/defrag-classad-attributes:defrag classad attributes`
    for information about the attributes in these updates.) The default is
    300 seconds.

:macro-def:`DEFRAG_SCHEDULE[DEFRAG]`
    A setting that specifies the draining schedule to use when draining
    machines. Possible values are ``graceful``, ``quick``, and ``fast``.
    The default is ``graceful``.

     graceful
        Initiate a graceful eviction of the job. This means all promises
        that have been made to the job are honored, including
        ``MaxJobRetirementTime``. The eviction of jobs is coordinated to
        reduce idle time. This means that if one slot has a job with a
        long retirement time and the other slots have jobs with shorter
        retirement times, the effective retirement time for all of the
        jobs is the longer one.
     quick
        ``MaxJobRetirementTime`` is not honored. Eviction of jobs is
        immediately initiated. Jobs are given time to shut down
        according to the usual policy, as given by
        :macro:`MachineMaxVacateTime`.
     fast
        Jobs are immediately hard-killed, with no chance to gracefully
        shut down.

:macro-def:`DEFRAG_LOG[DEFRAG]`
    The path to the *condor_defrag* daemon's log file. The default log
    location is ``$(LOG)/DefragLog``.

*condor_gangliad* Configuration File Macros
--------------------------------------------

:index:`condor_gangliad configuration variables<single: condor_gangliad configuration variables; configuration>`
:index:`condor_gangliad daemon`

*condor_gangliad* is an optional daemon responsible for publishing
information about HTCondor daemons to the Ganglia\ :sup:`™` monitoring
system. The Ganglia monitoring system must be installed and configured
separately. In the typical case, a single instance of the
*condor_gangliad* daemon is run per pool. A default set of metrics are
sent. Additional metrics may be defined, in order to publish any
information available in ClassAds that the *condor_collector* daemon
has.

:macro-def:`GANGLIAD_INTERVAL[GANGLIAD]`
    The integer number of seconds between consecutive sending of metrics
    to Ganglia. Daemons update the *condor_collector* every 300
    seconds, and the Ganglia heartbeat interval is 20 seconds.
    Therefore, multiples of 20 between 20 and 300 makes sense for this
    value. Negative values inhibit sending data to Ganglia. The default
    value is 60.

:macro-def:`GANGLIAD_MIN_METRIC_LIFETIME[GANGLIAD]`
    An integer value representing the minimum DMAX value for all metrics.
    Where DMAX is the number number of seconds without updating that
    a metric will be kept before deletion. This value defaults to ``86400``
    which is equivalent to 1 day. This value will be overridden by a
    specific metric defined ``Lifetime`` value.

:macro-def:`GANGLIAD_VERBOSITY[GANGLIAD]`
    An integer that specifies the maximum verbosity level of metrics to
    be published to Ganglia. Basic metrics have a verbosity level of 0,
    which is the default. Additional metrics can be enabled by
    increasing the verbosity to 1. In the default configuration, there
    are no metrics with verbosity levels higher than 1. Some metrics
    depend on attributes that are not published to the *condor_collector*
    when using the default value of :macro:`STATISTICS_TO_PUBLISH`. For
    example, per-user file transfer statistics will only be published to
    Ganglia if :macro:`GANGLIAD_VERBOSITY` is set to 1 or higher in the
    *condor_gangliad* configuration and :macro:`STATISTICS_TO_PUBLISH` in
    the *condor_schedd* configuration contains ``TRANSFER:2``, or if
    the :macro:`STATISTICS_TO_PUBLISH_LIST` contains the desired attributes
    explicitly.

:macro-def:`GANGLIAD_REQUIREMENTS[GANGLIAD]`
    An optional boolean ClassAd expression that may restrict the set of
    daemon ClassAds to be monitored. This could be used to monitor a
    subset of a pool's daemons or machines. The default is an empty
    expression, which has the effect of placing no restriction on the
    monitored ClassAds. Keep in mind that this expression is applied to
    all types of monitored ClassAds, not just machine ClassAds.

:macro-def:`GANGLIAD_PER_EXECUTE_NODE_METRICS[GANGLIAD]`
    A boolean value that, when ``False``, causes metrics from execute
    node daemons to not be published. Aggregate values from these
    machines will still be published. The default value is ``True``.
    This option is useful for pools such that use glidein, in which it
    is not desired to record metrics for individual execute nodes.

:macro-def:`GANGLIAD_WANT_PROJECTION[GANGLIAD]`
    A boolean value that, when ``True``, causes the *condor_gangliad* to
    use an attribute projection when querying the collector whenever possible.
    This significantly reduces the memory consumption of the *condor_gangliad*, and also
    places less load on the *condor_collector*.
    The default value is currently ``False``; it is expected this default will
    be changed to ``True`` in a future release after additional testing.

:macro-def:`GANGLIAD_WANT_RESET_METRICS[GANGLIAD]`
    A boolean value that, when ``True``, causes aggregate numeric metrics
    to be reset to a value of zero when they are no longer being updated.
    The default value is ``False``, causing aggregate metrics published to
    Ganglia to retain the last value published indefinitely.

:macro-def:`GANGLIAD_RESET_METRICS_FILE[GANGLIAD]`
    The file name where persistent data will
    be stored if ``GANGLIAD_WANT_RESET_METRICS`` is set to ``True``. 
    If not set to a fully qualified path, the file will be stored in the 
    SPOOL directory with a filename extension of ``.ganglia_metrics``.
    If you are running multiple *condor_gangliad* instances
    that share a SPOOL directory, this knob should be customized. 
    The default is ``$(SPOOL)/metricsToReset.ganglia_metrics``.

:macro-def:`GANGLIA_CONFIG[GANGLIAD]`
    The path and file name of the Ganglia configuration file. The
    default is ``/etc/ganglia/gmond.conf``.

:macro-def:`GANGLIA_GMETRIC[GANGLIAD]`
    The full path of the *gmetric* executable to use. If none is
    specified, ``libganglia`` will be used instead when possible,
    because the library interface is more efficient than invoking
    *gmetric*. Some versions of ``libganglia`` are not compatible. When
    a failure to use ``libganglia`` is detected, *gmetric* will be used,
    if *gmetric* can be found in HTCondor's ``PATH`` environment
    variable.

:macro-def:`GANGLIA_GSTAT_COMMAND[GANGLIAD]`
    The full *gstat* command used to determine which hosts are monitored
    by Ganglia. For a *condor_gangliad* running on a host whose local
    *gmond* does not know the list of monitored hosts, change
    ``localhost`` to be the appropriate host name or IP address within
    this default string:

    .. code-block:: text

          gstat --all --mpifile --gmond_ip=localhost --gmond_port=8649

:macro-def:`GANGLIA_SEND_DATA_FOR_ALL_HOSTS[GANGLIAD]`
    A boolean value that when ``True`` causes data to be sent to Ganglia
    for hosts that it is not currently monitoring. The default is
    ``False``.

:macro-def:`GANGLIA_LIB[GANGLIAD]`
    The full path and file name of the ``libganglia`` shared library to
    use. If none is specified, and if configuration variable
    :macro:`GANGLIA_GMETRIC` is also not specified, then a search for
    ``libganglia`` will be performed in the directories listed in
    configuration variable :macro:`GANGLIA_LIB_PATH` or
    :macro:`GANGLIA_LIB64_PATH`. The special value ``NOOP``
    indicates that *condor_gangliad* should not publish statistics to
    Ganglia, but should otherwise go through all the motions it normally
    does.

:macro-def:`GANGLIA_LIB_PATH[GANGLIAD]`
    A comma-separated list of directories within which to search for the
    ``libganglia`` executable, if :macro:`GANGLIA_LIB` is not configured.
    This is used in 32-bit versions of HTCondor.

:macro-def:`GANGLIA_LIB64_PATH[GANGLIAD]`
    A comma-separated list of directories within which to search for the
    ``libganglia`` executable, if :macro:`GANGLIA_LIB` is not configured.
    This is used in 64-bit versions of HTCondor.

:macro-def:`GANGLIAD_DEFAULT_CLUSTER[GANGLIAD]`
    An expression specifying the default name of the Ganglia cluster for
    all metrics. The expression may refer to attributes of the machine.

:macro-def:`GANGLIAD_DEFAULT_MACHINE[GANGLIAD]`
    An expression specifying the default machine name of Ganglia
    metrics. The expression may refer to attributes of the machine.

:macro-def:`GANGLIAD_DEFAULT_IP[GANGLIAD]`
    An expression specifying the default IP address of Ganglia metrics.
    The expression may refer to attributes of the machine.

:macro-def:`GANGLIAD_LOG[GANGLIAD]`
    The path and file name of the *condor_gangliad* daemon's log file.
    The default log is ``$(LOG)/GangliadLog``.

:macro-def:`GANGLIAD_METRICS_CONFIG_DIR[GANGLIAD]`
    Path to the directory containing files which define Ganglia metrics
    in terms of HTCondor ClassAd attributes to be published. All files
    in this directory are read, to define the metrics. The default
    directory ``/etc/condor/ganglia.d/`` is used when not specified.

condor_annex Configuration File Macros
--------------------------------------

:index:`condor_annex configuration variables<single: condor_annex configuration variables; configuration>`

See :doc:`/cloud-computing/annex-configuration` for :tool:`condor_annex`
configuration file macros.

``htcondor annex`` Configuration File Macros
--------------------------------------------
:index:`htcondor annex configuration variables<single: htcondor annex configuration variables; configuration>`

:macro-def:`HPC_ANNEX_ENABLED`
    If true, users will have access to the ``annex`` noun of the
    :doc:`../man-pages/htcondor` command.

    .. warning::

        This does not configure the AP so that ``htcondor annex``
        will *work*.  Configuring an AP for ``htcondor annex`` is
        tricky, and we recommend that you add :macro:`use feature:HPC_ANNEX`
        instead, which sets this macro.

    Defaults to false.
