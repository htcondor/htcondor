Monitoring
==========

:index:`monitoring<single: monitoring; pool management>`
:index:`monitoring pools` :index:`pool monitoring`

Information that the *condor_collector* collects can be used to monitor
a pool. The *condor_status* command can be used to display snapshot of
the current state of the pool. Monitoring systems can be set up to track
the state over time, and they might go further, to alert the system
administrator about exceptional conditions.

Ganglia
-------

:index:`with Ganglia<single: with Ganglia; Monitoring>`
:index:`Ganglia monitoring`
:index:`condor_gangliad daemon`

Support for the Ganglia monitoring system
(`http://ganglia.info/ <http://ganglia.info/>`_) is integral to
HTCondor. Nagios (`http://www.nagios.org/ <http://www.nagios.org/>`_)
is often used to provide alerts based on data from the Ganglia
monitoring system. The *condor_gangliad* daemon provides an efficient
way to take information from an HTCondor pool and supply it to the
Ganglia monitoring system.

The *condor_gangliad* gathers up data as specified by its
configuration, and it streamlines getting that data to the Ganglia
monitoring system. Updates sent to Ganglia are done using the Ganglia
shared libraries for efficiency.

If Ganglia is already deployed in the pool, the monitoring of HTCondor
is enabled by running the *condor_gangliad* daemon on a single machine
within the pool. If the machine chosen is the one running Ganglia's
*gmetad*, then the HTCondor configuration consists of adding
``GANGLIAD`` to the definition of configuration variable ``DAEMON_LIST``
on that machine. It may be advantageous to run the *condor_gangliad*
daemon on the same machine as is running the *condor_collector* daemon,
because on a large pool with many ClassAds, there is likely to be less
network traffic. If the *condor_gangliad* daemon is to run on a
different machine than the one running Ganglia's *gmetad*, modify
configuration variable ``GANGLIA_GSTAT_COMMAND``
:index:`GANGLIA_GSTAT_COMMAND` to get the list of monitored hosts
from the master *gmond* program.

If the pool does not use Ganglia, the pool can still be monitored by a
separate server running Ganglia.

By default, the *condor_gangliad* will only propagate metrics to hosts
that are already monitored by Ganglia. Set configuration variable
``GANGLIA_SEND_DATA_FOR_ALL_HOSTS``
:index:`GANGLIA_SEND_DATA_FOR_ALL_HOSTS` to ``True`` to set up a
Ganglia host to monitor a pool not monitored by Ganglia or have a
heterogeneous pool where some hosts are not monitored. In this case,
default graphs that Ganglia provides will not be present. However, the
HTCondor metrics will appear.

On large pools, setting configuration variable
``GANGLIAD_PER_EXECUTE_NODE_METRICS``
:index:`GANGLIAD_PER_EXECUTE_NODE_METRICS` to ``False`` will
reduce the amount of data sent to Ganglia. The execute node data is the
least important to monitor. One can also limit the amount of data by
setting configuration variable ``GANGLIAD_REQUIREMENTS``
:index:`GANGLIAD_REQUIREMENTS`. Be aware that aggregate sums over
the entire pool will not be accurate if this variable limits the
ClassAds queried.

Metrics to be sent to Ganglia are specified in all files within the
directory specified by configuration variable
``GANGLIAD_METRICS_CONFIG_DIR``
:index:`GANGLIAD_METRICS_CONFIG_DIR`. Each file in the directory
is read, and the format within each file is that of New ClassAds. Here
is an example of a single metric definition given as a New ClassAd:

.. code-block:: condor-classad

    [
      Name   = "JobsSubmitted";
      Desc   = "Number of jobs submitted";
      Units  = "jobs";
      TargetType = "Scheduler";
    ]

A nice set of default metrics is in file:
``$(GANGLIAD_METRICS_CONFIG_DIR)/00_default_metrics``.

