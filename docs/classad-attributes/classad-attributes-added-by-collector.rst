      

ClassAd Attributes Added by the *condor\_collector*
===================================================

:index:`ClassAd attribute added by the condor_collector<single: ClassAd attribute added by the condor_collector>`
:index:`AuthenticatedIdentity;ClassAd attribute added by the condor_collector<single: AuthenticatedIdentity;ClassAd attribute added by the condor_collector>`

 ``AuthenticatedIdentity``:
    The authenticated name assigned by the *condor\_collector* to the
    daemon that published the ClassAd.
    :index:`AuthenticationMethod;ClassAd attribute added by the condor_collector<single: AuthenticationMethod;ClassAd attribute added by the condor_collector>`
 ``AuthenticationMethod``:
    The authentication method used by the *condor\_collector* to
    determine the ``AuthenticatedIdentity``.
    :index:`LastHeardFrom;ClassAd attribute added by the condor_collector<single: LastHeardFrom;ClassAd attribute added by the condor_collector>`
 ``LastHeardFrom``:
    The time inserted into a daemon’s ClassAd representing the time that
    this *condor\_collector* last received a message from the daemon.
    Time is represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970). This attribute is added if
    ``COLLECTOR_DAEMON_STATS`` is ``True``.
    :index:`UpdatesHistory;ClassAd attribute added by the condor_collector<single: UpdatesHistory;ClassAd attribute added by the condor_collector>`
 ``UpdatesHistory``:
    A bitmap representing the status of the most recent updates received
    from the daemon. This attribute is only added if
    ``COLLECTOR_DAEMON_HISTORY_SIZE``
    :index:`COLLECTOR_DAEMON_HISTORY_SIZE<single: COLLECTOR_DAEMON_HISTORY_SIZE>` is non-zero. See
    page \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__ for more
    information on this setting. This attribute is added if
    ``COLLECTOR_DAEMON_STATS`` is ``True``.
    :index:`UpdatesLost;ClassAd attribute added by the condor_collector<single: UpdatesLost;ClassAd attribute added by the condor_collector>`
 ``UpdatesLost``:
    An integer count of the number of updates from the daemon that the
    *condor\_collector* can definitively determine were lost since the
    *condor\_collector* started running. This attribute is added if
    ``COLLECTOR_DAEMON_STATS`` is ``True``.
    :index:`UpdatesSequenced;ClassAd attribute added by the condor_collector<single: UpdatesSequenced;ClassAd attribute added by the condor_collector>`
 ``UpdatesSequenced``:
    An integer count of the number of updates received from the daemon,
    for which the *condor\_collector* can tell how many were or were not
    lost, since the *condor\_collector* started running. This attribute
    is added if ``COLLECTOR_DAEMON_STATS`` is ``True``.
    :index:`UpdatesTotal;ClassAd attribute added by the condor_collector<single: UpdatesTotal;ClassAd attribute added by the condor_collector>`
 ``UpdatesTotal``:
    An integer count started when the *condor\_collector* started
    running, representing the sum of the number of updates actually
    received from the daemon plus the number of updates that the
    *condor\_collector* determined were lost. This attribute is added if
    ``COLLECTOR_DAEMON_STATS`` is ``True``.

      
