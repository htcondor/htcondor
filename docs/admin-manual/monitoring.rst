Monitoring and Observability
============================

:index:`monitoring<single: monitoring; pool management>`
:index:`monitoring pools` :index:`pool monitoring`

HTCondor records operational data in several places. The *condor_collector*
holds the current state of every slot and every daemon in the pool and, if
absent ads are enabled, of slots that have recently left it. Each AP keeps a
job history file describing jobs that have completed or been removed, and each
EP keeps a ``startd_history`` file covering the jobs that ran there. All of it
can be queried directly: :tool:`condor_status` reads the *condor_collector*,
and :tool:`condor_history` reads the history files.

Those tools answer a question asked once. They are a poor foundation for
monitoring over time, because none of this data is indexed or held in a real
database: queries get slower as a pool grows, old records age out so history
stays shallow, and there is nothing to graph with.

The usual answer is to copy the data out of HTCondor and into systems built to
store, index and visualize it. HTCondor ships a daemon for each half of the
data:

.. mermaid::
   :caption: Copying HTCondor's operational data into systems built to keep it
   :align: center

   flowchart LR
      COLL[(condor_collector<br/>live slot and daemon ads)]
      HIST[(job history files<br/>on each AP and EP)]
      COLL --> METRICD[condor_metricd]
      HIST --> ADSTASH[condor_adstash]
      METRICD --> PROM[Prometheus]
      METRICD --> GANG[Ganglia]
      ADSTASH --> ES[Elasticsearch<br/>or OpenSearch]
      PROM --> DASH[dashboards and graphs]
      GANG --> DASH
      ES --> SEARCH[searches and<br/>aggregations]

- *condor_metricd* turns the ClassAds in the *condor_collector* into pool
  metrics and publishes them to Prometheus and/or Ganglia, which in turn can
  drive dashboards in Grafana and similar tools. See
  :ref:`admin-manual/monitoring:Monitor Pool Metrics with Prometheus and Ganglia`.

- :tool:`condor_adstash` copies job history records into Elasticsearch or
  OpenSearch, where they can be searched and aggregated long after HTCondor
  itself has aged them out. See
  :ref:`admin-manual/monitoring:Searching Job History in Elasticsearch and OpenSearch`.

This chapter covers both, and then the log files HTCondor writes for itself,
which remain the first place to look when something has gone wrong.

Monitor Pool Metrics with Prometheus and Ganglia
------------------------------------------------

:index:`with Prometheus<single: with Prometheus; Monitoring>`
:index:`with Ganglia<single: with Ganglia; Monitoring>`
:index:`Prometheus monitoring` :index:`Ganglia monitoring`
:index:`condor_metricd daemon`

Why a daemon for this
'''''''''''''''''''''

Suppose you want to know how many slots in the pool are currently running a
job. That is a question about the ClassAds in the *condor_collector*: count the
slots where ``State == "Claimed"`` and ``Activity != "Idle"``. A one-off answer
is easy enough with a constraint and a projection:

.. code-block:: console

      $ condor_status -slots \
            -constraint 'State=="Claimed" && Activity!="Idle"' \
            -autoformat Name | wc -l

Suppose you then want that number plotted over time, so you put the command in
a cron job. Then you want the number of *cores* in use rather than slots, so
you sum the ``Cpus`` attribute instead. Then you want to know *why* the
remaining cores are idle, which is several more queries. Before long you have
dozens or hundreds of cron jobs, each independently querying the
*condor_collector* every minute, and a central manager spending all its time
answering them.

*condor_metricd* exists to do this properly: query the *condor_collector* once
per cycle for everything needed to compute every metric, and hand the results
to a time-series database built for storing and graphing them.

How it works
''''''''''''

*condor_metricd* has two halves.

.. mermaid::
   :caption: One collector query per cycle feeds every metric and every backend
   :align: center

   flowchart TD
      COLL[(condor_collector)]
      COLL -->|queried once per METRICD_INTERVAL| ENGINE
      subgraph METRICD[condor_metricd]
         ENGINE["Engine<br/>evaluate metric definitions:<br/>filter, compute, aggregate"]
         ENGINE -->|metrics| BACKEND["Backends<br/>present metrics to a<br/>time-series database"]
      end
      BACKEND --> PROM[Prometheus]
      BACKEND --> GANG[Ganglia]

The **engine** queries one or more *condor_collector* daemons on a timer, and
evaluates every metric definition against every ClassAd returned. A metric
definition says how to turn ClassAds into a metric: which ads to look at, what
expression to evaluate for the value, and whether to publish one metric per ad
or aggregate across all of them. The result is a stream of metrics, each a name
and a value at a point in time, with some labels attached.

The **backends** take those metrics and present them to a time-series database.
Two are supported. The Prometheus backend serves the metrics over HTTP for
Prometheus to scrape, and also writes them to a file. The Ganglia backend
hands them to Ganglia's *gmond*. Both can be active at once, and each metric
definition can choose which backends it goes to.

Because a single collector query serves every metric and every backend, the
cost to the *condor_collector* does not grow as metrics are added.

.. note::

    For backwards compatibility the *condor_metricd* binary is also installed
    under the name *condor_gangliad*, and inspects ``argv[0]`` at startup to
    decide which mode to run in. When invoked as *condor_gangliad* it behaves
    exactly as in previous releases: it reads only the ``GANGLIAD_*``
    configuration variables, reads metric definitions from
    ``$(ETC)/ganglia.d``, and publishes only to Ganglia. An existing
    *condor_gangliad* configuration therefore continues to work untouched, and
    nothing in this chapter applies to it.

    That legacy mode is deprecated and is not documented here. Administrators
    maintaining a *condor_gangliad* configuration should consult the
    documentation for the HTCondor version they are running. New deployments,
    and anyone adding Prometheus, should use *condor_metricd*.

Enabling the condor_metricd
'''''''''''''''''''''''''''

Add the following to the configuration of the machine that should run the
daemon, and :tool:`condor_restart` HTCondor there:

.. code-block:: condor-config

      use FEATURE: METRICD

Unless the pool is exceptionally large, the central manager is a good choice of
machine, since that is where the *condor_collector* it queries already runs.

That alone is enough to get a default set of metrics about the pool published
to Prometheus, both at the ``/metrics`` HTTP endpoint and in the file named by
:macro:`PROMETHEUS_METRICS_FILE`. For a comprehensive listing of all *condor_metricd*
configuration knobs see :ref:`metricd_config_options`. Everything below describes how to define
additional metrics and how to route them.

