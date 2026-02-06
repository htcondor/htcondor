.. _hooks_config_options:

HTCondor Hooks Configuration Options
====================================

:index:`Job Hook Options<single: Configuration; Job Hook Options>`

.. _job_hook_config_options:

Job Hook Configuration Options
------------------------------

These macros control the various hooks that interact with HTCondor.
Currently, there are two independent sets of hooks. One is a set of
fetch work hooks, some of which are invoked by the *condor_startd* to
optionally fetch work, and some are invoked by the *condor_starter*.
See :ref:`admin-manual/ep-policy-configuration:job hooks that fetch work` for more
details. The other set replace functionality of the
*condor_job_router* daemon. Documentation for the
*condor_job_router* daemon is in
:doc:`/grid-computing/job-router`.

:macro-def:`SLOT<N>_JOB_HOOK_KEYWORD`
    For the fetch work hooks, the keyword used to define which set of
    hooks a particular compute slot should invoke. The value of <N> is
    replaced by the slot identification number. For example, on slot 1,
    the variable name will be called ``SLOT1_JOB_HOOK_KEYWORD``. There
    is no default keyword. Sites that wish to use these job hooks must
    explicitly define the keyword and the corresponding hook paths.

:macro-def:`STARTD_JOB_HOOK_KEYWORD`
    For the fetch work hooks, the keyword used to define which set of
    hooks a particular *condor_startd* should invoke. This setting is
    only used if a slot-specific keyword is not defined for a given
    compute slot. There is no default keyword. Sites that wish to use
    job hooks must explicitly define the keyword and the corresponding
    hook paths.

:macro-def:`STARTER_DEFAULT_JOB_HOOK_KEYWORD`
    A string valued parameter that defaults to empty.  If the job
    does not define a hook, or defines an invalid one, this
    can be used to force a default hook for the job.

:macro-def:`STARTER_JOB_HOOK_KEYWORD`
    This can be defined to force the *condor_starter* to always use a 
    given keyword for its own hooks, regardless of the value in the 
    job ClassAd for the :ad-attr:`HookKeyword` attribute.¬

:macro-def:`<Keyword>_HOOK_FETCH_WORK`
    For the fetch work hooks, the full path to the program to invoke
    whenever the *condor_startd* wants to fetch work. ``<Keyword>`` is
    the hook keyword defined to distinguish between sets of hooks. There
    is no default.

:macro-def:`<Keyword>_HOOK_REPLY_FETCH`
    For the fetch work hooks, the full path to the program to invoke
    when the hook defined by :macro:`<Keyword>_HOOK_FETCH_WORK` returns data
    and then the *condor_startd* decides if it is going to accept the
    fetched job or not. ``<Keyword>`` is the hook keyword defined to
    distinguish between sets of hooks.

:macro-def:`<Keyword>_HOOK_REPLY_CLAIM`
    For the fetch work hooks, the full path to the program to invoke
    whenever the *condor_startd* finishes fetching a job and decides
    what to do with it. ``<Keyword>`` is the hook keyword defined to
    distinguish between sets of hooks. There is no default.

:macro-def:`<Keyword>_HOOK_PREPARE_JOB`
    For the fetch work hooks, the full path to the program invoked by
    the *condor_starter* before it runs the job. ``<Keyword>`` is the
    hook keyword defined to distinguish between sets of hooks.

:macro-def:`<Keyword>_HOOK_PREPARE_JOB_ARGS`
    The arguments for the *condor_starter* to use when invoking the
    prepare job hook specified by ``<Keyword>``.

:macro-def:`<Keyword>_HOOK_UPDATE_JOB_INFO`
    This configuration variable is used by both fetch work hooks and by
    *condor_job_router* hooks.

    For the fetch work hooks, the full path to the program invoked by
    the *condor_starter* periodically as the job runs, allowing the
    *condor_starter* to present an updated and augmented job ClassAd to
    the program. See :ref:`admin-manual/ep-policy-configuration:job hooks that fetch work` for
    the list of additional attributes included. When the job is first invoked,
    the *condor_starter* will invoke the program after
    ``$(STARTER_INITIAL_UPDATE_INTERVAL)`` seconds. Thereafter, the
    *condor_starter* will invoke the program every
    ``$(STARTER_UPDATE_INTERVAL)`` seconds. ``<Keyword>`` is the hook
    keyword defined to distinguish between sets of hooks.

    As a Job Router hook, the full path to the program invoked when the
    Job Router polls the status of routed jobs at intervals set by
    :macro:`JOB_ROUTER_POLLING_PERIOD`. ``<Keyword>`` is the hook keyword
    defined by :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the hooks.

