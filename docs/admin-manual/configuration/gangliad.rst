:index:`Gangliad Options<single: Configuration; Gangliad Options>`

.. _gangliad_config_options:

Metricd / Gangliad Configuration Options
========================================

*condor_metricd* is an optional daemon responsible for collecting metrics
from HTCondor daemons (via the *condor_collector*) and publishing them
to one or more monitoring backends. It currently supports two backends:
the Ganglia\ :sup:`™` monitoring system and Prometheus text-format file
export.

For backwards compatibility, the *condor_metricd* binary is also installed
under the name *condor_gangliad*. When invoked as *condor_gangliad*, the
daemon runs in legacy mode and behaves exactly as in previous releases:
it reads only the ``GANGLIAD_*`` knobs documented below and publishes only
to Ganglia. Administrators who are not adopting Prometheus need not make
any configuration changes.

When invoked as *condor_metricd* the daemon reads ``METRICD_*`` knobs for
engine-level settings, ``GANGLIA_*`` knobs for the Ganglia backend, and
``PROMETHEUS_*`` knobs for the Prometheus backend. In this mode a single
collector query is dispatched to both backends.

Engine Knobs
------------

These knobs control the metric-collection engine itself. The
``METRICD_*`` form is used in *condor_metricd* mode; the ``GANGLIAD_*``
form is used when the binary is invoked as *condor_gangliad*. There is
no fallback between the two forms — each mode reads only its own knobs.

:macro-def:`METRICD_INTERVAL` / :macro-def:`GANGLIAD_INTERVAL`
    The integer number of seconds between consecutive sending of metrics.
    Daemons update the *condor_collector* every 300 seconds, and the
    Ganglia heartbeat interval is 20 seconds. Therefore, multiples of 20
    between 20 and 300 makes sense for this value. Negative values
    inhibit publication. The default value is 60. Use ``METRICD_INTERVAL``
    in metricd mode, ``GANGLIAD_INTERVAL`` in legacy gangliad mode.

:macro-def:`GANGLIAD_MIN_METRIC_LIFETIME`
    An integer value representing the minimum DMAX value for all metrics.
    Where DMAX is the number number of seconds without updating that
    a metric will be kept before deletion. This value defaults to ``86400``
    which is equivalent to 1 day. This value will be overridden by a
    specific metric defined ``Lifetime`` value.

:macro-def:`METRICD_VERBOSITY` / :macro-def:`GANGLIAD_VERBOSITY`
    An integer that specifies the maximum verbosity level of metrics to
    be published. Basic metrics have a verbosity level of 0, which is the
    default. Additional metrics can be enabled by increasing the verbosity
    to 1. In the default configuration, there are no metrics with
    verbosity levels higher than 1. Some metrics depend on attributes that
    are not published to the *condor_collector* when using the default
    value of :macro:`STATISTICS_TO_PUBLISH`. For example, per-user file
    transfer statistics will only be published if the verbosity is set
    to 1 or higher and :macro:`STATISTICS_TO_PUBLISH` in the
    *condor_schedd* configuration contains ``TRANSFER:2``, or if
    :macro:`STATISTICS_TO_PUBLISH_LIST` contains the desired attributes
    explicitly.

:macro-def:`METRICD_REQUIREMENTS` / :macro-def:`GANGLIAD_REQUIREMENTS`
    An optional boolean ClassAd expression that may restrict the set of
    daemon ClassAds to be monitored. This could be used to monitor a
    subset of a pool's daemons or machines. The default is an empty
    expression, which has the effect of placing no restriction on the
    monitored ClassAds. Keep in mind that this expression is applied to
    all types of monitored ClassAds, not just machine ClassAds.

:macro-def:`METRICD_PER_EXECUTE_NODE_METRICS` / :macro-def:`GANGLIAD_PER_EXECUTE_NODE_METRICS`
    A boolean value that, when ``False``, causes metrics from execute
    node daemons to not be published. Aggregate values from these
    machines will still be published. The default value is ``True``.
    This option is useful for pools such that use glidein, in which it
    is not desired to record metrics for individual execute nodes.

:macro-def:`METRICD_LOG` / :macro-def:`GANGLIAD_LOG`
    The path and file name of the daemon's log file. In metricd mode the
    default is ``$(LOG)/MetricdLog``; in legacy gangliad mode it is
    ``$(LOG)/GangliadLog``.

:macro-def:`METRICD_METRICS_CONFIG_DIR` / :macro-def:`GANGLIAD_METRICS_CONFIG_DIR`
    Path to the directory containing files which define metrics in terms
    of HTCondor ClassAd attributes to be published. All files in this
    directory are read to define the metrics. The default directory is
    ``/etc/condor/ganglia.d/``.

