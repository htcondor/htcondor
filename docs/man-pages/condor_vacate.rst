      

*condor_vacate*
================

Vacate jobs that are running on the specified hosts
:index:`condor_vacate<single: condor_vacate; HTCondor commands>`\ :index:`condor_vacate command`

Synopsis
--------

**condor_vacate** [**-help | -version** ]

**condor_vacate** [**-graceful | -fast** ] [**-debug** ]
[**-pool** *centralmanagerhostname[:portnumber]*] [
**-name** *hostname* | *hostname* | **-addr** *"<a.b.c.d:port>"*
| *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all** ]

Description
-----------

*condor_vacate* causes HTCondor force jobs to vacate from a given set of
machines. The job(s) remains
in the submitting machine's job queue.

Given the (default) **-graceful** option, jobs are killed
and HTCondor restarts the job from the
beginning somewhere else. *condor_vacate* has no effect on a machine
with no HTCondor job currently running.

There is generally no need for the user or administrator to explicitly
run *condor_vacate*. HTCondor takes care of jobs in this way
automatically following the policies given in configuration files.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-graceful**
    Give the job a change to shut down cleanly, then soft-kill it.
 **-fast**
    Hard-kill jobs instead of giving them to shut down cleanly.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-name** *hostname*
    Send the command to a machine identified by *hostname*
 *hostname*
    Send the command to a machine identified by *hostname*
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine's master located at *"<a.b.c.d:port>"*
 *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-constraint** *expression*
    Apply this command only to machines matching the given ClassAd
    *expression*
 **-all**
    Send the command to all machines in the pool

Exit Status
-----------

*condor_vacate* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Examples
--------

To send a *condor_vacate* command to two named machines:

.. code-block:: console

    $ condor_vacate  robin cardinal

To send the *condor_vacate* command to a machine within a pool of
machines other than the local pool, use the **-pool** option. The
argument is the name of the central manager for the pool. Note that one
or more machines within the pool must be specified as the targets for
the command. This command sends the command to a the single machine
named **cae17** within the pool of machines that has
**condor.cae.wisc.edu** as its central manager:

.. code-block:: console

    $ condor_vacate -pool condor.cae.wisc.edu -name cae17

