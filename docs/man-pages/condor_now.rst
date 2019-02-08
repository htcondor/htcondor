      

condor\_now
===========

Start a job now.

Synopsis
--------

**condor\_now** **-help**

**condor\_now** [**-name  **\ **] [**-debug**\ ] *now-job* *vacate-job*

Description
-----------

*condor\_now* tries to run the *now-job* now. The *vacate-job* is
immediately vacated; after it terminates, if the schedd still has the
claim to the vacated job’s slot – and it usually will – the schedd will
immediately start the now-job on that slot.

You must specify each job using both the cluster and proc IDs.

Options
-------

 **-help**
    Print a usage reminder.
 **-debug**
    Print debugging output. Control the verbosity with the environment
    variables \_CONDOR\_TOOL\_DEBUG, as usual.
 **-name **\ **
    Specify the scheduler(’s name) and (optionally) the pool to find it
    in.

General Remarks
---------------

The now-job and the vacated-job must have the same owner; if you are not
the queue super-user, you must own both jobs. The jobs must be on the
same schedd, and both jobs must be in the vanilla universe. The now-job
must be idle and the vacated-job must be running.

Examples
--------

To begin running job 17.3 as soon as possible using job 4.2’s slot:

::

      condor_now 17.3 4.2

To try to figure out why that doesn’t work for the ‘magic’ scheduler in
the ’gandalf’ pool, set the environment variable \_CONDOR\_TOOL\_DEBUG
to ‘D\_FULLDEBUG’ and then:

::

      condor_now -debug -schedd magic -pool gandalf 17.3 4.2

Exit Status
-----------

*condor\_now* will exit with a status value of 0 (zero) if the schedd
accepts its request to vacate the vacate-job and start the now-job in
its place. It does not wait for the now-job to have started running.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
