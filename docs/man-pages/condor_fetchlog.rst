*condor_fetchlog*
==================

Retrieve a daemon's log file that is located on another computer
:index:`condor_fetchlog<single: condor_fetchlog; HTCondor commands>`
:index:`condor_fetchlog command`

Synopsis
--------

**condor_fetchlog** [**-help | -version** ]

**condor_fetchlog**
[**-pool** *centralmanagerhostname[:portnumber]*] [**-master |
-startd | -schedd | -collector | -negotiator | -kbdd** ]
*machine-name* *subsystem[.extension]*

Description
-----------

*condor_fetchlog* contacts HTCondor running on the machine specified by
*machine-name*, and asks it to return a log file from that machine.
Which log file is determined from the *subsystem[.extension]* argument.
The log file is printed to standard output. This command eliminates the
need to remotely log in to a machine in order to retrieve a daemon's log
file.

For security purposes of authentication and authorization, this command
requires ``ADMINISTRATOR`` level of access.

The *subsystem[.extension]* argument is utilized to construct the log
file's name. Without an optional *.extension*, the value of the
configuration variable named *subsystem* _LOG defines the log file's
name. When specified, the *.extension* is appended to this value.

The *subsystem* argument is any value ``$(SUBSYSTEM)`` that has a
defined configuration variable of ``$(SUBSYSTEM)_LOG``, or any of

-  ``NEGOTIATOR_MATCH``
-  :macro:`HISTORY`
-  :macro:`STARTD_HISTORY`

A value for the optional *.extension* to the *subsystem* argument is
typically one of the three strings:

#. ``.old``
#. ``.slot<X>``
#. ``.slot<X>.old``

Within these strings, <X> is substituted with the slot number.

A *subsystem* argument of :macro:`STARTD_HISTORY` fetches all
*condor_startd* history by concatenating all instances of log files
resulting from rotation.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-master**
    Send the command to the *condor_master* daemon (default)
 **-startd**
    Send the command to the *condor_startd* daemon
 **-schedd**
    Send the command to the *condor_schedd* daemon
 **-collector**
    Send the command to the *condor_collector* daemon
 **-kbdd**
    Send the command to the *condor_kbdd* daemon

Examples
--------

To get the *condor_negotiator* daemon's log from a host named
``head.example.com`` from within the current pool:

.. code-block:: console

    $ condor_fetchlog head.example.com NEGOTIATOR

To get the *condor_startd* daemon's log from a host named
``execute.example.com`` from within the current pool:

.. code-block:: console

    $ condor_fetchlog execute.example.com STARTD

This command requested the *condor_startd* daemon's log from the
*condor_master*. If the *condor_master* has crashed or is
unresponsive, ask another daemon running on that computer to return the
log. For example, ask the *condor_startd* daemon to return the
*condor_master* 's log:

.. code-block:: console

    $ condor_fetchlog -startd execute.example.com MASTER

Exit Status
-----------

*condor_fetchlog* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