:macro-def:`<Keyword>_HOOK_EVICT_CLAIM`
    For the fetch work hooks, the full path to the program to invoke
    whenever the *condor_startd* needs to evict a fetched claim.
    ``<Keyword>`` is the hook keyword defined to distinguish between
    sets of hooks. There is no default.

:macro-def:`<Keyword>_HOOK_JOB_EXIT`
    For the fetch work hooks, the full path to the program invoked by
    the *condor_starter* whenever a job exits, either on its own or
    when being evicted from an execution slot. ``<Keyword>`` is the hook
    keyword defined to distinguish between sets of hooks.

:macro-def:`<Keyword>_HOOK_JOB_EXIT_TIMEOUT`
    For the fetch work hooks, the number of seconds the
    *condor_starter* will wait for the hook defined by
    :macro:`<Keyword>_HOOK_JOB_EXIT` hook to exit, before continuing with job
    clean up. Defaults to 30 seconds. ``<Keyword>`` is the hook keyword
    defined to distinguish between sets of hooks.

:macro-def:`FetchWorkDelay`
    An expression that defines the number of seconds that the
    *condor_startd* should wait after an invocation of
    :macro:`<Keyword>_HOOK_FETCH_WORK` completes before the hook
    should be invoked again. The expression is evaluated in the context
    of the slot ClassAd, and the ClassAd of the currently running job
    (if any). The expression must evaluate to an integer. If not
    defined, the *condor_startd* will wait 300 seconds (five minutes)
    between attempts to fetch work. For more information about this
    expression, see :ref:`admin-manual/ep-policy-configuration:job hooks that fetch work`.

:macro-def:`JOB_ROUTER_HOOK_KEYWORD`
    For the Job Router hooks, the keyword used to define the set of
    hooks the *condor_job_router* is to invoke to replace
    functionality of routing translation. There is no default keyword.
    Use of these hooks requires the explicit definition of the keyword
    and the corresponding hook paths.

:macro-def:`<Keyword>_HOOK_TRANSLATE_JOB`
    A Job Router hook, the full path to the program invoked when the Job
    Router has determined that a job meets the definition for a route.
    This hook is responsible for doing the transformation of the job.
    ``<Keyword>`` is the hook keyword defined by
    :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the hooks.

:macro-def:`<Keyword>_HOOK_JOB_FINALIZE`
    A Job Router hook, the full path to the program invoked when the Job
    Router has determined that the job completed. ``<Keyword>`` is the
    hook keyword defined by :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the
    hooks.

:macro-def:`<Keyword>_HOOK_JOB_CLEANUP`
    A Job Router hook, the full path to the program invoked when the Job
    Router finishes managing the job. ``<Keyword>`` is the hook keyword
    defined by :macro:`JOB_ROUTER_HOOK_KEYWORD` to identify the hooks.

:index:`Daemon Hook Options<single: Configuration; Daemon Hook Options>`
:index:`Startd Cron Options<single: Configuration; Startd Cron Options>`
:index:`Schedd Cron Options<single: Configuration; Schedd Cron Options>`

.. _daemon_hooks_config_options:

Daemon Hook Configuration Options
---------------------------------

The following macros describe the daemon ClassAd hooks which run
startd cron and schedd cron.  These run executables or scripts
directly from the *condor_startd* and *condor_schedd*
daemons.  The output is merged into the 
ClassAd generated by the respective daemon.  The mechanism is described
in :ref:`admin-manual/ep-policy-configuration:Startd Cron`.

These macros all include ``CRON`` because the default mode for a daemon 
ClassAd hook is to run periodically.  A specific daemon ClassAd 
hook is called a ``JOB``.

To define a job:

* Add a ``JobName`` to :macro:`STARTD_CRON_JOBLIST`.  (If
  you want to define a benchmark, or a daemon ClassAd hook in the schedd,
  use ``BENCHMARK`` or ``SCHEDD`` in the macro name instead.)  A
  ``JobName`` identifies a specific job and must be unique.  In the rest of
  this section, where ``<JobName>`` appears in a macro name, it means to
  replace ``<JobName>`` with one of the names :macro:`STARTD_CRON_JOBLIST`.

