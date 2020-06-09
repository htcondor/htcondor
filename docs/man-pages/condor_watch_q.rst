.. _condor_watch_q:

*condor_watch_q*
======================

Track the status of jobs over time.

:index:`condor_watch_q<single: condor_watch_q; HTCondor commands>`
:index:`condor_watch_q command`

Synopsis
--------

**condor_watch_q** [**-help**]


Description
-----------

**condor_watch_q** is a tool for tracking the status of jobs over time
without repeatedly querying the *condor_schedd*. It provides a variety of
options for output formatting, including: colorized output, tabular information,
progress bars, and text summaries. These display options are highly-customizable
via command line options.

**condor_watch_q** also provides a minimal language for exiting when
certain conditions are met by the tracked jobs. For example, it can be
configured to exit when all of the tracked jobs have terminated.

Examples
--------

If no users, cluster ids, or event logs are given, **condor_watch_q** will
default to tracking all of the current user's jobs. Thus, with no arguments,

.. code-block:: bash

    condor_watch_q

will track all of your currently-active clusters.

Any option beginning with a single ``-`` can be specified by its unique
prefix. For example, these commands are all equivalent:

.. code-block:: bash

    condor_watch_q -clusters 12345
    condor_watch_q -clu 12345
    condor_watch_q -c 12345

By default, **condor_watch_q** will never exit on its own
(unless it errors).
You can tell it to exit when certain conditions are met.
For example, to exit when all of the jobs it is tracking
are done (with exit status ``0``), or when any
job is held (with exit status ``1``), you could run

.. code-block:: bash

    condor_watch_q -exit all,done,0 -exit any,held,1

Exit Status
-----------

Returns ``0`` when sent a SIGINT (keyboard interrupt).

Returns ``1`` for internal errors.

Can be configured via the ``-exit`` option to return any valid exit status when
a certain condition is met.

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2020 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.
