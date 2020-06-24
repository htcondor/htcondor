      

*condor_procd*
===============

Track and manage process families
:index:`condor_procd<single: condor_procd; HTCondor commands>`\ :index:`condor_procd command`

Synopsis
--------

**condor_procd** **-h**

**condor_procd** **-A** *address-file* [**options** ]

Description
-----------

*condor_procd* tracks and manages process families on behalf of the
HTCondor daemons. It may track families of PIDs via relationships such
as: direct parent/child, environment variables, UID, and supplementary
group IDs. Management of the PID families include

-  registering new families or new members of existing families
-  getting usage information
-  signaling families for operations such as suspension, continuing, or
   killing the family
-  getting a snapshot of the tree of families

In a regular HTCondor installation, this program is not intended to be
used or executed by any human.

The required argument, **-A** *address-file*, is the path and file
name of the address file which is the named pipe that clients must use
to speak with the *condor_procd*.

Options
-------

 **-h**
    Print out usage information and exit.
 **-D**
    Wait for the debugger. Initially sleep 30 seconds before beginning
    normal function.
 **-C** *principal*
    The *principal* is the UID of the owner of the named pipe that
    clients must use to speak to the *condor_procd*.
 **-L** *log-file*
    A file the *condor_procd* will use to write logging information.
 **-E**
    When specified, another tool such as the *procd_ctl* tool must
    allocate the GID associated with a process. When this option is not
    specified, the *condor_procd* will allocate the GID itself.
 **-P** *PID*
    If not specified, the *condor_procd* will use the
    *condor_procd* 's parent, which may not be PID 1 on Unix, as the
    parent of the *condor_procd* and the root of the tracking family.
    When not specified, if the *condor_procd* 's parent PID dies, the
    *condor_procd* exits. When specified, the *condor_procd* will
    track this *PID* family in question and not also exit if the PID
    exits.
 **-S** *seconds*
    The maximum number of seconds the *condor_procd* will wait between
    taking snapshots of the tree of families. Different clients to the
    *condor_procd* can specify different snapshot times. The quickest
    snapshot time is the one performed by the *condor_procd*. When this
    option is not specified, a default value of 60 seconds is used.
 **-G** *min-gid max-gid*
    If the **-E** option is not specified, then track process families
    using a self-allocated, free GID out of the inclusive range
    specified by *min-gid* and *max-gid*. This means that if a new
    process shows up using a previously known GID, the new process will
    automatically associate into the process family assigned that GID.
    If the **-E** option is specified, then instead of self-allocating
    the GID, the *procd_ctl* tool must be used to associate the GID
    with the PID root of the family. The associated GID must still be in
    the range specified. This is a Linux-only feature.
 **-K** *windows-softkill-binary*
    This is the path and executable name of the *condor_softkill.exe*
    binary. It is used to send softkill signals to process families.
    This is a Windows-only feature.
 **-I** *glexec-kill-path glexec-path*
    Specifies, with *glexec-kill-path*, the path and executable name of
    a binary used to send a signal to a PID. The *glexec* binary,
    specified by *glexec-path*, executes the program specified with
    *glexec-kill-path* under the right privileges to send the signal.

General Remarks
---------------

This program may be used in a stand alone mode, independent of HTCondor,
to track process families. The programs *procd_ctl* and *gidd_alloc*
are used with the *condor_procd* in stand alone mode to interact with
the daemon and to inquire about certain state of running processes on
the machine, respectively.

Exit Status
-----------

*condor_procd* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

