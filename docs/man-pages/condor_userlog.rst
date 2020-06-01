      

*condor_userlog*
=================

Display and summarize job statistics from job log files.
:index:`condor_userlog<single: condor_userlog; HTCondor commands>`\ :index:`condor_userlog command`

Synopsis
--------

**condor_userlog** [**-help** ] [**-total | -raw** ] [**-debug** ]
[**-evict** ] [**-j** *cluster | cluster.proc*] [**-all** ]
[**-hostname** ] *logfile ...*

Description
-----------

*condor_userlog* parses the information in job log files and displays
summaries for each workstation allocation and for each job. See the
*condor_submit* manual page for instructions for specifying that
HTCondor write a log file for your jobs.

If **-total** is not specified, *condor_userlog* will first display a
record for each workstation allocation, which includes the following
information:

 Job
    The cluster/process id of the HTCondor job.
 Host
    The host where the job ran. By default, the host's IP address is
    displayed. If **-hostname** is specified, the host name will be
    displayed instead.
 Start Time
    The time (month/day hour:minute) when the job began running on the
    host.
 Evict Time
    The time (month/day hour:minute) when the job was evicted from the
    host.
 Wall Time
    The time (days+hours:minutes) for which this workstation was
    allocated to the job.
 Good Time
    The allocated time (days+hours:min) which contributed to the
    completion of this job. If the job exited during the allocation,
    then this value will equal "Wall Time." If the job performed a
    checkpoint, then the value equals the work saved in the checkpoint
    during this allocation. If the job did not exit or perform a
    checkpoint during this allocation, the value will be 0+00:00. This
    value can be greater than 0 and less than "Wall Time" if the
    application completed a periodic checkpoint during the allocation
    but failed to checkpoint when evicted.
 CPU Usage
    The CPU time (days+hours:min) which contributed to the completion of
    this job.

*condor_userlog* will then display summary statistics per host:

 Host/Job
    The IP address or host name for the host.
 Wall Time
    The workstation time (days+hours:minutes) allocated by this host to
    the jobs specified in the query. By default, all jobs in the log are
    included in the query.
 Good Time
    The time (days+hours:minutes) allocated on this host which
    contributed to the completion of the jobs specified in the query.
 CPU Usage
    The CPU time (days+hours:minutes) obtained from this host which
    contributed to the completion of the jobs specified in the query.
 Avg Alloc
    The average length of an allocation on this host
    (days+hours:minutes).
 Avg Lost
    The average amount of work lost (days+hours:minutes) when a job was
    evicted from this host without successfully performing a checkpoint.
 Goodput
    This percentage is computed as Good Time divided by Wall Time.
 Util.
    This percentage is computed as CPU Usage divided by Good Time.

*condor_userlog* will then display summary statistics per job:

 Host/Job
    The cluster/process id of the HTCondor job.
 Wall Time
    The total workstation time (days+hours:minutes) allocated to this
    job.
 Good Time
    The total time (days+hours:minutes) allocated to this job which
    contributed to the job's completion.
 CPU Usage
    The total CPU time (days+hours:minutes) which contributed to this
    job's completion.
 Avg Alloc
    The average length of a workstation allocation obtained by this job
    in minutes (days+hours:minutes).
 Avg Lost
    The average amount of work lost (days+hours:minutes) when this job
    was evicted from a host without successfully performing a
    checkpoint.
 Goodput
    This percentage is computed as Good Time divided by Wall Time.
 Util.
    This percentage is computed as CPU Usage divided by Good Time.

Finally, *condor_userlog* will display a summary for all hosts and
jobs.

Options
-------

 **-help**
    Get a brief description of the supported options
 **-total**
    Only display job totals
 **-raw**
    Display raw data only
 **-debug**
    Debug mode
 **-j**
    Select a specific cluster or cluster.proc
 **-evict**
    Select only allocations which ended due to eviction
 **-all**
    Select all clusters and all allocations
 **-hostname**
    Display host name instead of IP address

General Remarks
---------------

Since the HTCondor job log file format does not contain a year field in
the timestamp, all entries are assumed to occur in the current year.
Allocations which begin in one year and end in the next will be silently
ignored.

Exit Status
-----------

*condor_userlog* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