Defining Metrics
''''''''''''''''

A metric is a name, a value at a timestamp, and a set of labels. Metrics are
defined in files in the directory named by
:macro:`METRICD_METRICS_CONFIG_DIR`, which defaults to ``$(ETC)/metrics.d``.
Every file in that directory is read, and each may hold any number of
definitions.

A metric definition is itself a ClassAd, written in the bracketed form, whose
attributes are evaluated against each ClassAd fetched from the
*condor_collector*. Here is a complete one:

.. code-block:: condor-classad

    [
      Name   = "SchedulerRecentDaemonCoreDutyCycle";
      Value  = RecentDaemonCoreDutyCycle;
      Desc   = "Recent percentage of busy time in the daemon event loop";
      Scale  = 100;
      Units  = "percent";
      TargetType = "Scheduler";
    ]

``TargetType`` selects which ClassAds this metric applies to, here the ad
published by each *condor_schedd*. ``Value`` is evaluated against each such ad.
Since the attributes of the definition are ClassAd expressions evaluated in the
context of the daemon ad, they can refer to any attribute of it. ``Name`` can
too, which is how one definition can cover several daemon types:

.. code-block:: condor-classad

    [
      Name   = strcat(MyType, "RecentDaemonCoreDutyCycle");
      Value  = RecentDaemonCoreDutyCycle;
      Desc   = "Recent percentage of busy time in the daemon event loop";
      Scale  = 100;
      Units  = "percent";
      TargetType = "Scheduler,Negotiator";
    ]

An example set of metrics to use as a starting point is installed as
``$(METRICD_METRICS_CONFIG_DIR)/00_default_metrics``.

Aggregate Metrics
'''''''''''''''''

By default a metric produces one value per matching ClassAd. The ``Aggregate``
keyword instead combines them into a single pool-wide value, which is usually
what you want for a question about the pool rather than about one machine.
``Requirements`` restricts which ads contribute. These two definitions answer
"how many cores are in use, and how many are not":

.. code-block:: condor-classad

    [
      Name   = "CpusInUse";
      Aggregate = "SUM";
      Value  = Cpus;
      Requirements = State=="Claimed" && Activity!="Idle";
      TargetType = "Machine";
    ]
    [
      Name   = "CpusNotInUse";
      Aggregate = "SUM";
      Value  = Cpus;
      Requirements = State!="Claimed" || Activity=="Idle";
      TargetType = "Machine";
    ]

Breaking the idle cores down by reason is a matter of adding definitions with
narrower ``Requirements``:

.. code-block:: condor-classad

    [
      Name   = "CpusNotInUse_LowMemory";
      Aggregate = "SUM";
      Value  = Cpus;
      Requirements = State=="Unclaimed" && Memory < 1024;
      TargetType = "Machine";
    ]
    [
      Name   = "CpusNotInUse_Draining";
      Aggregate = "SUM";
      Value  = Cpus;
      Requirements = State=="Drained";
      TargetType = "Machine";
    ]

``AVG``, ``MIN`` and ``MAX`` work the same way. Note that ``Requirements`` can
be used to exclude ads that would distort the result, as here where slots with
no reported usage would drag an average toward zero:

.. code-block:: condor-classad

    [
      Name   = "MemoryEfficiency";
      Aggregate = "AVG";
      Value  = real(MemoryUsage)/Memory*100;
      Requirements = MemoryUsage > 0.0;
      Units  = "percent";
      TargetType = "Machine";
    ]

What host an aggregate belongs to
+++++++++++++++++++++++++++++++++

An ordinary metric describes one daemon, so it is naturally associated with
that daemon's host. An aggregate describes the pool as a whole and has no one
daemon behind it, so by default it is associated with the host running the
*condor_collector* it was gathered from — that is, the central manager.

This matters in both backends. Ganglia files the metric under the central
manager's host page rather than any execute node's. In Prometheus, the
``MetricMachine`` pseudo-attribute evaluates to the central manager's name, so
with the shipped :macro:`PROMETHEUS_DEFAULT_LABELS` an aggregate is labeled
``machine="cm.example.edu"`` while a per-daemon metric from the same pool is
labeled with the individual daemon. ``MetricPool`` is the central manager
either way, so on an aggregate the two coincide.

When several pools are monitored, each pool's aggregates are associated with
that pool's own central manager, and aggregation happens per pool rather than
across all of them.

Metric Definition Keywords
''''''''''''''''''''''''''

Most keywords are ClassAd expressions evaluated against the daemon ClassAd
being processed, so they may refer to any attribute of it. The value type
expected for each keyword, and what happens when it is omitted, are noted
below.

