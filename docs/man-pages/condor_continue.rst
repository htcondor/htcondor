      

*condor_continue*
==================

continue suspended jobs from the HTCondor queue
:index:`condor_continue<single: condor_continue; HTCondor commands>`\ :index:`condor_continue command`

Synopsis
--------

**condor_continue** [**-help | -version** ]

**condor_continue** [**-debug** ] [
**-pool** *centralmanagerhostname[:portnumber]* |
**-name** *scheddname* ] | [**-addr** *"<a.b.c.d:port>"*] **

Description
-----------

*condor_continue* continues one or more suspended jobs from the
HTCondor job queue. If the **-name** option is specified, the named
*condor_schedd* is targeted for processing. Otherwise, the local
*condor_schedd* is targeted. The job(s) to be continued are identified
by one of the job identifiers, as described below. For any given job,
only the owner of the job or one of the queue super users (defined by
the ``QUEUE_SUPER_USERS`` macro) can continue the job.

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
    Continue all jobs in the specified cluster
 *cluster.process*
    Continue the specific job in the cluster
 *user*
    Continue jobs belonging to specified user
 **-constraint** *expression*
    Continue all jobs which match the job ClassAd expression constraint
 **-all**
    Continue all the jobs in the queue

Exit Status
-----------

*condor_continue* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To continue all jobs except for a specific user:

.. code-block:: console

    $ condor_continue -constraint 'Owner =!= "foo"'

