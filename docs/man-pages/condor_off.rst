      

*condor_off*
=============

Shutdown HTCondor daemons
:index:`condor_off<single: condor_off; HTCondor commands>`\ :index:`condor_off command`

Synopsis
--------

**condor_off** [**-help | -version** ]

**condor_off** [**-graceful | -fast | -peaceful |
-force-graceful** ] [**-annex** *name*] [**-debug** ]
[**-pool** *centralmanagerhostname[:portnumber]*] [
**-name** *hostname* | *hostname* | **-addr** *"<a.b.c.d:port>"*
| *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all** ]
[**-daemon** *daemonname*]

Description
-----------

*condor_off* shuts down a set of the HTCondor daemons running on a set
of one or more machines. It does this cleanly so that checkpointable
jobs may gracefully exit with minimal loss of work.

The command *condor_off* without any arguments will shut down all
daemons except *condor_master*, unless **-annex** *name* is
specified. The *condor_master* can then handle both local and remote
requests to restart the other HTCondor daemons if need be. To restart
HTCondor running on a machine, see the *condor_on* command.

With the **-daemon** *master* option, *condor_off* will shut down all
daemons including the *condor_master*. Specification using the
**-daemon** option will shut down only the specified daemon.

For security reasons of authentication and authorization, this command
requires ``ADMINISTRATOR`` level of access.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-graceful**
    The default. If jobs are running, wait for up to the configured grace period for them to finish, then exit
 **-fast**
    Quickly shutdown daemons, immediately evicting any running jobs. A minimum of the first two characters of
    this option must be specified, to distinguish it from the
    **-force-graceful** command.
 **-peaceful**
    Wait indefinitely for jobs to finish
 **-force-graceful**
    Force a graceful shutdown, even after issuing a **-peaceful**
    command. A minimum of the first two characters of this option must
    be specified, to distinguish it from the **-fast** command.
 **-annex** *name*
    Turn off master daemons in the specified annex. By default this will
    result in the corresponding instances shutting down.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
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
 **-daemon** *daemonname*
    Send the command to the named daemon. Without this option, the
    command is sent to the *condor_master* daemon.

Graceful vs. Peaceful vs Fast
-----------------------------

A "fast" shutdown will cause the requested daemon to exit.  Jobs
running under a startd that is shutdown fast will be evicted. Jobs
running on a schedd that is shutdown fast will be left running for
their job lease duration (default of 20 minutes). (That is, assuming
the corresponding startd is not also being shut down). If that schedd restarts
before the job lease expires, it will reconnect to these running jobs
and continue to run them, as long as the schedd and startd are running.

A "graceful" shutdown of a schedd is functionally the same as a "fast"
shutdown of a schedd.

A "graceful" shutdown of a startd that has jobs running under it causes
the startd to wait for the jobs to exit of their own accord, up to the 
MaxJobRetirementTime.  After the MaxJobRetirementTime, the startd will evict
any remaining running jobs and exit.

A "peaceful" shutdown of a startd or schedd will cause that daemon to
wait indefinitely for all existing jobs to exit before shutting down.
During this time, no new jobs will start.

Exit Status
-----------

*condor_off* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Examples
--------

To shut down all daemons (other than *condor_master*) on the local
host:

.. code-block:: console

    $ condor_off

To shut down only the *condor_collector* on three named machines:

.. code-block:: console

    $ condor_off  cinnamon cloves vanilla -daemon collector

To shut down daemons within a pool of machines other than the local
pool, use the **-pool** option. The argument is the name of the central
manager for the pool. Note that one or more machines within the pool
must be specified as the targets for the command. This command shuts
down all daemons except the *condor_master* on the single machine named
**cae17** within the pool of machines that has **condor.cae.wisc.edu**
as its central manager:

.. code-block:: console

    $ condor_off  -pool condor.cae.wisc.edu -name cae17