* You must set :macro:`STARTD_CRON_<JobName>_EXECUTABLE`,
  and you'll probably want to set :macro:`STARTD_CRON_<JobName>_ARGS` as well.
  These macros tell HTCondor how to actually run the job.

* You must also decide when your job will run.  By default, a job runs
  every :macro:`STARTD_CRON_<JobName>_PERIOD` seconds after the daemon
  starts up.  You may set :macro:`STARTD_CRON_<JobName>_MODE` to change
  to this to continuously (``WaitForExit``); on start-up (``OneShot``)
  and optionally, when the daemon is reconfigured; or as a benchmark
  (``OnDemand``).  If you do not select ``OneShot``, you must set
  :macro:`STARTD_CRON_<JobName>_PERIOD`.

All the other job-specific macros are optional, of which
:macro:`STARTD_CRON_<JobName>_ENV` and :macro:`STARTD_CRON_<JobName>_CWD`
are probably the most common.

:macro-def:`STARTD_CRON_AUTOPUBLISH`
    Optional setting that determines if the *condor_startd* should
    automatically publish a new update to the *condor_collector* after
    any of the jobs produce output. Beware that enabling this setting
    can greatly increase the network traffic in an HTCondor pool,
    especially when many modules are executed, or if the period in which
    they run is short. There are three possible (case insensitive)
    values for this variable:

     ``Never``
        This default value causes the *condor_startd* to not
        automatically publish updates based on any jobs. Instead,
        updates rely on the usual behavior for sending updates, which is
        periodic, based on the :macro:`UPDATE_INTERVAL` configuration
        variable, or whenever a given slot changes state.
     ``Always``
        Causes the *condor_startd* to always send a new update to the
        *condor_collector* whenever any job exits.
     ``If_Changed``
        Causes the *condor_startd* to only send a new update to the
        *condor_collector* if the output produced by a given job is
        different than the previous output of the same job. The only
        exception is the ``LastUpdate`` attribute, which is
        automatically set for all jobs to be the timestamp when the job
        last ran. It is ignored when :macro:`STARTD_CRON_AUTOPUBLISH` is set
        to ``If_Changed``.

:macro-def:`STARTD_CRON_CONFIG_VAL`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_CONFIG_VAL`
    .. faux-definition::

:macro-def:`BENCHMARKS_CONFIG_VAL`
    This configuration variable can be used to specify the path and
    executable name of the :tool:`condor_config_val` program which the jobs
    (hooks) should use to get configuration information from the daemon.
    If defined, an environment variable by the same name with the same
    value will be passed to all jobs.

:macro-def:`STARTD_CRON_JOBLIST`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_JOBLIST`
    .. faux-definition::

:macro-def:`BENCHMARKS_JOBLIST`
    These configuration variables are defined by a comma and/or white
    space separated list of job names to run. Each is the logical name
    of a job. This name must be unique; no two jobs may have the same
    name. The *condor_startd* reads this configuration variable on startup
    and on reconfig.  The *condor_schedd* reads this variable and other
    ``SCHEDD_CRON_*`` variables only on startup.

:macro-def:`STARTD_CRON_MAX_JOB_LOAD`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_MAX_JOB_LOAD`
    .. faux-definition::

:macro-def:`BENCHMARKS_MAX_JOB_LOAD`
    A floating point value representing a threshold for CPU load, such
    that if starting another job would cause the sum of assumed loads
    for all running jobs to exceed this value, no further jobs will be
    started. The default value for ``STARTD_CRON`` or a ``SCHEDD_CRON``
    hook managers is 0.1. This implies that a maximum of 10 jobs (using
    their default, assumed load) could be concurrently running. The
    default value for the ``BENCHMARKS`` hook manager is 1.0. This
    implies that only 1 ``BENCHMARKS`` job (at the default, assumed
    load) may be running.

:macro-def:`STARTD_CRON_LOG_NON_ZERO_EXIT`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_LOG_NON_ZERO_EXIT`
    If true, each time a cron job returns a non-zero exit code, the
    corresponding daemon will log the cron job's exit code and output.  There
    is no default value, so no logging will occur by default.

:macro-def:`STARTD_CRON_<JobName>_ARGS`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_ARGS`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_ARGS`
    The command line arguments to pass to the job as it is invoked. The
    first argument will be ``<JobName>``.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_CONDITION`
    A ClassAd expression evaluated each time the job might otherwise
    be started.  If this macro is set, but the expression does not evaluate to
    ``True``, the job will not be started.  The expression is evaluated in
    a context similar to a slot ad, but without any slot-specific attributes.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`.

