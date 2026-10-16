.. _metricd_config_options:

:index:`Metricd Options<single: Configuration; Metricd Options>`

This is the configuration variable reference for *condor_metricd*. For an
introduction to what *condor_metricd* does, how to enable it, and how to write
the metric definition files it reads, see
:ref:`admin-manual/monitoring:Monitor Pool Metrics with Prometheus and Ganglia`.

The variables are grouped the way the daemon itself is structured. The
``METRICD_*`` variables configure the engine, which queries the
*condor_collector* and turns the ClassAds it finds into metrics. The
``PROMETHEUS_*`` and ``GANGLIA_*`` variables configure the two backends that
the engine hands those metrics to.

Engine Configuration Variables
------------------------------

These variables control metric collection itself, independent of which
backend the resulting metrics are published to.

:macro-def:`METRICD[MetricD]`
    The full path to the *condor_metricd* binary. The default is
    ``$(LIBEXEC)/condor_metricd``.

:macro-def:`METRICD_INTERVAL[MetricD]`
    The integer number of seconds between consecutive collector queries, and
    therefore between consecutive publications of every metric. Daemons update
    the *condor_collector* every 300 seconds, and the Ganglia heartbeat
    interval is 20 seconds, so multiples of 20 between 20 and 300 make sense
    for this value. Negative values inhibit publication. The default is
    ``60``.

:macro-def:`METRICD_METRICS_CONFIG_DIR[MetricD]`
    The directory containing the files that define metrics. Every file in this
    directory is read, and each may contain any number of metric definitions.
    The default is ``$(ETC)/metrics.d``.

:macro-def:`METRICD_DEFAULT_EXPORT_METRIC[MetricD]`
    A comma-separated list of backend names supplying the default for the
    ``ExportMetric`` keyword, used for every metric definition that does not
    set its own. Recognized names are ``ganglia`` and ``prometheus``. An empty
    value means every enabled backend. The default is ``prometheus``, so out of
    the box nothing is routed to Ganglia and the Ganglia backend is never even
    initialized.

:macro-def:`METRICD_REQUIREMENTS[MetricD]`
    A boolean ClassAd expression that every ad fetched from the
    *condor_collector* must satisfy before any metric is evaluated against it.
    This could be used to monitor only a subset of a pool's daemons or
    machines. Keep in mind that the expression is applied to every type of
    monitored ClassAd, not just machine ClassAds. Be aware too that aggregate
    metrics will no longer cover the whole pool if this excludes ads that
    would have contributed. The default is an empty expression, which places
    no restriction.

:macro-def:`METRICD_PER_EXECUTE_NODE_METRICS[MetricD]`
    A boolean. When ``False``, metrics from execute node daemons are not
    published, which substantially reduces the volume of data produced by a
    large pool. Aggregate values from those machines are still published. This
    is particularly useful for pools that use glidein, where recording metrics
    for individual execute nodes is rarely wanted. The default is ``True``.

:macro-def:`METRICD_VERBOSITY[MetricD]`
    An integer specifying the maximum verbosity level of metrics to publish.
    A metric definition whose ``Verbosity`` keyword exceeds this value is not
    published. Basic metrics have a verbosity level of 0, which is the default;
    additional metrics can be enabled by raising this to 1. In the default
    configuration there are no metrics with a verbosity level above 1.

    Note that raising the verbosity is not always sufficient on its own, since
    some of the higher-verbosity metrics in the shipped
    ``00_default_metrics`` file depend on ClassAd attributes that daemons do
    not publish to the *condor_collector* by default. For example, per-user
    file transfer statistics appear only if the verbosity is 1 or higher
    **and** :macro:`STATISTICS_TO_PUBLISH` in the *condor_schedd*
    configuration contains ``TRANSFER:2``, or
    :macro:`STATISTICS_TO_PUBLISH_LIST` names the desired attributes
    explicitly. The default is ``0``.

