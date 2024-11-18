*condor_vacate_job*
===================

Stop running job(s) and relinquish resources back to the host machine.

:index:`condor_vacate_job<double: condor_vacate_job; HTCondor commands>`

Synopsis
--------

**condor_vacate_job** [**-help** | **-version**]

**condor_vacate_job** [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_vacate_job** [**-debug**] [**-long**] [**-totals**] [**-all**] [**-constraint** *expression*]
[**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*] [**-fast**]

Description
-----------

Vacate job(s) from the host machine(s) where they are currently running;
relinquishing resources back to the Execution Point and returning the job(s)
back to idle to restart resource matchmaking and execution elsewhere. For
any given job, only the owner of the job or one of the queue super users
(defined by the :macro:`QUEUE_SUPER_USERS` macro) can vacate the job.

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
 **-fast**
    Perform a fast vacate and hard kill the jobs.
 **-pool** *hostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number.
 **-name** *scheddname*
    Send the command to a machine identified by *scheddname*.
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*.
 **-constraint** *expression*
    Vacate all jobs which match the job ClassAd expression constraint.
 **-all**
    Vacate all the jobs in the queue.
 *cluster*
    Vacate all jobs in the specified cluster.
 *cluster.process*
    Vacate the specific job in the cluster.
 *user*
    Vacate jobs belonging to specified user.

General Remarks
---------------

If the **-name** option is specified, the named *condor_schedd* is targeted
for processing. Otherwise, the local *condor_schedd* is targeted.

Vacated jobs will be sent a soft kill signal (``SIGTERM`` by default or
the value of :ad-attr:`KillSig` in the job's ClassAd) unless **-fast** is used.

Using *condor_vacate_job* on jobs which are not currently running has
no effect.

.. warning::

    Do not confuse this tool with :tool:`condor_vacate`. :tool:`condor_vacate`
    is intended for use by the owners/administrators of an Execution Point to
    evict jobs from their host machine.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

To vacate a specific:

.. code-block:: console

    $ condor_vacate_job 23.0

To vacate a job fast:

.. code-block:: console

    $ condor_vacate_job -fast 23.0

To vacate all jobs owned by user Mary:

.. code-block:: console

    # condor_vacate_job mary

To vacate all vanilla universe jobs owned by Mary:

.. code-block:: console

    # condor_vacate_job -constraint 'JobUniverse == 5 && Owner == "mary"'

See Also
--------

:tool:`condor_vacate`, :tool:`condor_rm`, :tool:`condor_continue`, :tool:`condor_suspend`,
:tool:`condor_hold`, :tool:`condor_release`

Availability
------------

Linux, MacOS, Windows
