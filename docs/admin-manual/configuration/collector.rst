:index:`Collector Daemon Options<single: Configuration; Collector Daemon Options>`

.. _collector_config_options:

Collector Daemon Configuration Options
======================================

These macros affect the *condor_collector*.

:macro-def:`CLASSAD_LIFETIME`
    The default maximum age in seconds for ClassAds collected by the
    *condor_collector*. ClassAds older than the maximum age are
    discarded by the *condor_collector* as stale.

    If present, the ClassAd attribute ``ClassAdLifetime`` specifies the
    ClassAd's lifetime in seconds. If ``ClassAdLifetime`` is not present
    in the ClassAd, the *condor_collector* will use the value of
    ``$(CLASSAD_LIFETIME)``. This variable is defined in terms of
    seconds, and it defaults to 900 seconds (15 minutes).

    To ensure that the *condor_collector* does not miss any ClassAds,
    the frequency at which all other subsystems that report using an
    update interval must be tuned. The configuration variables that set
    these subsystems are

    -  :macro:`UPDATE_INTERVAL` (for the *condor_startd* daemon)
    -  :macro:`NEGOTIATOR_UPDATE_INTERVAL`
    -  :macro:`SCHEDD_INTERVAL`
    -  :macro:`MASTER_UPDATE_INTERVAL`
    -  :macro:`DEFRAG_UPDATE_INTERVAL`
    -  :macro:`HAD_UPDATE_INTERVAL`

:macro-def:`COLLECTOR_REQUIREMENTS`
    A boolean expression that filters out unwanted ClassAd updates. The
    expression is evaluated for ClassAd updates that have passed through
    enabled security authorization checks. The default behavior when
    this expression is not defined is to allow all ClassAd updates to
    take place. If ``False``, a ClassAd update will be rejected.

    Stronger security mechanisms are the better way to authorize or deny
    updates to the *condor_collector*. This configuration variable
    exists to help those that use host-based security, and do not trust
    all processes that run on the hosts in the pool. This configuration
    variable may be used to throw out ClassAds that should not be
    allowed. For example, for *condor_startd* daemons that run on a
    fixed port, configure this expression to ensure that only machine
    ClassAds advertising the expected fixed port are accepted. As a
    convenience, before evaluating the expression, some basic sanity
    checks are performed on the ClassAd to ensure that all of the
    ClassAd attributes used by HTCondor to contain IP:port information
    are consistent. To validate this information, the attribute to check
    is ``TARGET.MyAddress``.

    Please note that _all_ ClassAd updates are filtered.  Unless your
    requirements are the same for all daemons, including the collector
    itself, you'll want to use the :ad-attr:`MyType` attribute to limit your
    filter(s).

:macro-def:`CLIENT_TIMEOUT`
    Network timeout that the *condor_collector* uses when talking to
    any daemons or tools that are sending it a ClassAd update. It is
    defined in seconds and defaults to 30.

:macro-def:`QUERY_TIMEOUT`
    Network timeout when talking to anyone doing a query. It is defined
    in seconds and defaults to 60.

:macro-def:`COLLECTOR_NAME`
    This macro is used to specify a short description of your pool. It
    should be about 20 characters long. For example, the name of the
    UW-Madison Computer Science HTCondor Pool is ``"UW-Madison CS"``.
    While this macro might seem similar to :macro:`MASTER_NAME` or
    :macro:`SCHEDD_NAME`, it is unrelated. Those settings are used to
    uniquely identify (and locate) a specific set of HTCondor daemons,
    if there are more than one running on the same machine. The
    :macro:`COLLECTOR_NAME` setting is just used as a human-readable string
    to describe the pool.

:macro-def:`COLLECTOR_UPDATE_INTERVAL`
    This variable is defined in seconds and defaults to 900 (every 15
    minutes). It controls the frequency of the periodic updates sent to
    a central *condor_collector*.

:macro-def:`COLLECTOR_SOCKET_BUFSIZE`
    This specifies the buffer size, in bytes, reserved for
    *condor_collector* network UDP sockets. The default is 10240000, or
    a ten megabyte buffer. This is a healthy size, even for a large
    pool. The larger this value, the less likely the *condor_collector*
    will have stale information about the pool due to dropping update
    packets. If your pool is small or your central manager has very
    little RAM, considering setting this parameter to a lower value
    (perhaps 256000 or 128000).

    .. note::

        For some Linux distributions, it may be necessary to raise the
        OS's system-wide limit for network buffer sizes. The parameter that
        controls this limit is /proc/sys/net/core/rmem_max. You can see the
        values that the *condor_collector* actually uses by enabling
        D_FULLDEBUG for the collector and looking at the log line that
        looks like this:

    Reset OS socket buffer size to 2048k (UDP), 255k (TCP).

    For changes to this parameter to take effect, *condor_collector*
    must be restarted.