:macro-def:`METRICD_WANT_PROJECTION[MetricD]`
    A boolean. When ``True``, *condor_metricd* asks the *condor_collector* for
    only the ClassAd attributes its metric definitions actually reference,
    rather than whole ads. This significantly reduces memory consumption and
    places much less load on the *condor_collector*. The projection is abandoned automatically if any metric definition
    uses the ``Regex`` keyword, or lacks a ``Value`` while having a
    non-literal ``Name``, since in those cases the needed attributes cannot be
    determined in advance. The default is ``True``.

:macro-def:`METRICD_LOG[MetricD]`
    The log file for this daemon. The default is ``$(LOG)/MetricdLog``.

:macro-def:`MAX_METRICD_LOG[MetricD]`
    The maximum size of the log file before it is rotated. The default is
    ``$(MAX_DEFAULT_LOG)``.

:macro-def:`METRICD_DEBUG[MetricD]`
    The debugging levels written to :macro:`METRICD_LOG`, as described in
    :macro:`<SUBSYS>_DEBUG`. The default is empty.

Monitoring Multiple Pools
~~~~~~~~~~~~~~~~~~~~~~~~~

By default *condor_metricd* queries the *condor_collector* of its own pool.
These variables let one *condor_metricd* gather metrics from several pools,
keeping their aggregates separate.

:macro-def:`MONITOR_MULTIPLE_COLLECTORS[MetricD]`
    A comma-separated list of collectors to query instead of the local pool's.
    Each entry has the form *name*\ ``/``\ *address*, where *address* is what
    would be given to the ``-pool`` argument of :tool:`condor_status`, and
    *name* is the pool name recorded with the metrics gathered from it. For
    example::

          MONITOR_MULTIPLE_COLLECTORS = ce1.example.edu/ce1.example.edu:9619, \
                                        ce2.example.edu/ce2.example.edu:9619

    The name is what aggregate metrics are grouped by and what the
    ``MetricPool`` pseudo-attribute evaluates to, so aggregation happens
    per pool rather than across all of them. The default is empty.

:macro-def:`MONITOR_COLLECTOR[MetricD]`
    Rather than listing collectors statically, query this collector to
    *discover* the list, and then monitor every collector found. This is useful
    when the set of pools changes over time. The result is used exactly as if
    it had been written into :macro:`MONITOR_MULTIPLE_COLLECTORS`, and is
    refreshed every 30 minutes. The default is empty.

:macro-def:`MONITOR_COLLECTOR_CONSTRAINT[MetricD]`
    A ClassAd expression limiting which ads returned by
    :macro:`MONITOR_COLLECTOR` are treated as collectors to monitor. The
    default is ``True``.

:macro-def:`MONITOR_COLLECTOR_AD_TYPE[MetricD]`
    The ClassAd type to query for when discovering collectors via
    :macro:`MONITOR_COLLECTOR`. The default is ``Scheduler``.

:macro-def:`MONITOR_COLLECTOR_NAME_ATTR[MetricD]`
    The attribute of a discovered ad holding the pool name. The default is
    ``Name``.

:macro-def:`MONITOR_COLLECTOR_ADDR_ATTR[MetricD]`
    The attribute of a discovered ad holding the collector address. The default
    is ``CollectorHost``.

Prometheus Backend Configuration Variables
------------------------------------------

These variables control how metrics are presented to Prometheus. See
:ref:`admin-manual/monitoring:Exposing Metrics to Prometheus` for the
corresponding setup instructions.

:macro-def:`PROMETHEUS_METRICS_FILE[MetricD]`
    Absolute path to a file written in the Prometheus text exposition format.
    On every publication cycle the file is rewritten atomically, by writing a
    ``.tmp`` companion and renaming it into place, so a reader never observes a
    partial file. This is the same content served at ``/metrics``, and is
    useful for the Prometheus node exporter textfile collector or for
    debugging. Setting this to an empty value disables the Prometheus backend
    altogether. The default is ``$(LOG)/metrics.prom``.

:macro-def:`PROMETHEUS_METRICS_INCLUDE_TIMESTAMP[MetricD]`
    A boolean. When ``True``, every sample line carries an explicit
    millisecond timestamp derived from the source daemon's
    ``LastHeardFrom`` attribute, rather than letting Prometheus use the
    time of the scrape. The default is ``True``.

