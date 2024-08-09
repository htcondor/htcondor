      

*condor_restart*
=================

Restart a set of HTCondor daemons
:index:`condor_restart<single: condor_restart; HTCondor commands>`\ :index:`condor_restart command`

Synopsis
--------

**condor_restart** [**-help | -version** ]

**condor_restart** [**-debug[:opts]**] ] [**-graceful | -fast |
-peaceful | -drain** ] [**-pool** *centralmanagerhostname[:portnumber]*] [
**-name** *hostname* | *hostname* | **-addr** *"<a.b.c.d:port>"*
| *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all** ]
[**-daemon** *daemonname* | **-master**]
[**-exec** *name*]
[**-reason** *"reason-string"*]
[**-request-id** *id*]
[**-check** *expr*]
[**-start** *expr*]

Description
-----------

*condor_restart* restarts a set of HTCondor daemons on a set of
machines. The daemons will be put into a consistent state, killed, and
then invoked anew.

If, for example, the *condor_master* needs to be restarted again with a
fresh state, this is the command that should be used to do so. If the
:macro:`DAEMON_LIST` variable in the configuration file has been changed,
this command is used to restart the *condor_master* in order to see
this change. The *condor_reconfigure* command cannot be used in the
case where the :macro:`DAEMON_LIST` expression changes.

The command *condor_restart* with no arguments or with the
**-daemon** *master* option will safely shut down all running jobs and
all submitted jobs from the machine(s) being restarted, then shut down
all the child daemons of the *condor_master*, and then restart the
*condor_master*. This, in turn, will allow the *condor_master* to
start up other daemons as specified in the :macro:`DAEMON_LIST` configuration
file entry.

When restarting down all daemons including the *condor_master*, the **-exec**
argument can be used to tell the master to run a configured :macro:`MASTER_SHUTDOWN_<Name>`
script before it restarts.

When the **-drain** option is chosen, draining options can be specified
by using the optional **-reason**, **-request-id**, **-check**, and **-start**
arguments.


For security reasons of authentication and authorization, this command
requires ADMINISTRATOR level of access.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug[:opts]**
    Causes debugging information to be sent to ``stderr``. The debug level can be set
    by specifying an optional *opts* value. Otherwise, the configuration variable :macro:`TOOL_DEBUG`
    sets the debug level.
 **-graceful**
    Gracefully shutdown daemons (the default) before restarting them
 **-fast**
    Quickly shutdown daemons before restarting them
 **-peaceful**
    Wait indefinitely for jobs to finish before shutting down daemons,
    prior to restarting them
 **-drain**
    Send a *condor_drain* command with the *-exit-on-completion* option to all
    *condor_startd* daemons that are managed by this master. Then wait for all *condor_startd*
    daemons to exit before before restarting.
 **-reason** *"reason-string"*
    Use with **-drain** to set a **-reason** *"reason-string"* value for the *condor_drain* command.
 **-request-id** *id*
    Use with **-drain** to set a **-request-id** *id* value for the *condor_drain* command.
 **-check** *expr*
    Use with **-drain** to set a **-check** *expr* value for the *condor_drain* command.
 **-start** *expr*
    Use with **-drain** to set a **-start** *expr* value for the *condor_drain* command.
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-name** *hostname*
    Send the command to a machine identified by *hostname*
 *hostname*
    Send the command to a machine identified by *hostname*
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine's master located at *"<a.b.c.d:port>"*
 *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-constraint** *expression*
    Apply this command only to machines matching the given ClassAd
    *expression*
 **-all**
    Send the command to all machines in the pool
 **-master**
    Restart the *condor_master* after shutting down all other daemons. This will have the
    effect of restarting all of the daemons.
 **-exec** *name*
    When used with **-master**, the *condor_master* will run the program configured as
    :macro:`MASTER_SHUTDOWN_<Name>` after shutting down all other daemons.
 **-daemon** *daemonname*
    Send the command to the named daemon. Without this option, the
    command is sent to the *condor_master* daemon.

Exit Status
-----------

*condor_restart* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To restart the *condor_master* and all its children on the local host:

.. code-block:: console

    $ condor_restart

To restart only the *condor_startd* on a named machine:

.. code-block:: console

    $ condor_restart -name bluejay -daemon startd

To restart a machine within a pool other than the local pool, use the
**-pool** option. The argument is the name of the central manager for
the pool. Note that one or more machines within the pool must be
specified as the targets for the command. This command restarts the
single machine named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

.. code-block:: console

    $ condor_restart -pool condor.cae.wisc.edu -name cae17

