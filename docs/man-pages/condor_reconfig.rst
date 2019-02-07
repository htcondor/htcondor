      

condor\_reconfig
================

Reconfigure HTCondor daemons

Synopsis
--------

**condor\_reconfig** [**-help \| -version**\ ]

**condor\_reconfig** [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]
[**-daemon  **\ *daemonname*]

Description
-----------

*condor\_reconfig* reconfigures all of the HTCondor daemons in
accordance with the current status of the HTCondor configuration
file(s). Once reconfiguration is complete, the daemons will behave
according to the policies stated in the configuration file(s). The main
exception is with the ``DAEMON_LIST`` variable, which will only be
updated if the *condor\_restart* command is used. Other configuration
variables that can only be changed if the HTCondor daemons are restarted
are listed in the HTCondor manual in the section on configuration. In
general, *condor\_reconfig* should be used when making changes to the
configuration files, since it is faster and more efficient than
restarting the daemons.

The command *condor\_reconfig* with no arguments or with the
**-daemon **\ *master* option will cause the reconfiguration of the
*condor\_master* daemon and all the child processes of the
*condor\_master*.

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

*condor\_reconfig* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To reconfigure the *condor\_master* and all its children on the local
host:

::

    % condor_reconfig

To reconfigure only the *condor\_startd* on a named machine:

::

    % condor_reconfig -name bluejay -daemon startd

To reconfigure a machine within a pool other than the local pool, use
the **-pool** option. The argument is the name of the central manager
for the pool. Note that one or more machines within the pool must be
specified as the targets for the command. This command reconfigures the
single machine named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

::

    % condor_reconfig -pool condor.cae.wisc.edu -name cae17

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
