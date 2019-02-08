      

condor\_restart
===============

Restart a set of HTCondor daemons

Synopsis
--------

**condor\_restart** [**-help \| -version**\ ]

**condor\_restart** [**-debug**\ ] [**-graceful \| -fast \|
-peaceful**\ ] [**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]
[**-daemon  **\ *daemonname*]

Description
-----------

*condor\_restart* restarts a set of HTCondor daemons on a set of
machines. The daemons will be put into a consistent state, killed, and
then invoked anew.

If, for example, the *condor\_master* needs to be restarted again with a
fresh state, this is the command that should be used to do so. If the
``DAEMON_LIST`` variable in the configuration file has been changed,
this command is used to restart the *condor\_master* in order to see
this change. The *condor\_reconfigure* command cannot be used in the
case where the ``DAEMON_LIST`` expression changes.

The command *condor\_restart* with no arguments or with the
**-daemon **\ *master* option will safely shut down all running jobs and
all submitted jobs from the machine(s) being restarted, then shut down
all the child daemons of the *condor\_master*, and then restart the
*condor\_master*. This, in turn, will allow the *condor\_master* to
start up other daemons as specified in the ``DAEMON_LIST`` configuration
file entry.

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
    value of the configuration variable ``TOOL_DEBUG``.
 **-graceful**
    Gracefully shutdown daemons (the default) before restarting them
 **-fast**
    Quickly shutdown daemons before restarting them
 **-peaceful**
    Wait indefinitely for jobs to finish before shutting down daemons,
    prior to restarting them
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number
 **-name **\ *hostname*
    Send the command to a machine identified by *hostname*
 *hostname*
    Send the command to a machine identified by *hostname*
 **-addr **\ *"<a.b.c.d:port>"*
    Send the command to a machine’s master located at *"<a.b.c.d:port>"*
 *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-constraint **\ *expression*
    Apply this command only to machines matching the given ClassAd
    *expression*
 **-all**
    Send the command to all machines in the pool
 **-daemon **\ *daemonname*
    Send the command to the named daemon. Without this option, the
    command is sent to the *condor\_master* daemon.

Exit Status
-----------

*condor\_restart* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To restart the *condor\_master* and all its children on the local host:

::

    % condor_restart

To restart only the *condor\_startd* on a named machine:

::

    % condor_restart -name bluejay -daemon startd

To restart a machine within a pool other than the local pool, use the
**-pool** option. The argument is the name of the central manager for
the pool. Note that one or more machines within the pool must be
specified as the targets for the command. This command restarts the
single machine named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

::

    % condor_restart -pool condor.cae.wisc.edu -name cae17

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
