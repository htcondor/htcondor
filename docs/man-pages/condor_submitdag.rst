      

condor\_submit\_dag
===================

Manage and queue jobs within a specified DAG for execution on remote
machines

Synopsis
--------

**condor\_submit\_dag** [**-help \| -version**\ ]

**condor\_submit\_dag** [**-no\_submit**\ ] [**-verbose**\ ]
[**-force**\ ] [**-maxidle  **\ *NumberOfProcs*]
[**-maxjobs  **\ *NumberOfClusters*] [**-dagman  **\ *DagmanExecutable*]
[**-maxpre  **\ *NumberOfPreScripts*]
[**-maxpost  **\ *NumberOfPostScripts*] [**-notification  **\ *value*]
[**-noeventchecks**\ ] [**-allowlogerror**\ ] [**-r  **\ *schedd\_name*]
[**-debug  **\ *level*] [**-usedagdir**\ ]
[**-outfile\_dir  **\ *directory*] [**-config  **\ *ConfigFileName*]
[**-insert\_sub\_file  **\ *FileName*] [**-append  **\ *Command*]
[**-batch-name  **\ *batch\_name*] [**-autorescue  **\ *0\|1*]
[**-dorescuefrom  **\ *number*] [**-allowversionmismatch**\ ]
[**-no\_recurse**\ ] [**-do\_recurse**\ ] [**-update\_submit**\ ]
[**-import\_env**\ ] [**-DumpRescue**\ ] [**-valgrind**\ ]
[**-DontAlwaysRunPost**\ ] [**-AlwaysRunPost**\ ]
[**-priority  **\ *number*] [**-dont\_use\_default\_node\_log**\ ]
[**-schedd-daemon-ad-file  **\ *FileName*]
[**-schedd-address-file  **\ *FileName*] [**-suppress\_notification**\ ]
[**-dont\_suppress\_notification**\ ] [**-DoRecovery**\ ]
*DAGInputFile1* [*DAGInputFile2 …DAGInputFileN*\ ]

Description
-----------

*condor\_submit\_dag* is the program for submitting a DAG (directed
acyclic graph) of jobs for execution under HTCondor. The program
enforces the job dependencies defined in one or more *DAGInputFile*\ s.
Each *DAGInputFile* contains commands to direct the submission of jobs
implied by the nodes of a DAG to HTCondor. Extensive documentation is in
the HTCondor User Manual section on DAGMan.

Some options may be specified on the command line or in the
configuration or in a node job’s submit description file. Precedence is
given to command line options or configuration over settings from a
submit description file. An example is e-mail notifications. When
configuration variable ``DAGMAN_SUPPRESS_NOTIFICATION`` is its default
value of ``True``, and a node job’s submit description file contains

::

      notification = Complete

