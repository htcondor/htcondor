DaemonCore Statistics Attributes
================================


:index:`DaemonCore statistics attributes<single: DaemonCore statistics attributes; ClassAd>`

Every HTCondor daemon keeps a set of operational statistics, some of
which are common to all daemons, others are specific to the running of a
particular daemon. In some cases, the statistics can reveal buggy or
slow performance of the HTCondor system. The following statistics are
available for all daemons, and can be accessed directly with the
condor_status command with a direct query, such as

.. code-block:: console

    $ condor_status -direct somehostname.example.com -schedd -statistics DC:2 -l


:index:`DCUdpQueueDepth<single: DCUdpQueueDepth; ClassAd statistics attribute>`

``DCUdpQueueDepth``:
    This attribute is the number of bytes in the incoming UDP receive
    queue for this daemon, if it has a UDP command port. This attribute
    is polled once a minute by default, so may be out of date. The
    attribute DCUdpQueueDepthPeak records the peak depth since the
    daemon has started.

:index:`DebugOuts<single: DebugOuts; ClassAd statistics attribute>`

``DebugOuts``:
    This attribute is the count of debugging messages printed to the
    daemon's debug log, such as the ScheddLog. There is a moderate cost
    to writing these logging messages, if the debug level is very high
    for an active daemon, the logging will slow performance. The
    corresponding attribute RecentDebugOuts is the count of the messages
    in the last 20 minutes.

:index:`PipeMessages<single: PipeMessages; ClassAd statistics attribute>`

``PipeMessages``:
    This attribute is the number of messages received on a Unix pipe by
    this daemon since start time. The corresponding attribute
    RecentPipeMessages is the count of message in the last 20 minutes.

:index:`PipeRuntime<single: PipeRuntime; ClassAd statistics attribute>`

``PipeRuntime``:
    This attribute respresents the total number of wall clock seconds
    this daemon has spent processing pipe message since start. The
    corresponding attribute RecentPipeRuntime is the total time in the
    last 20 minutes.

:index:`SelectWaittime<single: SelectWaittime; ClassAd statistics attribute>`

``SelectWaittime``:
    This attribute represents the total number of wall clock seconds
    this daemon has spent completely idle, waiting to process incoming
    requests or internal timers. The attribute DaemonCoreDutyCycle,
    which may be easier to write policy around, is based off of this.

:index:`SignalRuntime<single: SignalRuntime; ClassAd statistics attribute>`

``SignalRuntime``:
    This attribute respresents the total number of wall clock time
    seconds this daemon has spent processing signals since start. The
    corresponding attribute RecentSignalRuntime is the total time in the
    last 20 minutes.

:index:`Signals<single: Signals; ClassAd statistics attribute>`

``Signals``:
    This attribute is the number of signals, either Unix signals, or
    HTCondor simulated signals received by this daemon since start time.
    The corresponding attribute RecentSignals is the number of signals
    in the last 20 minutes.

:index:`SocketRuntime<single: SocketRuntime; ClassAd statistics attribute>`

``SocketRuntime``:
    This attribute respresents the total number of wall clock time
    seconds this daemon has spent processing socket messages since
    start. The corresponding attribute RecentTimerRuntime is the total
    time in the last 20 minutes.

:index:`SockMessages<single: SockMessages; ClassAd statistics attribute>`

``SockMessages``:
    This attribute is the number of messages received on socket by this
    daemon since start time. The corresponding attribute
    RecentSockMessages is the count of message in the last 20 minutes.

:index:`TimerRuntime<single: TimerRuntime; ClassAd statistics attribute>`

``TimerRuntime``:
    This attribute respresents the total number of wall clock time
    seconds this daemon has spent processing timers since start. The
    corresponding attribute RecentTimerRuntime is the total time in the
    last 20 minutes.

:index:`TimersFired<single: TimersFired; ClassAd statistics attribute>`

``TimersFired``:
    This attribute is the number of internal timers which have fired in
    this daemon during the most recent pass of the event loop. The corresponding attribute
    TimersFiredPeak is the maximum number of timers fired in one pass of the
    event loop since daemon start time.


