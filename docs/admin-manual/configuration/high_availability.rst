:index:`High Availability Options<single: Configuration; High Availability Options>`

.. _high_availability_config_options:

High Availability Configuration Options
=======================================

These macros affect the high availability operation of HTCondor.

:macro-def:`MASTER_HA_LIST`
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

:macro-def:`HA_LOCK_URL`
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

:macro-def:`HA_<SUBSYS>_LOCK_URL`
    This macro controls the High Availability lock URL for a specific
    subsystem as specified in the configuration variable name, and it
    overrides the system-wide lock URL specified by :macro:`HA_LOCK_URL`. If
    not defined for each subsystem, :macro:`HA_<SUBSYS>_LOCK_URL` is ignored,
    and the value of :macro:`HA_LOCK_URL` is used.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`HA_LOCK_HOLD_TIME`
    This macro specifies the number of seconds that the :tool:`condor_master`
    will hold the lock for each High Availability daemon. Upon gaining
    the shared lock, the :tool:`condor_master` will hold the lock for this
    number of seconds. Additionally, the :tool:`condor_master` will
    periodically renew each lock as long as the :tool:`condor_master` and the
    daemon are running. When the daemon dies, or the :tool:`condor_master`
    exists, the :tool:`condor_master` will immediately release the lock(s) it
    holds.

    :macro:`HA_LOCK_HOLD_TIME` defaults to 3600 seconds (one hour).

:macro-def:`HA_<SUBSYS>_LOCK_HOLD_TIME`
    This macro controls the High Availability lock hold time for a
    specific subsystem as specified in the configuration variable name,
    and it overrides the system wide poll period specified by
    :macro:`HA_LOCK_HOLD_TIME`. If not defined for each subsystem,
    :macro:`HA_<SUBSYS>_LOCK_HOLD_TIME` is ignored, and the value of
    :macro:`HA_LOCK_HOLD_TIME` is used.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`HA_POLL_PERIOD`
    This macro specifies how often the :tool:`condor_master` polls the High
    Availability locks to see if any locks are either stale (meaning not
    updated for :macro:`HA_LOCK_HOLD_TIME` seconds), or have been released by
    the owning :tool:`condor_master`. Additionally, the :tool:`condor_master`
    renews any locks that it holds during these polls.

    :macro:`HA_POLL_PERIOD` defaults to 300 seconds (five minutes).

:macro-def:`HA_<SUBSYS>_POLL_PERIOD`
    This macro controls the High Availability poll period for a specific
    subsystem as specified in the configuration variable name, and it
    overrides the system wide poll period specified by
    :macro:`HA_POLL_PERIOD`. If not defined for each subsystem,
    :macro:`HA_<SUBSYS>_POLL_PERIOD` is ignored, and the value of
    :macro:`HA_POLL_PERIOD` is used.

    List of possible subsystems to set :macro:`<SUBSYS>` can be found at :macro:`SUBSYSTEM`.

:macro-def:`MASTER_<SUBSYS>_CONTROLLER`
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

:macro-def:`HAD_LIST`
    A comma-separated list of all *condor_had* daemons in the form
    ``IP:port`` or ``hostname:port``. Each central manager machine that
    runs the *condor_had* daemon should appear in this list. If
    :macro:`HAD_USE_PRIMARY` is set to ``True``, then the first machine in
    this list is the primary central manager, and all others in the list
    are backups.

    All central manager machines must be configured with an identical
    :macro:`HAD_LIST`. The machine addresses are identical to the addresses
    defined in :macro:`COLLECTOR_HOST`.

:macro-def:`HAD_USE_PRIMARY`
    Boolean value to determine if the first machine in the :macro:`HAD_LIST`
    configuration variable is a primary central manager. Defaults to
    ``False``.

:macro-def:`HAD_CONTROLLEE`
    This variable is used to specify the name of the daemon which the
    *condor_had* daemon controls. This name should match the daemon
    name in the :tool:`condor_master` daemon's :macro:`DAEMON_LIST` definition.
    The default value is ``NEGOTIATOR``.

:macro-def:`HAD_CONNECTION_TIMEOUT`
    The time (in seconds) that the *condor_had* daemon waits before
    giving up on the establishment of a TCP connection. The failure of
    the communication connection is the detection mechanism for the
    failure of a central manager machine. For a LAN, a recommended value
    is 2 seconds. The use of authentication (by HTCondor) increases the
    connection time. The default value is 5 seconds. If this value is
    set too low, *condor_had* daemons will incorrectly assume the
    failure of other machines.

:macro-def:`HAD_ARGS`
    Command line arguments passed by the :tool:`condor_master` daemon as it
    invokes the *condor_had* daemon. To make high availability work,
    the *condor_had* daemon requires the port number it is to use. This
    argument is of the form

    .. code-block:: text

           -p $(HAD_PORT_NUMBER)


    where ``HAD_PORT_NUMBER`` is a helper configuration variable defined
    with the desired port number. Note that this port number must be the
    same value here as used in :macro:`HAD_LIST`. There is no default value.

