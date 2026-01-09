:index:`DAGMan Options<single: Configuration; DAGMan Options>`

.. _DAGMan Configuration:

DAGMan Configuration Options
============================

These macros affect the operation of DAGMan and DAGMan jobs within
HTCondor.

.. note::

    Many of these configuration variables are appropriate to set on a
    per DAG basis. For more information see :ref:`Per DAG Config`.

.. warning::

    Configuration settings do not get applied to running DAGMan workflows
    when executing :tool:`condor_reconfig`.

:index:`General<single: DAGMan Configuration Sections; General>`

General
-------

:macro-def:`DAGMAN_CONFIG_FILE`
    The path to the configuration file to be used by :tool:`condor_dagman`.
    This option is set by :tool:`condor_submit_dag` automatically and should not be
    set explicitly by the user. Defaults to an empty string.

:macro-def:`DAGMAN_USE_OLD_FILE_PARSER`
    A boolean that defaults to ``False``, when ``True`` *condor_dagman* will use
    the old file parser to process DAG files.

.. note::

    This option is intended to be a fall back to the known working DAG file parser
    while transitioning to the new style parser. This will be deprecated in the
    future.

:macro-def:`DAGMAN_USE_STRICT`
    An integer defining the level of strictness :tool:`condor_dagman` will
    apply when turning warnings into fatal errors, as follows:

    -  0: no warnings become errors
    -  1: severe warnings become errors
    -  2: medium-severity warnings become errors
    -  3: almost all warnings become errors

    Using a strictness value greater than 0 may help find problems with
    a DAG that may otherwise escape notice. The default value if not
    defined is 1.

:macro-def:`DAGMAN_STARTUP_CYCLE_DETECT`
    A boolean value that defaults to ``False``. When ``True``, causes
    :tool:`condor_dagman` to check for cycles in the DAG before submitting
    DAG node jobs, in addition to its run time cycle detection.

.. note::

    The startup process for DAGMan is much slower for large DAGs when set
    to ``True``.

:macro-def:`DAGMAN_ABORT_DUPLICATES`
    A boolean that defaults to ``True``. When ``True``, upon startup DAGMan
    will check to see if a previous DAGMan process for a specific DAG is
    still running and prevent the new DAG instance from executing. This
    check is done by checking for a DAGMan lock file and verifying whether
    or not the recorded PID's associated process is still running.

.. note::

    This value should rarely be changed, especially by users, since have
    multiple DAGMan processes executing the same DAG in the same directory
    can lead to strange behavior and issues.

:macro-def:`DAGMAN_USE_SHARED_PORT`
    A boolean that defaults to ``False``. When ``True``, :tool:`condor_dagman`
    will attempt to connect to the shared port daemon.

.. note::

    This value should never be changed since it was added to prevent spurious
    shared port related error messages from appearing the DAGMan debug log.

:macro-def:`DAGMAN_USE_DIRECT_SUBMIT`
    A boolean value that defaults to ``True``. When ``True``, :tool:`condor_dagman`
    will open a direct connection to the local *condor_schedd* to submit jobs rather
    than spawning the :tool:`condor_submit` process.

:macro-def:`DAGMAN_PRODUCE_JOB_CREDENTIALS`
    A boolean value that defaults to ``True``. When ``True``, :tool:`condor_dagman`
    will attempt to produce needed credentials for jobs at submit time when using
    direct submission.

:macro-def:`DAGMAN_USE_JOIN_NODES`
    A boolean value that defaults to ``True``. When ``True``, :tool:`condor_dagman`
    will create special *join nodes* for :dag-cmd:`PARENT/CHILD` relationships between
    multiple parent nodes and multiple child nodes.

.. note::

    This should never be changed since it reduces the number of dependencies in the
    graph resulting in a significant improvement to the memory footprint, parse time,
    and submit speed of :tool:`condor_dagman` (Especially for large DAGs).

:macro-def:`DAGMAN_PUT_FAILED_JOBS_ON_HOLD`
    A boolean value that defaults to ``False``. When ``True``, DAGMan will automatically
    retry a node with its job submitted on hold if any of the nodes jobs fail. Script
    failures do not cause this behavior. The job is only put on hold if the node has no
    more declared :dag-cmd:`RETRY` attempts.

