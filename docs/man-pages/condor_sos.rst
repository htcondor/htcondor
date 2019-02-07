      

condor\_sos
===========

Issue a command that will be serviced with a higher priority

Synopsis
--------

**condor\_sos** [**-help \| -version**\ ]

**condor\_sos** [**-debug**\ ] [**-timeoutmult  **\ *value*]
*condor\_command*

Description
-----------

*condor\_sos* sends the *condor\_command* in such a way that the command
is serviced ahead of other waiting commands. It appears to have a higher
priority than other waiting commands.

*condor\_sos* is intended to give administrators a way to query the
*condor\_schedd* and *condor\_collector* daemons when they are under
such a heavy load that they are not responsive.

There must be a special command port configured, in order for a command
to be serviced with priority. The *condor\_schedd* and
*condor\_collector* always have the special command port. Other daemons
require configuration by setting configuration variable
``<SUBSYS>_SUPER_ADDRESS_FILE``.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Print extra debugging information as the command executes.
 **-timeoutmult **\ *value*
    Multiply any timeouts set for the command by the integer *value*.

Examples
--------

The example command

::

      condor_sos -timeoutmult 5 condor_hold -all

causes the ``condor_hold -all`` command to be handled by the
*condor\_schedd* with priority over any other commands that the
*condor\_schedd* has waiting to be serviced. It also extends any set
timeouts by a factor of 5.

Exit Status
-----------

*condor\_sos* will exit with the value 1 on error and with the exit
value of the invoked command when the command is successfully invoked.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
