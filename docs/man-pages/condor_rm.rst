      

condor\_rm
==========

remove jobs from the HTCondor queue

Synopsis
--------

**condor\_rm** [**-help \| -version**\ ]

**condor\_rm** [**-debug**\ ] [**-forcex**\ ] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*]
*cluster… \| cluster.process… \| user…* \|
**-constraint **\ *expression* …

**condor\_rm** [**-debug**\ ] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*] **-all**

Description
-----------

*condor\_rm* removes one or more jobs from the HTCondor job queue. If
the **-name** option is specified, the named *condor\_schedd* is
targeted for processing. Otherwise, the local *condor\_schedd* is
targeted. The jobs to be removed are identified by one or more job
identifiers, as described below. For any given job, only the owner of
the job or one of the queue super users (defined by the
``QUEUE_SUPER_USERS`` macro) can remove the job.

When removing a grid job, the job may remain in the “X” state for a very
long time. This is normal, as HTCondor is attempting to communicate with
the remote scheduling system, ensuring that the job has been properly
cleaned up. If it takes too long, or in rare circumstances is never
removed, the job may be forced to leave the job queue by using the
**-forcex** option. This forcibly removes jobs that are in the “X” state
without attempting to finish any clean up at the remote scheduler.

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
 **-forcex**
    Force the immediate local removal of jobs in the ’X’ state (only
    affects jobs already being removed)
 *cluster*
    Remove all jobs in the specified cluster
 *cluster.process*
    Remove the specific job in the cluster
 *user*
    Remove jobs belonging to specified user
 **-constraint **\ *expression*
    Remove all jobs which match the job ClassAd expression constraint
 **-all**
    Remove all the jobs in the queue

General Remarks
---------------

Use the *-forcex* argument with caution, as it will remove jobs from the
local queue immediately, but can orphan parts of the job that are
running remotely and have not yet been stopped or removed.

Examples
--------

For a user to remove all their jobs that are not currently running:

::

    % condor_rm -constraint 'JobStatus =!= 2'

Exit Status
-----------

*condor\_rm* will exit with a status value of 0 (zero) upon success, and
it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