Recognized metric attribute names and their use:

 Name
    The name of this metric, which corresponds to the ClassAd attribute
    name. Metrics published for the same machine must have unique names.
 Value
    A ClassAd expression that produces the value when evaluated. The
    default value is the value in the daemon ClassAd of the attribute
    with the same name as this metric.
 Desc
    A brief description of the metric. This string is displayed when the
    user holds the mouse over the Ganglia graph for the metric.
 Verbosity
    The integer verbosity level of this metric. Metrics with a higher
    verbosity level than that specified by configuration variable
    ``GANGLIA_VERBOSITY`` :index:`GANGLIA_VERBOSITY` will not be
    published.
 TargetType
    A string containing a comma-separated list of daemon ClassAd types
    that this metric monitors. The specified values should match the
    value of ``MyType`` of the daemon ClassAd. In addition, there are
    special values that may be included. "Machine_slot1" may be
    specified to monitor the machine ClassAd for slot 1 only. This is
    useful when monitoring machine-wide attributes. The special value
    "ANY" matches any type of ClassAd.
 Requirements
    A boolean expression that may restrict how this metric is
    incorporated. It defaults to ``True``, which places no restrictions
    on the collection of this ClassAd metric.
 Title
    The graph title used for this metric. The default is the metric
    name.
 Group
    A string specifying the name of this metric's group. Metrics are
    arranged by group within a Ganglia web page. The default is
    determined by the daemon type. Metrics in different groups must have
    unique names.
 Cluster
    A string specifying the cluster name for this metric. The default
    cluster name is taken from the configuration variable
    ``GANGLIAD_DEFAULT_CLUSTER``
    :index:`GANGLIAD_DEFAULT_CLUSTER`.
 Units
    A string describing the units of this metric.
 Scale
    A scaling factor that is multiplied by the value of the ``Value``
    attribute. The scale factor is used when the value is not in the
    basic unit or a human-interpretable unit. For example, duty cycle is
    commonly expressed as a percent, but the HTCondor value ranges from
    0 to 1. So, duty cycle is scaled by 100. Some metrics are reported
    in KiB. Scaling by 1024 allows Ganglia to pick the appropriate
    units, such as number of bytes rather than number of KiB. When
    scaling by large values, converting to the "float" type is
    recommended.
 Derivative
    A boolean value that specifies if Ganglia should graph the
    derivative of this metric. Ganglia versions prior to 3.4 do not
    support this.
 Type
    A string specifying the type of the metric. Possible values are
    "double", "float", "int32", "uint32", "int16", "uint16", "int8",
    "uint8", and "string". The default is "string" for string values,
    the default is "int32" for integer values, the default is "float"
    for real values, and the default is "int8" for boolean values.
    Integer values can be coerced to "float" or "double". This is
    especially important for values stored internally as 64-bit values.
 Regex
    This string value specifies a regular expression that matches
    attributes to be monitored by this metric. This is useful for
    dynamic attributes that cannot be enumerated in advance, because
    their names depend on dynamic information such as the users who are
    currently running jobs. When this is specified, one metric per
    matching attribute is created. The default metric name is the name
    of the matched attribute, and the default value is the value of that
    attribute. As usual, the ``Value`` expression may be used when the
    raw attribute value needs to be manipulated before publication.
    However, since the name of the attribute is not known in advance, a
    special ClassAd attribute in the daemon ClassAd is provided to allow
    the ``Value`` expression to refer to it. This special attribute is
    named ``Regex``. Another special feature is the ability to refer to
    text matched by regular expression groups defined by parentheses
    within the regular expression. These may be substituted into the
    values of other string attributes such as ``Name`` and ``Desc``.
    This is done by putting macros in the string values. "\\\\1" is
    replaced by the first group, "\\\\2" by the second group, and so on.
 Aggregate
    This string value specifies an aggregation function to apply,
    instead of publishing individual metrics for each daemon ClassAd.
    Possible values are "sum", "avg", "max", and "min".
 AggregateGroup
    When an aggregate function has been specified, this string value
    specifies which aggregation group the current daemon ClassAd belongs
    to. The default is the metric ``Name``. This feature works like
    GROUP BY in SQL. The aggregation function produces one result per
    value of ``AggregateGroup``. A single aggregate group would
    therefore be appropriate for a pool-wide metric. As an example, to
    publish the sum of an attribute across different types of slot
    ClassAds, make the metric name an expression that is unique to each
    type. The default ``AggregateGroup`` would be set accordingly. Note
    that the assumption is still that the result is a pool-wide metric,
    so by default it is associated with the *condor_collector* daemon's
    host. To group by machine and publish the result into the Ganglia
    page associated with each machine, make the ``AggregateGroup``
    contain the machine name and override the default ``Machine``
    attribute to be the daemon's machine name, rather than the
    *condor_collector* daemon's machine name.
 Machine
    The name of the host associated with this metric. If configuration
    variable ``GANGLIAD_DEFAULT_MACHINE``
    :index:`GANGLIAD_DEFAULT_MACHINE` is not specified, the
    default is taken from the ``Machine`` attribute of the daemon
    ClassAd. If the daemon name is of the form name@hostname, this may
    indicate that there are multiple instances of HTCondor running on
    the same machine. To avoid the metrics from these instances
    overwriting each other, the default machine name is set to the
    daemon name in this case. For aggregate metrics, the default value
    of ``Machine`` will be the name of the *condor_collector* host.
 IP
    A string containing the IP address of the host associated with this
    metric. If ``GANGLIAD_DEFAULT_IP``
    :index:`GANGLIAD_DEFAULT_IP` is not specified, the default is
    extracted from the ``MyAddress`` attribute of the daemon ClassAd.
    This value must be unique for each machine published to Ganglia. It
    need not be a valid IP address. If the value of ``Machine`` contains
    an "@" sign, the default IP value will be set to the same value as
    ``Machine`` in order to make the IP value unique to each instance of
    HTCondor running on the same host.

