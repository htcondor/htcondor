.. _htcondor_command:

*htcondor*
===============

Manage HTCondor jobs, job sets, dags, event logs, and resources
:index:`htcondor<single: htcondor; HTCondor commands>`\ :index:`htcondor command`

Synopsis
--------

**htcondor** [ **-h** | **-\-help** ] [ **-v** | **-q** ]

| **htcondor** **job** *submit* [**-\-resource** *resource-type*] [**-\-runtime** *time-seconds*] [**-\-email** *email-address*] submit_file
| **htcondor** **job** *status* [**-\-resource** *resource-type*] [**-\-skip-history**] job_id
| **htcondor** **job** *out* [**-\-resource** *resource-type*] [**-\-skip-history**] job_id
| **htcondor** **job** *error* [**-\-resource** *resource-type*] [**-\-skip-history**] job_id
| **htcondor** **job** *log* [**-\-resource** *resource-type*] [**-\-skip-history**] job_id
| **htcondor** **job** *resources* [**-\-resource** *resource-type*] [**-\-skip-history**] job_id

| **htcondor** **jobset** *submit* description-file
| **htcondor** **jobset** *list* [**-\-allusers**]
| **htcondor** **jobset** *status* job-set-name [**-\-owner** *user-name*] [**-\-nobatch**] [**-\-skip-history**]
| **htcondor** **jobset** *remove* job-set-name [**-\-owner** *user-name*]

| **htcondor** **dag** *submit* dag-file
| **htcondor** **dag** *status* dagman-job-id

| **htcondor** **eventlog** *read* [ **-csv** | **-json**] [ **-\-groupby attribute** ] eventlog [ eventlog2 [ eventlog3 ...  ] ]
| **htcondor** **eventlog** *follow* [ **-csv** | **-json**] [ **-\-groupby attribute** ] eventlog 

Description
-----------

*htcondor* is a tool for managing HTCondor jobs, job sets, resources, event
logs, and DAGs.  It can replace *condor_submit*, *condor_submit_dag*,
*condor_q*, *condor_status*, and *condor_userlog*, as well as all-new
functionality and features.  The user interface is more consistent than its
predecessor tools.

The first argument of the *htcondor* command (ignoring any global options) is
the *noun* representing an object in the HTCondor system to be operated on.
The nouns include an individual *job*, *jobset*, *eventlog*, or a *dag*.  Each
noun is then followed by a noun-specific *verb* that describe the operation on
that noun.

One of the following optional global option may appear before the noun:

Global Options
--------------

 **htcondor -h**, **htcondor -\-help**
     Display the help message. Can also be specified after any
     verb to display the options available for each verb.
 **htcondor -q ...**
     Reduce verbosity of log messages.
 **htcondor -v ...**
     Increase verbosity of log messages.


A noun-specific verb appears after each noun; the verbs are sorted by noun in
the list, which includes with their individual option flags.

Job Verbs
---------

 **htcondor job submit** *submit_file*
     Takes as an argument a submit file in the *condor_submit* job submit
     description language, and places a new job in an Access Point

     **htcondor job submit options**

          **htcondor job submit -\-resource** *resource_type submit_file*
            Resource type used to run this job. Currently supports ``Slurm`` and ``EC2``.
            Assumes the necessary setup is complete and security tokens available.
          **htcondor job submit -\-runtime** *runtime_in_seconds submit_file*
            Amount of time in seconds to allocate resources.
            Used in conjunction with the *-\-resource* flag.
          **htcondor job submit -\-email** *address submit_file*
            Email address to receive notification messages.
            Used in conjunction with the *-\-resource* flag.
    
    
 **htcondor job status**
     Takes as an argument a job id in the form of clusterid.procid,
     and returns a human readable presentation of the status
     of that job.

     **job status option**

      **htcondor job status -\-skip-history** *job.id*

        Passed to the *status* verb to skip checking history
        if job not found in the active job queue.

 **htcondor job out**
     Takes as an argument a job id in the form of clusterid.procid,
     and prints out the contents of that job's standard output
     file, assuming that it exists on the AP.

 **htcondor job err**
     Takes as an argument a job id in the form of clusterid.procid,
     and prints out the contents of that job's standard error
     file, assuming that it exists on the AP.

 **htcondor job log**
     Takes as an argument a job id in the form of clusterid.procid,
     and prints out the contents of that job's event log 
     file.  If the job shared an event log file with other jobs,
     the complete event log file will be printed, which may contain
     events for other jobs.

 **htcondor job resources**
     Takes as an argument a job id in the form of clusterid.procid,
     and returns a human readable presentation the machine resource
     used by this job.

