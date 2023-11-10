      

*condor_on*
============

Start up HTCondor daemons
:index:`condor_on<single: condor_on; HTCondor commands>`\ :index:`condor_on command`

Synopsis
--------

**condor_on** [**-help | -version** ]

**condor_on** [**-debug** ]
[**-pool** *centralmanagerhostname[:portnumber]*] [
**-name** *hostname* | *hostname* | **-addr** *"<a.b.c.d:port>"*
| *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all** ]
[**-daemon** *daemonname*]

Description
-----------

*condor_on* starts up a set of the HTCondor daemons on a set of
machines. This command assumes that the *condor_master* is already
running on the machine. If this is not the case, *condor_on* will fail
complaining that it cannot find the address of the master. The command
*condor_on* with no arguments or with the **-daemon** *master* option
will tell the *condor_master* to start up the HTCondor daemons
specified in the configuration variable :macro:`DAEMON_LIST`. If a daemon
other than the *condor_master* is specified with the **-daemon**
option, *condor_on* starts up only that daemon.

This command cannot be used to start up the *condor_master* daemon.

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

*condor_on* will exit with a status value of 0 (zero) upon success, and
it will exit with the value 1 (one) upon failure.

Examples
--------

To begin running all daemons (other than *condor_master*) given in the
configuration variable :macro:`DAEMON_LIST` on the local host:

.. code-block:: console

    $ condor_on

To start up only the *condor_negotiator* on two named machines:

.. code-block:: console

    $ condor_on  robin cardinal -daemon negotiator

To start up only a daemon within a pool of machines other than the local
pool, use the **-pool** option. The argument is the name of the central
manager for the pool. Note that one or more machines within the pool
must be specified as the targets for the command. This command starts up
only the *condor_schedd* daemon on the single machine named **cae17**
within the pool of machines that has **condor.cae.wisc.edu** as its
central manager:

.. code-block:: console

    $ condor_on -pool condor.cae.wisc.edu -name cae17 -daemon schedd

