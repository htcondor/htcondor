      

condor\_dagman
==============

meta scheduler of the jobs submitted as the nodes of a DAG or DAGs

Synopsis
--------

**condor\_dagman** *-f* *-t* *-l .* **-help**

**condor\_dagman** **-version**

**condor\_dagman** *-f* *-l .* **-csdversion **\ *version\_string*
[**-debug  **\ *level*] [**-maxidle  **\ *numberOfProcs*]
[**-maxjobs  **\ *numberOfJobs*] [**-maxpre  **\ *NumberOfPreScripts*]
[**-maxpost  **\ *NumberOfPostScripts*] [**-noeventchecks**\ ]
[**-allowlogerror**\ ] [**-usedagdir**\ ] **-lockfile **\ *filename*
[**-waitfordebug**\ ] [**-autorescue  **\ *0\|1*]
[**-dorescuefrom  **\ *number*] [**-allowversionmismatch**\ ]
[**-DumpRescue**\ ] [**-verbose**\ ] [**-force**\ ]
[**-notification  **\ *value*] [**-suppress\_notification**\ ]
[**-dont\_suppress\_notification**\ ]
[**-dagman  **\ *DagmanExecutable*] [**-outfile\_dir  **\ *directory*]
[**-update\_submit**\ ] [**-import\_env**\ ] [**-priority  **\ *number*]
[**-dont\_use\_default\_node\_log**\ ] [**-DontAlwaysRunPost**\ ]
[**-AlwaysRunPost**\ ] [**-DoRecovery**\ ] **-dag **\ *dag\_file* [**\ ]

Description
-----------

*condor\_dagman* is a meta scheduler for the HTCondor jobs within a DAG
(directed acyclic graph) (or multiple DAGs). In typical usage, a
submitter of jobs that are organized into a DAG submits the DAG using
*condor\_submit\_dag*. *condor\_submit\_dag* does error checking on
aspects of the DAG and then submits *condor\_dagman* as an HTCondor job.
*condor\_dagman* uses log files to coordinate the further submission of
the jobs within the DAG.

All command line arguments to the *DaemonCore* library functions work
for *condor\_dagman*. When invoked from the command line,
*condor\_dagman* requires the arguments *-f -l .* to appear first on the
command line, to be processed by *DaemonCore*. The **csdversion** must
also be specified; at start up, *condor\_dagman* checks for a version
mismatch with the *condor\_submit\_dag* version in this argument. The
*-t* argument must also be present for the **-help** option, such that
output is sent to the terminal.

Arguments to *condor\_dagman* are either automatically set by
*condor\_submit\_dag* or they are specified as command-line arguments to
*condor\_submit\_dag* and passed on to *condor\_dagman*. The method by
which the arguments are set is given in their description below.

*condor\_dagman* can run multiple, independent DAGs. This is done by
specifying multiple **-dag **\ *a*\ rguments. Pass multiple DAG input
files as command-line arguments to *condor\_submit\_dag*.

Debugging output may be obtained by using the **-debug **\ *level*
option. Level values and what they produce is described as

-  level = 0; never produce output, except for usage info
-  level = 1; very quiet, output severe errors
-  level = 2; normal output, errors and warnings
-  level = 3; output errors, as well as all warnings
-  level = 4; internal debugging output
-  level = 5; internal debugging output; outer loop debugging
-  level = 6; internal debugging output; inner loop debugging; output
   DAG input file lines as they are parsed
