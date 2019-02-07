      

condor\_reschedule
==================

Update scheduling information to the central manager

Synopsis
--------

**condor\_reschedule** [**-help \| -version**\ ]

**condor\_reschedule** [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]

Description
-----------

*condor\_reschedule* updates the information about a set of machines’
resources and jobs to the central manager. This command is used to force
an update before viewing the current status of a machine. Viewing the
status of a machine is done with the *condor\_status* command.
*condor\_reschedule* also starts a new negotiation cycle between
resource owners and resource providers on the central managers, so that
jobs can be matched with machines right away. This can be useful in
situations where the time between negotiation cycles is somewhat long,
and an administrator wants to see if a job in the queue will get matched
without waiting for the next negotiation cycle.

A new negotiation cycle cannot occur more frequently than every 20
seconds. Requests for new negotiation cycle within that 20 second window
will be deferred until 20 seconds have passed since that last cycle.

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

Exit Status
-----------

*condor\_reschedule* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To update the information on three named machines:

::

    % condor_reschedule robin cardinal bluejay

To reschedule on a machine within a pool other than the local pool, use
the **-pool** option. The argument is the name of the central manager
for the pool. Note that one or more machines within the pool must be
specified as the targets for the command. This command reschedules the
single machine named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

::

    % condor_reschedule -pool condor.cae.wisc.edu -name cae17

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