:macro-def:`DAGMAN_NODE_JOB_FAILURE_TOLERANCE`
    An integer value representing the number of jobs in a single cluster that can fail
    before DAGMan considers the cluster as failed and removes any remaining jobs. This
    value is applied to all nodes in the DAG for each execution. The default value is
    ``0`` meaning no jobs should fail.

    .. warning::

        If the tolerance value is greater than or equal to the total number of jobs in
        a cluster then DAGMan will consider the cluster as successful even if all jobs
        fail.

:macro-def:`DAGMAN_DEFAULT_APPEND_VARS`
    A boolean value that defaults to ``False``. When ``True``, variables
    parsed in the DAG file :dag-cmd:`VARS` line will be appended to the given Job
    submit description file unless :dag-cmd:`VARS` specifies *PREPEND* or *APPEND*.
    When ``False``, the parsed variables will be prepended unless specified.

:macro-def:`DAGMAN_MANAGER_JOB_APPEND_GETENV`
    A comma separated list of variable names to add to the DAGMan ``*.condor.sub``
    file's :subcom:`getenv` option. This will in turn add any found matching environment
    variables to the DAGMan proper jobs :ad-attr:`Environment`. Setting this value to
    ``True`` will result in ``getenv = true``. The Base ``*.condor.sub`` values for
    :subcom:`getenv` are the following:

    +---------------+--------------------+--------------------+--------------------+
    |               |        PATH        |        HOME        |        USER        |
    |               +--------------------+--------------------+--------------------+
    | General Shell |         TZ         |        LANG        |       LC_ALL       |
    |               +--------------------+--------------------+--------------------+
    |               |     PYTHONPATH     |        PERL*       |                    |
    +---------------+--------------------+--------------------+--------------------+
    |    HTCondor   |   CONDOR_CONFIG    |       CONDOR_*     |                    |
    +---------------+--------------------+--------------------+--------------------+
    |    Scitoken   |    BEARER_TOKEN    | BEAERER_TOKEN_FILE |   XDG_RUNTIME_DIR  |
    +---------------+--------------------+--------------------+--------------------+
    |     Misc.     |     PEGASUS_*      |                    |                    |
    +---------------+--------------------+--------------------+--------------------+

:macro-def:`DAGMAN_NODE_RECORD_INFO`
    A string that when set to ``RETRY`` will cause DAGMan to insert a nodes current
    retry attempt number into the nodes job ad as the attribute :ad-attr:`DAGManNodeRetry`
    at submission time. This knob is not set by default.

:macro-def:`DAGMAN_RECORD_MACHINE_ATTRS`
    A comma separated list of machine attributes that DAGMan will insert into a node jobs
    submit description for :subcom:`job_ad_information_attrs` and :subcom:`job_machine_attrs`.
    This will result in the listed machine attributes to be injected into the nodes produced
    job ads and userlog. This knob is not set by default.

:macro-def:`DAGMAN_METRICS_FILE_VERSION`
    An integer value that represents the version of metrics file to write (see info below).
    This value defaults to ``2``.

    V1 Metrics File (1):
        Original metric file output that refers to DAG nodes as ``jobs``.
    V2 Metrics File (2):
        New metric file using updated terminology (i.e. using the word ``nodes``).

:macro-def:`DAGMAN_REPORT_GRAPH_METRICS`
    A boolean that defaults to ``False``. When ``True``, DAGMan will write additional
    information regarding graph metrics to ``*.metrics`` file. The included graph metrics
    are as follows:

    - Graph Height
    - Graph Width
    - Number of edges (dependencies)
    - Number of vertices (nodes)

:macro-def:`DAGMAN_DISABLE_PORT`
    A boolean that defaults to ``False``. When ``True``, DAGMan will not open up a command
    port.

    .. warning::

        Disabling the command port will make tools, such as :tool:`htcondor dag halt`
        that talk directly to a running DAGMan process fail.

:index:`Throttling<single: DAGMan Configuration Sections; Throttling>`

Throttling
----------

:macro-def:`DAGMAN_MAX_JOBS_IDLE`
    An integer value that controls the maximum number of idle procs
    allowed within the DAG before :tool:`condor_dagman` temporarily stops
    submitting jobs. :tool:`condor_dagman` will resume submitting jobs once
    the number of idle procs falls below the specified limit.
    :macro:`DAGMAN_MAX_JOBS_IDLE` currently counts each individual proc
    within a cluster as a job, which is inconsistent with
    :macro:`DAGMAN_MAX_JOBS_SUBMITTED`. Note that submit description files
    that queue multiple procs can cause the :macro:`DAGMAN_MAX_JOBS_IDLE`
    limit to be exceeded. If a submit description file contains
    ``queue 5000`` and :macro:`DAGMAN_MAX_JOBS_IDLE` is set to 250, this will
    result in 5000 procs being submitted to the *condor_schedd*, not
    250; in this case, no further jobs will then be submitted by
    :tool:`condor_dagman` until the number of idle procs falls below 250. The
    default value is 1000. To disable this limit, set the value to 0.
    This configuration option can be overridden by the
    :tool:`condor_submit_dag` **-maxidle** command-line argument (see
    :doc:`/man-pages/condor_submit_dag`).

