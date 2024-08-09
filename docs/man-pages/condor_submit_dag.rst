*condor_submit_dag*
=====================

Manage and queue jobs within a specified DAG for execution on remote
machines
:index:`condor_submit_dag<single: condor_submit_dag; HTCondor commands>`\ :index:`condor_submit_dag command`

Synopsis
--------

**condor_submit_dag** [**-help | -version** ]

**condor_submit_dag** [**-no_submit** ] [**-verbose** ]
[**-force** ] [**-dagman** *DagmanExecutable*]
[**-maxidle** *NumberOfProcs*] [**-maxjobs** *NumberOfClusters*]
[**-maxpre** *NumberOfPreScripts*] [**-maxpost** *NumberOfPostScripts*]
[**-notification** *value*] [**-r** *schedd_name*]
[**-debug** *level*] [**-usedagdir** ]
[**-outfile_dir** *directory*] [**-config** *ConfigFileName*]
[**-insert_sub_file** *FileName*] [**-append** *Command*]
[**-batch-name** *batch_name*] [**-autorescue** *0|1*]
[**-dorescuefrom** *number*] [**-load_save** *filename*]
[**-allowversionmismatch** ]
[**-no_recurse** ] [**-do_recurse** ] [**-update_submit** ]
[**-import_env** ] [**-include_env** *Variables*] [**-insert_env** *Key=Value*]
[**-DumpRescue** ] [**-valgrind** ] [**-DontAlwaysRunPost** ] [**-AlwaysRunPost** ]
[**-priority** *number*] [**-SubmitMethod** *value*]
[**-schedd-daemon-ad-file** *FileName*]
[**-schedd-address-file** *FileName*] [**-suppress_notification** ]
[**-dont_suppress_notification** ] [**-DoRecovery** ]
*DAGInputFile1* [*DAGInputFile2 ... DAGInputFileN* ]

Description
-----------

*condor_submit_dag* is the program for submitting a DAG (directed
acyclic graph) of jobs for execution under HTCondor. The program
enforces the job dependencies defined in one or more *DAGInputFile*\s.
Each *DAGInputFile* contains commands to direct the submission of jobs
implied by the nodes of a DAG to HTCondor. Extensive documentation is in
the HTCondor User Manual section on DAGMan.
:index:`in DAGs<single: in DAGs; email notification>`
:index:`e-mail in DAGs<single: e-mail in DAGs; notification>`

Some options may be specified on the command line or in the
configuration or in a node job's submit description file. Precedence is
given to command line options or configuration over settings from a
submit description file. An example is e-mail notifications. When
configuration variable  :macro:`DAGMAN_SUPPRESS_NOTIFICATION` is its default value of
``True``, and a node job's submit description file contains

.. code-block:: condor-submit

      notification = Complete

