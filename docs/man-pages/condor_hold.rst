*condor_hold*
=============

Prevents existing job(s) from running in HTCondor.

:index:`condor_hold<double: condor_hold; HTCondor commands>`

Synopsis
--------

**condor_hold** [**-help** | **-version**]

**condor_hold** [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_hold** [**-debug**] [**-long**] [**-totals**] [**-all**]
[**-constraint** *expression*] [**-reason** *message*] [**-subcode** *number*]
[**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*]

Description
-----------

Place job(s) into the hold state in HTCondor. Held jobs will remain
in the *condor_schedd* but will not be matched to resources or executed.
For any given job, only the owner of the job or one of the queue super
users (defined by the :macro:`QUEUE_SUPER_USERS` macro) can hold the job.

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
    Sets the :ad-attr:`HoldReason` attribute to the provided *message*
    in the job(s) ClassAd.  This message will appear in the output
    of the :tool:`condor_q` -hold command.
 **-subcode** *number*
    Sets :ad-attr:`HoldReasonSubCode` attribute to the provided
    *number* in the job(s) ClassAd.
 **-constraint** *expression*
    Hold all jobs which match the job ClassAd expression constraint.
 **-all**
    Hold all the jobs in the queue.
 *cluster*
    Hold all jobs in the specified cluster.
 *cluster.process*
    Hold the specific job in the cluster.
 *user*
    Hold all jobs belonging to specified user.

General Remarks
---------------

If the **-name** option is specified, the named *condor_schedd* is targeted
for processing. Otherwise, the local *condor_schedd* is targeted.

A job in the hold state remains in HTCondor, but the job will not run until
released via :tool:`condor_release` or removed via :tool:`condor_rm`.

Jobs put on hold while running will send a hard kill signal to stop the
current execution.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

To hold a specific job:

.. code-block:: console

    $ condor_hold 432.1

To hold a specific job with a reason:

.. code-block:: console

    $ condor_hold 432.1 -reason "Defer running job until needed"

To hold all jobs that are not currently running:

.. code-block:: console

    $ condor_hold -constraint "JobStatus!=2"

To hold all of user Mary's jobs currently not running:

.. code-block:: console

    # condor_hold Mary -constraint "JobStatus!=2"


See Also
--------

:tool:`condor_release`, :tool:`condor_rm`, :tool:`condor_continue`, :tool:`condor_suspend`,
:tool:`condor_vacate_job`, :tool:`condor_vacate`

Availability
------------

Linux, MacOS, Windows
