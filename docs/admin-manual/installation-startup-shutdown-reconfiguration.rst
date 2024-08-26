Starting Up, Shutting Down and  Reconfiguring the System
========================================================

If you installed HTCondor with administrative privileges, HTCondor will
start up when the machine boots and shut down when the machine does, using
the usual mechanism for the machine's operating system.  You can generally
use those mechanisms in the usual way if you need to manually control
whether or not HTCondor is running.  There are two situations in
which you might want to run :doc:`../man-pages/condor_master`,
:doc:`../man-pages/condor_on`, or :doc:`../man-pages/condor_off` from the
command line.

#. If you installed HTCondor without administrative privileges, you'll
   have to run :tool:`condor_master` from the command line to turn on HTCondor:

    .. code-block:: console

        $ condor_master

   Then run the following command to turn HTCondor completely off:

    .. code-block:: console

        $ condor_off -master

#. If the usual OS-specific method of controlling HTCondor is inconvenient
   to use remotely, you may be able to use the :tool:`condor_on` and :tool:`condor_off`
   tools instead.

Daemons That Do Not Run as root
-------------------------------

:index:`running as root`
:index:`running as root<single: running as root; daemon>`

HTCondor is normally installed such that the HTCondor daemons have root
permission. This allows HTCondor to run the *condor_shadow*
daemon and the job with the submitting user's UID and file access
rights. When HTCondor is started as root, HTCondor jobs can access
whatever files the user that submits the jobs can.

However, it is possible that the HTCondor installation does not have
root access, or has decided not to run the daemons as root. That is
unfortunate, since HTCondor is designed to be run as root. To see if
HTCondor is running as root on a specific machine, use the command

.. code-block:: console

      $ condor_status -master -l <machine-name>

where <machine-name> is the name of the specified machine. This command
displays the full condor_master ClassAd; if the attribute :ad-attr:`RealUid`
equals zero, then the HTCondor daemons are indeed running with root
access. If the :ad-attr:`RealUid` attribute is not zero, then the HTCondor
daemons do not have root access.

.. note::

   The Unix program *ps* is not an effective method of determining if HTCondor is
   running with root access. When using *ps*, it may often appear that the daemons
   are running as the condor user instead of root.  However, note that the *ps*
   command shows the current effective owner of the process, not the real owner.
   (See the *getuid* (2) and *geteuid* (2) Unix man pages for details.) In Unix, a
   process running under the real UID of root may switch its effective UID. (See
   the *seteuid* (2) man page.) For security reasons, the daemons only set the
   effective UID to root when absolutely necessary, as it will be to perform a
   privileged operation.

If daemons are not running with root access, make any and all files
and/or directories that the job will touch readable and/or writable by
the UID (user id) specified by the :ad-attr:`RealUid` attribute. Often this may
mean using the Unix command chmod 777 on the directory from which the
HTCondor job is submitted.

Remote Management Features
--------------------------

:index:`shutting down HTCondor<single: shutting down HTCondor; pool management>`
:index:`restarting HTCondor<single: restarting HTCondor; pool management>`

All of the commands described in this section are subject to the
security policy chosen for the HTCondor pool.  As such, the commands must
be either run from a machine that has the proper authorization, or run
by a user that is authorized to issue the commands.
The :doc:`/admin-manual/security` section details the
implementation of security in HTCondor.

 Shutting Down HTCondor
    There are a variety of ways to shut down all or parts of an HTCondor
    pool. All utilize the :tool:`condor_off` tool.

    To stop a single execute machine from running jobs, the
    :tool:`condor_off` command specifies the machine by host name.

    .. code-block:: console

        $ condor_off -startd <hostname>

    Jobs will be killed. If it is instead desired that the machine
    stops running jobs only after the currently executing job completes,
    the command is

    .. code-block:: console

        $ condor_off -startd -peaceful <hostname>

    Note that this waits indefinitely for the running job to finish,
    before the *condor_startd* daemon exits.

    Th shut down all execution machines within the pool,

    .. code-block:: console

        $ condor_off -all -startd

    To wait indefinitely for each machine in the pool to finish its
    current HTCondor job, shutting down all of the execute machines as
    they no longer have a running job,

    .. code-block:: console

        $ condor_off -all -startd -peaceful

    To shut down HTCondor on a machine from which jobs are submitted,

    .. code-block:: console

        $ condor_off -schedd <hostname>

    If it is instead desired that the access point (which runs the
    *condor_schedd*) shuts down only after all jobs that are currently in the
    queue are finished, first disable new submissions to the queue by setting
    the configuration variable

    .. code-block:: condor-config

        MAX_JOBS_SUBMITTED = 0

    See instructions below in :ref:`Reconfiguring an HTCondor Pool <reconfiguring>`
    for how to reconfigure a pool. After the reconfiguration,
    the command to wait for all jobs to complete and shut down the submission of
    jobs is

    .. code-block:: console

        $ condor_off -schedd -peaceful <hostname>

    Substitute the option **-all** for the host name, if all submit
    machines in the pool are to be shut down.

 Restarting HTCondor, If HTCondor Daemons Are Not Running
    If HTCondor is not running, perhaps because one of the :tool:`condor_off`
    commands was used, then starting HTCondor daemons back up depends on
    which part of HTCondor is currently not running.

    If no HTCondor daemons are running, then starting HTCondor is a
    matter of executing the :tool:`condor_master` daemon. The
    :tool:`condor_master` daemon will then invoke all other specified daemons
    on that machine. The :tool:`condor_master` daemon executes on every
    machine that is to run HTCondor.

    If a specific daemon needs to be started up, and the
    :tool:`condor_master` daemon is already running, then issue the command
    on the specific machine with

    .. code-block:: console

        $ condor_on -subsystem <subsystemname>

    where <subsystemname> is replaced by the daemon's subsystem name.
    Or, this command might be issued from another machine in the pool
    (which has administrative authority) with

    .. code-block:: console

        $ condor_on <hostname> -subsystem <subsystemname>

    where <subsystemname> is replaced by the daemon's subsystem name,
    and <hostname> is replaced by the host name of the machine where
    this :tool:`condor_on` command is to be directed.

 Restarting HTCondor, If HTCondor Daemons Are Running
    If HTCondor daemons are currently running, but need to be killed and
    newly invoked, the :tool:`condor_restart` tool does this. This would be
    the case for a new value of a configuration variable for which using
    :tool:`condor_reconfig` is inadequate.

    To restart all daemons on all machines in the pool,

    .. code-block:: console

        $ condor_restart -all

    To restart all daemons on a single machine in the pool,

    .. code-block:: console

        $ condor_restart <hostname>

    where <hostname> is replaced by the host name of the machine to be
    restarted.