e-mail will not be sent upon completion, as the value of
:macro:`DAGMAN_SUPPRESS_NOTIFICATION` is enforced.

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display version information and exit.
 **-no_submit**
    Produce the HTCondor submit description file for DAGMan, but do not
    submit DAGMan as an HTCondor job.
 **-verbose**
    Cause *condor_submit_dag* to give verbose error messages.
 **-force**
    Require *condor_submit_dag* to overwrite the files that it
    produces, if the files already exist. Note that ``dagman.out`` will
    be appended to, not overwritten. If rescue files exist then
    DAGMan will run the original DAG and rename the rescue files.
    Any old-style rescue files will be deleted.
 **-dagman** *DagmanExecutable*
    Allows the specification of an alternate *condor_dagman* executable
    to be used instead of the one found in the user's path. This must be
    a fully qualified path.
 **-maxidle** *NumberOfProcs*
    Sets the maximum number of idle procs allowed before
    *condor_dagman* stops submitting more node jobs. If this option is
    omitted then the number of idle procs is limited by the configuration
    variable :macro:`DAGMAN_MAX_JOBS_IDLE` which defaults to 1000.
    To disable this limit, set *NumberOfProcs* to 0. The *NumberOfProcs*
    can be exceeded if a nodes job has a queue command with more than
    one proc to queue. i.e. ``queue 500`` will submit all procs even
    if *NumberOfProcs* is ``250``. In this case DAGMan will wait for
    for the number of idle procs to fall below 250 before submitting
    more jobs to the **condor_schedd**.
 **-maxjobs** *NumberOfClusters*
    Sets the maximum number of clusters within the DAG that will be
    submitted to HTCondor at one time. Each cluster is associated with
    one node job no matter how many individual procs are in the cluster.
    *NumberOfClusters* is a non-negative integer. If this option is
    omitted then the number of clusters is limited by the configuration
    variable :macro:`DAGMAN_MAX_JOBS_SUBMITTED` which defaults to 0 (unlimited).
 **-maxpre** *NumberOfPreScripts*
    Sets the maximum number of PRE scripts within the DAG that may be
    running at one time. *NumberOfPreScripts* is a non-negative integer.
    If this option is omitted, the number of PRE scripts is limited by
    the configuration variable :macro:`DAGMAN_MAX_PRE_SCRIPTS`
    which defaults to 20.
 **-maxpost** *NumberOfPostScripts*
    Sets the maximum number of POST scripts within the DAG that may be
    running at one time. *NumberOfPostScripts* is a non-negative
    integer. If this option is omitted, the number of POST scripts is
    limited by the configuration variable :macro:`DAGMAN_MAX_POST_SCRIPTS`
    which defaults to 20.
 **-notification** *value*
    Sets the e-mail notification for DAGMan itself. This information
    will be used within the HTCondor submit description file for DAGMan.
    This file is produced by *condor_submit_dag*. See the description
    of **notification** :index:`notification<single: notification; submit commands>`
    within *condor_submit* manual page for a specification of *value*.
 **-r** *schedd_name*
    Submit *condor_dagman* to a *condor_schedd* on a remote machine.
    It is assumed that any necessary files will be present on the
    remote machine via some method like a shared filesystem between the
    local and remote machines. The user also requires the correct
    permissions to submit remotely similarly to *condor_submit*'s
    **-remote** option. If other options are desired, including
    transfer of other input files, consider using the **-no_submit**
    option and modifying the resulting submit file for specific needs
    before using *condor_submit* on the prouduced DAGMan job submit file.
 **-debug** *level*
    Passes the the *level* of debugging output desired to
    *condor_dagman*. *level* is an integer, with values of 0-7
    inclusive, where 7 is the most verbose output. See the
    *condor_dagman* manual page for detailed descriptions of these
    values. If not specified, no **-debug** *V*\alue is passed to
    *condor_dagman*.
 **-usedagdir**
    This optional argument causes *condor_dagman* to run each specified
    DAG as if *condor_submit_dag* had been run in the directory
    containing that DAG file. This option is most useful when running
    multiple DAGs in a single *condor_dagman*. Note that the
    **-usedagdir** flag must not be used when running an old-style
    Rescue DAG.
 **-outfile_dir** *directory*
    Specifies the directory in which the ``.dagman.out`` file will be
    written. The *directory* may be specified relative to the current
    working directory as *condor_submit_dag* is executed, or specified
    with an absolute path. Without this option, the ``.dagman.out`` file
    is placed in the same directory as the first DAG input file listed
    on the command line.
 **-config** *ConfigFileName*
    Specifies a configuration file to be used for this DAGMan run. This
    configuration will apply to all DAGs submitted in via DAGMan. Note
    that only one custom configuration file can be specified for a DAGMan
    workflow which will cause a failure if used in conjuntion with a
    DAG using the ``CONFIG`` command.
 **-insert_sub_file** *FileName*
    Specifies a file to insert into the ``.condor.sub`` file created by
    *condor_submit_dag*. The specified file must contain only legal
    submit file commands. Only one file can be inserted. The specified
    file will override the file set by the configuration variable
    :macro:`DAGMAN_INSERT_SUB_FILE`. The specified file is inserted
    into the ``.condor.sub`` file before the queue command and
    any commands specified with the **-append** option.
 **-append** *Command*
    Specifies a command to append to the ``.condor.sub`` file created by
    *condor_submit_dag*. The specified command is appended to the
    ``.condor.sub`` file immediately before the queue command and after
    any commands added via **-insert_sub_file** or :macro:`DAGMAN_INSERT_SUB_FILE`.
    Multiple commands are specified by using the **-append** option
    multiple times. Commands with spaces in them must be enclosed in
    double quotes.
 **-batch-name** *batch_name*
    Set the batch name for this DAG/workflow. The batch name is
    displayed by *condor_q*. If omitted DAGMan will set the batch
    name to ``DagFile+ClusterId`` where *DagFile* is the name of
    the primary DAG submitted DAGMan and *ClusterId* is the DAGMan
    proper jobs :ad-attr:`ClusterId`. The batch name is set in all jobs
    submitted by DAGMan and propagated down into sub-DAGs. Note:
    set the batch name to ' ' (space) to avoid overriding batch
    names specified in node job submit files.
 **-autorescue** *0|1*
    Whether to automatically run the newest rescue DAG for the given DAG
    file, if one exists (0 = ``false``, 1 = ``true``).
 **-dorescuefrom** *number*
    Forces *condor_dagman* to run the specified rescue DAG number for
    the given DAG. A value of 0 is the same as not specifying this
    option. Specifying a non-existent rescue DAG is a fatal error.
 **-load_save** *filename*
    Specify a file with saved DAG progress to re-run the DAG from. If
    given a path DAGMan will attempt to read that file following that
    path. Otherwise, DAGMan will check for the file in the DAG's
    ``save_files`` sub-directory.
 **-allowversionmismatch**
    This optional argument causes *condor_dagman* to allow a version
    mismatch between *condor_dagman* itself and the ``.condor.sub``
    file produced by *condor_submit_dag* (or, in other words, between
    *condor_submit_dag* and *condor_dagman*). WARNING! This option
    should be used only if absolutely necessary. Allowing version
    mismatches can cause subtle problems when running DAGs.
 **-no_recurse**
    This optional argument causes *condor_submit_dag* to not run
    itself recursively on nested DAGs (this is now the default; this
    flag has been kept mainly for backwards compatibility).
 **-do_recurse**
    This optional argument causes *condor_submit_dag* to run itself
    recursively on nested DAGs to pre-produce their ``.condor.sub``
    files. DAG nodes specified with the **SUBDAG EXTERNAL** keyword
    or with submit file names ending in ``.condor.sub`` are considered
    nested DAGs. This flag is useful when the configuration variable
    :macro:`DAGMAN_GENERATE_SUBDAG_SUBMITS` is ``False`` (Not default).
 **-update_submit**
    This optional argument causes an existing ``.condor.sub`` file to
    not be treated as an error; rather, the ``.condor.sub`` file will be
    overwritten, but the existing values of **-maxjobs**, **-maxidle**,
    **-maxpre**, and **-maxpost** will be preserved.
 **-import_env**
    This optional argument causes *condor_submit_dag* to import the
    current environment into the **environment** command of the
    ``.condor.sub`` file it generates.
 **-include_env** *Variables*
     This optional argument takes a comma separated list of enviroment
     variables to add to ``.condor.sub`` ``getenv`` environment filter
     which causes found matching environment variables to be added to
     the DAGMan manager jobs **environment**.
 **-insert_env** *Key=Value*
     This optional argument takes a delimited string of *Key=Value* pairs
     to explicitly set into the ``.condor.sub`` files :ad-attr:`Environment` macro.
     The base delimiter is a semicolon that can be overriden by setting
     the first character in the string to a valid delimiting character.
     If multiple **-insert_env** flags contain the same *Key* then the last
     occurances *Value* will be set in the DAGMan jobs **environment**.
 **-DumpRescue**
    This optional argument tells *condor_dagman* to immediately dump a
    rescue DAG and then exit, as opposed to actually running the DAG.
    This feature is mainly intended for testing. The Rescue DAG file is
    produced whether or not there are parse errors reading the original
    DAG input file. The name of the file differs if there was a parse
    error.
 **-valgrind**
    This optional argument causes the submit description file generated
    for the submission of *condor_dagman* to be modified. The
    executable becomes *valgrind* run on *condor_dagman*, with a
    specific set of arguments intended for testing *condor_dagman*.
    Note that this argument is intended for testing purposes only. Using
    the **-valgrind** option without the necessary *valgrind* software
    installed will cause the DAG to fail. If the DAG does run, it will
    run much more slowly than usual.
 **-DontAlwaysRunPost**
    This option causes the submit description file generated for the
    submission of *condor_dagman* to be modified. It causes
    *condor_dagman* to not run the POST script of a node if the PRE
    script fails.
 **-AlwaysRunPost**
    This option causes the submit description file generated for the
    submission of *condor_dagman* to be modified. It causes
    *condor_dagman* to always run the POST script of a node, even if
    the PRE script fails.
 **-priority** *number*
    Sets the minimum job priority of node jobs submitted and running
    under the *condor_dagman* job submitted by this
    *condor_submit_dag* command.
 **-schedd-daemon-ad-file** *FileName*
    Specifies a full path to a daemon ad file dropped by a
    *condor_schedd*. Therefore this allows submission to a specific
    scheduler if several are available without repeatedly querying the
    *condor_collector*. The value for this argument defaults to the
    configuration attribute :macro:`SCHEDD_DAEMON_AD_FILE`.
 **-schedd-address-file** *FileName*
    Specifies a full path to an address file dropped by a
    *condor_schedd*. Therefore this allows submission to a specific
    scheduler if several are available without repeatedly querying the
    *condor_collector*. The value for this argument defaults to the
    configuration attribute :macro:`SCHEDD_ADDRESS_FILE`.
 **-suppress_notification**
    Causes jobs submitted by *condor_dagman* to not send email
    notification for events. The same effect can be achieved by setting
    configuration variable :macro:`DAGMAN_SUPPRESS_NOTIFICATION` to ``True``. This
    command line option is independent of the **-notification** command
    line option, which controls notification for the *condor_dagman*
    job itself.
 **-dont_suppress_notification**
    Causes jobs submitted by *condor_dagman* to defer to content within
    the submit description file when deciding to send email notification
    for events. The same effect can be achieved by setting configuration
    variable :macro:`DAGMAN_SUPPRESS_NOTIFICATION` to ``False``. This
    command line flag is independent of the **-notification** command
    line option, which controls notification for the *condor_dagman*
    job itself. If both **-dont_suppress_notification** and
    **-suppress_notification** are specified with the same command
    line, the last argument is used.
 **-DoRecovery**
    Causes *condor_dagman* to start in recovery mode. This means that
    DAGMan reads the relevant ``.nodes.log`` file to restore its previous
    state of node completions and failures to continue running.
 **-SubmitMethod** *value*
    This optional argument takes an enumerated value representing the
    method in which *condor_dagman* will submit managed jobs for execution.
    Enumeration values are as follows:

    -  **0** : Run :tool:`condor_submit`
    -  **1** : Directly submit job to local *condor_schedd* queue

Exit Status
-----------

*condor_submit_dag* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To run a single DAG:

.. code-block:: console

    $ condor_submit_dag diamond.dag

To run a DAG when it has already been run and the output files exist:

.. code-block:: console

    $ condor_submit_dag -force diamond.dag

To run a DAG, limiting the number of idle node jobs in the DAG to a
maximum of five:

.. code-block:: console

    $ condor_submit_dag -maxidle 5 diamond.dag

To run a DAG, limiting the number of concurrent PRE scripts to 10 and
the number of concurrent POST scripts to five:

.. code-block:: console

    $ condor_submit_dag -maxpre 10 -maxpost 5 diamond.dag

To run two DAGs, each of which is set up to run in its own directory:

.. code-block:: console

    $ condor_submit_dag -usedagdir dag1/diamond1.dag dag2/diamond2.dag

