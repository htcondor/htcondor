:index:`Gangliad Options<single: Configuration; Gangliad Options>`

.. _gangliad_config_options:

Gangliad Configuration Options
==============================

*condor_gangliad* is an optional daemon responsible for publishing
information about HTCondor daemons to the Ganglia\ :sup:`™` monitoring
system. The Ganglia monitoring system must be installed and configured
separately. In the typical case, a single instance of the
*condor_gangliad* daemon is run per pool. A default set of metrics are
sent. Additional metrics may be defined, in order to publish any
information available in ClassAds that the *condor_collector* daemon
has.

:macro-def:`GANGLIAD_INTERVAL`
    The integer number of seconds between consecutive sending of metrics
    to Ganglia. Daemons update the *condor_collector* every 300
    seconds, and the Ganglia heartbeat interval is 20 seconds.
    Therefore, multiples of 20 between 20 and 300 makes sense for this
    value. Negative values inhibit sending data to Ganglia. The default
    value is 60.

:macro-def:`GANGLIAD_MIN_METRIC_LIFETIME`
    An integer value representing the minimum DMAX value for all metrics.
    Where DMAX is the number number of seconds without updating that
    a metric will be kept before deletion. This value defaults to ``86400``
    which is equivalent to 1 day. This value will be overridden by a
    specific metric defined ``Lifetime`` value.

:macro-def:`GANGLIAD_VERBOSITY`
    An integer that specifies the maximum verbosity level of metrics to
    be published to Ganglia. Basic metrics have a verbosity level of 0,
    which is the default. Additional metrics can be enabled by
    increasing the verbosity to 1. In the default configuration, there
    are no metrics with verbosity levels higher than 1. Some metrics
    depend on attributes that are not published to the *condor_collector*
    when using the default value of :macro:`STATISTICS_TO_PUBLISH`. For
    example, per-user file transfer statistics will only be published to
    Ganglia if :macro:`GANGLIAD_VERBOSITY` is set to 1 or higher in the
    *condor_gangliad* configuration and :macro:`STATISTICS_TO_PUBLISH` in
    the *condor_schedd* configuration contains ``TRANSFER:2``, or if
    the :macro:`STATISTICS_TO_PUBLISH_LIST` contains the desired attributes
    explicitly.

:macro-def:`GANGLIAD_REQUIREMENTS`
    An optional boolean ClassAd expression that may restrict the set of
    daemon ClassAds to be monitored. This could be used to monitor a
    subset of a pool's daemons or machines. The default is an empty
    expression, which has the effect of placing no restriction on the
    monitored ClassAds. Keep in mind that this expression is applied to
    all types of monitored ClassAds, not just machine ClassAds.

:macro-def:`GANGLIAD_PER_EXECUTE_NODE_METRICS`
    A boolean value that, when ``False``, causes metrics from execute
    node daemons to not be published. Aggregate values from these
    machines will still be published. The default value is ``True``.
    This option is useful for pools such that use glidein, in which it
    is not desired to record metrics for individual execute nodes.

:macro-def:`MONITOR_MULTIPLE_COLLECTORS`
    An optional comma-separated list of HTCondor collectors to monitor,
    specified as ``name/address`` pairs. Each entry consists of a descriptive
    name for the collector followed by a slash and the collector's address
    (as would be passed to the ``-pool`` option of :tool:`condor_status`).
    When set, *condor_gangliad* will query each of the specified collectors
    instead of the default pool collector. For example:
    
    .. code-block:: condor-config
    
        MONITOR_MULTIPLE_COLLECTORS = ce1.wisc.edu/ce1.wisc.edu:9619, ce2.wisc.edu/ce2.wisc.edu:9619
    
    When monitoring multiple collectors, aggregate metrics are automatically
    grouped by collector name to distinguish metrics from different pools.
    If a collector becomes unresponsive, it will be temporarily skipped and
    retried after 30 minutes. This configuration parameter has no default value.

:macro-def:`MONITOR_COLLECTOR`
    An optional collector address to query for dynamically generating the list
    of collectors to monitor. When set, *condor_gangliad* will query this
    collector for daemon ads matching the criteria specified by
    :macro:`MONITOR_COLLECTOR_CONSTRAINT` and :macro:`MONITOR_COLLECTOR_AD_TYPE`,
    and automatically populate :macro:`MONITOR_MULTIPLE_COLLECTORS` with the
    results. The list of collectors is refreshed every 30 minutes.
    This is useful when you need to monitor a dynamic set of collectors
    that may change over time. This configuration parameter has no default value.

:macro-def:`MONITOR_COLLECTOR_NAME_ATTR`
    When using :macro:`MONITOR_COLLECTOR`, this specifies the ClassAd attribute
    name to use for the collector's name. The default value is ``Name``.

:macro-def:`MONITOR_COLLECTOR_ADDR_ATTR`
    When using :macro:`MONITOR_COLLECTOR`, this specifies the ClassAd attribute
    name to use for the collector's address. The default value is ``CollectorHost``.