:macro-def:`COLLECTOR_TCP_SOCKET_BUFSIZE`
    This specifies the TCP buffer size, in bytes, reserved for
    *condor_collector* network sockets. The default is 131072, or a 128
    kilobyte buffer. This is a healthy size, even for a large pool. The
    larger this value, the less likely the *condor_collector* will have
    stale information about the pool due to dropping update packets. If
    your pool is small or your central manager has very little RAM,
    considering setting this parameter to a lower value (perhaps 65536
    or 32768).

    .. note::

        See the note for :macro:`COLLECTOR_SOCKET_BUFSIZE`.

:macro-def:`KEEP_POOL_HISTORY`
    This boolean macro is used to decide if the collector will write out
    statistical information about the pool to history files. The default
    is ``False``. The location, size, and frequency of history logging
    is controlled by the other macros.

:macro-def:`POOL_HISTORY_DIR`
    This macro sets the name of the directory where the history files
    reside (if history logging is enabled). The default is the :macro:`SPOOL`
    directory.

:macro-def:`POOL_HISTORY_MAX_STORAGE`
    This macro sets the maximum combined size of the history files. When
    the size of the history files is close to this limit, the oldest
    information will be discarded. Thus, the larger this parameter's
    value is, the larger the time range for which history will be
    available. The default value is 10000000 (10 MB).

:macro-def:`POOL_HISTORY_SAMPLING_INTERVAL`
    This macro sets the interval, in seconds, between samples for
    history logging purposes. When a sample is taken, the collector goes
    through the information it holds, and summarizes it. The information
    is written to the history file once for each 4 samples. The default
    (and recommended) value is 60 seconds. Setting this macro's value
    too low will increase the load on the collector, while setting it to
    high will produce less precise statistical information.

:macro-def:`FLOCK_FROM`
    The macros contains a comma separate list of schedd names that
    should be allowed to flock to this central manager.  Defaults
    to an empty list.

:macro-def:`COLLECTOR_DAEMON_STATS`
    A boolean value that controls whether or not the *condor_collector*
    daemon keeps update statistics on incoming updates. The default
    value is ``True``. If enabled, the *condor_collector* will insert
    several attributes into the ClassAds that it stores and sends.
    ClassAds without the ``UpdateSequenceNumber`` and
    ``DaemonStartTime`` attributes will not be counted, and will not
    have attributes inserted (all modern HTCondor daemons which publish
    ClassAds publish these attributes).
    :index:`UpdatesTotal<single: UpdatesTotal; ClassAd attribute added by the condor_collector>`
    :index:`UpdatesSequenced<single: UpdatesSequenced; ClassAd attribute added by the condor_collector>`
    :index:`UpdatesLost<single: UpdatesLost; ClassAd attribute added by the condor_collector>`

    The attributes inserted are ``UpdatesTotal``, :ad-attr:`UpdatesSequenced`,
    and ``UpdatesLost``. ``UpdatesTotal`` is the total number of updates
    (of this ClassAd type) the *condor_collector* has received from
    this host. :ad-attr:`UpdatesSequenced` is the number of updates that the
    *condor_collector* could have as lost. In particular, for the first
    update from a daemon, it is impossible to tell if any previous ones
    have been lost or not. ``UpdatesLost`` is the number of updates that
    the *condor_collector* has detected as being lost. See
    :doc:`/classad-attributes/classad-attributes-added-by-collector` for
    more information on the added attributes.

:macro-def:`COLLECTOR_STATS_SWEEP`
    This value specifies the number of seconds between sweeps of the
    *condor_collector* 's per-daemon update statistics. Records for
    daemons which have not reported in this amount of time are purged in
    order to save memory. The default is two days. It is unlikely that
    you would ever need to adjust this.
    :index:`UpdatesHistory<single: UpdatesHistory; ClassAd attribute added by the condor_collector>`

:macro-def:`COLLECTOR_DAEMON_HISTORY_SIZE`
    This variable controls the size of the published update history that
    the *condor_collector* inserts into the ClassAds it stores and
    sends. The default value is 128, which means that history is stored
    and published for the latest 128 updates. This variable's value is
    ignored, if :macro:`COLLECTOR_DAEMON_STATS` is not enabled.

    If the value is a non-zero one, the *condor_collector* will insert
    attribute :ad-attr:`UpdatesHistory` into the ClassAd (similar to
    ``UpdatesTotal``). AttrUpdatesHistory is a hexadecimal string which
    represents a bitmap of the last :macro:`COLLECTOR_DAEMON_HISTORY_SIZE`
    updates. The most significant bit (MSB) of the bitmap represents
    the most recent update, and the least significant bit (LSB) represents
    the least recent. A value of zero means that the update was not lost,
    and a value of 1 indicates that the update was detected as lost.

    For example, if the last update was not lost, the previous was lost,
    and the previous two not, the bitmap would be 0100, and the matching
    hex digit would be ``"4"``. Note that the MSB can never be marked as
    lost because its loss can only be detected by a non-lost update (a
    gap is found in the sequence numbers). Thus,
    ``UpdatesHistory = "0x40"`` would be the history for the last 8
    updates. If the next updates are all successful, the values
    published, after each update, would be: 0x20, 0x10, 0x08, 0x04,
    0x02, 0x01, 0x00.

    See
    :ref:`classad-attributes/classad-attributes-added-by-collector:classad attributes added by the *condor_collector*`
    for more information on the added attribute.

