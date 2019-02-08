      

procd\_ctl
==========

command line interface to the *condor\_procd*

Synopsis
--------

**procd\_ctl** **-h**

**procd\_ctl** **-A **\ *address-file* [**command**\ ]

Description
-----------

This is a programmatic interface to the *condor\_procd* daemon. It may
be used to cause the *condor\_procd* to do anything that the
*condor\_procd* is capable of doing, such as tracking and managing
process families.

This is a program only available for the Linux ports of HTCondor.

The **-h** option prints out usage information and exits. The
*address-file* specification within the **-A** argument specifies the
path and file name of the address file which the named pipe clients must
use to speak with the *condor\_procd*.

One command is given to the *condor\_procd*. The choices for the command
are defined by the Options.

Options
-------

 **TRACK\_BY\_ASSOCIATED\_GID** *GID* [*PID*\ ]
    Use the specified *GID* to track the specified family rooted at
    *PID*. If the optional *PID* is not specified, then the PID used is
    the one given or assumed by *condor\_procd*.
 **GET\_USAGE** [*PID*\ ]
    Get the total usage information about the PID family at *PID*. If
    the optional *PID* is not specified, then the PID used is the one
    given or assumed by *condor\_procd*.
 **DUMP** [*PID*\ ]
    Print out information about both the root *PID* being watched and
    the tree of processes under this root *PID*. If the optional *PID*
    is not specified, then the PID used is the one given or assumed by
    *condor\_procd*.
 **LIST** [*PID*\ ]
    With no *PID* given, print out information about all the watched
    processes. If the optional *PID* is specified, print out information
    about the process specified by *PID* and all its child processes.
 **SIGNAL\_PROCESS** *signal* [*PID*\ ]
    Send the *signal* to the process specified by *PID*. If the optional
    *PID* is not specified, then the PID used is the one given or
    assumed by *condor\_procd*.
 **SUSPEND\_FAMILY** *PID*
    Suspend the process family rooted at *PID*.
 **CONTINUE\_FAMILY** *PID*
    Continue execution of the process family rooted at *PID*.
 **KILL\_FAMILY** *PID*
    Kill the process family rooted at *PID*.
 **UNREGISTER\_FAMILY** *PID*
    Stop tracking the process family rooted at *PID*.
 **SNAPSHOT**
    Perform a snapshot of the tracked family tree.
 **QUIT**
    Disconnect from the *condor\_procd* and exit.

General Remarks
---------------

This program may be used in a standalone mode, independent of HTCondor,
to track process families. The programs *procd\_ctl* and *gidd\_alloc*
are used with the *condor\_procd* in standalone mode to interact with
the daemon and inquire about certain state of running processes on the
machine, respectively.

Exit Status
-----------

*procd\_ctl* will exit with a status value of 0 (zero) upon success, and
it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
