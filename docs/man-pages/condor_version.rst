      

condor\_version
===============

print HTCondor version and platform information

Synopsis
--------

**condor\_version** [**-help**\ ]

**condor\_version** [**-arch**\ ] [**-opsys**\ ] [**-syscall**\ ]

Description
-----------

With no arguments, *condor\_version* prints the currently installed
HTCondor version number and platform information. The version number
includes a build identification number, as well as the date built.

Options
-------

 **-help**
    Print usage information
 **-arch**
    Print this machine’s ClassAd value for ``Arch``
 **-opsys**
    Print this machine’s ClassAd value for ``OpSys``
 **-syscall**
    Get any requested version and/or platform information from the
    ``libcondorsyscall.a`` that this HTCondor pool is configured to use,
    instead of using the values that are compiled into the tool itself.
    This option may be used in combination with any other options to
    modify where the information is coming from.

Exit Status
-----------

*condor\_version* will exit with a status value of 0 (zero) upon
success, and it should never exit with a failing value.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