.. _reconfiguring:

 Reconfiguring an HTCondor Pool
    :index:`reconfiguration<single: reconfiguration; pool management>`

    To change a global configuration variable and have all the machines
    start to use the new setting, change the value within the file, and send
    a :tool:`condor_reconfig` command to each host. Do this with a single
    command,

    .. code-block:: console

      $ condor_reconfig -all

    If the global configuration file is not shared among all the machines,
    as it will be if using a shared file system, the change must be made to
    each copy of the global configuration file before issuing the
    :tool:`condor_reconfig` command.

    Issuing a :tool:`condor_reconfig` command is inadequate for some
    configuration variables. For those, a restart of HTCondor is required.
    Those configuration variables that require a restart are listed in
    the :ref:`admin-manual/introduction-to-configuration:macros that will require a
    restart when changed` section.  You can also refer to the
    :doc:`/man-pages/condor_restart` manual page.

DaemonCore
----------

:index:`daemoncore`
:index:`shared functionality in daemons<single: shared functionality in daemons; HTCondor>`

This section is a brief description of DaemonCore. DaemonCore is a
library that is shared among most of the HTCondor daemons which provides
common functionality. Currently, the following daemons use DaemonCore:

-  *condor_master*
-  *condor_startd*
-  *condor_schedd*
-  *condor_collector*
-  *condor_negotiator*
-  *condor_kbdd*
-  *condor_gridmanager*
-  *condor_credd*
-  *condor_had*
-  *condor_replication*
-  *condor_transferer*
-  *condor_job_router*
-  *condor_lease_manager*
-  *condor_rooster*
-  *condor_shared_port*
-  *condor_defrag*
-  *condor_c-gahp*
-  *condor_c-gahp_worker_thread*
-  *condor_dagman*
-  *condor_ft-gahp*
-  *condor_rooster*
-  *condor_shadow*
-  *condor_shared_port*
-  *condor_transferd*
-  *condor_vm-gahp*

Most of DaemonCore's details are not interesting for administrators.
However, DaemonCore does provide a uniform interface for the daemons to
various Unix signals, and provides a common set of command-line options
that can be used to start up each daemon.

