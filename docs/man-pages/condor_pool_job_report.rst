      

condor\_pool\_job\_report
=========================

generate report about all jobs that have run in the last 24 hours on all
execute hosts

Synopsis
--------

**condor\_pool\_job\_report**

Description
-----------

*condor\_pool\_job\_report* is a Linux-only tool that is designed to be
run nightly using *cron*. It is intended to be run on the central
manager, or another machine that has administrative permissions, and is
able to fetch the *condor\_startd* history logs from all of the
*condor\_startd* daemons in the pool. After fetching these logs,
*condor\_pool\_job\_report* then generates a report about job run times
and mails it to administrators, as defined by configuration variable
``CONDOR_ADMIN`` .

Exit Status
-----------

*condor\_pool\_job\_report* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
