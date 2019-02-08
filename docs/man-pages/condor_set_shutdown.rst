      

condor\_set\_shutdown
=====================

Set a program to execute upon *condor\_master* shut down

Synopsis
--------

**condor\_set\_shutdown** [**-help \| -version**\ ]

**condor\_set\_shutdown** **-exec **\ *programname* [**-debug**\ ]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [
**-name **\ *hostname* \| *hostname* \| **-addr **\ *"<a.b.c.d:port>"*
\| *"<a.b.c.d:port>"* \| **-constraint **\ *expression* \| **-all** ]

Description
-----------

*condor\_set\_shutdown* sets a program (typically a script) to execute
when the *condor\_master* daemon shuts down. The
**-exec **\ *programname* argument is required, and specifies the
program to run. The string *programname* must match the string that
defines ``Name`` in the configuration variable
``MASTER_SHUTDOWN_<Name>`` in the *condor\_master* daemon’s
configuration. If it does not match, the *condor\_master* will log an
error and ignore the request.

For security reasons of authentication and authorization, this command
requires ADMINISTRATOR level of access.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number
 **-name **\ *hostname*
    Send the command to a machine identified by *hostname*
 *hostname*
    Send the command to a machine identified by *hostname*
 **-addr **\ *"<a.b.c.d:port>"*
    Send the command to a machine’s master located at *"<a.b.c.d:port>"*
 *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-constraint **\ *expression*
    Apply this command only to machines matching the given ClassAd
    *expression*
 **-all**
    Send the command to all machines in the pool

Exit Status
-----------

*condor\_set\_shutdown* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To have all *condor\_master* daemons run the program */bin/reboot* upon
shut down, configure the *condor\_master* to contain a definition
similar to:

::

    MASTER_SHUTDOWN_REBOOT = /sbin/reboot

where ``REBOOT`` is an invented name for this program that the
*condor\_master* will execute. On the command line, run

::

    % condor_set_shutdown -exec reboot -all 
    % condor_off -graceful -all

where the string reboot matches the invented name.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