:macro-def:`PROMETHEUS_DEFAULT_LABELS[MetricD]`
    A ClassAd of label expressions applied to every Prometheus sample. Each
    attribute name is a label name and each attribute value is a ClassAd
    expression evaluated against the ad of the daemon the sample came from.
    The default is ``[machine=MetricMachine; pool=MetricPool]``, which labels
    every sample with the host it describes and the pool it came from.

    For a short label set, write the ad bracketed on one line:

    .. code-block:: condor-config

          PROMETHEUS_DEFAULT_LABELS = [ pool = "mypoolname"; machine = Machine ]

    For anything longer, use a configuration heredoc and the long ClassAd form,
    one label per line:

    .. code-block:: condor-config

          PROMETHEUS_DEFAULT_LABELS @=end
             pool     = "mypoolname"
             machine  = Machine
             daemon   = MyType
          @end

    Labels set by a metric's own ``PrometheusLabels`` keyword override
    same-named labels from this variable. The label naming rules, the
    value-evaluation rules and the ``MetricMachine`` / ``MetricPool``
    pseudo-attributes are all described under ``PrometheusLabels`` in
    :ref:`admin-manual/monitoring:Metric Definition Keywords`.

:macro-def:`PROMETHEUS_HTTP_PORT[MetricD]`
    The TCP port on which *condor_metricd* serves the ``/metrics`` endpoint for
    Prometheus to scrape. The default is :macro:`SHARED_PORT_PORT` (normally
    ``9618``), meaning requests arrive through *condor_shared_port* alongside
    all other HTCondor traffic. Setting this to any other positive port causes
    *condor_metricd* to open a listening socket of its own on that port.
    Setting it to ``-1`` disables HTTP serving entirely, leaving only
    :macro:`PROMETHEUS_METRICS_FILE`.

:macro-def:`PROMETHEUS_HTTP_AUTH_FILE[MetricD]`
    Path to an Apache-style ``htpasswd`` file. When set, the ``/metrics``
    endpoint requires HTTP Basic authentication against it, and requests
    without valid credentials are answered with ``401``. The default is empty,
    which leaves the endpoint unauthenticated.

    Create the file with ``htpasswd -B`` to select bcrypt:

    .. code-block:: console

          $ htpasswd -B -c /etc/condor/prometheus.htpasswd prometheus

    Entries other than ``{SHA}`` are evaluated using the host's ``crypt(3)``,
    so which hash formats work is a property of the operating system rather
    than of HTCondor. bcrypt (``$2b$`` / ``$2y$``), ``$1$``, ``$5$``, ``$6$``
    and DES are supported by the libxcrypt implementation found on current
    Linux distributions, and ``{SHA}`` is handled by *condor_metricd* itself.
    However ``$apr1$`` (Apache MD5), which some versions of ``htpasswd``
    produce by default, is **not** available on several common platforms
    including the RHEL 9 family. An entry whose format the host cannot evaluate
    never authenticates; *condor_metricd* logs the reason and denies the
    request. Note also that unless HTTPS is configured, Basic credentials cross
    the network in the clear.

Ganglia Backend Configuration Variables
---------------------------------------

These variables control how metrics are presented to Ganglia. They take effect
only if at least one metric definition is actually routed to the Ganglia
backend. Since :macro:`METRICD_DEFAULT_EXPORT_METRIC` defaults to
``prometheus``, that is not the case out of the box; see
:ref:`admin-manual/monitoring:Exposing Metrics to Ganglia`.

:macro-def:`GANGLIA_SEND_DATA_FOR_ALL_HOSTS[MetricD]`
    A boolean. When ``True``, data is sent to Ganglia for hosts it is not
    currently monitoring. Set this to monitor a pool that Ganglia does not
    otherwise know about, or a heterogeneous pool where only some hosts are
    monitored. In that case the default graphs Ganglia provides will be absent,
    but the HTCondor metrics will appear. The default is ``False``.

