*condor_rm*
===========

Withdraw job(s) from an Access Point.

:index:`condor_rm<double: condor_rm; HTCondor commands>`

Synopsis
--------

**condor_rm** [**-help** | **-version**]

**condor_rm** [**-transfer**] [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_rm** [**-debug**] [**-long**] [**-totals**] [**-all**] [**-hold**] [**-transfer**] [**-constraint** *expression*]
[**-forcex**][**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*]

Description
-----------

Remove job(s) currently being managed by an Access Point. Any jobs actively
running or transferring files will be stopped and cleaned prior to removing
the job and writing its ClassAd to the :macro:`HISTORY` file. For any given job,
only the owner of the job or one of the queue super users (defined by the
:macro:`QUEUE_SUPER_USERS` macro) can remove the job.

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
 **-forcex**
    Force the immediate local removal of jobs in the ``X`` state. This only
    affects jobs already being removed.
 **-transfer**
    If the job is running, ask the job to transfer any output files to the
    submitter before the job is removed.
 **-constraint** *expression*
    Remove all jobs which match the job ClassAd expression constraint.
 **-all**
    Remove all the jobs in the queue.
 **-hold**
    Only remove jobs that are in the held state. This may be combined with
    any of the other constraints (a *cluster*, a *cluster.process*, a *user*,
    or a **-constraint** *expression*); only the held jobs among those
    selected are removed.
 *cluster*
    Remove all jobs in the specified cluster.
 *cluster.process*
    Remove the specific job in the cluster.
 *user*
    Remove jobs belonging to specified user.

General Remarks
---------------

If the **-name** option is specified, the named *condor_schedd* is targeted
for processing. Otherwise, the local *condor_schedd* is targeted.

Removed jobs that were running in the grid universe may remain in the ``X``
state for a long time. This is normal, as HTCondor is attempting to communicate
with the remote scheduling system to ensure the job has been properly cleaned
up. The job can be forcibly removed with the **-forcex** if the job is stuck
in the ``X`` state.

.. warning::

    The **-forcex** option will cause immediate removal of the job which
    an orphan parts of the job that running remotely and have not been
    stopped or cleaned up.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

Remove a specific job:

.. code-block:: console

    $ condor_rm 123.45

Remove all jobs currently not running:

.. code-block:: console

    $ condor_rm -constraint 'JobStatus =!= 2'

Remove all of user Bob's jobs:

.. code-block:: console

    # condor_rm bob

Remove only your held jobs:

.. code-block:: console

    $ condor_rm -hold

Remove a specific job, but only if it is held:

.. code-block:: console

    $ condor_rm -hold 123.45

Remove held jobs in cluster 123 whose ``ExitCode`` was 2:

.. code-block:: console

    $ condor_rm -hold -constraint 'ExitCode == 2' 123

See Also
--------

:tool:`condor_suspend`, :tool:`condor_continue`, :tool:`condor_hold`, :tool:`condor_release`,
:tool:`condor_vacate_job`, :tool:`condor_vacate`

Availability
------------

Linux, MacOS, Windows
