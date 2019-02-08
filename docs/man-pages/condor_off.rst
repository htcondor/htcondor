      

condor\_off
===========

Shutdown HTCondor daemons

Synopsis
--------

**condor\_off** [**-help \| -version**\ ]

**condor\_off** [**-graceful \| -fast \| -peaceful \|
-force-graceful**\ ] [**-annex  **\ *name*] [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]
[**-daemon  **\ *daemonname*]

Description
-----------

*condor\_off* shuts down a set of the HTCondor daemons running on a set
of one or more machines. It does this cleanly so that checkpointable
jobs may gracefully exit with minimal loss of work.

The command *condor\_off* without any arguments will shut down all
daemons except *condor\_master*, unless **-annex **\ *name* is
specified. The *condor\_master* can then handle both local and remote
requests to restart the other HTCondor daemons if need be. To restart
HTCondor running on a machine, see the *condor\_on* command.

With the **-daemon **\ *master* option, *condor\_off* will shut down all
daemons including the *condor\_master*. Specification using the
**-daemon** option will shut down only the specified daemon.

For security reasons of authentication and authorization, this command
requires ``ADMINISTRATOR`` level of access.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-graceful**
    Gracefully shutdown daemons (the default)
 **-fast**
    Quickly shutdown daemons. A minimum of the first two characters of
    this option must be specified, to distinguish it from the
    **-force-graceful** command.
 **-peaceful**
    Wait indefinitely for jobs to finish
 **-force-graceful**
    Force a graceful shutdown, even after issuing a **-peaceful**
    command. A minimum of the first two characters of this option must
    be specified, to distinguish it from the **-fast** command.
 **-annex **\ *name*
    Turn off master daemons in the specified annex. By default this will
    result in the corresponding instances shutting down.
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

*condor\_off* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Examples
--------

To shut down all daemons (other than *condor\_master*) on the local
host:

::

    % condor_off

To shut down only the *condor\_collector* on three named machines:

::

    % condor_off  cinnamon cloves vanilla -daemon collector

To shut down daemons within a pool of machines other than the local
pool, use the **-pool** option. The argument is the name of the central
manager for the pool. Note that one or more machines within the pool
must be specified as the targets for the command. This command shuts
down all daemons except the *condor\_master* on the single machine named
**cae17** within the pool of machines that has **condor.cae.wisc.edu**
as its central manager:

::

    % condor_off  -pool condor.cae.wisc.edu -name cae17

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
