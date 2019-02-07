      

condor\_tail
============

Display the last contents of a running job’s standard output or file

Synopsis
--------

**condor\_tail** [**-help**\ ] \| [**-version**\ ]

**condor\_tail** [**-pool  **\ *centralmanagerhostname[:portnumber]*]
[**-name  **\ *name*] [**-debug**\ ] [**-maxbytes  **\ *numbytes*]
[**-auto-retry**\ ] [**-follow**\ ] [**-no-stdout**\ ] [**-stderr**\ ]
*job-ID* [*filename1*\ ] [*filename2 …*\ ]

Description
-----------

*condor\_tail* displays the last bytes of a file in the sandbox of a
running job identified by the command line argument *job-ID*. ``stdout``
is tailed by default. The number of bytes displayed is limited to 1024,
unless changed by specifying the **-maxbytes** option. This limit is
applied for each individual tail of a file; for example, when following
a file, the limit is applied each subsequent time output is obtained.

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display version information and exit.
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number.
 **-name **\ *name*
    Query the *condor\_schedd* daemon identified with *name*.
 **-debug**
    Display extra debugging information.
 **-maxbytes **\ *numbytes*
    Limits the maximum number of bytes transferred per tail access. If
    not specified, the maximum number of bytes is 1024.
 **-auto-retry**
    Retry the tail of the file(s) every 2 seconds, if the job is not yet
    running.
 **-follow**
    Repetitively tail the file(s), until interrupted.
 **-no-stdout**
    Do not tail ``stdout``.
 **-stderr**
    Tail ``stderr`` instead of ``stdout``.

Exit Status
-----------

The exit status of *condor\_tail* is zero on success.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
