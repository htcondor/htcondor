*condor_submit_dag*
===================

Place a node scheduler job to local AP to manage job workflow described as a Direct Acyclic Graph (DAG).

:index:`condor_submit_dag<double: condor_submit_dag; HTCondor commands>`

Synopsis
--------

**condor_submit_dag** [**-help** | **-version**]

**condor_submit_dag** [*OPTIONS*] *DAG_file* [*DAG_file*...]

**condor_submit_dag** [**-no_submit**] [**-v/-verbose**] [**-f/-force**]
[**-dagman** *path*] [**-notification** *value*] [**-debug** *level*]
[**-MaxIdle** *N*] [**-MaxJobs** *N*] [**-MaxPre** *N*] [**-MaxPost** *N*]
[**-MaxHold** *N*] [**-suppress_notification** | **-dont_suppress_notification**]
[**-SubmitMethod** *value*] [**-UseDagDir**] [**-outfile_dir** *path*]
[**-config** *filename*] [**-Lockfile** *filename*] [**-insert_sub_file** *filename*]
[**-a/-append** *command*] [**-batch-name** *name*] [**-AutoRescue** *<0|1>*]
[**-DoRescueFrom** *N*] [**-load_save** *filename*] [**-DoRecovery**]
[**-AllowVersionMismatch**] [**-no_recurse** | **-do_recurse**]
[**-update_submit**] [**-import_env**] [**-include_env** *variable[,variable...]*]
[**-insert_env** *key=value[;key=value...]*] [**-DumpRescue**] [**-valgrind**]
[**-DontAlwaysRunPost** | **-AlwaysRunPost**] [**-priority** *N*]
[**-r/-remote** *schedd_name*] [**-schedd-daemon-ad-file** *filename*]
[**-schedd-address-file** *filename*]


Description
-----------

Automatically produce and place a scheduler :subcom:`universe` job
to execute HTCondor's DAGMan workflow manager on a list of specified
Directed Acyclic Graphs (DAGs) of HTCondor jobs. Extensive documentation
is in the :ref:`HTCondor DAGMan Workflows Section<DAGMan documentation>`.