Absent ClassAds
---------------

:index:`absent ClassAds<single: absent ClassAds; pool management>`
:index:`absent ClassAd` :index:`absent ClassAd<single: absent ClassAd; ClassAd>`

By default, HTCondor assumes that resources are transient: the
*condor_collector* will discard ClassAds older than
``CLASSAD_LIFETIME`` :index:`CLASSAD_LIFETIME` seconds. Its
default configuration value is 15 minutes, and as such, the default
value for ``UPDATE_INTERVAL`` :index:`UPDATE_INTERVAL` will pass
three times before HTCondor forgets about a resource. In some pools,
especially those with dedicated resources, this approach may make it
unnecessarily difficult to determine what the composition of the pool
ought to be, in the sense of knowing which machines would be in the
pool, if HTCondor were properly functioning on all of them.

This assumption of transient machines can be modified by the use of
absent ClassAds. When a machine ClassAd would otherwise expire, the
*condor_collector* evaluates the configuration variable
``ABSENT_REQUIREMENTS`` :index:`ABSENT_REQUIREMENTS` against the
machine ClassAd. If ``True``, the machine ClassAd will be saved in a
persistent manner and be marked as absent; this causes the machine to
appear in the output of ``condor_status -absent``. When the machine
returns to the pool, its first update to the *condor_collector* will
invalidate the absent machine ClassAd.

Absent ClassAds, like offline ClassAds, are stored to disk to ensure
that they are remembered, even across *condor_collector* crashes. The
configuration variable ``COLLECTOR_PERSISTENT_AD_LOG``
:index:`COLLECTOR_PERSISTENT_AD_LOG` defines the file in which the
ClassAds are stored, and replaces the no longer used variable
``OFFLINE_LOG``. Absent ClassAds are retained on disk as maintained by
the *condor_collector* for a length of time in seconds defined by the
configuration variable ``ABSENT_EXPIRE_ADS_AFTER``
:index:`ABSENT_EXPIRE_ADS_AFTER`. A value of 0 for this variable
means that the ClassAds are never discarded, and the default value is
thirty days.

Absent ClassAds are only returned by the *condor_collector* and
displayed when the **-absent** option to *condor_status* is specified,
or when the absent machine ClassAd attribute is mentioned on the
*condor_status* command line. This renders absent ClassAds invisible to
the rest of the HTCondor infrastructure.