:macro-def:`DAGMAN_MAX_JOBS_SUBMITTED`
    An integer value that controls the maximum number of node jobs
    (clusters) within the DAG that will be submitted to HTCondor at one
    time. A single invocation of :tool:`condor_submit` by :tool:`condor_dagman`
    counts as one job, even if the submit file produces a multi-proc
    cluster. The default value is 0 (unlimited). This configuration
    option can be overridden by the :tool:`condor_submit_dag`
    **-maxjobs** command-line argument (see :doc:`/man-pages/condor_submit_dag`).

:macro-def:`DAGMAN_MAX_PRE_SCRIPTS`
    An integer defining the maximum number of PRE scripts that any given
    :tool:`condor_dagman` will run at the same time. The value 0 allows any
    number of PRE scripts to run. The default value if not defined is
    20. Note that the :macro:`DAGMAN_MAX_PRE_SCRIPTS` value can be overridden
    by the :tool:`condor_submit_dag` **-maxpre** command line option.

:macro-def:`DAGMAN_MAX_POST_SCRIPTS`
    An integer defining the maximum number of POST scripts that any
    given :tool:`condor_dagman` will run at the same time. The value 0 allows
    any number of POST scripts to run. The default value if not defined
    is 20. Note that the :macro:`DAGMAN_MAX_POST_SCRIPTS` value can be
    overridden by the :tool:`condor_submit_dag` **-maxpost** command line
    option.

:macro-def:`DAGMAN_MAX_HOLD_SCRIPTS`
    An integer defining the maximum number of HOLD scripts that any
    given :tool:`condor_dagman` will run at the same time. The default value
    0 allows any number of HOLD scripts to run.

:macro-def:`DAGMAN_REMOVE_JOBS_AFTER_LIMIT_CHANGE`
    A boolean that determines if after changing some of these throttle limits,
    :tool:`condor_dagman` should forceably remove jobs to meet the new limit.
    Defaults to ``False``.

:index:`Node Semantics<single: DAGMan Configuration Sections; Node Semantics>`

Priority, node semantics
------------------------

:macro-def:`DAGMAN_DEFAULT_PRIORITY`
    An integer value defining the minimum priority of node jobs running
    under this :tool:`condor_dagman` job. Defaults to 0.

:macro-def:`DAGMAN_SUBMIT_DEPTH_FIRST`
    A boolean value that controls whether to submit ready DAG node jobs
    in (more-or-less) depth first order, as opposed to breadth-first
    order. Setting :macro:`DAGMAN_SUBMIT_DEPTH_FIRST` to ``True`` does not
    override dependencies defined in the DAG. Rather, it causes newly
    ready nodes to be added to the head, rather than the tail, of the
    ready node list. If there are no PRE scripts in the DAG, this will
    cause the ready nodes to be submitted depth-first. If there are PRE
    scripts, the order will not be strictly depth-first, but it will
    tend to favor depth rather than breadth in executing the DAG. If
    :macro:`DAGMAN_SUBMIT_DEPTH_FIRST` is set to ``True``, consider also
    setting :macro:`DAGMAN_RETRY_SUBMIT_FIRST` and
    :macro:`DAGMAN_RETRY_NODE_FIRST` to ``True``. If not defined,
    :macro:`DAGMAN_SUBMIT_DEPTH_FIRST` defaults to ``False``.

:macro-def:`DAGMAN_ALWAYS_RUN_POST`
    A boolean value defining whether :tool:`condor_dagman` will ignore the
    return value of a PRE script when deciding whether to run a POST
    script. The default is ``False``, which means that the failure of a
    PRE script causes the POST script to not be executed. Changing this
    to ``True`` will restore the previous behavior of :tool:`condor_dagman`,
    which is that a POST script is always executed, even if the PRE
    script fails.

:index:`Job Management<single: DAGMan Configuration Sections; Job Management>`

Node job submission/removal
---------------------------

