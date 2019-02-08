      

condor\_hold
============

put jobs in the queue into the hold state

Synopsis
--------

**condor\_hold** [**-help \| -version**\ ]

**condor\_hold** [**-debug**\ ] [**-reason  **\ *reasonstring*]
[**-subcode  **\ *number*] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*]
*cluster… \| cluster.process… \| user…* \|
**-constraint **\ *expression* …

**condor\_hold** [**-debug**\ ] [**-reason  **\ *reasonstring*]
[**-subcode  **\ *number*] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*] **-all**

Description
-----------

*condor\_hold* places jobs from the HTCondor job queue in the hold
state. If the **-name** option is specified, the named *condor\_schedd*
is targeted for processing. Otherwise, the local *condor\_schedd* is
targeted. The jobs to be held are identified by one or more job
identifiers, as described below. For any given job, only the owner of
the job or one of the queue super users (defined by the
``QUEUE_SUPER_USERS`` macro) can place the job on hold.

A job in the hold state remains in the job queue, but the job will not
run until released with *condor\_release*.

A currently running job that is placed in the hold state by
*condor\_hold* is sent a hard kill signal. For a standard universe job,
this means that the job is removed from the machine without allowing a
checkpoint to be produced first.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number
 **-name **\ *scheddname*
    Send the command to a machine identified by *scheddname*
 **-addr **\ *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-reason **\ *reasonstring*
    Sets the job ClassAd attribute ``HoldReason`` to the value given by
    *reasonstring*. *reasonstring* will be delimited by double quote
    marks on the command line, if it contains space characters.
 **-subcode **\ *number*
    Sets the job ClassAd attribute ``HoldReasonSubCode`` to the integer
    value given by *number*.
 *cluster*
    Hold all jobs in the specified cluster
 *cluster.process*
    Hold the specific job in the cluster
 *user*
    Hold all jobs belonging to specified user
 **-constraint **\ *expression*
    Hold all jobs which match the job ClassAd expression constraint
    (within quotation marks). Note that quotation marks must be escaped
    with the backslash characters for most shells.
 **-all**
    Hold all the jobs in the queue

See Also
--------

*condor\_release*

Examples
--------

To place on hold all jobs (of the user that issued the *condor\_hold*
command) that are not currently running:

::

    % condor_hold -constraint "JobStatus!=2"

Multiple options within the same command cause the union of all jobs
that meet either (or both) of the options to be placed in the hold
state. Therefore, the command

::

    % condor_hold Mary -constraint "JobStatus!=2"

places all of Mary’s queued jobs into the hold state, and the constraint
holds all queued jobs not currently running. It also sends a hard kill
signal to any of Mary’s jobs that are currently running. Note that the
jobs specified by the constraint will also be Mary’s jobs, if it is Mary
that issues this example *condor\_hold* command.

Exit Status
-----------

*condor\_hold* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