:macro-def:`HAD`
    The path to the *condor_had* executable. Normally it is defined
    relative to ``$(SBIN)``. This configuration variable has no default
    value.

:macro-def:`MAX_HAD_LOG`
    Controls the maximum length in bytes to which the *condor_had*
    daemon log will be allowed to grow. It will grow to the specified
    length, then be saved to a file with the suffix ``.old``. The
    ``.old`` file is overwritten each time the log is saved, thus the
    maximum space devoted to logging is twice the maximum length of this
    log file. A value of 0 specifies that this file may grow without
    bounds. The default is 1 MiB.

:macro-def:`HAD_DEBUG`
    Logging level for the *condor_had* daemon. See :macro:`<SUBSYS>_DEBUG`
    for values.

:macro-def:`HAD_LOG`
    Full path and file name of the log file. The default value is
    ``$(LOG)``/HADLog.

:macro-def:`HAD_FIPS_MODE`
    Controls what type of checksum will be sent along with files that are replicated.
    Set it to 0 for MD5 checksums and to 1 for SHA-2 checksums.
    Prior to versions 8.8.13 and 8.9.12 only MD5 checksums are supported. In the 10.0 and
    later release of HTCondor, MD5 support will be removed and only SHA-2 will be
    supported.  This configuration variable is intended to provide a transition
    between the 8.8 and 9.0 releases.  Once all machines in your pool involved in HAD replication
    have been upgraded to 9.0 or later, you should set the value of this configuration
    variable to 1. Default value is 0 in HTCondor versions before 9.12 and 1 in version 9.12 and later.

:macro-def:`REPLICATION_LIST`
    A comma-separated list of all *condor_replication* daemons in the
    form ``IP:port`` or ``hostname:port``. Each central manager machine
    that runs the *condor_had* daemon should appear in this list. All
    potential central manager machines must be configured with an
    identical :macro:`REPLICATION_LIST`.

:macro-def:`STATE_FILE`
    A full path and file name of the file protected by the replication
    mechanism. When not defined, the default path and file used is

    .. code-block:: console

          $(SPOOL)/Accountantnew.log


:macro-def:`REPLICATION_INTERVAL`
    Sets how often the *condor_replication* daemon initiates its tasks
    of replicating the ``$(STATE_FILE)``. It is defined in seconds and
    defaults to 300 (5 minutes).

:macro-def:`MAX_TRANSFER_LIFETIME`
    A timeout period within which the process that transfers the state
    file must complete its transfer. The recommended value is
    ``2 * average size of state file / network rate``. It is defined in
    seconds and defaults to 300 (5 minutes).

:macro-def:`HAD_UPDATE_INTERVAL`
    Like :macro:`UPDATE_INTERVAL`, determines how often the *condor_had* is
    to send a ClassAd update to the *condor_collector*. Updates are
    also sent at each and every change in state. It is defined in
    seconds and defaults to 300 (5 minutes).

:macro-def:`HAD_USE_REPLICATION`
    A boolean value that defaults to ``False``. When ``True``, the use
    of *condor_replication* daemons is enabled.

:macro-def:`REPLICATION_ARGS`
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

:macro-def:`REPLICATION`
    The full path and file name of the *condor_replication* executable.
    It is normally defined relative to ``$(SBIN)``. There is no default
    value.

:macro-def:`MAX_REPLICATION_LOG`
    Controls the maximum length in bytes to which the
    *condor_replication* daemon log will be allowed to grow. It will
    grow to the specified length, then be saved to a file with the
    suffix ``.old``. The ``.old`` file is overwritten each time the log
    is saved, thus the maximum space devoted to logging is twice the
    maximum length of this log file. A value of 0 specifies that this
    file may grow without bounds. The default is 1 MiB.

:macro-def:`REPLICATION_DEBUG`
    Logging level for the *condor_replication* daemon. See
    :macro:`<SUBSYS>_DEBUG` for values.

:macro-def:`REPLICATION_LOG`
    Full path and file name to the log file. The default value is
    ``$(LOG)``/ReplicationLog.

:macro-def:`TRANSFERER`
    The full path and file name of the *condor_transferer* executable.
    The default value is ``$(LIBEXEC)``/condor_transferer.

:macro-def:`TRANSFERER_LOG`
    Full path and file name to the log file. The default value is
    ``$(LOG)``/TransfererLog.

:macro-def:`TRANSFERER_DEBUG`
    Logging level for the *condor_transferer* daemon. See
    :macro:`<SUBSYS>_DEBUG` for values.

:macro-def:`MAX_TRANSFERER_LOG`
    Controls the maximum length in bytes to which the
    *condor_transferer* daemon log will be allowed to grow. A value of
    0 specifies that this file may grow without bounds. The default is 1
    MiB.
