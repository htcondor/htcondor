*condor_evicted_files*
======================

Inspect the file(s) that HTCondor is holding on to as a result of a job
being evicted when ``when_to_transfer_files = TRUE``, or checkpointing
when `CheckpointExitCode`` is set.

:index:`condor_evicted_files<single: condor_evicted_files; HTCondor commands>`
:index:`condor_evicted_files command`

Synopsis
--------

**condor_evicted_files** [COMMAND] <clusterID>.<procID>[ <clusterID.<procID>]*

Description
-----------

Print the directory or directories HTCondor is using to store files for the
specified job or jobs.  If COMMAND is specified:

    dir -- Print the directory (for each job) in which the file(s) are stored.
    list -- List the contents of the directory (for each job).
    get -- Copy the contents of the directory to a subdirectory named after
           each job's ID.

Exit Status
-----------

Returns 0 on success.

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.
