      

condor\_fetchlog
================

Retrieve a daemon’s log file that is located on another computer

Synopsis
--------

**condor\_fetchlog** [**-help \| -version**\ ]

**condor\_fetchlog**
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [**-master \|
-startd \| -schedd \| -collector \| -negotiator \| -kbdd**\ ]
*machine-name* *subsystem[.extension]*

Description
-----------

*condor\_fetchlog* contacts HTCondor running on the machine specified by
*machine-name*, and asks it to return a log file from that machine.
Which log file is determined from the *subsystem[.extension]* argument.
The log file is printed to standard output. This command eliminates the
need to remotely log in to a machine in order to retrieve a daemon’s log
file.

For security purposes of authentication and authorization, this command
requires ``ADMINISTRATOR`` level of access.

The *subsystem[.extension]* argument is utilized to construct the log
file’s name. Without an optional *.extension*, the value of the
configuration variable named *subsystem*\ \_LOG defines the log file’s
name. When specified, the *.extension* is appended to this value.

The *subsystem* argument is any value ``$(SUBSYSTEM)`` that has a
defined configuration variable of ``$(SUBSYSTEM)_LOG``, or any of

-  ``NEGOTIATOR_MATCH``
-  ``HISTORY``
-  ``STARTD_HISTORY``

A value for the optional *.extension* to the *subsystem* argument is
typically one of the three strings:

#. ``.old``
#. ``.slot<X>``
#. ``.slot<X>.old``

Within these strings, <X> is substituted with the slot number.

A *subsystem* argument of ``STARTD_HISTORY`` fetches all
*condor\_startd* history by concatenating all instances of log files
resulting from rotation.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number
 **-master**
    Send the command to the *condor\_master* daemon (default)
 **-startd**
    Send the command to the *condor\_startd* daemon
 **-schedd**
    Send the command to the *condor\_schedd* daemon
 **-collector**
    Send the command to the *condor\_collector* daemon
 **-kbdd**
    Send the command to the *condor\_kbdd* daemon

Examples
--------

To get the *condor\_negotiator* daemon’s log from a host named
``head.example.com`` from within the current pool:

::

    condor_fetchlog head.example.com NEGOTIATOR

To get the *condor\_startd* daemon’s log from a host named
``execute.example.com`` from within the current pool:

::

    condor_fetchlog execute.example.com STARTD

This command requested the *condor\_startd* daemon’s log from the
*condor\_master*. If the *condor\_master* has crashed or is
unresponsive, ask another daemon running on that computer to return the
log. For example, ask the *condor\_startd* daemon to return the
*condor\_master*\ ’s log:

::

    condor_fetchlog -startd execute.example.com MASTER

Exit Status
-----------

*condor\_fetchlog* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
