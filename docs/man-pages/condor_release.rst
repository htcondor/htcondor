*condor_release*
================

Change held jobs back to idle.

:index:`condor_release<double: condor_release; HTCondor commands>`

Synopsis
--------

**condor_release** [**-help** | **-version**]

**condor_release** [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_release** [**-debug**] [**-long**] [**-totals**] [**-all**]
[**-reason** *message*] [**-constraint** *expression*]
[**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*]

Description
-----------

Releases job(s) from the held state back to idle in HTCondor to allow
the job(s) to be matched to resources and executed. For any given job,
only the owner of the job or one of the queue super users (defined by
the :macro:`QUEUE_SUPER_USERS` macro) can release the job.

Options
-------

 **-help**
    Display usage information.
 **-version**
    Display version information.
 **-long**
    Display result ClassAd.
 **-totals**
    Display success/failure totals.
 **-pool** *hostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number.
 **-name** *scheddname*
    Send the command to a machine identified by *scheddname*.
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-reason** *message*
    Sets the :ad-attr:`ReleaseReason` attribute to the provided *message*
    in the job(s) ClassAd.
 **-constraint** *expression*
    Release all jobs which match the job ClassAd expression constraint.
 **-all**
    Release all the jobs in the queue.
 *cluster*
    Release all jobs in the specified cluster.
 *cluster.process*
    Release the specific job in the cluster.
 *user*
    Release jobs belonging to specified user.

General Remarks
---------------

If the **-name** option is specified, the named *condor_schedd* is targeted
for processing. Otherwise, the local *condor_schedd* is targeted.

A job can be put on hold by the user who submitted the job, the AP administrator(s),
or by HTCondor when something went wrong during the job's attempt to run.

Attempts to release a job not in the held state will result in a failure
and the job will be unchanged.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

To release a specific held job:

.. code-block:: console

    $ condor_release 432.1

Release all held jobs for user Mary:

.. code-block:: console

    # condor_release mary

See Also
--------

:tool:`condor_hold`, :tool:`condor_rm`, :tool:`condor_continue`, :tool:`condor_suspend`,
:tool:`condor_vacate_job`, :tool:`condor_vacate`

Availability
------------

Linux, MacOS, Windows