-  level = 7; internal debugging output; rarely used; output DAG input
   file lines as they are parsed

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display version information and exit.
 **-debug **\ *level*
    An integer level of debugging output. *level* is an integer, with
    values of 0-7 inclusive, where 7 is the most verbose output. This
    command-line option to *condor\_submit\_dag* is passed to
    *condor\_dagman* or defaults to the value 3.
 **-maxidle **\ *NumberOfProcs*
    Sets the maximum number of idle procs allowed before
    *condor\_dagman* stops submitting more node jobs. Note that for this
    argument, each individual proc within a cluster counts as a towards
    the limit, which is inconsistent with **-maxjobs **\ *.* Once idle
    procs start to run, *condor\_dagman* will resume submitting jobs
    once the number of idle procs falls below the specified limit.
    *NumberOfProcs* is a non-negative integer. If this option is
    omitted, the number of idle procs is limited by the configuration
    variable ``DAGMAN_MAX_JOBS_IDLE`` (see
     `3.5.23 <ConfigurationMacros.html#x33-2120003.5.23>`__), which
    defaults to 1000. To disable this limit, set *NumberOfProcs* to 0.
    Note that submit description files that queue multiple procs can
    cause the *NumberOfProcs* limit to be exceeded. Setting
    ``queue 5000`` in the submit description file, where *-maxidle* is
    set to 250 will result in a cluster of 5000 new procs being
    submitted to the *condor\_schedd*, not 250. In this case,
    *condor\_dagman* will resume submitting jobs when the number of idle
    procs falls below 250.
 **-maxjobs **\ *NumberOfClusters*
    Sets the maximum number of clusters within the DAG that will be
    submitted to HTCondor at one time. Note that for this argument, each
    cluster counts as one job, no matter how many individual procs are
    in the cluster. *NumberOfClusters* is a non-negative integer. If
    this option is omitted, the number of clusters is limited by the
    configuration variable ``DAGMAN_MAX_JOBS_SUBMITTED`` (see
     `3.5.23 <ConfigurationMacros.html#x33-2120003.5.23>`__), which
    defaults to 0 (unlimited).
 **-maxpre **\ *NumberOfPreScripts*
    Sets the maximum number of PRE scripts within the DAG that may be
    running at one time. *NumberOfPreScripts* is a non-negative integer.
    If this option is omitted, the number of PRE scripts is limited by
    the configuration variable ``DAGMAN_MAX_PRE_SCRIPTS`` (see
     `3.5.23 <ConfigurationMacros.html#x33-2120003.5.23>`__), which
    defaults to 20.
 **-maxpost **\ *NumberOfPostScripts*
    Sets the maximum number of POST scripts within the DAG that may be
    running at one time. *NumberOfPostScripts* is a non-negative
    integer. If this option is omitted, the number of POST scripts is
    limited by the configuration variable ``DAGMAN_MAX_POST_SCRIPTS``
    (see  `3.5.23 <ConfigurationMacros.html#x33-2120003.5.23>`__), which
    defaults to 20.
 **-noeventchecks**
    This argument is no longer used; it is now ignored. Its
    functionality is now implemented by the ``DAGMAN_ALLOW_EVENTS``
    configuration variable.
 **-allowlogerror**
    As of verson 8.5.5 this argument is no longer supported, and setting
    it will generate a warning.
 **-usedagdir**
    This optional argument causes *condor\_dagman* to run each specified
    DAG as if the directory containing that DAG file was the current
    working directory. This option is most useful when running multiple
    DAGs in a single *condor\_dagman*.
 **-lockfile **\ *filename*
    Names the file created and used as a lock file. The lock file
    prevents execution of two of the same DAG, as defined by a DAG input
    file. A default lock file ending with the suffix ``.dag.lock`` is
    passed to *condor\_dagman* by *condor\_submit\_dag*.
 **-waitfordebug**
    This optional argument causes *condor\_dagman* to wait at startup
    until someone attaches to the process with a debugger and sets the
    wait\_for\_debug variable in main\_init() to false.
 **-autorescue **\ *0\|1*
    Whether to automatically run the newest rescue DAG for the given DAG
    file, if one exists (0 = ``false``, 1 = ``true``).
 **-dorescuefrom **\ *number*
    Forces *condor\_dagman* to run the specified rescue DAG number for
    the given DAG. A value of 0 is the same as not specifying this
    option. Specifying a nonexistent rescue DAG is a fatal error.
 **-allowversionmismatch**
    This optional argument causes *condor\_dagman* to allow a version
    mismatch between *condor\_dagman* itself and the ``.condor.sub``
    file produced by *condor\_submit\_dag* (or, in other words, between
    *condor\_submit\_dag* and *condor\_dagman*). WARNING! This option
    should be used only if absolutely necessary. Allowing version
    mismatches can cause subtle problems when running DAGs. (Note that,
    starting with version 7.4.0, *condor\_dagman* no longer requires an
    exact version match between itself and the ``.condor.sub`` file.
    Instead, a "minimum compatible version" is defined, and any
    ``.condor.sub`` file of that version or newer is accepted.)
 **-DumpRescue**
    This optional argument causes *condor\_dagman* to immediately dump a
    Rescue DAG and then exit, as opposed to actually running the DAG.
    This feature is mainly intended for testing. The Rescue DAG file is
    produced whether or not there are parse errors reading the original
    DAG input file. The name of the file differs if there was a parse
    error.
 **-verbose**
    (This argument is included only to be passed to
    *condor\_submit\_dag* if lazy submit file generation is used for
    nested DAGs.) Cause *condor\_submit\_dag* to give verbose error
    messages.
 **-force**
    (This argument is included only to be passed to
    *condor\_submit\_dag* if lazy submit file generation is used for
    nested DAGs.) Require *condor\_submit\_dag* to overwrite the files
    that it produces, if the files already exist. Note that
    ``dagman.out`` will be appended to, not overwritten. If new-style
    rescue DAG mode is in effect, and any new-style rescue DAGs exist,
    the **-force** flag will cause them to be renamed, and the original
    DAG will be run. If old-style rescue DAG mode is in effect, any
    existing old-style rescue DAGs will be deleted, and the original DAG
    will be run. See the HTCondor manual section on Rescue DAGs for more
    information.
 **-notification **\ *value*
    This argument is only included to be passed to *condor\_submit\_dag*
    if lazy submit file generation is used for nested DAGs. Sets the
    e-mail notification for DAGMan itself. This information will be used
    within the HTCondor submit description file for DAGMan. This file is
    produced by *condor\_submit\_dag*. The **notification** option is
    described in the *condor\_submit* manual page.
 **-suppress\_notification**
    Causes jobs submitted by *condor\_dagman* to not send email
    notification for events. The same effect can be achieved by setting
    the configuration variable ``DAGMAN_SUPPRESS_NOTIFICATION`` to
    ``True``. This command line option is independent of the
    **-notification** command line option, which controls notification
    for the *condor\_dagman* job itself. This flag is generally
    superfluous, as ``DAGMAN_SUPPRESS_NOTIFICATION`` defaults to
    ``True``.
 **-dont\_suppress\_notification**
    Causes jobs submitted by *condor\_dagman* to defer to content within
    the submit description file when deciding to send email notification
    for events. The same effect can be achieved by setting the
    configuration variable ``DAGMAN_SUPPRESS_NOTIFICATION`` to
    ``False``. This command line flag is independent of the
    **-notification** command line option, which controls notification
    for the *condor\_dagman* job itself. If both
    **-dont\_suppress\_notification** and **-suppress\_notification**
    are specified within the same command line, the last argument is
    used.
 **-dagman **\ *DagmanExecutable*
    (This argument is included only to be passed to
    *condor\_submit\_dag* if lazy submit file generation is used for
    nested DAGs.) Allows the specification of an alternate
    *condor\_dagman* executable to be used instead of the one found in
    the user’s path. This must be a fully qualified path.
 **-outfile\_dir **\ *directory*
    (This argument is included only to be passed to
    *condor\_submit\_dag* if lazy submit file generation is used for
    nested DAGs.) Specifies the directory in which the ``.dagman.out``
    file will be written. The *directory* may be specified relative to
    the current working directory as *condor\_submit\_dag* is executed,
    or specified with an absolute path. Without this option, the
    ``.dagman.out`` file is placed in the same directory as the first
    DAG input file listed on the command line.
 **-update\_submit**
    (This argument is included only to be passed to
    *condor\_submit\_dag* if lazy submit file generation is used for
    nested DAGs.) This optional argument causes an existing
    ``.condor.sub`` file to not be treated as an error; rather, the
    ``.condor.sub`` file will be overwritten, but the existing values of
    **-maxjobs**, **-maxidle**, **-maxpre**, and **-maxpost** will be
    preserved.
 **-import\_env**
    (This argument is included only to be passed to
    *condor\_submit\_dag* if lazy submit file generation is used for
    nested DAGs.) This optional argument causes *condor\_submit\_dag* to
    import the current environment into the **environment** command of
    the ``.condor.sub`` file it generates.
 **-priority **\ *number*
    Sets the minimum job priority of node jobs submitted and running
    under this *condor\_dagman* job.
 **-dont\_use\_default\_node\_log**
    **This option is disabled as of HTCondor version 8.3.1.** Tells
    *condor\_dagman* to use the file specified by the job ClassAd
    attribute ``UserLog`` to monitor job status. If this command line
    argument is used, then the job event log file cannot be defined with
    a macro.
 **-DontAlwaysRunPost**
    This option causes *condor\_dagman* to not run the POST script of a
    node if the PRE script fails. (This was the default behavior prior
    to HTCondor version 7.7.2, and is again the default behavior from
    version 8.5.4 onwards.)
 **-AlwaysRunPost**
    This option causes *condor\_dagman* to always run the POST script of
    a node, even if the PRE script fails. (This was the default behavior
    for HTCondor version 7.7.2 through version 8.5.3.)
 **-DoRecovery**
    Causes *condor\_dagman* to start in recovery mode. This means that
    it reads the relevant job user log(s) and catches up to the given
    DAG’s previous state before submitting any new jobs.
 **-dag **\ *filename*
    *filename* is the name of the DAG input file that is set as an
    argument to *condor\_submit\_dag*, and passed to *condor\_dagman*.

Exit Status
-----------

*condor\_dagman* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Examples
--------

*condor\_dagman* is normally not run directly, but submitted as an
HTCondor job by running condor\_submit\_dag. See the condor\_submit\_dag
manual page \ `2235 <Condorsubmitdag.html#x150-109200012>`__ for
examples.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
