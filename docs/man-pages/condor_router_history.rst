      

condor\_router\_history
=======================

Display the history for routed jobs

Synopsis
--------

**condor\_router\_history** [--**h**]

**condor\_router\_history** [--**show\_records**] [--**show\_iwd**]
[--**age** *days*] [--**days** *days*] [--**start** *"YYYY-MM-DD
HH:MM"*]

Description
-----------

*condor\_router\_history* summarizes statistics for routed jobs over the
previous 24 hours. With no command line options, statistics for run
time, number of jobs completed, and number of jobs aborted are listed
per route (site).

Options
-------

 **—h**
    Display usage information and exit.
 **—show\_records**
    Displays individual records in addition to the summary.
 **—show\_iwd**
    Include working directory in displayed records.
 **—age **\ *days*
    Set the ending time of the summary to be *days* days ago.
 **—days **\ *days*
    Set the number of days to summarize.
 **—start **\ *"YYYY-MM-DD HH:MM"*
    Set the start time of the summary.

Exit Status
-----------

*condor\_router\_history* will exit with a status of 0 (zero) upon
success, and non-zero otherwise.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
