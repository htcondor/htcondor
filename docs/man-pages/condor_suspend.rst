      

condor\_suspend
===============

suspend jobs from the HTCondor queue

Synopsis
--------

**condor\_suspend** [**-help \| -version**\ ]

**condor\_suspend** [**-debug**\ ] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*] **

Description
-----------

*condor\_suspend* suspends one or more jobs from the HTCondor job queue.
When a job is suspended, the match between the *condor\_schedd* and
machine is not been broken, such that the claim is still valid. But, the
job is not making any progress and HTCondor is no longer generating a
load on the machine. If the **-name** option is specified, the named
*condor\_schedd* is targeted for processing. Otherwise, the local
*condor\_schedd* is targeted. The job(s) to be suspended are identified
by one of the job identifiers, as described below. For any given job,
only the owner of the job or one of the queue super users (defined by
the ``QUEUE_SUPER_USERS`` macro) can suspend the job.

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
 *cluster*
    Suspend all jobs in the specified cluster
 *cluster.process*
    Suspend the specific job in the cluster
 *user*
    Suspend jobs belonging to specified user
 **-constraint **\ *expression*
    Suspend all jobs which match the job ClassAd expression constraint
 **-all**
    Suspend all the jobs in the queue

Exit Status
-----------

*condor\_suspend* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To suspend all jobs except for a specific user:

::

    % condor_suspend -constraint 'Owner =!= "foo"'

Run *condor\_continue* to continue execution.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