Options
-------

 **-help**
    Display usage information.
 **-version**
    Display version information.
 **-no_submit**
    Produce the HTCondor submit description file for DAGMan, but do not
    submit DAGMan as an HTCondor job.
 **-v/-verbose**
    Cause *condor_submit_dag* to give verbose error messages.
 **-f/-force**
    Don't fail to place DAGMan job to AP if previous execution files
    are discovered. Previous execution files (except the ``*.dagman.out``)
    are cleaned up and old rescue files are renamed.
 **-dagman** *path*
    Specify a path to a *condor_dagman* executable to be executed rather
    than the installed one discovered in user's path.
 **-MaxIdle** *N*
    Set the maximum number of idle jobs allowed before placing more jobs to
    the AP. If not specified then this is set to the value of :macro:`DAGMAN_MAX_JOBS_IDLE`.

    .. note::

        *condor_dagman* can place jobs beyond this threshold in a single
        job placement cycle, but won't place more jobs until the detected
        number of idle jobs drops below the specified threshold again.

 **-MaxJobs** *N*
    Set the maximum number of nodes that have placed a list of jobs to
    the AP at any given moment. If not specified then this is set to the
    value of :macro:`DAGMAN_MAX_JOBS_SUBMITTED`.
 **-MaxPre** *N*
    Set the maximum number of ``PRE`` scripts being executed at any given
    moment. If not specified then this value is set to :macro:`DAGMAN_MAX_PRE_SCRIPTS`.
 **-MaxPost** *N*
    Set the maximum number of ``POST`` scripts being executed at any given
    moment. If not specified then this value is set to :macro:`DAGMAN_MAX_POST_SCRIPTS`.
 **-MaxHold** *N*
    Set the maximum number of ``HOLD`` scripts being executed at any given
    moment. If not specified then this value is set to :macro:`DAGMAN_MAX_HOLD_SCRIPTS`.
 **-notification** *value*
    Set the e-mail :subcom:`notification` for DAGMan itself.
 **-r/-remote** *schedd_name*
    Specify a *condor_schedd* on a remote machine to place the DAGMan
    job.

    .. note::

        Since DAGMan expects all necessary files to be present in
        it's working directory, all files used by DAGMan need to
        be present on the remote machine via some method like a
        shared filesystem.
 **-debug** *level*
    Set the DAGMan debug log level (see below for levels).
 **-UseDagDir**
    Inform DAGMan to execute each specified DAG from their respective
    directories.
 **-outfile_dir** *path*
    Specify the path to a directory for DAGMan's ``*.dagman.out`` debug
    log to be written.
 **-config** *filename*
    Specify an HTCondor configuration file to a specific DAGMan instance's
    execution.

    .. note::

        Only one configuration file can be specified which will cause
        a failure if used in conjunction with the :dag-cmd:`CONFIG` command.
 **-Lockfile** *filename*
    Path to a file to write DAGMan's process information. This prevents
    other DAGMan processes executing the same DAG(s) from being executed in the
    same directory.
 **-insert_sub_file** *filename*
    Specify a file containing JDL submit commands to be inserted in the
    produced DAGMan job submit description (``*.condor.sub``).

    .. note::

        Only one extra submit description file can be specified. So,
        the specified file will override any file specified via
        :macro:`DAGMAN_INSERT_SUB_FILE`
 **-a/-append** *command*
    Specify JDL commands to insert into the produced DAGMan job submit
    description (``*.condor.sub``). Can be used multiple times to add
    multiple JDL commands. Commands with spaces in them must be enclosed
    in double quotes.

    .. note::

        JDL commands specified via **-append** take precedence over
        any commands added via **-insert_sub_file** or :macro:`DAGMAN_INSERT_SUB_FILE`.
 **-batch-name** *name*
    Set the :ad-attr:`JobBatchName` to *name* for DAGMan and all jobs managed
    by DAGMan.
 **-AutoRescue** *<0|1>*
    Automatically detect rescue DAG files upon startup and rescue from the
    most recent (highest number) rescue file discovered. ``0`` is ``False``.
    Default ``1`` is ``True``.
 **-DoRescueFrom** *N*
    Specify a specific rescue number to locate and restore state from.
 **-load_save** *filename*
    Specify a specific :dag-cmd:`SAVE_POINT_FILE` to restore state from.
    If provided a path DAGMan will attempt to read that file following
    that path. Otherwise, DAGMan will check for the file in the DAG's
    ``save_files`` sub-directory.
 **-DoRecovery**
    Inform DAGMan to startup in recovery mode (restore state from ``*.nodes.log``).
 **-AllowVersionMismatch**
    Allow a version difference between *condor_dagman* itself and the
    ``*.condor.sub`` file produced by :tool:`condor_submit_dag`.

    .. warning::

        This option should only be used if absolutely necessary because
        version mismatches can cause subtle problems when running DAGMan.
 **-no_recurse**
    Don't recursively pre-produce :dag-cmd:`SUBDAG`\'s submit description
    files prior to placing root DAG to the AP.
 **-do_recurse**
    Recursively pre-produce all :dag-cmd:`SUBDAG`\'s submit description
    files prior to placing the root DAG to the AP.
 **-update_submit**
    Don't treat an existing DAGMan submit description file (``*.condor.sub``)
    as an error; rather update the file while preserving the **-maxjobs**,
    **-maxidle**, **-maxpre**, and **-maxpost** options (if specified).
 **-import_env**
    Inform *condor_submit_dag* to import the current shell environment into
    the produced DAGMan submit description file's :subcom:`environment` command.
 **-include_env** *variable[,variable...]*
    Specify a comma separated list of environment variables to add to
    the produced DAGMan submit description file's :subcom:`getenv` command.
 **-insert_env** *key=value[;key=value...]*
    Specify a delimited string of *key=value* pairs to explicitly set into
    the produced DAGMan submit description file's :subcom:`environment` command.
    If the same *key* is specified multiple times then the last occurrences
    *value* takes precedence.

    .. note::

        The base delimiter is a semicolon that can be overridden by setting
        the first character in the string to a valid delimiting character.

        .. code-block:: console

            $ condor_submit_dag -insert_env |foo=0|bar=1|baz=2
 **-DumpRescue**
    Inform DAGMan to produce a full rescue DAG file and exit before
    executing the DAG.
 **-valgrind**
    Run DAGMan under *valgrind*.

    .. note::

        This is option is intended for testing and development of DAGMan.
        The DAGMan execution speed is drastically reduced.

    .. warning::

        Failure will occur if necessary *valgrind* is not installed.
        *valgrind* is only available of Linux OS.
 **-DontAlwaysRunPost**
    Always execute ``POST`` scripts even upon failure of any ``PRE`` scripts.
 **-AlwaysRunPost**
    Only execute ``POST`` scripts after a node's list of jobs have completed
    (Success or Failure). Default.
 **-priority** *N*
    Set the minimum :ad-attr:`JobPrio` for jobs managed by DAGMan.
 **-schedd-daemon-ad-file** *filename*
    Specifies a full path to a daemon ad file for a specific *condor_schedd*
    to place the DAGMan job.
 **-schedd-address-file** *filename*
    Specifies a full path to an address file for a specific *condor_schedd*
    to place the DAGMan job.
 **-suppress_notification**
    Suppress email notifications for all jobs managed by DAGMan.
 **-dont_suppress_notification**
    Allow email notifications for any jobs managed by DAGMan that have notifications
    specified. Default.
 **-SubmitMethod** *value*
    Specify how DAGMan will place managed jobs to the AP (see *value*\s below).

