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

.. note::

    In *condor_metricd* mode these knobs take effect **only if at least one
    metric definition is actually routed to the Ganglia backend**. If none is,
    *condor_metricd* never initializes the backend at all -- deliberately, so
    that a Prometheus-only pool does not need ``libganglia`` installed -- and
    every knob in this section is left unread.

    This matters because the shipped default for
    :macro:`METRICD_DEFAULT_EXPORT_METRIC` is ``prometheus``, and the metric
    definitions shipped in :macro:`METRICD_METRICS_CONFIG_DIR` set no
    ``ExportMetric`` of their own. Out of the box, therefore, nothing routes to
    Ganglia and setting :macro:`GANGLIA_LIB` alone has no effect. To publish to
    Ganglia from *condor_metricd*, also name it in
    :macro:`METRICD_DEFAULT_EXPORT_METRIC` (for example ``ganglia, prometheus``,
    or an empty value meaning every enabled backend), or add an
    ``ExportMetric`` keyword naming ``ganglia`` to the individual metrics you
    want published there.

    *condor_metricd* records which backends it activated in its log at startup
    and on each reconfiguration, for example::

          Ganglia backend is active
          Prometheus backend is active

    None of this applies in legacy *condor_gangliad* mode, where Ganglia is the
    only backend and is always initialized.

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

:macro-def:`GANGLIA_MIN_METRIC_LIFETIME` / :macro-def:`GANGLIAD_MIN_METRIC_LIFETIME`
    An integer value representing the minimum DMAX value for all metrics,
    where DMAX is the number of seconds without updating that a metric will
    be kept before deletion. This value defaults to ``86400``, which is
    equivalent to 1 day. It is overridden for an individual metric by that
    metric's ``Lifetime`` value. Use ``GANGLIA_MIN_METRIC_LIFETIME`` in
    metricd mode, ``GANGLIAD_MIN_METRIC_LIFETIME`` in legacy gangliad mode.

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
    A ClassAd of label expressions applied as defaults to every Prometheus
    sample. Each attribute name is a Prometheus label name, and each attribute
    value is a ClassAd expression that is evaluated against the ad of the
    daemon the sample came from. *condor_metricd* handles all quoting and
    escaping of the resulting label values, so nothing in this ad ever needs
    to be escaped by hand.

    For a short label set, write the ad bracketed on a single line:

    .. code-block:: condor-config

          PROMETHEUS_DEFAULT_LABELS = [ pool = "mypoolname"; machine = Machine ]

    For anything longer, use a configuration heredoc and the long ClassAd
    form, one label per line:

    .. code-block:: condor-config

          PROMETHEUS_DEFAULT_LABELS @=end
             pool    = "mypoolname"
             machine = Machine
             daemon  = MyType
          @end

    Because these are expressions rather than fixed text, a pool-wide default
    can depend on the daemon being sampled -- ``machine = Machine`` above adds
    the correct ``machine`` label to every sample without any per-metric
    configuration.

    Per-metric labels supplied via the ``PrometheusLabels`` keyword override
    defaults that share the same label name. The label-name rules, the
    value-evaluation rules, and the ``MetricMachine`` / ``MetricPool``
    pseudo-attributes described under ``PrometheusLabels`` apply here as well,
    so a pool-wide ``[ pool = MetricPool ]`` labels every sample with the
    central manager of the pool it came from.

:macro-def:`PROMETHEUS_HTTP_PORT`
    The TCP port on which *condor_metricd* accepts HTTP (or HTTPS) requests
    for the ``/metrics`` endpoint, allowing a Prometheus server to scrape
    metrics directly rather than reading
    :macro:`PROMETHEUS_METRICS_FILE` off disk. The default is
    :macro:`SHARED_PORT_PORT`, meaning requests arrive through
    *condor_shared_port*. Set it to a specific port to have *condor_metricd*
    open a listening socket of its own, or to ``-1`` to disable HTTP serving
    entirely. Serving over HTTPS instead of HTTP is enabled by configuring
    :macro:`AUTH_SSL_SERVER_CERTFILE` and :macro:`AUTH_SSL_SERVER_KEYFILE`.

:macro-def:`PROMETHEUS_HTTP_AUTH_FILE`
    Path to an Apache-style ``htpasswd`` file. When set, the ``/metrics``
    endpoint requires HTTP Basic authentication against it; requests without
    valid credentials are answered with ``401``. Empty by default, which
    leaves the endpoint unauthenticated. Note that unless HTTPS is configured
    (see :macro:`PROMETHEUS_HTTP_PORT`), Basic credentials cross the network
    in the clear.

    Create the file with Apache's ``htpasswd`` tool, using ``-B`` to select
    bcrypt:

    .. code-block:: console

          $ htpasswd -B -c /etc/condor/prometheus.htpasswd prometheus

    Use ``-B`` rather than the default. Entries other than ``{SHA}`` are
    evaluated with the host's ``crypt(3)``, so which hash formats work is a
    property of the operating system, not of HTCondor. bcrypt (``$2b$`` /
    ``$2y$``), ``$1$``, ``$5$``, ``$6$``, and DES are supported by the
    libxcrypt implementation used on current Linux distributions, and
    ``{SHA}`` is handled by *condor_metricd* itself. However ``$apr1$``
    (Apache MD5), which some versions of ``htpasswd`` produce by default, is
    **not** available on several common platforms, including the RHEL 9
    family. An entry whose format the host cannot evaluate never
    authenticates; *condor_metricd* logs the reason and denies the request.

