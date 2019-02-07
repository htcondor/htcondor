      

condor\_prio
============

change priority of jobs in the HTCondor queue

Synopsis
--------

**condor\_prio** **-p **\ *priority* \| **+ **\ *value* \|
**- **\ *value* [**-n  **\ *schedd\_name*] **

**condor\_prio** **-p **\ *priority* \| **+ **\ *value* \|
**- **\ *value* [**-pool **\ *pool\_name* **-n **\ *schedd\_name* ]\ **

Description
-----------

*condor\_prio* changes the priority of one or more jobs in the HTCondor
queue. If the job identification is given by *cluster.process*,
*condor\_prio* attempts to change the priority of the single job with
job ClassAd attributes ``ClusterId`` and ``ProcId``. If described by
*cluster*, *condor\_prio* attempts to change the priority of all
processes with the given ``ClusterId`` job ClassAd attribute. If
*username* is specified, *condor\_prio* attempts to change priority of
all jobs belonging to that user. For **-a**, *condor\_prio* attempts to
change priority of all jobs in the queue.

The user must set a new priority with the **-p** option, or specify a
priority adjustment. The priority of a job can be any integer, with
higher numbers corresponding to greater priority. For adjustment of the
current priority, **+ **\ *value* increases the priority by the amount
given with *value*. **- **\ *value* decreases the priority by the amount
given with *value*.

Only the owner of a job or the super user can change the priority.

The priority changed by *condor\_prio* is only used when comparing to
the priority jobs owned by the same user and submitted from the same
machine.

Options
-------

 **-n **\ *schedd\_name*
    Change priority of jobs queued at the specified *condor\_schedd* in
    the local pool.
 **-pool **\ *pool\_name* **-n **\ *schedd\_name*
    Change priority of jobs queued at the specified *condor\_schedd* in
    the specified pool.

Exit Status
-----------

*condor\_prio* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
