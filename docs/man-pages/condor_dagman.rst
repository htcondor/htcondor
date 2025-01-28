:orphan:

*condor_dagman*
===============

Node scheduler for automating HTCondor job workflows.

:index:`condor_dagman<double: condor_dagman; HTCondor commands>`

Synopsis
--------

**condor_dagman** [**-help** | **-version**]

**condor_dagman** **-p 0** **-f** **-l .** [*OPTIONS*] *DAG_file* [*DAG_file*...]

**condor_dagman** **-p 0** **-f** **-l .** [**-WaitForDebug**]
[**-CsdVersion** *VersionString*] [**-AllowVersionMismatch**]
[**-Batch-Name** *name*] [**-Dag** *DAG_file*] [**-Debug** *level*]
[**-Dot**] [**-DryRun**] [**-DumpRescue**] [**-Lockfile** *filename*]
[**-Priority** *priority*] [**-SubmitMethod** *value*]
[**-UseDagDir**] [**-AlwaysRunPost** | **-DontAlwaysRunPost**]
[**-suppress_notification** | **-dont_suppress_notification**]
[**-AutoRescue** *<0|1>*] [**-DoRescueFrom** *N*] [**-DoRecovery**]
[**-load_save** *filename*] [**-MaxHold** *N*] [**-MaxIdle** *N*]
[**-MaxJobs** *N*] [**-MaxPost** *N*] [**-MaxPre** *N*]

Description
-----------

A meta node scheduler that automatically manages HTCondor user workflows
that describe jobs as nodes in a Directed Acyclic Graph (DAG). All parts
of the DAG (nodes, dependencies, visualizations, etc) are described in a
DAG Description File (``*.dag``) using the DAG Description Language (DDL).

Options
-------

 **-help**
    Display usage information.
 **-version**
    Display version information.
 **-WaitForDebug**
    Pause execution of DAGMan until debugger is attached.

    .. note::

        Requires manual setting of ``wait_for_debug`` variable to
        ``false`` to continue execution.
 **-CsdVersion** *VersionString*
    Version of :tool:`condor_submit_dag` that produced the scheduler
    universe job representing this DAGMan instance.
 **-AllowVersionMismatch**
    Allow a version difference between *condor_dagman* itself and the
    ``*.condor.sub`` file produced by :tool:`condor_submit_dag`.

    .. warning::

        This option should only be used if absolutely necessary because
        version mismatches can cause subtle problems when running DAGMan.
 **-Batch-Name** *name*
    Set the :ad-attr:`JobBatchName` to *name* for all jobs placed to the
    AP by *condor_dagman*.
 **-dag** *DAG_file*
    Specify a file detailing a DAG structure for *condor_dagman* for manage.
 **-Debug** *level*
    Set the debug log level (see below).
 **-Dot**
    Exit immediately after producing the first DAG visual :dag-cmd:`DOT` file.
 **-DryRun**
    Inform *condor_dagman* to execute the DAG without actually placing any
    jobs to the AP.
 **-DumpRescue**
    Inform *condor_dagman* to produce a full rescue DAG file and exit before
    executing the DAG.
 **-Lockfile** *filename*
    Path to a file to write *condor_dagman* process information. This prevents
    other *condor_dagman* executing the same DAG(s) from being executed in the
    same directory.
 **-Priority** *priority*
    Set the minimum :ad-attr:`JobPrio` for jobs managed by *condor_dagman*.
 **-SubmitMethod** *value*
    Specify how *condor_dagman* will place managed jobs to the AP.
 **-UseDagDir**
    Inform *condor_dagman* to execute each specified DAG from their respective
    directories.
 **-AlwaysRunPost**
    Always execute ``POST`` scripts even upon failure of any ``PRE`` scripts.
 **-DontAlwaysRunPost**
    Only execute ``POST`` scripts after a node's list of jobs have completed
    (Success or Failure). Default.
 **-suppress_notification**
    Suppress email notifications for all managed jobs.
 **-dont_suppress_notification**
    Allow email notifications for any managed jobs that have notifications
    specified. Default.
 **-AutoRescue** *<0|1>*
    Automatically detect rescue DAG files upon startup and rescue from the
    most recent (highest number) rescue file discovered. ``0`` is ``False``.
    Default ``1`` is ``True``.
 **-DoRescueFrom** *N*
    Specify a specific rescue number to locate and restore state from.
 **-DoRecovery**
    Specify to startup in recovery mode (restore state from ``*.nodes.log``).
 **-load_save** *filename*
    Specify a specific :dag-cmd:`SAVE_POINT_FILE` to restore state from.
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

General Remarks
---------------

``condor_dagman`` is not intended to be ran manually unless to debug or
test DAGMan for a specific reason. Instead to execute a DAG place a meta
scheduler universe job to the local AP via :tool:`condor_submit_dag` or
:tool:`htcondor dag submit`.

The following options that appear first are for *DaemonCore*:

    - **-p 0** : Causes DAGMan to run without a Socket.
    - **-f** : Run in foreground (i.e. not spawned by ``condor_master``).
    - **-l .** : Sets the debug log directory to the execution directory.

The following options from :tool:`condor_submit_dag` can also be provided
to ``condor_dagman`` which will be used for submitting :dag-cmd:`SUBDAG`\'s:

    - **-dagman**
    - **-f/-force**
    - **-import_env**
    - **-include_env** *Var1[,Var2...]*
    - **-insert_env** *key=value[,key=value...]*
    - **-notification**
    - **-outfile_dir**
    - **-update_submit**
    - **-v/-verbose**

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

0  -  Success

1  -  Failure has occurred

2  -  DAGMan aborted

3  -  DAGMan restart


Examples
--------

Display installed *condor_dagman* version:

.. code-block:: console

    $ condor_dagman -version

Display *condor_dagman* usage:

.. code-block:: console

    $ condor_dagman -help

Manually run ``sample.dag``:

.. code-block:: console

    $ condor_dagman -p 0 -f -l . sample.dag

Execute *condor_dagman* in a debugger:

.. code-block:: console

    $ condor_dagman -p 0 -f -l . -waitfordebug -dag sample.dag

See Also
--------

:tool:`condor_submit_dag`

Availability
------------

Linux, MacOS, Windows
