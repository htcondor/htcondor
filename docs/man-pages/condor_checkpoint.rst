      

condor\_checkpoint
==================

send a checkpoint command to jobs running on specified hosts

Synopsis
--------

**condor\_checkpoint** [**-help \| -version**\ ]

**condor\_checkpoint** [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]

Description
-----------

*condor\_checkpoint* sends a checkpoint command to a set of machines
within a single pool. This causes the startd daemon on each of the
specified machines to take a checkpoint of any running job that is
executing under the standard universe. The job is temporarily stopped, a
checkpoint is taken, and then the job continues. If no machine is
specified, then the command is sent to the machine that issued the
*condor\_checkpoint* command.

The command sent is a periodic checkpoint. The job will take a
checkpoint, but then the job will immediately continue running after the
checkpoint is completed. *condor\_vacate*, on the other hand, will
result in the job exiting (vacating) after it produces a checkpoint.

If the job being checkpointed is running under the standard universe,
the job produces a checkpoint and then continues running on the same
machine. If the job is running under another universe, or if there is
currently no HTCondor job running on that host, then
*condor\_checkpoint* has no effect.

There is generally no need for the user or administrator to explicitly
run *condor\_checkpoint*. Taking checkpoints of running HTCondor jobs is
handled automatically following the policies stated in the configuration
files.

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

*condor\_checkpoint* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To send a *condor\_checkpoint* command to two named machines:

::

    % condor_checkpoint  robin cardinal

To send the *condor\_checkpoint* command to a machine within a pool of
machines other than the local pool, use the **-pool** option. The
argument is the name of the central manager for the pool. Note that one
or more machines within the pool must be specified as the targets for
the command. This command sends the command to a the single machine
named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

::

    % condor_checkpoint -pool condor.cae.wisc.edu -name cae17

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
