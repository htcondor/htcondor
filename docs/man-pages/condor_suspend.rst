*condor_suspend*
================

Suspend running job(s) from HTCondor.

:index:`condor_suspend<double: condor_suspend; HTCondor commands>`

Synopsis
--------

**condor_suspend** [**-help** | **-version**]

**condor_suspend** [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_suspend** [**-debug**] [**-long**] [**-totals**] [**-all**] [**-constraint** *expression*]
[**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*]

Description
-----------

All the processes in a suspended job are sent the ``SIGSTOP`` signal,
or equivalent. They consume no cpu usage, but continue to use memory
and scratch disk space. Suspended jobs still consume the slot they
run in. Suspended jobs are still charged to submitters in term of user
priority. For any given job, only the owner of the job or one of the
queue super users (defined by the :macro:`QUEUE_SUPER_USERS` macro)
can suspend the job.

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
    Suspend all jobs which match the job ClassAd expression constraint.
 **-all**
    Suspend all the jobs in the queue.
 *cluster*
    Suspend all jobs in the specified cluster.
 *cluster.process*
    Suspend the specific job in the cluster.
 *user*
    Suspend jobs belonging to specified user.

General Remarks
---------------

If the **-name** option is specified, the named *condor_schedd* is targeted
for processing. Otherwise, the local *condor_schedd* is targeted.

When a job is suspended, the match between the *condor_schedd* and machine
is not been broken, such that the claim is still valid.

An administrator might want to suspend the jobs in a pool to quickly reduce the
power draw in a pool, in the case where they may be able to :tool:`condor_continue`
the jobs after a short while, and do not wish the jobs to be vacated and then
restart from their beginning or last checkpoint.

Use :tool:`condor_continue` to continue suspended job executions.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

To suspend a specific job:

.. code-block:: console

    $ condor_continue 432.1

To suspend all jobs except for a specific user:

.. code-block:: console

    # condor_suspend -constraint 'Owner =!= "foo"'

See Also
--------

:tool:`condor_continue`, :tool:`condor_rm`, :tool:`condor_hold`, :tool:`condor_release`,
:tool:`condor_vacate_job`, :tool:`condor_vacate`

Availability
------------

Linux, MacOS, Windows