:macro-def:`MONITOR_COLLECTOR_CONSTRAINT`
    When using :macro:`MONITOR_COLLECTOR`, this specifies a ClassAd constraint
    expression to filter which daemon ads should be considered as collectors to
    monitor. The default value is ``True`` (no filtering).

:macro-def:`MONITOR_COLLECTOR_AD_TYPE`
    When using :macro:`MONITOR_COLLECTOR`, this specifies the type of daemon ads
    to query. Valid values are standard HTCondor ad types such as ``Schedd``,
    ``Startd``, ``Collector``, etc. The default value is ``Schedd``.

:macro-def:`GANGLIAD_WANT_PROJECTION`
    A boolean value that, when ``True``, causes the *condor_gangliad* to
    use an attribute projection when querying the collector whenever possible.
    This significantly reduces the memory consumption of the *condor_gangliad*, and also
    places less load on the *condor_collector*.
    The default value is currently ``False``; it is expected this default will
    be changed to ``True`` in a future release after additional testing.

:macro-def:`GANGLIAD_WANT_RESET_METRICS`
    A boolean value that, when ``True``, causes aggregate numeric metrics
    to be reset to a value of zero when they are no longer being updated.
    The default value is ``False``, causing aggregate metrics published to
    Ganglia to retain the last value published indefinitely.

:macro-def:`GANGLIAD_RESET_METRICS_FILE`
    The file name where persistent data will
    be stored if ``GANGLIAD_WANT_RESET_METRICS`` is set to ``True``. 
    If not set to a fully qualified path, the file will be stored in the 
    SPOOL directory with a filename extension of ``.ganglia_metrics``.
    If you are running multiple *condor_gangliad* instances
    that share a SPOOL directory, this knob should be customized. 
    The default is ``$(SPOOL)/metricsToReset.ganglia_metrics``.

:macro-def:`GANGLIA_CONFIG`
    The path and file name of the Ganglia configuration file. The
    default is ``/etc/ganglia/gmond.conf``.

:macro-def:`GANGLIA_GMETRIC`
    The full path of the *gmetric* executable to use. If none is
    specified, ``libganglia`` will be used instead when possible,
    because the library interface is more efficient than invoking
    *gmetric*. Some versions of ``libganglia`` are not compatible. When
    a failure to use ``libganglia`` is detected, *gmetric* will be used,
    if *gmetric* can be found in HTCondor's ``PATH`` environment
    variable.

:macro-def:`GANGLIA_GSTAT_COMMAND`
    The full *gstat* command used to determine which hosts are monitored
    by Ganglia. For a *condor_gangliad* running on a host whose local
    *gmond* does not know the list of monitored hosts, change
    ``localhost`` to be the appropriate host name or IP address within
    this default string:

    .. code-block:: console

         $ gstat --all --mpifile --gmond_ip=localhost --gmond_port=8649

:macro-def:`GANGLIA_SEND_DATA_FOR_ALL_HOSTS`
    A boolean value that when ``True`` causes data to be sent to Ganglia
    for hosts that it is not currently monitoring. The default is
    ``False``.

:macro-def:`GANGLIA_LIB`
    The full path and file name of the ``libganglia`` shared library to
    use. If none is specified, and if configuration variable
    :macro:`GANGLIA_GMETRIC` is also not specified, then a search for
    ``libganglia`` will be performed in the directories listed in
    configuration variable :macro:`GANGLIA_LIB_PATH` or
    :macro:`GANGLIA_LIB64_PATH`. The special value ``NOOP``
    indicates that *condor_gangliad* should not publish statistics to
    Ganglia, but should otherwise go through all the motions it normally
    does.

:macro-def:`GANGLIA_LIB_PATH`
    A comma-separated list of directories within which to search for the
    ``libganglia`` executable, if :macro:`GANGLIA_LIB` is not configured.
    This is used in 32-bit versions of HTCondor.

:macro-def:`GANGLIA_LIB64_PATH`
    A comma-separated list of directories within which to search for the
    ``libganglia`` executable, if :macro:`GANGLIA_LIB` is not configured.
    This is used in 64-bit versions of HTCondor.

:macro-def:`GANGLIAD_DEFAULT_CLUSTER`
    An expression specifying the default name of the Ganglia cluster for
    all metrics. The expression may refer to attributes of the machine.

:macro-def:`GANGLIAD_DEFAULT_MACHINE`
    An expression specifying the default machine name of Ganglia
    metrics. The expression may refer to attributes of the machine.

:macro-def:`GANGLIAD_DEFAULT_IP`
    An expression specifying the default IP address of Ganglia metrics.
    The expression may refer to attributes of the machine.

:macro-def:`GANGLIAD_LOG`
    The path and file name of the *condor_gangliad* daemon's log file.
    The default log is ``$(LOG)/GangliadLog``.

:macro-def:`GANGLIAD_METRICS_CONFIG_DIR`
    Path to the directory containing files which define Ganglia metrics
    in terms of HTCondor ClassAd attributes to be published. All files
    in this directory are read, to define the metrics. The default
    directory ``/etc/condor/ganglia.d/`` is used when not specified.
