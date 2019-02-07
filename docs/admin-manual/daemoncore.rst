      

DaemonCore
==========

This section is a brief description of DaemonCore. DaemonCore is a
library that is shared among most of the HTCondor daemons which provides
common functionality. Currently, the following daemons use DaemonCore:

-  *condor\_master*
-  *condor\_startd*
-  *condor\_schedd*
-  *condor\_collector*
-  *condor\_negotiator*
-  *condor\_kbdd*
-  *condor\_gridmanager*
-  *condor\_credd*
-  *condor\_had*
-  *condor\_replication*
-  *condor\_transferer*
-  *condor\_job\_router*
-  *condor\_lease\_manager*
-  *condor\_rooster*
-  *condor\_shared\_port*
-  *condor\_defrag*
-  *condor\_c-gahp*
-  *condor\_c-gahp\_worker\_thread*
-  *condor\_dagman*
-  *condor\_ft-gahp*
-  *condor\_rooster*
-  *condor\_shadow*
-  *condor\_shared\_port*
-  *condor\_transferd*
-  *condor\_vm-gahp*
-  *condor\_vm-gahp-vmware*

Most of DaemonCore’s details are not interesting for administrators.
However, DaemonCore does provide a uniform interface for the daemons to
various Unix signals, and provides a common set of command-line options
that can be used to start up each daemon.

DaemonCore and Unix signals
---------------------------

One of the most visible features that DaemonCore provides for
administrators is that all daemons which use it behave the same way on
certain Unix signals. The signals and the behavior DaemonCore provides
are listed below:

 SIGHUP
    Causes the daemon to reconfigure itself.
 SIGTERM
    Causes the daemon to gracefully shutdown.
 SIGQUIT
    Causes the daemon to quickly shutdown.

Exactly what gracefully and quickly means varies from daemon to daemon.
For daemons with little or no state (the *condor\_kbdd*,
*condor\_collector* and *condor\_negotiator*) there is no difference,
and both ``SIGTERM`` and ``SIGQUIT`` signals result in the daemon
shutting itself down quickly. For the *condor\_master*, a graceful
shutdown causes the *condor\_master* to ask all of its children to
perform their own graceful shutdown methods. The quick shutdown causes
the *condor\_master* to ask all of its children to perform their own
quick shutdown methods. In both cases, the *condor\_master* exits after
all its children have exited. In the *condor\_startd*, if the machine is
not claimed and running a job, both the ``SIGTERM`` and ``SIGQUIT``
signals result in an immediate exit. However, if the *condor\_startd* is
running a job, a graceful shutdown results in that job writing a
checkpoint, while a fast shutdown does not. In the *condor\_schedd*, if
there are no jobs currently running, there will be no *condor\_shadow*
processes, and both signals result in an immediate exit. However, with
jobs running, a graceful shutdown causes the *condor\_schedd* to ask
each *condor\_shadow* to gracefully vacate the job it is serving, while
a quick shutdown results in a hard kill of every *condor\_shadow*, with
no chance to write a checkpoint.

For all daemons, a reconfigure results in the daemon re-reading its
configuration file(s), causing any settings that have changed to take
effect. See
section \ `3.3 <IntroductiontoConfiguration.html#x31-1690003.3>`__ on
page \ `515 <IntroductiontoConfiguration.html#x31-1690003.3>`__,
Configuring HTCondor for full details on what settings are in the
configuration files and what they do.

DaemonCore and Command-line Arguments
-------------------------------------

The second visible feature that DaemonCore provides to administrators is
a common set of command-line arguments that all daemons understand.
These arguments and what they do are described below:

 -a string
    Append a period character ('.') concatenated with **string** to the
    file name of the log for this daemon, as specified in the
    configuration file.
 -b
    Causes the daemon to start up in the background. When a DaemonCore
    process starts up with this option, it disassociates itself from the
    terminal and forks itself, so that it runs in the background. This
    is the default behavior for HTCondor daemons.
 -c filename
    Causes the daemon to use the specified **filename** as a full path
    and file name as its global configuration file. This overrides the
    ``CONDOR_CONFIG`` environment variable and the regular locations
    that HTCondor checks for its configuration file.
 -d
    Use dynamic directories. The ``$(LOG)``, ``$(SPOOL)``, and
    ``$(EXECUTE)`` directories are all created by the daemon at run
    time, and they are named by appending the parent’s IP address and
    PID to the value in the configuration file. These values are then
    inherited by all children of the daemon invoked with this **-d**
    argument. For the *condor\_master*, all HTCondor processes will use
    the new directories. If a *condor\_schedd* is invoked with the *-d*
    argument, then only the *condor\_schedd* daemon and any
    *condor\_shadow* daemons it spawns will use the dynamic directories
    (named with the *condor\_schedd* daemon’s PID).

    Note that by using a dynamically-created spool directory named by
    the IP address and PID, upon restarting daemons, jobs submitted to
    the original *condor\_schedd* daemon that were stored in the old
    spool directory will not be noticed by the new *condor\_schedd*
    daemon, unless you manually specify the old, dynamically-generated
    ``SPOOL`` directory path in the configuration of the new
    *condor\_schedd* daemon.

 -f
    Causes the daemon to start up in the foreground. Instead of forking,
    the daemon runs in the foreground.

    NOTE: When the *condor\_master* starts up daemons, it does so with
    the **-f** option, as it has already forked a process for the new
    daemon. There will be a **-f** in the argument list for all HTCondor
    daemons that the *condor\_master* spawns.

 -k filename
    For non-Windows operating systems, causes the daemon to read out a
    PID from the specified **filename**, and send a SIGTERM to that
    process. The daemon started with this optional argument waits until
    the daemon it is attempting to kill has exited.
 -l directory
    Overrides the value of ``LOG`` as specified in the configuration
    files. Primarily, this option is used with the *condor\_kbdd* when
    it needs to run as the individual user logged into the machine,
    instead of running as root. Regular users would not normally have
    permission to write files into HTCondor’s log directory. Using this
    option, they can override the value of ``LOG`` and have the
    *condor\_kbdd* write its log file into a directory that the user has
    permission to write to.
 -local-name name
    Specify a local name for this instance of the daemon. This local
    name will be used to look up configuration parameters.
    Section \ `3.3.3 <IntroductiontoConfiguration.html#x31-1720003.3.3>`__
    contains details on how this local name will be used in the
    configuration.
 -p port
    Causes the daemon to bind to the specified port as its command
    socket. The *condor\_master* daemon uses this option to ensure that
    the *condor\_collector* and *condor\_negotiator* start up using
    well-known ports that the rest of HTCondor depends upon them using.
 -pidfile filename
    Causes the daemon to write out its PID (process id number) to the
    specified **filename**. This file can be used to help shutdown the
    daemon without first searching through the output of the Unix *ps*
    command.

    Since daemons run with their current working directory set to the
    value of ``LOG``, if a full path (one that begins with a slash
    character, ``/``) is not specified, the file will be placed in the
    ``LOG`` directory.

 -q
    Quiet output; write less verbose error messages to ``stderr`` when
    something goes wrong, and before regular logging can be initialized.
 -r minutes
    Causes the daemon to set a timer, upon expiration of which, it sends
    itself a SIGTERM for graceful shutdown.
 -t
    Causes the daemon to print out its error message to ``stderr``
    instead of its specified log file. This option forces the **-f**
    option.
 -v
    Causes the daemon to print out version information and exit.

      
