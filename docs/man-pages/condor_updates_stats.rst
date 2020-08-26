      

*condor_updates_stats*
========================

Display output from *condor_status*
:index:`condor_updates_stats<single: condor_updates_stats; HTCondor commands>`
:index:`condor_updates_stats command`

Synopsis
--------

**condor_updates_stats** [--**help** | -**h**] | [--**version**]

**condor_updates_stats** [--**long** | -**l**]
[--**history=<min>-<max>**] [--**interval=<seconds>**] [--**notime**]
[--**time**] [--**summary** | -**s**]

Description
-----------

*condor_updates_stats* parses the output from *condor_status*, and it
displays the information relating to update statistics in a useful
format. The statistics are displayed with the most recent update first;
the most recent update is numbered with the smallest value.

The number of historic points that represent updates is configurable on
a per-source basis by configuration variable
``COLLECTOR_DAEMON_HISTORY_SIZE``
:index:`COLLECTOR_DAEMON_HISTORY_SIZE`.

Options
-------

 **-help**
    Display usage information and exit.
 **-h**
    Same as **-help**.
 **-version**
    Display HTCondor version information and exit.
 **-long**
    All update statistics are displayed. Without this option, the
    statistics are condensed.
 **-l**
    Same as **-long**.
 **-history=<min>-<max>**
    Sets the range of update numbers that are printed. By default, the
    entire history is displayed. To limit the range, the minimum and/or
    maximum number may be specified. If a minimum is not specified,
    values from 0 to the maximum are displayed. If the maximum is not
    specified, all values after the minimum are displayed. When both
    minimum and maximum are specified, the range to be displayed
    includes the endpoints as well as all values in between. If no =
    sign is given, command-line parsing fails, and usage information is
    displayed. If an = sign is given, with no minimum or maximum values,
    the default of the entire history is displayed.
 **-interval=<seconds>**
    The assumed update interval, in seconds. Assumed times for the the
    updates are displayed, making the use of the **-time** option
    together with the **-interval** option redundant.
 **-notime**
    Do not display assumed times for the the updates. If more than one
    of the options **-notime** and **-time** are provided, the final one
    within the command line parsed determines the display.
 **-time**
    Display assumed times for the the updates. If more than one of the
    options **-notime** and **-time** are provided, the final one within
    the command line parsed determines the display.
 **-summary**
    Display only summary information, not the entire history for each
    machine.
 **-s**
    Same as **-summary**.

Exit Status
-----------

*condor_updates_stats* will exit with a status value of 0 (zero) upon
success, and it will exit with a nonzero value upon failure.

Examples
--------

Assuming the default of 128 updates kept, and assuming that the update
interval is 5 minutes, *condor_updates_stats* displays:

.. code-block:: console

    $ condor_status -l host1 | condor_updates_stats --interval=300 
    (Reading from stdin) 
    *** Name/Machine = 'HOST1.cs.wisc.edu' MyType = 'Machine' *** 
     Type: Main 
       Stats: Total=2277, Seq=2276, Lost=3 (0.13%) 
         0 @ Mon Feb 16 12:55:38 2004: Ok 
      ... 
        28 @ Mon Feb 16 10:35:38 2004: Missed 
        29 @ Mon Feb 16 10:30:38 2004: Ok 
      ... 
       127 @ Mon Feb 16 02:20:38 2004: Ok

Within this display, update numbered 27, which occurs later in time than
the missed update numbered 28, is Ok. Each change in state, in reverse
time order, displays in this condensed version.