General Remarks
---------------

.. note::

    All command line flags are case insensitive.

Some of the command line options also have corresponding configuration
values. The values specified via the command line will take precedence
over any configured values.

Some of the command line options are passed down to DAGMan to use when
executing :dag-cmd:`SUBDAG`\s.

Debug Level Values
^^^^^^^^^^^^^^^^^^

+-------+----------------------------------+
| level |            Details               |
+=======+==================================+
|   0   | Never produce output except for  |
|       | usage info.                      |
+-------+----------------------------------+
|   1   | Very quiet. Only output severe   |
|       | errors.                          |
+-------+----------------------------------+
|   2   | Normal output and error messages |
+-------+----------------------------------+
|   3   | (Default) Print warnings.        |
+-------+----------------------------------+
|   4   | Internal debugging output.       |
+-------+----------------------------------+
|   5   | Outer loop debugging.            |
+-------+----------------------------------+
|   6   | Inner loop debugging.            |
+-------+----------------------------------+
|   7   | Output parsed DAG input lines.   |
+-------+----------------------------------+

Submit Method Values
^^^^^^^^^^^^^^^^^^^^

+-------+------------------------------+
| Value |           Method             |
+=======+==============================+
|   0   | Run :tool:`condor_submit`    |
+-------+------------------------------+
|   1   | Direct place job(s) to local |
|       | *condor_schedd*              |
+-------+------------------------------+

Exit Status
-----------

0 - Success

1 - Failure

Examples
--------

Execute a single DAG:

.. code-block:: console

    $ condor_submit_dag sample.dag

Execute a single DAG that has successfully completed once already:

.. code-block:: console

    $ condor_submit_dag -force sample.dag

Execute a DAG with a max threshold of 10 idle jobs:

.. code-block:: console

    $ condor_submit_dag -maxidle 10 sample.dag

Execute a DAG with a max limit of 10 ``PRE`` scripts and 5 ``POST``
scripts executing concurrently:

.. code-block:: console

    $ condor_submit_dag -maxpre 10 -maxpost 5 sample.dag

Execute multiple DAGs:

.. code-block:: console

    $ condor_submit_dag first.dag second.dag

Execute multiple DAGs in their respective directories:

.. code-block:: console

    $ condor_submit_dag -usedagdir subdir1/first.dag subdir2/second.dag

Execute DAG and notify user once DAG is complete:

.. code-block:: console

    $ condor_submit_dag -notification complete sample.dag

Execute DAG with a custom batch name:

.. code-block:: console

    $ condor_submit_dag -batch-name my-awesome-dag sample.dag

Execute DAG and restore state from specific rescue file 8:

.. code-block::

    $ condor_submit_dag -dorescuefrom 8 sample.dag

Execute DAG and restore state from save file post-analysis.save:

.. code-block:: console

    $ condor_submit_dag -load_save post-analysis.save sample.dag

Execute DAG and suppress all job e-mail notifications:

.. code-block:: console

    $ condor_submit_dag -suppress_notification sample.dag

See Also
--------

None

Availability
------------

Linux, MacOS, Windows
