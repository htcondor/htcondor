Starting Up, Shutting Down, Reconfiguring, and Restarting HTCondor
==================================================================

If you installed HTCondor with administrative privileges, HTCondor will
start up when the machine boots and shut down when the machine does, using
the usual mechanism for the machine's operating system.  You can generally
use those mechanisms in the usual way if you need to manually control
whether or not HTCondor is running.  There are two situations in
which you might want to run :doc:`../man-pages/condor_master`,
:doc:`../man-pages/condor_on`, or :doc:`../man-pages/condor_off` from the
command line.

#. If you installed HTCondor without administrative privileges, you'll
   have to run *condor_master* from the command line to turn on HTCondor:

    .. code-block:: console

        $ condor_master

   Then run the following command to turn HTCondor completely off:

    .. code-block:: console

        $ condor_off -master

#. If the usual OS-specific method of controlling HTCondor is inconvenient
   to use remotely, you may be able to use the *condor_on* and *condor_off*
   tools instead.

Using HTCondor's Remote Management Features
-------------------------------------------

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
    pool. All utilize the *condor_off* tool.

    To stop a single execute machine from running jobs, the
    *condor_off* command specifies the machine by host name.

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

    If it is instead desired that the submit machine shuts down only
    after all jobs that are currently in the queue are finished, first
    disable new submissions to the queue by setting the configuration
    variable

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
    If HTCondor is not running, perhaps because one of the *condor_off*
    commands was used, then starting HTCondor daemons back up depends on
    which part of HTCondor is currently not running.

    If no HTCondor daemons are running, then starting HTCondor is a
    matter of executing the *condor_master* daemon. The
    *condor_master* daemon will then invoke all other specified daemons
    on that machine. The *condor_master* daemon executes on every
    machine that is to run HTCondor.

    If a specific daemon needs to be started up, and the
    *condor_master* daemon is already running, then issue the command
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
    this *condor_on* command is to be directed.

 Restarting HTCondor, If HTCondor Daemons Are Running
    If HTCondor daemons are currently running, but need to be killed and
    newly invoked, the *condor_restart* tool does this. This would be
    the case for a new value of a configuration variable for which using
    *condor_reconfig* is inadequate.

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
    a *condor_reconfig* command to each host. Do this with a single
    command,

    .. code-block:: console

      $ condor_reconfig -all

    If the global configuration file is not shared among all the machines,
    as it will be if using a shared file system, the change must be made to
    each copy of the global configuration file before issuing the
    *condor_reconfig* command.

    Issuing a *condor_reconfig* command is inadequate for some
    configuration variables. For those, a restart of HTCondor is required.
    Those configuration variables that require a restart are listed in
    the :ref:`admin-manual/introduction-to-configuration:macros that will require a
    restart when changed` section.  You can also refer to the
    :doc:`/man-pages/condor_restart` manual page.
