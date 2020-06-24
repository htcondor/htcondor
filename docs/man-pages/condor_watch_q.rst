.. _condor_watch_q:

*condor_watch_q*
======================

Track the status of jobs over time.

:index:`condor_watch_q<single: condor_watch_q; HTCondor commands>`
:index:`condor_watch_q command`

Synopsis
--------

**condor_watch_q** [**-help**]

**condor_watch_q** [*general options*] [*display options*] [*behavior options*] [*tracking options*]


Description
-----------

**condor_watch_q** is a tool for tracking the status of jobs over time
without repeatedly querying the *condor_schedd*. It does this by reading
job event log files.
These files may be specified directly (the ``-files`` option),
or indirectly via a single query to the *condor_schedd* when **condor_watch_q**
starts up (options like ``-users`` or ``-clusters``).

**condor_watch_q** provides a variety of
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

To track jobs from a specific cluster,
use the ``-clusters`` option, passing the cluster ID:

.. code-block:: bash

    condor_watch_q -clusters 12345

To track jobs from a specific user,
use the ``-users`` option, passing the user's name
the actual query will be the for the ``Owner`` job ad attribute):

.. code-block:: bash

    condor_watch_q -users jane

To track jobs from a specific event log file,
use the ``-files`` option, passing the path to the event log:

.. code-block:: bash

    condor_watch_q -users /home/jane/events.log

To track jobs from a specific batch,
use the ``-batches`` option, passing the batch name:

.. code-block:: bash

    condor_watch_q -batches BatchOfJobsFromTuesday

All of the above "tracking" options can be used together, and multiple values
may be passed to each one. For example, to track all of the jobs that are:
owned by ``jane`` or ``jim``, in cluster ``12345``,
or in the event log ``/home/jill/events.log``, run

.. code-block:: bash

    condor_watch_q -users jane jim -clusters 12345 -files /home/jill/events.log

By default, **condor_watch_q** will never exit on its own
(unless it encounters an error or it is not tracking any jobs).
You can tell it to exit when certain conditions are met. For example,
to exit with status 0 when all of the jobs it is tracking are done
or with status 1 when any job is held, you could run

.. code-block:: bash

    condor_watch_q -exit all,done,0 -exit any,held,1


Options
-------

General Options
'''''''''''''''

 **-help**
    Display the help message and exit.

 **-debug**
    Causes debugging information to be sent to ``stderr``.


Tracking Options
''''''''''''''''

These options control which jobs **condor_watch_q** will track,
and how it discovers them.

 **-users USER [USER ...]**
    Choose which users to track jobs for.
    All of the user's jobs will be tracked.
    One or more user names may be passed.

 **-clusters CLUSTER_ID [CLUSTER_ID ...]**
    Which cluster IDs to track jobs for.
    One or more cluster ids may be passed.

 **-files FILE [FILE ...]**
    Which job event log files (i.e., the ``log`` file from ``condor_submit``)
    to track jobs from.
    One or more file paths may be passed.

 **-batches BATCH_NAME [BATCH_NAME ...]**
    Which job batch names to track jobs for.
    One or more batch names may be passed.

 **-collector COLLECTOR**
    Which collector to contact to find the schedd, if needed.
    Defaults to the local collector.

 **-schedd SCHEDD**
    Which schedd to contact for queries, if needed.
    Defaults to the local schedd.


Behavior Options
''''''''''''''''

 **-exit GROUPER,JOB_STATUS[,EXIT_STATUS]**
    Specify conditions under which condor_watch_q should exit.
    ``GROUPER`` is one of ``all``, ``any`` or ``none``.
    ``JOB_STATUS`` is one of ``active``, ``done``, ``idle``, or ``held``.
    The "active" status means "in the queue",
    and includes jobs in the idle, running, and held states.
    ``EXIT_STATUS`` may be any valid exit status integer.
    To specify multiple exit conditions, pass this option multiple times.
    **condor_watch_q** will exit when any of the conditions are satisfied.


Display Options
'''''''''''''''

These options control how **condor_watch_q** formats its output.
Many of them are "toggles": ``-x`` enables option "x", and ``-no-x`` disables it.

 **-groupby {batch, log, cluster}**
    How to group jobs into rows for display in the table.
    Must be one of
    ``batch`` (group by job batch name),
    ``log`` (group by event log file path),
    or
    ``cluster`` (group by cluster ID).
    Defaults to ``batch``.

 **-table/-no-table**
    Enable/disable the table.
    Enabled by default.

 **-progress/-no-progress**
    Enable/disable the progress bar.
    Enabled by default.

 **-row-progress/-no-row-progress**
    Enable/disable the progress bar for each row.
    Enabled by default.

 **-summary/-no-summary**
    Enable/disable the summary line.
    Enabled by default.

 **-summary-type {totals, percentages}**
    Choose what to display on the summary line,
    ``totals`` (the number of each jobs in each state),
    or
    ``percentages`` (the percentage of jobs in each state, of the total number of tracked jobs)
    By default, show ``totals``.

 **-updated-at/-no-updated-at**
    Enable/disable the "updated at" line.
    Enabled by default.

 **-abbreviate/-no-abbreviate**
    Enable/disable abbreviating path components to the shortest somewhat-unique prefix.
    Disabled by default.

 **-color/-no-color**
    Enable/disable colored output.
    Enabled by default if connected to a tty.
    Disabled on Windows if colorama is not available (https://pypi.org/project/colorama/).

 **-refresh/-no-refresh**
    Enable/disable refreshing output.
    If refreshing is disabled, output will be appended instead.
    Enabled by default if connected to a tty.


Exit Status
-----------

Returns ``0`` when sent a SIGINT (keyboard interrupt).

Returns ``0`` if no jobs are found to track.

Returns ``1`` for fatal internal errors.

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
