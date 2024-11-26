*condor_continue*
=================

Continue suspended jobs from HTCondor.

:index:`condor_continue<double: condor_continue; HTCondor commands>`

Synopsis
--------

**condor_continue** [**-help** | **-version**]

**condor_continue** [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_continue** [**-debug**] [**-long**] [**-totals**] [**-all**] [**-constraint** *expression*]
[**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*]

Description
-----------

*condor_continue* continues one or more jobs that have been suspended
as a result of running :tool:`condor_suspend`. The job(s) to be continued
are identified by one of the job identifiers, as described below. For
any given job, only the owner of the job or one of the queue super users
(defined by the :macro:`QUEUE_SUPER_USERS` macro) can continue the job.

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
 **-constraint** *expression*
    Continue all jobs which match the job ClassAd expression constraint.
 **-all**
    Continue all the jobs in the queue.
 *cluster*
    Continue all jobs in the specified cluster.
 *cluster.process*
    Continue the specific job in the cluster.
 *user*
    Continue all jobs belonging to specified user.

General Remarks
---------------

If the **-name** option is specified, the named *condor_schedd* is targeted
for processing. Otherwise, the local *condor_schedd* is targeted.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

To continue a specific job:

.. code-block:: console

    $ condor_continue 432.1

To continue all jobs except for a specific user:

.. code-block:: console

    # condor_continue -constraint 'Owner =!= "foo"'

See Also
--------

:tool:`condor_suspend`, :tool:`condor_rm`, :tool:`condor_hold`, :tool:`condor_release`,
:tool:`condor_vacate_job`, :tool:`condor_vacate`

Availability
------------

Linux, MacOS, Windows