:macro-def:`DAGMAN_USER_LOG_SCAN_INTERVAL[NEGOTIATOR]`
    An integer value representing the number of seconds that
    :tool:`condor_dagman` waits between checking the workflow log file for
    status updates. Setting this value lower than the default increases
    the CPU time :tool:`condor_dagman` spends checking files, perhaps
    fruitlessly, but increases responsiveness to nodes completing or
    failing. The legal range of values is 1 to INT_MAX. If not defined,
    it defaults to 5 seconds. This default may be automatically decreased
    if :macro:`DAGMAN_MAX_JOBS_IDLE` is set to a small value. If so,
    this will be noted in the ``dagman.out`` file.

:macro-def:`DAGMAN_MAX_SUBMITS_PER_INTERVAL`
    An integer that controls how many individual jobs :tool:`condor_dagman`
    will submit in a row before servicing other requests (such as a
    :tool:`condor_rm`). The legal range of values is 1 to 1000. If defined
    with a value less than 1, the value 1 will be used. If defined with
    a value greater than 1000, the value 1000 will be used. If not
    defined, it defaults to 100. This default may be automatically
    decreased if :macro:`DAGMAN_MAX_JOBS_IDLE` is set to a small value. If so,
    this will be noted in the ``dagman.out`` file.

    **Note: The maximum rate at which DAGMan can submit jobs is
    DAGMAN_MAX_SUBMITS_PER_INTERVAL / DAGMAN_USER_LOG_SCAN_INTERVAL.**

:macro-def:`DAGMAN_MAX_SUBMIT_ATTEMPTS`
    An integer that controls how many times in a row :tool:`condor_dagman`
    will attempt to execute :tool:`condor_submit` for a given job before
    giving up. Note that consecutive attempts use an exponential
    backoff, starting with 1 second. The legal range of values is 1 to
    16. If defined with a value less than 1, the value 1 will be used.
    If defined with a value greater than 16, the value 16 will be used.
    Note that a value of 16 would result in :tool:`condor_dagman` trying for
    approximately 36 hours before giving up. If not defined, it defaults
    to 6 (approximately two minutes before giving up).

:macro-def:`DAGMAN_MAX_JOB_HOLDS`
    An integer value defining the maximum number of times a node job is
    allowed to go on hold. As a job goes on hold this number of times,
    it is removed from the queue. For example, if the value is 2, as the
    job goes on hold for the second time, it will be removed. At this
    time, this feature is not fully compatible with node jobs that have
    more than one ``ProcID``. The number of holds of each process in the
    cluster count towards the total, rather than counting individually.
    So, this setting should take that possibility into account, possibly
    using a larger value. A value of 0 allows a job to go on hold any
    number of times. The default value if not defined is 100.

:macro-def:`DAGMAN_HOLD_CLAIM_TIME`
    An integer defining the number of seconds that :tool:`condor_dagman` will
    cause a hold on a claim after a job is finished, using the job
    ClassAd attribute :ad-attr:`KeepClaimIdle`. The default value is 20. A
    value of 0 causes :tool:`condor_dagman` not to set the job ClassAd
    attribute.

:macro-def:`DAGMAN_SUBMIT_DELAY`
    An integer that controls the number of seconds that :tool:`condor_dagman`
    will sleep before submitting consecutive jobs. It can be increased
    to help reduce the load on the *condor_schedd* daemon. The legal
    range of values is any non negative integer. If defined with a value
    less than 0, the value 0 will be used.

:macro-def:`DAGMAN_PROHIBIT_MULTI_JOBS`
    A boolean value that controls whether :tool:`condor_dagman` prohibits
    node job submit description files that queue multiple job procs
    other than parallel universe. If a DAG references such a submit
    file, the DAG will abort during the initialization process. If not
    defined, :macro:`DAGMAN_PROHIBIT_MULTI_JOBS` defaults to ``False``.

:macro-def:`DAGMAN_GENERATE_SUBDAG_SUBMITS`
    A boolean value specifying whether :tool:`condor_dagman` itself should
    create the ``.condor.sub`` files for nested DAGs. If set to
    ``False``, nested DAGs will fail unless the ``.condor.sub`` files
    are generated manually by running :tool:`condor_submit_dag`
    *-no_submit* on each nested DAG, or the *-do_recurse* flag is
    passed to :tool:`condor_submit_dag` for the top-level DAG. DAG nodes
    specified with the ``SUBDAG EXTERNAL`` keyword or with submit
    description file names ending in ``.condor.sub`` are considered
    nested DAGs. The default value if not defined is ``True``.