Core keywords
+++++++++++++

 Name
    *String expression.* **Required.** The name of this metric. Metrics
    published for the same machine must have unique names. When published to
    Prometheus the name is converted to snake_case, prefixed with
    ``htcondor_``, and may gain a unit, ``_total`` or ``_info`` suffix; see
    :ref:`admin-manual/monitoring:Exposing Metrics to Prometheus`.

    *If omitted*, there is no name to publish the metric under and the
    definition is unusable — except for a ``Regex`` metric, where the name of
    each matched attribute is used.
 TargetType
    *String expression.* **Required.** A comma-separated list of daemon
    ClassAd types this metric applies to, matching the :ad-attr:`MyType` of the
    daemon ClassAd. The special value ``Machine_slot1`` monitors the machine
    ClassAd for slot 1 only, which is useful for machine-wide attributes. The
    special value ``ANY`` matches every type of ClassAd.

    *If omitted*, *condor_metricd* refuses to start, reporting a configuration
    error naming the offending definition.
 Value
    *Expression of any type.* Produces the value of the metric when evaluated.

    *If omitted*, the value is that of the attribute in the daemon ClassAd
    whose name matches this metric's ``Name``; this is the usual way to
    publish an attribute unchanged. If the result is ``UNDEFINED`` after that
    fallback, no metric is published for that ad.
 Desc
    *String expression.* A brief description of the metric. Published to
    Prometheus as the ``# HELP`` text and displayed by Ganglia when hovering
    over the graph.

    *If omitted*, the metric is published with no description.
 Units
    *String expression.* The units of this metric, such as ``bytes``,
    ``seconds`` or ``percent``. For Prometheus this is appended to both the
    metric name and the help text, following Prometheus naming convention.

    *If omitted*, no unit suffix is added and nothing is appended to the help
    text.
 Scale
    *Numeric expression.* A factor multiplied by ``Value``. Used when the raw
    value is not in a human-interpretable unit: duty cycle is reported from 0
    to 1 but is conventionally expressed as a percentage, so it is scaled by
    100. When scaling by large values, setting ``Type`` to ``float`` is
    recommended.

    *If omitted*, the default is ``1``, leaving the value unchanged.
 Type
    *String.* The type of the metric: ``double``, ``float``, ``int32``,
    ``uint32``, ``int16``, ``uint16``, ``int8``, ``uint8`` or ``string``.
    Integer values can be coerced to ``float`` or ``double``, which matters for
    values stored internally as 64 bits.

    *If omitted*, the type is inferred from the evaluated value: ``string`` for
    strings, ``int32`` for integers, ``float`` for reals and ``int8`` for
    booleans.
 Requirements
    *Boolean expression.* Restricts which ClassAds contribute to this metric.
    An ad for which this is not ``True`` is skipped entirely.

    *If omitted*, the default is ``True``, so every ad of the matching
    ``TargetType`` contributes.
 Verbosity
    *Integer.* The verbosity level of this metric. The metric is not published
    if this exceeds :macro:`METRICD_VERBOSITY`.

    *If omitted*, the default is ``0``, so the metric is always published.
 Counter
    *Boolean.* When ``True``, the value is treated as a monotonically
    increasing counter rather than a gauge — a quantity that starts at zero
    and only grows, such as :ad-attr:`FileTransferUploadBytes`. Prometheus
    publishes it as a ``counter`` with a ``_total`` suffix and computes rates
    itself; Ganglia graphs its derivative. ``Derivative`` is an accepted
    synonym.

    For an aggregate metric, the per-ad increase since the previous cycle is
    computed first and those increases are then aggregated, so that a daemon
    restarting does not produce a spurious negative contribution.

    *If omitted*, the default is ``False``: a numeric value is published as a
    gauge, meaning a reading at a point in time with no implied relationship
    to the previous reading, and a string value is published as an info
    metric.
 Aggregate
    *String.* An aggregation function applied across all matching ClassAds,
    producing one combined metric instead of one metric per ad. Possible
    values are ``SUM``, ``AVG``, ``MAX`` and ``MIN``.

    *If omitted*, no aggregation is performed and one metric is published per
    matching ClassAd.
 AggregateGroup
    *String expression.* Meaningful only when ``Aggregate`` is set, where it
    works like ``GROUP BY`` in SQL: one result is produced per distinct value
    of this expression. Set it to group by something other than the pool as a
    whole, such as the owner of the running job, as in
    :ref:`admin-manual/monitoring:Labels versus names` below. Name only the
    dimension to group by; the metric's own name is always part of the
    grouping, so two metrics that use the same expression here are still
    aggregated separately from each other. When several pools are being
    monitored, the collector name is part of it too, so each pool is
    aggregated separately.

    *If omitted*, there is no further grouping and a single pool-wide result
    is produced.
 ExportMetric
    *String expression.* A comma-separated list of the backends this metric
    should be published to; recognized names are ``ganglia`` and
    ``prometheus``. An empty value means every enabled backend.

    *If omitted*, the value of :macro:`METRICD_DEFAULT_EXPORT_METRIC` is used,
    which itself defaults to ``prometheus``.
 Regex
    *String.* A regular expression matching the attributes to be monitored by
    this metric. This is for dynamic attributes that cannot be enumerated in
    advance, because their names depend on runtime information such as which
    users are currently running jobs. One metric is created per matching
    attribute; the default metric name is the name of the matched attribute,
    and the default value is that attribute's value.

    Because the attribute name is not known in advance, a special attribute
    named ``Regex`` is made available to the ``Value`` expression. Text matched
    by parenthesized groups in the regular expression can be substituted into
    other string keywords such as ``Name`` and ``Desc`` by writing "\\\\1" for
    the first group, "\\\\2" for the second, and so on.

    Note that using ``Regex`` in any metric definition disables the collector
    projection described under :macro:`METRICD_WANT_PROJECTION` for the whole
    daemon, since the attributes needed can no longer be determined in
    advance.

    *If omitted*, the metric monitors the single attribute named by ``Name``
    rather than a family of them.

Prometheus-specific keywords
++++++++++++++++++++++++++++

 PrometheusLabels
    *Nested ClassAd.* Its attribute names are Prometheus label names and its
    attribute values are ClassAd expressions, evaluated against the daemon ad
    exactly as ``Value`` is:

    .. code-block:: condor-classad

          PrometheusLabels = [
              machine = Machine;
              pool    = "cm.example.edu";
              state   = strcat(State, "_", Activity);
          ];

    The result is merged with :macro:`PROMETHEUS_DEFAULT_LABELS`, with labels
    defined here winning over pool-wide ones of the same name.
    *condor_metricd* quotes and escapes the label values, so a value
    containing a comma, quote, backslash or newline is emitted correctly
    without anything being escaped by hand here.

    Label names are converted to snake_case before that merge, so ``State``
    here and ``state`` in :macro:`PROMETHEUS_DEFAULT_LABELS` are the same
    label and this one wins, however either is spelled. For the same reason,
    two names within a single label ad that differ by more than case but
    converge on one label name — ``AccountingGroup`` and ``Accounting_Group``,
    say — are a configuration error: only the first can be published, and
    *condor_metricd* reports the clash at startup.

    An expression evaluating to ``UNDEFINED`` or ``ERROR``, or to a non-scalar
    value such as a list, **omits** that label rather than emitting it empty.
    This is the intended way to make a label conditional:

    .. code-block:: condor-classad

          PrometheusLabels = [
              machine = Machine;
              tier    = ifThenElse(IsProductionMachine, "prod", undefined);
          ];

    Label names must match ``[a-zA-Z_][a-zA-Z0-9_]*`` and must not begin with
    ``__``, which Prometheus reserves. An illegal name is reported once in the
    *condor_metricd* log when the configuration is read, and that label is not
    published.

    Each label expression is evaluated against the daemon ad directly rather
    than within the label ad, so ``machine = Machine`` means "the ``Machine``
    attribute of the daemon ad" and is not a self-reference. One consequence is
    that label expressions cannot refer to one another.

    *If omitted*, the metric carries only the labels from
    :macro:`PROMETHEUS_DEFAULT_LABELS`.

    Two pseudo-attributes are supplied by *condor_metricd* for use in label
    expressions:

    ``MetricMachine``
        The host this metric is associated with. For a metric gathered from a
        single daemon this is that daemon's name; for an aggregate metric it is
        the pool's central manager.

    ``MetricPool``
        The name of the pool the metric came from. When several pools are
        monitored via :macro:`MONITOR_MULTIPLE_COLLECTORS` or
        :macro:`MONITOR_COLLECTOR`, this is the name configured for that pool,
        which is what makes a per-pool label correct on aggregate metrics.

    On an aggregate metric the two are the same value; on a non-aggregate
    metric they usually differ.

    .. warning::

        Prefer ``MetricMachine`` over a bare ``Machine`` reference when
        labeling an **aggregate** metric. An aggregate is published from the
        metric built for whichever contributing daemon ad happened to be
        processed first, so ``machine = Machine`` resolves to an arbitrary one
        of the daemons that fed the aggregate, and which one may change from
        cycle to cycle.

 String-valued metrics
    This is not a keyword but a consequence of ``Type``.
    Prometheus sample values must be numeric, so a metric whose value is a
    string is published as an *info* metric: the name gains an ``_info``
    suffix, the string moves into a label named after the metric, and the value
    becomes 1. A metric named ``condor_version`` with a string value is
    therefore scraped as::

          htcondor_condor_version_info{condor_version="10.2.0"} 1

