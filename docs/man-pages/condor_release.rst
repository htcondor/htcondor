      

condor\_release
===============

release held jobs in the HTCondor queue

Synopsis
--------

**condor\_release** [**-help \| -version**\ ]

**condor\_release** [**-debug**\ ] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*]
*cluster… \| cluster.process… \| user…* \|
**-constraint **\ *expression* …

**condor\_release** [**-debug**\ ] [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*] **-all**

Description
-----------

*condor\_release* releases jobs from the HTCondor job queue that were
previously placed in hold state. If the **-name** option is specified,
the named *condor\_schedd* is targeted for processing. Otherwise, the
local *condor\_schedd* is targeted. The jobs to be released are
identified by one or more job identifiers, as described below. For any
given job, only the owner of the job or one of the queue super users
(defined by the ``QUEUE_SUPER_USERS`` macro) can release the job.

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
    Release all jobs in the specified cluster
 *cluster.process*
    Release the specific job in the cluster
 *user*
    Release jobs belonging to specified user
 **-constraint **\ *expression*
    Release all jobs which match the job ClassAd expression constraint
 **-all**
    Release all the jobs in the queue

See Also
--------

*condor\_hold*

Examples
--------

To release all of the jobs of a user named Mary:

::

    % condor_release Mary

Exit Status
-----------

*condor\_release* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
