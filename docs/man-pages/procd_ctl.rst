      

procd_ctl
==========

command line interface to the *condor_procd*
:index:`procd_ctl<single: procd_ctl; HTCondor commands>`\ :index:`procd_ctl command`

Synopsis
--------

**procd_ctl** **-h**

**procd_ctl** **-A** *address-file* [**command** ]

Description
-----------

This is a programmatic interface to the *condor_procd* daemon. It may
be used to cause the *condor_procd* to do anything that the
*condor_procd* is capable of doing, such as tracking and managing
process families.

This is a program only available for the Linux ports of HTCondor.

The **-h** option prints out usage information and exits. The
*address-file* specification within the **-A** argument specifies the
path and file name of the address file which the named pipe clients must
use to speak with the *condor_procd*.

One command is given to the *condor_procd*. The choices for the command
are defined by the Options.

Options
-------

 **TRACK_BY_ASSOCIATED_GID** *GID* [*PID* ]
    Use the specified *GID* to track the specified family rooted at
    *PID*. If the optional *PID* is not specified, then the PID used is
    the one given or assumed by *condor_procd*.
 **GET_USAGE** [*PID* ]
    Get the total usage information about the PID family at *PID*. If
    the optional *PID* is not specified, then the PID used is the one
    given or assumed by *condor_procd*.
 **DUMP** [*PID* ]
    Print out information about both the root *PID* being watched and
    the tree of processes under this root *PID*. If the optional *PID*
    is not specified, then the PID used is the one given or assumed by
    *condor_procd*.
 **LIST** [*PID* ]
    With no *PID* given, print out information about all the watched
    processes. If the optional *PID* is specified, print out information
    about the process specified by *PID* and all its child processes.
 **SIGNAL_PROCESS** *signal* [*PID* ]
    Send the *signal* to the process specified by *PID*. If the optional
    *PID* is not specified, then the PID used is the one given or
    assumed by *condor_procd*.
 **SUSPEND_FAMILY** *PID*
    Suspend the process family rooted at *PID*.
 **CONTINUE_FAMILY** *PID*
    Continue execution of the process family rooted at *PID*.
 **KILL_FAMILY** *PID*
    Kill the process family rooted at *PID*.
 **UNREGISTER_FAMILY** *PID*
    Stop tracking the process family rooted at *PID*.
 **SNAPSHOT**
    Perform a snapshot of the tracked family tree.
 **QUIT**
    Disconnect from the *condor_procd* and exit.

General Remarks
---------------

This program may be used in a standalone mode, independent of HTCondor,
to track process families. The programs *procd_ctl* and *gidd_alloc*
are used with the *condor_procd* in standalone mode to interact with
the daemon and inquire about certain state of running processes on the
machine, respectively.

Exit Status
-----------

*procd_ctl* will exit with a status value of 0 (zero) upon success, and
it will exit with the value 1 (one) upon failure.