:macro-def:`COLLECTOR_CLASS_HISTORY_SIZE`
    This variable controls the size of the published update history that
    the *condor_collector* inserts into the *condor_collector*
    ClassAds it produces. The default value is zero.

    If this variable has a non-zero value, the *condor_collector* will
    insert ``UpdatesClassHistory`` into the *condor_collector* ClassAd
    (similar to :ad-attr:`UpdatesHistory`). These are added per class of
    ClassAd, however. The classes refer to the type of ClassAds.
    Additionally, there is a Total class created, which represents the
    history of all ClassAds that this *condor_collector* receives.

    Note that the *condor_collector* always publishes Lost, Total and
    Sequenced counts for all ClassAd classes. This is similar to the
    statistics gathered if :macro:`COLLECTOR_DAEMON_STATS` is enabled.

:macro-def:`COLLECTOR_QUERY_WORKERS`
    This macro sets the maximum number of child worker processes that
    the *condor_collector* can have, and defaults to a value of 4 on
    Linux and MacOS platforms. When receiving a large query request, the
    *condor_collector* may fork() a new process to handle the query,
    freeing the main process to handle other requests. Each forked child
    process will consume memory, potentially up to 50% or more of the
    memory consumed by the parent collector process. To limit the amount
    of memory consumed on the central manager to handle incoming
    queries, the default value for this macro is 4. When the number of
    outstanding worker processes reaches the maximum specified by this
    macro, any additional incoming query requests will be queued and
    serviced after an existing child worker completes. Note that on
    Windows platforms, this macro has a value of zero and cannot be
    changed.

:macro-def:`COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO`
    This macro defines the number of :macro:`COLLECTOR_QUERY_WORKERS`
    slots will be held in reserve to only service high priority query
    requests. Currently, high priority queries are defined as those
    coming from the *condor_negotiator* during the course of matchmaking,
    or via a "condor_sos condor_status" command. The idea here is the critical
    operation of matchmaking machines to jobs will take precedence over
    user condor_status invocations. Defaults to a value of 1. The
    maximum allowable value for this macro is equal to
    :macro:`COLLECTOR_QUERY_WORKERS` minus 1.

:macro-def:`COLLECTOR_QUERY_WORKERS_PENDING`
    This macro sets the maximum of collector pending query requests that
    can be queued waiting for child workers to exit. Queries that would
    exceed this maximum are immediately aborted. When a forked child
    worker exits, a pending query will be pulled from the queue for
    service. Note the collector will confirm that the client has not
    closed the TCP socket (because it was tired of waiting) before going
    through all the work of actually forking a child and starting to
    service the query. Defaults to a value of 50.

:macro-def:`COLLECTOR_QUERY_MAX_WORKTIME`
    This macro defines the maximum amount of time in seconds that a
    query has to complete before it is aborted. Queries that wait in the
    pending queue longer than this period of time will be aborted before
    forking. Queries that have already forked will also abort after the
    worktime has expired - this protects against clients on a very slow
    network connection. If set to 0, then there is no timeout. The
    default is 0.

:macro-def:`HANDLE_QUERY_IN_PROC_POLICY`
    This variable sets the policy for which queries the
    *condor_collector* should handle in process rather than by forking
    a worker. It should be set to one of the following values

    -  ``always`` Handle all queries in process
    -  ``never`` Handle all queries using fork workers
    -  ``small_table`` Handle only queries of small tables in process
    -  ``small_query`` Handle only small queries in process
    -  ``small_table_and_query`` Handle only small queries on small
       tables in process
    -  ``small_table_or_query`` Handle small queries or small tables in
       process

    A small table is any table of ClassAds in the collector other than
    Master,Startd,Generic and Any ads. A small query is a locate query,
    or any query with both a projection and a result limit that is
    smaller than 10. The default value is ``small_table_or_query``.

