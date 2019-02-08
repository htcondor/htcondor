      

condor\_on
==========

Start up HTCondor daemons

Synopsis
--------

**condor\_on** [**-help \| -version**\ ]

**condor\_on** [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]
[**-daemon  **\ *daemonname*]

Description
-----------

*condor\_on* starts up a set of the HTCondor daemons on a set of
machines. This command assumes that the *condor\_master* is already
running on the machine. If this is not the case, *condor\_on* will fail
complaining that it cannot find the address of the master. The command
*condor\_on* with no arguments or with the **-daemon **\ *master* option
will tell the *condor\_master* to start up the HTCondor daemons
specified in the configuration variable ``DAEMON_LIST``. If a daemon
other than the *condor\_master* is specified with the **-daemon**
option, *condor\_on* starts up only that daemon.

This command cannot be used to start up the *condor\_master* daemon.

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

*condor\_on* will exit with a status value of 0 (zero) upon success, and
it will exit with the value 1 (one) upon failure.

Examples
--------

To begin running all daemons (other than *condor\_master*) given in the
configuration variable ``DAEMON_LIST`` on the local host:

::

    % condor_on

To start up only the *condor\_negotiator* on two named machines:

::

    % condor_on  robin cardinal -daemon negotiator

To start up only a daemon within a pool of machines other than the local
pool, use the **-pool** option. The argument is the name of the central
manager for the pool. Note that one or more machines within the pool
must be specified as the targets for the command. This command starts up
only the *condor\_schedd* daemon on the single machine named **cae17**
within the pool of machines that has **condor.cae.wisc.edu** as its
central manager:

::

    % condor_on -pool condor.cae.wisc.edu -name cae17 -daemon schedd

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
