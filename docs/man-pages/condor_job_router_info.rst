      

condor\_job\_router\_info
=========================

Discover and display information related to job routing

Synopsis
--------

**condor\_job\_router\_info** [**-help \| -version**\ ]

**condor\_job\_router\_info** **-config**

**condor\_job\_router\_info** **** [**-ignore-prior-routing**\ ]

Description
-----------

*condor\_job\_router\_info* displays information about job routing. The
information will be either the available, configured routes or the
routes for specified jobs.

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display HTCondor version information and exit.
 **-config**
    Display configured routes.
 **-match-jobs**
    For each job listed in the file specified by the **-jobads** option,
    display the first route found.
 **-ignore-prior-routing**
    For each job, remove any existing routing ClassAd attributes, and
    set attribute ``JobStatus`` to the Idle state before finding the
    first route.
 **-jobads **\ *filename*
    Read job ClassAds from file *filename*. If *filename* is ``-``, then
    read from ``stdin``.

Exit Status
-----------

*condor\_job\_router\_info* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