:macro-def:`STARTD_CRON_<JobName>_CWD`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_CWD`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_CWD`
    The working directory in which to start the job.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_ENV`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_ENV`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_ENV`
    The environment string to pass to the job. The syntax is the same as
    that of :macro:`<DaemonName>_ENVIRONMENT` as defined at
    :ref:`master_config_options`.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_EXECUTABLE`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_EXECUTABLE`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_EXECUTABLE`
    The full path and executable to run for this job. Note that multiple
    jobs may specify the same executable, although the jobs need to have
    different logical names.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_JOB_LOAD`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_JOB_LOAD`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_JOB_LOAD`
    A floating point value that represents the assumed and therefore
    expected CPU load that a job induces on the system. This job load is
    then used to limit the total number of jobs that run concurrently,
    by not starting new jobs if the assumed total load from all jobs is
    over a set threshold. The default value for each individual
    ``STARTD_CRON`` or a ``SCHEDD_CRON`` job is 0.01. The default value
    for each individual ``BENCHMARKS`` job is 1.0.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_KILL`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_KILL`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_KILL`
    A boolean value applicable only for jobs with a ``MODE`` of anything
    other than ``WaitForExit``. The default value is ``False``.

    This variable controls the behavior of the daemon hook manager when
    it detects that an instance of the job's executable is still running
    as it is time to invoke the job again. If ``True``, the daemon hook
    manager will kill the currently running job and then invoke an new
    instance of the job. If ``False``, the existing job invocation is
    allowed to continue running.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_METRICS`
    A space or comma -separated list. Each element in the list is a
    metric type, either ``SUM`` or ``PEAK``; a colon; and a metric name.

    An attribute preceded by ``SUM`` is a metric which accumulates over
    time. The canonical example is seconds of CPU usage.

    An attribute preceded by ``PEAK`` is a metric which instead records
    the largest value reported over the period of use. The canonical
    example is megabytes of memory usage.

    A job with :macro:`STARTD_CRON_<JobName>_METRICS` set is a custom machine
    resource monitor (CMRM), and its output is handled differently than
    a normal job's. A CMRM should output one ad per custom machine
    resource instance and use ``SlotMergeConstraint``\ s (see
    :ref:`admin-manual/ep-policy-configuration:Startd Cron`) to specify the instance to
    which it applies.

    The ad corresponding to each custom machine resource instance should
    have an attribute for each metric named in the configuration. For
    SUM metrics, the attribute should be ``Uptime<MetricName>Seconds``;
    for PEAK metrics, the attribute should be
    ``Uptime<MetricName>PeakUsage``.

    Each value should be the value of the metric since the last time the
    job reported. The reported value may therefore go up or down;
    HTCondor will record either the sum or the peak value, as
    appropriate, for the duration of the job running in a slot assigned
    resources of the corresponding type.

    For example, if your custom resources are SQUIDs, and you detected
    four of them, your monitor might output the following:

    .. code-block:: text

        SlotMergeConstraint = StringListMember( "SQUID0", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 5.0
        UptimeSQUIDsMemoryPeakUsage = 50
        - SQUIDsReport0
        SlotMergeConstraint = StringListMember( "SQUID1", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 1.0
        UptimeSQUIDsMemoryPeakUsage = 10
        - SQUIDsReport1
        SlotMergeConstraint = StringListMember( "SQUID2", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 9.0
        UptimeSQUIDsMemoryPeakUsage = 90
        - SQUIDsReport2
        SlotMergeConstraint = StringListMember( "SQUID3", AssignedSQUIDs )
        UptimeSQUIDsSeconds = 4.0
        UptimeSQUIDsMemoryPeakUsage = 40
        - SQUIDsReport3

    The names ('SQUIDsReport0') may be anything, but must be consistent
    from report to report and the ClassAd for each report must have a
    distinct name.

    You might specify the monitor in the example above as follows:

    .. code-block:: condor-config

        MACHINE_RESOURCE_INVENTORY_SQUIDs = /usr/local/bin/cmr-squid-discovery

        STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) SQUIDs_MONITOR
        STARTD_CRON_SQUIDs_MONITOR_MODE = Periodic
        STARTD_CRON_SQUIDs_MONITOR_PERIOD = 10
        STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE = /usr/local/bin/cmr-squid-monitor
        STARTD_CRON_SQUIDs_MONITOR_METRICS = SUM:SQUIDs, PEAK:SQUIDsMemory

:macro-def:`STARTD_CRON_<JobName>_MODE`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_MODE`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_MODE`
    A string that specifies a mode within which the job operates. Legal
    values are

    -  ``Periodic``, which is the default.
    -  ``WaitForExit``
    -  ``OneShot``
    -  ``OnDemand``

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

    The default ``Periodic`` mode is used for most jobs. In this mode,
    the job is expected to be started by the *condor_startd* daemon,
    gather and publish its data, and then exit.

    In ``WaitForExit`` mode the *condor_startd* daemon interprets the
    period as defined by :macro:`STARTD_CRON_<JobName>_PERIOD` differently.
    In this case, it refers to the amount of time to wait after the job
    exits before restarting it. With a value of 1, the job is kept
    running nearly continuously. In general, ``WaitForExit`` mode is for
    jobs that produce a periodic stream of updated data, but it can be
    used for other purposes, as well. The output data from the job is
    accumulated into a temporary ClassAd until the job exits or until it
    writes a line starting with dash (-) character. At that point, the
    temporary ClassAd replaces the active ClassAd for the job. The
    active ClassAd for the job is merged into the appropriate slot
    ClassAds whenever the slot ClassAds are published.

    The ``OneShot`` mode is used for jobs that are run once at the start
    of the daemon. If the ``reconfig_rerun`` option is specified, the
    job will be run again after any reconfiguration.

    The ``OnDemand`` mode is used only by the ``BENCHMARKS`` mechanism.
    All benchmark jobs must be ``OnDemand`` jobs. Any other jobs
    specified as ``OnDemand`` will never run. Additional future features
    may allow for other ``OnDemand`` job uses.

:macro-def:`STARTD_CRON_<JobName>_PERIOD`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_PERIOD`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_PERIOD`
    The period specifies time intervals at which the job should be run.
    For periodic jobs, this is the time interval that passes between
    starting the execution of the job. The value may be specified in
    seconds, minutes, or hours. Specify this time by appending the
    character ``s``, ``m``, or ``h`` to the value. As an example, 5m
    starts the execution of the job every five minutes. If no character
    is appended to the value, seconds are used as a default. In
    ``WaitForExit`` mode, the value has a different meaning: the period
    specifies the length of time after the job ceases execution and
    before it is restarted. The minimum valid value of the period is 1
    second.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_PREFIX`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_PREFIX`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_PREFIX`
    Specifies a string which is prepended by HTCondor to all attribute
    names that the job generates. The use of prefixes avoids the
    conflicts that would be caused by attributes of the same name
    generated and utilized by different jobs. For example, if a module
    prefix is ``xyz_``, and an individual attribute is named ``abc``, then the
    resulting attribute name will be ``xyz_abc``. Due to restrictions on
    ClassAd names, a prefix is only permitted to contain alpha-numeric
    characters and the underscore character.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST`,
    ``SCHEDD_CRON_JOBLIST``, or ``BENCHMARKS_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_RECONFIG`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_RECONFIG`
    A boolean value that when ``True``, causes the daemon to send an HUP
    signal to the job when the daemon is reconfigured. The job is
    expected to reread its configuration at that time.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST` or
    ``SCHEDD_CRON_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_RECONFIG_RERUN`
    .. faux-definition::

:macro-def:`SCHEDD_CRON_<JobName>_RECONFIG_RERUN`
    A boolean value that when ``True``, causes the daemon ClassAd hook
    mechanism to re-run the specified job when the daemon is
    reconfigured via :tool:`condor_reconfig`. The default value is ``False``.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST` or
    ``SCHEDD_CRON_JOBLIST``.

:macro-def:`STARTD_CRON_<JobName>_SLOTS`
    .. faux-definition::

:macro-def:`BENCHMARKS_<JobName>_SLOTS`
    Only the slots specified in this comma-separated list may
    incorporate the output of the job specified by ``<JobName>``. If the
    list is not specified, any slot may. Whether or not a specific slot
    actually incorporates the output depends on the output; see
    :ref:`admin-manual/ep-policy-configuration:Startd Cron`.

    ``<JobName>`` is the logical name assigned for a job as defined by
    configuration variable :macro:`STARTD_CRON_JOBLIST` or
    ``BENCHMARKS_JOBLIST``.