:macro-def:`DAGMAN_REMOVE_NODE_JOBS`
    A boolean value that controls whether :tool:`condor_dagman` removes its
    node jobs itself when it is removed (in addition to the
    *condor_schedd* removing them). Note that setting
    :macro:`DAGMAN_REMOVE_NODE_JOBS` to ``True`` is the safer option (setting
    it to ``False`` means that there is some chance of ending up with
    "orphan" node jobs). Setting :macro:`DAGMAN_REMOVE_NODE_JOBS` to
    ``False`` is a performance optimization (decreasing the load on the
    *condor_schedd* when a :tool:`condor_dagman` job is removed). Note that
    even if :macro:`DAGMAN_REMOVE_NODE_JOBS` is set to ``False``,
    :tool:`condor_dagman` will remove its node jobs in some cases, such as a
    DAG abort triggered by an *ABORT-DAG-ON* command. Defaults to
    ``True``.

:macro-def:`DAGMAN_MUNGE_NODE_NAMES`
    A boolean value that controls whether :tool:`condor_dagman` automatically
    renames nodes when running multiple DAGs. The renaming is done to
    avoid possible name conflicts. If this value is set to ``True``, all
    node names have the DAG number followed by the period character (.)
    prepended to them. For example, the first DAG specified on the
    :tool:`condor_submit_dag` command line is considered DAG number 0, the
    second is DAG number 1, etc. So if DAG number 2 has a node named B,
    that node will internally be renamed to 2.B. If not defined,
    :macro:`DAGMAN_MUNGE_NODE_NAMES` defaults to ``True``. **Note: users
    should rarely change this setting.**

:macro-def:`DAGMAN_SUPPRESS_JOB_LOGS`
    A boolean value specifying whether events should be written to a log
    file specified in a node job's submit description file. The default
    value is ``False``, such that events are written to a log file
    specified by a node job.

:macro-def:`DAGMAN_SUPPRESS_NOTIFICATION`
    A boolean value that controls whether jobs submitted by :tool:`condor_dagman`
    can send email notifications. If **True** then no submitted jobs will
    send email notifications. This is equivalent to setting :subcom:`notification`
    to ``Never``. Defaults to **False**.

:macro-def:`DAGMAN_CONDOR_SUBMIT_EXE`
    The executable that :tool:`condor_dagman` will use to submit HTCondor
    jobs. If not defined, :tool:`condor_dagman` looks for :tool:`condor_submit` in
    the path. **Note: users should rarely change this setting.**

:macro-def:`DAGMAN_CONDOR_RM_EXE`
    The executable that :tool:`condor_dagman` will use to remove HTCondor
    jobs. If not defined, :tool:`condor_dagman` looks for :tool:`condor_rm` in the
    path. **Note: users should rarely change this setting.**

:macro-def:`DAGMAN_ABORT_ON_SCARY_SUBMIT`
    A boolean value that controls whether to abort a DAG upon detection
    of a scary submit event. An example of a scary submit event is one
    in which the HTCondor ID does not match the expected value. Note
    that in all HTCondor versions prior to 6.9.3, :tool:`condor_dagman` did
    not abort a DAG upon detection of a scary submit event. This
    behavior is what now happens if :macro:`DAGMAN_ABORT_ON_SCARY_SUBMIT` is
    set to ``False``. If not defined, :macro:`DAGMAN_ABORT_ON_SCARY_SUBMIT`
    defaults to ``True``. **Note: users should rarely change this
    setting.**

:index:`Rescue and Retry<single: DAGMan Configuration Sections; Rescue and Retry>`

Rescue/retry
------------

:macro-def:`DAGMAN_AUTO_RESCUE`
    A boolean value that controls whether :tool:`condor_dagman` automatically
    runs Rescue DAGs. If :macro:`DAGMAN_AUTO_RESCUE` is ``True`` and the DAG
    input file ``my.dag`` is submitted, and if a Rescue DAG such as the
    examples ``my.dag.rescue001`` or ``my.dag.rescue002`` exists, then
    the largest magnitude Rescue DAG will be run. If not defined,
    :macro:`DAGMAN_AUTO_RESCUE` defaults to ``True``.

:macro-def:`DAGMAN_MAX_RESCUE_NUM`
    An integer value that controls the maximum Rescue DAG number that
    will be written, in the case that ``DAGMAN_OLD_RESCUE`` is
    ``False``, or run if :macro:`DAGMAN_AUTO_RESCUE` is ``True``. The maximum
    legal value is 999; the minimum value is 0, which prevents a Rescue
    DAG from being written at all, or automatically run. If not defined,
    :macro:`DAGMAN_MAX_RESCUE_NUM` defaults to 100.