DaemonCore and Unix signals
'''''''''''''''''''''''''''

:index:`Unix signals<single: Unix signals; daemoncore>`

One of the most visible features that DaemonCore provides for
administrators is that all daemons which use it behave the same way on
certain Unix signals. The signals and the behavior DaemonCore provides
are listed below:

SIGHUP
    Causes the daemon to reconfigure itself.
SIGTERM
    Causes the daemon to gracefully shutdown.
SIGQUIT
    Causes the daemon to quickly shutdown.

Exactly what gracefully and quickly means varies from daemon to daemon.
For daemons with little or no state (the *condor_kbdd*,
*condor_collector* and *condor_negotiator*) there is no difference,
and both ``SIGTERM`` and ``SIGQUIT`` signals result in the daemon
shutting itself down quickly. For the :tool:`condor_master`, a graceful
shutdown causes the :tool:`condor_master` to ask all of its children to
perform their own graceful shutdown methods. The quick shutdown causes
the :tool:`condor_master` to ask all of its children to perform their own
quick shutdown methods. In both cases, the :tool:`condor_master` exits after
all its children have exited. In the *condor_startd*, if the machine is
not claimed and running a job, both the ``SIGTERM`` and ``SIGQUIT``
signals result in an immediate exit. In the *condor_schedd*, if
there are no jobs currently running, there will be no *condor_shadow*
processes, and both signals result in an immediate exit. However, with
jobs running, a graceful shutdown causes the *condor_schedd* to ask
each *condor_shadow* to gracefully vacate the job it is serving, while
a quick shutdown results in a hard kill of every *condor_shadow*.

For all daemons, a reconfigure results in the daemon re-reading its
configuration file(s), causing any settings that have changed to take
effect. See the :doc:`/admin-manual/introduction-to-configuration` section for
full details on what settings are in the configuration files and what they do.

DaemonCore and Command-line Arguments
'''''''''''''''''''''''''''''''''''''

:index:`command line arguments<single: command line arguments; daemoncore>`
:index:`command line arguments<single: command line arguments; HTCondor daemon>`

The second visible feature that DaemonCore provides to administrators is
a common set of command-line arguments that all daemons understand.
These arguments and what they do are described below:

\-a string
    Append a period character ('.') concatenated with **string** to the
    file name of the log for this daemon, as specified in the
    configuration file.

\-b
    Causes the daemon to start up in the background. When a DaemonCore
    process starts up with this option, it disassociates itself from the
    terminal and forks itself, so that it runs in the background. This
    is the default behavior for the :tool:`condor_master`. Prior to 8.9.7 it
    was the default for all HTCondor daemons.

\-c filename
    Causes the daemon to use the specified **filename** as a full path
    and file name as its global configuration file. This overrides the
    ``CONDOR_CONFIG`` environment variable and the regular locations
    that HTCondor checks for its configuration file.

\-d
    Use dynamic directories. The ``$(LOG)``, ``$(SPOOL)``, and
    ``$(EXECUTE)`` directories are all created by the daemon at run
    time, and they are named by appending the parent's IP address and
    PID to the value in the configuration file. These values are then
    inherited by all children of the daemon invoked with this **-d**
    argument. For the :tool:`condor_master`, all HTCondor processes will use
    the new directories. If a *condor_schedd* is invoked with the *-d*
    argument, then only the *condor_schedd* daemon and any
    *condor_shadow* daemons it spawns will use the dynamic directories
    (named with the *condor_schedd* daemon's PID).

    Note that by using a dynamically-created spool directory named by
    the IP address and PID, upon restarting daemons, jobs submitted to
    the original *condor_schedd* daemon that were stored in the old
    spool directory will not be noticed by the new *condor_schedd*
    daemon, unless you manually specify the old, dynamically-generated
    :macro:`SPOOL` directory path in the configuration of the new
    *condor_schedd* daemon.

\-f
    Causes the daemon to start up in the foreground. Instead of forking,
    the daemon runs in the foreground. Since 8.9.7, this has been the default
    for all daemons other than the :tool:`condor_master`.

\-k filename
    For non-Windows operating systems, causes the daemon to read out a
    PID from the specified **filename**, and send a SIGTERM to that
    process. The daemon started with this optional argument waits until
    the daemon it is attempting to kill has exited.

\-l directory
    Overrides the value of :macro:`LOG` as specified in
    the configuration files. Primarily, this option is used with the
    *condor_kbdd* when it needs to run as the individual user logged
    into the machine, instead of running as root. Regular users would
    not normally have permission to write files into HTCondor's log
    directory. Using this option, they can override the value of :macro:`LOG`
    and have the *condor_kbdd* write its log file into a directory that
    the user has permission to write to.

\-local-name name
    Specify a local name for this instance of the daemon. This local
    name will be used to look up configuration parameters.
    The :ref:`admin-manual/introduction-to-configuration:configuration file
    macros` section contains details on how this local name will be used in the
    configuration.

\-p port
    Causes the daemon to bind to the specified port as its command
    socket. The :tool:`condor_master` daemon uses this option to ensure that
    the *condor_collector* and *condor_negotiator* start up using
    well-known ports that the rest of HTCondor depends upon them using.

\-pidfile filename
    Causes the daemon to write out its PID (process id number) to the
    specified **filename**. This file can be used to help shutdown the
    daemon without first searching through the output of the Unix *ps*
    command.

    Since daemons run with their current working directory set to the
    value of :macro:`LOG`, if a full path (one that begins with a slash
    character, ``/``) is not specified, the file will be placed in the
    :macro:`LOG` directory.

\-q
    Quiet output; write less verbose error messages to ``stderr`` when
    something goes wrong, and before regular logging can be initialized.

\-r minutes
    Causes the daemon to set a timer, upon expiration of which, it sends
    itself a SIGTERM for graceful shutdown.

\-t
    Causes the daemon to print out its error message to ``stderr``
    instead of its specified log file. This option forces the **-f**
    option.

\-v
    Causes the daemon to print out version information and exit.
