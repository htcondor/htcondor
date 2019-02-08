      

condor\_check\_userlogs
=======================

Check job event log files for errors

Synopsis
--------

**condor\_check\_userlogs** *UserLogFile1* [*UserLogFile2
…UserLogFileN*\ ]

Description
-----------

*condor\_check\_userlogs* is a program for checking a job event log or a
set of job event logs for errors. Output includes an indication that no
errors were found within a log file, or a list of errors such as an
execute or terminate event without a corresponding submit event, or
multiple terminated events for the same job.

*condor\_check\_userlogs* is especially useful for debugging
*condor\_dagman* problems. If *condor\_dagman* reports an error it is
often useful to run *condor\_check\_userlogs* on the relevant log files.

Exit Status
-----------

*condor\_check\_userlogs* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
