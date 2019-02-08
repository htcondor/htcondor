      

gidd\_alloc
===========

find a GID within the specified range which is not used by any process

Synopsis
--------

**gidd\_alloc** *min-gid* *max-gid*

Description
-----------

This program will scan the alive PIDs, looking for which GID is unused
in the supplied, inclusive range specified by the required arguments
*min-gid* and *max-gid*. Upon finding one, it will add the GID to its
own supplementary group list, and then scan the PIDs again expecting to
find only itself using the GID. If no collision has occurred, the
program exits, otherwise it retries.

General Remarks
---------------

This is a program only available for the Linux ports of HTCondor.

Exit Status
-----------

*gidd\_alloc* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
