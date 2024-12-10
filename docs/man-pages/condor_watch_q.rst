*condor_watch_q*
================

Continually monitor and track the state of jobs.

:index:`condor_watch_q<double: condor_watch_q; HTCondor commands>`

Synopsis
--------

**condor_watch_q** [**-help**]

**condor_watch_q** [*general options*] [*display options*] [*behavior options*] [*tracking options*]


Description
-----------

Continually track the status of jobs over time updating the displayed
output at job status change.

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

The following options control which jobs are tracked and how tracked
jobs are discovered.

 **-users** *USER [USER ...]*
    Specify which user(s) jobs to track.

 **-clusters** *CLUSTER_ID [CLUSTER_ID ...]*
    Specify which :ad-attr:`ClusterId`\'s to track.

 **-larger-than** *CLUSTER_ID*
    Track jobs for all :ad-attr:`ClusterId`\'s than or equal to the
    specified *CLUSTER_ID*.

 **-files** *FILE [FILE ...]*
    Specify which job event log(s) to track jobs from.

 **-batches** *BATCH_NAME [BATCH_NAME ...]*
    Track jobs with the specified :ad-attr:`JobBatchName`.

 **-collector** *COLLECTOR*
    Specify which *COLLECTOR* to query to locate the *SCHEDD*, if needed.

 **-schedd** *SCHEDD*
    Specify which *SCHEDD* to discover which active jobs to track.

Behavior Options
''''''''''''''''

 **-exit** *GROUPER,JOB_STATUS[,EXIT_STATUS]*
    Specify exit conditions:

        1. ``GROUPER`` is one of ``all``, ``any`` or ``none``.
        2. ``JOB_STATUS`` is one of ``active``, ``done``, ``idle``, or ``held``.
        3. (Optional) ``EXIT_STATUS`` is a valid exit code.

        .. note::

            ``JOB_STATUS`` ``active`` status represents all jobs the AP
            (``idle``, ``running``, and ``held``)

Display Options
'''''''''''''''

The following options control the output.

 **-groupby** *{batch, log, cluster}*
    Specify how to group tracked jobs:

        - ``batch``: Group by :ad-attr:`JobBatchName` (default).
        - ``log``: Group by job event log.
        - ``cluster``: Group by :ad-attr:`ClusterId`.

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

 **-summary-type** *{totals, percentages}*
    Choose what to display on the summary line:

        - ``totals``: The number of jobs in each state (default).
        - ``percentages``: The percentage of jobs in each state of all tracked jobs.

 **-updated-at/-no-updated-at**
    Enable/disable the last time updated line.
    Enabled by default.

 **-abbreviate/-no-abbreviate**
    Enable/disable abbreviating path components to the shortest somewhat-unique prefix.
    Disabled by default.

 **-color/-no-color**
    Enable/disable colored output.
    Enabled by default if connected to a ``tty``.
    Disabled on Windows if `colorama <https://pypi.org/project/colorama/>`_ is not available.

 **-refresh/-no-refresh**
    Enable/disable refreshing output.
    If refreshing is disabled, output will be appended instead.
    Enabled by default if connected to a tty.

General Remarks
---------------

This tool monitors job event log files directly to determine the status of
tracked jobs rather than querying the AP for required information.

A variety of options for output formatting are provided, including:

    - colorized output
    - tabular information
    - progress bars
    - text summaries.

A minimal language for exiting when certain conditions are met by the
tracked jobs is provided.

Exit Status
-----------

0  -  No jobs are found to track or stopped due to ``SIGINT`` (keyboard interrupt)

1  -  Failure has occurred

.. note::

    Any valid exit status can be returned when a condition set by ``-exit`` is met.

Examples
--------

Track all of the current users jobs:

.. code-block:: console

    $ condor_watch_q

Track a specific active cluster for the current user:

.. code-block:: console

    $ condor_watch_q -clusters 12345

Track users Jane's jobs:

.. code-block:: console

    # condor_watch_q -users jane

Track jobs in a specific job log file:

.. code-block:: console

    $ condor_watch_q -files /home/jane/events.log

Track jobs with a specified :ad-attr:`JobBatchName`

.. code-block:: console

    $ condor_watch_q -batches BatchOfJobsFromTuesday

Exit with ``0`` when all jobs are done or exit with ``1`` if
any jobs are held:

.. code-block:: bash

    condor_watch_q -exit all,done,0 -exit any,held,1

See Also
--------

:tool:`condor_q`

Availability
------------

Linux, MacOS, Windows
