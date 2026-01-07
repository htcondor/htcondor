:index:`Shared Port Daemon Options<single: Configuration; Shared Port Daemon Options>`

.. _shared_port_config_options:

Shared Port Daemon Configuration Options
========================================

These configuration variables affect the *condor_shared_port* daemon.
For general discussion of the *condor_shared_port* daemon,
see :ref:`admin-manual/networking:reducing port usage with the
*condor_shared_port* daemon`.

:macro-def:`SHARED_PORT`
    The path to the binary of the shared_port daemon.

:macro-def:`USE_SHARED_PORT`
    A boolean value that specifies whether HTCondor daemons should rely
    on the *condor_shared_port* daemon for receiving incoming
    connections. Under Unix, write access to the location defined by
    :macro:`DAEMON_SOCKET_DIR` is required for this to take effect.
    The default is ``True``.

:macro-def:`SHARED_PORT_PORT`
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

:macro-def:`SHARED_PORT_DAEMON_AD_FILE`
    This specifies the full path and name of a file used to publish the
    address of *condor_shared_port*. This file is read by the other
    daemons that have ``USE_SHARED_PORT=True`` and which are therefore
    sharing the same port. The default typically does not need to be
    changed.

:macro-def:`SHARED_PORT_MAX_WORKERS`
    An integer that specifies the maximum number of sub-processes
    created by *condor_shared_port* while servicing requests to
    connect to the daemons that are sharing the port. The default is 50.

:macro-def:`DAEMON_SOCKET_DIR`
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

:macro-def:`SHARED_PORT_ARGS`
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

:macro-def:`SHARED_PORT_AUDIT_LOG`
    On Linux platforms, the path and file name of the
    *condor_shared_port* log that records connections made via the
    :macro:`DAEMON_SOCKET_DIR`. If not defined, there will be no
    *condor_shared_port* audit log.

:macro-def:`MAX_SHARED_PORT_AUDIT_LOG`
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

:macro-def:`MAX_NUM_SHARED_PORT_AUDIT_LOG`
    On Linux platforms, the integer that controls the maximum number of
    rotations that the *condor_shared_port* audit log is allowed to
    perform, before the oldest one will be rotated away. The default
    value is 1.
