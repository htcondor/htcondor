      

condor\_vacate
==============

Vacate jobs that are running on the specified hosts

Synopsis
--------

**condor\_vacate** [**-help \| -version**\ ]

**condor\_vacate** [**-graceful \| -fast**\ ] [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]

Description
-----------

*condor\_vacate* causes HTCondor to checkpoint any running jobs on a set
of machines and force the jobs to vacate the machine. The job(s) remains
in the submitting machine’s job queue.

Given the (default) **-graceful** option, a job running under the
standard universe will first produce a checkpoint and then the job will
be killed. HTCondor will then restart the job somewhere else, using the
checkpoint to continue from where it left off. A job running under the
vanilla universe is killed, and HTCondor restarts the job from the
beginning somewhere else. *condor\_vacate* has no effect on a machine
with no HTCondor job currently running.

There is generally no need for the user or administrator to explicitly
run *condor\_vacate*. HTCondor takes care of jobs in this way
automatically following the policies given in configuration files.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-graceful**
    Inform the job to checkpoint, then soft-kill it.
 **-fast**
    Hard-kill jobs instead of checkpointing them
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

*condor\_vacate* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Examples
--------

To send a *condor\_vacate* command to two named machines:

::

    % condor_vacate  robin cardinal

To send the *condor\_vacate* command to a machine within a pool of
machines other than the local pool, use the **-pool** option. The
argument is the name of the central manager for the pool. Note that one
or more machines within the pool must be specified as the targets for
the command. This command sends the command to a the single machine
named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

::

    % condor_vacate -pool condor.cae.wisc.edu -name cae17

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
