      

*condor_suspend*
=================

suspend jobs from the HTCondor queue
:index:`condor_suspend<single: condor_suspend; HTCondor commands>`\ :index:`condor_suspend command`

Synopsis
--------

**condor_suspend** [**-help | -version** ]

**condor_suspend** [**-debug** ] [
**-pool** *centralmanagerhostname[:portnumber]* |
**-name** *scheddname* ] | [**-addr** *"<a.b.c.d:port>"*] **

Description
-----------

*condor_suspend* suspends one or more jobs from the HTCondor job queue.
When a job is suspended, the match between the *condor_schedd* and
machine is not been broken, such that the claim is still valid. But, the
job is not making any progress and HTCondor is no longer generating a
load on the machine. If the **-name** option is specified, the named
*condor_schedd* is targeted for processing. Otherwise, the local
*condor_schedd* is targeted. The job(s) to be suspended are identified
by one of the job identifiers, as described below. For any given job,
only the owner of the job or one of the queue super users (defined by
the ``QUEUE_SUPER_USERS`` macro) can suspend the job.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-name** *scheddname*
    Send the command to a machine identified by *scheddname*
 **-addr** *"<a.b.c.d:port>"*
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
 **-constraint** *expression*
    Suspend all jobs which match the job ClassAd expression constraint
 **-all**
    Suspend all the jobs in the queue

Exit Status
-----------

*condor_suspend* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To suspend all jobs except for a specific user:

.. code-block:: console

    $ condor_suspend -constraint 'Owner =!= "foo"'

Run *condor_continue* to continue execution.