:macro-def:`DAGMAN_RESET_RETRIES_UPON_RESCUE`
    A boolean value that controls whether node retries are reset in a
    Rescue DAG. If this value is ``False``, the number of node retries
    written in a Rescue DAG is decreased, if any retries were used in
    the original run of the DAG; otherwise, the original number of
    retries is allowed when running the Rescue DAG. If not defined,
    :macro:`DAGMAN_RESET_RETRIES_UPON_RESCUE` defaults to ``True``.

:macro-def:`DAGMAN_WRITE_PARTIAL_RESCUE`
    A boolean value that controls whether :tool:`condor_dagman` writes a
    partial or a full DAG file as a Rescue DAG. If not defined,
    :macro:`DAGMAN_WRITE_PARTIAL_RESCUE` defaults to ``True``. **Note: users
    should rarely change this setting.**

    .. warning::

        :macro:`DAGMAN_WRITE_PARTIAL_RESCUE[Deprecation Warning]` is deprecated
        as the writing of full Rescue DAG's is deprecated. This is slated to be
        removed during the lifetime of the HTCondor V24 feature series.

:macro-def:`DAGMAN_RETRY_SUBMIT_FIRST`
    A boolean value that controls whether a failed submit is retried
    first (before any other submits) or last (after all other ready jobs
    are submitted). If this value is set to ``True``, when a job submit
    fails, the job is placed at the head of the queue of ready jobs, so
    that it will be submitted again before any other jobs are submitted.
    This had been the behavior of :tool:`condor_dagman`. If this value is set
    to ``False``, when a job submit fails, the job is placed at the tail
    of the queue of ready jobs. If not defined, it defaults to ``True``.

:macro-def:`DAGMAN_RETRY_NODE_FIRST`
    A boolean value that controls whether a failed node with retries is
    retried first (before any other ready nodes) or last (after all
    other ready nodes). If this value is set to ``True``, when a node
    with retries fails after the submit succeeded, the node is placed at
    the head of the queue of ready nodes, so that it will be tried again
    before any other jobs are submitted. If this value is set to
    ``False``, when a node with retries fails, the node is placed at the
    tail of the queue of ready nodes. This had been the behavior of
    :tool:`condor_dagman`. If not defined, it defaults to ``False``.

:index:`Nodes Log File<single: DAGMan Configuration Sections; Nodes Log File>`

Log files
---------

:macro-def:`DAGMAN_DEFAULT_NODE_LOG`
    The default name of a file to be used as a job event log by all node
    jobs of a DAG.

    This configuration variable uses a special syntax in which **@**
    instead of **$** indicates an evaluation of special variables.
    Normal HTCondor configuration macros may be used with the normal
    **$** syntax.

    Special variables to be used only in defining this configuration
    variable:

    -  ``@(DAG_DIR)``: The directory in which the primary DAG input file
       resides. If more than one DAG input file is specified to
       :tool:`condor_submit_dag`, the primary DAG input file is the leftmost
       one on the command line.
    -  ``@(DAG_FILE)``: The name of the primary DAG input file. It does
       not include the path.
    -  ``@(CLUSTER)``: The :ad-attr:`ClusterId` attribute of the
       :tool:`condor_dagman` job.
    -  ``@(OWNER)``: The user name of the user who submitted the DAG.
    -  ``@(NODE_NAME)``: For SUBDAGs, this is the node name of the
       SUBDAG in the upper level DAG; for a top-level DAG, it is the
       string ``"undef"``.

    If not defined, ``@(DAG_DIR)/@(DAG_FILE).nodes.log`` is the default
    value.

    Notes:

    -  Using ``$(LOG)`` in defining a value for
       :macro:`DAGMAN_DEFAULT_NODE_LOG` will not have the expected effect,
       because ``$(LOG)`` is defined as ``"."`` for :tool:`condor_dagman`. To
       place the default log file into the log directory, write the
       expression relative to a known directory, such as
       ``$(LOCAL_DIR)/log`` (see examples below).
    -  A default log file placed in the spool directory will need extra
       configuration to prevent :tool:`condor_preen` from removing it; modify
       :macro:`VALID_SPOOL_FILES`. Removal of the default log file during a
       run will cause severe problems.
    -  **The value defined for DAGMAN_DEFAULT_NODE_LOG must ensure that the
       file is unique for each DAG.** Therefore, the value should always
       include ``@(DAG_FILE)``. For example,

       .. code-block:: condor-config

             DAGMAN_DEFAULT_NODE_LOG = $(LOCAL_DIR)/log/@(DAG_FILE).nodes.log


       is okay, but

       .. code-block:: condor-config

             DAGMAN_DEFAULT_NODE_LOG = $(LOCAL_DIR)/log/dag.nodes.log


       will cause failure when more than one DAG is run at the same time
       on a given access point.