:macro-def:`METRICD_WANT_PROJECTION` / :macro-def:`GANGLIAD_WANT_PROJECTION`
    A boolean value that, when ``True``, causes the daemon to use an
    attribute projection when querying the collector whenever possible.
    This significantly reduces memory consumption and also places less
    load on the *condor_collector*. The default value is currently
    ``False``.

:macro-def:`METRICD_DEFAULT_EXPORT_METRIC`
    Modern-mode only. A string that supplies a default value for the
    ``ExportMetric`` keyword for every metric definition that does not
    set its own value. The string is a comma-separated list of backend
    names such as ``"ganglia"``, ``"prometheus"``, or
    ``"ganglia, prometheus"``. An empty value (the default) means every
    metric is exported to every enabled backend.

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

Ganglia Backend Knobs
---------------------

These knobs configure the Ganglia backend. They are used in both
*condor_metricd* and *condor_gangliad* modes unless otherwise noted.

:macro-def:`GANGLIA_WANT_RESET_METRICS` / :macro-def:`GANGLIAD_WANT_RESET_METRICS`
    A boolean value that, when ``True``, causes aggregate numeric metrics
    to be reset to a value of zero when they are no longer being updated.
    Otherwise, aggregate metrics published to Ganglia retain the last
    value published indefinitely. In metricd mode the default is ``True``;
    in legacy gangliad mode the default is ``False``.

:macro-def:`GANGLIA_RESET_METRICS_FILE` / :macro-def:`GANGLIAD_RESET_METRICS_FILE`
    The file name where persistent data will be stored if the
    corresponding ``WANT_RESET_METRICS`` knob is set to ``True``.
    If not set to a fully qualified path, the file will be stored in the
    SPOOL directory with a filename extension of ``.ganglia_metrics``.
    If you are running multiple instances that share a SPOOL directory,
    this knob should be customized.
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

:macro-def:`GANGLIA_DEFAULT_CLUSTER` / :macro-def:`GANGLIAD_DEFAULT_CLUSTER`
    An expression specifying the default name of the Ganglia cluster for
    all metrics. The expression may refer to attributes of the machine.

:macro-def:`GANGLIA_DEFAULT_MACHINE` / :macro-def:`GANGLIAD_DEFAULT_MACHINE`
    An expression specifying the default machine name of Ganglia
    metrics. The expression may refer to attributes of the machine.

:macro-def:`GANGLIA_DEFAULT_IP` / :macro-def:`GANGLIAD_DEFAULT_IP`
    An expression specifying the default IP address of Ganglia metrics.
    The expression may refer to attributes of the machine.

Prometheus Backend Knobs
------------------------

The Prometheus text-format file export backend is only active in
*condor_metricd* mode and is disabled unless ``PROMETHEUS_METRICS_FILE``
is set.

:macro-def:`PROMETHEUS_METRICS_FILE`
    Absolute path to the Prometheus text-format output file. On each
    publication cycle the file is rewritten atomically (written to a
    ``.tmp`` companion and renamed into place). Empty by default, which
    disables the Prometheus backend.

:macro-def:`PROMETHEUS_METRICS_INCLUDE_TIMESTAMP`
    Boolean. When ``True``, each sample line is appended with the
    millisecond timestamp derived from the source daemon's
    ``LastHeardFrom`` value. Default is ``False``.

:macro-def:`PROMETHEUS_DEFAULT_LABELS`
    A label-set string applied as defaults to every Prometheus sample.
    Example: ``pool="mypoolname"``. Per-metric labels supplied via the
    ``PrometheusLabels`` keyword override defaults that share the same
    key.

:macro-def:`PROMETHEUS_WANT_RESET_METRICS`
    Per-backend reset-metrics flag. Defaults to ``False``.

:macro-def:`PROMETHEUS_RESET_METRICS_FILE`
    Backing file for the Prometheus backend's reset-metrics state. Used
    only when ``PROMETHEUS_WANT_RESET_METRICS`` is ``True``. Behaves like
    the Ganglia analogue but uses the suffix ``.prometheus_metrics``.

Metric-Definition Keywords
--------------------------

The following keywords may appear inside individual metric definitions
read from the metric config dir. They are silently ignored in legacy
gangliad mode.

``ExportMetric``
    A string ClassAd expression that evaluates to a comma-separated list
    of backend names this metric should be sent to. Recognized values
    are ``"ganglia"`` and ``"prometheus"``. If empty or omitted (and no
    pool-wide default is set via :macro:`METRICD_DEFAULT_EXPORT_METRIC`),
    the metric is exported to every enabled backend.

``PrometheusLabels``
    A string ClassAd expression that evaluates to a label-set string,
    e.g. ``strcat("machine=\"",Machine,"\"")``. The result is merged
    with :macro:`PROMETHEUS_DEFAULT_LABELS`; same-keyed entries from this
    expression override the pool-wide defaults.

``Counter``
    A boolean. Synonym for ``Derivative``. Applies in both modes.
