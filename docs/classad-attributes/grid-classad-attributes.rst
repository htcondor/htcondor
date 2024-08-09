Grid ClassAd Attributes
=======================

:classad-attribute-def:`GahpCommandRuntime`
    A Statistics attribute defining the time it takes for commands
    issued to the GAHP server to complete.

:classad-attribute-def:`GahpCommandsInFlight`
    A Statistics attribute defining the number of commands issued to
    the GAHP server that haven't completed yet.

:classad-attribute-def:`GahpCommandsIssued`
    A Statistics attribute defining the total number of commands that
    have been issued to the GAHP server.

:classad-attribute-def:`GahpCommandsQueued`
    A Statistics attribute defining the number of commands the
    *condor_gridmanager* is refraining from issuing to the GAHP server
    due to configuration parameter :macro:`GRIDMANAGER_MAX_PENDING_REQUESTS`.

:classad-attribute-def:`GahpCommandsTimedOut`
    A Statistics attribute defining the number of commands issued to
    the GAHP server that didn't complete within the timeout period
    set by configuration parameter :macro:`GRIDMANAGER_GAHP_RESPONSE_TIMEOUT`.

:classad-attribute-def:`GahpPid`
    The process id of the GAHP server used to interact with the grid
    service.

:classad-attribute-def:`GridResourceUnavailableTime`
    Time at which the grid service became unavailable.
    Measured in the number of seconds since the epoch (00:00:00 UTC,
    Jan 1, 1970).

:classad-attribute-def:`GridResourceUnavailableTimeReason`
    A string giving details as to why the grid service is currently
    considered unavailable.

:classad-attribute-def:`GridResourceUnavailableTimeReasonCode`
    An integer classifying the type of error that caused the grid
    service to be considered unavailable.

    +-------+-----------------------------+
    | Value | Failure Type                |
    +=======+=============================+
    | 1     | GAHP PING command failed    |
    +-------+-----------------------------+
    | 2     | Failed to start GAHP server |
    +-------+-----------------------------+

:classad-attribute-def:`IdleJobs`
    The number of idle jobs currently submitted to the grid service by
    this *condor_gridmanager*.

:classad-attribute-def:`JobLimit`
    The maximum number of jobs this *condor_gridmanager* will submit
    to the grid service at a time.
    This is controlled by configuration parameter
    :macro:`GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE`.

:classad-attribute-def:`NumJobs`
    The number of jobs this *condor_gridmanager* is managing that are
    intended for the grid service.

:classad-attribute-def:`SubmitsAllowed`
    The number of jobs this *condor_gridmanager* currently has
    submitted to the grid resource.

:classad-attribute-def:`SubmitsWanted`
    The number of jobs this *condor_gridmanager* has refrained from
    submitting to the grid resource due to :ad-attr:`JobLimit`.
