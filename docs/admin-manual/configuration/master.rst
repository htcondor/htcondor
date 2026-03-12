:index:`Master Daemon Options<single: Configuration; Master Daemon Options>`

.. _master_config_options:

Master Daemon Configuration Options
===================================

These macros control the :tool:`condor_master`.

:macro-def:`DAEMON_LIST`
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

:macro-def:`DC_DAEMON_LIST`
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

:macro-def:`<SUBSYS>`
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

:macro-def:`<DaemonName>_ENVIRONMENT`
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

:macro-def:`<SUBSYS>_ARGS`
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

:macro-def:`<SUBSYS>_USERID`
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

:macro-def:`PREEN`
    In addition to the daemons defined in ``$(DAEMON_LIST)``, the
    :tool:`condor_master` also starts up a special process, :tool:`condor_preen`
    to clean out junk files that have been left laying around by
    HTCondor. This macro determines where the :tool:`condor_master` finds the
    :tool:`condor_preen` binary. If this macro is set to nothing,
    :tool:`condor_preen` will not run.

:macro-def:`PREEN_ARGS`
    Controls how :tool:`condor_preen` behaves by allowing the specification
    of command-line arguments. This macro works as ``$(<SUBSYS>_ARGS)``
    does. The difference is that you must specify this macro for
    :tool:`condor_preen` if you want it to do anything. :tool:`condor_preen` takes
    action only because of command line arguments. **-m** means you want
    e-mail about files :tool:`condor_preen` finds that it thinks it should
    remove. **-r** means you want :tool:`condor_preen` to actually remove
    these files.

:macro-def:`PREEN_INTERVAL`
    This macro determines how often :tool:`condor_preen` should be started.
    It is defined in terms of seconds and defaults to 86400 (once a
    day).

:macro-def:`PUBLISH_OBITUARIES`
    When a daemon crashes, the :tool:`condor_master` can send e-mail to the
    address specified by ``$(CONDOR_ADMIN)`` with an obituary letting
    the administrator know that the daemon died, the cause of death
    (which signal or exit status it exited with), and (optionally) the
    last few entries from that daemon's log file. If you want
    obituaries, set this macro to ``True``.

:macro-def:`OBITUARY_LOG_LENGTH`
    This macro controls how many lines of the log file are part of
    obituaries. This macro has a default value of 20 lines.

:macro-def:`START_MASTER`
    If this setting is defined and set to ``False`` the :tool:`condor_master`
    will immediately exit upon startup. This appears strange, but
    perhaps you do not want HTCondor to run on certain machines in your
    pool, yet the boot scripts for your entire pool are handled by a
    centralized set of files - setting :macro:`START_MASTER` to ``False`` for
    those machines would allow this. Note that :macro:`START_MASTER` is an
    entry you would most likely find in a local configuration file, not
    a global configuration file. If not defined, :macro:`START_MASTER`
    defaults to ``True``.

:macro-def:`START_DAEMONS`
    This macro is similar to the ``$(START_MASTER)`` macro described
    above. However, the :tool:`condor_master` does not exit; it does not
    start any of the daemons listed in the ``$(DAEMON_LIST)``. The
    daemons may be started at a later time with a :tool:`condor_on` command.

:macro-def:`MASTER_UPDATE_INTERVAL`
    This macro determines how often the :tool:`condor_master` sends a ClassAd
    update to the *condor_collector*. It is defined in seconds and
    defaults to 300 (every 5 minutes).

:macro-def:`MASTER_CHECK_NEW_EXEC_INTERVAL`
    This macro controls how often the :tool:`condor_master` checks the
    timestamps of the running daemons. If any daemons have been
    modified, the master restarts them. It is defined in seconds and
    defaults to 300 (every 5 minutes).

:macro-def:`MASTER_NEW_BINARY_RESTART`
    Defines a mode of operation for the restart of the :tool:`condor_master`,
    when it notices that the :tool:`condor_master` binary has changed. Valid
    values are ``FAST``, ``GRACEFUL``, ``PEACEFUL``, and ``NEVER``, with a default
    value of ``GRACEFUL``. On a ``GRACEFUL`` restart of the master,
    child processes are told to exit, but if they do not before a timer
    expires, then they are killed. On a ``PEACEFUL`` restart, child
    processes are told to exit, after which the :tool:`condor_master` waits
    until they do so.

:macro-def:`MASTER_NEW_BINARY_DELAY`
    Once the :tool:`condor_master` has discovered a new binary, this macro
    controls how long it waits before attempting to execute the new
    binary. This delay exists because the :tool:`condor_master` might notice
    a new binary while it is in the process of being copied, in which
    case trying to execute it yields unpredictable results. The entry is
    defined in seconds and defaults to 120 (2 minutes).

:macro-def:`SHUTDOWN_FAST_TIMEOUT`
    This macro determines the maximum amount of time daemons are given
    to perform their fast shutdown procedure before the :tool:`condor_master`
    kills them outright. It is defined in seconds and defaults to 300 (5
    minutes).