Ganglia-specific keywords
+++++++++++++++++++++++++

 Title
    *String expression.* The graph title used for this metric.

    *If omitted*, the metric name is used.
 Group
    *String expression.* The name of this metric's group. Metrics are arranged
    by group within a Ganglia web page, and metrics in different groups must
    have unique names.

    *If omitted*, the group is determined by the daemon type, for example
    "HTCondor Schedd" or "HTCondor Startd".
 Cluster
    *String expression.* The Ganglia cluster name for this metric.

    *If omitted*, the value of :macro:`GANGLIA_DEFAULT_CLUSTER` is used.
 Machine
    *String expression.* The name of the host associated with this metric.

    *If omitted*, the value of :macro:`GANGLIA_DEFAULT_MACHINE` is used; if
    that is not set either, the default is taken from the ``Machine``
    attribute of the daemon ClassAd. If the daemon name is of the form
    *name@hostname*, indicating several instances of HTCondor on one machine,
    the daemon name is used instead so that their metrics do not overwrite
    each other. For an aggregate metric the default is the name of the
    *condor_collector* host, as described above.
 IP
    *String expression.* The IP address of the host associated with this
    metric. The value must be unique for each machine published to Ganglia,
    but need not be a valid IP address.

    *If omitted*, the value of :macro:`GANGLIA_DEFAULT_IP` is used; if that is
    not set either, the default is extracted from the ``MyAddress`` attribute
    of the daemon ClassAd. If ``Machine`` contains an "@", the default IP is
    set to the same value as ``Machine`` to keep it unique per instance.
 Lifetime
    *Positive integer.* The maximum number of seconds a metric will be kept
    without being updated before Ganglia deletes it, represented in Ganglia as
    DMAX.

    *If omitted*, a value is calculated from the publication interval, with a
    floor of :macro:`GANGLIA_MIN_METRIC_LIFETIME`.

Backend-specific overrides
++++++++++++++++++++++++++

Prefixing any keyword with ``Ganglia_`` or ``Prometheus_`` overrides that
keyword for that backend only, leaving the unprefixed value in effect for the
other. This is what makes a single definition able to serve both backends when
they want different things, as in the per-user example in
:ref:`admin-manual/monitoring:Labels versus names` below, where
``Ganglia_Name`` embeds the user in the metric name while Prometheus gets a
label instead.

.. code-block:: condor-classad

    [
      Name   = "jobs_running";
      Prometheus_Desc = "Number of running jobs";
      Ganglia_Desc = "Jobs running in this pool right now";
      TargetType = "Scheduler";
    ]

Choosing Which Backend Each Metric Goes To
''''''''''''''''''''''''''''''''''''''''''

The ``ExportMetric`` keyword names the backends a metric should be published
to. :macro:`METRICD_DEFAULT_EXPORT_METRIC` supplies the default for definitions
that do not set it, and ships as ``prometheus``. So out of the box every metric
goes to Prometheus only, and the Ganglia backend is never initialized — which
is deliberate, since it means a Prometheus-only pool never needs
``libganglia`` installed.

To send everything to both backends, set the default to empty, which means
"every enabled backend":

.. code-block:: condor-config

      METRICD_DEFAULT_EXPORT_METRIC =

Or route individual metrics:

.. code-block:: condor-classad

    [
      Name   = "JobsSubmitted";
      ExportMetric = "ganglia, prometheus";
      Desc   = "Number of jobs submitted";
      Units  = "jobs";
      TargetType = "Scheduler";
    ]

Labels versus names
+++++++++++++++++++

Prometheus and Ganglia disagree about how to express a metric that has a
dimension to it, such as a per-user or per-daemon-type breakdown.

Prometheus wants one metric name with a **label** carrying the dimension, so
that queries can sum over it or filter by it. Ganglia has no labels, and wants
the dimension **embedded in the metric name**.

A metric definition can satisfy both at once. ``PrometheusLabels`` supplies the
labels, and a ``Ganglia_`` prefix on any keyword overrides that keyword for the
Ganglia backend only:

.. code-block:: condor-classad

    [
      Name   = "UserMemoryEfficiency";
      PrometheusLabels = [ user = RemoteUser ];
      Ganglia_Name = strcat(RemoteUser, "-UserMemoryEfficiency");
      Title  = strcat(RemoteUser, " Memory Efficiency");
      Aggregate = "AVG";
      AggregateGroup = RemoteUser;
      Value  = real(MemoryUsage)/Memory*100;
      Requirements = MemoryUsage > 0.0;
      Units  = "percent";
      TargetType = "Machine";
    ]

Prometheus receives ``htcondor_user_memory_efficiency_percent{user="alice"}``
while Ganglia receives a separate metric named
``alice-UserMemoryEfficiency``. The same prefix trick works for any keyword:
``Prometheus_Desc`` gives Prometheus a different description than Ganglia,
``Ganglia_Units`` a different unit string, and so on.

Here is the same idea applied to a metric covering several daemon types, where
Prometheus gets one series labeled by type and Ganglia gets one metric per
type:

.. code-block:: condor-classad

    [
      Name   = "RecentDaemonCoreDutyCycle";
      PrometheusLabels = [
          type = MyType;
          tier = ifThenElse(IsProductionMachine, "production", undefined);
      ];
      Ganglia_Name = strcat(MyType, "RecentDaemonCoreDutyCycle");
      Value  = RecentDaemonCoreDutyCycle;
      Desc   = "Recent percentage of busy time in the daemon event loop";
      Scale  = 100;
      Units  = "percent";
      TargetType = "Scheduler,Negotiator";
    ]

A label expression evaluating to ``UNDEFINED`` drops that label rather than
emitting it empty, which is how ``tier`` above appears only on production
machines.

Exposing Metrics to Prometheus
''''''''''''''''''''''''''''''

``use FEATURE: METRICD`` gives you a Prometheus exporter with no further
configuration. *condor_metricd* serves the standard ``/metrics`` endpoint over
HTTP on the port named by :macro:`PROMETHEUS_HTTP_PORT`, which defaults to
:macro:`SHARED_PORT_PORT` — normally ``9618``, the same port the rest of
HTCondor already uses. The feature arranges for *condor_shared_port* to forward
HTTP requests on that port to *condor_metricd*, so no additional port needs to
be opened in a firewall.

Confirm it is working with:

.. code-block:: console

      $ curl http://cm.example.edu:9618/metrics

and point Prometheus at it:

.. code-block:: yaml

      scrape_configs:
        - job_name: htcondor
          static_configs:
            - targets: ['cm.example.edu:9618']

