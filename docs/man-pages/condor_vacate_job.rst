      

condor\_vacate\_job
===================

vacate jobs in the HTCondor queue from the hosts where they are running

Synopsis
--------

**condor\_vacate\_job** [**-help \| -version**\ ]

**condor\_vacate\_job** [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*]
[**-fast**\ ] *cluster… \| cluster.process… \| user…* \|
**-constraint **\ *expression* …

**condor\_vacate\_job** [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*]
[**-fast**\ ] **-all**

Description
-----------

*condor\_vacate\_job* finds one or more jobs from the HTCondor job queue
and vacates them from the host(s) where they are currently running. The
jobs remain in the job queue and return to the idle state.

A job running under the standard universe will first produce a
checkpoint and then the job will be killed. HTCondor will then restart
the job somewhere else, using the checkpoint to continue from where it
left off. A job running under any other universe will be sent a soft
kill signal (SIGTERM by default, or whatever is defined as the
``SoftKillSig`` in the job ClassAd), and HTCondor will restart the job
from the beginning somewhere else.

If the **-fast** option is used, the job(s) will be immediately killed,
meaning that standard universe jobs will not be allowed to checkpoint,
and the job will have to revert to the last checkpoint or start over
from the beginning.

If the **-name** option is specified, the named *condor\_schedd* is
targeted for processing. If the **-addr** option is used, the
*condor\_schedd* at the given address is targeted for processing.
Otherwise, the local *condor\_schedd* is targeted. The jobs to be
vacated are identified by one or more job identifiers, as described
below. For any given job, only the owner of the job or one of the queue
super users (defined by the ``QUEUE_SUPER_USERS`` macro) can vacate the
job.

Using *condor\_vacate\_job* on jobs which are not currently running has
no effect.

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
 *cluster*
    Vacate all jobs in the specified cluster
 *cluster.process*
    Vacate the specific job in the cluster
 *user*
    Vacate jobs belonging to specified user
 **-constraint **\ *expression*
    Vacate all jobs which match the job ClassAd expression constraint
 **-all**
    Vacate all the jobs in the queue
 **-fast**
    Perform a fast vacate and hard kill the jobs

General Remarks
---------------

Do not confuse *condor\_vacate\_job* with *condor\_vacate*.
*condor\_vacate* is given a list of hosts to vacate, regardless of what
jobs happen to be running on them. Only machine owners and
administrators have permission to use *condor\_vacate* to evict jobs
from a given host. *condor\_vacate\_job* is given a list of job to
vacate, regardless of which hosts they happen to be running on. Only the
owner of the jobs or queue super users have permission to use
*condor\_vacate\_job*.

Examples
--------

To vacate job 23.0:

::

    % condor_vacate_job 23.0

To vacate all jobs of a user named Mary:

::

    % condor_vacate_job mary

To vacate all standard universe jobs owned by Mary:

::

    % condor_vacate_job -constraint 'JobUniverse == 1 && Owner == "mary"'

Note that the entire constraint, including the quotation marks, must be
enclosed in single quote marks for most shells.

Exit Status
-----------

*condor\_vacate\_job* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