e-mail will not be sent upon completion, as the value of
``DAGMAN_SUPPRESS_NOTIFICATION`` is enforced.

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display version information and exit.
 **-no\_submit**
    Produce the HTCondor submit description file for DAGMan, but do not
    submit DAGMan as an HTCondor job.
 **-verbose**
    Cause *condor\_submit\_dag* to give verbose error messages.
 **-force**
    Require *condor\_submit\_dag* to overwrite the files that it
    produces, if the files already exist. Note that ``dagman.out`` will
    be appended to, not overwritten. If new-style rescue DAG mode is in
    effect, and any new-style rescue DAGs exist, the **-force** flag
    will cause them to be renamed, and the original DAG will be run. If
    old-style rescue DAG mode is in effect, any existing old-style
    rescue DAGs will be deleted, and the original DAG will be run.
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
 **-dagman **\ *DagmanExecutable*
    Allows the specification of an alternate *condor\_dagman* executable
    to be used instead of the one found in the user’s path. This must be
    a fully qualified path.
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
 **-notification **\ *value*
    Sets the e-mail notification for DAGMan itself. This information
    will be used within the HTCondor submit description file for DAGMan.
    This file is produced by *condor\_submit\_dag*. See the description
    of **notification** within *condor\_submit* manual page for a
    specification of *value*.
 **-noeventchecks**
    This argument is no longer used; it is now ignored. Its
    functionality is now implemented by the ``DAGMAN_ALLOW_EVENTS``
    configuration variable.
 **-allowlogerror**
    As of verson 8.5.5 this argument is no longer supported, and setting
    it will generate a warning.
 **-r **\ *schedd\_name*
    Submit *condor\_dagman* to a remote machine, specifically the
    *condor\_schedd* daemon on that machine. The *condor\_dagman* job
    will not run on the local *condor\_schedd* (the submit machine), but
    on the specified one. This is implemented using the **-remote**
    option to *condor\_submit*. Note that this option does not currently
    specify input files for *condor\_dagman*, nor the individual nodes
    to be taken along! It is assumed that any necessary files will be
    present on the remote computer, possibly via a shared file system
    between the local computer and the remote computer. It is also
    necessary that the user has appropriate permissions to submit a job
    to the remote machine; the permissions are the same as those
    required to use *condor\_submit*\ ’s **-remote** option. If other
    options are desired, including transfer of other input files,
    consider using the **-no\_submit** option, modifying the resulting
    submit file for specific needs, and then using *condor\_submit* on
    that.
 **-debug **\ *level*
    Passes the the *level* of debugging output desired to
    *condor\_dagman*. *level* is an integer, with values of 0-7
    inclusive, where 7 is the most verbose output. See the
    *condor\_dagman* manual page for detailed descriptions of these
    values. If not specified, no **-debug **\ *v*\ alue is passed to
    *condor\_dagman*.
 **-usedagdir**
    This optional argument causes *condor\_dagman* to run each specified
    DAG as if *condor\_submit\_dag* had been run in the directory
    containing that DAG file. This option is most useful when running
    multiple DAGs in a single *condor\_dagman*. Note that the
    **-usedagdir** flag must not be used when running an old-style
    Rescue DAG.
 **-outfile\_dir **\ *directory*
    Specifies the directory in which the ``.dagman.out`` file will be
    written. The *directory* may be specified relative to the current
    working directory as *condor\_submit\_dag* is executed, or specified
    with an absolute path. Without this option, the ``.dagman.out`` file
    is placed in the same directory as the first DAG input file listed
    on the command line.
 **-config **\ *ConfigFileName*
    Specifies a configuration file to be used for this DAGMan run. Note
    that the options specified in the configuration file apply to all
    DAGs if multiple DAGs are specified. Further note that it is a fatal
    error if the configuration file specified by this option conflicts
    with a configuration file specified in any of the DAG files, if they
    specify one.
 **-insert\_sub\_file **\ *FileName*
    Specifies a file to insert into the ``.condor.sub`` file created by
    *condor\_submit\_dag*. The specified file must contain only legal
    submit file commands. Only one file can be inserted. (If both the
    DAGMAN\_INSERT\_SUB\_FILE configuration variable and
    **-insert\_sub\_file** are specified, **-insert\_sub\_file**
    overrides DAGMAN\_INSERT\_SUB\_FILE.) The specified file is inserted
    into the ``.condor.sub`` file before the Queue command and before
    any commands specified with the **-append** option.
 **-append **\ *Command*
    Specifies a command to append to the ``.condor.sub`` file created by
    *condor\_submit\_dag*. The specified command is appended to the
    ``.condor.sub`` file immediately before the Queue command. Multiple
    commands are specified by using the **-append** option multiple
    times. Each new command is given in a separate **-append** option.
    Commands with spaces in them must be enclosed in double quotes.
    Commands specified with the **-append** option are appended to the
    ``.condor.sub`` file after commands inserted from a file specified
    by the **-insert\_sub\_file** option or the
    DAGMAN\_INSERT\_SUB\_FILE configuration variable, so the **-append**
    command(s) will override commands from the inserted file if the
    commands conflict.
 **-batch-name **\ *batch\_name*
    Set the batch name for this DAG/workflow. The batch name is
    displayed by *condor\_q* **-batch**. It is intended for use by users
    to give meaningful names to their workflows and to influence how
    *condor\_q* groups jobs for display. As of version 8.5.5, the batch
    name set with this argument is propagated to all node jobs of the
    given DAG (including sub-DAGs), overriding any batch names set in
    the individual submit files. Note: set the batch name to ’ ’ (space)
    to avoid overriding batch names specified in node job submit files.
    If no batch name is set, the batch name defaults to
    *DagFile*\ +\ *cluster* (where *DagFile* is the primary DAG file of
    the top-level DAGMan, and *cluster* is the HTCondor cluster of the
    top-level DAGMan); the default will override any lower-level batch
    names.
 **-autorescue **\ *0\|1*
    Whether to automatically run the newest rescue DAG for the given DAG
    file, if one exists (0 = ``false``, 1 = ``true``).
 **-dorescuefrom **\ *number*
    Forces *condor\_dagman* to run the specified rescue DAG number for
    the given DAG. A value of 0 is the same as not specifying this
    option. Specifying a non-existent rescue DAG is a fatal error.
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
 **-no\_recurse**
    This optional argument causes *condor\_submit\_dag* to not run
    itself recursively on nested DAGs (this is now the default; this
    flag has been kept mainly for backwards compatibility).
 **-do\_recurse**
    This optional argument causes *condor\_submit\_dag* to run itself
    recursively on nested DAGs. The default is now that it does not run
    itself recursively; instead the ``.condor.sub`` files for nested
    DAGs are generated "lazily" by *condor\_dagman* itself. DAG nodes
    specified with the **SUBDAG EXTERNAL** keyword or with submit file
    names ending in ``.condor.sub`` are considered nested DAGs. The
    ``DAGMAN_GENERATE_SUBDAG_SUBMITS`` configuration variable may be
    relevant.
 **-update\_submit**
    This optional argument causes an existing ``.condor.sub`` file to
    not be treated as an error; rather, the ``.condor.sub`` file will be
    overwritten, but the existing values of **-maxjobs**, **-maxidle**,
    **-maxpre**, and **-maxpost** will be preserved.
 **-import\_env**
    This optional argument causes *condor\_submit\_dag* to import the
    current environment into the **environment** command of the
    ``.condor.sub`` file it generates.
 **-DumpRescue**
    This optional argument tells *condor\_dagman* to immediately dump a
    rescue DAG and then exit, as opposed to actually running the DAG.
    This feature is mainly intended for testing. The Rescue DAG file is
    produced whether or not there are parse errors reading the original
    DAG input file. The name of the file differs if there was a parse
    error.
 **-valgrind**
    This optional argument causes the submit description file generated
    for the submission of *condor\_dagman* to be modified. The
    executable becomes *valgrind* run on *condor\_dagman*, with a
    specific set of arguments intended for testing *condor\_dagman*.
    Note that this argument is intended for testing purposes only. Using
    the **-valgrind** option without the necessary *valgrind* software
    installed will cause the DAG to fail. If the DAG does run, it will
    run much more slowly than usual.
 **-DontAlwaysRunPost**
    This option causes the submit description file generated for the
    submission of *condor\_dagman* to be modified. It causes
    *condor\_dagman* to not run the POST script of a node if the PRE
    script fails. (This was the default behavior prior to HTCondor
    version 7.7.2, and is again the default behavior from version 8.5.4
    onwards.)
 **-AlwaysRunPost**
    This option causes the submit description file generated for the
    submission of *condor\_dagman* to be modified. It causes
    *condor\_dagman* to always run the POST script of a node, even if
    the PRE script fails. (This was the default behavior for HTCondor
    version 7.7.2 through version 8.5.3.)
 **-priority **\ *number*
    Sets the minimum job priority of node jobs submitted and running
    under the *condor\_dagman* job submitted by this
    *condor\_submit\_dag* command.
 **-dont\_use\_default\_node\_log**
    **** Tells *condor\_dagman* to use the file specified by the job
    ClassAd attribute ``UserLog`` to monitor job status. If this command
    line argument is used, then the job event log file cannot be defined
    with a macro.
 **-schedd-daemon-ad-file **\ *FileName*
    Specifies a full path to a daemon ad file dropped by a
    *condor\_schedd*. Therefore this allows submission to a specific
    scheduler if several are available without repeatedly querying the
    *condor\_collector*. The value for this argument defaults to the
    configuration attribute ``SCHEDD_DAEMON_AD_FILE``.
 **-schedd-address-file **\ *FileName*
    Specifies a full path to an address file dropped by a
    *condor\_schedd*. Therefore this allows submission to a specific
    scheduler if several are available without repeatedly querying the
    *condor\_collector*. The value for this argument defaults to the
    configuration attribute ``SCHEDD_ADDRESS_FILE``.
 **-suppress\_notification**
    Causes jobs submitted by *condor\_dagman* to not send email
    notification for events. The same effect can be achieved by setting
    configuration variable ``DAGMAN_SUPPRESS_NOTIFICATION`` to ``True``.
    This command line option is independent of the **-notification**
    command line option, which controls notification for the
    *condor\_dagman* job itself.
 **-dont\_suppress\_notification**
    Causes jobs submitted by *condor\_dagman* to defer to content within
    the submit description file when deciding to send email notification
    for events. The same effect can be achieved by setting configuration
    variable ``DAGMAN_SUPPRESS_NOTIFICATION`` to ``False``. This command
    line flag is independent of the **-notification** command line
    option, which controls notification for the *condor\_dagman* job
    itself. If both **-dont\_suppress\_notification** and
    **-suppress\_notification** are specified with the same command
    line, the last argument is used.
 **-DoRecovery**
    Causes *condor\_dagman* to start in recovery mode. (This means that
    it reads the relevant job user log(s) and "catches up" to the given
    DAG’s previous state before submitting any new jobs.)

Exit Status
-----------

*condor\_submit\_dag* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To run a single DAG:

::

    % condor_submit_dag diamond.dag

To run a DAG when it has already been run and the output files exist:

::

    % condor_submit_dag -force diamond.dag

To run a DAG, limiting the number of idle node jobs in the DAG to a
maximum of five:

::

    % condor_submit_dag -maxidle 5 diamond.dag

To run a DAG, limiting the number of concurrent PRE scripts to 10 and
the number of concurrent POST scripts to five:

::

    % condor_submit_dag -maxpre 10 -maxpost 5 diamond.dag

To run two DAGs, each of which is set up to run in its own directory:

::

    % condor_submit_dag -usedagdir dag1/diamond1.dag dag2/diamond2.dag

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