:macro-def:`DAGMAN_LOG_ON_NFS_IS_ERROR`
    A boolean value that controls whether :tool:`condor_dagman` prohibits a
    DAG workflow log from being on an NFS file system. This value is
    ignored if :macro:`CREATE_LOCKS_ON_LOCAL_DISK` and
    :macro:`ENABLE_USERLOG_LOCKING` are both ``True``. If a DAG uses such a
    workflow log file file and :macro:`DAGMAN_LOG_ON_NFS_IS_ERROR` is
    ``True`` (and not ignored), the DAG will abort during the
    initialization process. If not defined,
    :macro:`DAGMAN_LOG_ON_NFS_IS_ERROR` defaults to ``False``.

:macro-def:`DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS`
    Allows any characters to be used in DAGMan node names, even
    characters that are considered illegal because they are used internally
    as separators. Turning this feature on could lead to instability when
    using splices or munged node names. The default value is ``False``.

:macro-def:`DAGMAN_ALLOW_EVENTS`
    An integer that controls which bad events are considered fatal
    errors by :tool:`condor_dagman`. This macro replaces and expands upon the
    functionality of the ``DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION``
    macro. If :macro:`DAGMAN_ALLOW_EVENTS` is set, it overrides the setting
    of ``DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION``. **Note: users should
    rarely change this setting.**

    The :macro:`DAGMAN_ALLOW_EVENTS` value is a logical bitwise OR of the
    following values:

        -  0 = allow no bad events
        -  1 = allow all bad events, except the event ``"job re-run after terminated event"``
        -  2 = allow terminated/aborted event combination
        -  4 = allow a ``"job re-run after terminated event"`` bug
        -  8 = allow garbage or orphan events
        - 16 = allow an execute or terminate event before job's submit event
        - 32 = allow two terminated events per job, as sometimes seen with grid jobs
        - 64 = allow duplicated events in general

    The default value is 114, which allows terminated/aborted event
    combination, allows an execute and/or terminated event before job's
    submit event, allows double terminated events, and allows general
    duplicate events.

    As examples, a value of 6 instructs :tool:`condor_dagman` to allow both
    the terminated/aborted event combination and the
    ``"job re-run after terminated event"`` bug. A value of 0 means that
    any bad event will be considered a fatal error.

    A value of 5 will never abort the DAG because of a bad event. But
    this value should almost never be used, because the
    ``"job re-run after terminated event"`` bug breaks the semantics of
    the DAG.

:index:`Debugging Log<single: DAGMan Configuration Sections; Debugging Log>`

Debug output
''''''''''''

:macro-def:`DAGMAN_DEBUG`
    This variable is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`DAGMAN_VERBOSITY`
    An integer value defining the verbosity of output to the
    ``dagman.out`` file, as follows (each level includes all output from
    lower debug levels):

    -  level = 0; never produce output, except for usage info
    -  level = 1; very quiet, output severe errors
    -  level = 2; output errors and warnings
    -  level = 3; normal output
    -  level = 4; internal debugging output
    -  level = 5; internal debugging output; outer loop debugging
    -  level = 6; internal debugging output; inner loop debugging
    -  level = 7; internal debugging output; rarely used

    The default value if not defined is 3.

:macro-def:`DAGMAN_DEBUG_CACHE_ENABLE`
    A boolean value that determines if log line caching for the
    ``dagman.out`` file should be enabled in the :tool:`condor_dagman`
    process to increase performance (potentially by orders of magnitude)
    when writing the ``dagman.out`` file to an NFS server. Currently,
    this cache is only utilized in Recovery Mode. If not defined, it
    defaults to ``False``.

:macro-def:`DAGMAN_DEBUG_CACHE_SIZE`
    An integer value representing the number of bytes of log lines to be
    stored in the log line cache. When the cache surpasses this number,
    the entries are written out in one call to the logging subsystem. A
    value of zero is not recommended since each log line would surpass
    the cache size and be emitted in addition to bracketing log lines
    explaining that the flushing was happening. The legal range of
    values is 0 to INT_MAX. If defined with a value less than 0, the
    value 0 will be used. If not defined, it defaults to 5 Megabytes.