:macro-def:`DEFAULT_MASTER_SHUTDOWN_SCRIPT`
    A full path and file name of a program that the :tool:`condor_master` is
    to execute via the Unix execl() call, or the similar Win32 _execl()
    call, instead of the normal call to exit(). This allows the admin to
    specify a program to execute as root when the :tool:`condor_master`
    exits. Note that a successful call to the :tool:`condor_set_shutdown`
    program will override this setting; see the documentation for config
    knob :macro:`MASTER_SHUTDOWN_<Name>` below.

:macro-def:`MASTER_SHUTDOWN_<Name>`
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

:macro-def:`MASTER_BACKOFF_CONSTANT`
    .. faux-definition::

:macro-def:`MASTER_<name>_BACKOFF_CONSTANT`
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

:macro-def:`MASTER_BACKOFF_FACTOR`
    .. faux-definition::

:macro-def:`MASTER_<name>_BACKOFF_FACTOR`
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

:macro-def:`MASTER_BACKOFF_CEILING`
    .. faux-definition::

:macro-def:`MASTER_<name>_BACKOFF_CEILING`
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

:macro-def:`MASTER_RECOVER_FACTOR`
    .. faux-definition::

:macro-def:`MASTER_<name>_RECOVER_FACTOR`
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

:macro-def:`MASTER_NAME`
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

:macro-def:`MASTER_ATTRS`
    This macro is described in :macro:`<SUBSYS>_ATTRS`.

:macro-def:`MASTER_DEBUG`
    This macro is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`MASTER_ADDRESS_FILE`
    This macro is described in :macro:`<SUBSYS>_ADDRESS_FILE`.

:macro-def:`ALLOW_ADMIN_COMMANDS`
    If set to NO for a given host, this macro disables administrative
    commands, such as :tool:`condor_restart`, :tool:`condor_on`, and
    :tool:`condor_off`, to that host.

:macro-def:`MASTER_INSTANCE_LOCK`
    Defines the name of a file for the :tool:`condor_master` daemon to lock
    in order to prevent multiple :tool:`condor_master` s from starting. This
    is useful when using shared file systems like NFS which do not
    technically support locking in the case where the lock files reside
    on a local disk. If this macro is not defined, the default file name
    will be ``$(LOCK)/InstanceLock``. ``$(LOCK)`` can instead be defined
    to specify the location of all lock files, not just the
    :tool:`condor_master` 's ``InstanceLock``. If ``$(LOCK)`` is undefined,
    then the master log itself is locked.

:macro-def:`ADD_WINDOWS_FIREWALL_EXCEPTION`
    When set to ``False``, the :tool:`condor_master` will not automatically
    add HTCondor to the Windows Firewall list of trusted applications.
    Such trusted applications can accept incoming connections without
    interference from the firewall. This only affects machines running
    Windows XP SP2 or higher. The default is ``True``.

:macro-def:`WINDOWS_FIREWALL_FAILURE_RETRY`
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

:macro-def:`USE_PROCESS_GROUPS`
    A boolean value that defaults to ``True``. When ``False``, HTCondor
    daemons on Unix machines will not create new sessions or process
    groups. HTCondor uses processes groups to help it track the
    descendants of processes it creates. This can cause problems when
    HTCondor is run under another job execution system.

:macro-def:`CGROUP_ALL_DAEMONS`
    A boolean that default to false.  When true, each daemon will
    be put into its own cgroup. This knob requires a restart to take
    effect.

:macro-def:`DISCARD_SESSION_KEYRING_ON_STARTUP`
    A boolean value that defaults to ``True``. When ``True``, the
    :tool:`condor_master` daemon will replace the kernel session keyring it
    was invoked with a new keyring named ``htcondor``. Various
    Linux system services, such as OpenAFS and eCryptFS, use the kernel
    session keyring to hold passwords and authentication tokens. By
    replacing the keyring on start up, the :tool:`condor_master` ensures
    these keys cannot be unintentionally obtained by user jobs.

:macro-def:`ENABLE_KERNEL_TUNING`
    Relevant only to Linux platforms, a boolean value that defaults to
    ``True``. When ``True``, the :tool:`condor_master` daemon invokes the
    kernel tuning script specified by configuration variable
    :macro:`LINUX_KERNEL_TUNING_SCRIPT` once as root when the
    :tool:`condor_master` daemon starts up.

:macro-def:`KERNEL_TUNING_LOG`
    A string value that defaults to ``$(LOG)/KernelTuningLog``. If the
    kernel tuning script runs, its output will be logged to this file.

:macro-def:`LINUX_KERNEL_TUNING_SCRIPT`
    A string value that defaults to ``$(LIBEXEC)/linux_kernel_tuning``.
    This is the script that the :tool:`condor_master` runs to tune the kernel
    when :macro:`ENABLE_KERNEL_TUNING` is ``True``.
