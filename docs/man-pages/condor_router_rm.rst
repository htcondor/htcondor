      

condor\_router\_rm
==================

Remove jobs being managed by the HTCondor Job Router

Synopsis
--------

**condor\_router\_rm** [*router\_rm options*\ ] [*condor\_rm options*\ ]

Description
-----------

*condor\_router\_rm* is a script that provides additional features above
those offered by *condor\_rm*, for removing jobs being managed by the
HTCondor Job Router.

The options that may be supplied to *condor\_router\_rm* belong to two
groups:

-  **router\_rm options** provide the additional features
-  **condor\_rm options** are those options already offered by
   *condor\_rm*. See the *condor\_rm* manual page for specification of
   these options.

Options
-------

 **-constraint **\ *X*
    (router\_rm option) Remove jobs matching the constraint specified by
    *X*
 **-held**
    (router\_rm option) Remove only jobs in the hold state
 **-idle**
    (router\_rm option) Remove only idle jobs
 **-route **\ *name*
    (router\_rm option) Remove only jobs on specified route

Exit Status
-----------

*condor\_router\_rm* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
