      

condor\_router\_q
=================

Display information about routed jobs in the queue

Synopsis
--------

**condor\_router\_q** [**-S**\ ] [**-R**\ ] [**-I**\ ] [**-H**\ ]
[**-route  **\ *name*] [**-idle**\ ] [**-held**\ ]
[**-constraint  **\ *X*] [**condor\_q options**\ ]

Description
-----------

*condor\_router\_q* displays information about jobs managed by the
*condor\_job\_router* that are in the HTCondor job queue. The
functionality of this tool is that of *condor\_q*, with additional
options specialized for routed jobs. Therefore, any of the options for
*condor\_q* may also be used with *condor\_router\_q*.

Options
-------

 **-S**
    Summarize the state of the jobs on each route.
 **-R**
    Summarize the running jobs on each route.
 **-I**
    Summarize the idle jobs on each route.
 **-H**
    Summarize the held jobs on each route.
 **-route **\ *name*
    Display only the jobs on the route identified by *name*.
 **-idle**
    Display only the idle jobs.
 **-held**
    Display only the held jobs.
 **-constraint **\ *X*
    Display only the jobs matching constraint *X*.

Exit Status
-----------

*condor\_router\_q* will exit with a status of 0 (zero) upon success,
and non-zero otherwise.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