Jobset Verbs
------------

 **htcondor jobset submit** *submit_file*
     Takes as an argument a submit file in the *condor_submit* job submit
     description language, and places a new job set in an Access Point

 **htcondor jobset list**
    Succinctly lists all the jobsets in the queue which are owned by the current user.

     **htcondor jobset list options**
     
          **htcondor jobset list -\-allusers**
            Shows jobs from all users, not just those owned by the current user.

 **htcondor jobset status** *submit_file*
     Takes as an argument a job set name, and shows detailed information about
     that job set.

     **htcondor jobset status options**
     
          **htcondor jobset status -\-nobatch**
            Shows jobs in a more detailed view, one line per job
     
          **htcondor jobset status -\-owner** *ownername*
            Shows jobs from the specified job owner.
     
          **htcondor jobset status -\-skiphistory**
            Shows detailed information only about active jobs in the queue, and
            ignore historical jobs which have left the queue.  This runs much
            faster.


 **htcondor jobset remove** *job_name*
     Takes as an argument a *job_name* in the queue, and removes it from 
     the Access Point.

     **htcondor jobsets remove options**
     
          **htcondor jobset remove -\-owner=owner_name**
          Removes all jobs owned by the given owner.

Eventlog Verbs
--------------

 **htcondor eventlog read** *logfile* *optional-other-logfile*
     Takes one or more arguments, which are event log files to process.  It may be the per-job or
     per-jobset eventlog, which was specified by the *log = some_file* in the
     submit description language.  For a dag, it may also be the *nodes.log*
     file that all dags generate.  Or, if the global event log is enabled by an
     administrator with the *EVENT_LOG* configuration knob, it may be the global
     event log, containing information about all jobs on the Access point.

     Given this, `htcondor eventlog read` returns information about all
     the contained jobs, and their status. It runs much faster than
     *condor_history*, because these logs are more concise than the history
     files.  Unlike *condor_history*, it will also show information about
     jobs that have not yet left the queue.

 **htcondor eventlog follow** *logfile*
     Takes as an argument an event log to process, as above, but instead
     of processing that file to completion, it does the equivalent of
     *tail -f*, and runs until interruption, emitting information about
     jobs as it appears in the file.

     **Eventlog Options**

       **-\-csv**
          By default, *htcondor eventlog read* emits a table of information
          in human readable format.  With this option, the output is in
          a command separated value format, suitable for injestion by a spreadsheet
          or database.

       **-\-json**
          Emits output in the json format. Only one of **-csv** or **-json** should
          be given.

      **-\-group-by attributeName**
          With a job ad attribute name, instead of one line per job, emit one line
          summarizing all jobs that share the same value for the attribute name
          given.  In the OSG, the GLIDEIN_SITE attribute is injected into all jobs,
          so one can quickly get a count of all jobs running, idle and exitted 
          per site by using this option.

Examples
--------

.. code-block:: console

    $ htcondor eventlog read logfile

    Job       Host            Start Time   Evict Time   Evictions   Wall Time     Good Time     CPU Usage
    19989.0   slot1_1@speedy  5/18 12:34   5/18 12:54   0           0+00:20:00    0+00:20:00    0+00:00:00
    19990.0   slot1_1@lumpy   5/22 18:51   5/22 18:51   1           0+00:02:00    0+00:00:00    0+00:00:43
    20003.0   slot1_1@chtc    8/9 23:33    8/9 23:37    1           0+00:04:00    0+00:00:00    0+00:00:00
    20004.0   slot1_1@wisc    8/9 23:38    8/9 23:58    0           0+00:20:00    0+00:20:00    0+00:00:00



Exit Status
-----------

*htcondor* will exit with a non-zero status value if it fails and
zero status if it succeeds.
