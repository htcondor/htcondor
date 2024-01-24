ClassAd Attributes Added by the *condor_collector*
===================================================

:classad-attribute-def:`AuthenticatedIdentity`
    The authenticated name assigned by the *condor_collector* to the
    daemon that published the ClassAd.

:classad-attribute-def:`AuthenticationMethod`
    The authentication method used by the *condor_collector* to
    determine the :ad-attr:`AuthenticatedIdentity`.

:classad-attribute-def:`LastHeardFrom`
    The time inserted into a daemon's ClassAd representing the time that
    this *condor_collector* last received a message from the daemon.
    Time is represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970). This attribute is added if
    :macro:`COLLECTOR_DAEMON_STATS` is ``True``.

:classad-attribute-def:`UpdatesHistory`
    A bitmap representing the status of the most recent updates received
    from the daemon. This attribute is only added if
    :macro:`COLLECTOR_DAEMON_HISTORY_SIZE` is non-zero. See
    the :ref:`admin-manual/configuration-macros:condor_collector configuration
    file entries` section for more information on this setting. This attribute
    is added if :macro:`COLLECTOR_DAEMON_STATS` is ``True``.

:classad-attribute-def:`UpdatesLost`
    An integer count of the number of updates from the daemon that the
    *condor_collector* can definitively determine were lost since the
    *condor_collector* started running. This attribute is added if
    :macro:`COLLECTOR_DAEMON_STATS` is ``True``.

:classad-attribute-def:`UpdatesSequenced`
    An integer count of the number of updates received from the daemon,
    for which the *condor_collector* can tell how many were or were not
    lost, since the *condor_collector* started running. This attribute
    is added if :macro:`COLLECTOR_DAEMON_STATS` is ``True``.

:classad-attribute-def:`UpdatesTotal`
    An integer count started when the *condor_collector* started
    running, representing the sum of the number of updates actually
    received from the daemon plus the number of updates that the
    *condor_collector* determined were lost. This attribute is added if
    :macro:`COLLECTOR_DAEMON_STATS` is ``True``.