Every metric name is prefixed with ``htcondor_``, following the Prometheus
convention of namespacing an application's metrics, so a metric defined as
``Name = "jobs_running"`` is scraped as ``htcondor_jobs_running``.

Metric names and label names are also converted from the CamelCase of HTCondor
ClassAd attributes to the snake_case that Prometheus conventionally uses, so
``Name = "TotalRunningJobs"`` is scraped as ``htcondor_total_running_jobs``.
Embedded acronyms are kept whole rather than split letter by letter, so
``MonitorSelfCPUUsage`` becomes ``monitor_self_cpu_usage``. A name you already
wrote in snake_case is left exactly as it is, and the conversion applies only
to Prometheus: the Ganglia backend publishes names unchanged.

The same content is also written to the file named by
:macro:`PROMETHEUS_METRICS_FILE`, by default ``$(LOG)/metrics.prom``. The file
is rewritten atomically on every cycle, so a reader never sees a partial one.
This is useful for inspecting the output by hand, and for sites that prefer to
collect metrics with the Prometheus node exporter's textfile collector rather
than scrape a second endpoint.

There are therefore three routes by which metrics reach Prometheus. The
subsections below cover the choices along each of them.

.. mermaid::
   :caption: Three routes out of *condor_metricd*; either HTTP route also serves HTTPS
   :align: center

   flowchart LR
      METRICD[condor_metricd]
      METRICD -->|default| SP["condor_shared_port<br/>on port 9618"]
      METRICD -->|dedicated port| OWN["its own socket on<br/>PROMETHEUS_HTTP_PORT"]
      METRICD -->|every cycle| FILE[("metrics.prom")]
      SP --> PROM[Prometheus]
      OWN --> PROM
      OWN --> PROXY["reverse proxy<br/>if you want one"]
      PROXY --> PROM
      FILE --> NODE["node exporter<br/>textfile collector"]
      NODE --> PROM

Serving over HTTPS
++++++++++++++++++

If :macro:`AUTH_SSL_SERVER_CERTFILE` and :macro:`AUTH_SSL_SERVER_KEYFILE` are
configured and readable, *condor_metricd* automatically accepts TLS on the same
port; it detects whether each connection is plain HTTP or TLS and responds
accordingly. No separate setting enables this. Scrape it with ``https://``
instead, and configure Prometheus accordingly:

.. code-block:: yaml

      scrape_configs:
        - job_name: htcondor
          scheme: https
          static_configs:
            - targets: ['cm.example.edu:9618']

Requiring a password
++++++++++++++++++++

Set :macro:`PROMETHEUS_HTTP_AUTH_FILE` to the path of an Apache-style
``htpasswd`` file to require HTTP Basic authentication on ``/metrics``:

.. code-block:: condor-config

      PROMETHEUS_HTTP_AUTH_FILE = /etc/condor/prometheus.htpasswd

.. code-block:: yaml

      scrape_configs:
        - job_name: htcondor
          basic_auth:
            username: prometheus
            password: s3cr3t
          static_configs:
            - targets: ['cm.example.edu:9618']

Use ``htpasswd -B`` to create entries; see
:macro:`PROMETHEUS_HTTP_AUTH_FILE` for which hash formats work. Basic
credentials are sent in the clear unless HTTPS is also configured.

Using a different port, or a reverse proxy
++++++++++++++++++++++++++++++++++++++++++

Setting :macro:`PROMETHEUS_HTTP_PORT` to a port other than
:macro:`SHARED_PORT_PORT` causes *condor_metricd* to open a listening socket of
its own on that port, rather than receiving forwarded requests from
*condor_shared_port*:

.. code-block:: condor-config

      use FEATURE: METRICD
      PROMETHEUS_HTTP_PORT = 9100

This is the usual arrangement when putting a reverse proxy in front of the
endpoint, so that the proxy owns the public port and terminates TLS, while
*condor_metricd* listens only on the loopback-facing port behind it. With
NGINX, for example:

.. code-block:: nginx

      location /metrics {
          proxy_pass http://127.0.0.1:9100/metrics;
      }

Because that is the usual reason for moving the endpoint off the shared port,
a dedicated socket binds **only the loopback address** by default: the proxy
can reach it, the network cannot. If you instead want Prometheus to scrape the
dedicated port directly from another host, bind the regular interfaces with:

.. code-block:: condor-config

      PROMETHEUS_HTTP_LOCALHOST_ONLY = False

:macro:`PROMETHEUS_HTTP_LOCALHOST_ONLY` is consulted only when
*condor_metricd* opens a socket of its own, which is to say only when
:macro:`PROMETHEUS_HTTP_PORT` differs from :macro:`SHARED_PORT_PORT`. In the
default configuration the endpoint arrives through *condor_shared_port* on a
socket that belongs to *condor_shared_port* and carries all other HTCondor
traffic, so there is nothing for *condor_metricd* to restrict; the setting has
no effect there whatever its value, and a line is logged at startup saying so.
Use :macro:`PROMETHEUS_HTTP_AUTH_FILE` to control access to an endpoint that is
reachable from the network.

Setting :macro:`PROMETHEUS_HTTP_PORT` to ``-1`` disables HTTP serving
altogether, leaving only only a prom file at location :macro:`PROMETHEUS_METRICS_FILE`. That is the right
choice for sites collecting the file with the node exporter's textfile
collector (although be aware using the node exporter's textfile collector
will also require setting :macro:`PROMETHEUS_METRICS_INCLUDE_TIMESTAMP` to False, as 
the textfile collector does not support timestamps).

.. note::

    :macro:`PROMETHEUS_HTTP_PORT` is read once at startup. Changing it and
    issuing a :tool:`condor_reconfig` has no effect, and *condor_metricd* will
    log that a restart is needed.

Exposing Metrics to Ganglia
'''''''''''''''''''''''''''

Installing and configuring Ganglia itself is beyond the scope of this document;
see the Ganglia homepage at `http://ganglia.info/ <http://ganglia.info/>`_.

Two things are needed to publish to Ganglia. First, route some metrics there,
since nothing goes to Ganglia by default:

.. code-block:: condor-config

      use FEATURE: METRICD
      # send every metric to both backends
      METRICD_DEFAULT_EXPORT_METRIC =

Second, make sure *condor_metricd* can reach Ganglia. It is simplest to run it
on the same machine as Ganglia's *gmetad*. If it runs elsewhere, set
:macro:`GANGLIA_GSTAT_COMMAND` so it can get the list of monitored hosts from
the master *gmond*.

By default, metrics are only propagated for hosts Ganglia already monitors. Set
:macro:`GANGLIA_SEND_DATA_FOR_ALL_HOSTS` to ``True`` to monitor a pool Ganglia
does not otherwise know about, or a pool where only some hosts are monitored.
The default Ganglia graphs will then be absent for those hosts, but the
HTCondor metrics will appear.

If no metric is routed to Ganglia, the backend is never initialized and none of
the ``GANGLIA_*`` variables have any effect — including
:macro:`GANGLIA_LIB`. *condor_metricd* records which backends it activated in
its log at startup::

      Ganglia backend is active
      Prometheus backend is active

Reducing the Volume of Data
'''''''''''''''''''''''''''