:macro-def:`PROMETHEUS_WANT_RESET_METRICS`
    Per-backend reset-metrics flag. Defaults to ``False``.

:macro-def:`PROMETHEUS_RESET_METRICS_FILE`
    Backing file for the Prometheus backend's reset-metrics state. Used
    only when ``PROMETHEUS_WANT_RESET_METRICS`` is ``True``. Behaves like
    the Ganglia analogue but uses the suffix ``.prometheus_metrics``.

Metric-Definition Keywords
--------------------------

The following keywords may appear inside individual metric definitions
read from the metric config dir. Except where noted below, they have no
effect in legacy gangliad mode.

``ExportMetric``
    A string ClassAd expression that evaluates to a comma-separated list
    of backend names this metric should be sent to. Recognized values
    are ``"ganglia"`` and ``"prometheus"``. If empty or omitted (and no
    pool-wide default is set via :macro:`METRICD_DEFAULT_EXPORT_METRIC`),
    the metric is exported to every enabled backend.

    This keyword also applies in legacy gangliad mode, where the only
    backend is Ganglia: a metric whose ``ExportMetric`` is non-empty and
    does not name ``"ganglia"`` is not published at all. An empty or
    omitted ``ExportMetric`` publishes normally. Note that
    :macro:`METRICD_DEFAULT_EXPORT_METRIC` is *not* consulted in legacy
    mode, so in that mode this keyword can only be set per metric.

``PrometheusLabels``
    A ClassAd whose attribute names are Prometheus label names and whose
    attribute values are ClassAd expressions. Each expression is evaluated
    against the ad of the daemon the sample came from, exactly like the
    ``Value`` and ``Desc`` keywords are. For example:

    .. code-block:: text

          [
            Name = "jobs_running";
            Value = TotalRunningJobs;
            Desc = "Number of running jobs";
            TargetType = "Scheduler";
            PrometheusLabels = [
                machine = Machine;
                pool    = "cm.example.edu";
                version = CondorVersion;
            ];
          ]

    which produces exposition lines such as::

          jobs_running{machine="ap1.example.edu",pool="cm.example.edu",version="..."} 17

    The result is merged with :macro:`PROMETHEUS_DEFAULT_LABELS`; labels
    defined here override pool-wide defaults with the same label name.

    *condor_metricd* quotes and escapes each label value when it writes the
    exposition text, so a label value containing a comma, a double quote, or
    a backslash is emitted correctly without any escaping in the metric
    definition.

    Label values are rendered according to the type each expression evaluates
    to: strings are used verbatim, integers are rendered as decimal, real
    numbers are rendered with ``%g``, and booleans are rendered as ``true``
    or ``false``.

    An expression that evaluates to ``UNDEFINED`` or ``ERROR`` -- or to a
    non-scalar value such as a list or a nested ad -- causes that label to be
    **omitted** from the sample rather than emitted with an empty value. This
    is the intended way to make a label conditional:

    .. code-block:: text

          PrometheusLabels = [
              machine = Machine;
              tier    = ifThenElse(IsProductionMachine, "prod", undefined);
          ];

    Here the ``tier`` label appears only on samples from machines where
    ``IsProductionMachine`` is true, and ``machine`` is simply absent from any
    daemon ad that has no ``Machine`` attribute.

    Label names must match the Prometheus grammar
    ``[a-zA-Z_][a-zA-Z0-9_]*`` and must not begin with ``__``, which
    Prometheus reserves for its own use. An illegal label name is reported
    once in the *condor_metricd* log when the configuration is read, and that
    label is not published.

    Each label expression is evaluated against the daemon ad directly rather
    than within the label ad, so ``machine = Machine`` means "the ``Machine``
    attribute of the daemon ad" and is not a self-reference. One consequence
    is that label expressions cannot refer to one another.

    Two pseudo-attributes are supplied by *condor_metricd* itself and may be
    used in any label expression:

    ``MetricMachine``
        The host this metric is associated with -- the same value the Ganglia
        backend uses as the spoof host, as described under the ``Machine``
        keyword. For a metric gathered from a single daemon this is that
        daemon's name; for an aggregate metric it is the pool's central
        manager.

    ``MetricPool``
        The name of the pool the metric came from: the central manager of the
        pool whose *condor_collector* supplied the daemon ad. When several
        pools are being monitored via :macro:`MONITOR_MULTIPLE_COLLECTORS` or
        :macro:`MONITOR_COLLECTOR`, this is the name configured for that pool,
        which is what makes a per-pool label correct on aggregate metrics.

    On an aggregate metric ``MetricMachine`` and ``MetricPool`` are the same
    value; on a non-aggregate metric they usually differ.

    .. warning::

        Prefer ``MetricMachine`` over a bare ``Machine`` reference when
        labeling an **aggregate** metric. An aggregate is published from the
        metric built for whichever contributing daemon ad happened to be
        processed first, so a label such as ``machine = Machine`` resolves to
        an arbitrary one of the daemons that fed the aggregate, and which one
        may change from cycle to cycle. ``MetricMachine`` and ``MetricPool``
        are well defined for aggregates; attributes read straight from the
        daemon ad are not.

``Counter``
    A boolean. Synonym for ``Derivative``. Applies in both modes.
