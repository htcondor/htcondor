*condor_vacate*
===============

Vacate jobs that are running on the specified hosts.

:index:`condor_vacate<double: condor_vacate; HTCondor commands>`

Synopsis
--------

**condor_vacate** [**-help** | **-version**]

**condor_vacate** [**-graceful** | **-fast**] [**-debug**] [**-pool** *hostname[:portnumber]*]
[**-name** *hostname* | **-addr** *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all**]

Description
-----------

*condor_vacate* causes HTCondor to force jobs to vacate from a given set of
machines. The job(s) remain in the submitting machine's job queue.

Given the (default) **-graceful** option, jobs are killed
and HTCondor restarts the job from the
beginning somewhere else. *condor_vacate* has no effect on a machine
with no HTCondor job currently running.

There is generally no need for the user or administrator to explicitly
run *condor_vacate*. HTCondor takes care of jobs in this way
automatically following the policies given in configuration files.

.. warning::

    Do not confuse this tool with :tool:`condor_vacate_job`.
    :tool:`condor_vacate_job` is intended for use by job owners to
    vacate their specific jobs. *condor_vacate* is intended for use
    by machine owners/administrators to vacate machines.

Options
-------

 **-help**
    Display usage information.
 **-version**
    Display version information.
 **-graceful**
    Give the job a chance to shut down cleanly, then soft-kill it. (Default)
 **-fast**
    Hard-kill jobs instead of giving them time to shut down cleanly.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-pool** *hostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number.
 **-name** *hostname*
    Send the command to a machine identified by *hostname*.
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine's master located at *"<a.b.c.d:port>"*.
 **-constraint** *expression*
    Apply this command only to machines matching the given ClassAd
    *expression*.
 **-all**
    Send the command to all machines in the pool.

Exit Status
-----------

0  -  Success

1  -  Failure

Examples
--------

Vacate two named machines:

.. code-block:: console

    $ condor_vacate robin cardinal

Vacate a machine in a different pool:

.. code-block:: console

    $ condor_vacate -pool condor.cae.wisc.edu -name cae17

Vacate all machines matching a constraint:

.. code-block:: console

    $ condor_vacate -constraint 'Machine == "exec-*.example.com"'

See Also
--------

:tool:`condor_vacate_job`, :tool:`condor_off`, :tool:`condor_on`, :tool:`condor_restart`

Availability
------------

Linux, MacOS, Windows

