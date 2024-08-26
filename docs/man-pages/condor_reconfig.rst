      

*condor_reconfig*
==================

Reconfigure HTCondor daemons
:index:`condor_reconfig<single: condor_reconfig; HTCondor commands>`\ :index:`condor_reconfig command`

Synopsis
--------

**condor_reconfig** [**-help | -version** ]

**condor_reconfig** [**-debug** ]
[**-pool** *centralmanagerhostname[:portnumber]*] [
**-name** *hostname* | *hostname* | **-addr** *"<a.b.c.d:port>"*
| *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all** ]
[**-daemon** *daemonname*]

Description
-----------

*condor_reconfig* reconfigures all of the HTCondor daemons in
accordance with the current status of the HTCondor configuration
file(s). Once reconfiguration is complete, the daemons will behave
according to the policies stated in the configuration file(s). The main
exception is with the :macro:`DAEMON_LIST` variable, which will only be
updated if the *condor_restart* command is used. Other configuration
variables that can only be changed if the HTCondor daemons are restarted
are listed in the HTCondor manual in the section on configuration. In
general, *condor_reconfig* should be used when making changes to the
configuration files, since it is faster and more efficient than
restarting the daemons.

The command *condor_reconfig* with no arguments or with the
**-daemon** *master* option will cause the reconfiguration of the
*condor_master* daemon and all the child processes of the
*condor_master*.

For security reasons of authentication and authorization, this command
requires ADMINISTRATOR level of access.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
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
 **-daemon** *daemonname*
    Send the command to the named daemon. Without this option, the
    command is sent to the *condor_master* daemon.

Exit Status
-----------

*condor_reconfig* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To reconfigure the *condor_master* and all its children on the local
host:

.. code-block:: console

    $ condor_reconfig

To reconfigure only the *condor_startd* on a named machine:

.. code-block:: console

    $ condor_reconfig -name bluejay -daemon startd

To reconfigure a machine within a pool other than the local pool, use
the **-pool** option. The argument is the name of the central manager
for the pool. Note that one or more machines within the pool must be
specified as the targets for the command. This command reconfigures the
single machine named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

.. code-block:: console

    $ condor_reconfig -pool condor.cae.wisc.edu -name cae17