:macro-def:`COLLECTOR_DEBUG`
    This macro (and other macros related to debug logging in the
    *condor_collector* is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`CONDOR_VIEW_CLASSAD_TYPES`
    Provides the ClassAd types that will be forwarded to the
    :macro:`CONDOR_VIEW_HOST`. The ClassAd types can be found with
    :tool:`condor_status` **-any**. The default forwarding behavior of the
    *condor_collector* is equivalent to

    .. code-block:: condor-config

          CONDOR_VIEW_CLASSAD_TYPES=Machine,Submitter

    There is no default value for this variable.

:macro-def:`COLLECTOR_FORWARD_FILTERING`
    When this boolean variable is set to ``True``, Machine and Submitter
    ad updates are not forwarded to the :macro:`CONDOR_VIEW_HOST` if certain
    attributes are unchanged from the previous update of the ad. The
    default is ``False``, meaning all updates are forwarded.

:macro-def:`COLLECTOR_FORWARD_WATCH_LIST`
    When :macro:`COLLECTOR_FORWARD_FILTERING` is set to ``True``, this
    variable provides the list of attributes that controls whether a
    Machine or Submitter ad update is forwarded to the
    :macro:`CONDOR_VIEW_HOST`. If all attributes in this list are unchanged
    from the previous update, then the new update is not forwarded. The
    default value is ``State,Cpus,Memory,IdleJobs``.

:macro-def:`COLLECTOR_FORWARD_INTERVAL`
    When :macro:`COLLECTOR_FORWARD_FILTERING` is set to ``True``, this
    variable limits how long forwarding of updates for a given ad can be
    filtered before an update must be forwarded. The default is one
    third of :macro:`CLASSAD_LIFETIME`.

:macro-def:`COLLECTOR_FORWARD_CLAIMED_PRIVATE_ADS`
    When this boolean variable is set to ``False``, the *condor_collector*
    will not forward the private portion of Machine ads to the
    :macro:`CONDOR_VIEW_HOST` if the ad's :ad-attr:`State` is ``Claimed``.
    The default value is ``$(NEGOTIATOR_CONSIDER_PREEMPTION)``.

:macro-def:`COLLECTOR_FORWARD_PROJECTION`
    An expression that evaluates to a string in the context of an update. The string is treated as a list
    of attributes to forward.  If the string has no attributes, it is ignored. The intended use is to
    restrict the list of attributes forwarded for claimed Machine ads.
    When ``$(NEGOTIATOR_CONSIDER_PREEMPTION)`` is false, the negotiator needs only a few attributes from
    Machine ads that are in the ``Claimed`` state. A Suggested use might be

    .. code-block:: condor-config

          if ! $(NEGOTIATOR_CONSIDER_PREEMPTION)
             COLLECTOR_FORWARD_PROJECTION = IfThenElse(State is "Claimed", "$(FORWARD_CLAIMED_ATTRS)", "")
             # forward only the few attributes needed by the Negotiator and a few more needed by condor_status
             FORWARD_CLAIMED_ATTRS = Name MyType MyAddress StartdIpAddr Machine Requirements \
                State Activity AccountingGroup Owner RemoteUser SlotWeight ConcurrencyLimits \
                Arch OpSys Memory Cpus CondorLoadAvg EnteredCurrentActivity
          endif

    There is no default value for this variable.


The following macros control where, when, and for how long HTCondor
persistently stores absent ClassAds. See
section :ref:`admin-manual/cm-configuration:absent classads` for more details.

:macro-def:`ABSENT_REQUIREMENTS`
    A boolean expression evaluated by the *condor_collector* when a
    machine ClassAd would otherwise expire. If ``True``, the ClassAd
    instead becomes absent. If not defined, the implementation will
    behave as if ``False``, and no absent ClassAds will be stored.

:macro-def:`ABSENT_EXPIRE_ADS_AFTER`
    The integer number of seconds after which the *condor_collector*
    forgets about an absent ClassAd. If 0, the ClassAds persist forever.
    Defaults to 30 days.

:macro-def:`COLLECTOR_PERSISTENT_AD_LOG`
    The full path and file name of a file that stores machine ClassAds
    for every hibernating or absent machine. This forms a persistent
    storage of these ClassAds, in case the *condor_collector* daemon
    crashes.

    To avoid :tool:`condor_preen` removing this log, place it in a directory
    other than the directory defined by ``$(SPOOL)``. Alternatively, if
    this log file is to go in the directory defined by ``$(SPOOL)``, add
    the file to the list given by :macro:`VALID_SPOOL_FILES`.

:macro-def:`EXPIRE_INVALIDATED_ADS`
    A boolean value that defaults to ``False``. When ``True``, causes
    all invalidated ClassAds to be treated as if they expired. This
    permits invalidated ClassAds to be marked absent, as defined in
    :ref:`admin-manual/cm-configuration:absent classads`.
