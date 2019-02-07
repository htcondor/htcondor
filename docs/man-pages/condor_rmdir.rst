      

condor\_rmdir
=============

Windows-only no-fail deletion of directories

Synopsis
--------

**condor\_rmdir** [**/HELP \| /?**\ ]

**condor\_rmdir** *@filename*

**condor\_rmdir** [**/VERBOSE**\ ] [**/DIAGNOSTIC**\ ]
[**/PATH:<path>**\ ] [**/S**\ ] [**/C**\ ] [**/Q**\ ] [**/NODEL**\ ]
*directory*

Description
-----------

*condor\_rmdir* can delete a specified *directory*, and will not fail if
the directory contains files that have ACLs that deny the SYSTEM process
delete access, unlike the built-in Windows *rmdir* command.

The directory to be removed together with other command line arguments
may be specified within a file named *filename*, prefixing this argument
with an ``@`` character.

The *condor\_rmdir.exe* executable is is intended to be used by HTCondor
with the **/S** **/C** options, which cause it to recurse into
subdirectories and continue on errors.

Options
-------

 **/HELP**
    Print usage information.
 **/?**
    Print usage information.
 **/VERBOSE**
    Print detailed output.
 **/DIAGNOSTIC**
    Print out the internal flow of control information.
 **/PATH:<path>**
    Remove the directory given by **<path>**.
 **/S**
    Include subdirectories in those removed.
 **/C**
    Continue even if access is denied.
 **/Q**
    Print error output only.
 **/NODEL**
    Do not remove directories. ACLs may still be changed.

Exit Status
-----------

*condor\_rmdir* will exit with a status value of 0 (zero) upon success,
and it will exit with the standard HRESULT error code upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