On a large pool, three variables control how much work is done and how much
data is produced:

- :macro:`METRICD_PER_EXECUTE_NODE_METRICS` set to ``False`` stops publishing
  per-execute-node metrics, which are usually the least interesting and by far
  the most numerous. Pool-wide aggregates are unaffected.
- :macro:`METRICD_REQUIREMENTS` restricts which ClassAds are considered at all.
  Be aware that aggregates will no longer cover the whole pool if this excludes
  ads that would have contributed.
- :macro:`METRICD_INTERVAL` controls how often the whole cycle runs.

:macro:`METRICD_WANT_PROJECTION`, on by default, already limits the collector
query to just the attributes the metric definitions reference.

Searching Job History in Elasticsearch and OpenSearch
-----------------------------------------------------

:index:`Elasticsearch`
:index:`OpenSearch`
:index:`adstash`
:index:`condor_adstash`

What is in the job archive
''''''''''''''''''''''''''

"Job history" is really four distinct kinds of record, three of them written
by the AP and one by the EP. They answer different questions, and
:tool:`condor_adstash` can push any of them.

 Completed job ads
    One ClassAd per job that has left the queue, whether it ran to completion
    or was removed, appended to the file named by :macro:`HISTORY` (by default
    ``$(SPOOL)/history``). This is the job's final state: what it asked for,
    where it ran, how long it took and how it ended. :tool:`condor_history`
    reads these unless told otherwise.

 Job epoch ads
    One ClassAd per *run attempt*, appended to the file named by
    :macro:`JOB_EPOCH_HISTORY` (by default ``$(SPOOL)/epoch_history``) each
    time a job stops running — because it finished, was held, or was evicted
    and went back to idle to be run again.

    A job that is evicted twice before finally completing therefore leaves
    three epoch ads and one completed job ad. Each epoch ad is a full snapshot
    of the job ad as it stood at the end of that run.

    Note that the completed job ad is not merely the last attempt: it carries
    counters and totals that already summarize the whole history, such as
    :ad-attr:`NumJobStarts`, :ad-attr:`NumShadowStarts`,
    :ad-attr:`TotalSuspensions` and :ad-attr:`RemoteWallClockTime`. Anything
    answerable by a count or a sum is therefore available without epoch ads at
    all.

    What the completed ad cannot give you is the per-attempt detail behind
    those aggregates, since attributes describing a single run get overwritten
    by the next one — :ad-attr:`RemoteHost` records only where the job ran
    last. Epoch ads preserve each run's values separately, which is what you
    need to know every machine a job ever ran on, or to reconstruct exactly
    what the job ad looked like at the end of one particular attempt. Read
    them with ``condor_history -epochs``.

 File transfer ads
    One ClassAd per transfer of a job's input, output or checkpoint files,
    written to the same :macro:`JOB_EPOCH_HISTORY` file and distinguished by a
    ``TransferClass`` attribute naming which kind it was. These carry
    per-transfer timing, byte counts and plugin results, which is what you
    need to tell a job that is slow from a job that is merely waiting on its
    data. Read them with ``condor_history -transfer-history``, optionally
    narrowed to input, output or checkpoint transfers.

 Startd history
    One ClassAd per job that ran on a given EP, written on that EP to the file
    named by :macro:`STARTD_HISTORY` (by default ``$(LOG)/startd_history``).
    This is the machine's view rather than the job's, and is the record that
    survives on the EP regardless of what the AP does. Read it with
    ``condor_history -startd``.

By default :tool:`condor_adstash` pushes **only the completed job ads**, from
every Access Point in the pool. The other three sources are available but must
be asked for: an AP's epoch and transfer records are written to disk whether or
not anything reads them, so turning the corresponding variable on is all that
is needed to start indexing them alongside the completed job ads.

Each source is enabled separately:

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Configuration variable
     - Pushes
   * - :macro:`ADSTASH_SCHEDD_HISTORY`
     - completed job ads
   * - :macro:`ADSTASH_SCHEDD_JOB_EPOCH_HISTORY`
     - job epoch ads
   * - :macro:`ADSTASH_SCHEDD_TRANSFER_EPOCH_HISTORY`
     - file transfer ads
   * - :macro:`ADSTASH_STARTD_HISTORY`
     - startd history ads

All four default to ``False``; ``use feature : adstash`` sets
:macro:`ADSTASH_SCHEDD_HISTORY` to ``True``, which is what makes completed job
ads from every AP the out-of-the-box behavior. To index epoch or transfer
records as well, add for example:

.. code-block:: condor-config

      use feature : adstash
      ADSTASH_SCHEDD_JOB_EPOCH_HISTORY = True
      ADSTASH_SCHEDD_TRANSFER_EPOCH_HISTORY = True

.. note::

    Writing epoch and transfer records is controlled separately from pushing
    them. Both are written whenever :macro:`JOB_EPOCH_HISTORY` names a file,
    which it does by default, so the records normally exist whether or not
    anything is reading them. Setting it to nothing disables both. HTCondor
    rotates that file according to :macro:`MAX_EPOCH_HISTORY_LOG` and
    :macro:`MAX_EPOCH_HISTORY_ROTATIONS`, so it will not grow without bound,
    but a busy AP will age records out of it faster than out of
    :macro:`HISTORY`.