:macro-def:`DAGMAN_PENDING_REPORT_INTERVAL`
    An integer value representing the number of seconds that controls
    how often :tool:`condor_dagman` will print a report of pending nodes to
    the ``dagman.out`` file. The report will only be printed if
    :tool:`condor_dagman` has been waiting at least
    :macro:`DAGMAN_PENDING_REPORT_INTERVAL` seconds without seeing any node
    job events, in order to avoid cluttering the ``dagman.out`` file.
    This feature is mainly intended to help diagnose :tool:`condor_dagman`
    processes that are stuck waiting indefinitely for a job to finish.
    If not defined, :macro:`DAGMAN_PENDING_REPORT_INTERVAL` defaults to 600
    seconds (10 minutes).

:macro-def:`DAGMAN_CHECK_QUEUE_INTERVAL`
    An integer value representing the number of seconds DAGMan will wait
    pending on nodes to make progress before querying the local *condor_schedd*
    queue to verify that the jobs the DAG is pending on are in said queue.
    If jobs are missing, DAGMan will write a rescue DAG and abort. When set
    to a value equal to or less than 0 DAGMan will not query the *condor_schedd*.
    Default value is 28800 seconds (8 Hours).

:macro-def:`DAGMAN_PRINT_JOB_TABLE_INTERVAL`
    An integer value representing the number of seconds DAGMan will wait
    before printing out the job state table to the debug log. If set to
    ``0`` then the table is never printed. This value defaults to 900
    seconds (15 minutes).

:macro-def:`MAX_DAGMAN_LOG`
    This variable is described in :macro:`MAX_<SUBSYS>_LOG`. If not defined,
    :macro:`MAX_DAGMAN_LOG` defaults to 0 (unlimited size).

:index:`HTCondor Attributes<single: DAGMan Configuration Sections; HTCondor Attributes>`

HTCondor attributes
-------------------

:macro-def:`DAGMAN_COPY_TO_SPOOL`
    A boolean value that when ``True`` copies the :tool:`condor_dagman`
    binary to the spool directory when a DAG is submitted. Setting this
    variable to ``True`` allows long-running DAGs to survive a DAGMan
    version upgrade. For running large numbers of small DAGs, leave this
    variable unset or set it to ``False``. The default value if not
    defined is ``False``. **Note: users should rarely change this
    setting.**

:macro-def:`DAGMAN_INSERT_SUB_FILE`
    A file name of a file containing submit description file commands to
    be inserted into the ``.condor.sub`` file created by
    :tool:`condor_submit_dag`. The specified file is inserted into the
    ``.condor.sub`` file before the
    :subcom:`queue[and DAGMAN_INSERT_SUB_FILE]` command and before
    any commands specified with the **-append** :tool:`condor_submit_dag`
    command line option. Note that the :macro:`DAGMAN_INSERT_SUB_FILE` value
    can be overridden by the :tool:`condor_submit_dag`
    **-insert_sub_file** command line option.

:macro-def:`DAGMAN_ON_EXIT_REMOVE`
    Defines the ``OnExitRemove`` ClassAd expression placed into the
    :tool:`condor_dagman` submit description file by :tool:`condor_submit_dag`.
    The default expression is designed to ensure that :tool:`condor_dagman`
    is automatically re-queued by the *condor_schedd* daemon if it
    exits abnormally or is killed (for example, during a reboot). If
    this results in :tool:`condor_dagman` staying in the queue when it should
    exit, consider changing to a less restrictive expression, as in the
    example

    .. code-block:: condor-classad-expr

          (ExitBySignal == false || ExitSignal =!= 9)

    If not defined, :macro:`DAGMAN_ON_EXIT_REMOVE` defaults to the expression

    .. code-block:: condor-classad-expr

          ( ExitSignal =?= 11 || (ExitCode =!= UNDEFINED && ExitCode >=0 && ExitCode <= 2))

:macro-def:`DAGMAN_INHERIT_ATTRS`
    A list of job ClassAd attributes from the root DAGMan job's ClassAd to be
    passed down to all managed jobs including SubDAGs. Empty by default.

:macro-def:`DAGMAN_INHERIT_ATTRS_PREFIX`
    A string to be prefixed to the job ClassAd attributes described in
    :macro:`DAGMAN_INHERIT_ATTRS`. Empty by default.
