      

*condor_hold*
==============

put jobs in the queue into the hold state
:index:`condor_hold<single: condor_hold; HTCondor commands>`\ :index:`condor_hold command`

Synopsis
--------

**condor_hold** [**-help | -version** ]

**condor_hold** [**-debug** ] [**-reason** *reasonstring*]
[**-subcode** *number*] [
**-pool** *centralmanagerhostname[:portnumber]* |
**-name** *scheddname* ] | [**-addr** *"<a.b.c.d:port>"*]
*cluster... | cluster.process... | user...* |
**-constraint** *expression* ...

**condor_hold** [**-debug** ] [**-reason** *reasonstring*]
[**-subcode** *number*] [
**-pool** *centralmanagerhostname[:portnumber]* |
**-name** *scheddname* ] | [**-addr** *"<a.b.c.d:port>"*] **-all**

Description
-----------

*condor_hold* places jobs from the HTCondor job queue in the hold
state. If the **-name** option is specified, the named *condor_schedd*
is targeted for processing. Otherwise, the local *condor_schedd* is
targeted. The jobs to be held are identified by one or more job
identifiers, as described below. For any given job, only the owner of
the job or one of the queue super users (defined by the
:macro:`QUEUE_SUPER_USERS` macro) can place the job on hold.

A job in the hold state remains in the job queue, but the job will not
run until released with *condor_release*.

A currently running job that is placed in the hold state by
*condor_hold* is sent a hard kill signal.

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
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-reason** *reasonstring*
    Sets the job ClassAd attribute :ad-attr:`HoldReason` to the value given by
    *reasonstring*. *reasonstring* will be delimited by double quote
    marks on the command line, if it contains space characters.
 **-subcode** *number*
    Sets the job ClassAd attribute :ad-attr:`HoldReasonSubCode` to the integer
    value given by *number*.
 *cluster*
    Hold all jobs in the specified cluster
 *cluster.process*
    Hold the specific job in the cluster
 *user*
    Hold all jobs belonging to specified user
 **-constraint** *expression*
    Hold all jobs which match the job ClassAd expression constraint
    (within quotation marks). Note that quotation marks must be escaped
    with the backslash characters for most shells.
 **-all**
    Hold all the jobs in the queue

See Also
--------

*condor_release*

Examples
--------

To place on hold all jobs (of the user that issued the *condor_hold*
command) that are not currently running:

.. code-block:: console

    $ condor_hold -constraint "JobStatus!=2"

Multiple options within the same command cause the union of all jobs
that meet either (or both) of the options to be placed in the hold
state. Therefore, the command

.. code-block:: console

    $ condor_hold Mary -constraint "JobStatus!=2"

places all of Mary's queued jobs into the hold state, and the constraint
holds all queued jobs not currently running. It also sends a hard kill
signal to any of Mary's jobs that are currently running. Note that the
jobs specified by the constraint will also be Mary's jobs, if it is Mary
that issues this example *condor_hold* command.

Exit Status
-----------

*condor_hold* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