Pushing records to a search engine
''''''''''''''''''''''''''''''''''

HTCondor supports pushing *condor_schedd* and *condor_startd* job
and job epoch ClassAds
to search engines
(currently Elasticsearch and OpenSearch are supported)
and other targets via the
:tool:`condor_adstash` tool/daemon.
:tool:`condor_adstash` collects job ClassAds as specified by its
configuration, either querying specified daemons
or reading job ClassAds from a specified file,
converts each ClassAd to a JSON document,
and pushes each doc to the configured search engine index.
Fields are mapped based on well-known job ClassAd attributes,
and mappings are updated each polling cycle to account for new attributes.
Custom attributes are also pushed, typically as keyword (string) fields
unless a custom field mapping is provided.

:tool:`condor_adstash` is a Python script
that uses the HTCondor :ref:`apis/python-bindings/index:Python Bindings`
and either the
`Python Elasticsearch Client <https://elasticsearch-py.readthedocs.io/>`_
or the
`Python OpenSearch Client <https://opensearch.org/docs/latest/clients/python-low-level/>`_,
both of which must be available to the system Python installation
if using the daemonized version of :tool:`condor_adstash`.
:tool:`condor_adstash` can also be run as a standalone tool
(e.g. in a Python virtual environment containing the necessary libraries).

Running :tool:`condor_adstash` as a daemon (i.e. under the watch of the
:tool:`condor_master`) can be enabled by adding
``use feature : adstash``
to your HTCondor configuration.
By default, this configuration will,
every 20 minutes,
poll the job history on all *condor_schedds* that report to the ``$(CONDOR_HOST)`` *condor_collector*
and push the contents of the job history ClassAds
to a search engine instance running on ``localhost``
to an index named ``htcondor``.
Your situation and monitoring needs are likely different!
See the ``condor_config.local.adstash`` example configuration file
in the ``examples/`` directory
for detailed information on how to modify your configuration.

Before running :tool:`condor_adstash` for the first time,
consider using ``condor_adstash --init_index``
to generate the JSON files needed
to set up the search engine index, index template, and (for Elasticsearch) ILM policy.
See the :tool:`condor_adstash` man page
for details on index initialization and other options.

If you prefer to run :tool:`condor_adstash` in standalone mode, or are
curious about other ClassAd sources or targets, see the
:doc:`../man-pages/condor_adstash` man page for more
details.

Files, Directories and Logs
---------------------------

:index:`logging`

HTCondor records many types of information in a variety of logs.
Administration may require locating and using the contents of a log to
debug issues. Listed here are details of the logs, to aid in
identification.

Job and Daemon Logs
'''''''''''''''''''

job event log
    The job event log is an optional, chronological list of events that
    occur as a job runs. The job event log is written on the submit
    machine. The submit description file for the job requests a job
    event log with the submit command
    :subcom:`log[definition]`. The log is created
    on and remains on the access point. Contents of the log are detailed
    in the :ref:`users-manual/managing-a-job:in the job event log file` section.
    Examples of events are that the job is running, that the job is placed on
    hold, or that the job completed.

daemon logs
    Each daemon configured to have a log writes events relevant to that
    daemon. Each event written consists of a timestamp and message. The
    name of the log file is set by the value of configuration variable
    :macro:`<SUBSYS>_LOG`, where :macro:`<SUBSYS>` is
    replaced by the name of the daemon. The log is not permitted to grow
    without bound; log rotation takes place after a configurable maximum
    size or length of time is encountered. This maximum is specified by
    configuration variable :macro:`MAX_<SUBSYS>_LOG`.

    Which events are logged for a particular daemon are determined by
    the value of configuration variable :macro:`<SUBSYS>_DEBUG`. The
    possible values for :macro:`<SUBSYS>_DEBUG` categorize events, such
    that it is possible to control the level and quantity of events
    written to the daemon's log.

    Configuration variables that affect daemon logs are

    +------------------------------------+
    |:macro:`MAX_NUM_<SUBSYS>_LOG`       |
    +------------------------------------+
    | :macro:`<SUBSYS>_LOG_KEEP_OPEN`    |
    +------------------------------------+
    |:macro:`TRUNC_<SUBSYS>_LOG_ON_OPEN` |
    +------------------------------------+
    | :macro:`TOUCH_LOG_INTERVAL`        |
    +------------------------------------+
    | :macro:`<SUBSYS>_LOCK`             |
    +------------------------------------+
    | :macro:`FILE_LOCK_VIA_MUTEX`       |
    +------------------------------------+
    | :macro:`LOGS_USE_TIMESTAMP`        |
    +------------------------------------+
    | :macro:`LOG_TO_SYSLOG`             |
    +------------------------------------+

    Daemon logs are often investigated to accomplish administrative
    debugging. :tool:`condor_config_val` can be used to determine the
    location and file name of the daemon log. For example, to display
    the location of the log for the *condor_collector* daemon, use

    .. code-block:: console

          $ condor_config_val COLLECTOR_LOG

job queue log
    The job queue log is a transactional representation of the current
    job queue. If the *condor_schedd* crashes, the job queue can be
    rebuilt using this log. The file name is set by configuration
    variable :macro:`JOB_QUEUE_LOG`, and defaults to ``$(SPOOL)/job_queue.log``.

    Within the log, each transaction is identified with an integer value
    and followed where appropriate with other values relevant to the
    transaction. To reduce the size of the log and remove any
    transactions that are no longer relevant, a copy of the log is kept
    by renaming the log at each time interval defined by configuration
    variable :macro:`QUEUE_CLEAN_INTERVAL`, and then a new log is written
    with only current and relevant transactions.

    Configuration variables that affect the job queue log are

    +------------------------------+--------------------------------------+
    | :macro:`SCHEDD_BACKUP_SPOOL` | :macro:`MAX_JOB_QUEUE_LOG_ROTATIONS` |
    +------------------------------+--------------------------------------+
    | :macro:`QUEUE_CLEAN_INTERVAL`|                                      |
    +------------------------------+--------------------------------------+

*condor_schedd* audit log
    The optional *condor_schedd* audit log records user-initiated
    events that modify the job queue, such as invocations of
    :tool:`condor_submit`, :tool:`condor_rm`, :tool:`condor_hold` and
    :tool:`condor_release`. Each event has a time stamp and a message that
    describes details of the event.

    This log exists to help administrators track the activities of pool
    users.

    The file name is set by configuration variable :macro:`SCHEDD_AUDIT_LOG`.

    Configuration variables that affect the audit log are

    +-------------------------------+----------------------------------+
    | :macro:`MAX_SCHEDD_AUDIT_LOG` | :macro:`MAX_NUM_SCHEDD_AUDIT_LOG`|
    +-------------------------------+----------------------------------+

*condor_shared_port* audit log
    The optional *condor_shared_port* audit log records connections
    made through the :macro:`DAEMON_SOCKET_DIR`. Each record includes the source
    address, the socket file name, and the target process's PID, UID,
    GID, executable path, and command line.

    This log exists to help administrators track the activities of pool
    users.

    The file name is set by configuration variable :macro:`SHARED_PORT_AUDIT_LOG`.

    Configuration variables that affect the audit log are

    +------------------------------------+----------------------------------------+
    | :macro:`MAX_SHARED_PORT_AUDIT_LOG` | :macro:`MAX_NUM_SHARED_PORT_AUDIT_LOG` |
    +------------------------------------+----------------------------------------+

event log
    The event log is an optional, chronological list of events that
    occur for all jobs and all users. The events logged are the same as
    those that would go into a job event log. The file name is set by
    configuration variable :macro:`EVENT_LOG`. The
    log is created only if this configuration variable is set.

    Configuration variables that affect the event log, setting details
    such as the maximum size to which this log may grow and details of
    file rotation and locking are

    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_MAX_SIZE`        | :macro:`EVENT_LOG_MAX_ROTATIONS`           |
    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_LOCKING`         |  :macro:`EVENT_LOG_ROTATION_LOCK`          |
    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_FSYNC`           | :macro:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS`|
    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_USE_XML`         |                                            |
    +------------------------------------+--------------------------------------------+

accountant log
    The accountant log is a transactional representation of the
    *condor_negotiator* daemon's database of accounting information,
    which are user priorities. The file name of the accountant log is
    ``$(SPOOL)/Accountantnew.log``. Within the log, users are identified
    by username@uid_domain.

    To reduce the size and remove information that is no longer
    relevant, a copy of the log is made when its size hits the number of
    bytes defined by configuration variable
    :macro:`MAX_ACCOUNTANT_DATABASE_SIZE`, and then a new log is written in a
    more compact form.

    Administrators can change user priorities kept in this log by using
    the command line tool :tool:`condor_userprio`.

negotiator match log
    The negotiator match log is a second daemon log from the
    *condor_negotiator* daemon. Events written to this log are those
    with debug level of ``D_MATCH``. The file name is set by
    configuration variable :macro:`NEGOTIATOR_MATCH_LOG`, and defaults to
    ``$(LOG)/MatchLog``.

history log
    This log contains information about all jobs that have been
    completed or removed. It is written by the *condor_schedd* and/or *condor_shadow* daemon. The default file
    name is ``$(SPOOL)/history`` as specified in :macro:`HISTORY`.

    Administrators can view this historical information by using
    the command line tool :tool:`condor_history`.

    Configuration variables that affect the history log, setting details
    such as the maximum size to which this log may grow are

    +----------------------------------+--------------------------------+
    | :macro:`ENABLE_HISTORY_ROTATION` |                                |
    +----------------------------------+--------------------------------+
    | :macro:`MAX_HISTORY_LOG`         | :macro:`MAX_HISTORY_ROTATIONS` |
    +----------------------------------+--------------------------------+
    | :macro:`ROTATE_HISTORY_MONTHLY`  | :macro:`ROTATE_HISTORY_DAILY`  |
    +----------------------------------+--------------------------------+

DAGMan Logs
'''''''''''