A daemon may inform the *condor_collector* that the daemon's ClassAd
should not expire, but should be removed right away; the daemon asks for
its ClassAd to be invalidated. It may be useful to place an invalidated
ClassAd in the absent state, instead of having it removed as an
invalidated ClassAd. An example of a ClassAd that could benefit from
being absent is a system with an uninterruptible power supply that shuts
down cleanly but unexpectedly as a result of a power outage. To cause
all invalidated ClassAds to become absent instead of invalidated, set
``EXPIRE_INVALIDATED_ADS`` :index:`EXPIRE_INVALIDATED_ADS` to
``True``. Invalidated ClassAds will instead be treated as if they
expired, including when evaluating ``ABSENT_REQUIREMENTS``.

GPUs
----

:index:`monitoring GPUS`
:index:`GPU monitoring`

HTCondor supports monitoring GPU utilization for NVidia GPUs.  This feature
is enabled by default if you set ``use feature : GPUs`` in your configuration
file.

Doing so will cause the startd to run the ``condor_gpu_utilization`` tool.
This tool polls the (NVidia) GPU device(s) in the system and records their
utilization and memory usage values.  At regular intervals, the tool reports
these values to the *condor_startd*, assigning them to each device's usage
to the slot(s) to which those devices have been assigned.

Please note that ``condor_gpu_utilization`` can not presently assign GPU
utilization directly to HTCondor jobs.  As a result, jobs sharing a GPU
device, or a GPU device being used by from outside HTCondor, will result
in GPU usage and utilization being misreported accordingly.

However, this approach does simplify monitoring for the owner/administrator
of the GPUs, because usage is reported by the *condor_startd* in addition
to the jobs themselves.

:index:`DeviceGPUsAverageUsage<single: DeviceGPUsAverageUsage; machine attribute>`

  ``DeviceGPUsAverageUsage``
    The number of seconds executed by GPUs assigned to this slot,
    divided by the number of seconds since the startd started up.

:index:`DeviceGPUsMemoryPeakUsage<single: DeviceGPUsMemoryPeakUsage; machine attribute>`

  ``DeviceGPUsMemoryPeakUsage``
    The largest amount of GPU memory used GPUs assigned to this slot,
    since the startd started up.

Elasticsearch
-------------

:index:`Elasticsearch`
:index:`adstash`
:index:`condor_adstash`

HTCondor supports pushing *condor_schedd* and *condor_startd* job
history ClassAds to Elasticsearch via the *condor_adstash*
tool/daemon.
*condor_adstash* collects job history ClassAds as specified by its
configuration, converts each ClassAd to a JSON document, and pushes
each doc to the configured Elasticsearch index.
The index is automatically created if it does not exist, and fields
are added and configured based on well known job ClassAd attributes.
(Custom attributes are also pushed, though always as keyword fields.)

*condor_adstash* is a Python 3.6+ script that uses the
HTCondor :ref:`apis/python-bindings/index:Python Bindings`
and the
`Python Elasticsearch Client <https://elasticsearch-py.readthedocs.io/>`_,
both of which must be available to the system Python 3 installation
if using the daemonized version of *condor_adstash*.
*condor_adstash* can also be run as a standalone tool (e.g. in a
Python 3 virtual environment containing the necessary libraries).

Running *condor_adstash* as a daemon (i.e. under the watch of the
*condor_master*) can be enabled by adding
``use feature : adstash``
to your HTCondor configuration.
By default, this configuration will poll all *condor_schedds* that
report to the ``$(CONDOR_HOST)`` *condor_collector* every 20 minutes
and push the contents of the job history ClassAds to an Elasticsearch
instance running on ``localhost`` to an index named
``htcondor-000001``.
Your situation and monitoring needs are likely different!
See the ``condor_config.local.adstash`` example configuration file in
the ``examples/`` directory for detailed information on how to modify
your configuration.

If you prefer to run *condor_adstash* in standalone mode, see the
:doc:`../man-pages/condor_adstash` man page for more
details.