:macro-def:`GANGLIA_GSTAT_COMMAND[MetricD]`
    The full *gstat* command used to determine which hosts Ganglia is
    monitoring. If *condor_metricd* runs on a host whose local *gmond* does not
    know the list of monitored hosts, change ``localhost`` here to an
    appropriate host name or address. The default is::

          gstat --all --mpifile --gmond_ip=localhost --gmond_port=8649

:macro-def:`GANGLIA_CONFIG[MetricD]`
    The path to the Ganglia configuration file. The default is
    ``/etc/ganglia/gmond.conf``.

:macro-def:`GANGLIA_LIB[MetricD]`
    The full path and file name of the ``libganglia`` shared library to use.
    If neither this nor :macro:`GANGLIA_GMETRIC` is set, ``libganglia`` is
    searched for in the directories named by :macro:`GANGLIA_LIB_PATH` or
    :macro:`GANGLIA_LIB64_PATH`. The special value ``NOOP`` makes
    *condor_metricd* go through all the motions without publishing anything to
    Ganglia, which is useful for testing. The default is empty.

:macro-def:`GANGLIA_GMETRIC[MetricD]`
    The full path of the *gmetric* executable to use. If unset, ``libganglia``
    is used instead where possible, since the library interface is more
    efficient than invoking *gmetric*. Some versions of ``libganglia`` are not
    compatible; when a failure to use it is detected, *gmetric* is used if it
    can be found in HTCondor's ``PATH``. The default is empty.

:macro-def:`GANGLIA_LIB_PATH[MetricD]`
    A comma-separated list of directories searched for ``libganglia`` on 32-bit
    systems when :macro:`GANGLIA_LIB` is not set. The default is
    ``/lib,/usr/lib,/usr/local/lib``.

:macro-def:`GANGLIA_LIB64_PATH[MetricD]`
    A comma-separated list of directories searched for ``libganglia`` on 64-bit
    systems when :macro:`GANGLIA_LIB` is not set. The default is
    ``/lib64,/usr/lib64,/usr/local/lib64``.

:macro-def:`GANGLIA_MIN_METRIC_LIFETIME[MetricD]`
    An integer, the minimum DMAX value for all metrics, where DMAX is the
    number of seconds a metric will be kept without being updated before
    Ganglia deletes it. A metric's own ``Lifetime`` keyword overrides this.
    The default is ``86400``, one day.

:macro-def:`GANGLIA_WANT_RESET_METRICS[MetricD]`
    A boolean. When ``True``, an aggregate metric that stops being updated is
    explicitly republished with a value of zero, so that Ganglia does not go on
    displaying the last value it saw indefinitely. Aggregate counters are
    deliberately excluded, since forcing a counter to zero would look like a
    counter reset and corrupt its rate calculation. The default is ``True``.

    There is no Prometheus equivalent, and none is needed: a series that stops
    appearing in the exposition is marked stale and ends on its own, which
    represents a vanished metric more accurately than a zero would.

:macro-def:`GANGLIA_RESET_METRICS_FILE[MetricD]`
    The file in which the state needed by :macro:`GANGLIA_WANT_RESET_METRICS`
    is persisted, so that metrics are still reset correctly across a restart.
    If not a fully qualified path, it is placed in the :macro:`SPOOL`
    directory. If several instances share a :macro:`SPOOL` directory, give each
    its own file. The default is
    ``$(SPOOL)/metricsToReset.ganglia_metrics``.

:macro-def:`GANGLIA_DEFAULT_CLUSTER[MetricD]`
    A ClassAd expression giving the default Ganglia cluster name for all
    metrics, overriding the per-metric ``Cluster`` keyword default. The
    expression may refer to attributes of the daemon ad. The default is empty.

:macro-def:`GANGLIA_DEFAULT_MACHINE[MetricD]`
    A ClassAd expression giving the default machine name for Ganglia metrics,
    overriding the per-metric ``Machine`` keyword default. The expression may
    refer to attributes of the daemon ad. The default is empty.

:macro-def:`GANGLIA_DEFAULT_IP[MetricD]`
    A ClassAd expression giving the default IP address for Ganglia metrics,
    overriding the per-metric ``IP`` keyword default. The expression may refer
    to attributes of the daemon ad. The default is empty.