default node log
    A job event log of all node jobs within a single DAG. It is used to
    enforce the dependencies of the DAG.

    The file name is set by configuration variable
    :macro:`DAGMAN_DEFAULT_NODE_LOG`,
    and the full path name of this file must be unique while any and all
    submitted DAGs and other jobs from the submit host run. The syntax
    used in the definition of this configuration variable is different
    to enable the setting of a unique file name. See
    the :ref:`DAGMan Configuration` section for the complete definition.

the ``.dagman.out`` file
    A log created or appended to for each DAG submitted with timestamped
    events and extra information about the configuration applied to the
    DAG. The name of this log is formed by appending ``.dagman.out`` to
    the name of the DAG input file. The file remains after the DAG
    completes.

    This log may be helpful in debugging what has happened in the
    execution of a DAG, as well as help to determine the final state of
    the DAG.

    Configuration variables that affect this log are

    +---------------------------+-----------------------------------------+
    | :macro:`DAGMAN_VERBOSITY` | :macro:`DAGMAN_PENDING_REPORT_INTERVAL` |
    +---------------------------+-----------------------------------------+

the DAGMan job state log
    This optional, machine-readable log enables automated monitoring of
    DAG. The page :ref:`DAGMan Machine Readable History` details this log.


Directories
'''''''''''

HTCondor uses a few different directories, some of which are role-specific.
Do not use these directories for any other purpose, and do not share these
directories between machines.  The directories are listed in here by the
name of the configuration option used to tell HTCondor where they are; you
will not normally need to change these.

Directories used by More than One Role
++++++++++++++++++++++++++++++++++++++

 :macro:`LOG`
    Each HTCondor daemon writes its own log file, and each log file
    is placed in the :macro:`LOG` directory.  You can configure the name
    of each daemon's log by setting :macro:`<SUBSYS>_LOG`,
    although you should never need to do so.  You can also control the sizes
    of the log files or how often they rotate; see
    :ref:`daemon_logging_config_options`
    for details.  If you want to write your logs to a shared filesystem,
    we recommend including ``$(HOSTNAME)`` in the value of :macro:`LOG` rather
    than changing the names of each individual log to not collide.  If you
    set :macro:`LOG` to a shared filesystem, you should set :macro:`LOCK` to a local
    filesystem; see below.

 :macro:`LOCK`
    HTCondor uses a small number of lock files to synchronize access
    to certain files that are shared between multiple daemons.
    Because of problems encountered with file locking and network
    file systems (particularly NFS), these lock files should be
    placed on a local filesystem on each machine.  By default, they
    are placed in the :macro:`LOG` directory.

Directories used by the Submit Role
+++++++++++++++++++++++++++++++++++

 :macro:`SPOOL`
    The :macro:`SPOOL` directory holds two types of files: system
    data and (user) job data.  The former includes the job queue and
    history files.  The latter includes:

    - the files transferred, if any, when a job which set
      ``when_to_transfer_files`` to ``EXIT_OR_EVICT`` is evicted.
    - the input and output files of remotely-submitted jobs.
    - the checkpoint files stored by self-checkpointing jobs.

    Disk usage therefore varies widely based on the job mix, but
    since the schedd will abort if it can't append to the job queue log,
    you want to make sure this directory is on a partition which
    won't run out of space.

    To help ensure this, you may set
    :macro:`JOB_QUEUE_LOG` to separate the job queue log (system data)
    from the (user) job data.  This can also be used to increase performance
    (or reliability) by moving the job queue log to specialized hardware (an
    SSD or a high-redundancy RAID, for example).

Directories used by the Execute Role
++++++++++++++++++++++++++++++++++++

 :macro:`EXECUTE`
    The :macro:`EXECUTE` directory is the parent directory of the
    current working directory for any HTCondor job that runs on a given
    execute-role machine.  HTCondor copies the executable and input files
    for a job to its subdirectory; the job's standard output and standard
    error streams are also logged here.  Jobs will also almost always
    generate their output here as well, so the :macro:`EXECUTE` directory should provide plenty of space.  :macro:`EXECUTE` should not be placed under /tmp
    or /var/tmp if possible, as HTCondor loses the ability to make /tmp and
    /var/tmp private to the job.  While not a requirement, ideally :macro:`EXECUTE`
    should be on a distinct filesystem, so that it is impossible for a rogue job
    to fill up non-HTCondor related partitions.

    Usually, the per-job scratch execute directory is created by the startd
    as a directory under :macro:`EXECUTE`.  However, on Linux machines where HTCondor
    has root privilege, it can be configured to make an ephemeral per-job scratch
    filesystem. For more information visit :ref:`LVM Description`.
